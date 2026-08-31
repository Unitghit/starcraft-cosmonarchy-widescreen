# Subsystem research template

Copy this file into `features/` when beginning a new subsystem.

## Symptom and success condition

- Observed behavior:
- Reproduction steps:
- Resolution and game state:
- Expected behavior:
- Neighboring features at risk:

## Ownership map

| Stage | Module | Function / RVA | Input space | Output space | Evidence |
|---|---|---|---|---|---|
| Windows event or draw entry | | | | | |
| StarCraft native processing | | | | | |
| GPTP replacement/custom path | | | | | |
| aidebug composition/routing | | | | | |

## Structures and globals

| Address / field | Access width | Proposed meaning | Value class | Confidence | Evidence |
|---|---:|---|---|---|---|
| | | | position / dimension / pointer / flag / count | unknown | |

## Native and expanded invariants

Must remain native:

-

Must derive from output geometry:

-

Per-pass transient state requiring restoration:

-

## Hypothesis

Statement:

Predicted diagnostic result:

Observation that would disprove it:

## Baseline diagnostics

- Capture directory:
- Module hashes:
- Relevant log excerpt:
- Pass or screenshot comparison:
- Disassembly range and binary identity:

## Proposed change

- Exact semantic change:
- Preflight signature or expected bytes:
- Failure behavior:
- Why no native buffer/pitch invariant is violated:

## Verification

- [ ] Builds in x86 Release.
- [ ] Geometry verifier passes.
- [ ] Exact reproduction passes.
- [ ] Neighboring layer/input tests pass.
- [ ] Runtime patch log passes.
- [ ] Stable GPTP hash remains correct.
- [ ] Diagnostics match the prediction.

## Conclusions and map updates

- Confirmed facts:
- Rejected hypotheses:
- Remaining unknowns:
- Documents updated:
