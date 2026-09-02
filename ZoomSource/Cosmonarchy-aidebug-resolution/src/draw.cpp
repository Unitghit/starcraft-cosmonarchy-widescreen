#include "draw.h"
#include <cmath>
#include <cstdio>
#include <cstring>

#include "offsets.h"
#include "memory.h"
#include "limits.h"
#include "player.h"
#include "scconsole.h"
#include "unit_type.h"
#include "runtime_diagnostics.h"
#include <vector>
#include "yms.h"

// Every fopen call in this translation unit writes diagnostic output.
#define fopen runtime_diagnostics::Open

std::atomic<uintptr_t> draw_counter;
bool all_visions;
int frame_skip_ms = 0;
uint32_t fastForwardStartFrames = 0;
uint32_t fastForwardEndFrames = 0;
int fastForwardProgressCount = 0;
bool isFastForwarding = false;
uint32_t last_frame_skip_tick = 0;

class drawhook
{
    public:
        drawhook(void (*f)(uint8_t *, xuint, yuint), int p) { func = f; priority = p; }
        bool operator<(const drawhook &other) const { return priority < other.priority; }

        void (*func)(uint8_t *, xuint, yuint);
        int priority;
};
std::vector<drawhook> draw_hooks;

#include "console/windows_wrap.h"

uint8_t fake_screenbuf[resolution::maximum_frame_size];

// As sc only draws the parts of screen marked dirty,
// this adds an additional buffer to which has the original
// image without any of the draw hook additions.
// So the draw hooks do not have to mark areas dirty or
// anything, but it also makes drawing a lot slower.
uint8_t fake_screenbuf_2[resolution::maximum_frame_size];

namespace
{
    constexpr size_t native_frame_size =
        resolution::native_width * resolution::native_height;
    size_t expanded_frame_size()
    {
        return static_cast<size_t>(resolution::screen_width) *
            static_cast<unsigned>(resolution::screen_height);
    }

    uint8_t native_present[native_frame_size];
    uint8_t native_work[native_frame_size];
    uint8_t native_tile[native_frame_size];
    uint8_t native_game_reference[native_frame_size];
    uint8_t native_current_ui_frame[native_frame_size];
    uint8_t native_popup_reference[native_frame_size];
    uint8_t native_popup_frame[native_frame_size];
    uint8_t native_game_text_frame[native_frame_size];
    uint8_t native_stock_frame[native_frame_size];
    uint8_t native_inner_frame[native_frame_size];
    std::vector<uint8_t> previous_world_frame;
    uint32_t previous_world_camera_x;
    uint32_t previous_world_camera_y;
    bool previous_world_frame_valid;
    bool replay_tunit_load_attempted;
    bool replay_tunit_loaded;
    uint8_t replay_tunit_colors[256 * 8];
    uint8_t cached_menu_source[native_frame_size];
    bool cached_menu_frame_valid;
    unsigned cached_menu_screen_width;
    unsigned cached_menu_screen_height;
    unsigned cached_menu_width;
    unsigned cached_menu_height;
    unsigned cached_menu_left;
    unsigned cached_menu_top;
    bool recursive_stock_draw;
    unsigned stock_draw_depth;
    uint8_t saved_cursor_layer_draw;
    uint8_t saved_selection_layer_draw;
    bool cursor_layer_suppressed;
    bool selection_layer_suppressed;
    int expanded_cursor_offset_x;
    int expanded_cursor_offset_y;

    uint32_t fnv1a(const uint8_t *data, size_t size);

    typedef void (__thiscall *DrawGptpGraphicProc)(void *, int);

    struct GptpMapGraphicsState
    {
        DrawGptpGraphicProc draw = nullptr;
        uint8_t *module = nullptr;
        bool resolved = false;
    };

    GptpMapGraphicsState gptp_map_graphics;

    bool ResolveGptpMapGraphics()
    {
        if (gptp_map_graphics.resolved)
            return gptp_map_graphics.draw != nullptr;
        gptp_map_graphics.resolved = true;

        HMODULE module = GetModuleHandleA("gptp.qdp");
        if (!module)
            module = GetModuleHandleA("CM-GPTP-Release.qdp");
        if (!module)
            return false;

        uint8_t *base = reinterpret_cast<uint8_t *>(module);
        // Stable GPTP CC6BF422... has its layer-5 wrapper at RVA 0xD5620
        // and its 40-byte graphic draw method at RVA 0xED090. Validate both
        // entry points before touching the internal graphic vectors.
        const uint8_t wrapper_signature[] =
            { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8, 0x51, 0x80 };
        const uint8_t draw_signature[] =
            { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8, 0x83, 0xEC, 0x2C };
        if (memcmp(base + 0xD5620, wrapper_signature,
                   sizeof(wrapper_signature)) != 0 ||
            memcmp(base + 0xED090, draw_signature,
                   sizeof(draw_signature)) != 0)
        {
            return false;
        }

        gptp_map_graphics.module = base;
        gptp_map_graphics.draw =
            reinterpret_cast<DrawGptpGraphicProc>(base + 0xED090);
        return true;
    }

    bool ValidGptpGraphicVector(uint8_t *begin, uint8_t *end)
    {
        if (begin == end)
            return true;
        if (!begin || !end)
            return false;
        const uintptr_t begin_address = reinterpret_cast<uintptr_t>(begin);
        const uintptr_t end_address = reinterpret_cast<uintptr_t>(end);
        if (end_address < begin_address)
            return false;
        const size_t bytes = static_cast<size_t>(
            end_address - begin_address);
        return bytes % 0x28 == 0 && bytes <= 0x28 * 4096;
    }

    void DrawGptpMapGraphics(uint8_t *destination, uint16_t width,
                             uint16_t height)
    {
        if (!destination || !ResolveGptpMapGraphics())
            return;

        // The three vectors are traversed by GPTP's layer-5 wrapper with
        // draw variants 0, 1, and 1. Graphic field +0x04 is its coordinate
        // mode. Mode 1 subtracts the live camera origin and is ON_MAP.
        const uintptr_t vector_offsets[][3] =
        {
            { 0x25A1F0, 0x25A1F4, 0 },
            { 0x25A208, 0x25A20C, 1 },
            { 0x25A214, 0x25A218, 1 },
        };

        uint8_t *frame_state =
            reinterpret_cast<uint8_t *>(&bw::draw_layers[0]) +
            sizeof(DrawLayer) * 8;
        uint16_t saved_width = 0;
        uint16_t saved_height = 0;
        uint8_t *saved_pixels = nullptr;
        memcpy(&saved_width, frame_state, sizeof(saved_width));
        memcpy(&saved_height, frame_state + 2, sizeof(saved_height));
        memcpy(&saved_pixels, frame_state + 4, sizeof(saved_pixels));
        memcpy(frame_state, &width, sizeof(width));
        memcpy(frame_state + 2, &height, sizeof(height));
        memcpy(frame_state + 4, &destination, sizeof(destination));

        for (const auto &vector : vector_offsets)
        {
            uint8_t *begin = nullptr;
            uint8_t *end = nullptr;
            memcpy(&begin, gptp_map_graphics.module + vector[0],
                   sizeof(begin));
            memcpy(&end, gptp_map_graphics.module + vector[1], sizeof(end));
            if (!ValidGptpGraphicVector(begin, end))
                continue;

            for (uint8_t *graphic = begin; graphic < end; graphic += 0x28)
            {
                uint32_t coordinate_mode = 0;
                memcpy(&coordinate_mode, graphic + 4,
                       sizeof(coordinate_mode));
                if (coordinate_mode == 1)
                    gptp_map_graphics.draw(graphic,
                        static_cast<int>(vector[2]));
            }
        }

        memcpy(frame_state, &saved_width, sizeof(saved_width));
        memcpy(frame_state + 2, &saved_height, sizeof(saved_height));
        memcpy(frame_state + 4, &saved_pixels, sizeof(saved_pixels));
    }

    struct PopupBounds
    {
        int left;
        int top;
        int right;
        int bottom;
        bool valid;

        bool Contains(unsigned x, unsigned y) const
        {
            return valid && static_cast<int>(x) >= left &&
                static_cast<int>(x) < right &&
                static_cast<int>(y) >= top &&
                static_cast<int>(y) < bottom;
        }
    };

    void ForceNativeRedraw();

    PopupBounds GetPopupBounds(bool popup_active)
    {
        PopupBounds result = { 0, 0, 0, 0, false };
        if (!popup_active || !*bw::popup_dialog)
            return result;

        // BinDlg begins with next, then an inclusive left/top/right/bottom
        // DialogBounds at offset 4. BIN files store x/y/width/height, but
        // Cosmonarchy converts them during dialog initialization.
        const uint8_t *dialog = reinterpret_cast<const uint8_t *>(
            *bw::popup_dialog);
        int16_t x = 0;
        int16_t y = 0;
        int16_t right = 0;
        int16_t bottom = 0;
        memcpy(&x, dialog + 4, sizeof(x));
        memcpy(&y, dialog + 6, sizeof(y));
        memcpy(&right, dialog + 8, sizeof(right));
        memcpy(&bottom, dialog + 10, sizeof(bottom));
        if (right < x || bottom < y)
            return result;

        result.left = std::max(0, static_cast<int>(x));
        result.top = std::max(0, static_cast<int>(y));
        result.right = std::min(
            static_cast<int>(resolution::native_width),
            static_cast<int>(right) + 1);
        result.bottom = std::min(
            static_cast<int>(resolution::native_height),
            static_cast<int>(bottom) + 1);
        result.valid = result.left < result.right &&
            result.top < result.bottom;
        return result;
    }

    uint8_t GetActiveConsoleRace()
    {
        const uint32_t local_player = *bw::local_player_id;
        if (local_player < 12)
            return bw::players[local_player].race;
        return *bw::player_race;
    }

    bool IsDiscardedNativeHudOrnament(unsigned x, unsigned y,
                                      uint8_t console_race)
    {
        // Cosmonarchy draws a small rusty-pipe ornament above the native
        // console bitmap as an independent screen-space overlay. In the
        // stock 4:3 layout it protrudes from the lower-left edge; in the
        // expanded compositor it becomes a chopped-off island at the far
        // left. Diagnostics isolate the component to x=0..22, y=293..313.
        // That piece is unwanted only for the Terran console. It completes
        // the Protoss and Zerg minimap surround and must follow their HUD.
        const bool corner_piece =
            x <= 22 && y >= 293 && y <= 313;
        const bool keep_for_race =
            console_race == Race::Zerg || console_race == Race::Protoss;
        return corner_piece && !keep_for_race;
    }

