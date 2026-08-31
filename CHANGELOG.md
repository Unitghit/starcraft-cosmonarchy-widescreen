# Changelog

## 0.3.0, first release candidate

- Added one universal runtime-resolution renderer supporting internal sizes
  from 640x480 through 3840x2160.
- Added 16:9 presets through 2560x1440 and 3840x2160 (4K).
- Added custom internal width and height controls.
- Removed experimental labels from 16:9 presets; 4:3 remains experimental.
- Kept internal viewport and external presentation resolution independent.
- Added windowed, borderless fullscreen, exclusive fullscreen, display,
  nearest/smooth filter, fit, exact-output, and scale controls.
- Expanded world rendering, units, HUD, text, cursor/input, drag selection,
  tooltips, minimap interaction and viewport outline, placement ghosts,
  rally lines, positional audio, camera centering, portrait centering, and
  modal transparency to resolution-derived geometry.
- Added transactional installation, verified restoration, binary compatibility
  checks, and stable-GPTP protection.
- Fixed ghostly map bands during middle-mouse panning by pairing fresh UI and
  world passes for the duration of the gesture.
- Made Restore Original close the launcher/game first, verify the restored
  renderer and display settings, and remove generated widescreen state,
  backups, logs, and diagnostics.
- Simplified explanatory copy in the configurator.
- Added architecture/system-map documentation and automated geometry/source
  validation.

Known limitation: true 4K internal rendering is extremely expensive in the
single-threaded multipass StarCraft renderer and match loading can take several
minutes. Use 1920x1080 internal with 2x external scaling for a faster 4K-sized
presentation.
