# System overview

The working design preserves StarCraft's native renderer and composes a larger
view from multiple valid native camera passes.

```text
Windows / cnc-ddraw 1280x720 client
              |
              v
StarCraft DrawScreen (native 640x480 assumptions retained)
              |
              +--> stable GPTP layer-5 wrapper
              |      native world + Cosmonarchy map graphics
              |
              v
aidebug AfterStockDrawScreen
  six private native passes for a 1280x640 battlefield
  + one native outer frame for HUD/dialog state
              |
              +--> compose terrain, sprites, placement, rally graphics
              +--> relocate bottom-centered HUD
              +--> center modal popup only
              +--> draw text, selection box, and cursor once
              |
              v
present one 1280x720 indexed-8 frame
```

The input path runs in the opposite direction. Physical expanded coordinates
remain one-to-one over the battlefield. Coordinates are translated back to
native space only when they hit the relocated HUD or a centered modal dialog.

## Why multipass composition is required

StarCraft 1.16.1 and Cosmonarchy's plugins contain native buffer sizes, pitches,
dirty grids, STrans surfaces, and clipping behavior. Expanding all of them in
place caused missing UI, duplicated text, mangled pixels, flicker, and crashes.
The stable solution treats each 640-wide render as a valid camera tile and
copies safe portions into an expanded output surface.

For 1280x720:

- physical output: 1280x720;
- expanded battlefield: 1280x640;
- native frame: 640x480;
- safe private-pass copy height: 256;
- pass grid: 2 columns x 3 rows;
- derived tile size: 640x216;
- native HUD source begins at y=314 and remains 640x166.

## Principal source locations

- Resolution formulas: `ZoomSource/zoom_resolution.h` and
  `ZoomSource/Cosmonarchy-aidebug-resolution/src/resolution.h`
- Window setup: `src/mainpatch.cpp`
- Rendering and composition: `src/draw.cpp`
- StarCraft and GPTP bounds: `src/limits.cpp`
- Physical-to-native input routing: `src/scconsole.cpp`
- Addresses and hook sites: `src/offsets.h` and `src/offsets_hooks.h`

## Stability boundary

The installed stable GPTP QDP is a compatibility boundary. A locally rebuilt
GPTP loaded far enough to demonstrate code changes but produced data-file
errors or crashes against the current Release data. The working integration
therefore applies narrowly signature-checked runtime compatibility patches
from aidebug while reinstalling the known-good GPTP file on every restart.
