# Zoom activation latency

Date: 2026-09-04. User reports a delay at zoom activation, distinct from its
overall duration. Installed source/binary baseline renderer hash:
`FB213F3A8F95AF5AD1791AEDEE92CAA69DB2DB3463CB44BCC47EBAF46D22C4C3`.
Settings: 1080p internal, 4K output, single-stage nearest, Smooth, 150% start.

## Ownership and baseline evidence

- `ConsoleWndProc::WM_MOUSEWHEEL` consumes eligible battlefield input, changes
  only the target/anchor, and refreshes cursor state. No texture allocation or
  world rendering occurs inside this handler. Preserve that ownership.
- `world_zoom.cpp::UpdateAnimation` advances only on the outer draw. The
  120ms smoothstep has zero starting velocity: after a simulated 16ms frame,
  the production function has advanced only 60 of 1250 zoom units (4.8%).
- `single_stage.cpp::World` resizes a packed world vector to each crop. The
  isolated production-adapter test warms both alternating buffers at 1280x720,
  then expands through crops to 1920x1080. Four capacity growths occur during
  that sequence. They can allocate/copy/free memory on the game thread.
- GPU textures already allocate to the logical maximum and do not resize with
  each crop. At this 150% starting view they are initialized before wheel use.

These are reproducible mechanisms, not a measured live input-to-photon delay.
No evidence yet establishes how much of the user's sensation each causes.

## Proposed correction and invariants

- Keep the 120ms duration and all intermediate draws, but use quadratic
  ease-out: nonzero onset velocity, decelerating smoothly to zero at completion.
- Size each CPU world buffer for the full logical frame on first use. Crop
  dimensions continue to delimit packed valid pixels for all three adapters.
  Never resize or zero-fill the vector as the crop changes afterward.
- Retain existing target selection, pointer anchoring, input lookup publication,
  map-edge handoff, palette/UI coherence, and renderer fallbacks.
- First-ever GPU setup from a 100% starting view is not removed by this change.
  Do not claim that all sources of activation latency have been eliminated.

## Verification

Baseline tests: response contract fails at 60/1250; warmed-capacity contract
fails at four growths. Both tests live in offline executables, not the renderer.
Run native zoom, exact GPU/CPU presentation, Wine, and full release gates, then
request user validation of initial wheel response and zoom reversals.

## Implemented result

The same 16ms sample advances 311/1250 units rather than 60/1250, with completion
still at 120ms and monotonic intermediate frames. The warmed crop-growth test
now reports zero allocations, and packed source rows remain byte-identical.
Exact output pixel tests, device reset/fallback tests, and zoom anchoring/edge
tests pass on Windows and Ubuntu/Wine. Full release-dev build and its disabled
diagnostics gate pass. Live activation-latency improvement remains for the user
to confirm; no input-to-photon timing claim is made.

Local handoff: renderer `0F848FBB185730EDBC997E32FBFA9984948A69CDBCE7DE367FF00ADF790885E7`,
configurator `FD9B985EEC5D2F0B7D7E34DFDF498ED60B1ADCF53EAEA07CBA62EBD9AF883327`.
Backup: `C:\Cosmonarchy\ZoomIntegration\backups\zoom-activation-20260904`.
Installed settings unchanged. GPTP and cnc-ddraw hashes unchanged. No publishing.
