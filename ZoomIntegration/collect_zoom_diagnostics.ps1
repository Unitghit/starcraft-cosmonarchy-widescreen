param(
    [string]$Label = "interaction",
    [string]$Workspace = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = "Continue"
$workspace = [System.IO.Path]::GetFullPath($Workspace)
$release = Join-Path $workspace "Release"
$source = Join-Path $workspace "ZoomSource\Cosmonarchy-aidebug-resolution"
$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$safeLabel = $Label -replace '[^A-Za-z0-9_-]', '_'
$output = Join-Path $workspace "ZoomIntegration\diagnostics\${stamp}_${safeLabel}"
New-Item -ItemType Directory -Path $output -Force | Out-Null

@(
    "fixed_zoom_renderer.log",
    "fixed_zoom_cursor_hover.log",
    "fixed_zoom_tooltip.log",
    "fixed_zoom_presentation.log",
    "fixed_zoom_input.log",
    "fixed_zoom_first_frame.raw",
    "fixed_zoom_first_frame.txt"
) | ForEach-Object {
    $path = Join-Path $release $_
    if (Test-Path -LiteralPath $path) {
        Copy-Item -LiteralPath $path -Destination $output -Force
    }
}

$processes = Get-Process StarCraft,Cosmonarchy -ErrorAction SilentlyContinue
$processes |
    Select-Object Id,ProcessName,Responding,MainWindowTitle,StartTime,Path |
    Format-List |
    Out-File -LiteralPath (Join-Path $output "processes.txt") -Encoding utf8

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class ZoomDiagnosticWindow {
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll")]
    public static extern bool GetClientRect(IntPtr hwnd, out RECT rect);
    [DllImport("user32.dll")]
    public static extern uint GetDpiForWindow(IntPtr hwnd);
}
'@ -ErrorAction SilentlyContinue

$windowLines = foreach ($process in $processes) {
    $rect = New-Object ZoomDiagnosticWindow+RECT
    $ok = [ZoomDiagnosticWindow]::GetClientRect($process.MainWindowHandle,
        [ref]$rect)
    $dpi = [ZoomDiagnosticWindow]::GetDpiForWindow($process.MainWindowHandle)
    "pid=$($process.Id) hwnd=$($process.MainWindowHandle) ok=$ok " +
        "client=$($rect.Right-$rect.Left)x$($rect.Bottom-$rect.Top) dpi=$dpi"
}
$windowLines | Out-File -LiteralPath (Join-Path $output "windows.txt") -Encoding utf8

$hashPaths = @(
    (Join-Path $source "Release\aize_debug.qdp"),
    (Join-Path $release "plugins\aize_debug.qdp"),
    (Join-Path $workspace "Starcraft\StarCraft.exe")
)
$hashPaths | Where-Object { Test-Path -LiteralPath $_ } |
    ForEach-Object { Get-FileHash -LiteralPath $_ -Algorithm SHA256 } |
    Format-Table -AutoSize |
    Out-File -LiteralPath (Join-Path $output "hashes.txt") -Encoding utf8

git -C $source status --short 2>&1 |
    Out-File -LiteralPath (Join-Path $output "git_status.txt") -Encoding utf8
git -C $source diff --stat 2>&1 |
    Out-File -LiteralPath (Join-Path $output "git_diff_stat.txt") -Encoding utf8

$inputLog = Join-Path $release "fixed_zoom_input.log"
if (Test-Path -LiteralPath $inputLog) {
    $lines = Get-Content -LiteralPath $inputLog
    $events = $lines | Where-Object { $_ -match ' raw=\(' }
    $wide = $events | Where-Object {
        $_ -match 'raw=\(([-0-9]+),([-0-9]+)\)' -and
        ([int]$Matches[1] -ge 640 -or [int]$Matches[2] -ge 400)
    }
    @(
        "event_count=$($events.Count)",
        "wide_or_lower_event_count=$($wide.Count)",
        "last_40_events:",
        ($events | Select-Object -Last 40)
    ) | Out-File -LiteralPath (Join-Path $output "input_summary.txt") -Encoding utf8
}

try {
    Get-WinEvent -FilterHashtable @{
        LogName = "Application"
        StartTime = (Get-Date).AddHours(-4)
    } -ErrorAction Stop |
        Where-Object {
            $_.Message -match 'StarCraft|Cosmonarchy|aize_debug'
        } |
        Select-Object TimeCreated,Id,LevelDisplayName,ProviderName,Message |
        Format-List |
        Out-File -LiteralPath (Join-Path $output "application_events.txt") -Encoding utf8
} catch {
    $_ | Out-File -LiteralPath (Join-Path $output "application_events_error.txt") -Encoding utf8
}

$archive = "$output.zip"
Compress-Archive -LiteralPath $output -DestinationPath $archive -Force
Write-Output "Diagnostics: $output"
Write-Output "Archive: $archive"
