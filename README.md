# StarCraft Cosmonarchy Widescreen

StarCraft Cosmonarchy Widescreen adds modern resolutions to Cosmonarchy while
keeping gameplay, input, the HUD, minimap, overlays, camera behavior, and
positional audio aligned correctly.

It includes a small Windows configurator. Internal gameplay resolution and
external window size can be adjusted independently.

## Install

1. Install [Cosmonarchy](https://www.fraudsclub.com/cosmonarchy-bw/).
2. Download `Cosmonarchy Widescreen Settings.exe` from the
   [latest release](https://github.com/Unitghit/starcraft-cosmonarchy-widescreen/releases/latest).
3. Place the EXE beside `Cosmonarchy BW.exe`.
4. Open it and choose your settings.
5. Select **Save** or **Save & Play**.

The configurator checks that the installed game build is compatible before it
changes anything.

## Recommended settings

- Internal resolution: `1280 x 720`
- External scale: choose the size that fits your display
- Window mode: windowed or borderless fullscreen
- Filter: nearest neighbor for sharp pixels

Internal resolutions up to 4K and custom resolutions are supported. High
internal resolutions can take several minutes to load because the original
engine must perform many additional render passes. For a faster 4K-sized
window, use `1920 x 1080` internally with `2x` external scale.

## Restore the original game

Select **Restore Original** in the configurator. It restores the original
renderer and cnc-ddraw settings, then removes generated configuration files,
backups, logs, and diagnostics.

The configurator EXE remains in the folder. It can be used again or deleted
manually.

## Build from source

Open a Developer PowerShell prompt and run:

```powershell
.\scripts\build-release.ps1
```

The finished EXE and checksum are written to `artifacts/release`.

See [BUILDING.md](BUILDING.md) for requirements and detailed instructions.

## Compatibility

This release targets the compatible Cosmonarchy and StarCraft 1.16.1 builds
listed in the bundled compatibility manifest. Fixed-address engine patches are
only installed after file hashes have been verified.

This repository does not distribute StarCraft, Cosmonarchy, GPTP, cnc-ddraw,
maps, audiovisual assets, or other game data. See
[THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md) for attribution.

## Project documentation

- [Release notes](RELEASE_NOTES.md)
- [Changelog](CHANGELOG.md)
- [Feature status](ZoomSystemMap/features/status.md)
- [Engineering system map](ZoomSystemMap/README.md)
- [Contributing](CONTRIBUTING.md)

## License

The original source in this repository is available under the
[MIT License](LICENSE). Third-party components retain their original licenses.
