param(
    [ValidateRange(1.0, 4.0)]
    [decimal]$Scale = 2.0
)

$ErrorActionPreference = 'Stop'
$workspace = Split-Path -Parent $PSScriptRoot
$presentationPath = Join-Path $workspace 'ZoomSource\zoom_presentation.h'

function Get-GreatestCommonDivisor([int]$left, [int]$right) {
    while ($right -ne 0) {
        $remainder = $left % $right
        $left = $right
        $right = $remainder
    }
    return [Math]::Abs($left)
}

$scaleDenominator = 1000
$scaleNumerator = [int][Math]::Round(
    [double]($Scale * $scaleDenominator),
    [MidpointRounding]::AwayFromZero)
$divisor = Get-GreatestCommonDivisor $scaleNumerator $scaleDenominator
$scaleNumerator = [int]($scaleNumerator / $divisor)
$scaleDenominator = [int]($scaleDenominator / $divisor)

$presentationText = Get-Content -LiteralPath $presentationPath -Raw
$presentationText = [regex]::Replace(
    $presentationText,
    'constexpr int scale_numerator = \d+;',
    "constexpr int scale_numerator = $scaleNumerator;")
$presentationText = [regex]::Replace(
    $presentationText,
    'constexpr int scale_denominator = \d+;',
    "constexpr int scale_denominator = $scaleDenominator;")
Set-Content -LiteralPath $presentationPath -Value $presentationText -NoNewline
& (Join-Path $PSScriptRoot 'sync-presentation-runtime.ps1')
Write-Output (
    "Presentation scale source configured: scale={0} ({1}/{2})" -f
    $Scale, $scaleNumerator, $scaleDenominator)
Write-Output 'Rebuild aidebug and restart Cosmonarchy to apply the change.'
