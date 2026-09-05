# Gameplay zoom audit

Date: 2026-09-04. Scope: local, uncommitted opt-in zoom implementation.
The initial audit changed no game files or processes. The implementation
follow-up below supersedes its originally failing results.

## Implementation follow-up

Implemented after approval on 2026-09-04:

- Retarget events preserve the completed input transform and capture anchors
  without a second clock advance.
- Final handoff solves the camera-plus-crop function, including overlapping
  edge regions; secondary crop updates do not advance animation.
- The guarded GPTP converter call now centers the visible crop. It undoes the
  native half-box subtraction, retains native minimap-to-world conversion,
  then solves for the base camera. Disabled/100% zoom delegates unchanged.
- Minimap origin shadows are no longer overwritten by private cameras.
- Popup layer 5 receives the presented background at native pitch, layer 2
  retains native blending, and redundant world/placement layers are disabled
  for this isolated popup pass. The 100% popup path is unchanged.
- Stationary pointer synchronization handles the return to 100%.
- Original wheel conversion recognized the outer ddraw window procedure.
  Superseded by the windowed follow-up in `features/world-zoom.md`: wheel
  anchors now reuse normalized gameplay mouse messages without a second
  coordinate conversion or a window-procedure ownership guess.
- Lookup tables are cached by dimensions rather than crop origin.

Verification: all four original failed contracts now pass. The actual C++
scaler matrix (63 combinations), 256 edge/direction/delayed-frame transition
cases, disabled identity test, existing geometry/menu/configurator tests,
GPTP binary patch guards, and diagnostic-free build checks pass.
`ZoomIntegration/verify_world_zoom_native.ps1` is now a release build gate.
No test code is linked into the renderer.

Interactive verification remains pending for popup blending, stationary hover,
minimap hold/release, and the complete zoom matrix on all three wrappers.
Offline compatibility preflights pass for 6.9, 7.0, and 7.1; no wrapper DLL or
INI is replaced by this fix.

