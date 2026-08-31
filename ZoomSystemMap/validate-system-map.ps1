$ErrorActionPreference = 'Stop'

$mapRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$workspace = Split-Path -Parent $mapRoot
$rendererSource = Join-Path $workspace 'ZoomSource\Cosmonarchy-aidebug-resolution'
$installRoot = Join-Path $workspace 'Release'
if (-not (Test-Path -LiteralPath $installRoot -PathType Container)) {
    $adjacentInstall = Join-Path (Split-Path -Parent $workspace) 'Release'
    if (Test-Path -LiteralPath $adjacentInstall -PathType Container) {
        $installRoot = $adjacentInstall
    }
}
$stableGptp = Join-Path $installRoot 'plugins\gptp.qdp'
$stableGptpBackup = Join-Path $workspace 'ZoomIntegration\backups\gptp.pre_fixed_zoom.qdp'
$geometryVerifier = Join-Path $workspace 'ZoomIntegration\verify_fixed_zoom.py'
$expectedGptpHash = 'CC6BF422B4DC6174EC6B002ACAE12A826D61CBF144661FE0C4F9E3687664BB99'

$requiredDocuments = @(
    'README.md',
    'architecture\overview.md',
    'architecture\rendering-pipeline.md',
    'architecture\input-pipeline.md',
    'architecture\coordinate-spaces.md',
    'components\modules.md',
    'features\status.md',
    'features\building-placement.md',
    'features\cursor-hover.md',
    'features\legacy-hud-tooltips.md',
    'features\positional-audio.md',
    'features\presentation-scaling.md',
    'features\portable-configurator.md',
    'reverse-engineering\functions-and-globals.md',
    'reverse-engineering\draw-layers-and-structures.md',
    'reverse-engineering\binary-patches.md',
    'invariants\native-vs-expanded.md',
    'diagnostics\tools-and-artifacts.md',
    'methodology\change-workflow.md',
    'methodology\subsystem-template.md',
    'testing\regression-checklist.md',
    'testing\resolution-matrix.md'
)

foreach ($relative in $requiredDocuments) {
    $path = Join-Path $mapRoot $relative
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Missing system-map document: $relative"
    }
}

$brokenLinks = @()
$markdownFiles = Get-ChildItem -LiteralPath $mapRoot -Filter '*.md' -Recurse
foreach ($file in $markdownFiles) {
    $text = Get-Content -LiteralPath $file.FullName -Raw
    foreach ($match in [regex]::Matches($text, '\]\(([^)#]+)(?:#[^)]+)?\)')) {
        $target = $match.Groups[1].Value
        if ($target -match '^[a-z]+:' -or [System.IO.Path]::IsPathRooted($target)) {
            continue
        }
        $resolved = [System.IO.Path]::GetFullPath(
            (Join-Path $file.DirectoryName $target))
        if (-not (Test-Path -LiteralPath $resolved)) {
            $brokenLinks += "$($file.FullName): $target"
        }
    }
}
if ($brokenLinks.Count -ne 0) {
    throw "Broken system-map links:`n$($brokenLinks -join "`n")"
}

foreach ($gptpPath in @($stableGptp, $stableGptpBackup)) {
    if (-not (Test-Path -LiteralPath $gptpPath -PathType Leaf)) {
        throw "Missing stable GPTP: $gptpPath"
    }
    $hash = (Get-FileHash -LiteralPath $gptpPath -Algorithm SHA256).Hash
    if ($hash -ne $expectedGptpHash) {
        throw "Unexpected GPTP hash at $gptpPath`: $hash"
    }
}

