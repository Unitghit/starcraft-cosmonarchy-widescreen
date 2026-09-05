// Frozen pre-fix characterization, not part of the renderer or release gate.
// The runner verifies the archived function fingerprint.
#define NOMINMAX
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>
#include "../ZoomSource/Cosmonarchy-aidebug-resolution/src/resolution.h"

static std::vector<uint8_t> pixels, previous_world_frame;
static uint8_t *fake_screenbuf;
static uint32_t previous_world_camera_x, previous_world_camera_y;
static bool previous_world_frame_valid;
static size_t expanded_frame_size()
{
    return static_cast<size_t>(resolution::screen_width) * resolution::screen_height;
}

    void RepairMovingFullWidthPassEdges(uint32_t camera_x,
                                        uint32_t camera_y,
                                        bool middle_pan_active)
    {
        const size_t frame_size = expanded_frame_size();
        if (previous_world_frame.size() != frame_size)
        {
            previous_world_frame.resize(frame_size);
            previous_world_frame_valid = false;
        }

        if (middle_pan_active && previous_world_frame_valid &&
            resolution::tile_width == resolution::native_width &&
            resolution::tile_columns > 1)
        {
            const int64_t delta_x = static_cast<int64_t>(camera_x) -
                previous_world_camera_x;
            const int64_t delta_y = static_cast<int64_t>(camera_y) -
                previous_world_camera_y;
            constexpr unsigned stale_edge_width = 4;

            for (unsigned column = 1;
                 column < resolution::tile_columns; ++column)
            {
                const unsigned seam_x =
                    column * resolution::tile_width;
                if (seam_x < stale_edge_width ||
                    seam_x > resolution::screen_width)
                    continue;

                for (unsigned y = 0; y < resolution::screen_height; ++y)
                {
                    const int64_t source_y =
                        static_cast<int64_t>(y) + delta_y;
                    if (source_y < 0 ||
                        source_y >= resolution::screen_height)
                        continue;

                    for (unsigned x = seam_x - stale_edge_width;
                         x < seam_x; ++x)
                    {
                        const int64_t source_x =
                            static_cast<int64_t>(x) + delta_x;
                        if (source_x < 0 ||
                            source_x >= resolution::screen_width)
                            continue;
                        fake_screenbuf[
                            static_cast<size_t>(y) *
                                resolution::screen_width + x] =
                            previous_world_frame[
                                static_cast<size_t>(source_y) *
                                    resolution::screen_width +
                                static_cast<size_t>(source_x)];
                    }
                }
            }
        }

        memcpy(previous_world_frame.data(), fake_screenbuf, frame_size);
        previous_world_camera_x = camera_x;
        previous_world_camera_y = camera_y;
        previous_world_frame_valid = true;
    }
// End production excerpt

static void Reset(unsigned width, unsigned height)
{
    resolution::Configure(width, height);
    // Preserve pre-fix 640-wide geometry now that production uses guard bands.
    resolution::tile_columns = (width + 639) / 640;
    const unsigned old_width = (width + resolution::tile_columns - 1) / resolution::tile_columns;
    resolution::tile_width = (old_width + 7) / 8 * 8;
    pixels.assign(expanded_frame_size(), 10);
    fake_screenbuf = pixels.data();
    previous_world_frame.clear();
    previous_world_frame_valid = false;
}
static bool Case(unsigned width, unsigned height)
{
    Reset(width, height);
    RepairMovingFullWidthPassEdges(1000, 1000, false);
    size_t bad = 0;
    for (unsigned frame = 0; frame < 120; ++frame)
    {
        // Every newly rendered world pixel changed. Old palette indices must
        // not survive in fresh terrain, sprites, fog, or effects.
        std::fill(pixels.begin(), pixels.end(), 20);
        RepairMovingFullWidthPassEdges(1000, 1000, true);
    }
    bad = std::count(pixels.begin(), pixels.end(), 10);
    const size_t expected = resolution::tile_width == resolution::native_width ?
        static_cast<size_t>(resolution::tile_columns - 1) * 4 * height : 0;
    std::printf("%ux%u: grid=%ux%u tile=%ux%u old pixels after 120 held frames=%zu\n",
        width, height, resolution::tile_columns, resolution::tile_rows,
        static_cast<unsigned>(resolution::tile_width),
        static_cast<unsigned>(resolution::tile_height), bad);
    std::fill(pixels.begin(), pixels.end(), 20);
    RepairMovingFullWidthPassEdges(1000, 1000, false);
    return bad == expected &&
        std::count(pixels.begin(), pixels.end(), 10) == 0;
}
int main()
{
    bool reproduced = true;
    for (auto size : { std::pair{1280u,720u}, {1600u,900u},
                       {1920u,1080u}, {2560u,1440u}, {3840u,2160u} })
        reproduced &= Case(size.first, size.second);
    Reset(1920,1080);
    pixels[200 * 1920 + 638] = 42; // A sprite that disappears next frame.
    RepairMovingFullWidthPassEdges(1000,1000,false);
    bool vertical_ghost = true;
    for (unsigned frame = 1; frame <= 100; ++frame)
    {
        std::fill(pixels.begin(), pixels.end(), 10);
        RepairMovingFullWidthPassEdges(1000,1000 + frame,true);
        vertical_ghost &= pixels[(200 - frame) * 1920 + 638] == 42;
    }
    std::printf("vertical pan: disappeared sprite retained for 100 frames=%s\n",
        vertical_ghost ? "yes" : "no");
    reproduced &= vertical_ghost;
    std::puts(reproduced ? "History feedback defect reproduced: YES" :
                           "Characterization changed: investigate");
    return reproduced ? 0 : 1;
}
