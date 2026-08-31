# Coordinate spaces

Every renderer or input value must be assigned to one of these spaces before
it is modified.

| Space | 1280x720 extent | Origin and use |
|---|---:|---|
| Physical output | 1280x720 | Windows client and presented surface |
| Expanded battlefield | 1280x640 | Visible world and gameplay input |
| Native frame | 640x480 | StarCraft layer renderer and UI source |
| Native gameplay viewport | 640x400 | Stock game clipping and semantic bounds |
| Native safe private pass | 640x312 maximum | Region above the HUD-shaped terrain mask |
| Private pass tile | 640x216 | Derived copied region for the current target |
| Native HUD source | x=0..639, y=314..479 | Stock console artwork and controls |
| Presented HUD | x=320..959, y=554..719 | Bottom-centered native HUD |
| Native modal UI | 640x480 | StarCraft dialog tree and hit testing |
| Presented modal UI | offset (320,120) | Centered native modal dialog |
| World pixels | map-dependent | Camera origin plus battlefield position |
| Map tiles | world / 32 | Building placement and terrain logic |
| Pass-local screen | 640x480 | World position relative to a private camera |

## Derived geometry

The only intended user-selected values are `screen_width` and
`screen_height` in `ZoomSource/zoom_resolution.h`.

```text
game_width  = screen_width
game_height = screen_height - (native_height - native_game_height)

hud_height = native_height - native_hud_top
hud_left   = (screen_width - native_width) / 2
hud_top    = screen_height - hud_height

native_ui_left = (screen_width - native_width) / 2
native_ui_top  = (screen_height - native_height) / 2
```

At 1280x720, `game_height` is 640, `hud_left` is 320, `hud_top` is
554, and `native_ui_top` is 120.

## World-to-pass conversion

An object prepared relative to the outer expanded camera must be translated
for private camera `P` using:

```text
pass_local_x = outer_local_x + outer_camera_x - P.camera_x
pass_local_y = outer_local_y + outer_camera_y - P.camera_y
```

Only position fields receive this delta. Width, height, pitch, bitmap pointer,
tile coordinate, and world coordinate do not.

## Input conversion

- Battlefield: physical coordinate is forwarded unchanged.
- Presented HUD: subtract `(hud_left, hud_top - native_hud_top)` only when the
  native STrans mask says the pixel belongs to solid HUD artwork.
- Modal dialog: subtract `(native_ui_left, native_ui_top)` and clamp to the
  native frame.
- Obsolete invisible native HUD: bypass its dialog hit test, then restore the
  original physical coordinate in the gameplay callback.

## Common category errors

- `DrawLayer.left/top` are positions; `DrawLayer.width/height` are dimensions.
- The aidebug `DrawLayer.area.right/bottom` member names are misleading for
  StarCraft layers: the underlying fields are width and height.
- `0x64095C` and `0x640964` are placement **Surface** records, not positions.
- The rectangle at `0x5993B0` is consumed by both input and rendering. Do not
  globally enlarge it; redirect gameplay-only consumers to an expanded copy.
- The native `0x48D5F2` placement draw clip must remain 640x400 per pass.
