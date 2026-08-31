# Performance optimization audit

## Scope and checkpoint

This audit began from local development checkpoint `dd90143`, the
user-confirmed 1280x720 renderer with HUD side gutters and runtime-selectable
top text layout. Findings are separated by risk so performance work can
proceed one change at a time.

Evidence in this document is source-confirmed unless explicitly marked as a
live measurement. A live CPU trace has not yet been captured for this
checkpoint.

## Implemented optimization pass

The following source-confirmed optimizations are now present on the private
`development` branch:

1. Default-off debug overlays return before full-frame clearing and scans.
   Previously uninitialized overlay fields now start off.
2. The active compositor no longer performs its unused native-frame copy or a
   duplicate menu-frame clear.
3. Native menu scaling precomputes horizontal source coordinates, expands each
   distinct native row once, and copies repeated output rows.
4. An unchanged native menu frame reuses its already-scaled base. Any source,
   geometry, or gameplay transition invalidates that cache.
5. Mouse movement no longer queries the unchanged client rectangle. Window
   resize, position, activation, and paint messages retain correction.
6. Non-reference private terrain passes are cropped directly from the render
   buffer rather than copied through another complete 640x480 buffer.
7. Identical upper-UI rows are skipped, unconditional lower HUD rows use
   contiguous copies, and tightly pitched output uses one presentation copy.
8. Debug overlays safely handle custom widths that are not divisible by four.

`verify_menu_scaler.py` compares the optimized scaler against the original
pixel formula at standard, custom, 4:3, 16:9, and 4K sizes. It is part of the
system-map validator. The renderer also builds in Release, launches to the
front end, and passes the geometry, stable-GPTP, installed-payload, and
diagnostic-marker gates after every retained stage.

## Existing build optimization

The x86 Release build already uses maximum-speed optimization, intrinsics,
function-level linking, reference elimination, and identical COMDAT folding.
The diagnostic file API is a compile-time null implementation, and the shipped
QDP contains none of the diagnostic filename markers checked by the release
workflow. Compiler optimization is not the primary bottleneck.

The linked FreeType archive requests the debug static runtime (`LIBCMTD`) even
in a Release link. The linker keeps the renderer's release runtime and reports
the conflict. This dependency should eventually be replaced with a verified
Release FreeType build, but normal gameplay uses StarCraft's own text renderer;
FreeType is mainly relevant to the optional aidebug console and overlays.

## Structural render cost

Normal gameplay first executes StarCraft's outer native draw. The expanded
compositor then executes one complete recursive native draw for every private
tile. At 1280x720 the grid is 2 by 3, so a normal frame contains seven complete
StarCraft draw invocations: one outer draw plus six private draws.

| Internal resolution | Private draws | Total normal draws | Private draws if the confirmed 312-pixel safe height can be fully used |
|---:|---:|---:|---:|
| 640x480 | 2 | 3 | 2 |
| 800x600 | 6 | 7 | 4 |
| 960x540 | 4 | 5 | 4 |
| 1024x576 | 4 | 5 | 4 |
| 1280x720 | 6 | 7 | 6 |
| 1600x900 | 12 | 13 | 9 |
| 1920x1080 | 12 | 13 | 12 |
| 2560x1440 | 24 | 25 | 20 |
| 3840x2160 | 54 | 55 | 42 |

The 312-pixel column is a geometry result, not an approved implementation.
The current 256-pixel pass limit may encode an undocumented engine constraint
and must be validated before it changes.

Middle-mouse panning adds one fresh popup/UI draw while the gesture is active.
An open translucent game menu adds two more native draws, one clean-world
reference and one popup draw. These conditional passes fixed confirmed visual
defects and are not first-round removal candidates.

## Priority findings

### P0: establish a repeatable live baseline

Use external Windows Performance Recorder CPU sampling so profiling code never
enters the release payload. Capture the same replay or reproducible match state
for each comparison:

1. Static battle view at 1280x720 for 30 seconds.
2. Active battle at 1280x720 for 30 seconds.
3. Middle-mouse pan at 1280x720 for 15 seconds.
4. Open translucent menu at 1280x720 for 15 seconds.
5. Loading and active battle at 1920x1080.
6. Loading at 3840x2160 after lower-risk changes are proven.

Record load time, StarCraft CPU time, working set, and the hottest QDP and
StarCraft stacks. Use identical presentation settings and do not compare a 1x
run against a scaled or fullscreen run.

### P1 resolved: default-off debug overlay scanned every frame

`ScConsole::DrawDebugInfo` cleared two full-resolution buffers and then scanned
the game buffer and text buffer even when every visible debug overlay is off.
The default constructor enables no overlay that requires those buffers.
`DrawHook` nevertheless calls this function for every in-game frame.

Minimum pointless traffic before branch and function-call costs:

| Internal resolution | Approximate minimum traffic per frame |
|---:|---:|
| 1280x720 | 3.42 MiB |
| 1920x1080 | 7.76 MiB |
| 2560x1440 | 13.87 MiB |
| 3840x2160 | 31.35 MiB |

The implemented predicate includes every active overlay flag, grids,
information lines, region data, and fast-forward progress. When no visual
overlay is enabled, it preserves fast-forward state updates and returns before
allocating full-frame work.

The four-pixel scan now handles its remainder explicitly, so arbitrary custom
widths cannot cross into the next row.

### P1 resolved: loading and menu scaling repeated expensive work