Wheel forwarding was verified against local 6.9 source and upstream
[7.0 source](https://github.com/FunkyFr3sh/cnc-ddraw/blob/v7.0.0.0/src/wndproc.c)
and [7.1 source](https://github.com/FunkyFr3sh/cnc-ddraw/blob/v7.1.0.0/src/wndproc.c).

The sections below preserve the original failure evidence and rationale.

## Assessment

Keep the separate world-zoom module and the existing wrapper compatibility
layer. A rewrite is not justified. The indexed-pixel scaler and inverse input
lookup agree, use bounded buffers, and allocate no heap memory per frame.
The important work is state-transition correctness and integration with
consumers which still assume the base camera.

## Reproduced in production C++

`ZoomIntegration/audit_world_zoom.cpp` includes the actual `world_zoom.cpp`
translation unit. Only the clock is substituted; settings are seeded in the
test process. It never loads StarCraft or a plugin.

### High: wheel retarget invalidates the displayed input transform

Owner: `world_zoom::AdjustByWheel`, `UpdateAnimation`, `PresentedToSource`;
caller: `ConsoleWndProc` wheel handler.

Changing a target sets `lookup_valid=false` before the wheel handler's next
`PresentedToSource` call. That call returns an identity coordinate even though
the displayed world is still zoomed. At 1600x900 and 150%, pointer `(1200,200)`
maps to `(1066,250)` before the wheel event, then `(1200,200)` immediately after.

There is a second exposure: `AdjustByWheel` updates the clock again after
`ConsoleWndProc` has updated the transform. If the clock advances, animation
invalidates the lookup before the new anchor is captured. The one-millisecond
retarget test records the raw point rather than the displayed world point.

Proposed boundary: preserve the last complete transform for event mapping;
advance/publish animation geometry coherently. Capture anchors from that
complete transform. Add a regression gate before changing the implementation.

### Medium: final anchor handoff assumes an unchanged edge crop

Owner: `ResolveCameraAnchor` and `EdgeAwareCrop`.

At transition completion, the handoff computes a default crop using the old
camera. The caller then moves the camera and rebuilds that camera-dependent
crop. This is not always visually equivalent near an edge.

Reproduction: 1600x900, camera x=50, pointer x=1200, one inward wheel step,
followed by a 180 ms delayed frame. The anchored world x changes from 1250 to
1289, although the target is reachable. The test executes the same
update/resolve/update sequence as `BeginStockDrawScreen`. This proves the
delayed-frame case, not that every ordinary transition jumps.

Proposed boundary: solve for camera plus the crop at the resulting camera,
then publish both together. Test all edges, frame delays, and direction
reversals before deployment.

## Integration findings

### High-confidence dataflow issue: minimap click center versus draw center

`EnsureGptpMinimapViewportBox` writes zoom-dependent width/height to GPTP
globals shared by outline drawing and minimap input. The stable native input
routine at RVA `0x2C5F0` subtracts half those dimensions before converting the
minimap point to a base-camera destination. The new outline-origin patch only
redirects draw reads, adding `SourceLeft/Top`; input has no corresponding crop
correction.

The audit's integration model combines that native input calculation with the
actual C++ crop transform. At 1600x900, 150%, divisor 32, clicking world x=4096
places the presented center at x=4352, a 256-world-pixel difference, well above
minimap rounding. This test models GPTP input; it does not execute that input
routine in-game. Confirm by clicking a visible landmark while zoomed before
patching. Do not conflate this with the already-fixed outline size/origin bug
or with the cnc-ddraw pointer-confinement bug.

Likely solution: separate draw dimensions from input camera-centering semantics,
or explicitly solve the visible-camera destination at the input boundary.

### Static mismatch: popup transparency samples the unzoomed world

The popup branch in `AfterStockDrawScreen` renders its background with
`camera + native_ui_offset`, at native scale, then overlays those already
blended pixels on the zoomed battlefield. The displayed background instead
uses `camera + crop + sampled offset`. A translation alone cannot align both
scales across the entire popup. Keep the dialog dimensions native but perform
its background blending against the final zoomed world. Capture this first;
no live visual confirmation was performed in this audit.

### Follow-up checks, not claimed as reproduced gameplay defects

- `SynchronizeWorldZoomPointer` exits when `Active()` becomes false. Verify
  that completing a zoom-out at 100% with a stationary pointer clears the old
  source coordinate and cursor offset without requiring a mouse movement.
- `EnsureGptpMinimapViewportBox` also runs on private recursive render passes.
  Verify the draw-only origin shadows are correct at each consuming draw, not
  merely correct at outer-frame entry.
- Wheel coordinates need an explicit wrapper contract. The local cnc-ddraw
  6.9 source converts `WM_MOUSEWHEEL` from screen coordinates to logical client
  coordinates in `wndproc.c`; its hooked `ScreenToClient` is a no-op for the
  game window. Our handler calls `ScreenToClient` again. Check its actual import
  target and dispatch order on all wrappers, especially an offset window and
  non-1x scaling. The current documentation's unconditional claim that the
  handler receives screen coordinates is insufficient.

## cnc-ddraw compatibility evidence

`ZoomIntegration/audit_cnc_ddraw.py` reads official local DLLs without loading
or executing them. All three hashes match the GUI compatibility manifest and
all contain the x86 named imports used by the cursor guard.

| Version | ClipCursor IAT RVA | SetCursorPos IAT RVA | ScreenToClient IAT RVA |
|---|---|---|---|
| 6.9 | 0x3C330 | 0x3C34C | 0x3C2EC |
| 7.0 | 0x3C32C | 0x3C348 | 0x3C2E8 |
| 7.1 | 0x3E35C | 0x3E380 | 0x3E310 |

The existing production code discovers the first two slots by name, checks
their current targets, and rolls back the first patch if the second fails.
Do not introduce new version-specific fixed addresses. Hash/import validation
does not prove the runtime targets or complete zoom behavior on each wrapper.
The installed `C:\Cosmonarchy\Starcraft\ddraw.dll` matches 7.1. It was not swapped.

## Verification performed

- Actual C++ scaler and inverse-input sampling: PASS at seven resolutions and
  nine zoom levels (63 combinations), including odd custom size 1365x769 and
  3840x2160. Output/scratch trailing sentinels remained intact. This is not an
  exhaustive memory-safety proof or a frame-time benchmark.
- Actual C++ transition tests: three failed contracts, described above.
- Minimap input integration model: one failed contract, described above.
- Existing Python zoom geometry matrix: PASS despite the transition defects.
  It tests copied formulas, not the production state machine. Retain it, but
  add production-code transition coverage to the eventual release gate.
- Existing configurator tests: all nine PASS.
- Stable GPTP minimap byte guards and negative-encoding regression: PASS.
- cnc-ddraw 6.9/7.0/7.1 manifest hashes and named-import preflights: all PASS.

The audit executable intentionally returns nonzero while its contracts fail.
It is not part of the release build or renderer. Compile from an x86 Visual
Studio developer prompt in the repository root:

```bat
cl /nologo /std:c++20 /EHsc /O2 /D_CRT_SECURE_NO_WARNINGS ZoomIntegration\audit_world_zoom.cpp /Fe:artifacts\audit_world_zoom.exe /Fo:artifacts\audit_world_zoom.obj /link user32.lib
artifacts\audit_world_zoom.exe
```

For wrapper preflight, pass each local sample path to
`python ZoomIntegration/audit_cnc_ddraw.py`.

## Improvement order and release gate

1. Fix coherent transform publication and rapid-wheel anchoring first.
2. Fix the delayed-frame edge handoff.
3. Confirm and correct zoomed minimap click-centering and popup blending.
4. Test stationary hover at 100%, wheel reversal, selection, placement,
   middle-pan, map edges, minimap tap/hold/release, and popup interaction.
5. Repeat that short interactive matrix under 6.9, 7.0, and 7.1 with zoom
   disabled/enabled, windowed/fullscreen, and 1x/non-1x external scaling.

Low-priority optimization: cache x/y lookup tables by source dimensions rather
than invalidating both when only the crop position changes. They contain
relative offsets and do not depend on crop origin. Measure before prioritizing
this over correctness. Keep the no-op 100% path and do not add full-frame
diagnostics or speculative SIMD/render-pass rewrites.
