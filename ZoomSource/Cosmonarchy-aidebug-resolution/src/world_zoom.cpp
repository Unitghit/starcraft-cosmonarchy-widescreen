#include "world_zoom.h"
#include "pixel_zoom_steps.h"

#include <algorithm>
#include <cstring>

#include "console/windows_wrap.h"
#include "resolution.h"

namespace world_zoom
{
    namespace
    {
        struct Settings
        {
            bool loaded = false;
            bool enabled = false;
            bool smooth = true;
            unsigned start_zoom_units = 10000;
            unsigned extra_zoom_percent = 100;
            char path[MAX_PATH] = "cosmonarchy_viewport.ini";
        };

        constexpr unsigned base_zoom_units = 10000;
        constexpr DWORD animation_duration_ms = 120;
        unsigned current_zoom_units = base_zoom_units;
        unsigned animation_start_units = base_zoom_units;
        unsigned target_zoom_units = base_zoom_units;
        DWORD animation_start_tick = 0;
        int wheel_remainder = 0;

        struct ZoomAnchor
        {
            bool active = false;
            int presented_x = 0;
            int presented_y = 0;
            int64_t world_x = 0;
            int64_t world_y = 0;
        };

        struct Transform
        {
            unsigned source_left = 0;
            unsigned source_top = 0;
            unsigned source_width = 0;
            unsigned source_height = 0;
        };

        Settings settings;
        unsigned NormalizeExtraZoomPercent(unsigned value)
        {
            return value == 50 || value == 100 ? value : 0;
        }
        unsigned MaximumZoomUnits()
        {
            return std::max(20000u, settings.start_zoom_units) *
                (100u + NormalizeExtraZoomPercent(settings.extra_zoom_percent)) / 100u;
        }
        std::vector<unsigned> pixel_levels;
        Transform transform;
        ZoomAnchor anchor;
        unsigned x_lookup[resolution::maximum_screen_width] = {};
        unsigned y_lookup[resolution::maximum_screen_height] = {};
        bool lookup_valid = false;

