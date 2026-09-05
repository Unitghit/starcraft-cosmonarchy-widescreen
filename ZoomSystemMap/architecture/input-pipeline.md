# Input pipeline

The input system preserves physical expanded coordinates over the battlefield
and performs targeted translation only for relocated native UI. When optional
gameplay zoom is enabled, battlefield points alone receive an additional
presented-to-source transform.

## Event flow

```text
Windows mouse message
  -> ConsoleWndProc records raw physical point
  -> classify battlefield / relocated HUD / popup / obsolete native HUD
  -> translate native UI or inverse-transform an enabled world zoom
  -> StarCraft dialog dispatch
  -> gameplay click callback correction when a decoy point was required
  -> restore engine mouse globals to the real physical point
  -> trace raw, forwarded, and engine coordinates
```

## Coordinate ownership

### Battlefield

Points inside the output are forwarded one-to-one unless the relocated HUD
owns them or optional gameplay zoom is enabled. With zoom enabled,
`world_zoom::PresentedToSource` maps the visible point into the centered world
crop before StarCraft performs hover, selection, order, or placement logic.
The prepared cursor receives the inverse offset so it remains at the physical
pointer. Selection, placement, cursor hover, drag clipping, and
unit-collection bounds use the full output height so the visible side gutters
remain gameplay-active. The centered HUD still intercepts its complete bottom
row before any gameplay callback.

When a left-button sequence begins on the battlefield, the window procedure
keeps battlefield ownership until release. Crossing onto the presented HUD
during that sequence does not translate the move or release into native HUD
coordinates. This lets StarCraft finalize a battlefield selection rectangle
when the pointer is released over the bottom HUD. A sequence that begins on
the HUD retains native HUD ownership, including minimap capture.

The first and last physical battlefield pixels retain StarCraft's original
edge-scroll coordinates. The zoomed crop slides toward map boundaries, so
click and hover transforms remain aligned with the terrain visible there.

Stable GPTP replaces StarCraft's same-type selection routine and builds its
own visible-unit rectangle for both modifier branches. The four verified
640x400 immediates are replaced at runtime with `game_width` and
`screen_height`, so same-type selection sees units throughout the configured
viewport. The stable GPTP file on disk is not changed.

### Bottom-centered HUD

Optional HUD sizing uses the precomputed `ui_scale::hud` presentation geometry.
All presented HUD event and tooltip/menu lookups inverse-map through it.
Cursor correction is the actual physical point minus forwarded native point,
not a fixed translation. Native masks, dialog bounds, minimap storage and
hidden legacy-area suppression stay native. See [HUD sizing](../features/hud-sizing.md).

The native STrans mask decides whether a pixel belongs to solid HUD artwork or
to a transparent gap showing terrain. Solid pixels are translated into native
HUD coordinates. Transparent pixels remain battlefield input.

The translated point is staged into both the forwarded `DialogEvent`/window
message and StarCraft's shared `g_mouse` globals for the duration of native HUD
dispatch. This is required by Cosmonarchy's `minimap_game_mouse_update`, which
reads `g_mouse` directly instead of the event point. The globals return to the
physical expanded point immediately after dispatch for ordinary HUD controls.
When a left press begins inside the runtime minimap rectangle at `0x00512D00`,
the native point persists through the full captured drag because the minimap
polls between window messages. During that interval the cursor compositor adds
the derived HUD offset, keeping the visible pointer at its physical location.
Release, cancel mode, focus loss, or capture loss after the physical button is
up restores physical ownership. A capture-change notification while the button
is still held does not terminate minimap ownership.

Frame capture showed that minimap polling can alternate cursor layer 0 between
native and already-relocated rectangle coordinates during one captured press.
The compositor compares the prepared rectangle with both coordinate spaces and
applies the HUD offset only to the native form. It also retains the prepared
cursor while the native redraw flag is temporarily clear, because the expanded
frame is rebuilt rather than preserved like StarCraft's native framebuffer.
This keeps the pointer continuously visible while clicking or dragging.

### Popup dialog

While `popup_dialog_active` is nonzero, input is translated by the centered
native-UI offset. The HUD remains at its normal bottom position.

### Invisible native HUD copy

StarCraft's native dialog tree still exists at its old 640x480 location. A
gameplay-dispatch point can therefore collide with an invisible button or
minimap. For left/right clicks, `gameplay_input::RouteBattlefield` tests the
obsolete mask **after** zoom conversion, using the same source point forwarded
to the game. Visible HUD ownership remains a separate presentation-space test.
The window procedure detects solid obsolete-HUD pixels and sends a
temporary expanded-only decoy point through dialog hit testing. The wrapped
gameplay callback replaces the event point with the intended source point.
Zoomed bypasses retain their cursor transform instead of falling through the
native-UI cursor reset. Middle-pan initialization retains its previous bypass
policy because it does not use the left/right callback wrappers.
See the [zoom input-mask audit](../optimization/zoom-input-mask-audit.md).

## Cursor confinement and dragging

StarCraft has several independent coordinate gates:

- focus-time `ClipCursor` rectangle construction;
- imported `ClipCursor` calls during drag;
- WM_MOUSEMOVE and button-handler clamping;
- edge-scroll thresholds;
- gameplay `PtInRect` checks;
- visible-unit collection rectangles;
- a stored drag rectangle passed by `input_game_left_mouse_click`.

All semantic input gates are expanded. The native renderer-owned rectangle at
`0x5993B0` remains native; gameplay-only call sites are redirected to an
expanded copy. That copy is populated after the runtime profile is loaded,
not during C++ static initialization, so custom and high-resolution profiles
cannot inherit the universal build's default command bounds.

`PrepareExpandedDragClip` preserves the current rectangle origin and replaces
only its extent. `ClipCursorExpanded` widens a native game clip while a left
drag is active.

Relocated controls can also call `SetCursorPos` with the translated native
point. Both StarCraft and stable GPTP own independent imports; GPTP's custom
cursor updater calls its copy at module RVA `0x67F7E`. Those calls cannot be
repaired after dispatch because Windows has already queued a synthetic
`WM_MOUSEMOVE` in the obsolete 4:3 HUD. Both guarded imports are suppressed
while a translated UI event is inside the native window procedure. The guard
also remains active between messages for the entire captured minimap drag,
because GPTP polls minimap input outside window-message dispatch.

Native UI dispatch rebuilds cursor layer 0 from the forwarded 640x480 point.
After ordinary HUD or popup dispatch restores `g_mouse` to the physical
expanded point, aidebug calls StarCraft's verified cursor-layer refresh at
`0x004BE120`. This synchronizes the prepared layer area before the next
composed frame and prevents a one-frame cursor image at the obsolete 4:3
position. A captured minimap drag intentionally retains native `g_mouse` plus
its draw offset until release, then performs the same physical refresh.

GPTP can poll cursor type again between the translated window message and the
next frame. Final cursor composition temporarily prepares cursor type `0` and
its GRP whenever the live physical point belongs to the presented HUD and the
polled type is a gameplay cursor. It draws that temporary layer and restores
the original cursor state before returning to StarCraft. It never calls
`SetCursorSprite` at `0x004843F0`, because that function also clears the active
middle-pan callback at `0x005968AC`.
Visual cursor ownership is derived from the active race's live STrans mask.
A four-neighbor flood fill begins at every outer edge of the native HUD region.
Transparent pixels reached by that fill remain open terrain, while opaque art
and enclosed transparent panel interiors belong to the HUD. This follows all
race-specific HUD shapes without fixed component rectangles. Click ownership
continues to use the fine-grained STrans mask, and middle-button camera panning
and obsolete native-HUD bypass events never enter cursor substitution.

The local cnc-ddraw presentation shim owns an additional cursor clip and
position pair. At 2x external presentation scale, its legacy clip corner maps
to logical client point `(132,474)` and can synchronously clamp a relocated
minimap click there. The verified `ddraw.dll` imports are routed through the
same scoped guards; normal presentation input outside translated HUD dispatch
still forwards to USER32 unchanged.

cnc-ddraw 6.9, 7.0, and 7.1 place those imports at different RVAs. The
renderer parses the loaded x86 PE import directory, finds the named USER32
`ClipCursor` and `SetCursorPos` slots, verifies their current targets, and then
patches only those two slots. Missing names, malformed PE metadata, or
unexpected targets leave the wrapper untouched and mark the guard
incompatible.

Suppressing a legacy clip request is not enough on every host. Wine or a Linux
compositor may retain an older pointer constraint until it receives a new
screen-space rectangle. A translated minimap press therefore installs the
actual window client rectangle through the original USER32 import. Subsequent
legacy clip requests during that drag refresh the same physical rectangle, and
an explicit unlock is forwarded immediately. This path uses `GetClientRect`
and `ClientToScreen`, so it follows the real presentation window instead of
assuming internal resolution coordinates or a particular desktop origin.

## Middle-mouse camera panning

StarCraft's native middle-pan initializer at `0x00484520` and movement
callback at `0x00484460` convert cursor position to a percentage of the
scrollable map. Their range calculations subtract the native 640x400
battlefield from the map dimensions. That makes horizontal sensitivity and
stepping especially inaccurate after the logical battlefield is widened.

The initializer is replaced with a signature-checked compatibility function.
It derives both scrollable ranges from the configured `game_width` and
`game_height`, preserves the native cursor-layer cleanup, and installs a
matching movement callback. Both calculations clamp an axis to a zero range
when the configured viewport is at least as large as the map, avoiding native
unsigned underflow and division-by-zero cases. Native middle-button release
still clears the callback normally.

## Diagnostics

`fixed_zoom_input.log` records:

- raw physical point;
- point forwarded to StarCraft;
- engine mouse globals before and after dispatch;
- classified zone;
- expanded/native inclusion;
- popup and drag state;
- selection-layer state;
- maximum coordinates reached in the session;
- gameplay command-event correction.
- suppressed native cursor-warp targets during relocated HUD interaction.

The relevant implementation is in
`ZoomSource/Cosmonarchy-aidebug-resolution/src/scconsole.cpp` and
`src/limits.cpp`.
