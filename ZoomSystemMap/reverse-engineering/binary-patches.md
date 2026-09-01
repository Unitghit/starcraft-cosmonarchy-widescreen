# Binary patch ledger

All StarCraft patches below are installed from
`Cosmonarchy-aidebug-resolution/src/limits.cpp` or `src/mainpatch.cpp`. Every
operand is preflighted against its expected native value before replacement.

## Physical output

| Address | Native | Replacement |
|---:|---:|---:|
| `0x0041DA3E` | height 480 | `screen_height` |
| `0x0041DA43` | width 640 | `screen_width` |

## Cursor and OS input bounds

| Address | Native meaning | Replacement |
|---:|---|---|
| `0x00421603` | width 640 | `screen_width` |
| `0x0042160A` | height 480 | `screen_height` |
| `0x004D1300` | x edge 638 | `screen_width - 2` |
| `0x004D1334` | y edge 478 | `screen_height - 2` |
| `0x004D1963`, `0x004D19EF`, `0x004D1A7F`, `0x004D24E8` | width 640 | `screen_width` |
| `0x004D196F`, `0x004D19FB`, `0x004D1A8B`, `0x004D24F2` | x max 639 | `screen_width - 1` |
| `0x004D1984`, `0x004D1A10`, `0x004D1AA0`, `0x004D2506` | height 480 | `screen_height` |
| `0x004D1990`, `0x004D1A1C`, `0x004D1AAC`, `0x004D2512` | y max 479 | `screen_height - 1` |

The imported `ClipCursor` entry at `0x004FE37C` is replaced with
`ClipCursorExpanded`.

The imported `SetCursorPos` entry at `0x004FE2CC` is replaced with a guarded
forwarder. It calls the original function normally, but returns success without
warping while `ConsoleWndProc` is dispatching translated relocated-UI input or
while a captured minimap drag remains active between messages. This prevents
minimap clicks and held camera drags from generating a stale synthetic mouse
move at the native 4:3 coordinate.

Stable GPTP owns a separate `SetCursorPos` import at module RVA `0x1D6274`.
Its only verified caller is the custom cursor-update call at RVA `0x67F7E`.
After preflighting the two preceding relocated coordinate-pointer loads, the
indirect-call opcode, its IAT operand, and the original imported function, the
same guarded forwarder is installed in that slot at runtime.

The bundled cnc-ddraw `ddraw.dll` also imports `ClipCursor` at RVA `0x3C330`
and `SetCursorPos` at RVA `0x3C34C`. Their import thunks at RVAs `0x261CC` and
`0x261A2` are preflighted as `FF 25` jumps to those exact loaded IAT slots.
Both slots then use the same translated-HUD guards, preventing the presentation
shim from clamping a minimap interaction to its legacy 4:3 clip corner.

## Gameplay bounds

| Address | Purpose | Replacement |
|---:|---|---|
| `0x0046FC76/88` | alternate visible-unit width/height | `game_width/game_height` |
| `0x0046FE19/2B` | normal visible-unit width/height | `game_width/game_height` |
| `0x0046FFAA` | stored drag rectangle pointer | `expanded_drag_clip_rect` |
| `0x0048D665/70` | stock placement acceptance | `game_width/game_height` |
| `0x0048468E` | gameplay PtInRect source | `expanded_game_rect` |
| `0x004D14AA` | gameplay PtInRect source | `expanded_game_rect` |

The shared native rectangle at `0x005993B0` is not modified.

## Positional audio viewport

Cosmonarchy's stable GPTP audio system calls StarCraft's native pan helper at
`0x0048E850` and volume helper at `0x0048E8D0` for new and active positional
sounds. Their attenuation curves remain native; only verified geometry
immediates change:

| Address | Native | Replacement |
|---:|---:|---:|
| `0x0048E869` | horizontal center 10 tiles | `game_width / 64` |
| `0x0048E8E8`, `0x0048E8F5` | width 640 | `game_width` |
| `0x0048E90D` | height 400 | `game_height` |
| `0x0048E91A` | displacement -400 | `-game_height` |

The pan and volume opcode sequences are preflighted in addition to all five
operands. Presentation-only scaling is excluded from these values.

## Match-start location camera origin

During CHK unit decoding, StarCraft `0x004CB190` recognizes each start-location
unit and stores its world position. For the local player it also converts that
position to tiles, subtracts a native camera center, and writes the result to
`g_move_to_tile_x/y` at `0x0057F1D0/D2`. Later, `InitScreenPositions` at
`0x004BD3F0` restores those tile values through native `move_screen`.

| Address | Native | Replacement |
|---:|---:|---:|
| `0x004CB1EB` | x center 10 tiles | `camera_center_x / 32` |
| `0x004CB1FB` | y center 6 tiles | `camera_center_y / 32` |

