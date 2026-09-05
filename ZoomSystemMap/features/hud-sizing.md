# Independent HUD sizing

## Wine/OpenGL 1x-output follow-up, 2026-09-04

The user reports correct gameplay behavior but a scrunched HUD in the Ubuntu
Wine/cnc-ddraw 7.1 OpenGL test. This uses 1920x1080 output, unlike the prior
3840x2160 Windows acceptance: the 720p HUD reference is therefore 1.5x native
pixels, not 3x. Fractional nearest sampling necessarily has unequal pixel
widths; the world-only smoothing filter does not filter HUD art.

Re-ran the existing production-adapter `--gl-only` tests in the active Wine
prefix on VcXsrv display :7, Mesa 4.5 software OpenGL. Exact native UI/world
comparisons passed at 4K, 1080p and 1365x767, as did unchanged HUD under world
filtering and high-refresh pointer tests. This supports existing OpenGL
sampling parity, but is not proof against a live-game fallback. No renderer
change was made. Screenshot and setup notes: `C:\Cosmonarchy\WineTest\README.md`
and `XServer/hud-linux-current.png` beneath that directory.

Follow-up user acceptance: changed only the test output to 2560x1440 (and
enlarged its desktop), retaining 1080p internal and 720p HUD reference. The
user confirmed HUD scaling is now fine at this exact 2x native-pixel scale.
No OpenGL renderer fix was needed for this reproduction. This is Wine/Mesa
acceptance for the tested cnc-ddraw 7.1 setup, not macOS or all-driver coverage.

Status: sizing and input user-confirmed. Native final-output sampling implemented;
offline Windows/Wine pixel tests pass. Initial runtime integration rejected the
real cnc-ddraw palette shape; corrected build awaiting gameplay acceptance.

## Runtime integration failure and corrected test

Second live rejection: selected-unit portrait animation changed 2,328 pixels
between the zero/full coverage passes, all within x=416..470/y=415..460.
No mismatch occurred above y=400. The renderer already treats the whole
native y=400..479 console band as opaque, but the global coverage preflight
was incorrectly rejecting animation differences there. `NativeUiNeedsFallback`
now applies the background-dependence test only above that known-opaque band.
Actual portrait pixels still come from the normal native UI frame, not from
the probes. All 65,536 palette pairs are tested at y=399/400/415: animation
inside opaque HUD is accepted, upper-region blending fallback is unchanged.

User reported continued scrunching after native-plane implementation. External
read-only capture of the installed 7.1 process showed valid published frames
and correct 960x720 HUD geometry, but both adapter upload serials stayed zero
and the D3D9 adapter device owner was null. No diagnostics were injected.

The D3D9 guard required a 256x1 wrapper palette texture. cnc-ddraw 7.1
`render_d3d9.c` allocates 256x256 palette textures and uploads the first row.
Our private palette is 256x1, which had incorrectly been used as the fixture
for both sides. The live frame therefore fell back before resource creation.

The corrected offline fixture uses cnc-ddraw's pure hardware/multithreaded
device flags and a 256x256 X8R8G8B8 wrapper palette. It fails with the old
guard and passes with the guard accepting 256x256 or 256x1. Exact world/HUD
pixels, state restoration and fallback pass on Windows and Wine.

Capture: `C:\Cosmonarchy\ZoomIntegration\diagnostics\hud-live-20260904\pipeline.txt`.
External inspector: `C:\Cosmonarchy\ZoomIntegration\inspect_hud_pipeline.cpp`.
Requires an elevated token with debug privilege for the protected game process;
opens it with read/query rights only and uses matching renderer PDB symbols.

## Fractional scaling correction

Reproduction: 1920x1080 internal, 720p HUD reference, 3840x2160 output.
The native HUD is first resampled 1.5x into the logical frame, then 2x by
the wrapper. Native columns become alternating 2/4-pixel blocks, not uniform
3-pixel blocks. Pixel-centered sampling alone cannot undo that first rounding.

Correction boundary: preserve four native indexed/coverage UI planes
(resources, HUD, objectives, context help) through `single_stage::Frame`.
The D3D9/OpenGL/GDI presenters sample each plane directly at final output
pixel centers using its unchanged logical presentation rectangle. The regular
indexed frame remains the fallback; cursor and selection coverage stays on
top. No input coordinate or native dialog changes are needed. Enable this
path for resized HUDs even with gameplay zoom disabled.

Verify uniform 3x source pixels, all layer anchors, opaque black, transparent
holes, upper overlays, fractional output dimensions and GPU state restoration.
Noninteger final scale still cannot give every source pixel identical width.

Implemented in the Pixel-perfect backend, including HUD-only use with zoom
disabled. Standard, unsupported adapters, and paused modal-dialog fallback
still use the combined indexed frame. Those fallback paths retain two-stage
HUD scaling; do not describe them as pixel-perfect.

`verify_single_stage.ps1` reproduces 320 wrong columns in the old two-stage
1920-wide HUD output and verifies uniform 3x3 native cells in the new path.
The production GDI compositor, D3D9 shader and hook, and OpenGL presenters
match the CPU reference including overlapping native planes, transparent
holes, and opaque black overlays. Windows NVIDIA and Wine Mesa pass;
Wine GLSL 1.20/GL 2.1 and core-profile paths also pass. macOS is untested.

