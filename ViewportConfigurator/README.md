# Cosmonarchy Widescreen Settings

`Cosmonarchy Widescreen Settings.exe` is a portable configurator for the
expanded Cosmonarchy renderer. Put the executable beside `Cosmonarchy BW.exe`,
open it, choose settings, and select **Save** or **Save & Play**.

The program deliberately keeps two settings independent:

- **Internal viewport resolution** selects a 16:9 or 4:3 preset, or accepts a
  custom size, and changes world coverage and HUD geometry.
- **External presentation** scales or fits the completed image without
  changing gameplay coordinates.

One universal runtime renderer supports internal dimensions from 640x480
through 3840x2160. The selector includes 16:9 presets through 4K, experimental
4:3 presets, and custom width/height fields. The 1280x720 profile remains the
recommended and most thoroughly user-tested target.
External presentation supports scale presets, fit-to-display, exact output
dimensions, windowed, borderless fullscreen, exclusive fullscreen,
nearest-neighbor or smooth filtering, and aspect preservation.

## Portable installation

Expected layout before first Save:

```text
Release/
  Cosmonarchy BW.exe
  Cosmonarchy Widescreen Settings.exe
  plugins/
    aize_debug.qdp
    gptp.qdp
../Starcraft/
  StarCraft.exe
  ddraw.ini
```

Save creates:

```text
Release/
  cosmonarchy_viewport.ini
  .cosmonarchy-widescreen/
    installation.json
    backup/
      aize_debug.qdp
      ddraw.ini
```

The configurator embeds the viewport renderer. It transactionally replaces
`plugins/aize_debug.qdp`, updates only the top-level cnc-ddraw settings it owns,
and verifies installed hashes. **Restore Original** restores the files captured
before the first Save.

The configurator never writes, replaces, restores, or distributes `gptp.qdp`.
It only verifies the compatible GPTP hash before installing fixed-address
integration patches.

## Build

Build the universal x86 renderer, then publish the GUI:

```powershell
.\build-profile-pack.ps1

dotnet publish .\CosmonarchyWidescreenSettings.csproj `
  -c Release -r win-x64 --self-contained true `
  -p:PublishSingleFile=true -o .\publish
```

The publish output is one executable. The renderer, compatibility manifest,
and aidebug license are embedded resources.

## Release safety

- Keep the runtime bounds and GUI bounds synchronized. New aspect families
  remain experimental until completing their interactive regression checklist.
- Update supported hashes only after inspecting the corresponding upstream
  change and completing the regression checklist.
- Run the isolated `--validate`, `--apply-defaults`, `--apply-borderless`,
  representative `--apply-profile=<width>x<height>`,
  `--apply-custom=<width>x<height>`, and `--restore` smoke paths before
  publishing.
- Publish a SHA-256 checksum beside every GitHub release asset.
- Do not package Cosmonarchy, StarCraft, GPTP, cnc-ddraw, maps, or audiovisual
  assets without explicit redistribution permission.
