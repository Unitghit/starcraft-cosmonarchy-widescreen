# Cosmonarchy expanded-resolution system map

This directory is the engineering map for the fixed-resolution renderer and
the future arbitrary-resolution work. It records what each component owns,
which coordinate space each value belongs to, what has been verified, and
which assumptions must remain native to StarCraft 1.16.1.

The map uses the thoroughly tested 1280x720 configuration as its primary
worked example: a 1280x640 battlefield and an 80-pixel bottom presentation
region. The universal renderer now derives the same model at startup for
internal dimensions from 640x480 through 3840x2160. The native engine renderer
remains 640x480 with a nominal 640x400 game viewport.

## Start here

1. Read [the change methodology](methodology/change-workflow.md) before
   modifying a new subsystem.
2. Identify the relevant coordinate space in
   [coordinate-spaces.md](architecture/coordinate-spaces.md).
3. Check the component owner in [modules.md](components/modules.md).
4. Review [native-vs-expanded.md](invariants/native-vs-expanded.md).
5. Locate known functions, globals, and hooks in the
   [reverse-engineering index](reverse-engineering/functions-and-globals.md).
6. Run the applicable tests in
   [regression-checklist.md](testing/regression-checklist.md).
7. For performance work, review the
   [performance optimization audit](optimization/performance-audit.md).
8. Update this map with any new confirmed fact.

## Map index

- Architecture
  - [System overview](architecture/overview.md)
  - [Rendering pipeline](architecture/rendering-pipeline.md)
  - [Input pipeline](architecture/input-pipeline.md)
  - [Coordinate spaces](architecture/coordinate-spaces.md)
  - [Future GUI resolution controls](architecture/future-resolution-controls.md)
- Components
  - [Module ownership and loading](components/modules.md)
- Features
  - [Current feature status](features/status.md)
  - [Building-placement case study](features/building-placement.md)
  - [Gameplay hover cursor](features/cursor-hover.md)
  - [Legacy HUD tooltip duplicates](features/legacy-hud-tooltips.md)
  - [Independent HUD and top-text sizing](features/hud-sizing.md)
  - [Positional audio](features/positional-audio.md)
  - [Presentation-only rational scaling](features/presentation-scaling.md)
  - [Optional gameplay zoom](features/world-zoom.md)
  - [Single-stage world presentation design](features/single-stage-world-presentation.md)
  - [Portable widescreen configurator](features/portable-configurator.md)
- Reverse engineering
  - [Functions and globals](reverse-engineering/functions-and-globals.md)
  - [Draw layers and structures](reverse-engineering/draw-layers-and-structures.md)
  - [Binary patch ledger](reverse-engineering/binary-patches.md)
- Engineering controls
  - [Native versus expanded invariants](invariants/native-vs-expanded.md)
  - [Diagnostics](diagnostics/tools-and-artifacts.md)
  - [Change workflow](methodology/change-workflow.md)
  - [Subsystem research template](methodology/subsystem-template.md)
  - [Regression checklist](testing/regression-checklist.md)
  - [Resolution matrix](testing/resolution-matrix.md)
- Optimization
  - [Panning artifact audit](optimization/panning-artifact-audit.md)
  - [Zoom-relative pan speed](optimization/zoom-pan-speed.md)
  - [Gameplay zoom audit](optimization/world-zoom-audit.md)
  - [Performance optimization audit](optimization/performance-audit.md)

## Current implementation anchors

| Item | Location or value |
|---|---|
| Shared resolution configuration | `ZoomSource\zoom_resolution.h` |
| Presentation configuration | `ZoomPresentation` |
| Optional gameplay zoom | `src\world_zoom.cpp` and `[world_zoom]` runtime settings |
| Renderer source | `ZoomSource\Cosmonarchy-aidebug-resolution` |
| Renderer branch / baseline | `development` / tested checkpoint `dd90143` |
| GPTP reference source | `ZoomSource\Cosmonarchy-GPTP` |
| Installed renderer | `Release\plugins\aize_debug.qdp` |
| Installed stable GPTP | `Release\plugins\gptp.qdp` |
| Stable GPTP SHA-256 | `CC6BF422B4DC6174EC6B002ACAE12A826D61CBF144661FE0C4F9E3687664BB99` |
| Universal renderer SHA-256 | Build-specific; the configurator embeds and verifies its own payload hash |
| Portable configurator | `Release\Cosmonarchy Widescreen Settings.exe` |
| Build/install owner | `ViewportConfigurator` |
| Geometry verifier | `ZoomIntegration\verify_fixed_zoom.py` |
| Gameplay zoom verifier | `ZoomIntegration\verify_world_zoom.py` |
| System-map validator | `ZoomSystemMap\validate-system-map.ps1` |

Hashes describe this snapshot, not a permanent compatibility contract. Always
recalculate them after a build. The stable GPTP hash is additionally used as a
known-good recovery identity because locally rebuilt GPTP binaries have caused
data-file errors with the installed Cosmonarchy data set.

## Evidence labels

Use these terms consistently:

- **Confirmed:** observed in live diagnostics, disassembly, or a user test.
- **Source-confirmed:** directly represented in the checked-out source, but
  not necessarily identical to the installed binary.
- **Inferred:** strongly supported by behavior or disassembly but not yet
  isolated experimentally.
- **Unknown:** deliberately not assumed.

Never silently promote an inferred structure field or function role to
confirmed. Record the evidence that changed its status.

Run `validate-system-map.ps1` after changing mapped source anchors, installed
plugins, the resolution configuration, or documentation links.
