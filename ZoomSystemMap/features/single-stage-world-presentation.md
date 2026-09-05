# Single-stage world presentation

## Status

User-tested locally and named `Pixel-perfect` under `Zoom rendering`.
It is the default renderer choice when gameplay zoom is enabled. A missing
backend selects `single_stage`; an explicitly saved `cnc_ddraw` remains Standard,
and unknown explicit values fall back to Standard. Zoom itself remains opt-in:
missing/disabled world_zoom prevents adapter hook installation unless smooth
world edges, a resized HUD/top text, or high-refresh pointer movement needs
the Pixel-perfect presenter.
Native-size HUD with zoom off and explicitly Sharp world edges retains the
original hook-free path when high-refresh pointer movement is disabled.
The GUI's external output section is now called `Window & display`.
Removing the Preview label does not constitute macOS or full wrapper validation.
Maximum world coverage stays user-controlled through internal resolution.

## Reproduction and hypothesis

### Paused-game presentation regression

User reproduction: opening the in-game pause dialog disables world filtering
and makes a resized HUD uneven again. Source-confirmed: `AfterStockDrawScreen`
excludes popup frames from `Begin`, then `PointerBackground(flat=true)`
replaces the layers with a fully opaque logical frame. Correction contract:
keep the raw world and native HUD planes while paused, isolate the stock
preblended popup in its own logical coverage rectangle, and only use flat
pointer fallback when layer capture really failed. Popup translucency is
background-dependent and must not invalidate unrelated world/HUD pixels.
GPU filtering beneath the preblended dialog itself is not reconstructed by
this scoped fix. Verify pause/resume with and without high-refresh pointer,
filtering, resized HUD, and an active zoom crop.

Offline verification: producer tests preserve the 1280x720 crop, edge filter,
native 720p-reference HUD planes and modal coverage in a 1920x1080 frame with
high-refresh pointer on and off. Rejected-frame fallback still works. Windows
OpenGL/D3D9 pixel, filter, pointer, state-restoration and reset tests pass.
User confirmed the pause/resume appearance fix on 2026-09-04.

### Optional high-refresh cursor and selection

Implementation contract: the engine still owns cursor animation, cursor type,
gesture state, selection completion and all clicks. Capture its prepared cursor
into a small indexed/coverage plane at game-frame frequency. Save the clean
UI/world before drawing the ordinary cursor/selection fallback. The final GPU
presenter samples the live OS pointer and draws the stored cursor and selection
edge on each wrapper refresh, reusing all other textures and frame serials.
No extra engine ticks, world rendering, input writes or independent render thread.

Source/binary evidence: DrawExpandedCursor and DrawExpandedSelectionBox are the
last in-game draw layers. 0x004BE120 builds a cursor frame from GRP 0x00597394
and frame counter 0x00597390. 0x004BE200 advances that counter on its normal
100 ms timer; never call it from presentation. 0x004BDFA0 subtracts its X/Y
arguments from cursor_layer_left/top and clips to current_canvas, allowing
unclipped capture in a bounded scratch canvas. 0x0046EEF0 compares event X/Y
against the signed 16-bit drag anchor at 0x0066FF4C/4E before sorting the box.
These anchor fields are read-only and pass through the existing zoom transform.

cnc-ddraw 6.9/7.x support minfps=-1 and maxfps=-1 to reuse the wrapper's
refresh-rate limiter without force-uploading the primary surface (-2 would).
Track both settings in the configurator's restoration ownership. Option stays
off by default and requires the Pixel-perfect presenter. Unknown/unsupported
capture and software/Standard paths retain the original baked pointer.
First scope: gameplay and paused in-game dialogs; front-end menus retain their
existing cursor path. Verify held-frame movement, unchanged animation bytes,
focus/edge clipping, minimap/drag release, middle pan, zoom, and Wine.

