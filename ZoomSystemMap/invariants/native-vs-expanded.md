# Native versus expanded invariants

This list is the safety boundary for resolution work.

## Must remain native

- StarCraft private render canvas: 640x480.
- Nominal native game clip inside a pass: 640x400.
- Native placement draw clip at `0x0048D5F2`: 640x400.
- Native STrans surface formats and their pitch assumptions.
- Native dirty grid at `0x006CEFF8`.
- Native UI source and dialog coordinate space: 640x480.
- HUD artwork: 640 pixels wide and 166 pixels high from y=314.
- Placement Surface dimensions and pointers.
- GPTP's layer-5 wrapper function pointer.
- Stable GPTP QDP on disk.

## Must derive from configured output

- physical client width and height;
- presented surface and expanded buffer pitch;
- battlefield width and height;
- camera maximum for the expanded battlefield;
- tile row/column count and copy dimensions;
- bottom-centered HUD position;
- centered modal UI offset;
- gameplay hit-test bounds;
- visible-unit collection width and height;
- edge-scroll and mouse input maxima;
- drag cursor rectangle extent;
- stable-GPTP placement acceptance maxima;
- once-per-frame text, selection, and cursor clips.

## Per-pass transient state

These values may change only inside a private pass and must be restored:

- camera x/y;
- temporary camera-y maximum near map bottom;
- current canvas pointer;
- layer enable and refresh flags;
- placement layer `left/top`;
- recursive draw state;
- native redraw state.

After all passes, restore the expanded outer camera and rebuild visible-sprite
rows before gameplay input runs.

## Presentation rules

- Compose the expanded frame completely before presentation.
- Terrain and world sprites may be tiled.
- Screen-space text, selection rectangle, and cursor must be drawn once.
- HUD is relocated once and does not move when a popup opens.
- Only the popup rectangle is centered.
- A relocated translucent popup's source camera is offset by the same derived
  modal-UI origin; never relocate transparency that was composited against the
  obsolete native 4:3 background.
- Transparent HUD gaps remain battlefield pixels and gameplay input.

## Patch rules

- Every binary patch has an expected byte/value preflight.
- Module-relative addresses are used for ASLR modules.
- An unknown signature fails closed and is logged.
- No crash recovery step may overwrite user data or install experimental GPTP.
- The restart script is the canonical way to restore the known-good plugin set.
