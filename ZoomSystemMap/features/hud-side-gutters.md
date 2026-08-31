# HUD side-gutter world strip

## Goal

Remove the artificial black bars beside the bottom-centered 640-pixel HUD
without changing the logical camera, minimap viewport, or HUD placement.

At any configured output size, the compositor presents:

- the normal rectangular battlefield from y=0 through `game_height - 1`;
- real world pixels from y=`game_height` through `screen_height - 1` only
  to the left and right of the centered HUD;
- the unchanged native HUD over the center of that full world strip.

## Derived geometry

The side-gutter rectangles are calculated at runtime:

```text
left gutter:  x = 0 .. hud_left - 1
right gutter: x = hud_left + native_width .. screen_width - 1
vertical:     y = game_height .. screen_height - 1
```

No resolution-specific coordinates are stored. Odd output widths may produce
gutters that differ by one pixel, which follows the existing centered HUD
origin exactly.

The logical camera height remains `game_height`. This preserves camera
centering, camera bounds, the minimap viewport box, and positional-audio
geometry. At the bottom edge of a map, rows outside the map remain empty rather
than repeating the last terrain rows.

## Compositor policy

Tile rows are still selected from the logical battlefield height. Tile height
is derived from the full output height, then aligned to the eight-pixel camera
quantum. This lets the existing final row contain the 80-pixel side strip
without adding private world passes.

Each private pass first copies its normal battlefield rectangle. Any remaining
rows below `game_height` are copied only where that tile intersects a side
gutter. The center 640 pixels are left for the native HUD compositor.

At 1280x720 this keeps the same 2x3 pass grid and changes each tile from
640x216 to 640x240.

## Input ownership

Gameplay semantic rectangles use the full output height so cursor hover,
selection, drag clipping, commands, and building placement can reach the
visible side gutters. The centered HUD rectangle remains authoritative:

- its entire bottom row is translated to native HUD coordinates;
- gameplay callbacks are suppressed while a HUD event is dispatched;
- points outside that rectangle remain one-to-one gameplay coordinates.

This creates a nonrectangular visible gameplay region without changing the
engine's logical camera rectangle.

## Verification

`ZoomIntegration/verify_fixed_zoom.py` proves for every preset and custom
test resolution that:

- the normal battlefield remains completely covered;
- the existing tile count covers the full output height;
- only the two derived gutter widths receive below-battlefield pixels;
- private source rectangles remain within the 640x312 safe render surface;
- partial rows at map boundaries are copied without repeating terrain.

## Confirmed result

User testing at 1280x720 confirmed that both side gutters render the world to
the bottom edge and accept gameplay interaction while the centered HUD remains
fully functional.
