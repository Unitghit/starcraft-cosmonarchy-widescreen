$ErrorActionPreference = 'Stop'
$repository = Split-Path -Parent $PSScriptRoot
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installation) { throw 'Visual Studio x86 C++ tools are required' }
$vcvars = Join-Path $installation 'VC\Auxiliary\Build\vcvars32.bat'
$output = Join-Path $repository 'artifacts\ui-scale-tests'
New-Item -ItemType Directory -Path $output -Force | Out-Null
$source = Join-Path $PSScriptRoot 'verify_ui_scale.cpp'
$executable = Join-Path $output 'ui_scale_tests.exe'
$object = Join-Path $output 'ui_scale_tests.obj'
& cmd /d /s /c "`"$vcvars`" >nul && cl /nologo /std:c++20 /EHsc /O2 `"$source`" /Fe:`"$executable`" /Fo:`"$object`""
if ($LASTEXITCODE -ne 0) { throw 'HUD sizing test compilation failed' }
& $executable
if ($LASTEXITCODE -ne 0) { throw 'HUD sizing regression tests failed' }