`ScaleNativeMenuToOutput` performs an integer division for every destination
pixel. At a 3840x2160 internal output, the aspect-fitted menu is 2880x2160, or
6,220,800 divisions and writes per redraw. The active compositor also clears
the expanded frame before the menu branch, and the scaler clears it again.

StarCraft requests loading-screen draws after many GRP loads. The inactive
legacy `DrawScreen` path contains an explicit warning and one-draw guard for
this behavior, but the active begin/after compositor has no equivalent.

Implemented without freezing visible progress:

1. Remove the duplicate menu clear.
2. Precompute native x coordinates for the current menu width.
3. Expand each distinct native source row once and copy it for repeated output
   rows.
4. Cache the previous complete native menu frame. If it is unchanged, retain
   the already-scaled base frame instead of scaling it again.
5. Invalidate the cache on game/menu transitions and resolution setup.

This is expected to be the highest-impact loading-time improvement. Menu
animation, loading progress, and front-end input remain synchronized because
any native-frame change invalidates the cache.

### P1 resolved: redundant native-frame copy

The active `AfterStockDrawScreen` path copied `native_stock_frame` into
`native_present`, but the active compositor reads `native_stock_frame`
directly afterward. `native_present` belongs to the inactive alternate
renderer helpers and diagnostics. The active copy is now removed.

### P1 resolved: presentation size was checked on every mouse move

`ConsoleWndProc` called `presentation::EnsureClient` for every `WM_MOUSEMOVE`.
That reaches `GetClientRect` even when the window is stable. `WM_SIZE`,
`WM_WINDOWPOSCHANGED`, `WM_ACTIVATE`, and `WM_PAINT` already provide correction
opportunities. Mouse movement is now removed from this list while all four
window-state correction paths remain.

### P2: full expanded-frame copy before final overlays

Every frame copies the complete base buffer to `fake_screenbuf_2` before GPTP
graphics, draw hooks, text, tooltip, selection, and cursor composition. This is
0.88 MiB at 1280x720 and 7.91 MiB at 3840x2160 per frame. The base frame is
already rebuilt or cleared on every active path, so direct final composition
may be possible. The second buffer was historically needed for dirty native
drawing, so removal requires full visual regression, especially tooltip hold,
cursor background restoration, menu transitions, and map-edge gutters.

### P2 partially resolved: private-pass copies exceeded tile consumption

Each recursive private draw cleared a 640x480 frame and then copied the complete
640x480 frame to another native buffer. Most tiles consume at most 640x312.
Non-reference tiles are now cropped directly from `native_inner_frame` before
the next recursive pass. The clean-world reference and popup comparisons still
retain their complete frames because their later consumers require them.

### P2 partially resolved: UI extraction scanned large regions pixel by pixel

The active HUD compositor previously checked approximately 307,200 native
pixels per frame. It now skips identical upper rows and copies the always-owned
y=400..479 HUD rows contiguously when no popup overlaps them. The exact
transparency-mask path remains for the y=314..399 console overlap and
race-specific protrusions.

### P2: use the full confirmed native safe height

The renderer proves a native safe game height of 312 pixels but derives row
count from a conservative 256-pixel pass height. Deriving the minimum row count
from output height and the 312-pixel limit reduces private draws at 800x600,
1600x900, 2560x1440, and 3840x2160. It does not improve 1280x720 or 1920x1080.

This change directly alters camera crops and seam behavior. Test units,
bullets, fog, placement ghosts, rally lines, map edges, HUD gutters, and input
at every affected resolution before retaining it.

### P3: replace recursive stock draws or cache world layers

The dominant remaining cost is the native world renderer itself. Calling only
selected native layer functions, caching terrain independently from sprites,
or shifting a cached frame during camera motion could reduce complete engine
draws. Earlier development showed that seemingly equivalent direct layer
draws lost units, transparency, placement ghosts, and GPTP overlays. Treat this
as architectural research, not cleanup.

## Source cleanup with little runtime effect

The source still contains the inactive alternate `DrawScreen` pipeline and its
private helper chain. Release linking should discard unreferenced functions,
so deleting this code primarily reduces maintenance risk and audit surface.
Perform it only after the active path has dedicated tests and never mix it into
a performance-measurement commit.

Dormant logging functions also retain small first-call or periodic state logic
even though file creation compiles away. Compile-time guards around entire
diagnostic call sites can remove this residual control flow, but the expected
gain is small.

## Recommended implementation order

1. Capture the external baseline.
2. Add the default-off debug-overlay fast path and measure again.
3. Remove the unused native-frame copy and duplicate menu clear.
4. Add menu coordinate precomputation and unchanged-frame caching.
5. Remove or throttle the mouse-move client-size check.
6. Optimize private-pass copy regions.
7. Optimize UI row handling.
8. Test the 312-pixel pass-height experiment in its own reversible commit.
9. Consider single-buffer composition only after all lower-risk work passes.

Every item gets its own build and automatic relaunch before a private
checkpoint. Do not combine pass-count changes with pixel-composition changes.

## Acceptance gates

- Geometry verifier and system-map validator pass.
- Release QDP contains no diagnostic filename markers.
- Stable GPTP hash remains unchanged.
- Default visuals match the checkpoint at 1280x720.
- HUD, tooltip, cursor, minimap, building placement, rally line, popup,
  portrait centering, audio, and side-gutter checks pass.
- Menu and loading progress remain responsive.
- A change is retained only if the same external scenario shows a measurable
  improvement or the change removes proven redundant work with no regression.
