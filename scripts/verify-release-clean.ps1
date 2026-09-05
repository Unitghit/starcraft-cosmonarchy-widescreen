param(
    [string]$Payload,
    [string]$BundleDirectory
)

$ErrorActionPreference = 'Stop'
$repository = Split-Path -Parent $PSScriptRoot
$sourceRoot = Join-Path $repository 'ZoomSource\Cosmonarchy-aidebug-resolution'
if (-not $Payload) {
    $Payload = Join-Path $repository 'ViewportConfigurator\Payloads\1280x720\aize_debug.qdp'
}
$header = Get-Content -LiteralPath (Join-Path $sourceRoot 'src\runtime_diagnostics.h') -Raw
if ($header -notmatch '(?m)^#define\s+RUNTIME_DIAGNOSTICS_ENABLED\s+0\s*$' -or
    $header -notmatch 'constexpr\s+bool\s+Enabled\s*\(\s*\)\s*\{\s*return\s+false\s*;') {
    throw 'Runtime diagnostic gates must both be disabled.'
}
[xml]$project = Get-Content -LiteralPath (Join-Path $sourceRoot 'teippi.vcxproj') -Raw
$release = @($project.Project.ItemDefinitionGroup | Where-Object Condition -Match "Release\|Win32")
if ($release.Count -ne 1) { throw 'Cannot identify the Release build configuration.' }
$defines = [string]$release[0].ClCompile.PreprocessorDefinitions
if ($defines -match '(^|;)(DEBUG|_DEBUG|CONSOLE|\w+_TEST)(=|;|$)') {
    throw "Test or debug definition in Release: $defines"
}

# Check actual compiled bytes, not only source switches. Broad historical
# prefixes also catch newly added capture paths that a fixed list might miss.
$bytes = [System.IO.File]::ReadAllBytes($Payload)
$texts = @([System.Text.Encoding]::ASCII.GetString($bytes),
    [System.Text.Encoding]::Unicode.GetString($bytes))
$forbidden = @(
    'fixed_zoom_', 'replay_color_capture', 'replay_color_outer_stock',
    'replay_color_private_', 'replay_color_expanded_world',
    'worker_ai_capture', 'worker_ai_repair',
    'ai_production_capture', 'ai_production_repair',
    'VIEWPORT_LAYOUT_CAPTURE', 'TestCaptureWorld', 'post-render trace:'
)
foreach ($marker in $forbidden) {
    foreach ($text in $texts) {
        if ($text.Contains($marker)) { throw "Forbidden payload marker: $marker" }
    }
}
# The configurator embeds exactly one game binary: the viewport renderer.
[xml]$gui = Get-Content -LiteralPath (Join-Path $repository `
    'ViewportConfigurator\CosmonarchyWidescreenSettings.csproj') -Raw
$embeddedBinaries = @($gui.Project.ItemGroup.EmbeddedResource | Where-Object {
    $_.Include -match '\.(qdp|dll|exe)$'
})
if ($embeddedBinaries.Count -ne 1 -or
    $embeddedBinaries[0].Include -ne 'Payloads\1280x720\aize_debug.qdp') {
    throw 'Unexpected binary embedded in the configurator.'
}
if ($BundleDirectory) {
    $allowed = @('Cosmonarchy Widescreen Settings.exe', 'SHA256SUMS.txt',
        'README.txt', 'LICENSE.txt', 'THIRD-PARTY-NOTICES.txt')
    $entries = @(Get-ChildItem -LiteralPath $BundleDirectory -Force)
    if ($entries.Count -ne $allowed.Count) { throw 'Unexpected bundle entry count.' }
    foreach ($entry in $entries) {
        if ($entry.PSIsContainer -or $entry.Name -notin $allowed) {
            throw "Unexpected bundle entry: $($entry.Name)"
        }
    }
}
Write-Output "Release cleanliness: PASS ($((Get-FileHash -LiteralPath $Payload -Algorithm SHA256).Hash))"
