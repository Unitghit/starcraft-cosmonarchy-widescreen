param(
    [switch]$VerifyOnly
)

$ErrorActionPreference = 'Stop'
$workspace = Split-Path -Parent $PSScriptRoot
$resolutionPath = Join-Path $workspace 'ZoomSource\zoom_resolution.h'
$presentationPath = Join-Path $workspace 'ZoomSource\zoom_presentation.h'
$ddrawPath = Join-Path $workspace 'Starcraft\ddraw.ini'

function Read-Constant([string]$path, [string]$name) {
    $text = Get-Content -LiteralPath $path -Raw
    $match = [regex]::Match(
        $text,
        "constexpr\s+int\s+$([regex]::Escape($name))\s*=\s*(\d+)\s*;")
    if (-not $match.Success) {
        throw "Could not read $name from $path"
    }
    return [int]$match.Groups[1].Value
}

$logicalWidth = Read-Constant $resolutionPath 'screen_width'
$logicalHeight = Read-Constant $resolutionPath 'screen_height'
$scaleNumerator = Read-Constant $presentationPath 'scale_numerator'
$scaleDenominator = Read-Constant $presentationPath 'scale_denominator'
if ($scaleNumerator -lt 1 -or $scaleDenominator -lt 1) {
    throw 'Presentation scale numerator and denominator must both be positive'
}
if (($logicalWidth * $scaleNumerator) % $scaleDenominator -ne 0 -or
    ($logicalHeight * $scaleNumerator) % $scaleDenominator -ne 0) {
    throw 'Presentation scale does not produce integral client dimensions'
}
$clientWidth = $logicalWidth * $scaleNumerator / $scaleDenominator
$clientHeight = $logicalHeight * $scaleNumerator / $scaleDenominator
$scale = [decimal]$scaleNumerator / [decimal]$scaleDenominator

$required = [ordered]@{
    width = "$clientWidth"
    height = "$clientHeight"
    fullscreen = 'false'
    windowed = 'true'
    maintas = 'true'
    # Fractional presentation scales must use the complete client. cnc-ddraw's
    # boxing option means integer scaling and would reduce 2.5x back to a
    # centered 2x raster with large black borders.
    boxing = 'false'
    adjmouse = 'true'
    shader = 'Shaders\nearest-neighbor.glsl'
    posX = '-32000'
    posY = '-32000'
    savesettings = '0'
}

$lines = Get-Content -LiteralPath $ddrawPath
$inDdraw = $false
$seen = @{}
$mismatches = [System.Collections.Generic.List[string]]::new()
for ($index = 0; $index -lt $lines.Count; ++$index) {
    if ($lines[$index] -match '^\s*\[ddraw\]\s*$') {
        $inDdraw = $true
        continue
    }
    if ($inDdraw -and $lines[$index] -match '^\s*\[') {
        break
    }
    if ($inDdraw -and $lines[$index] -match '^\s*([^;=]+)=(.*)$') {
        $key = $Matches[1].Trim()
        if ($required.Contains($key)) {
            $actual = $Matches[2].Trim()
            $expected = $required[$key]
            $seen[$key] = $true
            if ($actual -ne $expected) {
                $mismatches.Add("$key=$actual (expected $expected)")
                if (-not $VerifyOnly) {
                    $lines[$index] = "$key=$expected"
                }
            }
        }
    }
}

$missing = @($required.Keys | Where-Object { -not $seen.ContainsKey($_) })
if ($missing.Count -ne 0) {
    throw "Missing top-level ddraw keys: $($missing -join ', ')"
}

if ($VerifyOnly) {
    if ($mismatches.Count -ne 0) {
        throw "Presentation runtime is out of sync: $($mismatches -join '; ')"
    }
} elseif ($mismatches.Count -ne 0) {
    Set-Content -LiteralPath $ddrawPath -Value $lines
}

Write-Output (
    "Presentation runtime synchronized: logical={0}x{1} scale={2} output={3}x{4}" -f
    $logicalWidth, $logicalHeight, $scale, $clientWidth, $clientHeight)
