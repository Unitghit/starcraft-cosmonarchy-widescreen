#!/usr/bin/env python3
"""Prove the optimized native-menu scaler matches the reference formula."""

import hashlib


NATIVE_WIDTH = 640
NATIVE_HEIGHT = 480
RESOLUTIONS = (
    (640, 480),
    (800, 600),
    (853, 601),
    (960, 540),
    (1024, 576),
    (1280, 720),
    (1600, 900),
    (1920, 1080),
    (2560, 1440),
    (3840, 2160),
)


def menu_geometry(width, height):
    width_if_height_fills = height * NATIVE_WIDTH // NATIVE_HEIGHT
    if width_if_height_fills <= width:
        menu_width = width_if_height_fills
        menu_height = height
    else:
        menu_width = width
        menu_height = width * NATIVE_HEIGHT // NATIVE_WIDTH
    return (
        menu_width,
        menu_height,
        (width - menu_width) // 2,
        (height - menu_height) // 2,
    )


SOURCE = bytes(
    (x * 37 + y * 53 + (x * y) % 251) & 0xFF
    for y in range(NATIVE_HEIGHT)
    for x in range(NATIVE_WIDTH)
)


def reference(width, height):
    menu_width, menu_height, left, top = menu_geometry(width, height)
    output = bytearray(width * height)
    for destination_y in range(menu_height):
        source_y = destination_y * NATIVE_HEIGHT // menu_height
        destination_offset = (top + destination_y) * width + left
        source_offset = source_y * NATIVE_WIDTH
        for destination_x in range(menu_width):
            source_x = destination_x * NATIVE_WIDTH // menu_width
            output[destination_offset + destination_x] = SOURCE[
                source_offset + source_x
            ]
    return output


def optimized(width, height):
    menu_width, menu_height, left, top = menu_geometry(width, height)
    output = bytearray(width * height)
    source_x_by_destination = [
        destination_x * NATIVE_WIDTH // menu_width
        for destination_x in range(menu_width)
    ]
    previous_source_y = NATIVE_HEIGHT
    previous_row = None
    for destination_y in range(menu_height):
        source_y = destination_y * NATIVE_HEIGHT // menu_height
        destination_offset = (top + destination_y) * width + left
        if source_y == previous_source_y:
            output[destination_offset : destination_offset + menu_width] = (
                previous_row
            )
            continue
        source_offset = source_y * NATIVE_WIDTH
        row = bytes(
            SOURCE[source_offset + source_x]
            for source_x in source_x_by_destination
        )
        output[destination_offset : destination_offset + menu_width] = row
        previous_source_y = source_y
        previous_row = row
    return output


def main():
    for width, height in RESOLUTIONS:
        expected = reference(width, height)
        actual = optimized(width, height)
        if actual != expected:
            raise SystemExit(f"menu scaler mismatch at {width}x{height}")
        digest = hashlib.sha256(actual).hexdigest()[:12]
        print(f"menu scaler {width}x{height}: PASS ({digest})")


if __name__ == "__main__":
    main()
