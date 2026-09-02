param(
    [string]$Version = '0.4.6'
)

$ErrorActionPreference = 'Stop'

$repository = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$configurator = Join-Path $repository 'ViewportConfigurator'
$output = Join-Path $repository 'artifacts\release'
$bundleRoot = Join-Path $repository 'artifacts\bundle'
$bundleName = "StarCraft-Cosmonarchy-Widescreen-v$Version"
$bundleDirectory = Join-Path $bundleRoot $bundleName
$bundleArchive = Join-Path $output "$bundleName.zip"

& (Join-Path $configurator 'build-profile-pack.ps1')
if ($LASTEXITCODE -ne 0) {
    throw 'Universal renderer build failed'
}

$diagnosticsHeader = Join-Path $repository `
    'ZoomSource\Cosmonarchy-aidebug-resolution\src\runtime_diagnostics.h'
$diagnosticsSource = Get-Content -LiteralPath $diagnosticsHeader -Raw
if ($diagnosticsSource -notmatch `
        'constexpr\s+bool\s+Enabled\s*\(\s*\)\s*\{\s*return\s+false\s*;') {
    throw 'Runtime diagnostics are not compile-time disabled'
}

$rendererPayload = Join-Path $configurator `
    'Payloads\1280x720\aize_debug.qdp'
$rendererText = [System.Text.Encoding]::ASCII.GetString(
    [System.IO.File]::ReadAllBytes($rendererPayload))
$diagnosticMarkers = @(
    'fixed_zoom_renderer.log',
    'fixed_zoom_input.log',
    'fixed_zoom_tooltip.log',
    'fixed_zoom_cursor_hover.log',
    'fixed_zoom_first_frame.raw',
    'fixed_zoom_first_frame.txt',
    'fixed_zoom_presentation.log',
    'replay_color_capture.txt',
    'replay_color_outer_stock.raw',
    'replay_color_private_0.raw',
    'replay_color_expanded_world.raw',
    'worker_ai_capture.log',
    'worker_ai_repair.log',
    'ai_production_capture.log',
    'ai_production_repair.log'
)
foreach ($marker in $diagnosticMarkers) {
    if ($rendererText.Contains($marker)) {
        throw "Diagnostic marker remains in renderer payload: $marker"
    }
}
Write-Output 'Release diagnostic audit: PASS'

& python (Join-Path $repository 'ZoomIntegration\verify_fixed_zoom.py')
if ($LASTEXITCODE -ne 0) {
    throw 'Offline resolution geometry verification failed'
}

& python (Join-Path $repository 'ZoomIntegration\verify_menu_scaler.py')
if ($LASTEXITCODE -ne 0) {
    throw 'Menu scaler verification failed'
}

& dotnet run --project (Join-Path $repository `
    'ViewportConfigurator.Tests\ViewportConfigurator.Tests.csproj') -c Release
if ($LASTEXITCODE -ne 0) {
    throw 'Configurator owned INI regression tests failed'
}

New-Item -ItemType Directory -Path $output -Force | Out-Null
& dotnet publish (Join-Path $configurator 'CosmonarchyWidescreenSettings.csproj') `
    -c Release -r win-x64 --self-contained true `
    -p:PublishSingleFile=true -o $output
if ($LASTEXITCODE -ne 0) {
    throw 'Configurator publish failed'
}

$executable = Join-Path $output 'Cosmonarchy Widescreen Settings.exe'
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
    throw "Published executable is missing: $executable"
}
$hash = (Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash
$checksum = "$hash *Cosmonarchy Widescreen Settings.exe`n"
[System.IO.File]::WriteAllText(
    (Join-Path $output 'SHA256SUMS.txt'), $checksum,
    [System.Text.UTF8Encoding]::new($false))

$expectedBundleRoot = [System.IO.Path]::GetFullPath($bundleRoot).TrimEnd(
    [System.IO.Path]::DirectorySeparatorChar) +
    [System.IO.Path]::DirectorySeparatorChar
$resolvedBundleDirectory = [System.IO.Path]::GetFullPath($bundleDirectory)
if (-not $resolvedBundleDirectory.StartsWith(
        $expectedBundleRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Unexpected bundle directory: $resolvedBundleDirectory"
}
if (Test-Path -LiteralPath $bundleDirectory) {
    [System.IO.Directory]::Delete($bundleDirectory, $true)
}
New-Item -ItemType Directory -Path $bundleDirectory -Force | Out-Null

Copy-Item -LiteralPath $executable -Destination $bundleDirectory
Copy-Item -LiteralPath (Join-Path $output 'SHA256SUMS.txt') `
    -Destination $bundleDirectory
Copy-Item -LiteralPath (Join-Path $repository 'RELEASE-README.txt') `
    -Destination (Join-Path $bundleDirectory 'README.txt')
Copy-Item -LiteralPath (Join-Path $repository 'LICENSE') `
    -Destination (Join-Path $bundleDirectory 'LICENSE.txt')
Copy-Item -LiteralPath (Join-Path $repository 'THIRD-PARTY-NOTICES.md') `
    -Destination (Join-Path $bundleDirectory 'THIRD-PARTY-NOTICES.txt')

if (Test-Path -LiteralPath $bundleArchive) {
    Remove-Item -LiteralPath $bundleArchive -Force
}
Compress-Archive -LiteralPath $bundleDirectory -DestinationPath $bundleArchive `
    -CompressionLevel Optimal

$bundleHash = (Get-FileHash -LiteralPath $bundleArchive -Algorithm SHA256).Hash
$releaseChecksums = @(
    "$hash *Cosmonarchy Widescreen Settings.exe"
    "$bundleHash *$bundleName.zip"
) -join "`n"
[System.IO.File]::WriteAllText(
    (Join-Path $output 'SHA256SUMS.txt'), "$releaseChecksums`n",
    [System.Text.UTF8Encoding]::new($false))

Write-Output "Release build: PASS"
Write-Output "Executable: $executable"
Write-Output "SHA-256: $hash"
Write-Output "Bundle: $bundleArchive"
Write-Output "Bundle SHA-256: $bundleHash"
