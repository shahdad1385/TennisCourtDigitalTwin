param(
    [Parameter(Mandatory = $true)][string]$OutName
)
$ErrorActionPreference = 'Continue'
$proj = 'C:\Projects\ClayTennis\ClayTennis.uproject'
$shots = 'C:\Projects\ClayTennis\Saved\Screenshots'
$cfgPath = 'C:\Projects\ClayTennis\RawAssets\shot_cfg.json'

if (-not (Test-Path $cfgPath)) {
    throw "missing cfg: $cfgPath"
}

Get-Process UnrealEditor -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 3
Get-ChildItem $shots -Recurse -File -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue

Start-Process -FilePath 'C:\UE_5.5\Engine\Binaries\Win64\UnrealEditor.exe' -ArgumentList @(
    $proj, '-ExecutePythonScript=C:\Projects\ClayTennis\Tools\shot_config.py',
    '-nosplash', '-nopause', '-windowed', '-ResX=1600', '-ResY=900'
) | Out-Null

$found = $null
for ($i = 0; $i -lt 60; $i++) {
    Start-Sleep -Seconds 4
    $f = Get-ChildItem $shots -Recurse -File -Filter '*.png' -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if ($f -and $f.Length -gt 20000) { Start-Sleep -Seconds 2; $found = $f; break }
}
if ($found) {
    Copy-Item $found.FullName ("C:\Projects\ClayTennis\RawAssets\" + $OutName) -Force
    Write-Host ("saved " + $OutName + " " + $found.Length)
} else {
    Write-Host 'no screenshot produced'
}
Get-Process UnrealEditor -ErrorAction SilentlyContinue | Stop-Process -Force
