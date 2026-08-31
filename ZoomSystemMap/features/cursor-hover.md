# Gameplay hover cursor

## Symptom and success condition

Confirmed symptom at 1280x720: the cursor graphic can move throughout the
expanded battlefield, but ally/enemy/neutral/targeting hover states change only
inside the original 4:3 gameplay area. Native HUD hover behavior is already
correct and must remain native.

Success means gameplay cursor type is selected from the unit under the mouse
throughout the 1280x640 battlefield, while bottom HUD and popup hover behavior
remain unchanged.

## Ownership map

| Stage | Owner | Function / state | Coordinate space |
|---|---|---|---|
| Physical movement and UI translation | aidebug | `ConsoleWndProc` | physical -> native only for UI |
| Cursor type selection | stable GPTP | hook replacing `0x004D1460` | engine mouse / expanded battlefield |
| Gameplay bounds predicate | aidebug hook | replacement for `0x004D1140` | expanded battlefield |
| Second cursor-limit rectangle | stable GPTP | PtInRect operand in selector | incorrectly native 640x400 |
| Unit under point | stable GPTP/StarCraft helper | mouse + camera world point | world pixels |
| Cursor GRP selection | StarCraft caller near `0x004D14D0` | cursor type table | native cursor asset |
| Final raster | aidebug | `DrawExpandedCursor` | expanded physical output |

## Installed-binary evidence

The installed stable GPTP has one reference to StarCraft's shared native game
rectangle in its cursor selector:

```text
GPTP VA 0x100675B8 / RVA 0x675B8
  push dword ptr [0x6cddc8]   ; mouse y
  push dword ptr [0x6cddc4]   ; mouse x
  push 0x005993b0             ; native 4:3 rectangle
  call esi                    ; PtInRect
```

The selector has already called the expanded-aware outside-screen predicate,
but this second native PtInRect returns false outside the original rectangle
and forces cursor type 0 (normal). GPTP replaced the entire StarCraft selector,
so the existing StarCraft operand patch at `0x004D14AA` is bypassed.

## Hypothesis

Redirecting only the installed GPTP selector's rectangle operand to
`expanded_game_rect` will allow its existing `find_unit_at_point(mouse + camera)`
logic to select gameplay hover states over the whole battlefield. HUD behavior
will not change because button, transport, and minimap hover checks occur before
the gameplay rectangle test and receive native-translated UI coordinates.

## Required safeguards

- Patch module RVA, never an observed ASLR VA.
- Verify mouse-y push, mouse-x push, push-immediate opcode, and following call.
- Accept only native `0x005993B0` or the already-patched expanded pointer.
- Leave the shared native rectangle itself unchanged.
- Log cursor type and GRP transitions with raw engine mouse coordinates.
- Test normal, ally, enemy, neutral, targeting, drag, HUD, minimap, and popup
  states on both sides of x=639.

## Implemented result

`EnsureGptpCursorHoverBounds` in aidebug redirects the verified stable-GPTP
operand at RVA `0x675C5` to `expanded_game_rect`. The native rectangle at
`0x5993B0` is unchanged.

Runtime preflight succeeded and `fixed_zoom_cursor_hover.log` immediately
recorded non-normal gameplay cursor states outside the native area, including:

- type 6 at `(778,437)`, `(1085,553)`, and `(1058,605)`;
- type 8 at `(1055,496)`, `(1145,568)`, and `(1176,601)`.

The active cursor GRP changed with each type. This confirms the selector and
raster path now operate over expanded gameplay coordinates. Full visual/user
regression of every cursor type remains required.