Implemented behind `[presentation] high_refresh_pointer=1` and the optional
`High-refresh cursor and selection` checkbox. The cursor uses the unused
128x128 tail at (0,1920) of the existing 1024x2048 native UI atlas. A direct
USER32 export read avoids the game-coordinate IAT shims. D3D9 takes the actual
quad origin as well as its size; GL converts the bottom-origin viewport into
client coordinates. Focus loss hides the pointer and selection; physical
button release hides the selection immediately. The engine still commits
selection through its unchanged input path. A paused dialog retains its
preblended rectangle as logical coverage while the world and native HUD stay
independent. Full-frame opaque pointer fallback is only used after rejection.

Offline verification on 2026-09-04: Windows NVIDIA D3D9/OpenGL and Ubuntu Wine
Mesa D3D9/OpenGL pass same-serial cursor movement, anchor/hotspot mapping,
focus/button release, clipping, palette-zero coverage and exact no-trail
comparisons. Legacy GL 2.1/GLSL 120 passes too. Tests assert that movement alone
does not upload another world/UI frame. Archived cnc-ddraw 6.9/7.0/7.1 device
discovery passes. These are adapter tests, not a completed live-game or macOS
acceptance claim. Tests and pointer injection are compiled out of release builds.

`--apply-saved` reapplies the saved GUI choices through the normal validated
ownership transaction without opening a window or launching the game. This
allows local build handoffs to preserve settings and record refresh overrides
correctly, instead of editing ddraw.ini outside the restoration system.

### Optional smooth pixel edges

Implement a world-only, single-pass boundary filter in the existing D3D9 and
GL final presenters. `[presentation] world_filter=sharp_edges` selects it;
missing values default to smooth edges. Explicit nearest and unknown values
retain nearest. Native UI and logical overlays bypass
filtering. Resolve palette indices to RGB before blending, never interpolate
indices. For enlargement, blend only the one-output-pixel footprint straddling
a source cell boundary. Exact integer axes use nearest, as does downscaling.
No additional render targets, uploads, frame history or CPU image conversion.
GDI/software fallback deliberately retains nearest to avoid CPU overhead.

Owners: GUI/ConfigurationService own the setting, Frame carries it coherently,
single_stage.hlsl and the portable GL shader own filtering. Test exact integer
identity, fractional edge coverage, no color overshoot, unchanged HUD, crop
clamping, RGB palette semantics, D3D9/GL parity and measured GPU cost.

Implemented as exact source-cell area coverage for enlargement: at most two
source cells per axis, blended only after palette lookup. Interior output
pixels and exact integer scales use nearest. This is not a broad blur,
edge detector, sharpening pass or sprite-reconstruction algorithm. The GUI
offers `World pixel edges: Sharp / Smooth pixel edges (Default)`; it requires
the Pixel-perfect backend, works with zoom disabled too, and persists its
choice independently from cnc-ddraw's external scaling filter.

Tests exposed derivative-based output-size errors and rounded-down exact
native UI quotients at 3200x1800. GL now passes actual viewport dimensions;
D3D9 reads the wrapper's 96-byte managed XYZRHW vertex buffer through normal
API calls. cnc-ddraw's SetViewport block is commented out, so the backbuffer
viewport cannot identify aspect-fit quad dimensions. No GPU framebuffer
readback is used in production. Quotient checks correct reciprocal rounding
at exact source-cell boundaries without an arbitrary epsilon.

Verification on 2026-09-04: Windows NVIDIA and Wine Mesa pass integer identity,
fractional coverage against an independent integer-overlap oracle (within one
RGB code value), exact unfiltered UI, downscale-nearest fallback, offset quad,
state restoration, reset, and zero warmed zoom-buffer growth. Wrapper device
discovery still passes archived 6.9/7.0/7.1 binaries. Cached-frame microbenchmark
at 4K, 1919x1079 crop, 40 warmed draws including hook/state/geometry overhead:
Windows 0.107 ms nearest / 0.103 ms filtered (noise); Wine 0.670 / 0.975 ms.
These are final-presentation measurements only, excluding new-frame uploads
and the engine renderer, not guarantees for other hardware. macOS untested.
Runtime diagnostics remain excluded. User verified the in-game appearance
and requested Smooth pixel edges as the default. GUI initialization, saved
settings parsing, model defaults and renderer missing-key behavior agree;
explicitly saved Sharp is preserved.

