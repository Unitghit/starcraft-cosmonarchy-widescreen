# Integration and diagnostic tools

The active implementation is the source-native universal multipass renderer in
`ZoomSource/Cosmonarchy-aidebug-resolution`. Architecture, addresses,
invariants, and workflow are documented in `ZoomSystemMap`.

This directory intentionally contains source-only tools:

- `verify_fixed_zoom.py` validates preset and custom geometry offline.
- `collect_zoom_diagnostics.ps1` gathers bounded logs and environment metadata
  from a local development installation.
- `capture_live_modules.ps1` records loaded module information.
- Python inspection/disassembly helpers support fixed-address research.

The failed legacy Resolution Expander binaries, bridge QDPs, memory captures,
game modules, backups, screenshots, and local logs from development are not
distributed in this repository. The historical conclusion remains: the legacy
expander is incompatible, and the active renderer must preserve the installed
stable GPTP build.