        bool FileExists(const char *path)
        {
            const DWORD attributes = GetFileAttributesA(path);
            return attributes != INVALID_FILE_ATTRIBUTES &&
                (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
        }

        void ResolveConfigurationPath(char *destination, size_t size)
        {
            char launch_path[MAX_PATH] = {};
            const DWORD launch_length = GetFullPathNameA(
                "cosmonarchy_viewport.ini", MAX_PATH, launch_path, nullptr);
            if (launch_length != 0 && launch_length < MAX_PATH &&
                FileExists(launch_path))
            {
                strcpy_s(destination, size, launch_path);
                return;
            }

            HMODULE module = nullptr;
            if (!GetModuleHandleExA(
                    GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    reinterpret_cast<LPCSTR>(&settings), &module))
                return;

            char module_path[MAX_PATH] = {};
            if (!GetModuleFileNameA(module, module_path, MAX_PATH))
                return;
            char *slash = strrchr(module_path, '\\');
            if (!slash)
                return;
            *slash = '\0';
            slash = strrchr(module_path, '\\');
            if (!slash)
                return;
            *slash = '\0';
            strcat_s(module_path, "\\cosmonarchy_viewport.ini");
            if (FileExists(module_path))
                strcpy_s(destination, size, module_path);
        }

        void LoadSettings()
        {
            if (settings.loaded)
                return;
            settings.loaded = true;
            ResolveConfigurationPath(settings.path, sizeof(settings.path));
            if (!FileExists(settings.path))
                return;

            const unsigned configured_width = GetPrivateProfileIntA(
                "viewport", "internal_width", resolution::screen_width,
                settings.path);
            const unsigned configured_height = GetPrivateProfileIntA(
                "viewport", "internal_height", resolution::screen_height,
                settings.path);
            if (configured_width != resolution::screen_width ||
                configured_height != resolution::screen_height)
                return;

            const bool requested = GetPrivateProfileIntA(
                "world_zoom", "enabled", 0, settings.path) != 0;
            settings.enabled = requested;
            char transition[16] = {};
            GetPrivateProfileStringA("world_zoom", "transition", "smooth",
                transition, sizeof(transition), settings.path);
            settings.smooth = _stricmp(transition, "instant") != 0;
            settings.extra_zoom_percent = NormalizeExtraZoomPercent(GetPrivateProfileIntA(
                "world_zoom", "extra_zoom_percent", 100, settings.path));
            const unsigned maximum_start = std::max(20000u,
                static_cast<unsigned>(resolution::game_width) * base_zoom_units / 640);
            const int configured_start = static_cast<int>(GetPrivateProfileIntA(
                "world_zoom", "start_zoom_units", base_zoom_units, settings.path));
            settings.start_zoom_units = static_cast<unsigned>(std::max(
                static_cast<int>(base_zoom_units), std::min(configured_start,
                    static_cast<int>(maximum_start))));
            char backend[32] = {}, filter[16] = {}, aspect[16] = {};
            GetPrivateProfileStringA("presentation", "backend", "single_stage", backend, sizeof(backend), settings.path);
            GetPrivateProfileStringA("presentation", "filter", "nearest", filter, sizeof(filter), settings.path);
            const bool exact_stops_supported =
                _stricmp(backend,"single_stage")==0 && _stricmp(filter,"nearest")==0;
            // Exact stops always supplement the regular sequence when supported.
            // Old steps=pixel_perfect configuration is intentionally ignored.
            if (exact_stops_supported)
            {
                unsigned ow=GetPrivateProfileIntA("presentation","output_width",0,settings.path);
                unsigned oh=GetPrivateProfileIntA("presentation","output_height",0,settings.path);
                if (ow>16384 || oh>16384) ow=oh=0;
                GetPrivateProfileStringA("presentation","preserve_aspect_ratio","true",aspect,sizeof(aspect),settings.path);
                pixel_zoom_steps::Fit(resolution::screen_width,resolution::screen_height,ow,oh,_stricmp(aspect,"false")!=0);
                pixel_levels=pixel_zoom_steps::Build(resolution::screen_width,resolution::screen_height,
                    ow,oh,MaximumZoomUnits());
            }
            current_zoom_units = requested ? settings.start_zoom_units : base_zoom_units;
            animation_start_units = target_zoom_units = current_zoom_units;
        }

        void UpdateAnimation(DWORD now)
        {
            if (!settings.enabled ||
                current_zoom_units == target_zoom_units)
                return;

            const DWORD elapsed = now - animation_start_tick;
            if (!settings.smooth || elapsed >= animation_duration_ms)
            {
                current_zoom_units = target_zoom_units;
                return;
            }

            const uint64_t time = elapsed;
            const uint64_t duration = animation_duration_ms;
            const uint64_t eased_numerator =
                time * (2 * duration - time);
            const uint64_t eased_denominator =
                duration * duration;
            const int64_t distance =
                static_cast<int64_t>(target_zoom_units) -
                static_cast<int64_t>(animation_start_units);
            const unsigned next = static_cast<unsigned>(
                static_cast<int64_t>(animation_start_units) +
                distance * static_cast<int64_t>(eased_numerator) /
                    static_cast<int64_t>(eased_denominator));
            if (next != current_zoom_units)
            {
                current_zoom_units = next;
            }
        }

        unsigned SourceExtent(unsigned output_extent)
        {
            const unsigned value = static_cast<unsigned>(
                static_cast<uint64_t>(output_extent) * base_zoom_units /
                    current_zoom_units);
            return std::max(1u, std::min(value, output_extent));
        }

        unsigned SampleOffset(int presented, unsigned source_extent,
                              unsigned output_extent)
        {
            presented = std::max(0, std::min(presented,
                static_cast<int>(output_extent) - 1));
            return static_cast<unsigned>(
                (static_cast<uint64_t>(2) * presented + 1) *
                source_extent /
                    (static_cast<uint64_t>(2) * output_extent));
        }

        unsigned AnchoredCrop(int64_t world_coordinate,
                              uint32_t camera,
                              int presented_coordinate,
                              unsigned output_extent,
                              unsigned source_extent)
        {
            const int64_t source_coordinate =
                world_coordinate - static_cast<int64_t>(camera);
            const int64_t requested = source_coordinate -
                static_cast<int64_t>(SampleOffset(
                    presented_coordinate, source_extent, output_extent));
            const int64_t maximum = output_extent - source_extent;
            return static_cast<unsigned>(std::max<int64_t>(0,
                std::min(requested, maximum)));
        }

        unsigned EdgeAwareCrop(unsigned output_extent,
                               unsigned source_extent,
                               unsigned preferred_crop,
                               uint32_t camera,
                               uint32_t maximum_camera)
        {
            const unsigned extra = output_extent - source_extent;
            preferred_crop = std::min(preferred_crop, extra);
            if (maximum_camera == 0)
                return preferred_crop;
            const uint32_t clamped_camera = std::min(camera, maximum_camera);
            // When the two edge regions overlap, interpolate once across the
            // available travel instead of introducing a discontinuity.
            if (maximum_camera < extra)
                return static_cast<unsigned>(static_cast<uint64_t>(extra) *
                    clamped_camera / maximum_camera);
            const uint32_t distance_to_far_edge =
                maximum_camera - clamped_camera;
            if (clamped_camera < preferred_crop)
                return static_cast<unsigned>(clamped_camera);
            const unsigned far_margin = extra - preferred_crop;
            if (distance_to_far_edge < far_margin)
                return extra - static_cast<unsigned>(distance_to_far_edge);
            return preferred_crop;
        }

        void RebuildLookup()
        {
            const unsigned output_width = resolution::game_width;
            const unsigned output_height = resolution::screen_height;
            for (unsigned x = 0; x < output_width; ++x)
                x_lookup[x] = static_cast<unsigned>(
                    (static_cast<uint64_t>(2) * x + 1) *
                    transform.source_width /
                    (static_cast<uint64_t>(2) * output_width));
            for (unsigned y = 0; y < output_height; ++y)
                y_lookup[y] = static_cast<unsigned>(
                    (static_cast<uint64_t>(2) * y + 1) *
                    transform.source_height /
                    (static_cast<uint64_t>(2) * output_height));
            lookup_valid = true;
        }

        uint32_t CameraForOrigin(int64_t origin, unsigned output,
                                 unsigned source, unsigned preferred,
                                 uint32_t maximum)
        {
            const auto visible_origin = [=](uint32_t camera) -> int64_t {
                return static_cast<int64_t>(camera) +
                    EdgeAwareCrop(output, source, preferred, camera, maximum);
            };
            uint32_t low = 0, high = maximum;
            while (low < high)
            {
                const uint32_t middle = low + (high - low) / 2;
                if (visible_origin(middle) < origin)
                    low = middle + 1;
                else
                    high = middle;
            }
            if (low && std::abs(visible_origin(low - 1) - origin) <=
                       std::abs(visible_origin(low) - origin))
                --low;
            return low;
        }
    }

    bool Enabled()
    {
        LoadSettings();
        return settings.enabled;
    }

    void BeginMatch()
    {
        LoadSettings();
        current_zoom_units = settings.enabled ? settings.start_zoom_units : base_zoom_units;
        animation_start_units = target_zoom_units = current_zoom_units;
        animation_start_tick = GetTickCount();
        anchor = {};
        lookup_valid = false;
        wheel_remainder = 0;
    }

    bool Active()
    {
        return Enabled() && current_zoom_units > base_zoom_units;
    }

    unsigned Percentage()
    {
        LoadSettings();
        return (current_zoom_units + 50) / 100;
    }

    unsigned VisibleWidth()
    {
        return Active() ? SourceExtent(resolution::game_width) :
            static_cast<unsigned>(resolution::game_width);
    }

    unsigned VisibleHeight()
    {
        return Active() ? SourceExtent(resolution::game_height) :
            static_cast<unsigned>(resolution::game_height);
    }

    unsigned SourceLeft()
    {
        return Active() && lookup_valid ? transform.source_left : 0;
    }

    unsigned SourceScreenHeight()
    {
        return Active() ? SourceExtent(resolution::screen_height) :
            static_cast<unsigned>(resolution::screen_height);
    }

    unsigned SourceTop()
    {
        return Active() && lookup_valid ? transform.source_top : 0;
    }

    SelectionBounds VisibleSelectionBounds(unsigned camera_x, unsigned camera_y)
    {
        const unsigned left = camera_x + SourceLeft();
        const unsigned top = camera_y + SourceTop();
        return {left, top, left + VisibleWidth(), top + SourceScreenHeight()};
    }

    void UpdateTransform(uint32_t camera_x, uint32_t camera_y,
                         uint32_t map_width, uint32_t map_height,
                         bool advance_animation)
    {
        if (!Enabled())
            return;

        if (advance_animation)
            UpdateAnimation(GetTickCount());

        const unsigned source_width = SourceExtent(resolution::game_width);
        const unsigned source_height = SourceExtent(resolution::screen_height);
        unsigned preferred_left =
            (resolution::game_width - source_width) / 2;
        unsigned preferred_top =
            static_cast<unsigned>(resolution::camera_center_y) -
            static_cast<unsigned>(static_cast<uint64_t>(
                resolution::camera_center_y) * base_zoom_units /
                current_zoom_units);
        if (anchor.active)
        {
            preferred_left = AnchoredCrop(
                anchor.world_x, camera_x, anchor.presented_x,
                resolution::game_width, source_width);
            preferred_top = AnchoredCrop(
                anchor.world_y, camera_y, anchor.presented_y,
                resolution::screen_height, source_height);
        }
        const uint32_t maximum_camera_x = map_width > resolution::game_width ?
            map_width - resolution::game_width : 0;
        const uint32_t maximum_camera_y = map_height > resolution::game_height ?
            map_height - resolution::game_height : 0;
        const unsigned source_left = anchor.active ? preferred_left : EdgeAwareCrop(
            resolution::game_width, source_width, preferred_left,
            camera_x, maximum_camera_x);
        const unsigned source_top = anchor.active ? preferred_top : EdgeAwareCrop(
            resolution::screen_height, source_height, preferred_top,
            camera_y, maximum_camera_y);

        if (lookup_valid && transform.source_left == source_left &&
            transform.source_top == source_top &&
            transform.source_width == source_width &&
            transform.source_height == source_height)
            return;

        const bool rebuild = !lookup_valid ||
            transform.source_width != source_width ||
            transform.source_height != source_height;
        transform.source_left = source_left;
        transform.source_top = source_top;
        transform.source_width = source_width;
        transform.source_height = source_height;
        if (rebuild)
            RebuildLookup();
    }

