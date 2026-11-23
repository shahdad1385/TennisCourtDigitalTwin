$ErrorActionPreference = 'Continue'
$log = 'C:\Projects\ClayTennis\RawAssets\capture_result.txt'
function Log($m) { Add-Content -Path $log -Value ("[" + (Get-Date -Format 'HH:mm:ss') + "] " + $m) }
Set-Content -Path $log -Value "start"

$editor = 'C:\UE_5.5\Engine\Binaries\Win64\UnrealEditor.exe'
$proj = 'C:\Projects\ClayTennis\ClayTennis.uproject'

if (-not (Get-Process UnrealEditor -ErrorAction SilentlyContinue)) {
    Start-Process -FilePath $editor -ArgumentList @($proj, '-game', '-RCWebControlEnable', '-nosplash', '-nopause', '-windowed', '-ResX=1600', '-ResY=900') | Out-Null
    Log 'editor launched'
} else {
    Log 'editor already running'
}

$rc = $false
for ($i = 0; $i -lt 200; $i++) {
    Start-Sleep -Seconds 5
    try {
        Invoke-RestMethod -Uri 'http://127.0.0.1:30010/remote/info' -TimeoutSec 2 | Out-Null
        $rc = $true
        Log ("RC up after " + (($i + 1) * 5) + "s")
        break
    } catch { }
}
if (-not $rc) { Log 'RC never came up' }

if ($rc) {
    # give the level time to load and compile its shaders
    Start-Sleep -Seconds 60
    foreach ($cmd in @('HighResShot 1920x1080', 'r.ScreenPercentage 100', 'HighResShot 1920x1080')) {
        try {
            $body = '{"Command":"' + $cmd + '"}'
            $r = Invoke-RestMethod -Uri 'http://127.0.0.1:30010/remote/console' -Method Post -ContentType 'application/json' -Body $body -TimeoutSec 20
            Log ("console ok: " + $cmd + " -> " + ($r | ConvertTo-Json -Compress -Depth 3))
        } catch {
            Log ("console failed: " + $cmd + " -> " + $_.Exception.Message)
        }
        Start-Sleep -Seconds 20
    }
}

$shots = Get-ChildItem 'C:\Projects\ClayTennis\Saved\Screenshots' -Recurse -File -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending
if ($shots) { foreach ($s in $shots | Select-Object -First 5) { Log ("shot: " + $s.FullName + " " + $s.Length) } } else { Log 'no engine screenshots' }

# fallback / extra: capture the editor window itself
Add-Type -AssemblyName System.Drawing
$code = @'
using System;
using System.Runtime.InteropServices;
public class WCap {
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h,int n);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  public struct RECT { public int L,T,R,B; }
}
'@
Add-Type -TypeDefinition $code -ErrorAction SilentlyContinue
$p = Get-Process UnrealEditor -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
if ($p) {
    [WCap]::ShowWindow($p.MainWindowHandle, 9) | Out-Null
    [WCap]::SetForegroundWindow($p.MainWindowHandle) | Out-Null
    Start-Sleep -Seconds 3
    $r = New-Object WCap+RECT
    [WCap]::GetWindowRect($p.MainWindowHandle, [ref]$r) | Out-Null
    $w = $r.R - $r.L; $h = $r.B - $r.T
    if ($w -gt 200 -and $h -gt 200) {
        $bmp = New-Object System.Drawing.Bitmap($w, $h)
        $g = [System.Drawing.Graphics]::FromImage($bmp)
        $g.CopyFromScreen($r.L, $r.T, 0, 0, $bmp.Size)
        $bmp.Save('C:\Projects\ClayTennis\RawAssets\editor_window.png', [System.Drawing.Imaging.ImageFormat]::Png)
        Log ("window captured " + $w + "x" + $h)
    } else { Log ("window too small: " + $w + "x" + $h) }
} else { Log 'no editor window handle' }

Log 'done'
