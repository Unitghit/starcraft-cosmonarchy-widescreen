$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$workspace = Split-Path -Parent $projectRoot
$rendererRoot = Join-Path $workspace 'ZoomSource\Cosmonarchy-aidebug-resolution'
$project = Join-Path $rendererRoot 'teippi.vcxproj'
$vswhere = Join-Path ${env:ProgramFiles(x86)} `
    'Microsoft Visual Studio\Installer\vswhere.exe'
$msbuild = if (Test-Path -LiteralPath $vswhere -PathType Leaf) {
    & $vswhere -latest -products * -requires Microsoft.Component.MSBuild `
        -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
} else {
    $null
}
if (-not $msbuild) {
    $msbuild = 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe'
}
$payloadRoot = Join-Path $projectRoot 'Payloads'
$objectRoot = Join-Path $workspace 'ZoomSource\profile-obj\universal'
$output = Join-Path $payloadRoot '1280x720'

if (-not (Test-Path -LiteralPath $msbuild -PathType Leaf)) {
    throw "MSBuild was not found at $msbuild"
}

& $msbuild $project /t:Rebuild /v:minimal `
    /p:Configuration=Release /p:Platform=Win32 `
    /p:ViewportWidth=1280 /p:ViewportHeight=720 `
    "/p:OutDir=$output\" "/p:IntDir=$objectRoot\" `
    /p:PostBuildEventUseInBuild=false /m
if ($LASTEXITCODE -ne 0) {
    throw 'Universal renderer build failed'
}

$payload = Join-Path $output 'aize_debug.qdp'
if (-not (Test-Path -LiteralPath $payload -PathType Leaf)) {
    throw "Universal renderer output is missing: $payload"
}
$hash = (Get-FileHash -LiteralPath $payload -Algorithm SHA256).Hash
Write-Output "Universal 640x480..3840x2160 $hash"
