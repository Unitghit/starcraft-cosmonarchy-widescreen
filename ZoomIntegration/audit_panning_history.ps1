$ErrorActionPreference = 'Stop'
$repository = Split-Path -Parent $PSScriptRoot
$testPath = Join-Path $PSScriptRoot 'audit_panning_history.cpp'
$test = Get-Content $testPath -Raw
$excerpt = [regex]::Match($test, '(?s)void RepairMovingFullWidthPassEdges.*?(?=// End production excerpt)').Value.Trim().Replace("`r`n", "`n")
$sha = [System.Security.Cryptography.SHA256]::Create()
$fingerprint = [BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($excerpt))).Replace('-', '')
$sha.Dispose()
if ($fingerprint -ne '958CD094914917FDEB88926CC3872BC421A663F17410F945D31217B3FF3C8329') {
    throw 'Archived pre-fix function differs from the characterized baseline'
}
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installation) { throw 'Visual Studio x86 C++ tools required' }
$vcvars = Join-Path $installation 'VC\Auxiliary\Build\vcvars32.bat'
$output = Join-Path $repository 'artifacts\panning-audit'
New-Item -ItemType Directory -Path $output -Force | Out-Null
$exe = Join-Path $output 'panning_history.exe'
$obj = Join-Path $output 'panning_history.obj'
& cmd /d /s /c "`"$vcvars`" >nul && cl /nologo /std:c++20 /EHsc /O2 /D_CRT_SECURE_NO_WARNINGS `"$testPath`" /Fe:`"$exe`" /Fo:`"$obj`""
if ($LASTEXITCODE -ne 0) { throw 'Panning audit compile failed' }
& $exe
if ($LASTEXITCODE -ne 0) { throw 'Panning characterization changed' }
