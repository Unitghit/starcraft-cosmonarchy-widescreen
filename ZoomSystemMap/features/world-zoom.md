# Optional gameplay zoom

See the [2026-09-04 audit](../optimization/world-zoom-audit.md) for implemented
corrections, offline evidence, and remaining interactive checks.

## Scope

Gameplay zoom is an isolated, opt-in transform owned by `src/world_zoom.cpp`.
It does not change the configured internal resolution, output resolution,
presentation scale, HUD size, menu size, or top-text layout. A missing
`[world_zoom]` section and `enabled=0` both select the original renderer path.

Wheel levels normally run from 100 through 200 percent in 12.5 percent increments,
with additional exact-pixel stops when single-stage nearest presentation is enabled.
Choosing a closer starting view extends the upper limit to that starting zoom.
This allows a 640-wide view even with a 4K internal viewport (600%).
The GUI defaults to Disabled and exposes the feature in its own Gameplay zoom
group.
`Extra zoom-in range` multiplies that existing wheel limit by 1.5 or
2, without changing the saved starting view. `[world_zoom] extra_zoom_percent`
accepts 0 (Off), 50 or 100 (Default). Missing values default to 100; explicitly
saved Off is preserved and invalid values safely use Off. Mouse-wheel zoom
itself remains opt-in. Both
regular wheel stops and the exact-pixel planner use the same computed maximum.
At a normal 200% cap this gives 300% or 400%; a configured 600% starting view
can extend to 900% or 1200%. These are magnification multipliers, not percentage
points or additional internal rendering resolution. HUD/input/camera geometry
remain owned by the existing zoom transform. No wrapper-specific API changes.

Extra-range local handoff, 2026-09-04: production scaler/input tests now cover
105 configurations through 1200%, plus extended cap/anchor/reset/reverse tests,
invalid-value defaults, exact stops, 256 edge handoffs, HUD geometry and GUI
round trips. Diagnostic exclusion checks pass. Windows renderer:
`A1D086F4FAAE9103FC3FB19B80F43B1D9D4CC5F6D59E3569CFD6A532101F8940`.
Configurator: `C1121737846B51512A8019D402C6E4F975FDD9DF0E42D5F276EBA2399FFCD563`.
Backup: `C:\Cosmonarchy\ZoomIntegration\backups\extra-zoom-20260904-205126`.
Enabled extra_zoom_percent=100 locally and reapplied through the configurator;
this was the only saved setting change, and the wrapper INI is byte-identical.
The user confirmed the extra range works. The initial preview default was 0;
the requested follow-up defaults to 100% more, with Off retaining normal zoom.
No release published or Wine copy changed.

Naming/default follow-up: GUI initialization, model defaults, missing-key
parsing and runtime defaults now agree on 100% more; explicitly saved Off
remains 0. GUI/native default and existing zoom tests pass. Installed renderer
`6A8BB90733A61CC68BDC368E4F0C76356C5753CB86A616B3EEF9D7FD3B0CE38B`,
configurator `B22FEF57ADA2D93D3977722A30B6B5DA8322168740D95B391D535D822A35B6D2`.
Backup: `C:\Cosmonarchy\ZoomIntegration\backups\extra-zoom-default-20260904-205708`.
Saved viewport and wrapper INIs are byte-identical after normal reapplication.
Diagnostic exclusion and installed system-map checks pass. Windows game relaunched.
The GUI also offers Smooth/Instant transitions and a Starting view selector.
Recommended starting views are 1280 pixels wide and 640 pixels wide when the
internal width allows them. Their height follows the internal aspect ratio:
1280x720 and 640x360 at 16:9, or 1280x960 and 640x480 at 4:3. These labels refer
to the sampled full world strip, not a change to the renderer resolution or HUD.
Percentage choices and Full viewport remain available. Profile changes retain
the selected percentage, clamped if necessary, and refresh the dimension labels.

When enabled, wheel up steps inward and wheel down steps outward. Reaching 100
percent retains wheel ownership for the current session, so wheel up can
enable magnification again. Each match resets to the saved starting view.
Missing new settings retain Smooth transitions and a 100% starting view.

