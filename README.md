# Cosmonarchy Widescreen & Resolution Settings

Cosmonarchy Widescreen expands the playable StarCraft: Brood War viewport and
keeps its rendering, input, HUD, minimap, overlays, camera behavior, and audio
aligned at modern resolutions. A portable Windows configurator keeps the
internal gameplay resolution independent from the external window/display
size.

This repository contains the complete source for the modified aidebug renderer,
the WinForms configurator, build and verification scripts, and the engineering
system map accumulated while reverse engineering the relevant StarCraft 1.16.1
paths.

## Installation

1. Install a compatible Cosmonarchy build normally.
2. Download `Cosmonarchy Widescreen Settings.exe` from this repository's
   Releases page.
3. Place it beside `Cosmonarchy BW.exe`.
4. Open it, choose an internal resolution and external presentation mode, then
   select **Save** or **Save & Play**.
5. Launch through the normal `Cosmonarchy BW.exe` afterward.

The configurator validates the StarCraft, GPTP, and QDP-loader builds before it
changes anything. It backs up the original aidebug renderer and cnc-ddraw
configuration and provides **Restore Original**.

## Resolution model

- Internal resolution controls world coverage and all gameplay/UI coordinate
  systems. Presets cover 16:9 through 3840x2160, with separate experimental
  4:3 presets and custom dimensions from 640x480 through 3840x2160.
- External presentation controls window size, borderless/exclusive fullscreen,
  display selection, scaling, filtering, and aspect preservation without
  changing gameplay coordinates.

True 4K internal rendering is supported, but the original engine must perform
dozens of sequential native render passes and match loading can take several
minutes even on a high-end CPU. For a fast 4K-sized result, use 1920x1080
internal resolution with 2x external scale.

The most thoroughly regression-tested configuration is 1280x720 internal.

## Building

See [BUILDING.md](BUILDING.md). The short version from a Developer PowerShell
prompt is:

```powershell
.\scripts\build-release.ps1
```

The script builds the universal x86 renderer, runs the offline geometry matrix,
publishes the self-contained x64 configurator, and writes release checksums to
`artifacts/release`.

## Repository layout

- `ZoomSource/Cosmonarchy-aidebug-resolution` — universal runtime-resolution
  renderer and StarCraft patches.
- `ViewportConfigurator` — portable Windows GUI and compatibility manifest.
- `ZoomIntegration` — source-only diagnostic and geometry tools.
- `ZoomSystemMap` — architecture, reverse-engineering map, invariants, workflow,
  and regression checklist.
- `ZoomPresentation` — presentation configuration helper scripts.

## Compatibility and redistribution

This project targets exact compatible Cosmonarchy and StarCraft 1.16.1 builds.
It does not contain or redistribute StarCraft, Cosmonarchy, GPTP, cnc-ddraw,
maps, audiovisual assets, or other game data. Cosmonarchy and StarCraft are
their respective owners' projects and trademarks. See
[THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).

## Status

Version 0.3.0 is the first GitHub release candidate. See the concise
[release notes](RELEASE_NOTES.md), then read the detailed
[feature status](ZoomSystemMap/features/status.md) and
[regression checklist](ZoomSystemMap/testing/regression-checklist.md) before
changing fixed-address patches or declaring another engine build compatible.
