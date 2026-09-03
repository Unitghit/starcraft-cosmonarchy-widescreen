# Changelog

## 0.4.7, cnc-ddraw compatibility hotfix

- Added compatibility for cnc-ddraw 7.0 and 7.1, including correct minimap dragging.
- Fixed gameplay cursor graphics appearing over the HUD and minimap.

## 0.4.6, control group camera hotfix

- Fixed control group camera centering.

## 0.4.5, replay color hotfix

- Fixed speckled extended player colors during replay playback.

## 0.4.4, camera and edge rendering hotfix

- Fixed minimap dragging, cursor blinking, and legacy 4:3 cursor snapping.
- Improved middle-mouse camera panning smoothness.
- Fixed distorted map graphics and HUD flickering at map edges.

## 0.4.3, selection hotfix

- Fixed Alt-click same-type selection outside the original 4:3 area.
- Fixed drag selection being canceled when released over the bottom HUD.

## 0.4.2, compatibility hotfix

- Fixed resolution-dependent horizontal rendering seams.
- Improved cnc-ddraw compatibility by preserving custom and later-edited
  `ddraw.ini` settings.
- Confirmed that the configurator never modifies `ddraw.dll`.

## 0.4.1, rally-point hotfix

- Restored accurate rally-point lines across the expanded battlefield.
- Fixed duplicate rally lines over the HUD during middle-mouse panning.

## 0.4.0

- Added live battlefield rendering beside the bottom HUD.
- Added centered 4:3 and screen-edge layouts for the top UI.
- Fixed several HUD tooltip, highlight, and interaction issues.
- Fixed unit orders outside the original 4:3 area at high resolutions.
- Fixed duplicated team-color graphics at 1080p and above.
- Optimized rendering for faster menus and improved performance.
- Changed the default presentation scale to 1x.

## 0.3.1, performance hotfix

- Removed runtime diagnostic tracing from release builds.
- Restored Cosmonarchy performance settings and unrestricted CPU affinity.

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