$limitsSource = Join-Path $rendererSource 'src\limits.cpp'
$drawSource = Join-Path $rendererSource 'src\draw.cpp'
$inputSource = Join-Path $rendererSource 'src\scconsole.cpp'
$presentationSource = Join-Path $rendererSource 'src\presentation.cpp'
$presentationHeader = Join-Path $rendererSource 'src\presentation.h'
$resolutionHeader = Join-Path $rendererSource 'src\resolution.h'
$mainPatchSource = Join-Path $rendererSource 'src\mainpatch.cpp'
$configuratorSource = Join-Path $workspace 'ViewportConfigurator\ConfigurationService.cs'
$profileManifest = Join-Path $workspace 'ViewportConfigurator\compatibility-manifest.json'
$requiredSourcePatterns = @(
    @{ Path = $limitsSource; Pattern = 'function_rva = 0x00087BF0' },
    @{ Path = $limitsSource; Pattern = 'EnsureGptpPlacementBounds' },
    @{ Path = $limitsSource; Pattern = 'EnsureGptpCursorHoverBounds' },
    @{ Path = $limitsSource; Pattern = 'sequence_rva = 0x000675B8' },
    @{ Path = $limitsSource; Pattern = 'PatchLegacyHudTooltipHitTests' },
    @{ Path = $limitsSource; Pattern = '0x00457E10' },
    @{ Path = $limitsSource; Pattern = '0x00457E50' },
    @{ Path = $limitsSource; Pattern = '0x00458015' },
    @{ Path = $limitsSource; Pattern = '0x00459796' },
    @{ Path = $limitsSource; Pattern = '0x00459825' },
    @{ Path = $limitsSource; Pattern = '0x00459870' },
    @{ Path = $limitsSource; Pattern = '0x004A5459' },
    @{ Path = $limitsSource; Pattern = '0x004A54BF' },
    @{ Path = $limitsSource; Pattern = 'PatchPositionalAudioViewport' },
    @{ Path = $limitsSource; Pattern = '0x0048E8D0' },
    @{ Path = $limitsSource; Pattern = 'PatchPortraitCameraOrigins' },
    @{ Path = $limitsSource; Pattern = '0x0045E3A0' },
    @{ Path = $limitsSource; Pattern = '0x0045EE4B' },
    @{ Path = $inputSource; Pattern = "Cosmonarchy's status portrait callback" },
    @{ Path = $inputSource; Pattern = 'world_x - 320' },
    @{ Path = $inputSource; Pattern = 'resolution::camera_center_x' },
    @{ Path = $limitsSource; Pattern = 'EnsureGptpUpgradeResearchClear' },
    @{ Path = $limitsSource; Pattern = 'sequence_address = 0x004CCEBD' },
    @{ Path = $drawSource; Pattern = 'saved_placement_left' },
    @{ Path = $drawSource; Pattern = 'saved_placement_top' },
    @{ Path = $drawSource; Pattern = 'DrawLayer &layer = bw::draw_layers[3 + box]' }
    @{ Path = $presentationSource; Pattern = 'settings.client_width = configured_width' },
    @{ Path = $presentationSource; Pattern = 'fixed_zoom_presentation.log' }
    @{ Path = $presentationSource; Pattern = 'cosmonarchy_viewport.ini' }
    @{ Path = $configuratorSource; Pattern = 'AtomicWrite' }
    @{ Path = $configuratorSource; Pattern = 'GptpPath' }
    @{ Path = $configuratorSource; Pattern = 'rendererPayloads' }
    @{ Path = $profileManifest; Pattern = '"aspectRatio": "16:9"' }
    @{ Path = $profileManifest; Pattern = '"aspectRatio": "4:3"' }
    @{ Path = $profileManifest; Pattern = '"aspectRatio": "Custom"' }
    @{ Path = $profileManifest; Pattern = '"width": 3840' }
    @{ Path = $resolutionHeader; Pattern = 'maximum_screen_width = 3840' }
    @{ Path = $resolutionHeader; Pattern = 'maximum_screen_height = 2160' }
    @{ Path = $resolutionHeader; Pattern = 'inline bool Configure' }
    @{ Path = $resolutionHeader; Pattern = 'top_ui_uses_screen_edges = use_screen_edges' }
    @{ Path = $mainPatchSource; Pattern = 'ConfigureRuntimeResolution' }
    @{ Path = $mainPatchSource; Pattern = 'resolution::Configure' }
    @{ Path = $mainPatchSource; Pattern = 'top_ui_layout' }
    @{ Path = $configuratorSource; Pattern = 'top_ui_layout=' }
)
foreach ($check in $requiredSourcePatterns) {
    if (-not (Select-String -LiteralPath $check.Path -SimpleMatch $check.Pattern -Quiet)) {
        throw "Mapped source anchor missing: $($check.Pattern) in $($check.Path)"
    }
}

& python $geometryVerifier
if ($LASTEXITCODE -ne 0) {
    throw 'Resolution geometry verification failed'
}

$rendererInstall = Join-Path $installRoot 'plugins\aize_debug.qdp'
$viewportConfig = Join-Path $installRoot 'cosmonarchy_viewport.ini'
$configuredWidth = 1280
$configuredHeight = 720
if (Test-Path -LiteralPath $viewportConfig -PathType Leaf) {
    $configText = Get-Content -LiteralPath $viewportConfig -Raw
    if ($configText -match '(?m)^internal_width=(\d+)\s*$') {
        $configuredWidth = [int]$Matches[1]
    }
    if ($configText -match '(?m)^internal_height=(\d+)\s*$') {
        $configuredHeight = [int]$Matches[1]
    }
}
$rendererBuild = Join-Path $workspace `
    'ViewportConfigurator\Payloads\1280x720\aize_debug.qdp'
if (-not (Test-Path -LiteralPath $rendererBuild -PathType Leaf)) {
    throw 'Universal renderer payload is missing'
}
$buildHash = (Get-FileHash -LiteralPath $rendererBuild -Algorithm SHA256).Hash
$installHash = (Get-FileHash -LiteralPath $rendererInstall -Algorithm SHA256).Hash
if ($buildHash -ne $installHash) {
    throw 'Installed renderer does not match the universal payload'
}

Write-Output "System map: PASS ($($markdownFiles.Count) documents, no broken links)"
Write-Output "Stable GPTP: PASS ($expectedGptpHash)"
Write-Output "Universal renderer $($configuredWidth)x$($configuredHeight) config/build/install: PASS ($buildHash)"
