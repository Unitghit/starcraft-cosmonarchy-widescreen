$ErrorActionPreference = 'Stop'
$repository = Split-Path -Parent $PSScriptRoot
$source = Join-Path $repository 'ZoomSource\Cosmonarchy-aidebug-resolution\src'
$kits = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin'
$fxc = Get-ChildItem -LiteralPath $kits -Directory | Sort-Object Name -Descending |
    ForEach-Object { Join-Path $_.FullName 'x86\fxc.exe' } |
    Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
if (-not $fxc) { throw 'Windows SDK shader compiler not found' }
& $fxc /nologo /T ps_3_0 /E main /O3 /Vn single_stage_shader /Fh (Join-Path $source 'single_stage_shader.h') (Join-Path $source 'single_stage.hlsl')
if ($LASTEXITCODE -ne 0) { throw 'Single-stage shader compilation failed' }
