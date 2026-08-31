#ifndef COSMONARCHY_ZOOM_RESOLUTION_H
#define COSMONARCHY_ZOOM_RESOLUTION_H

// Shared defaults and native constants for the expanded compositor. The
// universal aidebug payload replaces screen_width/screen_height at startup
// from cosmonarchy_viewport.ini before installing resolution-derived patches.
namespace zoom_resolution_config
{
    enum class TopUiLayout
    {
        centered_native_box,
        screen_edges,
    };

    constexpr int native_width = 640;
    constexpr int native_height = 480;
    constexpr int native_game_height = 400;

#ifndef COSMONARCHY_SCREEN_WIDTH
#define COSMONARCHY_SCREEN_WIDTH 1280
#endif
#ifndef COSMONARCHY_SCREEN_HEIGHT
#define COSMONARCHY_SCREEN_HEIGHT 720
#endif
    constexpr int screen_width = COSMONARCHY_SCREEN_WIDTH;
    constexpr int screen_height = COSMONARCHY_SCREEN_HEIGHT;
    // Keep both policies available. `centered_native_box` preserves the
    // confirmed stock-like 640-wide top UI, while `screen_edges` anchors
    // objectives to the output's left and resources to its right.
    constexpr TopUiLayout top_ui_layout = TopUiLayout::centered_native_box;
    // These compile-time values remain the safe fallback and offline-verifier
    // target. Runtime renderer geometry is owned by resolution::Configure().
    constexpr int game_width = screen_width;
    constexpr int game_height =
        screen_height - (native_height - native_game_height);

    // Several GPTP camera helpers use the center of StarCraft's unobscured
    // native playfield (y=140), rather than the nominal 400-pixel surface.
    constexpr int camera_center_x = game_width / 2;
    constexpr int camera_center_y =
        140 + (game_height - native_game_height) / 2;
}

#endif // COSMONARCHY_ZOOM_RESOLUTION_H
