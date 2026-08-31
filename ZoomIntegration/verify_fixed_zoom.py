#!/usr/bin/env python3
"""Offline geometry checks for the resolution-derived zoom compositor."""

from pathlib import Path
import re


SHARED_CONFIG = Path(__file__).resolve().parents[1] / "ZoomSource" / "zoom_resolution.h"
CONFIG_TEXT = SHARED_CONFIG.read_text(encoding="utf-8")


def config_value(name: str) -> int:
    match = re.search(rf"constexpr int {re.escape(name)}\s*=\s*(\d+)\s*;", CONFIG_TEXT)
    if match:
        return int(match.group(1))
    symbol_match = re.search(
        rf"constexpr int {re.escape(name)}\s*=\s*([A-Z][A-Z0-9_]*)\s*;",
        CONFIG_TEXT,
    )
    if symbol_match:
        default_match = re.search(
            rf"#define\s+{re.escape(symbol_match.group(1))}\s+(\d+)",
            CONFIG_TEXT,
        )
        if default_match:
            return int(default_match.group(1))
    raise RuntimeError(f"Missing integer or macro default {name} in {SHARED_CONFIG}")

NATIVE_WIDTH = config_value("native_width")
NATIVE_HEIGHT = config_value("native_height")
NATIVE_GAME_HEIGHT = config_value("native_game_height")
NATIVE_HUD_TOP = 314
CAMERA_QUANTUM = 8
SAFE_GAME_HEIGHT = NATIVE_HUD_TOP // CAMERA_QUANTUM * CAMERA_QUANTUM
MAX_PASS_WIDTH = NATIVE_WIDTH
MAX_PASS_HEIGHT = 256
OUTPUT_WIDTH = config_value("screen_width")
OUTPUT_HEIGHT = config_value("screen_height")


def align_up(value: int, alignment: int) -> int:
    return (value + alignment - 1) // alignment * alignment


