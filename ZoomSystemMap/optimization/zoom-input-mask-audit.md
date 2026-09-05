# Zoomed battlefield input and obsolete HUD hit tests

Date: 2026-09-05. Locally installed and user-verified; included in hotfix v0.5.1.

## Symptom and contract

At 1920x1080 internal resolution with a 720p-reference HUD, zooming and panning
can expose an invisible region that consumes battlefield order clicks.
Only the visibly presented HUD or an active popup may own UI input. An obsolete
native HUD collision must route to gameplay at the displayed world point.

## Ownership and hypothesis

- `scconsole.cpp::ConsoleWndProc` receives normalized presentation coordinates.
- `ExpandedHudConsumesInput` inverse-maps the independently sized visible HUD.
- `world_zoom::PresentedToSource` converts battlefield input to source pixels.
- The native dialog tree still uses 640x480 coordinates and the native STrans
  mask. `HiddenNativeHudConsumesInput` models that obsolete hit test.
- The existing hidden-HUD bypass tested the **presentation** point, then the
  normal zoom branch forwarded a different **source** point to the dialog tree.
- Prediction: a point outside the old HUD in presentation space can map inside
  it in source space, consume the click before the gameplay callback, and move
  its apparent blocked region as the crop changes.

The gameplay callback wrappers already restore the intended source point after
dialog dispatch uses a decoy. Preserve that mechanism and all native control
callbacks. No simulation or GPTP-on-disk changes are required.

## Evidence and verification

Source baseline: release v0.5.0, renderer SHA256
`687B809D92B3D8715AF9D43222FE452C3007495EAC6F927C2D42316AC9C6E2F0`.
The matching payload and PDB are available locally. Read-only live capture is
external to the game, under `artifacts/diagnostics/inspect_zoom_input.*`.

The 90-second capture recorded 350% zoom, crop `(158,315,548,308)` and a
presentation point `(181,542)`. The production inverse transform maps that
point to `(209,469)`, inside the native HUD band, while the old check rejects
it immediately because presentation y=542 exceeds 479. This establishes the
coordinate mismatch; read-only sampling does not prove which dialog callback
consumed each individual click. The user subsequently verified the correction
by repeatedly zooming, searching for the blocked regions and issuing orders.

## Local implementation and checks

- Added a small, allocation-free routing helper shared by production and tests.
- Left/right hidden-HUD checks and hover suppression use the source-space
  dispatch point, including the existing native edge-scroll exceptions.
- Actual HUD/minimap and popup routing remain presentation-owned and unchanged.
- The existing gameplay callback wrappers restore the source event after decoy
  dialog dispatch. Placement retains its existing shared-mouse correction.
- A zoomed bypass no longer falls through the native-UI cursor reset afterward.
- Middle-pan initialization retains its established bypass policy. It uses a
  different callback/anchor path and is not redesigned by this order-click fix.

Native tests pass over 1,329,132 points across 640x480 through 3840x2160 and
100% through 1200% zoom, with camera positions near both edges and the interior.
The synthetic solid native-panel predicate demonstrates 19,322 missed old hits
and 17,923 unnecessary old bypasses; these are test counts, not measured user
failures. The captured crop regression also passes. Production still queries
the original race-specific STrans mask, not that synthetic test rectangle.

Existing zoom/scaler/selection, 2,240 HUD geometry cases, fixed-resolution
geometry, stable-GPTP selection ABI, and OpenGL/D3D9/GDI presentation tests pass.
Runtime diagnostics remain compile-time disabled. Capture scripts and symbols
are local ignored artifacts only, not embedded in the configurator or renderer.

## Installed local test build

Renderer: `45EB910CB4E92F620444109ACE58F309C931658346D5C178BBAAD85F63FD4F83`.
Configurator: `F51B2B697E90D1DEB12D18E489FA1EF0F912AEC905496CD5C7D9C549A8DE4A73`.
Backup: `C:\Cosmonarchy\ZoomIntegration\backups\zoom-input-mask-20260905-001302`.
Installed through the configurator's saved-settings transaction. Viewport INI,
stable GPTP, cnc-ddraw 7.1 DLL and wrapper INI are byte-identical to baseline.
Cosmonarchy BW was relaunched after installation. The user reported that the
blocked regions could no longer be found after extensive zooming and repeated
order clicks. Separate placement/HUD/minimap/middle-pan checks were not expressly
confirmed in that report. These hashes identify the user-tested local candidate;
the versioned release is rebuilt from the same runtime source.

Hotfix cleanup check: no external zoom-input capture process remains running.
Runtime diagnostic gates are still disabled, and the binary cleanliness audit
passes for the installed payload. Read-only capture evidence remains in ignored
local artifacts for future research; none is included in the release build.
