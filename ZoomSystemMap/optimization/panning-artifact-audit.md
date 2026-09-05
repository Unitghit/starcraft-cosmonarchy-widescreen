# Panning artifact audit

## Scope and status

2026-09-04. Audit of subtle terrain artifacts during middle-button panning,
especially at 1920x1080, and possible residuals when motion stops.

The initial audit changed no production files. A subsequent user-authorized
implementation replaces the seam repair, described at the end of this file.
The offline characterization reproduces a defect in the pre-fix seam repair.
Its contribution to the user's exact live artifacts remains inferred until
a replacement is tested in-game. It does not establish that every reported
artifact has the same cause.

## Confirmed mechanism

Owner: `src/draw.cpp`, `RepairMovingFullWidthPassEdges`, called by
`AfterStockDrawScreen` after private world composition and before
`world_zoom::ScaleBattlefield`.

When middle pan is active and tile width is the full native 640 pixels, the
repair overwrites four columns before every internal column boundary with
camera-delta-translated pixels from `previous_world_frame`. The source is the
previous **repaired** world frame, not a terrain-only cache or fresh render.
It contains sprites, effects, fog, and map graphics as well as terrain.

The repaired output is copied back into history. With zero horizontal motion,
the repaired strips feed directly back into themselves. New world pixels in
those strips can be overwritten indefinitely, even when their prior source
has disappeared. Vertical camera translation does not break this feedback.
Small horizontal deltas can also carry previously repaired pixels forward.

This is a correctness issue, not a camera interpolation or floating-point
precision issue. Zoom scales these pixels afterward and can magnify it.

The routine stops borrowing history after both middle-pan owners become
inactive. In the isolated test, the first non-pan frame removes the history
artifact. Therefore persistent corruption after release would require
investigating the fresh native pass, gesture ownership, or another composition
stage too. The existing non-game branch invalidates history correctly.

## Offline reproduction

Run from the repository root:

```powershell
& .\ZoomIntegration\audit_panning_history.ps1
```

This compiles an offline harness containing the archived pre-fix function.
Before implementation, the runner verified it against `draw.cpp`; now it
verifies the frozen function fingerprint and uses the old tile geometry. It
does not load the game, hook a process, enable runtime diagnostics, or change
the renderer payload. Its successful exit means the current defect was
reproduced, not that rendering is correct. Do not use it as a release gate.

Test input replaces every fresh pixel with a new palette index each frame,
while holding the camera steady with middle pan active for 120 frames:

| Internal size | Pass grid | Tile size | Old pixels retained after 120 frames |
|---|---|---|---|
| 1280x720 | 2x3 | 640x240 | 2,880 |
| 1600x900 | 3x4 | 536x232 | 0 |
| 1920x1080 | 3x4 | 640x272 | 8,640 |
| 2560x1440 | 4x6 | 640x240 | 17,280 |
| 3840x2160 | 6x9 | 640x240 | 43,200 |

A separate case places one sprite-colored pixel at (638, 200), removes it
from every subsequent fresh render, and moves the camera vertically one pixel
per frame. Its ghost survives all 100 tested frames, translated with the map.

At 1080p, affected unzoomed source columns are 636 through 639 and 1276
through 1279. At 720p there is one internal seam; at 1080p there are two.
The trigger is full-width private columns, not resolution height alone.
1600x900 takes the overlapping-pass path and bypasses this history repair.
These counts describe synthetic overwritten pixels, not measured live visible
artifact counts.

## Preserve the existing fixes

- Native surfaces and pitch stay 640x480.
- Private world passes preserve exact horizontal camera positions after
  `MoveScreen`, then rebuild visible-sprite rows.
- Vertical source crops use the effective eight-pixel-aligned private camera.
- Outer camera restoration preserves Cosmonarchy's exact camera coordinates
  and restores camera-dependent bookkeeping.
- Matched UI/world comparison frames remain required during panning, at
  sub-quantum positions, and when the expanded camera was clamped at an edge.
- Native world passes replay only map-space GPTP graphics. UI comparison
  passes omit them so rally lines cannot become duplicate HUD pixels.
- Full-height side gutters, zoom, and native HUD composition remain separate.

## Recommended implementation scope

Replace temporal seam repair with same-frame composition, not a rewrite of
the input panner. The invariant is that all visible world pixels come from
the current render cycle at the intended world coordinates.

First inspect the native right-edge raster state that motivated the original
four-column workaround. A verified correction there could avoid extra passes.
Otherwise use a guarded overlap or current-frame seam pass whose crop lies
inside a known-good native region. Copying from the neighboring existing pass
is not automatically valid: full-width columns have no overlapping coverage.

Do not merely remove the repair, expand its historical strip, or add history
age heuristics. Those either reintroduce the original seam or retain stale
world-state pixels by design.

Measure pass count and frame time before choosing an overlap strategy.
Reducing the safe tile width indiscriminately can add an entire column of
native renders at common resolutions. Removing history should eventually
also remove its full-frame allocation and unconditional per-frame memcpy,
but that saving alone does not prove an overlapping renderer is faster.

## Acceptance tests for the replacement

1. Moving/disappearing sprites and effects update at every seam, including
   stationary camera with middle held and vertical-only movement.
2. Slow horizontal, vertical, and diagonal movement crosses all eight camera
   phases and 32-pixel terrain boundaries without stale pixels.
3. Release at each sub-quantum position, remain still, resume, and hit all map
   edges and corners without new terrain or HUD residuals.
4. Exercise 1280x720, 1600x900, 1920x1080, a custom uneven size, and 4K.
5. Cover native zoom, a non-integer zoom, zoom transitions, rally lines,
   placement ghosts, fog, overlapping sprites, popups, and all race HUDs.
6. Retain minimap dragging and cursor behavior with cnc-ddraw 6.9, 7.0, and
   7.1. This audit does not implicate a particular wrapper version.
7. Compare current-frame output and timings with the prior build. Keep any
   capture instrumentation out of playable release builds.

## Implemented replacement, awaiting live confirmation

The safe horizontal pass budget is now 624, with the native surface still
640x480. Balancing tile widths inside that budget guarantees eight pixels of
right margin on unclamped native passes. Internal joins no longer need the
native final four columns. Map-edge-clamped cameras remain aligned and retain
their native physical edge coverage, which needs separate live validation.

`resolution::PlanWorldPassX` owns horizontal source camera, crop, width, and
destination geometry for both composition paths. Camera movement/restoration,
vertical cropping, matched UI comparison, and zoom behavior are unchanged.
`RepairMovingFullWidthPassEdges` and its frame history are removed entirely.

The tradeoff is additional world passes at full-width-column resolutions:
720p goes from 6 to 9, 1080p from 12 to 16, and 4K from 54 to 63. 1600x900
remains at 12. The removed history allocation and copy are 2,073,600 bytes per
frame at 1080p and 8,294,400 bytes at 4K. Actual frame-time impact is not yet
measured. No unsupported native raster-state patch was added to avoid passes.

`verify_panning_native.ps1` compiles the production geometry directly and
checks 3,072,960 combinations covering every supported integer width, five
heights, all 32 horizontal tile phases, tight maps, large maps, and edges.
It also simulates 120 changing frames at seven sizes, poisons unsafe native
edge pixels, and checks panning, stopping, release, and boundary sequences.
These synthetic checks verify crop selection, not the real engine rasterizer.
The test rejects reintroduction of the removed temporal repair and is part of
the local release build gate. The Python geometry verifier now models exact
horizontal camera coordinates and the new guard budget too.
