$ErrorActionPreference = 'Stop'
$output = Join-Path $PSScriptRoot 'live_modules_expander.txt'
$process = Get-Process -Name StarCraft -ErrorAction Stop
$process.Modules |
    Select-Object ModuleName, FileName, BaseAddress, ModuleMemorySize |
    Format-Table -AutoSize |
    Out-File -LiteralPath $output -Width 260