## Contract

### Confirmed device-discovery failure, 2026-09-04

Read-only live capture `hud-live-vtables/pipeline.txt` showed valid native UI
frames but zero uploaded serials. cnc-ddraw 7.1's actual device used vtable
`0F1A74DC`; the dummy devices patched by Configure used `0F53493C` and
`0F52DE9C`. They were distinct instance-private tables. Therefore the live
presenter never received those frames, despite the direct-attach GPU test
passing. These are capture-specific addresses, not patch constants.

Replace dummy-device discovery with a read-only signature lookup of the
wrapper's actual device global. Verify the entire DrawPrimitive/EndScene
sequence, matching both references to a writable in-module pointer. Scan
once, refuse ambiguous matches, and attach the existing vtable hooks to that
device. Do not patch wrapper code or files. Verify 6.9, 7.0 and 7.1 binaries,
separate device tables, missing/ambiguous signatures, and live uploaded serials.

Offline verification: the production locator resolves the archived 6.9 and
7.0 device global at RVA `59544` and 7.1 at RVA `5E8AC`; these are evidence,
not hardcoded offsets. Windows NVIDIA and Wine Mesa tests pass private-table
discovery, exact native HUD output, state restoration and reset cleanup.
User verified the rebuilt renderer in-game: the resized HUD is now properly
pixelated at 1920x1080 internal, 720p HUD reference and 3840x2160 output.
Verified renderer SHA-256:
`BEC2BCCDD4063406CBF26C42A0F32B75F406BA5CDB98EF58D8C5225424FFB475`.
Live GPU upload counters were not captured after this restart; user visual
confirmation is the in-game evidence. Paused modal/Standard fallback limits
above remain separate and are not covered by this confirmation.

HUD size uses a reference resolution height. At internal 1920x1080,
the 720p reference draws native HUD pixels at 1.5x. Match internal preserves
the previous 1x native HUD. The setting affects the bottom console, minimap,
context help, resources and objectives. Top text can use a separate reference.
Neither gameplay zoom nor external window sizing changes.

## Owners and implementation boundary

- `src/ui_scale.h`: shared, precomputed native-to-presented geometry.
- `src/mainpatch.cpp`: reads sizing after validating internal resolution.
- `src/draw.cpp`: composites native console pixels, protrusions, context help
  and top text through that geometry. Cursor and modal menus remain unchanged.
- `src/scconsole.cpp`: inverse maps the actual HUD pointer for dispatch,
  minimap dragging, menu highlight and tooltip lookup. Hidden legacy hit tests
  remain native. Cursor offsets are physical minus mapped native coordinates.
- `src/limits.cpp`: minimap drag clipping stays at the real client bounds.
- Configurator: linked size by default, optional independent top text size.

Do not resize native 640x480 surfaces, dialog rectangles or minimap storage.
Do not change the gameplay camera dimensions or native resolution constants.
Use the same pixel-centered nearest-neighbor mapping for rendering and input.
Precompute geometry only at configuration time; no per-frame allocations for it.

## Verification required

Geometry tests: legacy identity, 480/720/1080/1440/2160 references, custom
internal dimensions, width fitting, round trips, edge ownership and clipping.
Configurator: absent keys preserve old size, linked and separate round trips.
Gameplay: all race consoles, tooltip stability, minimap click/hold/drag,
menu highlight, drag selection released over HUD, placement, zoom transitions,
top layout anchors, standard and pixel-perfect rendering, paused menus.
Keep runtime diagnostics disabled. Do not publish until user regression passes.

## Verified offline

- `verify_ui_scale.ps1`: 2,240 production geometry configurations, every
  presented sample maps to the source pixel that rendered it, including
  fractional scales, negative/outside coordinates and narrow width fitting.
- Configurator tests: missing and invalid references preserve current size;
  linked/separate choices and custom references serialize and parse correctly.
- Existing zoom, 3,072,960 panning geometry cases, D3D9/OpenGL/GDI backend
  pixel composition and release diagnostic exclusion checks pass.

## Implementation details

`[ui_size]` stores `hud_reference_height`, `separate_top_text`, and
`top_reference_height`. Zero means match internal. References outside
480..2160 fall back to match internal. A stored separate top reference is
retained but ignored while linked. It can be re-enabled without losing it.

`Geometry` precomputes source-cell destination boundaries using the inverse
pixel-center rule. Opaque HUD cells paint only those boundaries and mark the
same fallback coverage. During native-UI capture they instead populate one
of four 640x480 indexed/coverage planes, before logical downscaling can drop
source pixels. Final-output presenters sample that atlas directly. Cursor
and selection retain the final logical coverage above all four planes.
Native game text and context help are prepared
on native scratch canvases, with coverage determined from two backgrounds.
The actual raster pass samples the presented backdrop for blended ink.
Cursor and paused dialogs retain their existing sizes and coordinate paths.

The unchanged-size console retains its full-row copy path. Scratch canvases
are static, geometry has no frame-time allocation, and scaled HUDs request
the complete world strip behind the console so a smaller HUD exposes terrain.
Fractional HUD scaling uses nearest-neighbor sampling, not new font/art assets.
The atlas is double-buffered with the existing frame lock and serial. GDI
precomputes horizontal samples instead of doing per-pixel 64-bit division.
