# Module ownership and loading

## `StarCraft.exe`

Path: `Starcraft\StarCraft.exe`

Owns:

- native draw layers, STrans surfaces, dirty grids, camera, and map rendering;
- Windows input dispatch and most cursor clamps;
- selection, targeting, and building-placement engine state;
- native dialog tree and UI masks.

The expanded renderer patches narrowly selected StarCraft instructions but
does not resize its native internal surfaces.

## stable `gptp.qdp`

Path: `Release\plugins\gptp.qdp`

Known-good SHA-256:
`CC6BF422B4DC6174EC6B002ACAE12A826D61CBF144661FE0C4F9E3687664BB99`

Owns or replaces:

- many Cosmonarchy gameplay routines;
- the active layer-5 world wrapper and queued map graphics;
- rally and order graphics;
- the installed replacement for building-placement position initialization;
- Cosmonarchy-specific UI and gameplay behavior.

The module is ASLR-loaded. Runtime addresses must be calculated as module base
plus RVA. Do not document an observed live pointer as a permanent address.

Locally rebuilding GPTP has produced data-file errors or startup/gameplay
crashes with this Release data. The restart script always restores the stable
backup at `ZoomIntegration\backups\gptp.pre_fixed_zoom.qdp`.

## `aize_debug.qdp`

Source: `ZoomSource\Cosmonarchy-aidebug-resolution`

Installed path: `Release\plugins\aize_debug.qdp`

Owns:

- output window geometry;
- fixed multipass world composition;
- HUD and popup relocation;
- once-per-frame text, selection, and cursor composition;
- expanded input routing and diagnostics;
- guarded StarCraft and stable-GPTP compatibility patches.

## optional gameplay zoom subsystem

Final-output sampling is isolated in `src/single_stage.cpp`,
`src/single_stage_portable.cpp`, and `src/single_stage_frame.h`. The shader is
built from `src/single_stage.hlsl`. Named Pixel-perfect in the GUI and default
when zoom is enabled, this backend uses the public
graphics interfaces and process-local wrapper hooks, not a replacement
`ddraw.dll`. See [its contract](../features/single-stage-world-presentation.md).

Runtime configuration: `[world_zoom]` in `cosmonarchy_viewport.ini`

Implementation: `src\world_zoom.cpp`

Owns only the opt-in battlefield crop/scale and its inverse gameplay-input
transform. It does not own internal resolution, presentation size, HUD/menu
geometry, or top-text layout. At 100 percent it takes the original no-op path.

## cnc-ddraw / windowing layer

Files include `Starcraft\ddraw.dll` and `ddraw.ini`.

Owns physical DirectDraw/window presentation behavior. The game and compositor
request a logical 1280x720 surface; the isolated presentation subsystem may
display it at an independently configured rational or fitted client size.
Physical surface pitch must always
be taken from the SDraw lock result.

The renderer discovers cnc-ddraw's named USER32 `ClipCursor` and
`SetCursorPos` import slots from the loaded PE image. It does not depend on
version-specific RVAs. Official cnc-ddraw 6.9, 7.0, and 7.1 are confirmed
targets. Unknown wrappers fail the import preflight safely without modifying
their binary.

## presentation subsystem

Default configuration: `ZoomSource\zoom_presentation.h`

Runtime configuration: `Release\cosmonarchy_viewport.ini`

Implementation: `src\presentation.cpp`

Wrapper configuration: `Starcraft\ddraw.ini`

Owns only runtime client sizing, raster presentation, and the wrapper's
physical-to-logical mouse scale. It must not supply dimensions to
render, camera, HUD, dialog, selection, placement, or gameplay patches.

## Launcher and Release tree

`Release\Cosmonarchy BW.exe` launches the configured
StarCraft installation and plugins. The portable configurator is the canonical
installation path: it validates compatibility, backs up the original renderer,
installs the universal payload, updates owned presentation keys, and restores
the original files on request.

`Release\Cosmonarchy Widescreen Settings.exe` is the portable
user configuration path. It embeds the universal renderer, writes runtime
presentation configuration, updates owned cnc-ddraw keys transactionally, and
backs up/restores the pre-install aidebug file. Its installation state tracks
original and applied values for each owned ddraw key, allowing Restore to
preserve later user changes and unrelated wrapper configuration. It never
writes GPTP or `ddraw.dll`. It hashes a present `ddraw.dll` only to report
whether the installed cnc-ddraw version is tested or unverified.

## Load-order rule

GPTP may not be available during aidebug's earliest initialization. Runtime
GPTP patching therefore retries from `BeginStockDrawScreen` until the module is
loaded, then transitions permanently to Installed or Incompatible state.