Each wheel step records the world pixel under the pointer and changes a target
level rather than the rendered scale immediately. The current scale follows
that target with a 120 millisecond integer quadratic ease-out in Smooth mode.
It starts moving promptly and decelerates to its endpoint, instead of a
zero-velocity slow start. See the [activation audit](../optimization/zoom-activation-audit.md).
Instant mode publishes the target in the next draw without interpolation,
while retaining the same anchoring and input-transform ownership. The crop is
recalculated throughout the transition so that world pixel remains beneath the
pointer. If the crop reaches the boundary of the previous unzoomed viewport,
the real camera absorbs only the remaining movement. At completion, the final
crop bias is transferred to the camera in one visually equivalent operation
and anchor ownership ends. This avoids both an invisible crop wall and the
stagger caused by driving StarCraft's integer camera every animation frame.
Crop dimensions, input conversion, cursor offset, lower world strip, and the
minimap outline all use the same animated state. Integer fixed-point state
avoids frame-to-frame floating-point drift.

## Render transform

`AfterStockDrawScreen` first composes the normal expanded world. Before any
HUD or screen-space layer is added, `ScaleBattlefield` copies a crop of the
complete world strip into scratch storage and expands it back to
`screen_width x screen_height` with nearest-neighbor sampling. This keeps the
world visible in the two lower side gutters aligned with the main battlefield.
Before the first wheel step, the crop is horizontally centered and vertically
anchored around the engine's configured tactical camera center rather than the
geometric center hidden by the HUD. Wheel zooming replaces that default with
the pointer-derived crop described above.

Nearest-neighbor lookup samples the center of each destination pixel rather
than its left or top edge. Fractional zoom ratios still require uneven source
pixel repetition, but the repetition pattern is distributed symmetrically and
does not accumulate a directional bias across the frame. Gameplay input uses
the same lookup tables as rendering.

Near a map boundary, the source crop slides from its centered position toward
the relevant edge. This keeps outermost terrain visible without requiring a
negative StarCraft camera origin. The 100 percent path does not copy or scale
the battlefield.

The following are added after the transform and stay at their configured size:

- bottom HUD and minimap;
- top objectives and resources;
- popup dialogs and front-end menus;
- tooltips and context help;
- cursor graphic and drag-selection border.

World-owned layers, including terrain, units, sprites, placement ghosts,
rally lines, and other GPTP map graphics, are scaled together. While zoom is
active, the compositor also renders the complete world strip behind the native
HUD before scaling. The HUD is added afterward, preventing its normally hidden
black backing area from entering the visible zoomed battlefield.

## Input transform

`ConsoleWndProc` applies the inverse crop transform only when battlefield
input owns the event. StarCraft receives the corresponding source-space point
for hover, selection, orders, and building placement. The cursor compositor
retains the physical-minus-source offset, so the cursor graphic stays under
the Windows pointer.

HUD, minimap, popup, and front-end menu translations retain their existing
coordinate spaces. A battlefield drag also retains battlefield ownership when
it crosses the HUD. The selection rectangle is converted from source space
back to presented space before its one-time final-frame draw.

The outermost physical pixels still map to StarCraft's edge-scroll triggers.
The minimap camera outline uses the zoomed visible width, height, and cropped
world origin. Its draw-only camera reads are redirected to shadow coordinates,
while minimap clicking and dragging retain the real engine camera. External
presentation scale remains excluded from those calculations.

Native `WM_MOUSEWHEEL` uses screen coordinates, but cnc-ddraw normalizes them
before forwarding. Wheel zoom now reuses the last normalized gameplay mouse
message, captured before HUD or zoom conversion. It does not interpret wheel
lParam or infer its space from the outer window procedure. The event is consumed only over exposed
battlefield pixels, with no active selection or middle-button gesture. HUD,
minimap, popup, and front-end wheel events pass through untouched.

## State and compatibility boundaries