    bool IsRelocatedNativeHudProtrusion(unsigned x, unsigned y,
                                        uint8_t console_race)
    {
        if (y >= resolution::native_hud_protrusion_top)
            return true;

        // A native-vs-game-only subtraction of the Zerg first-frame capture
        // isolates a 14-pixel cap at x=0..5, y=290..292. It connects directly
        // to the shared protrusion beginning at y=293. Without this exact
        // race-specific extension, the top-UI path leaves those three rows at
        // x+native_ui_left in the middle-left of the expanded battlefield.
        return console_race == Race::Zerg && x <= 5 &&
            y >= 290 && y <= 292;
    }

    bool NativeUiPixelIsOpaque(unsigned x, unsigned y)
    {
        if (!*bw::trans_list ||
            x >= resolution::native_width ||
            y >= resolution::native_height)
        {
            return false;
        }

        // StarCraft already maintains an explicit transparency mask for the
        // native dialog/UI surface.  Use that ownership mask when extracting
        // the HUD instead of inferring transparency from pixel colors.  A UI
        // border pixel can legitimately have the same palette index as the
        // world behind it; color-difference extraction dropped those pixels
        // and made tooltip edges alternate as the terrain/reference changed.
        if (static_cast<int>(y) < *bw::trans_mouse_y_min)
            return false;
        if (static_cast<int>(y) >= *bw::trans_mouse_y_max)
            return true;
        return bw::STransGetPixel(*bw::trans_list,
                                  static_cast<int>(x),
                                  static_cast<int>(y)) == 0;
    }

    void DrawExpandedCursor(uint8_t *destination)
    {
        DrawLayer &cursor = bw::draw_layers[0];
        const int mouse_x = static_cast<int>(*bw::mouse_clickpos_x);
        const int mouse_y = static_cast<int>(*bw::mouse_clickpos_y);
        const uint32_t cursor_type = *bw::cursor_type;
        void *cursor_grp = *bw::current_cursor;
        const bool inside_native_game = mouse_x >= 0 && mouse_y >= 0 &&
            mouse_x < static_cast<int>(resolution::native_width) &&
            mouse_y < static_cast<int>(resolution::native_game_height);
        static uint32_t last_cursor_type = UINT32_MAX;
        static void *last_cursor_grp;
        static int last_native_state = -1;
        if (cursor_type != last_cursor_type || cursor_grp != last_cursor_grp ||
            static_cast<int>(inside_native_game) != last_native_state)
        {
            FILE *log = fopen("fixed_zoom_cursor_hover.log", "a");
            if (log)
            {
                fprintf(log,
                    "%lu mouse=(%d,%d) type=%lu grp=%p "
                    "native-game=%u expanded-game=%u popup=%lu "
                    "placing=%lu drag=%u layer=(draw=%u pos=%d,%d "
                    "size=%d,%d)\n",
                    static_cast<unsigned long>(GetTickCount()),
                    mouse_x, mouse_y,
                    static_cast<unsigned long>(cursor_type), cursor_grp,
                    static_cast<unsigned>(inside_native_game),
                    static_cast<unsigned>(mouse_x >= 0 && mouse_y >= 0 &&
                        mouse_x < static_cast<int>(resolution::game_width) &&
                        mouse_y < static_cast<int>(resolution::screen_height)),
                    static_cast<unsigned long>(*bw::popup_dialog_active),
                    static_cast<unsigned long>(*bw::is_placing_building),
                    static_cast<unsigned>(*bw::is_drag_selecting),
                    static_cast<unsigned>(cursor.draw),
                    static_cast<int>(cursor.area.left),
                    static_cast<int>(cursor.area.top),
                    static_cast<int>(cursor.area.right),
                    static_cast<int>(cursor.area.bottom));
                fclose(log);
            }
            last_cursor_type = cursor_type;
            last_cursor_grp = cursor_grp;
            last_native_state = static_cast<int>(inside_native_game);
        }
        // The native framebuffer retains an already-rasterized cursor when
        // StarCraft temporarily clears the layer redraw flag. Our expanded
        // frame is composed from scratch, so keep using the prepared layer
        // throughout a captured minimap drag or the pointer disappears for a
        // frame on press.
        if ((!cursor.draw && !IsTranslatedMinimapDragActive()) || !cursor.Draw)
            return;

        // The stock cursor rasterizer already clips against current_canvas,
        // so give it the final output surface rather than the native private
        // frame. This avoids both the 640-pixel clipping and the stock cursor
        // save/restore path using a different pitch from the presented frame.
        Surface expanded_canvas = {
            static_cast<x16u>(resolution::screen_width),
            static_cast<y16u>(resolution::screen_height),
            destination
        };
        DrawParam param;
        param.area = Rect16(0, 0,
            static_cast<int16_t>(resolution::screen_width - 1),
            static_cast<int16_t>(resolution::screen_height - 1));
        param.w = static_cast<x16u>(resolution::screen_width);
        param.h = static_cast<y16u>(resolution::screen_height);

        Surface *saved_canvas = *bw::current_canvas;
        *bw::current_canvas = &expanded_canvas;
        // Input over the centered HUD is translated to native control
        // coordinates. During a captured minimap drag, native cursor polling
        // can alternate the prepared layer rectangle between native and
        // already-relocated coordinates. Apply the HUD offset only when the
        // current layer is closer to the native mouse point. Applying it to an
        // already-relocated rectangle makes the rasterizer clip every pixel.
        int draw_cursor_offset_x = expanded_cursor_offset_x;
        int draw_cursor_offset_y = expanded_cursor_offset_y;
        if (expanded_cursor_offset_x != 0 || expanded_cursor_offset_y != 0)
        {
            const int native_distance =
                std::abs(static_cast<int>(cursor.area.left) - mouse_x) +
                std::abs(static_cast<int>(cursor.area.top) - mouse_y);
            const int physical_distance = std::abs(
                static_cast<int>(cursor.area.left) -
                    (mouse_x + expanded_cursor_offset_x)) +
                std::abs(static_cast<int>(cursor.area.top) -
                    (mouse_y + expanded_cursor_offset_y));
            if (physical_distance < native_distance)
            {
                draw_cursor_offset_x = 0;
                draw_cursor_offset_y = 0;
            }
        }
        (*cursor.Draw)(-draw_cursor_offset_x,
                       -draw_cursor_offset_y,
                       cursor.func_param, &param);
        *bw::current_canvas = saved_canvas;
    }

    void DrawExpandedContextHelp(uint8_t *destination)
    {
        DrawLayer &context = bw::draw_layers[1];
        if (!context.Draw)
            return;

        // Live capture establishes that layer 1 owns a 160x480 backing
        // surface.  context.area is the smaller prepared tooltip rectangle
        // within it (for example 145x35 or 158x55).  The earlier 160x92
        // inference rejected every native surface, leaving cache_valid false
        // precisely when multiselection help alternated visible/hidden.
        constexpr unsigned context_cache_width = 160;
        constexpr unsigned context_cache_height =
            resolution::native_height;
        static uint8_t cached_pixels[
            context_cache_width * context_cache_height];
        static Surface cached_surface = {};
        static Rect<int16_t> cached_area;
        static bool cache_valid;
        // context.draw and context_help_visible are both cleared during the
        // transient no-control polls produced by transparent unit-stat art.
        // Do not return on context.draw before consulting the stable cached
        // owner below; that made the hold path unreachable on exactly the
        // frames it was designed to cover.  Building armor icons never enter
        // this transient state, which is why their tooltips were already
        // visually solid.
        const bool live_context = context.draw != 0 &&
            *bw::context_help_visible != 0 && context.func_param != nullptr;
        Surface *live_surface = static_cast<Surface *>(context.func_param);
        if (live_context && live_surface && live_surface->image &&
            live_surface->w > 0 && live_surface->h > 0 &&
            live_surface->w <= context_cache_width &&
            live_surface->h <= context_cache_height)
        {
            cached_surface.w = live_surface->w;
            cached_surface.h = live_surface->h;
            cached_surface.image = cached_pixels;
            memcpy(cached_pixels, live_surface->image,
                static_cast<size_t>(live_surface->w) * live_surface->h);
            cached_area = context.area;
            cache_valid = true;
        }

        const bool held_context = !live_context && cache_valid &&
            ShouldHoldExpandedContextHelp();

        if (!live_context && !held_context)
        {
            if (!ShouldHoldExpandedContextHelp())
                cache_valid = false;
            return;
        }

        // Layer 1 is StarCraft's dedicated 160x480 context-help backing
        // surface. Its
        // live left/top/width/height are prepared by the tooltip owner at
        // 0x4813D0..0x48147B.  It is suppressed from the stock frame because
        // a screen-space layer cannot be allowed into every private world
        // pass; render that complete surface exactly once after composition.
        Surface expanded_canvas = {
            static_cast<x16u>(resolution::screen_width),
            static_cast<y16u>(resolution::screen_height),
            destination
        };
        DrawParam param;
        param.area = Rect16(0, 0,
            static_cast<int16_t>(resolution::screen_width - 1),
            static_cast<int16_t>(resolution::screen_height - 1));
        param.w = static_cast<x16u>(resolution::screen_width);
        param.h = static_cast<y16u>(resolution::screen_height);

        const Rect<int16_t> saved_area = context.area;
        void *saved_param = context.func_param;
        const uint32_t saved_visible = *bw::context_help_visible;
        if (held_context)
        {
            context.area = cached_area;
            context.func_param = &cached_surface;
            *bw::context_help_visible = 1;
        }
        const int16_t saved_left = context.area.left;
        const int16_t saved_top = context.area.top;
        const int horizontal_offset = *bw::popup_dialog_active ?
            static_cast<int>(resolution::native_ui_left) :
            static_cast<int>(resolution::hud_left);
        const int vertical_offset = *bw::popup_dialog_active ?
            static_cast<int>(resolution::native_ui_top) :
            static_cast<int>(resolution::screen_height -
                resolution::native_height);
        context.area.left = static_cast<int16_t>(
            static_cast<int>(saved_left) + horizontal_offset);
        context.area.top = static_cast<int16_t>(
            static_cast<int>(saved_top) + vertical_offset);

        Surface *saved_canvas = *bw::current_canvas;
        *bw::current_canvas = &expanded_canvas;
        (*context.Draw)(0, 0, context.func_param, &param);
        *bw::current_canvas = saved_canvas;
        context.area = saved_area;
        context.func_param = saved_param;
        *bw::context_help_visible = saved_visible;
    }

