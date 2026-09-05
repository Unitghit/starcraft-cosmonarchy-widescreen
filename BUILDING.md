# Building from source

## Requirements

- Windows 10 or 11
- Visual Studio 2022 with **Desktop development with C++** and the Windows SDK
- .NET 8 SDK
- Python 3 available as `python`
- PowerShell 7 or Windows PowerShell 5.1

The renderer is x86 because StarCraft 1.16.1 is a 32-bit process. The
configurator is published as a self-contained x64 Windows executable.

## One-command release build

From the repository root:

```powershell
.\scripts\build-release.ps1
```

This performs four steps:

1. Builds one universal `aize_debug.qdp` payload with a default 1280x720
   fallback. Runtime config safely selects 640x480 through 3840x2160.
2. Checks release cleanliness and runs geometry, zoom, HUD, panning,
   configurator, and D3D9/OpenGL presentation tests. The presentation tests
   require a working Windows graphics session and compatible drivers.
3. Embeds the payload and compatibility manifest into a single-file WinForms
   configurator.
4. Writes `Cosmonarchy Widescreen Settings.exe`, the versioned ZIP bundle,
   and `SHA256SUMS.txt` beneath `artifacts/release`. The ZIP contains only the
   configurator, its checksum, installation instructions and license notices.

The build has no dependency on an installed copy of StarCraft or Cosmonarchy.
Runtime and full system-map integration validation do require a compatible local
installation and intentionally are not part of the public build script.

## GitHub Actions graphics environment

Hosted Windows runners may expose only the system OpenGL 1.1 implementation.
The workflow runs `scripts/setup-ci-opengl.ps1` to download a pinned Mesa 26.2.0
MSVC archive, verify its SHA-256, and extract two x86 DLLs beside the graphics
test executable in `artifacts/single-stage-tests`. The build step selects
Mesa's software `llvmpipe` renderer. No tests are disabled: OpenGL shader/pixel
checks and the existing D3D9 tests must still pass.

These DLLs are test dependencies only. They are never installed system-wide,
embedded in the configurator, copied to the game, or included in the release
ZIP. Local builds normally use the installed graphics driver. Running the CI
setup script locally opts that test directory into Mesa too.

## Manual commands

```powershell
.\ViewportConfigurator\build-profile-pack.ps1
python .\ZoomIntegration\verify_fixed_zoom.py
dotnet publish .\ViewportConfigurator\CosmonarchyWidescreenSettings.csproj `
  -c Release -r win-x64 --self-contained true `
  -p:PublishSingleFile=true -o .\artifacts\release
```

## Compatibility manifest

`ViewportConfigurator/compatibility-manifest.json` contains the exact supported
binary hashes. Do not weaken or update those checks merely to accept a newer
game build: fixed-address patches must first be re-audited and the full
interactive regression checklist completed.

The configurator reads and verifies `gptp.qdp`; it never writes, replaces,
restores, embeds, or distributes it.