- Wheel events capture the last completed transform and change only the target.
  They never invalidate its input lookup or advance the clock independently.
- The outer draw advances animation once. Camera correction recalculates the
  crop without advancing time. Final handoff solves camera plus edge crop
  together; overlapping edge regions use a continuous monotonic mapping.
- Lookup tables rebuild only when sample dimensions change, not crop origin.
- Stationary pointer synchronization also runs at 100% when zoom is enabled.
- The minimap origin shadow updates only on outer draws. The guarded GPTP
  minimap converter call at RVA `0x2C638` routes active zoom clicks through
  `CameraForMinimapPoint`. Native map-coordinate conversion and the original
  disabled/100% path are retained.
- Zoomed popup rendering seeds the native layer-5 background with the actual
  presented region, then uses the original layer-2 dialog compositor. No
  placement or map graphics are replayed over that already-composed region.
- Wheel anchors share the ordinary gameplay mouse-message position, including
  full expanded coordinates beyond the native 4:3 rectangle. Focus loss and
  front-end messages invalidate it; until a fresh gameplay mouse message arrives,
  wheel events pass through unchanged. No asynchronous OS polling or new hooks.

### Windowed anchor follow-up, 2026-09-04

User reported offset cursor-centered zoom at 1920x1080 internal, 2880x1620
windowed output. Read-only window inspection confirmed client origin (480,230).
Protected-process reads were denied, so the exact live subclass chain is unknown.
Source confirms the old helper can subtract that physical origin from an already
logical point when it cannot identify the outer WndProc as belonging to ddraw.
Fullscreen origin (0,0) masks such an error. This is the working hypothesis,
not a confirmed live trace. Remove that independent conversion boundary and
reuse the pre-transform gameplay pointer already used by hover/clicks. Keep
camera math, wrapper files and normalized move/button dispatch unchanged.
Offline verification passes the production anchor matrix in both axes at 720p,
1080p and 4K, in/out and instant/smooth, plus the existing 256 edge handoffs,
HUD geometry and configurator regressions. A source-boundary guard rejects
reintroducing wheel lParam/ScreenToClient as a second pointer source. This
does not reproduce or identify the protected live subclass chain.

Installed local renderer SHA-256:
`3F281655284AF838D3C5DAA966348DBD5805A0EE8C33952E37FBBFFB66608ECD`.
Configurator SHA-256:
`BE66A43FF123039A546DCB6C0B188C853F9035887734498C243300EF5F723FFA`.
Backups: `C:\Cosmonarchy\ZoomIntegration\backups\windowed-wheel-20260904-204415`.
Applied through `--apply-saved`, preserving windowed 2880x1620 output and the
other saved settings. Stable GPTP and cnc-ddraw 7.1 hashes are unchanged.
Diagnostic exclusion checks pass. Restarted the Windows game for user testing;
User subsequently confirmed windowed cursor-centered zoom works. The separate
Wine test copy was not changed.

## Configuration values

### Automatic zoom steps

The step selector and exact-only mode were removed at the user's request.
Zoom always merges supported exact stops into the 12.5% sequence, in both directions, without
duplicating existing stops or snapping a non-perfect starting view. For example,
1080p internal to 3200x1800 output adds 120% between 112.5% and 125%.
Without compatible presentation, regular mode retains its original sequence.
The exact-stop calculation requires the single-stage backend and
nearest filtering. Older `steps` INI values are ignored and no longer saved.

The isolated `pixel_zoom_steps.h` planner uses configured physical output
dimensions and cnc-ddraw's aspect-fit rounding. It finds integer pixel scales
dividing both output axes, then solves the intersection of both existing
integer crop formulas. Rounding a percentage alone is not sufficient. The
level list is built once at configuration load, not per pixel or frame.
Output changes made through settings take effect with the normal game restart. Manually changed
wrapper aspect overrides or OS/compositor scaling are not measured here.

For 1920x1080 internal and 3840x2160 output, exact stops are 100%, 150%,
and 200%, alongside the usual fractional increments. A 300% starting range also
permits exact 250% and 300%. The configured starting value is preserved and
sets the range. Full viewport
is always an escape stop, even if that output cannot display it at an integer
scale. With no valid exact magnified stop, the usual increments remain available.