At a 1920x1080 internal viewport, a 150% zoom produces a 1280x720 source
crop. `world_zoom::ScaleBattlefield` samples this to 1920x1080. cnc-ddraw
then samples the combined world/UI image to a 3840x2160 output. The first
nearest-neighbor step produces alternating one/two-pixel runs; the second
step doubles them to two/four-pixel runs instead of uniform three-pixel runs.

By contrast, an unzoomed 1280x720 internal frame presented at 3840x2160
uses one 3x sampling step. A correct replacement must produce uniform 3x3
world pixels for the same source image and aligned output viewport. Changing
the first step's sample phase cannot recover this property while output still
consists of doubled 1080p pixels.

## Source-confirmed ownership

- `world_zoom.cpp::SourceExtent`, `RebuildLookup`, and `ScaleBattlefield`
  own crop dimensions and the first indexed nearest-neighbor resampling.
- `draw.cpp::AfterStockDrawScreen` subsequently composes HUD, popups, draw
  hooks, text, tooltips, selection outlines, and the software cursor.
- `draw.cpp::PresentExpandedFrame` copies one indexed logical frame into the
  Storm surface. It does not own the final output backbuffer.
- `presentation.cpp` controls window geometry and wrapper coordinate handling,
  not GPU drawing.
- cnc-ddraw owns the final display scaling and palette application.

The official cnc-ddraw 7.1 source at tag `v7.1.0.0`, commit
`541b5de218ec3fbd6ea91e606ebfadc07c1786b0`, was inspected locally in
`C:\Cosmonarchy\CncDdrawCompatibility\source-7.1`. Its
`src/render_d3d9.c::d3d9_render_main` uploads the primary indexed surface and
palette on a separate render thread, then draws a single textured quad.
`exports.def` provides no layer submission/presentation extension. Exported
`DDEnableZoom` only toggles its own zoom flag; it does not provide independent
world and HUD inputs. `DDGetProcAddress` forwards the system function lookup.

Reference: https://github.com/FunkyFr3sh/cnc-ddraw/tree/v7.1.0.0

## Required layer contract

The D3D9 wrapper palette guard accepts 256x256 storage (cnc-ddraw updates row
zero) as well as 256x1. The private palette upload remains 256x1. Regressions
must use the actual wrapper shape and pure-device creation flags, not only
our internal texture layout. A zero adapter upload serial with valid published
frames exposed this missing integration coverage during HUD sizing tests.

Resized HUDs now retain four native 640x480 indexed/coverage planes through
publication: resources, bottom HUD, objectives, context help. Each has a
logical presentation rectangle and is sampled directly at the final pixel
center. The logical cursor/selection overlay stays above them. See
[HUD sizing](hud-sizing.md) for evidence and fallback limits.

1. Retain the unscaled cropped world image before logical zoom resampling.
2. Maintain the existing logical zoomed image for input correspondence and the
   existing fallback, without treating it as the final world texture.
3. Carry explicit UI coverage with UI pixel data. A final-vs-world color
   comparison is not an alpha mask: an opaque UI pixel may equal the world
   palette index beneath it. This project previously fixed such failures.
4. Sample the original world crop once into the physical output viewport.
5. Sample UI in its existing logical coordinate space, independently of zoom.
6. Handle translucent dialogs using the correct final background. Flattened
   dialog colors already blended against the intermediate world cannot simply
   be reused as an independent translucent overlay.
7. Publish world, UI, coverage, crop, palette, and geometry as one coherent
   frame. The presentation thread must not mix generations.
