# Builds the ClayTennis editor module with the UE 5.5.4 toolchain.
$ErrorActionPreference = 'Stop'
$engine = 'C:\UE_5.5'
$project = 'C:\Projects\ClayTennis\ClayTennis.uproject'
$log = Join-Path (Split-Path $project) 'RawAssets\build_last.log'

& "$engine\Engine\Build\BatchFiles\Build.bat" ClayTennisEditor Win64 Development `
    -Project="$project" -WaitMutex *> $log

$code = $LASTEXITCODE
Write-Host "exit code: $code"
Select-String -Path $log -Pattern ': error|: fatal error|LINK :' | ForEach-Object { $_.Line }
if ($code -ne 0) {
    Get-Content $log -Tail 20
}
exit $code