Smooth/Instant remain separate controls. Smooth transitions can use fractional
scales between exact stops; use Instant to avoid those intermediate frames.
Single-stage fallback frames and external OS/compositor scaling retain the
limitations documented in [single-stage presentation](single-stage-world-presentation.md).
No changes to input, camera anchoring, HUD sizing, or minimap ownership.

Offline tests verify exactness and completeness of representable crops across
50 internal/output/aspect combinations, 4K recommended stops, range extension,
multi-notch/reverse wheel, and merged-list deduplication. Native tests also
verify advancing intermediate frames and completion at 120ms. Existing zoom
transition/input tests remain in the same executable. GUI tests verify that
the removed step option is absent from saved INI output.

### Earlier preview checkpoints

Local pixel-step preview installed on 2026-09-04 with steps enabled, 1080p
internal, 4K output, Instant transition, and a 150% starting view. Native zoom
tests pass on Windows and Ubuntu/Wine. Full build, diagnostics-disabled audit,
GUI tests, geometry checks, and system-map validation pass. Live acceptance is
pending. Backup: `C:\Cosmonarchy\ZoomIntegration\backups\pixel-perfect-steps-20260904`.
Renderer SHA-256: `C65BD1B31B9F93DFF436D396AD53D680D518FDE0F0A7B15663EBCBCA2E3EF665`.
Configurator SHA-256: `5E3B39F4FE67F2D014B03726B83EF5D6D6C0426886FB7A135F0EDEE46C62BC32`.
GPTP and cnc-ddraw are unchanged; no GitHub publication.

Follow-up on 2026-09-04: regular mode now merges exact stops automatically.
Forward/reverse union traversal, existing-stop deduplication, and preservation
of non-perfect starting views pass on Windows and Wine. Installed renderer:
`330D723679FD69DF68122F5AA39484648AD7DFA8F54582048063EB5DCB272D80`;
configurator: `9BDE933D62A56C6A0DC20D3A61F7F033FA34219D145EBC5AD4F11E1278CD7095`.
Local configuration switched to regular steps, retaining Instant and 150%
start. Backup: `C:\Cosmonarchy\ZoomIntegration\backups\merged-zoom-steps-20260904`.

Latest local build, 2026-09-04: removed the selector, old exact-only state and
unused GUI planner; merged steps are automatic. Smooth duration is 120ms,
with intermediate frames tested explicitly. Windows/Wine native tests, build,
diagnostics gate, GUI tests, and installed system-map validation pass. User
acceptance of the faster transition is pending. Current settings retain Smooth,
1080p internal, 4K output, and 150% starting view. Renderer:
`FB213F3A8F95AF5AD1791AEDEE92CAA69DB2DB3463CB44BCC47EBAF46D22C4C3`.
Configurator: `DFB172F1F65A4D0C450E914BC560E4A659C45BF07CEFBCB0B198D7F29B702287`.
Backup: `C:\Cosmonarchy\ZoomIntegration\backups\automatic-zoom-120ms-20260904`.
GPTP and ddraw.dll remain unchanged. Nothing published to GitHub.

`cosmonarchy_viewport.ini`:

```ini
[world_zoom]
enabled=0
percentage=100
transition=smooth
start_zoom_units=10000
```

Only the configurator should normally write these values. The percentage key
is retained as a base value for backward compatibility. `start_zoom_units`
uses 100 units per percentage point (12500 = 125%). It is clamped between 10000
and max(20000, internal_width * 10000 / 640). Runtime wheel targets stay in the
process. `BeginMatch` resets both target and displayed zoom without animating
the initial view, clears the anchor, and discards partial wheel deltas.

Native tests cover Smooth/Instant behavior, starting views, next-match resets,
non-grid starting ratios, and sampled buffers up to 600%. Configurator tests
cover recommended labels, 4:3/16:9/custom geometry, safe loading, and INI output.