At 1280x720 the replacements are 20 and 8 tiles. The y value intentionally
uses tile quantization: configured y=260 becomes 8 tiles/256 pixels, matching
the engine's existing camera-origin precision. The full 36-byte shift,
subtract, underflow-clamp sequence at `0x004CB1DE` is preflighted before either
signed imm8 changes. Live central-camera tracing confirmed the resulting
`InitScreenPositions` call was the first real match camera writer.

## Trigger Center View camera origin

Campaign and scenario starts commonly use StarCraft's trigger `Center View`
action at `0x004C6DE0`. The action averages a location's world bounds and then
subtracts the viewport center before calling native `move_screen` at
`0x0049C440`. Its original constants place the target at the legacy 4:3 center.

| Address | Native | Replacement |
|---:|---:|---:|
| `0x004C6E6A` | y center 200 | `camera_center_y` |
| `0x004C6E6F` | x center 320 | `camera_center_x` |

The complete 20-byte midpoint/subtraction/call sequence at `0x004C6E64` is
preflighted before either immediate changes. At 1280x720 the replacements are
y=260 and x=640. Native `move_screen` retains map-edge clamping, and external
presentation magnification is intentionally excluded.

## Middle-mouse pan viewport range

The native initializer at `0x00484520` and movement callback at `0x00484460`
subtract 640 and 400 from the map dimensions before converting cursor
position to a scroll target. A signature-checked replacement uses the runtime
logical battlefield dimensions for both calculations. It also treats maps no
larger than the viewport as a zero-range axis, so high internal resolutions do
not underflow the native unsigned division.

## Portrait camera centering

The native status and transmission portrait target encoders subtract the
640x400 playfield center. Their complete instruction sequences are preflighted
before changing only these signed displacements:

| Address | Native | Replacement |
|---:|---:|---:|
| `0x0045E3A4` | x displacement -320 | `-camera_center_x` |
| `0x0045E3B8` | y displacement -200 | `-camera_center_y` |
| `0x0045EE54` | x displacement -320 | `-camera_center_x` |
| `0x0045EE66` | y displacement -200 | `-camera_center_y` |

Cosmonarchy's live bottom-HUD portrait callback bypasses those native target
globals and derives a camera origin directly from the active portrait sprite.
A full event capture proved that its result is exactly the sprite world
position minus `(320,200)`, clamped and rounded to the engine's eight-pixel
camera grid. After a translated HUD left-button release, `ConsoleWndProc`
recognizes only that exact legacy result and repeats the operation using the
configured `camera_center_x/y`. This behavioral signature avoids guessed
portrait rectangles, leaves unrelated HUD controls alone, preserves native
map-edge clamping, and scales with every compiled internal-resolution profile.

## Draw hooks

| Site | Action |
|---:|---|
| `0x0041E297` | Call `BeginStockDrawScreen` |
| `0x0041E3FD` | Call `AfterStockDrawScreen` |
| `0x004BD614` | Replace text call with conditional/no-op native pass hook |
| `0x004BD619` | Replace selection-border call with conditional/no-op native pass hook |
| `0x004D1140` | Hook outside-game-screen predicate |
| `0x00411E4E/48` | Hook SDraw lock/unlock imports |

## Stable GPTP runtime patch

Module: stable `gptp.qdp`

Function RVA: `0x00087BF0`

| Function offset | Expected instruction prefix | Native immediate | Replacement |
|---:|---|---:|---:|
| `+0x04` | `81 3D C4 DD 6C 00` | 639 | `game_width - 1` |
| `+0x1C` | `81 3D C8 DD 6C 00` | 399 | `game_height - 1` |

The immediates begin six bytes into each instruction. The patch accepts either
the native pair or the already-patched configured pair, uses `VirtualProtect`,
flushes the instruction cache, restores protection, and logs the result.

### Same-type visible-unit selection

Function RVA: `0x000B52E0`

Stable GPTP replaces StarCraft's `unit_selection_click` at `0x0046FB40`.
Its modifier-selection branches bypass the native visible-unit bounds and
construct separate 640x400 rectangles. The following immediate operands are
patched only after the surrounding non-relocated instructions match the
stable build:

| GPTP operand RVA | Branch | Native value | Runtime value |
|---:|---|---:|---:|
| `0xB5437` | Ctrl width | 640 | `game_width` |
| `0xB544B` | Ctrl height | 400 | `screen_height` |
| `0xB5521` | Ctrl+Shift width | 640 | `game_width` |
| `0xB5542` | Ctrl+Shift height | 400 | `screen_height` |

`EnsureGptpSelectionBounds` accepts either all four native values or all four
configured values. A mixed value set or any signature mismatch is treated as
incompatible and no write occurs. The patch runs once after GPTP loads and
does not alter the module file on disk.