8. Handle resize, device reset/loss, aspect bars, menus, and disabled zoom
   explicitly. Keep logical mouse coordinates unchanged.

## Backend decision

An isolated opt-in presentation backend with world/UI texture submission is
the clean ownership boundary. Existing cnc-ddraw presentation remains a
fallback. The backend must not silently replace the user's `ddraw.dll`, force
a different renderer through their configuration, or depend on guessed private
wrapper addresses. Merely hooking a shared D3D9 vtable does not establish
correct frame synchronization, UI coverage, or OpenGL/GDI compatibility.

The selected implementation adds process-local adapters, without modifying the
wrapper file. `single_stage.cpp` owns frame publication, palette acquisition,
and the Direct3D 9 adapter. `single_stage_portable.cpp` owns OpenGL and GDI
adapters. `single_stage_frame.h` is their shared indexed frame/sampling contract.

Direct3D 9 uses public device methods, with the draw caller restricted to the
loaded cnc-ddraw image and its indexed/palette quad. Resources are cached per
device, state is captured/restored, and reset/release discard owned resources.
OpenGL intercepts the wrapper's named SwapBuffers import, uses its current
viewport, restores modified state, and supports GLSL 120 and 130. GDI intercepts
the named StretchDIBits import and composes directly into a physical-sized RGB
buffer. Its sampling lookups avoid per-pixel divisions. Import availability
alone is not proof that a particular wrapper build uses that import at runtime.

## Producer and fallback contract

- `draw.cpp` retains the raw world crop before `ScaleBattlefield`.
- Alternating world vectors allocate for the full logical frame on first use.
  The valid crop stays packed at the front, delimited by world_width/height.
  Zoom-out does not grow allocations or initialize additional vector elements.
- HUD spans carry explicit ownership. Prepared text/cursor/tooltip and native
  UI draws are probed against two constant backgrounds to distinguish opaque
  ink, including index zero, from skipped pixels. Background-dependent effects
  reject the frame instead of guessing alpha.
- `SourceScreenHeight` includes the world behind the bottom HUD and gutters.
- `Submit` packs final logical UI pixels with coverage and obtains the current
  palette through Storm `SDrawGetObjects` ordinal 347 and the borrowed palette's
  `GetEntries`. It never releases that borrowed palette or reads guessed globals.
- World, UI, palette, geometry, and serial publish together under a mutex.
  The wrapper render thread never calls Storm or the game raster callbacks.
- Modal dialogs use the existing combined-frame path, or its opaque snapshot
  plus a separate cursor when high-refresh movement is enabled. Changed
  third-party draw-hook output normally falls back. Unsupported devices/formats or failed setup also fall
  back. The ordinary indexed frame is always submitted.
- This preview does not replace logical input mapping, force a backend in
  ddraw.ini, or bypass Wine with a native-only presentation API.

UI probes add draw work only while this option is active. Actual game frame
times and coverage still need live validation. This is not a release-ready
claim of identical performance or compatibility on every Wine/macOS setup.

## Offline evidence, 2026-09-04

`ZoomIntegration/verify_single_stage.ps1` builds an isolated x86 test executable,
including the production adapters and compiled shader. No test hooks are
compiled into the release renderer. Tests cover:

- Exact every-pixel comparison with the CPU reference, including opaque UI
  palette index zero over world data, direct uniform 3x, fractional sizes, and
  an offset OpenGL viewport.
- Windows NVIDIA: OpenGL compatibility and 3.2 core contexts, Direct3D 9 shader
  and actual hooked DrawPrimitive, state restoration, invalid-frame unchanged
  fallback, device reset, and production GDI composition.
- Ubuntu 24.04 Wine/Mesa: the same compatibility-context and D3D9/GDI tests.
  A separate GL 2.1 / GLSL 120 run passes the production legacy path.
  The test executable alone has Wine X11 `UseXRandR=N` to avoid a WSLg
  BadRROutput initialization error. Game Wine settings are unchanged.

