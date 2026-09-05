$ErrorActionPreference='Stop'
$repository=Split-Path -Parent $PSScriptRoot
& (Join-Path $PSScriptRoot 'build-single-stage-shader.ps1')
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $installation 'VC\Auxiliary\Build\vcvars32.bat'
$output=Join-Path $repository 'artifacts\single-stage-tests'
New-Item -ItemType Directory -Path $output -Force | Out-Null
$source=Join-Path $PSScriptRoot 'verify_single_stage.cpp'
$adapter=Join-Path $repository 'ZoomSource\Cosmonarchy-aidebug-resolution\src\single_stage.cpp'
$exe=Join-Path $output 'single_stage_tests.exe'
$obj=Join-Path $output 'single_stage_tests.obj'
& cmd /d /s /c "`"$vcvars`" >nul && cl /nologo /std:c++20 /EHsc /O2 /DSINGLE_STAGE_TEST `"$source`" `"$adapter`" /Fe:`"$exe`" /Fo:`"$output\\`" /link user32.lib"
if($LASTEXITCODE -ne 0){throw 'Single-stage test compilation failed'}
& $exe
if($LASTEXITCODE -ne 0){throw 'Single-stage GPU/reference test failed'}