    void DrawExpandedSelectionBox(uint8_t *destination)
    {
        if (!*bw::is_drag_selecting)
            return;

        // StarCraft normally draws this rectangle from ScreenUpdateProc.
        // That call runs once per private camera pass, so leaving it enabled
        // stamps the same screen-space border into every composed tile. Draw
        // the inclusive rectangle directly once on the completed frame.
        const Rect16 &box = *bw::selection_box;
        int left = static_cast<int>(box.left);
        int top = static_cast<int>(box.top);
        int right = static_cast<int>(box.right);
        int bottom = static_cast<int>(box.bottom);
        if (left > right)
            std::swap(left, right);
        if (top > bottom)
            std::swap(top, bottom);

        left = std::max(0, std::min(left,
            static_cast<int>(resolution::game_width) - 1));
        right = std::max(0, std::min(right,
            static_cast<int>(resolution::game_width) - 1));
        top = std::max(0, std::min(top,
            static_cast<int>(resolution::screen_height) - 1));
        bottom = std::max(0, std::min(bottom,
            static_cast<int>(resolution::screen_height) - 1));

        const uint8_t color = *bw::selection_box_color;
        uint8_t *top_row = destination +
            static_cast<size_t>(top) * resolution::screen_width;
        uint8_t *bottom_row = destination +
            static_cast<size_t>(bottom) * resolution::screen_width;
        memset(top_row + left, color,
               static_cast<size_t>(right - left + 1));
        if (bottom != top)
        {
            memset(bottom_row + left, color,
                   static_cast<size_t>(right - left + 1));
        }
        for (int y = top + 1; y < bottom; ++y)
        {
            uint8_t *row = destination +
                static_cast<size_t>(y) * resolution::screen_width;
            row[left] = color;
            if (right != left)
                row[right] = color;
        }
    }

    void DrawExpandedGameText(uint8_t *destination)
    {
        // StarCraft uses Surface::w as both row pitch and UI layout width.
        // Pointing it at the 1280-wide final frame therefore gives the right
        // pitch but the wrong anchors: native top-left objectives and
        // top-right resources no longer occupy opposite sides of one 640-wide
        // UI region. Render onto an actual native surface seeded with the
        // pixels beneath the configured 640-wide top-UI origin, then copy the
        // completed box back with the expanded destination pitch. In centered
        // mode that origin is native_ui_left; in screen-edge mode it is zero.
        // Top-screen information remains flush with y=0; unlike modal dialogs,
        // it does not use the centered box's vertical offset.
        for (unsigned y = 0; y < resolution::native_height; ++y)
        {
            memcpy(native_game_text_frame +
                       static_cast<size_t>(y) * resolution::native_width,
                   destination +
                       static_cast<size_t>(y) * resolution::screen_width +
                       resolution::top_ui_left,
                   resolution::native_width);
        }
        Surface text_canvas = {
            static_cast<x16u>(resolution::native_width),
            static_cast<y16u>(resolution::native_height),
            native_game_text_frame
        };
        Surface *saved_canvas = *bw::current_canvas;
        *bw::current_canvas = &text_canvas;
        reinterpret_cast<void (__cdecl *)()>(0x0048CF60)();
        *bw::current_canvas = saved_canvas;
        for (unsigned y = 0; y < resolution::native_height; ++y)
        {
            memcpy(destination +
                       static_cast<size_t>(y) * resolution::screen_width +
                       resolution::top_ui_left,
                   native_game_text_frame +
                       static_cast<size_t>(y) * resolution::native_width,
                   resolution::native_width);
        }
    }

    void TracePostRenderer(const char *stage)
    {
        FILE *log = fopen("fixed_zoom_renderer.log", "a");
        if (!log)
            return;
        fprintf(log, "post-render trace: %s\n", stage);
        fclose(log);
    }

    bool CapturePhysicalNative(uint8_t *destination, int *physical_pitch)
    {
        uint8_t *surface = nullptr;
        int pitch = 0;
        if (!(*bw::SDrawLockSurface_Import)(0, 0, &surface, &pitch, 0) ||
            !surface || pitch < static_cast<int>(resolution::native_width))
        {
            return false;
        }
        for (unsigned y = 0; y < resolution::native_height; ++y)
        {
            memcpy(destination + static_cast<size_t>(y) *
                       resolution::native_width,
                   surface + static_cast<size_t>(y) * pitch,
                   resolution::native_width);
        }
        (*bw::SDrawUnlockSurface_Import)(0, surface, 0, 0);
        if (physical_pitch)
            *physical_pitch = pitch;
        return true;
    }

    bool PresentExpandedFrame(int *physical_pitch)
    {
        uint8_t *surface = nullptr;
        int pitch = 0;
        if (!(*bw::SDrawLockSurface_Import)(0, 0, &surface, &pitch, 0) ||
            !surface || pitch < static_cast<int>(resolution::screen_width))
        {
            return false;
        }
        if (pitch == static_cast<int>(resolution::screen_width))
        {
            memcpy(surface, fake_screenbuf_2, expanded_frame_size());
        }
        else
        {
            for (unsigned y = 0; y < resolution::screen_height; ++y)
            {
                memcpy(surface + static_cast<size_t>(y) * pitch,
                       fake_screenbuf_2 + static_cast<size_t>(y) *
                           resolution::screen_width,
                       resolution::screen_width);
            }
        }
        (*bw::SDrawUnlockSurface_Import)(0, surface, 0, 0);
        if (physical_pitch)
            *physical_pitch = pitch;
        return true;
    }

    void RunStockGameOnlyPass(uint32_t camera_x, uint32_t camera_y,
                              uint32_t base_camera_x,
                              uint32_t base_camera_y,
                              uint8_t *destination,
                              bool draw_map_graphics = true,
                              bool preserve_exact_x = false)
    {
        uint8_t saved_draw[8];
        uint8_t saved_flags[8];
        for (int i = 0; i < 8; ++i)
        {
            saved_draw[i] = bw::draw_layers[i].draw;
            saved_flags[i] = bw::draw_layers[i].flags;
            const bool world_layer = i == 5 || i == 3 || i == 4;
            bw::draw_layers[i].draw = world_layer ? saved_draw[i] : 0;
        }
        bw::draw_layers[5].draw = 1;
        // GPTP's layer-5 wrapper draws ON_SCREEN and stat-res graphics after
        // the native world. Running it in every private camera pass stamps
        // screen-space team indicators into every composed tile. Use the
        // native world renderer here, then explicitly draw only GPTP ON_MAP
        // graphics for this pass so rally lines and map effects remain intact.
        auto saved_game_draw = bw::draw_layers[5].Draw;
        bw::draw_layers[5].Draw =
            reinterpret_cast<decltype(bw::draw_layers[5].Draw)>(0x004BD580);

        // MoveScreen normally clamps y for a 400-pixel native viewport. Our
        // copied strips end above the HUD mask, so temporarily allow
        // their camera origin to approach the map by the same safe extent.
        // This keeps the final expanded row valid at the bottom map edge.
        const uint32_t saved_screen_y_max = *bw::screen_y_max;
        const uint32_t map_height =
            static_cast<uint32_t>(*bw::map_height_tiles) * 32;
        const uint32_t safe_screen_y_max =
            map_height > resolution::native_safe_game_height ?
            map_height - resolution::native_safe_game_height : 0;
        *bw::screen_y_max = std::max(saved_screen_y_max, safe_screen_y_max);
        bw::MoveScreen(static_cast<int>(camera_x), static_cast<int>(camera_y));
        *bw::screen_y_max = saved_screen_y_max;
        if (preserve_exact_x)
        {
            // MoveScreen rounds x down to eight pixels. Full-width private
            // columns have no spare source pixels for compensating with a
            // crop, so retain the requested x after MoveScreen has updated
            // all camera-dependent bookkeeping. A sub-eight correction
            // cannot cross a 32-pixel tile boundary.
            *bw::screen_x = camera_x;
        }
        // ScreenUpdateProc normally prepares only the outer camera's visible
        // sprite rows. Rebuild them for every private camera pass so units,
        // bullets, overlays, and lone sprites follow the moved viewport.
        reinterpret_cast<void (__cdecl *)()>(0x004BD3A0)();
        ForceNativeRedraw();
        bw::draw_layers[5].flags |= 0x21;
        bw::draw_layers[3].flags |= 0x21;
        bw::draw_layers[4].flags |= 0x21;

        // Placement layer parameters at 0x64095c/0x640964 are Surface
        // records: width, height, and pixels. They must never be translated.
        // The position prepared for the outer camera lives in each layer's
        // left/top fields. Shift only those fields into this private camera's
        // coordinate space, then restore them immediately after drawing.
        // This makes one world-space ghost appear in the correct output tile
        // without corrupting or shrinking its bitmap dimensions.
        int16_t saved_placement_left[2];
        int16_t saved_placement_top[2];
        const int32_t placement_delta_x =
            static_cast<int32_t>(base_camera_x) -
            static_cast<int32_t>(camera_x);
        const int32_t placement_delta_y =
            static_cast<int32_t>(base_camera_y) -
            static_cast<int32_t>(camera_y);
        for (unsigned box = 0; box < 2; ++box)
        {
            DrawLayer &layer = bw::draw_layers[3 + box];
            saved_placement_left[box] = layer.area.left;
            saved_placement_top[box] = layer.area.top;
            layer.area.left = static_cast<int16_t>(
                static_cast<int32_t>(saved_placement_left[box]) +
                placement_delta_x);
            layer.area.top = static_cast<int16_t>(
                static_cast<int32_t>(saved_placement_top[box]) +
                placement_delta_y);
        }

        memset(native_inner_frame, 0, native_frame_size);
        recursive_stock_draw = true;
        reinterpret_cast<void (__cdecl *)()>(0x0041E280)();
        recursive_stock_draw = false;
        if (draw_map_graphics)
        {
            DrawGptpMapGraphics(native_inner_frame,
                static_cast<uint16_t>(resolution::native_width),
                static_cast<uint16_t>(resolution::native_height));
        }
        for (unsigned box = 0; box < 2; ++box)
        {
            DrawLayer &layer = bw::draw_layers[3 + box];
            layer.area.left = saved_placement_left[box];
            layer.area.top = saved_placement_top[box];
        }
        if (destination)
            memcpy(destination, native_inner_frame, native_frame_size);

        bw::draw_layers[5].Draw = saved_game_draw;
        for (int i = 0; i < 8; ++i)
        {
            bw::draw_layers[i].draw = saved_draw[i];
            bw::draw_layers[i].flags = saved_flags[i];
        }
    }