    bool ResolveCameraAnchor(uint32_t camera_x, uint32_t camera_y,
                             uint32_t map_width, uint32_t map_height,
                             uint32_t *adjusted_x, uint32_t *adjusted_y)
    {
        if (!anchor.active || !lookup_valid ||
            !adjusted_x || !adjusted_y)
            return false;

        unsigned effective_left = transform.source_left;
        unsigned effective_top = transform.source_top;
        const bool transition_complete =
            current_zoom_units == target_zoom_units;
        if (transition_complete)
        {
            // Hand the final off-center crop to the engine camera in one
            // visually equivalent operation. This releases the anchor without
            // driving the integer camera on every animation frame.
            const unsigned centered_left =
                (resolution::game_width - transform.source_width) / 2;
            const unsigned centered_top =
                static_cast<unsigned>(resolution::camera_center_y) -
                static_cast<unsigned>(static_cast<uint64_t>(
                    resolution::camera_center_y) * base_zoom_units /
                    current_zoom_units);
            const uint32_t maximum_camera_x =
                map_width > resolution::game_width ?
                map_width - resolution::game_width : 0;
            const uint32_t maximum_camera_y =
                map_height > resolution::game_height ?
                map_height - resolution::game_height : 0;
            *adjusted_x = CameraForOrigin(anchor.world_x -
                SampleOffset(anchor.presented_x, transform.source_width,
                             resolution::game_width),
                resolution::game_width, transform.source_width,
                centered_left, maximum_camera_x);
            *adjusted_y = CameraForOrigin(anchor.world_y -
                SampleOffset(anchor.presented_y, transform.source_height,
                             resolution::screen_height),
                resolution::screen_height, transform.source_height,
                centered_top, maximum_camera_y);
            anchor.active = false;
            // Publish the matching crop even if the engine camera need not
            // move. Do not advance the animation clock twice in one frame.
            UpdateTransform(*adjusted_x, *adjusted_y, map_width, map_height, false);
            return *adjusted_x != camera_x || *adjusted_y != camera_y;
        }

        const int64_t current_world_x =
            static_cast<int64_t>(camera_x) + effective_left +
            SampleOffset(anchor.presented_x, transform.source_width,
                         resolution::game_width);
        const int64_t current_world_y =
            static_cast<int64_t>(camera_y) + effective_top +
            SampleOffset(anchor.presented_y, transform.source_height,
                         resolution::screen_height);
        const int64_t requested_x = static_cast<int64_t>(camera_x) +
            anchor.world_x - current_world_x;
        const int64_t requested_y = static_cast<int64_t>(camera_y) +
            anchor.world_y - current_world_y;
        const uint32_t maximum_x = map_width > resolution::game_width ?
            map_width - resolution::game_width : 0;
        const uint32_t maximum_y = map_height > resolution::game_height ?
            map_height - resolution::game_height : 0;
        *adjusted_x = static_cast<uint32_t>(std::max<int64_t>(0,
            std::min<int64_t>(requested_x, maximum_x)));
        *adjusted_y = static_cast<uint32_t>(std::max<int64_t>(0,
            std::min<int64_t>(requested_y, maximum_y)));

        const bool changed = *adjusted_x != camera_x ||
            *adjusted_y != camera_y;
        if (transition_complete)
            anchor.active = false;
        return changed;
    }

