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
2. Runs the offline preset/custom geometry matrix.
3. Embeds the payload and compatibility manifest into a single-file WinForms
   configurator.
4. Writes `Cosmonarchy Widescreen Settings.exe` and `SHA256SUMS.txt` beneath
   `artifacts/release`.

The build has no dependency on an installed copy of StarCraft or Cosmonarchy.
Runtime and full system-map integration validation do require a compatible local
installation and intentionally are not part of the public build script.

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
