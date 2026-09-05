# Test-only, per-application Mesa deployment. Never installs a system driver
# or puts a DLL in the game, configurator payload, or release bundle.
$ErrorActionPreference = 'Stop'
$repository = Split-Path -Parent $PSScriptRoot
$cache = Join-Path $repository 'artifacts\ci-graphics-download'
$output = Join-Path $repository 'artifacts\single-stage-tests'
$archive = Join-Path $cache 'mesa3d-26.2.0-release-msvc.7z'
$expectedHash = 'DCB2719EF346DAB5B609FCB193A5F13CFC4B0502E3F4DE1AD43D349477402F47'
$url = 'https://github.com/pal1000/mesa-dist-win/releases/download/26.2.0/mesa3d-26.2.0-release-msvc.7z'
New-Item -ItemType Directory -Path $cache, $output -Force | Out-Null
if (-not (Test-Path -LiteralPath $archive)) {
    Invoke-WebRequest -Uri $url -OutFile $archive
}
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $expectedHash) {
    throw 'Mesa test-driver archive checksum mismatch.'
}
$sevenZip = (Get-Command 7z.exe -ErrorAction SilentlyContinue).Source
if (-not $sevenZip) { $sevenZip = Join-Path $env:ProgramFiles '7-Zip\7z.exe' }
if (-not (Test-Path -LiteralPath $sevenZip)) { throw '7-Zip is required to extract the test driver.' }
# Extract only the x86 WGL entry point and software rasterizer dependency.
& $sevenZip e $archive "-o$output" 'x86\opengl32.dll' 'x86\libgallium_wgl.dll' -y
if ($LASTEXITCODE -ne 0) { throw 'Mesa test-driver extraction failed.' }
foreach ($name in @('opengl32.dll', 'libgallium_wgl.dll')) {
    if (-not (Test-Path -LiteralPath (Join-Path $output $name))) {
        throw "Missing Mesa test driver: $name"
    }
}
Write-Output 'Mesa 26.2.0 x86 installed beside the graphics test executable only.'