These tests do not prove the game produces accepted coverage on every frame,
the GDI import is used by every wrapper, or macOS compatibility. Real-game
acceptance on cnc-ddraw 6.9/7.0/7.1 and a Mac remains separate.

## Local preview handoff

High-refresh pointer handoff (2026-09-04): renderer
`DE5ABAE52F95597156EE1C53DD7846A6B10D0617B66DCAC00225748C35AB2451`,
configurator `7C002ECAECE63DD2A9FD4D0FC3E04E61927604F8016C3ACC0AFFB6D5294286DA`.
Full build, disabled-diagnostics audit, native geometry/zoom/HUD tests, adapter
tests and installed system-map validation pass. Current 1080p internal / 4K
output and 720p HUD settings are preserved; high-refresh pointer is enabled
locally, with minfps/maxfps recorded by the normal ownership transaction.
Backup: `C:\Cosmonarchy\ZoomIntegration\backups\high-refresh-pointer-20260904-193916`.
GPTP and cnc-ddraw 7.1 are unchanged. Live gameplay acceptance is pending.
No release was published.

Latest naming/default update (2026-09-04): renderer
`A589C9E462845701EAB2D137E8A5B450AF47A5E4E8BAECB7F1D533E6774D6592`,
configurator `32232232FBFC0C143A7297010CA583E9F560D78BD2E7B9745DE9B746582642AB`.
GUI default/preserved-Standard tests, native zoom, graphics adapter tests, build
and disabled-diagnostics gate, and installed system-map validation pass.
Backup: `C:\Cosmonarchy\ZoomIntegration\backups\pixel-default-labels-20260904`.
Installed settings still explicitly select Pixel-perfect. GPTP/ddraw unchanged.
No release was published. Earlier preview checkpoint follows.

- Installed renderer SHA-256:
  `7595F7D15C2A5DECE3205FF11886E8009FCD0EC319C6ABF434378F78A7474E7E`.
- Installed configurator SHA-256:
  `35DBDA5C29541120D4FE2FC10FCD843E465733EA82A3EE0A09A15380FB54A0E1`.
- Working predecessor and settings backed up under
  `C:\Cosmonarchy\ZoomIntegration\backups\single-stage-preview-20260904`.
- Test configuration: 1920x1080 internal, instant 150% starting zoom,
  3840x2160 borderless output, nearest, centered top UI, cnc-ddraw 7.1.
- Full release build, compile-time diagnostics gate, configurator tests,
  geometry tests, and system-map validation pass. This is a local dev build;
  nothing was published. GPTP and the user's ddraw.dll hashes are unchanged.

## Maximum view and starting zoom

Keep maximum world coverage user-controlled. Internal resolution currently
determines full-world render work even when zoom crops most of it away.
Automatically selecting 4K increases native world passes, allocation, and
loading cost. A high-resolution physical output is not evidence that a machine
should render a 4K world view every frame.

Treat maximum view, starting view, and physical presentation size as three
distinct concepts. Later, render only the currently visible world crop plus
guard margins to reduce zoomed-in work. That optimization must preserve camera
limits, visibility lists, overlays, input, and minimap calculations, and is not
required to correct double sampling.

## Acceptance

- 1280x720 world crop to 3840x2160: identical 3x3 pixel blocks, no intermediate
  1920x1080 quantization; HUD remains at its configured logical scale.
- Same-color opaque UI pixels, thin text, cursor edges, tooltips, race HUD
  transparency, and translucent popups retain correct coverage.
- Fractional scales use one pixel-centered sampling step. Uniform pixel sizes
  at arbitrary fractional scales are mathematically impossible without bars,
  changed framing, or filtering.
- Validate matching frame generations, palette changes, resize/device reset,
  aspect-ratio preservation, minimap dragging, and pointer-to-world mapping.
- Measure frame-time/upload cost. Keep diagnostics out of release builds.
