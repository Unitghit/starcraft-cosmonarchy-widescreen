# Rendering pipeline

## Outer frame

`PatchDraw` brackets StarCraft's `DrawScreen` at `0x0041E297` and
`0x0041E3FD`. The stock frame is allowed to complete into a private native
buffer, but its intermediate result is not presented. `AfterStockDrawScreen`
then builds and presents the expanded frame.

`BeginStockDrawScreen` suppresses the native cursor and context-help layer in
gameplay. Those screen-space artifacts are drawn once after composition so
they cannot be stamped into every private pass. A full native base redraw is
requested while context help is disabled, preventing a previous tooltip from
surviving in the persistent stock surface.

## Private world passes

For each derived row and column:

1. Calculate the desired expanded-world origin.
2. Add overlap for non-first tiles so sprites crossing a seam survive both
   neighboring passes.
3. Clamp the actual native camera to map limits.
4. Move StarCraft's camera to the private origin.
5. Rebuild visible-sprite rows with `0x004BD3A0`.
6. Force the native dirty grid and full-redraw flag.
7. Draw only world layers 3, 4, and 5 into a 640x480 private buffer.
8. Merge native STrans output so units, bullets, selection circles, and other
   sprites are included.
9. Copy the safe tile rectangle to the expanded battlefield.

Layer 5 retains GPTP's installed wrapper. Calling raw StarCraft world draw
`0x004BD580` removed Cosmonarchy's rally lines and other queued `ON_MAP`
graphics from private passes.

## Placement layers

Layers 3 and 4 contain the building-placement surfaces. The engine prepares
their screen positions relative to the outer camera. Before each private pass,
only `left` and `top` are shifted by `outer_camera - private_camera`, then
restored immediately.

The surface records at `0x64095C` and `0x640964` remain unchanged. Their first
two words are width and height, followed by the pixel pointer.

## UI composition

The outer native frame is compared with a game-only reference to isolate UI
pixels.

- The 640x166 HUD source at y=314..479 is copied to the bottom center.
- Modal popup bounds are read from the active dialog and only that rectangle
  is centered. The whole HUD is never moved when the popup opens.
- A centered translucent popup cannot reuse the already-composited native
  popup pixels: those pixels sampled world imagery at the original 4:3
  location. While a popup is active, `RunStockPopupPass` renders layer 2 over
  world layers 3/4/5 with the private camera shifted by
  `(native_ui_left, native_ui_top)`. A matching game-only pass at that camera
  isolates the dialog contribution before it is copied to the centered popup
  rectangle. StarCraft therefore performs the palette/STrans blend against
  the world pixels actually beneath the presented popup.
- Race console artwork in native y=293..313 is owned by the HUD even though it
  protrudes above the shared y=314 main-console boundary. It is relocated with
  the bottom-centered HUD. The x=0..22 corner piece is kept for Zerg and
  Protoss, but discarded for Terran where it is the unwanted rusty-pipe
  ornament. The decision uses the active local player's engine race state.
- Screen-space game text is suppressed in private passes and drawn once over
  the final output.
- Top-screen objectives and resources have two compile-time policies in
  `zoom_resolution.h`. `centered_native_box` places both at opposite sides of
  a horizontally centered 640-wide native box; `screen_edges` places
  objectives at x=0 and translates the native resource strip by
  `screen_width - native_width`. Both remain flush with y=0. The centered
  policy is currently active.
- Layer 1's complete context-help backing surface is drawn once at its live
  runtime bounds after HUD composition. HUD tooltips are offset by the derived
  HUD origin; popup help uses the centered native-UI origin. Tooltip pixels do
  not pass through world-color differencing or HUD-row clipping.
- The confirmed backing surface is 160x480 even though each prepared live
  tooltip rectangle is much smaller. When multiselection help clears native
  visibility on alternating frames, the renderer reuses the identical cached
  surface and live bounds while the status-control envelope remains active.
- The drag-selection rectangle is drawn once from the engine's inclusive box.
- The cursor layer is drawn once against an expanded `Surface` and clip.

## Presentation

Outside gameplay, StarCraft still renders one native 640x480 front-end frame.
`ScaleNativeMenuToOutput` enlarges it with nearest-neighbor sampling into the
largest centered 4:3 rectangle that fits the configured logical output. At
1280x720 this is 960x720 at `(160,0)`. `ConsoleWndProc` applies the inverse
rectangle transform to menu mouse events, and the native menu `ClipCursor`
request is relocated to the same rectangle.

The completed indexed-8 output is copied row-by-row to the physical locked
surface using the physical pitch returned by SDraw. The internal expanded
buffers use `screen_width` as their pitch.

## Invariants

- Private canvases remain 640x480.
- Native world draw clipping remains 640x400.
- Native STrans and dirty-grid formats remain native.
- Every private camera is restored to the outer expanded camera before input
  processing.
- `0x004BD3A0` is called again for the restored outer camera so hit-testing
  does not use the final private tile's visible-sprite rows.
- Screen-space artifacts are composed exactly once.

## Primary implementation

`ZoomSource/Cosmonarchy-aidebug-resolution/src/draw.cpp`:

- `BeginStockDrawScreen`
- `AfterStockDrawScreen`
- `RunStockGameOnlyPass`
- `RunStockPopupPass`
- `DrawExpandedGameText`
- `DrawExpandedContextHelp`
- `DrawExpandedSelectionBox`
- `DrawExpandedCursor`
- `PresentExpandedFrame`
