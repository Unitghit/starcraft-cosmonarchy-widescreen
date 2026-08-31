# Input pipeline

The input system preserves physical expanded coordinates over the battlefield
and performs targeted translation only for relocated native UI.

## Event flow

```text
Windows mouse message
  -> ConsoleWndProc records raw physical point
  -> classify battlefield / relocated HUD / popup / obsolete native HUD
  -> translate only if native UI owns the pixel
  -> StarCraft dialog dispatch
  -> gameplay click callback correction when a decoy point was required
  -> restore engine mouse globals to the real physical point
  -> trace raw, forwarded, and engine coordinates
```

## Coordinate ownership

### Battlefield

Points inside the output are forwarded one-to-one unless the relocated HUD
owns them. Selection, placement, cursor hover, drag clipping, and
unit-collection bounds use the full output height so the visible side gutters
remain gameplay-active. The centered HUD still intercepts its complete bottom
row before any gameplay callback.

### Bottom-centered HUD

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
Release, capture loss, cancel mode, or focus loss restores physical ownership.

### Popup dialog

While `popup_dialog_active` is nonzero, input is translated by the centered
native-UI offset. The HUD remains at its normal bottom position.

### Invisible native HUD copy

StarCraft's native dialog tree still exists at its old 640x480 location. A
physical battlefield click can therefore collide with an invisible button or
minimap. The window procedure detects solid obsolete-HUD pixels and sends a
temporary expanded-only decoy point through dialog hit testing. The wrapped
gameplay callback replaces the event point with the original physical point.

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
`WM_MOUSEMOVE` in the obsolete 4:3 HUD. Both guarded imports are therefore
suppressed only while a translated HUD event is inside the native window
procedure. The entire captured left-button sequence, including down, minimap
camera moves, and release, uses this scope, so the physical cursor remains over the
presented minimap throughout a drag.

The local cnc-ddraw presentation shim owns an additional cursor clip and
position pair. At 2x external presentation scale, its legacy clip corner maps
to logical client point `(132,474)` and can synchronously clamp a relocated
minimap click there. The verified `ddraw.dll` imports are routed through the
same scoped guards; normal presentation input outside translated HUD dispatch
still forwards to USER32 unchanged.

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