    void ScaleBattlefield(uint8_t *frame, uint8_t *scratch)
    {
        if (!Active() || !frame || !scratch)
            return;
        if (!lookup_valid)
            UpdateTransform(0, 0, resolution::game_width,
                            resolution::game_height);

        const unsigned pitch = resolution::screen_width;
        const unsigned source_width = transform.source_width;
        const unsigned source_height = transform.source_height;
        for (unsigned y = 0; y < source_height; ++y)
        {
            memcpy(scratch + static_cast<size_t>(y) * source_width,
                   frame + static_cast<size_t>(transform.source_top + y) *
                       pitch + transform.source_left,
                   source_width);
        }

        for (unsigned y = 0; y < resolution::screen_height; ++y)
        {
            const uint8_t *source_row = scratch +
                static_cast<size_t>(y_lookup[y]) * source_width;
            uint8_t *destination_row = frame +
                static_cast<size_t>(y) * pitch;
            for (unsigned x = 0; x < resolution::game_width; ++x)
                destination_row[x] = source_row[x_lookup[x]];
        }
    }

    Point PresentedToSource(int x, int y)
    {
        if (!Active() || !lookup_valid)
            return { x, y };
        x = std::max(0, std::min(x,
            static_cast<int>(resolution::game_width) - 1));
        y = std::max(0, std::min(y,
            static_cast<int>(resolution::screen_height) - 1));
        return {
            static_cast<int>(transform.source_left + x_lookup[x]),
            static_cast<int>(transform.source_top + y_lookup[y])
        };
    }

    Point SourceToPresented(int x, int y)
    {
        if (!Active() || !lookup_valid)
            return { x, y };
        const int relative_x = x - static_cast<int>(transform.source_left);
        const int relative_y = y - static_cast<int>(transform.source_top);
        const int output_x = relative_x *
            static_cast<int>(resolution::game_width) /
            static_cast<int>(transform.source_width);
        const int output_y = relative_y *
            static_cast<int>(resolution::screen_height) /
            static_cast<int>(transform.source_height);
        return {
            std::max(0, std::min(output_x,
                static_cast<int>(resolution::game_width) - 1)),
            std::max(0, std::min(output_y,
                static_cast<int>(resolution::screen_height) - 1))
        };
    }

    Point CameraForMinimapPoint(uint32_t world_x, uint32_t world_y,
                               uint32_t map_width, uint32_t map_height)
    {
        const unsigned width = SourceExtent(resolution::game_width);
        const unsigned height = SourceExtent(resolution::screen_height);
        const unsigned left = (resolution::game_width - width) / 2;
        const unsigned top = static_cast<unsigned>(resolution::camera_center_y) -
            static_cast<unsigned>(static_cast<uint64_t>(resolution::camera_center_y) *
                                  base_zoom_units / current_zoom_units);
        const uint32_t maximum_x = map_width > resolution::game_width ?
            map_width - resolution::game_width : 0;
        const uint32_t maximum_y = map_height > resolution::game_height ?
            map_height - resolution::game_height : 0;
        anchor.active = false;
        return {
            static_cast<int>(CameraForOrigin(static_cast<int64_t>(world_x) -
                width / 2, resolution::game_width, width, left, maximum_x)),
            static_cast<int>(CameraForOrigin(static_cast<int64_t>(world_y) -
                (VisibleHeight() + 16) / 2,
                resolution::screen_height, height, top, maximum_y))
        };
    }

    bool AdjustByWheel(int delta, int presented_x, int presented_y,
                       uint32_t camera_x, uint32_t camera_y)
    {
        if (!Enabled() || delta == 0)
            return false;

        const unsigned maximum = MaximumZoomUnits();
        unsigned next_target = target_zoom_units;
        wheel_remainder += delta;
        while (wheel_remainder >= WHEEL_DELTA)
        {
            next_target = pixel_zoom_steps::RegularNext(pixel_levels,next_target,true,maximum);
            wheel_remainder -= WHEEL_DELTA;
        }
        while (wheel_remainder <= -WHEEL_DELTA)
        {
            next_target = pixel_zoom_steps::RegularNext(pixel_levels,next_target,false,maximum);
            wheel_remainder += WHEEL_DELTA;
        }

        if (next_target != target_zoom_units)
        {
            const Point source = PresentedToSource(
                presented_x, presented_y);
            anchor.active = true;
            anchor.presented_x = std::max(0, std::min(presented_x,
                static_cast<int>(resolution::game_width) - 1));
            anchor.presented_y = std::max(0, std::min(presented_y,
                static_cast<int>(resolution::screen_height) - 1));
            anchor.world_x = static_cast<int64_t>(camera_x) + source.x;
            anchor.world_y = static_cast<int64_t>(camera_y) + source.y;
            animation_start_units = current_zoom_units;
            target_zoom_units = next_target;
            animation_start_tick = GetTickCount();
        }
        return true;
    }
}
