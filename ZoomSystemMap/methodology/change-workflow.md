# Evidence-first change workflow

This is the required workflow for changing a new Cosmonarchy rendering, input,
UI, or gameplay-coordinate subsystem.

```text
Research
  -> map ownership and coordinate spaces
  -> state a falsifiable hypothesis
  -> instrument the current behavior
  -> capture a baseline
  -> implement one semantic change
  -> run preflight checks
  -> test the feature and neighboring layers
  -> compare evidence
  -> update the system map
```

## 1. Define the symptom and success condition

Write down what is visible, what input produced it, where it occurs, and what
correct behavior would be. Include the output resolution, game state, selected
units, popup state, and whether the point lies in battlefield, HUD, or dialog
space.

Do not start with an address or a patch. Start with an observable contract.

Exit gate: another person could reproduce the issue and recognize success.

## 2. Research ownership

Trace the behavior through all possible owners:

- Windows and cnc-ddraw: physical client size and OS cursor coordinates.
- StarCraft: native layers, clipping, world camera, input dispatch, and game
  globals.
- stable GPTP: replaced engine routines and custom layer-5 graphics.
- aidebug renderer: multipass cameras, composition, relocation, and input
  translation.

Inspect both source and the installed binary. Source is a guide; it is not
proof that the installed QDP contains the same implementation.

Exit gate: the owning module and entry point are identified, or the uncertainty
is explicitly documented.

## 3. Extend the system map before implementation

Record:

- function address or module-relative RVA;
- relevant globals and structure layouts;
- input and output coordinate spaces;
- callers and downstream consumers;
- native assumptions that must remain intact;
- evidence level and open questions.

If a field's semantics are unknown, record its bytes and observed changes. Do
not name it by intuition alone.

Exit gate: the proposed write targets have known ownership and semantics.

## 4. Form a falsifiable hypothesis

A useful hypothesis predicts a diagnostic result. Example:

> The placement ghost disappears outside x=639 because the installed GPTP
> replacement rejects the mouse point before layer 4 is enabled.

The corresponding observation was a live function containing inclusive
639/399 comparisons and inactive placement layers beyond that boundary.

Avoid broad hypotheses such as “the renderer does not support widescreen.”

Exit gate: one capture can prove or disprove the claim.

## 5. Instrument before changing behavior

Prefer small, bounded diagnostics:

- log the entry values and result of a coordinate conversion;
- capture every private render pass;
- record layer enable flags, positions, dimensions, and function pointers;
- log raw, forwarded, and engine mouse coordinates;
- disassemble the exact runtime target;
- hash installed and built modules.

Rate-limit recurring logs. One-time captures should reset on install. Never
change gameplay state merely to make diagnostics easier.

Exit gate: a baseline artifact exists in `ZoomIntegration\diagnostics` or the
Release diagnostic files.

## 6. Classify every value before scaling it

For every affected value, choose exactly one class:

- physical output geometry;
- expanded battlefield geometry;
- native renderer geometry;
- native UI geometry;
- per-pass local coordinates;
- world pixels;
- map tiles;
- structure dimension, pitch, count, pointer, or flag—not a coordinate.

This gate prevents the placement regression where the 128x128 ghost surface
dimensions were mistakenly translated as positions.

Exit gate: every value being changed has a documented class.

## 7. Implement the smallest semantic change

Change one ownership boundary at a time. Preserve the stable GPTP file. For
binary patches:

- verify exact original bytes or an instruction signature;
- calculate addresses relative to the loaded module when ASLR applies;
- refuse to patch an unknown build;
- log success or preflight failure;
- flush the instruction cache after changing executable memory.

For tiled rendering, translate only screen positions into pass-local space.
Never resize a native surface or expand a native pitch unless every producer
and consumer has been audited.

Exit gate: the implementation has a safe failure mode.

## 8. Verify offline before launching

At minimum:

1. Build the x86 Release target.
2. Run `verify_fixed_zoom.py`.
3. Confirm the installed GPTP recovery hash.
4. Search for unintended writes to newly understood structures.
5. Review the diff for unrelated modifications.

Exit gate: build and preflight checks pass.

## 9. Test the change and its neighbors

Test the exact reproduction first, then adjacent systems that share layers or
coordinate paths. A placement change also requires cursor, click, camera-edge,
HUD, selection, popup, rally-line, and map-edge checks.

Compare new diagnostics to the baseline. Visual success without state evidence
is not sufficient for a new structure interpretation.

Exit gate: the target behavior works and no neighboring regression is known.

## 10. Close the loop

Update:

- feature status;
- address and structure references;
- invariant lists;
- test results and resolutions;
- diagnostic documentation;
- confidence labels.

Include failures and abandoned approaches when they constrain future work.
The system map is part of the implementation, not optional cleanup.

## Stop conditions

Pause implementation and return to research when:

- the installed binary differs from the source being read;
- a candidate field could be either a coordinate or a dimension;
- a patch preflight fails;
- the proposed change expands a native buffer or pitch without all consumers
  being known;
- a change requires replacing the stable GPTP build;
- a crash or data-file error appears that was absent in the baseline;
- diagnostics contradict the current map.
