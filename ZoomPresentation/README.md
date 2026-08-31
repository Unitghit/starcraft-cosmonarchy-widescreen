# Presentation scaling

This subsystem enlarges the completed logical framebuffer without changing any
game or compositor coordinate. Current configuration:

- logical framebuffer: 1280x720;
- rational presentation scale: 2.5 (5/2);
- client/output target: 3200x1800;
- raster filter: nearest-neighbor;
- integer boxing: disabled (required for fractional scale);
- mouse mapping: cnc-ddraw `adjmouse=true`.

Ownership is intentionally split:

- `ZoomSource/zoom_presentation.h` contains the exact numerator and denominator;
- `src/presentation.cpp` owns only client-window sizing and its log;
- `Starcraft/ddraw.ini` owns raster scaling and physical-to-logical mouse
  mapping;
- `set-presentation-scale.ps1` changes the isolated scale setting;
- `sync-presentation-runtime.ps1` derives and verifies the wrapper output;
- the canonical restart runs synchronization only after every previous game
  process has exited, preventing stale cnc-ddraw settings from overwriting the
  configured output.

The renderer's `zoom_resolution.h`, patches, buffers, HUD layout, camera,
selection, and input bounds remain 1280x720.

The 2x and exact 2.5x presentations were visually confirmed in-game on
2026-08-30. The portable configurator now writes runtime presentation settings;
this directory retains the source fallback and developer synchronizer.

The required separation and control model for a future settings GUI is defined
in `ZoomSystemMap\architecture\future-resolution-controls.md`.
