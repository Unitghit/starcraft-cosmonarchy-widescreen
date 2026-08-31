#ifndef RESOLUTION_H
#define RESOLUTION_H

#include "types.h"
#include "../../zoom_resolution.h"

namespace resolution
{
    constexpr unsigned maximum_screen_width = 3840;
    constexpr unsigned maximum_screen_height = 2160;
    constexpr size_t maximum_frame_size =
        static_cast<size_t>(maximum_screen_width) * maximum_screen_height;

    constexpr xuint native_width = zoom_resolution_config::native_width;
    constexpr yuint native_height = zoom_resolution_config::native_height;
    constexpr yuint native_game_height =
        zoom_resolution_config::native_game_height;
    constexpr yuint native_hud_top = 314;
    constexpr yuint native_hud_protrusion_top = 293;
    constexpr unsigned camera_quantum = 8;
    constexpr yuint native_safe_game_height =
        native_hud_top / camera_quantum * camera_quantum;
    constexpr yuint hud_height = native_height - native_hud_top;
    constexpr xuint safe_pass_width = native_width;
    constexpr yuint safe_pass_height = 256;
    inline bool top_ui_uses_screen_edges =
        zoom_resolution_config::top_ui_layout ==
            zoom_resolution_config::TopUiLayout::screen_edges;

    inline xuint screen_width = xuint(
        static_cast<unsigned>(zoom_resolution_config::screen_width));
    inline yuint screen_height = yuint(
        static_cast<unsigned>(zoom_resolution_config::screen_height));
    inline xuint game_width = screen_width;
    inline yuint game_height = yuint(
        static_cast<unsigned>(zoom_resolution_config::game_height));
    inline xuint camera_center_x = xuint(
        static_cast<unsigned>(zoom_resolution_config::camera_center_x));
    inline yuint camera_center_y = yuint(
        static_cast<unsigned>(zoom_resolution_config::camera_center_y));
    inline yuint game_height_tiles = game_height / 32 + 1;
    inline xuint hud_left = (screen_width - native_width) / 2;
    inline yuint hud_top = screen_height - hud_height;
    inline xuint native_ui_left = (screen_width - native_width) / 2;
    inline yuint native_ui_top = (screen_height - native_height) / 2;
    inline xuint top_ui_left = top_ui_uses_screen_edges ?
        xuint(0u) : native_ui_left;
    inline xuint top_ui_right_native_origin = top_ui_uses_screen_edges ?
        xuint(static_cast<unsigned int>(screen_width - native_width)) :
        native_ui_left;

    inline xuint menu_width = native_width;
    inline yuint menu_height = native_height;
    inline xuint menu_left = (screen_width - menu_width) / 2;
    inline yuint menu_top = (screen_height - menu_height) / 2;

    inline unsigned tile_columns = 1;
    inline unsigned tile_rows = 1;
    inline xuint tile_width = native_width;
    inline yuint tile_height = safe_pass_height;

    inline bool Configure(unsigned width, unsigned height,
        bool use_screen_edges =
            zoom_resolution_config::top_ui_layout ==
                zoom_resolution_config::TopUiLayout::screen_edges)
    {
        if (width < static_cast<unsigned>(native_width) ||
            height < static_cast<unsigned>(native_height) ||
            width > maximum_screen_width || height > maximum_screen_height)
            return false;

        screen_width = xuint(width);
        screen_height = yuint(height);
        top_ui_uses_screen_edges = use_screen_edges;
        game_width = xuint(width);
        game_height = yuint(height -
            (static_cast<unsigned>(native_height) -
             static_cast<unsigned>(native_game_height)));
        camera_center_x = game_width / 2;
        camera_center_y = yuint(140u +
            (static_cast<unsigned>(game_height) -
             static_cast<unsigned>(native_game_height)) / 2u);
        game_height_tiles = game_height / 32 + 1;
        hud_left = (screen_width - native_width) / 2;
        hud_top = screen_height - hud_height;
        native_ui_left = (screen_width - native_width) / 2;
        native_ui_top = (screen_height - native_height) / 2;
        top_ui_left = top_ui_uses_screen_edges ? xuint(0u) : native_ui_left;
        top_ui_right_native_origin = top_ui_uses_screen_edges ?
            xuint(width - static_cast<unsigned>(native_width)) :
            native_ui_left;

        const unsigned menu_width_if_height_fills = height *
            static_cast<unsigned>(native_width) /
            static_cast<unsigned>(native_height);
        const bool menu_is_height_limited = menu_width_if_height_fills <= width;
        menu_width = xuint(menu_is_height_limited ?
            menu_width_if_height_fills : width);
        menu_height = yuint(menu_is_height_limited ? height :
            width * static_cast<unsigned>(native_height) /
                static_cast<unsigned>(native_width));
        menu_left = (screen_width - menu_width) / 2;
        menu_top = (screen_height - menu_height) / 2;

        tile_columns = (static_cast<unsigned>(game_width) +
            static_cast<unsigned>(safe_pass_width) - 1) /
            static_cast<unsigned>(safe_pass_width);
        tile_rows = (static_cast<unsigned>(game_height) +
            static_cast<unsigned>(safe_pass_height) - 1) /
            static_cast<unsigned>(safe_pass_height);
        const unsigned unaligned_tile_width =
            (static_cast<unsigned>(game_width) + tile_columns - 1) /
            tile_columns;
        const unsigned unaligned_tile_height =
            (static_cast<unsigned>(screen_height) + tile_rows - 1) /
            tile_rows;
        tile_width = xuint((unaligned_tile_width + camera_quantum - 1) /
            camera_quantum * camera_quantum);
        tile_height = yuint((unaligned_tile_height + camera_quantum - 1) /
            camera_quantum * camera_quantum);

        return tile_width <= native_width &&
            tile_height <= native_safe_game_height &&
            tile_rows * static_cast<unsigned>(tile_height) >=
                static_cast<unsigned>(screen_height) &&
            hud_left + native_width <= screen_width &&
            hud_top + hud_height == screen_height &&
            top_ui_left + native_width <= screen_width &&
            top_ui_right_native_origin + native_width <= screen_width &&
            menu_width > xuint(0u) && menu_height > yuint(0u) &&
            menu_left + menu_width <= screen_width &&
            menu_top + menu_height <= screen_height;
    }

    static_assert(maximum_screen_width <= 0x7fff &&
                  maximum_screen_height <= 0x7fff,
                  "maximum resolution must fit signed engine coordinates");
}

#endif // RESOLUTION_H
