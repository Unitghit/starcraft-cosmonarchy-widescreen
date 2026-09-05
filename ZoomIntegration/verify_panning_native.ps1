$ErrorActionPreference = 'Stop'
$repository = Split-Path -Parent $PSScriptRoot
$draw = Get-Content (Join-Path $repository 'ZoomSource\Cosmonarchy-aidebug-resolution\src\draw.cpp') -Raw
if ($draw -match 'previous_world_frame|RepairMovingFullWidthPassEdges') {
    throw 'Temporal world seam repair must not return to production'
}
if ([regex]::Matches($draw, 'resolution::PlanWorldPassX\(').Count -ne 2) {
    throw 'Both world composition paths must use the tested crop planner'
}
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installation) { throw 'Visual Studio x86 C++ tools required' }
$vcvars = Join-Path $installation 'VC\Auxiliary\Build\vcvars32.bat'
$output = Join-Path $repository 'artifacts\panning-tests'
New-Item -ItemType Directory -Path $output -Force | Out-Null
$source = Join-Path $PSScriptRoot 'verify_panning_native.cpp'
$exe = Join-Path $output 'panning_tests.exe'
$obj = Join-Path $output 'panning_tests.obj'
& cmd /d /s /c "`"$vcvars`" >nul && cl /nologo /std:c++20 /EHsc /O2 /D_CRT_SECURE_NO_WARNINGS `"$source`" /Fe:`"$exe`" /Fo:`"$obj`""
if ($LASTEXITCODE -ne 0) { throw 'Panning test compile failed' }
& $exe
if ($LASTEXITCODE -ne 0) { throw 'Panning regression test failed' }
