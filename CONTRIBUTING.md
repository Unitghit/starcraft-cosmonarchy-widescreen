# Contributing

Read `ZoomSystemMap/methodology/change-workflow.md` before modifying a new
subsystem. Every change should follow the repository's research → system map →
implementation → diagnostics → verification → documentation workflow.

In particular:

- Preserve native 640x480 engine assumptions unless the mapped owner is known.
- Derive expanded geometry through `resolution::Configure()`; do not scatter
  resolution literals through hooks.
- Keep internal viewport geometry separate from external presentation scaling.
- Never replace or redistribute GPTP as part of this project.
- Treat all fixed addresses and instruction signatures as build-specific.
- Run the offline geometry matrix and the relevant interactive regression tests.
- Document newly confirmed addresses, structures, invariants, and failure modes
  in `ZoomSystemMap`.

Bug reports should include the internal resolution, external output settings,
display mode, race, game/menu state, reproduction steps, and relevant generated
logs. Do not upload copyrighted game files or memory dumps containing them.