    void RunStockPopupPass(uint32_t camera_x, uint32_t camera_y,
                           uint32_t base_camera_x,
                           uint32_t base_camera_y,
                           uint8_t *destination,
                           bool draw_map_graphics = true)
    {
        uint8_t saved_draw[8];
        uint8_t saved_flags[8];
        for (int i = 0; i < 8; ++i)
        {
            saved_draw[i] = bw::draw_layers[i].draw;
            saved_flags[i] = bw::draw_layers[i].flags;
            // Layer 2 owns the modal dialog. Keep the world and placement
            // layers beneath it so StarCraft's STrans compositor blends the
            // dialog against the same pixels that will appear at its relocated
            // expanded-screen position. Cursor and context help are composed
            // once later and must not be duplicated by this private pass.
            const bool popup_layer =
                i == 2 || i == 3 || i == 4 || i == 5;
            bw::draw_layers[i].draw = popup_layer ? saved_draw[i] : 0;
        }
        bw::draw_layers[2].draw = 1;
        bw::draw_layers[5].draw = 1;
        auto saved_game_draw = bw::draw_layers[5].Draw;
        bw::draw_layers[5].Draw =
            reinterpret_cast<decltype(bw::draw_layers[5].Draw)>(0x004BD580);

        const uint32_t saved_screen_y_max = *bw::screen_y_max;
        const uint32_t map_height =
            static_cast<uint32_t>(*bw::map_height_tiles) * 32;
        const uint32_t safe_screen_y_max =
            map_height > resolution::native_safe_game_height ?
            map_height - resolution::native_safe_game_height : 0;
        *bw::screen_y_max = std::max(saved_screen_y_max, safe_screen_y_max);
        bw::MoveScreen(static_cast<int>(camera_x), static_cast<int>(camera_y));
        *bw::screen_y_max = saved_screen_y_max;
        reinterpret_cast<void (__cdecl *)()>(0x004BD3A0)();
        ForceNativeRedraw();
        bw::draw_layers[2].flags |= 0x21;
        bw::draw_layers[3].flags |= 0x21;
        bw::draw_layers[4].flags |= 0x21;
        bw::draw_layers[5].flags |= 0x21;

        int16_t saved_placement_left[2];
        int16_t saved_placement_top[2];
        const int32_t placement_delta_x =
            static_cast<int32_t>(base_camera_x) -
            static_cast<int32_t>(camera_x);
        const int32_t placement_delta_y =
            static_cast<int32_t>(base_camera_y) -
            static_cast<int32_t>(camera_y);
        for (unsigned box = 0; box < 2; ++box)
        {
            DrawLayer &layer = bw::draw_layers[3 + box];
            saved_placement_left[box] = layer.area.left;
            saved_placement_top[box] = layer.area.top;
            layer.area.left = static_cast<int16_t>(
                static_cast<int32_t>(saved_placement_left[box]) +
                placement_delta_x);
            layer.area.top = static_cast<int16_t>(
                static_cast<int32_t>(saved_placement_top[box]) +
                placement_delta_y);
        }

        memset(native_inner_frame, 0, native_frame_size);
        recursive_stock_draw = true;
        reinterpret_cast<void (__cdecl *)()>(0x0041E280)();
        recursive_stock_draw = false;
        if (draw_map_graphics)
        {
            DrawGptpMapGraphics(native_inner_frame,
                static_cast<uint16_t>(resolution::native_width),
                static_cast<uint16_t>(resolution::native_height));
        }
        for (unsigned box = 0; box < 2; ++box)
        {
            DrawLayer &layer = bw::draw_layers[3 + box];
            layer.area.left = saved_placement_left[box];
            layer.area.top = saved_placement_top[box];
        }
        memcpy(destination, native_inner_frame, native_frame_size);

        bw::draw_layers[5].Draw = saved_game_draw;
        for (int i = 0; i < 8; ++i)
        {
            bw::draw_layers[i].draw = saved_draw[i];
            bw::draw_layers[i].flags = saved_flags[i];
        }
    }

    size_t CountNonzero(const uint8_t *data, size_t size)
    {
        size_t count = 0;
        for (size_t i = 0; i < size; ++i)
            count += data[i] != 0;
        return count;
    }

    void LogLayerTable(const char *stage)
    {
        static bool logged_menu;
        static bool logged_game;
        bool &logged = strcmp(stage, "game") == 0 ? logged_game : logged_menu;
        if (logged)
            return;
        logged = true;
        FILE *log = fopen("fixed_zoom_renderer.log", "a");
        if (!log)
            return;
        fprintf(log, "%s layer table:\n", stage);
        for (int i = 0; i < 8; ++i)
        {
            const DrawLayer &layer = bw::draw_layers[i];
            fprintf(log,
                    "  layer[%d] draw=%u flags=%02X area=(%d,%d,%d,%d) "
                    "param=%p function=%p\n",
                    i, static_cast<unsigned>(layer.draw),
                    static_cast<unsigned>(layer.flags),
                    static_cast<int>(layer.area.left),
                    static_cast<int>(layer.area.top),
                    static_cast<int>(layer.area.right),
                    static_cast<int>(layer.area.bottom),
                    layer.func_param, layer.Draw);
        }
        fclose(log);
    }

    uint32_t fnv1a(const uint8_t *data, size_t size)
    {
        uint32_t hash = 2166136261u;
        for (size_t i = 0; i < size; ++i)
        {
            hash ^= data[i];
            hash *= 16777619u;
        }
        return hash;
    }

    void LogRendererState(const char *stage, uint32_t camera_x,
                          uint32_t camera_y, int physical_pitch)
    {
        FILE *log = fopen("fixed_zoom_renderer.log", "a");
        if (!log)
            return;
        fprintf(log,
                "%lu %s camera=(%lu,%lu) game_layer=%u "
                "game=%ux%u image=%p trans=%p redraw=%p pitch=%d "
                "mouse=(%ld,%ld) cursor_offset=(%d,%d) "
                "terrain_hash=%08X native_hash=%08X output_hash=%08X\n",
                static_cast<unsigned long>(GetTickCount()), stage,
                static_cast<unsigned long>(camera_x),
                static_cast<unsigned long>(camera_y),
                static_cast<unsigned>(bw::draw_layers[5].draw),
                static_cast<unsigned>((*bw::game_screen).w),
                static_cast<unsigned>((*bw::game_screen).h),
                (*bw::game_screen).image, *bw::trans_list,
                *bw::game_screen_redraw_trans, physical_pitch,
                static_cast<long>(*bw::mouse_clickpos_x),
                static_cast<long>(*bw::mouse_clickpos_y),
                expanded_cursor_offset_x, expanded_cursor_offset_y,
                fnv1a(native_game_reference, native_frame_size),
                fnv1a(native_present, native_frame_size),
                fnv1a(fake_screenbuf, expanded_frame_size()));
        fclose(log);
    }

    void DumpFirstExpandedFrame()
    {
        static bool dumped;
        if (dumped)
            return;
        dumped = true;

        FILE *pixels = fopen("fixed_zoom_first_frame.raw", "wb");
        if (pixels)
        {
            fwrite(fake_screenbuf, 1, expanded_frame_size(), pixels);
            fclose(pixels);
        }
        FILE *info = fopen("fixed_zoom_first_frame.txt", "w");
        if (info)
        {
            fprintf(info,
                    "format=indexed8\nwidth=%u\nheight=%u\n"
                    "game_height=%u\nnative_width=%u\nnative_height=%u\n",
                    static_cast<unsigned>(resolution::screen_width),
                    static_cast<unsigned>(resolution::screen_height),
                    static_cast<unsigned>(resolution::game_height),
                    static_cast<unsigned>(resolution::native_width),
                    static_cast<unsigned>(resolution::native_height));
            fclose(info);
        }
    }

    void SetNativeCanvas(Surface *screen, uint8_t *pixels)
    {
        screen->w = static_cast<x16u>(resolution::native_width);
        screen->h = static_cast<y16u>(resolution::native_height);
        screen->image = pixels;
        *bw::current_canvas = screen;
    }

    void DrawLayerNative(DrawLayer *layer)
    {
        if (!layer->draw || !layer->Draw)
            return;
        DrawParam param;
        param.area = Rect16(
            -layer->area.left, -layer->area.top,
            -1 - layer->area.left + resolution::native_width,
            -1 - layer->area.top + resolution::native_height);
        param.w = static_cast<x16u>(resolution::native_width);
        param.h = static_cast<y16u>(resolution::native_height);
        (*layer->Draw)(0, 0, layer->func_param, &param);
        layer->flags &= ~0x7;
    }

    void ForceNativeRedraw()
    {
        std::fill(bw::screen_redraw_tiles.begin(),
                  bw::screen_redraw_tiles.end(), 1);
        *bw::needs_full_redraw = 1;
    }

    void MergeNativeTrans(Surface *screen, uint8_t *destination)
    {
        memcpy(native_present, destination, native_frame_size);
        if (*bw::trans_list && *bw::game_screen_redraw_trans)
        {
            SetNativeCanvas(screen, destination);
            bw::STransBind(*bw::game_screen_redraw_trans);
            bw::STrans437(*bw::trans_list, &bw::screen_redraw_tiles[0], 3,
                          &*bw::game_screen_redraw_trans);
            bw::CopyGameScreenToFramebuf();
            memcpy(destination, native_present, native_frame_size);
        }
        std::fill(bw::screen_redraw_tiles.begin(),
                  bw::screen_redraw_tiles.end(), 0);
    }

    void RenderGameAt(Surface *screen, uint8_t *destination,
                      uint32_t camera_x, uint32_t camera_y)
    {
        bw::MoveScreen(static_cast<int>(camera_x), static_cast<int>(camera_y));
        *bw::screen_x = camera_x;
        memset(destination, 0, native_frame_size);
        SetNativeCanvas(screen, destination);
        ForceNativeRedraw();
        DrawLayerNative(&bw::draw_layers[5]);
        // Terrain is drawn directly, while units, bullets, selection circles,
        // and other sprites are merged through StarCraft's native STrans
        // surface.  Complete that native merge before cropping this tile.
        MergeNativeTrans(screen, destination);
    }

    unsigned TileCopyWidth(unsigned column)
    {
        const unsigned destination_x = column * resolution::tile_width;
        return std::min(static_cast<unsigned>(resolution::tile_width),
            static_cast<unsigned>(resolution::game_width) - destination_x);
    }

    unsigned TileCopyHeight(unsigned row)
    {
        const unsigned destination_y = row * resolution::tile_height;
        return std::min(static_cast<unsigned>(resolution::tile_height),
            static_cast<unsigned>(resolution::game_height) - destination_y);
    }

    unsigned TileVisualCopyHeight(unsigned row)
    {
        const unsigned destination_y = row * resolution::tile_height;
        return std::min(static_cast<unsigned>(resolution::tile_height),
            static_cast<unsigned>(resolution::screen_height) - destination_y);
    }

    bool LoadReplayTunitColors()
    {
        if (replay_tunit_load_attempted)
            return replay_tunit_loaded;
        replay_tunit_load_attempted = true;

        HMODULE storm = GetModuleHandleA("storm.dll");
        if (!storm)
            return false;
        using SBmpLoadImageFn = int (__stdcall *)(
            const char *, void *, uint8_t *, uint32_t,
            uint32_t *, uint32_t *, uint32_t *);
        auto load_image = reinterpret_cast<SBmpLoadImageFn>(GetProcAddress(
            storm, reinterpret_cast<LPCSTR>(static_cast<uintptr_t>(323))));
        if (!load_image)
            return false;
        replay_tunit_loaded = load_image(
            "game\\tunit.pcx", nullptr, replay_tunit_colors,
            sizeof(replay_tunit_colors), nullptr, nullptr, nullptr) != 0;
        return replay_tunit_loaded;
    }

    void RepairReplayPlayerColorRamps()
    {
        if (!is_in_replay() || !LoadReplayTunitColors())
            return;

        // Cosmonarchy extends the eight-pixel player-color ramp to 256 named
        // sets in game\\tunit.pcx. Replay playback can restore the color ID
        // without restoring the matching ramp for active player slots, which
        // makes team-color pixels appear as unrelated speckles. Keep the ID
        // authoritative and repair only a mismatched playable-player ramp.
        for (unsigned player = 0; player < 8; ++player)
        {
            const unsigned color = bw::player_color_ids[player];
            uint8_t *actual = &bw::player_in_game_colors[player * 8];
            const uint8_t *expected =
                replay_tunit_colors + color * 8;
            if (memcmp(actual, expected, 8) != 0)
                memcpy(actual, expected, 8);
        }
    }

    uint32_t AlignCameraDown(uint32_t value)
    {
        return value / resolution::camera_quantum *
            resolution::camera_quantum;
    }

