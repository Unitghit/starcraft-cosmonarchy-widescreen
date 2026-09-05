#!/usr/bin/env python3
"""Offline invariants for the optional gameplay-zoom coordinate transform."""


RESOLUTIONS = (
    (640, 480),
    (1024, 768),
    (1280, 720),
    (1600, 900),
    (1920, 1080),
    (2560, 1440),
    (3840, 2160),
)
PERCENTAGES = (112.5, 125, 137.5, 150, 162.5, 175, 187.5, 200)
HUD_HEIGHT = 80
ANIMATION_MS = 120


def source_extent(output_extent: int, percentage: int) -> int:
    zoom_units = round(percentage * 100)
    return max(1, min(output_extent * 10000 // zoom_units, output_extent))


def edge_crop(output_extent: int, source: int, preferred: int,
              camera: int, maximum_camera: int) -> int:
    extra = output_extent - source
    preferred = min(preferred, extra)
    if maximum_camera == 0:
        return preferred
    camera = min(camera, maximum_camera)
    far_distance = maximum_camera - camera
    if camera < preferred:
        return camera
    far_margin = extra - preferred
    if far_distance < far_margin:
        return extra - far_distance
    return preferred


def animated_zoom(start: int, target: int, elapsed: int) -> int:
    if elapsed >= ANIMATION_MS:
        return target
    numerator = elapsed * (2 * ANIMATION_MS - elapsed)
    denominator = ANIMATION_MS ** 2
    return start + (target - start) * numerator // denominator


def sample_offset(presented: int, source: int, output: int) -> int:
    presented = max(0, min(presented, output - 1))
    return (2 * presented + 1) * source // (2 * output)


def anchored_camera(world: int, camera: int, presented: int,
                    crop: int, output: int, source: int,
                    maximum: int) -> int:
    current_world = camera + crop + sample_offset(presented, source, output)
    requested = camera + world - current_world
    return max(0, min(requested, maximum))


def verify() -> None:
    inward = [animated_zoom(10000, 20000, elapsed)
              for elapsed in range(ANIMATION_MS + 1)]
    outward = [animated_zoom(20000, 10000, elapsed)
               for elapsed in range(ANIMATION_MS + 1)]
    assert inward[0] == 10000 and inward[-1] == 20000
    assert outward[0] == 20000 and outward[-1] == 10000
    assert all(left <= right for left, right in zip(inward, inward[1:]))
    assert all(left >= right for left, right in zip(outward, outward[1:]))

    for width, height in RESOLUTIONS:
        battlefield_height = height - HUD_HEIGHT
        for percentage in PERCENTAGES:
            source_width = source_extent(width, percentage)
            source_height = source_extent(height, percentage)
            assert 0 < source_width <= width
            assert 0 < source_height <= height
            assert source_width * source_height <= width * height

            map_width = width * 4
            map_height = battlefield_height * 4
            max_x = map_width - width
            max_y = map_height - battlefield_height
            extra_x = width - source_width
            extra_y = height - source_height
            preferred_x = extra_x // 2
            camera_center_y = 140 + (battlefield_height - 400) // 2
            zoom_units = round(percentage * 100)
            preferred_y = (camera_center_y -
                           camera_center_y * 10000 // zoom_units)
            assert edge_crop(width, source_width, preferred_x, 0, max_x) == 0
            assert edge_crop(
                width, source_width, preferred_x, max_x, max_x) == extra_x
            assert edge_crop(
                height, source_height, preferred_y, 0, max_y) == 0
            assert edge_crop(
                height, source_height, preferred_y, max_y, max_y
            ) == extra_y

            crop_x = edge_crop(
                width, source_width, preferred_x, max_x // 2, max_x)
            crop_y = edge_crop(
                height, source_height, preferred_y, max_y // 2, max_y)
            for presented_x in (0, width // 4, width // 2, width - 1):
                source_x = crop_x + ((2 * presented_x + 1) * source_width
                                     // (2 * width))
                restored_x = (source_x - crop_x) * width // source_width
                assert abs(restored_x - presented_x) <= percentage // 100 + 2
            for presented_y in (
                    0, battlefield_height // 4,
                    battlefield_height // 2, battlefield_height - 1):
                source_y = crop_y + ((2 * presented_y + 1) * source_height
                                     // (2 * height))
                restored_y = ((source_y - crop_y) * height
                              // source_height)
                assert abs(restored_y - presented_y) <= percentage // 100 + 2

            x_lookup = [((2 * x + 1) * source_width // (2 * width))
                        for x in range(width)]
            y_lookup = [((2 * y + 1) * source_height // (2 * height))
                        for y in range(height)]
            assert all(left <= right
                       for left, right in zip(x_lookup, x_lookup[1:]))
            assert all(top <= bottom
                       for top, bottom in zip(y_lookup, y_lookup[1:]))
            assert x_lookup[0] + x_lookup[-1] == source_width - 1
            assert y_lookup[0] + y_lookup[-1] == source_height - 1

            # A wheel step must retain the same world pixel beneath several
            # representative pointer positions by advancing the real camera,
            # not by retaining an off-center crop after the transition.
            for pointer_x in (0, width // 4, width // 2,
                              width * 3 // 4, width - 1):
                camera = width
                old_world = camera + sample_offset(pointer_x, width, width)
                adjusted = anchored_camera(
                    old_world, camera, pointer_x, preferred_x,
                    width, source_width, max_x)
                new_world = adjusted + preferred_x + sample_offset(
                    pointer_x, source_width, width)
                assert abs(new_world - old_world) <= 1
            for pointer_y in (0, battlefield_height // 4,
                              battlefield_height // 2,
                              battlefield_height - 1):
                camera = battlefield_height
                old_world = camera + sample_offset(pointer_y, height, height)
                adjusted = anchored_camera(
                    old_world, camera, pointer_y, preferred_y,
                    height, source_height, max_y)
                new_world = adjusted + preferred_y + sample_offset(
                    pointer_y, source_height, height)
                assert abs(new_world - old_world) <= 1

    print("optional gameplay zoom geometry matrix: PASS")


if __name__ == "__main__":
    verify()
    # Guard the actual event boundary, not only the camera equations. Ordinary
    # mouse messages are already normalized by the wrapper; wheel lParam is
    # deliberately not a second coordinate source for zoom.
    from pathlib import Path
    root = Path(__file__).resolve().parents[1]
    console = (root / "ZoomSource/Cosmonarchy-aidebug-resolution/src/scconsole.cpp").read_text()
    wheel = console.split("if (msg == WM_MOUSEWHEEL", 1)[1].split("// Front-end menus", 1)[0]
    assert "latest_physical_mouse_x, latest_physical_mouse_y" in wheel
    assert "if (latest_physical_mouse_valid &&" in wheel
    assert "LOWORD(lparam)" not in wheel and "HIWORD(lparam)" not in wheel
    assert "WheelPointToClient" not in wheel and "ScreenToClient" not in wheel
    print("wheel event uses normalized gameplay pointer, not window-origin conversion: PASS")
