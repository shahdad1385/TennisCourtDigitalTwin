$ErrorActionPreference = 'Continue'
$log = 'C:\Projects\ClayTennis\RawAssets\capture2.txt'
Set-Content -Path $log -Value ("start " + (Get-Date -Format 'HH:mm:ss'))

if (Get-Process UnrealEditor -ErrorAction SilentlyContinue) {
    Get-Process UnrealEditor -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Seconds 5
}

Start-Process -FilePath 'C:\UE_5.5\Engine\Binaries\Win64\UnrealEditor.exe' -ArgumentList @(
    'C:\Projects\ClayTennis\ClayTennis.uproject', '-game', '-windowed', '-ResX=1600', '-ResY=900', '-nosplash'
) | Out-Null

Add-Type -AssemblyName System.Drawing
$code = @'
using System;
using System.Runtime.InteropServices;
public class WShot {
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h,int n);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  public struct RECT { public int L,T,R,B; }
}
'@
Add-Type -TypeDefinition $code -ErrorAction SilentlyContinue

function Capture([string]$file) {
    $p = Get-Process UnrealEditor -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 -and $_.MainWindowTitle -like '*ClayTennis*' } | Select-Object -First 1
    if (-not $p) { Add-Content $log 'no game window yet'; return $false }
    $h = $p.MainWindowHandle
    [WShot]::ShowWindow($h, 9) | Out-Null
    [WShot]::SetForegroundWindow($h) | Out-Null
    Start-Sleep -Seconds 2
    $r = New-Object WShot+RECT
    [WShot]::GetWindowRect($h, [ref]$r) | Out-Null
    $w = $r.R - $r.L; $hh = $r.B - $r.T
    if ($w -lt 200 -or $hh -lt 200) { Add-Content $log ("bad rect " + $w + "x" + $hh); return $false }
    $bmp = New-Object System.Drawing.Bitmap($w, $hh)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($r.L, $r.T, 0, 0, $bmp.Size)
    $bmp.Save($file, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    Add-Content $log ("saved " + $file + " " + $w + "x" + $hh)
    return $true
}

$ok = $false
for ($i = 0; $i -lt 120; $i++) {
    Start-Sleep -Seconds 5
    $p = Get-Process UnrealEditor -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 -and $_.MainWindowTitle -like '*ClayTennis*' }
    if ($p) { $ok = $true; Add-Content $log ("window after " + (($i + 1) * 5) + "s"); break }
}
if (-not $ok) { Add-Content $log 'window never appeared'; exit }

Start-Sleep -Seconds 45
Capture 'C:\Projects\ClayTennis\RawAssets\game_shot_a.png' | Out-Null
Start-Sleep -Seconds 25
Capture 'C:\Projects\ClayTennis\RawAssets\game_shot_b.png' | Out-Null
Add-Content $log 'done'
