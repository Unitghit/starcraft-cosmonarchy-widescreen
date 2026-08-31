$ErrorActionPreference = 'Stop'

$repository = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$configurator = Join-Path $repository 'ViewportConfigurator'
$output = Join-Path $repository 'artifacts\release'

& (Join-Path $configurator 'build-profile-pack.ps1')
if ($LASTEXITCODE -ne 0) {
    throw 'Universal renderer build failed'
}

& python (Join-Path $repository 'ZoomIntegration\verify_fixed_zoom.py')
if ($LASTEXITCODE -ne 0) {
    throw 'Offline resolution geometry verification failed'
}

New-Item -ItemType Directory -Path $output -Force | Out-Null
& dotnet publish (Join-Path $configurator 'CosmonarchyWidescreenSettings.csproj') `
    -c Release -r win-x64 --self-contained true `
    -p:PublishSingleFile=true -o $output
if ($LASTEXITCODE -ne 0) {
    throw 'Configurator publish failed'
}

$executable = Join-Path $output 'Cosmonarchy Widescreen Settings.exe'
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
    throw "Published executable is missing: $executable"
}
$hash = (Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash
$checksum = "$hash *Cosmonarchy Widescreen Settings.exe`n"
[System.IO.File]::WriteAllText(
    (Join-Path $output 'SHA256SUMS.txt'), $checksum,
    [System.Text.UTF8Encoding]::new($false))

Write-Output "Release build: PASS"
Write-Output "Executable: $executable"
Write-Output "SHA-256: $hash"
