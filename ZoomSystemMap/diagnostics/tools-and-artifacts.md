# Diagnostic tools and artifacts

## Canonical commands

Build the renderer:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' `
  'ZoomSource\Cosmonarchy-aidebug-resolution\teippi.sln' `
  /m /t:Build /p:Configuration=Release /p:Platform=x86 `
  /p:PostBuildEventUseInBuild=false /v:minimal
```

Verify derived geometry:

```powershell
python ZoomIntegration\verify_fixed_zoom.py
```

Verify optimized menu-scaler pixel equivalence:

```powershell
python ZoomIntegration\verify_menu_scaler.py
```

Validate the system map, stable plugin identity, source anchors, geometry, and
installed renderer:

```powershell
& ZoomSystemMap\validate-system-map.ps1
```

Build the universal renderer payload:

```powershell
& ViewportConfigurator\build-profile-pack.ps1
```

Collect a diagnostic bundle:

```powershell
& ZoomIntegration\collect_zoom_diagnostics.ps1 -Label placement
```

## Runtime logs

| Artifact | Contents |
|---|---|
| `Release\fixed_zoom_renderer.log` | Geometry, patch preflights, client size, layer table, camera, hashes, popup transitions |
| `Release\fixed_zoom_input.log` | Raw/forwarded/engine mouse data, zones, commands, drag clips |
| `Release\fixed_zoom_cursor_hover.log` | Cursor type, GRP, mouse zone, popup, placement, drag, and layer transitions |
| `Release\fixed_zoom_tooltip.log` | Legacy-HUD tooltip suppression transitions and physical mouse coordinates |
| `Release\fixed_zoom_tooltip_frames.log` | Opt-in per-frame context-help owner, visibility, bounds, surface size, and hash |
| `Release\fixed_zoom_private_passes.txt` | First gameplay pass geometry and hashes |
| `Release\fixed_zoom_private_pass_0..5.raw` | First six native private buffers |
| `Release\fixed_zoom_placement_passes.txt` | Placement-specific mouse, tile, layer position, dimensions, and hashes |
| `Release\fixed_zoom_placement_pass_0..5.raw` | Native buffers for the first active placement frame |
| `Release\fixed_zoom_first_frame.raw/.txt` | Expanded output capture and metadata |
| `ZoomIntegration\restart_with_zoom.log` | Process close, install hash, and launch result |

The restart script clears one-time renderer and placement captures so each test
produces fresh evidence.

Per-frame tooltip capture is intentionally opt-in because a native visibility
transition can occur every frame. Create
`Release\fixed_zoom_tooltip_capture.enabled` to arm it; remove that marker to
disarm it. The renderer detects marker changes within one second without a
restart. The restart script clears the output log but preserves the marker.

## Analysis utilities

| Tool | Purpose |
|---|---|
| `render_raw_diagnostic.py` | Convert indexed-8 raw buffers to deterministic false-color PNGs |
| `disasm_pe.py` | Disassemble a virtual-address range from a PE image |
| `scan_gptp_resolution_constants.py` | Find installed GPTP mouse and placement references |
| `scan_gptp_cursor_hover.py` | Locate stable-GPTP native cursor-rectangle references and selector context |
| `inspect_live_renderer.py` | Inspect live renderer-related state when process access permits |
| `inspect_live_placement_hook.py` | Resolve live GPTP placement jump when caller integrity permits |
| `analyze_minidump_stack.py` | Extract crash-stack evidence |
| `capture_live_modules.ps1` | Record loaded module information |
| `collect_zoom_diagnostics.ps1` | Bundle logs, process/window state, hashes, git status, and event logs |

Live process-memory tools can fail with access denied if StarCraft and the
diagnostic process run at different integrity levels. Static disassembly plus a
guarded in-plugin runtime log is the fallback.

## Capture discipline

For each investigated issue:

1. Name the reproduction in the collector label.
2. Record the exact mouse location and game state.
3. Preserve pre-change captures as a baseline.
4. Capture every private pass, not only the composed output.
5. Log both raw structure fields and semantic interpretations when uncertain.
6. Hash the installed modules.
7. Store conclusions in this system map, not only in transient logs.