### Gameplay hover cursor

Selector sequence RVA: `0x000675B8`

The verified sequence pushes mouse y, mouse x, then the native game rectangle
before calling `PtInRect`. The push-immediate operand at RVA `0x675C5` changes
from `0x005993B0` to the runtime address of `expanded_game_rect`.

Preflight verifies:

- `FF 35 C8 DD 6C 00` (mouse y push);
- `FF 35 C4 DD 6C 00` (mouse x push);
- `68` (rectangle push-immediate opcode);
- `FF D6 85 C0` (call and result test).

Only the operand changes; GPTP's HUD/minimap checks and StarCraft's shared
native rectangle remain untouched.

### Minimap camera viewport dimensions

The stable GPTP module owns two writable `u16` globals used by both the local
white camera outline and multiplayer camera outlines:

| GPTP RVA | Role | Runtime value |
|---:|---|---|
| `0x26DC7C` | camera-box width | `max(2, ceil(game_width / zoom_level))` |
| `0x26D9DC` | camera-box height | `max(1, ceil((game_height + 16) / zoom_level))` |

Before resolving those globals, the renderer verifies the function at GPTP RVA
`0x2C5F0`: its prologue, four `movzx` opcodes, the two relocated global
operands, and the absolute `g_mouse.x/y` operands. It then synchronizes the
values whenever the map's `u16` minimap zoom level at StarCraft `0x0059CC6C`
changes. This is a runtime data update, not an on-disk GPTP modification.
External presentation magnification is intentionally excluded; the outline
represents the internal battlefield viewport.

### Extended upgrade research clear stabilization

Stable GPTP allocates `(100 - 46) * 12 = 648` bytes for extended per-player
upgrade research state. Its source applies the general ID bias of -46 to the
bulk-clear destination at StarCraft `0x004CCEC3`, causing the match-start
`REP STOSB` at `0x004CCEC7` to write 46 bytes before the allocation. Whether
that crashes depends on heap placement.

After GPTP loads, `EnsureGptpUpgradeResearchClear` verifies this sequence:

- `B9 88 02 00 00` (648-byte count);
- `BF <biased pointer>` (destination);
- `F3 AA` (`REP STOSB`);
- the following `B9` opcode.

It verifies that `[biased pointer + 46, + 648)` is committed writable memory,
then changes only the destination operand to `biased pointer + 46`. All other
ID-indexed GPTP pointers retain their intentional bias. The stable GPTP file
on disk remains unchanged.

## Invisible native HUD tooltips

Eight StarCraft shared control-lookup callsites are redirected through an
aidebug filter after their `E8` opcode and decoded target `0x00418340` are
verified:

| Callsite | Native owner | Filter behavior |
|---:|---|---|
| `0x00457E10` | Status event validation | Return no control over obsolete native HUD |
| `0x00457E50` | Status tooltip owner | Return no control over obsolete native HUD |
| `0x00458015` | Status tooltip refresh | Return no control over obsolete native HUD |
| `0x00459796` | Tooltip refresh | Return no control over obsolete native HUD |
| `0x00459825` | Status parent validation | Return no control over obsolete native HUD |
| `0x00459870` | Command-card mouse move | Return no control over obsolete native HUD |
| `0x004A5459` | Minimap-button refresh | Return no control over obsolete native HUD |
| `0x004A54BF` | Minimap-dialog mouse move | Return no control over obsolete native HUD |

The dedicated Game Menu context caller at `0x004F5142` is separately verified
to target `0x004F4F70`, which stable GPTP replaces with its own context
function. The earlier Game Menu hover-owner lookups at `0x004F509A` and
`0x004F511F` are verified to target `0x00418340`. The first owner path can
tail-jump directly to `0x004F4F70` at `0x004F50C1`. Both owner lookups therefore
filter their returned control using the exact presented button rectangle from
the live dialog and parent bounds. The direct context filter preserves its
dialog argument in `EDX` and otherwise chains to GPTP's live hook. The update
callback assignments at `0x004F50DB` and `0x004F51B3` are both verified to
target `0x004F4FB0`, then redirected through the same presented-rectangle hover
policy. Patching both is required because the mouse-event path restores the
second assignment after control creation.
All other control lookups, including the bottom-centered HUD and modal dialogs,
continue to use their original functions.

## Prohibited blind patches

- Do not patch a GPTP RVA without verifying the loaded instruction signature.
- Do not expand the fixed placement draw clip at `0x0048D5F2`.
- Do not globally replace `0x5993B0` with expanded dimensions.
- Do not install the locally rebuilt GPTP over the stable Release plugin.
- Do not treat observed ASLR pointers as stable addresses.
