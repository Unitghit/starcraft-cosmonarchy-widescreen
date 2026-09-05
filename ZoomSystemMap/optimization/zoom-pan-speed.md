# Zoom-relative pan speed

## Contract and evidence

2026-09-05. Keep approximately constant displayed movement per input at every
zoom level, including transitions. Preserve native sensitivity, inversion,
scroll acceleration, camera bookkeeping, and unzoomed behavior. Do not scale
minimap jumps, portrait/control-group centering, or scripted camera moves.

Read-only live disassembly is saved locally in
`artifacts/diagnostics/capture_pan_owners.txt` (not packaged). Stable GPTP SHA256:
`CC6BF422B4DC6174EC6B002ACAE12A826D61CBF144661FE0C4F9E3687664BB99`.

| Owner | Site | Contract |
|---|---|---|
| GPTP middle pan | RVA `0x67F28`, call to `0x67DA0` | Signed world-pixel deltas in ECX/EDX, after sensitivity/inversion |
| Engine scroll up | `0x47F210`, call to `0x49C360` | Positive world-pixel amount in EAX, Boolean result in EAX |
| Engine scroll down | `0x47F226`, call to `0x49C280` | Positive amount in EDX, result in EAX |
| Engine scroll left | `0x47F247`, call to `0x49C1A0` | Positive amount in EAX, result in EAX |
| Engine scroll right | `0x47F25B`, call to `0x49C0C0` | Positive amount in EDX, result in EAX |

GPTP replaces engine MMB move `0x484460` with RVA `0x67E80` after the
renderer initializes. The older `ExpandedMiddlePanMove` is not the active
Cosmonarchy panner. Engine edge/keyboard callers retain the native speed table
and GPTP's middle-pan scroll suppression.

Hypothesis: these five callers forward unchanged world-pixel deltas regardless
of the displayed crop. Magnifying that crop magnifies the apparent pan speed.
The captured call instructions confirm no zoom adjustment at this boundary.

## Implementation

The isolated `src/zoom_pan.cpp` module signature-checks all five call sites and the GPTP move
entry before installing in-memory call adapters. Only their world-pixel deltas
are scaled by displayed source extent / internal extent, independently per axis.
This uses the current rendered transform, not a wheel target or rounded percent.
Native routines retain their camera clamps, guards, dirty-region updates, and
return values. No renderer, OS cursor, wrapper configuration, or on-disk GPTP
changes are required.

Separate fractional accumulators for MMB and edge/keyboard input avoid losing
subpixel movement. Reset on inactivity, direction reversal, external camera
movement, native clamp/rejection, or returning to unzoomed behavior. No temporal
pixel reuse, diagnostics, allocations, or per-frame executable writes are added.

Map-boundary crop repositioning is an existing separate transform. This change
normalizes requested camera speed, not that transform or map-edge clamping.

## Verification status

Baseline ownership verified live. Release renderer compilation passed.
`ZoomIntegration/verify_zoom_pan.ps1` passes 700,000 fractional movement steps
across five dimensions and seven zoom factors, positive/negative movement,
direction reversal, inactivity, external camera jumps, clamped movement, tick
wraparound, and changing displayed zoom. It includes the actual production
adapter code, exercises all four register-ABI calls and the GPTP fastcall
adapter against isolated native stubs, and verifies return values and preserved
registers. Branch preflight accepts the expected call and rejects a changed
target/opcode. This runner is also a release-build gate.

Existing native world-zoom/input tests, 3,072,960 panning crop cases, fresh-frame
sequences at seven resolutions, fixed-resolution geometry, zoom geometry, and
2,240 HUD-sizing configurations pass. Runtime diagnostics remain disabled.

The initial test helper reserved fixed dummy engine addresses in its own
process. Narrowing this to three 64 KiB regions passed locally but still collided
with allocations on GitHub's runner. The harness now allocates a relocatable
dummy engine region and uses a compile-time-only address mapper for its native
stubs and camera data. Production builds retain the original constant addresses;
the Release gate rejects test definitions. Register ABI and motion tests are
unchanged, and no checks are skipped when low memory is occupied.

CPU/GDI/OpenGL/D3D9 presentation regression tests and the system-map validator
also pass. Local test renderer SHA256:
`C9D8EE0C09787C4938BFD10F9C2DB9BC5EE7579984269EF1068C5F5B7D055942`.
It is installed in the Windows Release game with existing settings preserved.
Stable GPTP, cnc-ddraw DLL, and cnc-ddraw INI hashes remain unchanged. The
previous renderer and settings were backed up before installation.

Read-only post-launch inspection in `artifacts/diagnostics/zoom_pan_installed.txt`
confirms all five live call sites now target this renderer's adapters. This
verifies installation/ownership, not the subjective pan feel. No runtime
capture loop or diagnostics were added to the shipped renderer.

The user tested this build and reported "Alright seems good." This confirms
the reported pan-feel issue is resolved in that session, not that every item
below was individually exercised. The verified change is included in v0.5.2.
The broader regression checklist includes horizontal/vertical/diagonal middle pan,
edge/keyboard scrolling at several zoom levels, native 100%, stop/reverse,
map edges, minimap drag, and portrait/control-group centering.