    void CopyTerrainTile(const uint8_t *source, unsigned source_x,
                         unsigned source_y, unsigned destination_x,
                         unsigned destination_y, unsigned copy_width,
                         unsigned copy_height)
    {
        for (unsigned row = 0; row < copy_height; ++row)
        {
            memcpy(
                fake_screenbuf +
                    static_cast<size_t>(destination_y + row) *
                        resolution::screen_width + destination_x,
                source + static_cast<size_t>(source_y + row) *
                    resolution::native_width + source_x,
                copy_width);
        }
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

    void CopyTerrainTileGutters(const uint8_t *source, unsigned source_x,
                                unsigned source_y, unsigned destination_x,
                                unsigned destination_y, unsigned copy_width,
                                unsigned visual_copy_height,
                                uint32_t base_camera_y,
                                uint32_t map_height)
    {
        if (resolution::hud_left == 0 || visual_copy_height == 0)
            return;

        const unsigned tile_bottom = std::min(
            static_cast<unsigned>(resolution::screen_height),
            destination_y + visual_copy_height);
        const unsigned gutter_top = std::max(
            static_cast<unsigned>(resolution::game_height), destination_y);
        if (gutter_top >= tile_bottom)
            return;

        const uint64_t world_top =
            static_cast<uint64_t>(base_camera_y) + gutter_top;
        if (world_top >= map_height)
            return;
        const unsigned map_rows = static_cast<unsigned>(std::min<uint64_t>(
            tile_bottom - gutter_top,
            static_cast<uint64_t>(map_height) - world_top));
        const unsigned source_gutter_y =
            source_y + gutter_top - destination_y;
        if (source_gutter_y >= resolution::native_safe_game_height)
            return;
        const unsigned copy_height = std::min(map_rows,
            static_cast<unsigned>(resolution::native_safe_game_height) -
                source_gutter_y);
        if (copy_height == 0)
            return;

        const unsigned tile_right = destination_x + copy_width;
        const unsigned left_gutter_right = std::min(tile_right,
            static_cast<unsigned>(resolution::hud_left));
        if (destination_x < left_gutter_right)
        {
            CopyTerrainTile(source, source_x, source_gutter_y,
                destination_x, gutter_top,
                left_gutter_right - destination_x, copy_height);
        }

        const unsigned right_gutter_left = std::max(destination_x,
            static_cast<unsigned>(resolution::hud_left +
                resolution::native_width));
        if (right_gutter_left < tile_right)
        {
            CopyTerrainTile(source,
                source_x + right_gutter_left - destination_x,
                source_gutter_y, right_gutter_left, gutter_top,
                tile_right - right_gutter_left, copy_height);
        }
    }

    void RenderExpandedTerrain(Surface *screen, uint32_t base_x,
                               uint32_t base_y, uint32_t map_width,
                               uint32_t map_height)
    {
        memset(fake_screenbuf, 0, expanded_frame_size());
        const uint32_t native_max_x = map_width > resolution::native_width ?
            map_width - resolution::native_width : 0;
        const uint32_t native_max_y =
            map_height > resolution::native_safe_game_height ?
            map_height - resolution::native_safe_game_height : 0;

        for (unsigned row = 0; row < resolution::tile_rows; ++row)
        {
            const unsigned copy_height = TileCopyHeight(row);
            const unsigned visual_copy_height = TileVisualCopyHeight(row);
            for (unsigned column = 0; column < resolution::tile_columns;
                 ++column)
            {
                const unsigned copy_width = TileCopyWidth(column);
                const uint32_t desired_x =
                    base_x + column * resolution::tile_width;
                const uint32_t desired_y =
                    base_y + row * resolution::tile_height;
                // Preserve exact x inside RenderGameAt. Model MoveScreen's
                // remaining y adjustment before deriving the source crop so
                // adjacent passes cannot repeat a vertical band.
                const uint32_t actual_x =
                    std::min(desired_x, native_max_x);
                const uint32_t actual_y = AlignCameraDown(
                    std::min(desired_y, native_max_y));
                const unsigned source_x = std::min(
                    static_cast<unsigned>(desired_x - actual_x),
                    static_cast<unsigned>(resolution::native_width -
                        copy_width));
                const unsigned source_y = std::min(
                    static_cast<unsigned>(desired_y - actual_y),
                    static_cast<unsigned>(resolution::native_safe_game_height -
                        copy_height));
                uint8_t *target = (row == 0 && column == 0) ?
                    native_game_reference : native_tile;
                RenderGameAt(screen, target, actual_x, actual_y);
                CopyTerrainTile(
                    target, source_x, source_y,
                    column * resolution::tile_width,
                    row * resolution::tile_height,
                    copy_width, copy_height);
                CopyTerrainTileGutters(
                    target, source_x, source_y,
                    column * resolution::tile_width,
                    row * resolution::tile_height,
                    copy_width, visual_copy_height, base_y, map_height);
            }
        }
    }

    void RenderAllNativeLayers(Surface *screen, uint32_t camera_x,
                               uint32_t camera_y, bool move_camera = true)
    {
        if (move_camera)
            bw::MoveScreen(static_cast<int>(camera_x), static_cast<int>(camera_y));
        memset(native_work, 0, native_frame_size);
        memset(native_present, 0, native_frame_size);
        SetNativeCanvas(screen, native_work);
        ForceNativeRedraw();
        for (int i = 7; i >= 0; --i)
            DrawLayerNative(&bw::draw_layers[i]);

        MergeNativeTrans(screen, native_work);
        memcpy(native_present, native_work, native_frame_size);
    }

    void CompositeNativeUi()
    {
        // The upper UI, cursors, selection boxes, and dialogs are the pixels
        // that differ from a game-only render at the same camera position.
        for (unsigned y = 0; y < resolution::native_game_height; ++y)
        {
            for (unsigned x = 0; x < resolution::native_width; ++x)
            {
                const size_t native_index =
                    static_cast<size_t>(y) * resolution::native_width + x;
                if (native_present[native_index] !=
                    native_game_reference[native_index])
                {
                    fake_screenbuf[
                        static_cast<size_t>(y) * resolution::screen_width + x] =
                        native_present[native_index];
                }
            }
        }

        // Preserve the stock HUD byte-for-byte, but move it beneath the
        // expanded battlefield instead of leaving it at native y=400.
        for (unsigned y = resolution::native_game_height;
             y < resolution::native_height; ++y)
        {
            memcpy(
                fake_screenbuf +
                    static_cast<size_t>(resolution::game_height +
                        y - resolution::native_game_height) *
                        resolution::screen_width,
                native_present + static_cast<size_t>(y) *
                    resolution::native_width,
                resolution::native_width);
        }
    }

    void ScaleNativeMenuToOutput(const uint8_t *source)
    {
        const unsigned output_width =
            static_cast<unsigned>(resolution::menu_width);
        const unsigned output_height =
            static_cast<unsigned>(resolution::menu_height);
        const unsigned output_left =
            static_cast<unsigned>(resolution::menu_left);
        const unsigned output_top =
            static_cast<unsigned>(resolution::menu_top);

        const bool geometry_unchanged =
            cached_menu_screen_width == resolution::screen_width &&
            cached_menu_screen_height == resolution::screen_height &&
            cached_menu_width == output_width &&
            cached_menu_height == output_height &&
            cached_menu_left == output_left && cached_menu_top == output_top;
        if (cached_menu_frame_valid && geometry_unchanged &&
            memcmp(source, cached_menu_source, native_frame_size) == 0)
        {
            return;
        }

        memcpy(cached_menu_source, source, native_frame_size);
        cached_menu_screen_width = resolution::screen_width;
        cached_menu_screen_height = resolution::screen_height;
        cached_menu_width = output_width;
        cached_menu_height = output_height;
        cached_menu_left = output_left;
        cached_menu_top = output_top;

        memset(fake_screenbuf, 0, expanded_frame_size());

        static uint16_t source_x_by_destination[
            resolution::maximum_screen_width];
        static unsigned mapped_output_width;
        if (mapped_output_width != output_width)
        {
            for (unsigned destination_x = 0;
                 destination_x < output_width; ++destination_x)
            {
                source_x_by_destination[destination_x] =
                    static_cast<uint16_t>(destination_x *
                        static_cast<unsigned>(resolution::native_width) /
                        output_width);
            }
            mapped_output_width = output_width;
        }

        unsigned previous_source_y =
            static_cast<unsigned>(resolution::native_height);
        uint8_t *previous_destination_row = nullptr;
        for (unsigned destination_y = 0;
             destination_y < output_height; ++destination_y)
        {
            const unsigned source_y = destination_y *
                static_cast<unsigned>(resolution::native_height) /
                output_height;
            uint8_t *destination_row = fake_screenbuf +
                static_cast<size_t>(output_top + destination_y) *
                    resolution::screen_width + output_left;
            if (source_y == previous_source_y)
            {
                memcpy(destination_row, previous_destination_row,
                       output_width);
                previous_destination_row = destination_row;
                continue;
            }

            const uint8_t *source_row = source +
                static_cast<size_t>(source_y) * resolution::native_width;
            for (unsigned destination_x = 0;
                 destination_x < output_width; ++destination_x)
            {
                destination_row[destination_x] =
                    source_row[source_x_by_destination[destination_x]];
            }
            previous_source_y = source_y;
            previous_destination_row = destination_row;
        }
        cached_menu_frame_valid = true;
    }

    void RenderNativeMenu(Surface *screen)
    {
        RenderAllNativeLayers(screen, 0, 0, false);
        ScaleNativeMenuToOutput(native_present);
    }
}

void BeginStockDrawScreen()
{
    // GPTP may be loaded after this plugin's InitialPatch.  Resolve and patch
    // its guarded building-placement bounds at the first available frame.
    EnsureGptpPlacementBounds();
    EnsureGptpCursorHoverBounds();
    EnsureGptpSelectionBounds();
    EnsureGptpCursorWarpGuard();
    EnsureGptpMinimapViewportBox();
    EnsureGptpUpgradeResearchClear();
    EnsureGptpInitialCameraCenter();
    EnsureGptpControlGroupCameraCenter();
    EnsurePresentationCursorGuards();
    RepairReplayPlayerColorRamps();
    ++stock_draw_depth;
    if (stock_draw_depth == 1 && !recursive_stock_draw && is_in_game())
    {
        // Draw the cursor once, after the expanded composition. Leaving it in
        // the native base pass clips it at x=639 and makes its dirty/background
        // restoration operate on the wrong pitch.
        saved_cursor_layer_draw = bw::draw_layers[0].draw;
        saved_selection_layer_draw = bw::draw_layers[1].draw;
        bw::draw_layers[0].draw = 0;
        bw::draw_layers[1].draw = 0;
        cursor_layer_suppressed = true;
        selection_layer_suppressed = true;
        // The private stock surface persists across frames. Force the world
        // and dialog base to repaint while layer 1 is disabled so a previous
        // tooltip cannot remain behind the one final-frame context draw.
        ForceNativeRedraw();
    }
}

void AfterStockDrawScreen()
{
    if (recursive_stock_draw || stock_draw_depth > 1)
    {
        if (stock_draw_depth != 0)
            --stock_draw_depth;
        return;
    }

    if (cursor_layer_suppressed)
    {
        bw::draw_layers[0].draw = saved_cursor_layer_draw;
        cursor_layer_suppressed = false;
    }
    if (selection_layer_suppressed)
    {
        bw::draw_layers[1].draw = saved_selection_layer_draw;
        selection_layer_suppressed = false;
    }

    static bool trace_first_call = true;
    if (trace_first_call)
    {
        TracePostRenderer("enter");
    }

    int physical_pitch = 0;
    if (trace_first_call)
        TracePostRenderer("using private stock surface");

    const bool in_game = is_in_game() && bw::draw_layers[5].draw &&
        bw::draw_layers[5].Draw;
    uint32_t camera_x = *bw::screen_x;
    uint32_t camera_y = *bw::screen_y;
    const uint32_t stock_camera_x = camera_x;
    const uint32_t stock_camera_y = camera_y;

    if (in_game)
    {
        cached_menu_frame_valid = false;
        memset(fake_screenbuf, 0, expanded_frame_size());
        LogLayerTable("game");
        const uint32_t map_width =
            static_cast<uint32_t>(*bw::map_width_tiles) * 32;
        const uint32_t map_height =
            static_cast<uint32_t>(*bw::map_height_tiles) * 32;
        const uint32_t expanded_max_x = map_width > resolution::game_width ?
            map_width - resolution::game_width : 0;
        const uint32_t expanded_max_y = map_height > resolution::game_height ?
            map_height - resolution::game_height : 0;
        camera_x = std::min(camera_x, expanded_max_x);
        camera_y = std::min(camera_y, expanded_max_y);
        const uint32_t native_max_x = map_width > resolution::native_width ?
            map_width - resolution::native_width : 0;
        const uint32_t native_max_y =
            map_height > resolution::native_safe_game_height ?
            map_height - resolution::native_safe_game_height : 0;
        const unsigned horizontal_overlap =
            resolution::native_width - resolution::tile_width;
        const unsigned vertical_overlap =
            resolution::native_safe_game_height - resolution::tile_height;
        for (unsigned row = 0; row < resolution::tile_rows; ++row)
        {
            const unsigned copy_height = TileCopyHeight(row);
            const unsigned visual_copy_height = TileVisualCopyHeight(row);
            for (unsigned column = 0; column < resolution::tile_columns;
                 ++column)
            {
                const unsigned copy_width = TileCopyWidth(column);
                const uint32_t desired_x =
                    camera_x + column * resolution::tile_width;
                const uint32_t desired_y =
                    camera_y + row * resolution::tile_height;
                // Later tiles are rendered with the unused part of the native
                // canvas as a leading overlap. This keeps sprites that cross a
                // crop boundary alive on both sides of the seam.
                const uint32_t overlap_x = column ? horizontal_overlap / 2 : 0;
                const uint32_t overlap_y = row ? vertical_overlap / 2 : 0;
                const uint32_t render_x = desired_x > overlap_x ?
                    desired_x - overlap_x : 0;
                const uint32_t render_y = desired_y > overlap_y ?
                    desired_y - overlap_y : 0;
                // Private passes preserve exact x after MoveScreen. Keep y at
                // its effective eight-pixel position and derive both overlap
                // crops from the camera actually used by each axis.
                const uint32_t actual_x =
                    std::min(render_x, native_max_x);
                const uint32_t actual_y = AlignCameraDown(
                    std::min(render_y, native_max_y));
                const unsigned source_x = std::min(
                    static_cast<unsigned>(desired_x - actual_x),
                    static_cast<unsigned>(resolution::native_width -
                        copy_width));
                const unsigned source_y = std::min(
                    static_cast<unsigned>(desired_y - actual_y),
                    static_cast<unsigned>(resolution::native_safe_game_height -
                        copy_height));
                uint8_t *target = row == 0 && column == 0 ?
                    native_game_reference : nullptr;
                RunStockGameOnlyPass(actual_x, actual_y,
                                     camera_x, camera_y, target, true, true);
                const uint8_t *rendered_frame = target ? target :
                    native_inner_frame;
                CopyTerrainTile(rendered_frame, source_x, source_y,
                                column * resolution::tile_width,
                                row * resolution::tile_height,
                                copy_width, copy_height);
                CopyTerrainTileGutters(
                    rendered_frame, source_x, source_y,
                    column * resolution::tile_width,
                    row * resolution::tile_height,
                    copy_width, visual_copy_height, camera_y, map_height);
            }
        }
        // StarCraft only partially refreshes its stock backing surface while
        // middle-mouse panning. Comparing that stale surface with the fully
        // redrawn first world tile can misclassify old map pixels as UI and
        // stamp ghost map bands into the centered top and bottom UI regions.
        // Generate a matched game-only and UI pair at the same camera for the
        // duration of the gesture. Map graphics are omitted from both private
        // comparison frames so rally lines cannot be mistaken for HUD pixels.
        // They remain enabled in the expanded gameplay tiles rendered above.
        // Normal rendering keeps the existing path and pays no additional
        // pass cost.
        const bool middle_button_down =
            (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
        const bool engine_middle_pan_active = IsExpandedMiddlePanActive();
        // Windows can report the physical release before StarCraft has
        // dispatched its middle-up event and cleared the installed camera
        // callback. Keep the protected comparison path until both owners
        // agree that the gesture has ended.
        const bool middle_pan_active =
            middle_button_down || engine_middle_pan_active;
        RepairMovingFullWidthPassEdges(camera_x, camera_y,
                                       middle_pan_active);
        // MoveScreen rounds private render passes down to the engine's
        // eight-pixel camera quantum. The outer stock frame retains the exact
        // sub-quantum camera used by Cosmonarchy's smooth middle panner. A
        // comparison between those two camera positions mistakes shifted
        // terrain for UI and produces translucent duplicate regions. Build
        // both comparison frames from the same camera whenever either axis has
        // a sub-quantum remainder.
        const bool camera_is_subquantum =
            camera_x % resolution::camera_quantum != 0 ||
            camera_y % resolution::camera_quantum != 0;
        const bool outer_camera_was_clamped =
            stock_camera_x != camera_x || stock_camera_y != camera_y;
        const bool use_matched_ui_pair = middle_pan_active ||
            camera_is_subquantum || outer_camera_was_clamped;
        const uint8_t *native_ui_frame = native_stock_frame;
        if (use_matched_ui_pair)
        {
            RunStockGameOnlyPass(camera_x, camera_y, camera_x, camera_y,
                                 native_game_reference, false);
            RunStockPopupPass(camera_x, camera_y, camera_x, camera_y,
                              native_current_ui_frame, false);
            native_ui_frame = native_current_ui_frame;
        }

        const bool popup_active = *bw::popup_dialog_active != 0;
        const PopupBounds popup = GetPopupBounds(popup_active);
        static bool last_popup_active;
        static void *last_popup_dialog;
        if (popup_active != last_popup_active ||
            *bw::popup_dialog != last_popup_dialog)
        {
            FILE *log = fopen("fixed_zoom_renderer.log", "a");
            if (log)
            {
                fprintf(log,
                    "popup transition: active=%u dialog=%p "
                    "bounds=(%d,%d,%d,%d) valid=%u hud_top=%u\n",
                    static_cast<unsigned>(popup_active),
                    static_cast<void *>(*bw::popup_dialog),
                    popup.left, popup.top, popup.right, popup.bottom,
                    static_cast<unsigned>(popup.valid),
                    static_cast<unsigned>(resolution::native_hud_top));
                fclose(log);
            }
            last_popup_active = popup_active;
            last_popup_dialog = *bw::popup_dialog;
        }

        const unsigned hud_vertical_offset =
            resolution::screen_height - resolution::native_height;
        const uint8_t console_race = GetActiveConsoleRace();
        static int last_logged_console_race = -1;
        if (last_logged_console_race != console_race)
        {
            FILE *log = fopen("fixed_zoom_renderer.log", "a");
            if (log)
            {
                fprintf(log,
                    "HUD protrusion race=%u corner-piece=%s "
                    "source-band=(0,293,22,313) zerg-cap=(0,290,5,292)\n",
                    static_cast<unsigned>(console_race),
                    console_race == Race::Zerg ||
                        console_race == Race::Protoss ? "kept" : "discarded");
                fclose(log);
            }
            last_logged_console_race = console_race;
        }

        // Compose the non-modal UI identically on every frame. Previously the
        // popup branch centered the entire native canvas, shifting the HUD up
        // by native_ui_top (30 pixels) and making UI fragments jump whenever
        // the game menu opened.
        if (!outer_camera_was_clamped ||
            native_ui_frame == native_current_ui_frame)
        {
            for (unsigned source_y = 0;
                 source_y < resolution::native_hud_top; ++source_y)
            {
                const uint8_t *native_ui_row = native_ui_frame +
                    static_cast<size_t>(source_y) *
                        resolution::native_width;
                const uint8_t *native_game_row = native_game_reference +
                    static_cast<size_t>(source_y) *
                        resolution::native_width;
                if (source_y < resolution::native_hud_protrusion_top &&
                    memcmp(native_ui_row, native_game_row,
                           resolution::native_width) == 0)
                {
                    continue;
                }
                for (unsigned source_x = 0;
                     source_x < resolution::native_width; ++source_x)
                {
                    if (popup.Contains(source_x, source_y))
                        continue;
                    const size_t native_index =
                        static_cast<size_t>(source_y) *
                            resolution::native_width + source_x;
                    const bool differs_from_world =
                        native_ui_frame[native_index] !=
                        native_game_reference[native_index];

                    if (IsRelocatedNativeHudProtrusion(
                            source_x, source_y, console_race))
                    {
                        // Protoss and Zerg console art extends above y=314.
                        // Move that exact source band with the bottom-centered
                        // HUD so its lower edge joins the minimap/status row.
                        // The isolated Terran rusty-pipe component remains an
                        // intentional exception and is discarded.
                        if (IsDiscardedNativeHudOrnament(
                            source_x, source_y, console_race))
                            continue;
                        if (differs_from_world ||
                            NativeUiPixelIsOpaque(source_x, source_y))
                        {
                            fake_screenbuf[
                                static_cast<size_t>(hud_vertical_offset +
                                    source_y) * resolution::screen_width +
                                resolution::hud_left + source_x] =
                                    native_ui_frame[native_index];
                        }
                        continue;
                    }

                    if (differs_from_world)
                    {
                        // Layer 2 owns top-screen resource counters rather
                        // than DrawExpandedGameText. Move those native UI
                        // pixels into the same horizontally centered 640-wide
                        // box as objectives instead of leaving them at the
                        // obsolete x=0 native-frame origin.
                        fake_screenbuf[static_cast<size_t>(source_y) *
                            resolution::screen_width +
                            resolution::top_ui_right_native_origin +
                            source_x] =
                            native_ui_frame[native_index];
                    }
                }
            }
        }

        // Keep the complete native console at the same bottom-centered
        // position regardless of whether a modal dialog is visible.
        for (unsigned source_y = resolution::native_hud_top;
             source_y < resolution::native_height; ++source_y)
        {
            const unsigned destination_y =
                hud_vertical_offset + source_y;
            const bool popup_intersects_row = popup.valid &&
                static_cast<int>(source_y) >= popup.top &&
                static_cast<int>(source_y) < popup.bottom &&
                popup.left < static_cast<int>(resolution::native_width) &&
                popup.right > 0;
            if (source_y >= resolution::native_game_height &&
                !popup_intersects_row)
            {
                memcpy(fake_screenbuf +
                           static_cast<size_t>(destination_y) *
                               resolution::screen_width +
                           resolution::hud_left,
                       native_ui_frame +
                           static_cast<size_t>(source_y) *
                               resolution::native_width,
                       resolution::native_width);
                continue;
            }
            for (unsigned source_x = 0;
                 source_x < resolution::native_width; ++source_x)
            {
                if (popup.Contains(source_x, source_y))
                    continue;
                const size_t native_index =
                    static_cast<size_t>(source_y) *
                        resolution::native_width + source_x;
                const bool differs_from_world =
                    native_ui_frame[native_index] !=
                        native_game_reference[native_index];
                if (source_y >= resolution::native_game_height ||
                    differs_from_world ||
                    NativeUiPixelIsOpaque(source_x, source_y))
                {
                    fake_screenbuf[
                        static_cast<size_t>(destination_y) *
                            resolution::screen_width +
                        resolution::hud_left + source_x] =
                        native_ui_frame[native_index];
                }
            }
        }

        if (popup.valid)
        {
            // The stock frame has already blended the translucent popup with
            // the terrain at its original 4:3 coordinates. Re-render it at a
            // camera shifted by the exact relocation offset so the native
            // STrans compositor samples the terrain and sprites that actually
            // sit beneath the centered popup. A matching game-only pass lets
            // us transfer only pixels introduced by the dialog.
            const uint32_t popup_camera_x = camera_x +
                resolution::native_ui_left;
            const uint32_t popup_camera_y = camera_y +
                resolution::native_ui_top;
            RunStockGameOnlyPass(popup_camera_x, popup_camera_y,
                                 camera_x, camera_y,
                                 native_popup_reference);
            RunStockPopupPass(popup_camera_x, popup_camera_y,
                              camera_x, camera_y,
                              native_popup_frame);

            static bool logged_popup_transparency_pass;
            if (!logged_popup_transparency_pass)
            {
                FILE *log = fopen("fixed_zoom_renderer.log", "a");
                if (log)
                {
                    fprintf(log,
                        "popup transparency pass: camera=(%lu,%lu) "
                        "offset=(%u,%u) bounds=(%d,%d,%d,%d)\n",
                        static_cast<unsigned long>(popup_camera_x),
                        static_cast<unsigned long>(popup_camera_y),
                        static_cast<unsigned>(resolution::native_ui_left),
                        static_cast<unsigned>(resolution::native_ui_top),
                        popup.left, popup.top, popup.right, popup.bottom);
                    fclose(log);
                }
                logged_popup_transparency_pass = true;
            }

            for (int source_y = popup.top; source_y < popup.bottom; ++source_y)
            {
                const unsigned destination_y = resolution::native_ui_top +
                    static_cast<unsigned>(source_y);
                for (int source_x = popup.left;
                     source_x < popup.right; ++source_x)
                {
                    const size_t native_index =
                        static_cast<size_t>(source_y) *
                            resolution::native_width +
                        static_cast<unsigned>(source_x);
                    if (native_popup_frame[native_index] !=
                        native_popup_reference[native_index])
                    {
                        fake_screenbuf[
                            static_cast<size_t>(destination_y) *
                                resolution::screen_width +
                            resolution::native_ui_left +
                            static_cast<unsigned>(source_x)] =
                            native_popup_frame[native_index];
                    }
                }
            }
        }
        bw::MoveScreen(static_cast<int>(camera_x), static_cast<int>(camera_y));
        // Cosmonarchy's middle-button panner can retain sub-quantum camera
        // positions for smooth motion. MoveScreen is still required to
        // restore the engine's camera-dependent redraw state after our
        // private passes, but it rounds both axes down to eight pixels.
        // Restore the exact outer position after that bookkeeping so the
        // compositor does not permanently quantize the live camera every
        // frame. The tile globals written by MoveScreen remain correct because
        // an eight-pixel round-down cannot cross a 32-pixel tile boundary.
        *bw::screen_x = camera_x;
        *bw::screen_y = camera_y;
        // Private passes rebuild the visible-sprite row index for their own
        // cameras. Restore it for the expanded camera before input processing;
        // otherwise unit hit-testing depends on whichever tile rendered last.
        reinterpret_cast<void (__cdecl *)()>(0x004BD3A0)();
        DumpFirstExpandedFrame();
    }
    else
    {
        previous_world_frame_valid = false;
        if (trace_first_call)
            TracePostRenderer("composing menu");
        LogLayerTable("menu");
        ScaleNativeMenuToOutput(native_stock_frame);
    }

    memcpy(fake_screenbuf_2, fake_screenbuf, expanded_frame_size());
    if (trace_first_call)
        TracePostRenderer("running draw hooks");
    for (drawhook &hook : draw_hooks)
        (*hook.func)(fake_screenbuf_2, resolution::screen_width,
                     resolution::screen_height);
    if (in_game)
    {
        DrawExpandedGameText(fake_screenbuf_2);
        DrawExpandedContextHelp(fake_screenbuf_2);
        DrawExpandedSelectionBox(fake_screenbuf_2);
        DrawExpandedCursor(fake_screenbuf_2);
    }
    if (trace_first_call)
        TracePostRenderer("presenting expanded frame");
    PresentExpandedFrame(&physical_pitch);
    if (trace_first_call)
        TracePostRenderer("present complete");

    static uint32_t next_log_tick;
    const uint32_t now = GetTickCount();
    if (next_log_tick == 0 || static_cast<int32_t>(now - next_log_tick) >= 0)
    {
        LogRendererState(in_game ? "stock-compose-game" :
                         "stock-compose-menu", camera_x, camera_y,
                         physical_pitch);
        FILE *log = fopen("fixed_zoom_renderer.log", "a");
        if (log)
        {
            fprintf(log,
                    "  stock-compose nonzero: base=%lu game=%lu hud=%lu\n",
                    static_cast<unsigned long>(CountNonzero(
                        native_stock_frame, native_frame_size)),
                    static_cast<unsigned long>(CountNonzero(
                        fake_screenbuf,
                        resolution::screen_width * resolution::game_height)),
                    static_cast<unsigned long>(CountNonzero(
                        fake_screenbuf + resolution::screen_width *
                            resolution::game_height,
                        resolution::screen_width *
                            (resolution::screen_height -
                             resolution::game_height))));
            fclose(log);
        }
        next_log_tick = now + 5000;
    }
    trace_first_call = false;
    if (stock_draw_depth != 0)
        --stock_draw_depth;
}

void SetExpandedCursorOffset(int x, int y)
{
    expanded_cursor_offset_x = x;
    expanded_cursor_offset_y = y;
}

void DrawGameTextConditional()
{
    // AfterStockDrawScreen draws this once onto the completed expanded frame.
}

void DrawSelectionBoxConditional()
{
    // AfterStockDrawScreen draws this once onto the completed expanded frame.
}

int IsOutsideExpandedGameScreen(int x, int y)
{
    if (!is_in_game())
    {
        // Preserve the stock menu/dialog STrans mask without recursively
        // entering the function replaced by this hook.
        if (y < *bw::trans_mouse_y_min)
            return 0;
        if (y >= *bw::trans_mouse_y_max)
            return 1;
        return bw::STransGetPixel(*bw::trans_list, x, y) == 0;
    }
    return x < 0 || y < 0 ||
        x >= static_cast<int>(resolution::game_width) ||
        y >= static_cast<int>(resolution::screen_height);
}


void DrawScreen()
{
    if (fastForwardEndFrames > 0) {
        bool draw = false;

        // Start ff
        if (fastForwardEndFrames > 0 && !isFastForwarding)
        {
            fastForwardProgressCount = 0;
            isFastForwarding = true;
            bw::game_speed_waits[*bw::game_speed] = 0;
            draw = true;
        }
        // Draw game every 5% while in fast forward mode
        int duration = fastForwardEndFrames - fastForwardStartFrames;
        int part = *bw::frame_count - fastForwardStartFrames;
        int progress = static_cast<int>(
            std::roundf((static_cast<float>(part) /
                         static_cast<float>(duration)) * 100));
        if (progress % 5 == 0 && fastForwardProgressCount < progress) {
            fastForwardProgressCount = progress;
            draw = true;
        }

        // End of ff
        if (isFastForwarding && *bw::frame_count > fastForwardEndFrames)
        {
            isFastForwarding = false;
            draw = true;
        }

        if (!draw)
            return;
    }

    const auto relaxed = std::memory_order_relaxed;
    // Overflowing is fine
    draw_counter.store(draw_counter.load(relaxed) + 1, relaxed);
    if (frame_skip_ms != 0)
    {
        uint32_t tick = GetTickCount();
        if (tick < last_frame_skip_tick + frame_skip_ms) {
            return;
        }
        last_frame_skip_tick = tick;
    }

    // The load screen code likes to draw screen after every grp loaded,
    // and this function is a lot slower than the original one
    if (*bw::load_screen != nullptr)
    {
        static bool drawn_load_screen_once;
        if (drawn_load_screen_once)
            return;
        drawn_load_screen_once = true;
    }
    Surface *game_screen = &*bw::game_screen;
    if (!game_screen->image)
        return;
    const Surface original_screen = *game_screen;
    const bool in_game = is_in_game() &&
        bw::draw_layers[5].draw && bw::draw_layers[5].Draw;
    uint32_t camera_x = *bw::screen_x;
    uint32_t camera_y = *bw::screen_y;

    if (*bw::no_draw)
    {
        memset(fake_screenbuf, 0, expanded_frame_size());
    }
    else if (in_game)
    {
        LogLayerTable("game");
        const uint32_t map_width =
            static_cast<uint32_t>(*bw::map_width_tiles) * 32;
        const uint32_t map_height =
            static_cast<uint32_t>(*bw::map_height_tiles) * 32;
        const uint32_t expanded_max_x = map_width > resolution::game_width ?
            map_width - resolution::game_width : 0;
        const uint32_t expanded_max_y = map_height > resolution::game_height ?
            map_height - resolution::game_height : 0;
        camera_x = std::min(camera_x, expanded_max_x);
        camera_y = std::min(camera_y, expanded_max_y);

        RenderExpandedTerrain(game_screen, camera_x, camera_y,
                              map_width, map_height);
        RenderAllNativeLayers(game_screen, camera_x, camera_y);
        CompositeNativeUi();
        DumpFirstExpandedFrame();
    }
    else
    {
        LogLayerTable("menu");
        RenderNativeMenu(game_screen);
    }

    *game_screen = original_screen;
    *bw::current_canvas = nullptr;

    memcpy(fake_screenbuf_2, fake_screenbuf, expanded_frame_size());
    for (drawhook &hook : draw_hooks)
    {
        (*hook.func)(fake_screenbuf_2, resolution::screen_width, resolution::screen_height);
    }
    uint8_t *surface = nullptr;
    int width = 0;
    if ((*bw::SDrawLockSurface_Import)(0, 0, &surface, &width, 0))
    {
        if (surface && width >= static_cast<int>(resolution::screen_width))
        {
            for (unsigned int i = 0; i < resolution::screen_height; i++)
                memcpy(surface + i * width,
                       fake_screenbuf_2 + i * resolution::screen_width,
                       resolution::screen_width);
        }
        (*bw::SDrawUnlockSurface_Import)(0, surface, 0, 0);
    }

    static uint32_t next_log_tick;
    const uint32_t now = GetTickCount();
    if (next_log_tick == 0 || static_cast<int32_t>(now - next_log_tick) >= 0)
    {
        LogRendererState(in_game ? "game" : "menu", camera_x, camera_y,
                         width);
        FILE *log = fopen("fixed_zoom_renderer.log", "a");
        if (log)
        {
            fprintf(log,
                    "  nonzero: terrain=%lu native_game=%lu native_hud=%lu "
                    "output_game=%lu output_hud=%lu\n",
                    static_cast<unsigned long>(CountNonzero(
                        native_game_reference, native_frame_size)),
                    static_cast<unsigned long>(CountNonzero(
                        native_present,
                        resolution::native_width *
                            resolution::native_game_height)),
                    static_cast<unsigned long>(CountNonzero(
                        native_present + resolution::native_width *
                            resolution::native_game_height,
                        resolution::native_width *
                            (resolution::native_height -
                             resolution::native_game_height))),
                    static_cast<unsigned long>(CountNonzero(
                        fake_screenbuf,
                        resolution::screen_width * resolution::game_height)),
                    static_cast<unsigned long>(CountNonzero(
                        fake_screenbuf + resolution::screen_width *
                            resolution::game_height,
                        resolution::screen_width *
                            (resolution::screen_height -
                             resolution::game_height))));
            fclose(log);
        }
        next_log_tick = now + 5000;
    }
}

int SDrawLockSurface_Hook(int surface_id, Rect32 *a2, uint8_t **surface, int *width, int unused)
{
    if (surface_id == 0 && stock_draw_depth != 0)
    {
        *surface = recursive_stock_draw ? native_inner_frame : native_stock_frame;
        *width = resolution::native_width;
        return 1;
    }
    else
    {
        return (*bw::SDrawLockSurface_Import)(surface_id, a2, surface, width, unused);
    }
}

int SDrawUnlockSurface_Hook(int surface_id, uint8_t *surface, int a3, int a4)
{
    if (surface == native_inner_frame || surface == native_stock_frame)
        return 1;

    return (*bw::SDrawUnlockSurface_Import)(surface_id, surface, a3, a4);
}

void GenerateFog()
{
    int screen_x = *bw::screen_pos_x_tiles;
    if (screen_x != 0)
        screen_x--;
    int screen_y = *bw::screen_pos_y_tiles;
    if (screen_y != 0)
        screen_y--;
    uint32_t *flags = (*bw::map_tile_flags) + screen_y * *bw::map_width_tiles + screen_x;
    uint32_t *orig_flags = flags;
    uint8_t *pos = *bw::fog_arr1;
    int shown_value = *bw::fog_variance_amount;
    int fow_value = shown_value / 2;

    int y_pos = screen_y;
    for (int i = 0; i < 0x11; i++)
    {
        int x_pos = *bw::screen_pos_x_tiles - 1;
        flags = orig_flags;
        for (int i = 0; i < 0x18; i++)
        {
            // Obviously people can just remove multiplayer check if they wish
            // Bw had nice vision-based sync but it does not work with dynamically allocated sprites
            if (all_visions && !is_multiplayer())
            {
                if ((0xff00 & flags[0]) == 0xff00)
                    *pos = 0;
                else if ((0xff & flags[0]) == 0xff)
                    *pos = fow_value;
                else
                    *pos = shown_value;
            }
            else if (is_in_replay())
            {
                if (*bw::replay_show_whole_map)
                    *pos = shown_value;
                else if (!((*bw::replay_visions << 8) & ~flags[0]))
                    *pos = 0;
                else if (!(*bw::replay_visions & ~flags[0]))
                    *pos = fow_value;
                else
                    *pos = shown_value;
            }
            else
            {
                if (*bw::player_exploration_visions & flags[0])
                    *pos = 0;
                else if (*bw::player_visions & flags[0])
                    *pos = fow_value;
                else
                    *pos = shown_value;
            }
            if (x_pos < *bw::map_width_tiles - 1 && x_pos >= 0)
                flags++;
            x_pos++;
            pos++;
        }
        if (y_pos < *bw::map_height_tiles - 1 && y_pos >= 0)
            orig_flags += *bw::map_width_tiles;
        y_pos++;
    }

    // Blend fog
    // Screen is 0x18 x 0x11 tiles
    // Well every border has 1 nonvisible tile, it is only used for blending?
    pos = *bw::fog_arr1 + 0x18 + 0x1;
    uint8_t *out = *bw::fog_arr2 + 0x18 + 0x1;
    for (int i = 0; i < 0x11 - 2; i++)
    {
        for (int i = 0; i < 0x18 - 2; i++)
        {
            int val = pos[0] * 2;
            val = (val + pos[-1] + pos[1] + pos[-0x18] + pos[0x18]) * 2;
            val = (val + pos[-0x17] + pos[0x17] + pos[-0x19] + pos[0x19]) / 16;
            *out = val;
            pos++;
            out++;
        }
        pos += 2;
        out += 2;
    }
}

void AddDrawHook(void (*func)(uint8_t *, xuint, yuint), int priority)
{
    drawhook hook(func, priority);
    auto it = lower_bound(draw_hooks.begin(), draw_hooks.end(), hook);
    draw_hooks.insert(it, hook);
}