def geometry(output_width: int, output_height: int) -> dict[str, int]:
    game_width = output_width
    game_height = output_height - (NATIVE_HEIGHT - NATIVE_GAME_HEIGHT)
    columns = (game_width + MAX_PASS_WIDTH - 1) // MAX_PASS_WIDTH
    rows = (game_height + MAX_PASS_HEIGHT - 1) // MAX_PASS_HEIGHT
    tile_width = align_up((game_width + columns - 1) // columns, CAMERA_QUANTUM)
    tile_height = align_up((game_height + rows - 1) // rows, CAMERA_QUANTUM)
    return {
        "output_width": output_width,
        "output_height": output_height,
        "game_width": game_width,
        "game_height": game_height,
        "columns": columns,
        "rows": rows,
        "tile_width": tile_width,
        "tile_height": tile_height,
    }


def verify_camera(g: dict[str, int], map_width: int, map_height: int,
                  base_x: int, base_y: int) -> None:
    game_width = g["game_width"]
    game_height = g["game_height"]
    tile_width = g["tile_width"]
    tile_height = g["tile_height"]
    assert map_width >= game_width
    assert map_height >= game_height
    native_max_x = max(0, map_width - NATIVE_WIDTH)
    safe_max_y = max(0, map_height - SAFE_GAME_HEIGHT)
    expanded_max_x = max(0, map_width - game_width)
    expanded_max_y = max(0, map_height - game_height)
    base_x = min(max(0, base_x), expanded_max_x)
    base_y = min(max(0, base_y), expanded_max_y)

    covered_area = 0
    for row in range(g["rows"]):
        destination_y = row * tile_height
        copy_height = min(tile_height, game_height - destination_y)
        for column in range(g["columns"]):
            destination_x = column * tile_width
            copy_width = min(tile_width, game_width - destination_x)
            desired_x = base_x + destination_x
            desired_y = base_y + destination_y
            horizontal_overlap = NATIVE_WIDTH - tile_width
            vertical_overlap = SAFE_GAME_HEIGHT - tile_height
            overlap_x = horizontal_overlap // 2 if column else 0
            overlap_y = vertical_overlap // 2 if row else 0
            render_x = max(0, desired_x - overlap_x)
            render_y = max(0, desired_y - overlap_y)
            actual_x = min(render_x, native_max_x)
            actual_y = min(render_y, safe_max_y)
            source_x = min(desired_x - actual_x, NATIVE_WIDTH - copy_width)
            source_y = min(desired_y - actual_y,
                           SAFE_GAME_HEIGHT - copy_height)
            assert source_x + copy_width <= NATIVE_WIDTH
            assert source_y + copy_height <= SAFE_GAME_HEIGHT
            assert destination_x + copy_width <= game_width
            assert destination_y + copy_height <= game_height
            covered_area += copy_width * copy_height
    # Destinations are a regular non-overlapping row/column grid; exact area
    # therefore proves complete coverage without allocating a pixel set.
    assert covered_area == game_width * game_height


def verify_resolution(width: int, height: int) -> None:
    g = geometry(width, height)
    assert width >= NATIVE_WIDTH
    assert height >= NATIVE_HEIGHT
    assert g["tile_width"] <= NATIVE_WIDTH
    assert g["tile_height"] <= SAFE_GAME_HEIGHT
    assert g["game_height"] + (NATIVE_HEIGHT - NATIVE_GAME_HEIGHT) == height
    hud_height = NATIVE_HEIGHT - NATIVE_HUD_TOP
    hud_left = (width - NATIVE_WIDTH) // 2
    hud_top = height - hud_height
    assert 0 <= hud_left and hud_left + NATIVE_WIDTH <= width
    assert hud_top + hud_height == height

    popup_left, popup_top, popup_right, popup_bottom = 184, 65, 448, 321
    ui_left = (width - NATIVE_WIDTH) // 2
    ui_top = (height - NATIVE_HEIGHT) // 2
    assert 0 <= ui_left + popup_left < ui_left + popup_right <= width
    assert 0 <= ui_top + popup_top < ui_top + popup_bottom <= height

    # Front-end menus keep a native 4:3 source and use the largest centered
    # aspect-fit rectangle in the logical output. Verify both the presentation
    # bounds and the inverse input map used by ConsoleWndProc.
    menu_width_if_height_fills = height * NATIVE_WIDTH // NATIVE_HEIGHT
    if menu_width_if_height_fills <= width:
        menu_width = menu_width_if_height_fills
        menu_height = height
    else:
        menu_width = width
        menu_height = width * NATIVE_HEIGHT // NATIVE_WIDTH
    menu_left = (width - menu_width) // 2
    menu_top = (height - menu_height) // 2
    assert menu_width > 0 and menu_height > 0
    assert menu_left >= 0 and menu_top >= 0
    assert menu_left + menu_width <= width
    assert menu_top + menu_height <= height
    for output_x in (menu_left, menu_left + menu_width // 2,
                     menu_left + menu_width - 1):
        native_x = (output_x - menu_left) * NATIVE_WIDTH // menu_width
        assert 0 <= native_x < NATIVE_WIDTH
    for output_y in (menu_top, menu_top + menu_height // 2,
                     menu_top + menu_height - 1):
        native_y = (output_y - menu_top) * NATIVE_HEIGHT // menu_height
        assert 0 <= native_y < NATIVE_HEIGHT

    for map_width, map_height in ((2048, 2048), (4096, 4096), (8192, 8192)):
        if map_width < width or map_height < g["game_height"]:
            continue
        max_x = map_width - width
        max_y = map_height - g["game_height"]
        for x in (0, 1, max_x // 2, max_x - 1, max_x, map_width):
            for y in (0, 1, max_y // 2, max_y - 1, max_y, map_height):
                verify_camera(g, map_width, map_height, x, y)


def main() -> int:
    packaged_profiles = (
        (640, 480), (800, 600), (960, 720), (1280, 960),
        (960, 540), (1024, 576), (1280, 720), (1600, 900),
        (1920, 1080), (2560, 1440), (3840, 2160),
    )
    # Non-preset dimensions exercise the same formulas used by custom input.
    resolutions = tuple(dict.fromkeys((
        (OUTPUT_WIDTH, OUTPUT_HEIGHT), *packaged_profiles,
        (1366, 768), (1920, 1200), (2560, 1080),
    )))
    for width, height in resolutions:
        verify_resolution(width, height)
    target = geometry(OUTPUT_WIDTH, OUTPUT_HEIGHT)
    target_menu_width = min(
        OUTPUT_WIDTH, OUTPUT_HEIGHT * NATIVE_WIDTH // NATIVE_HEIGHT)
    target_menu_height = min(
        OUTPUT_HEIGHT, OUTPUT_WIDTH * NATIVE_HEIGHT // NATIVE_WIDTH)
    print(
        f"resolution-derived compositor {OUTPUT_WIDTH}x{OUTPUT_HEIGHT}: "
        f"battlefield={target['game_width']}x{target['game_height']} "
        f"grid={target['columns']}x{target['rows']} "
        f"tile={target['tile_width']}x{target['tile_height']} "
        f"safe-height={SAFE_GAME_HEIGHT}: PASS"
    )
    print(
        f"front-end native menu {NATIVE_WIDTH}x{NATIVE_HEIGHT}: "
        f"aspect-fit={target_menu_width}x{target_menu_height} "
        f"origin=({(OUTPUT_WIDTH - target_menu_width) // 2},"
        f"{(OUTPUT_HEIGHT - target_menu_height) // 2}): PASS"
    )
    print(
        f"profile geometry pack ({len(packaged_profiles)} resolutions): PASS"
    )
    print(f"derived geometry matrix ({len(resolutions)} resolutions): PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
