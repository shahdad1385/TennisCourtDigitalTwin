<#
.SYNOPSIS
  Analyses the clay-tennis-court reference photographs in RawAssets\Refs and
  writes palette.json, PALETTE.md and swatch.png.

.DESCRIPTION
  For every ref_*.jpg:

    * Crops the CENTRAL 50% of the frame (the midpoint of the 40-60% band the
      spec allows; override with -CropPercent).
    * Samples every 4th pixel in x and y via LockBits (no per-pixel GetPixel).
    * Discards sky/people/out-of-range pixels using luminance 20..235, so the
      remaining set is effectively the court surface.
    * Accumulates a 3D histogram with 4 bits per channel (16x16x16 = 4096 bins)
      and reports the 5 dominant colours.
    * Reports mean sRGB, mean luminance, contrast (std dev), the 10th and 90th
      percentile luminance, and a warmth metric (R-B).

  It then pools every image into one aggregate histogram and derives a
  recommended albedo, a sun colour and a shadow tint (formulas documented in
  PALETTE.md), and renders swatch.png.

.OUTPUTS
  RawAssets\Refs\palette.json   machine readable
  RawAssets\Refs\PALETTE.md     human readable tables
  RawAssets\Refs\swatch.png     200 px tall colour strip

.NOTES
  Deterministic: no timestamps, stable key order, one entry per input file, so
  re-running byte-for-byte reproduces the outputs and never duplicates entries.
  PowerShell 5.1; source kept ASCII-only (PS 5.1 reads BOM-less .ps1 as ANSI).
#>
[CmdletBinding()]
param(
  # Central crop size as a percentage of each axis (spec allows 40..60).
  [ValidateRange(40, 60)]
  [int]$CropPercent = 50,

  # Luminance window (sRGB, Rec.709 weights) used to reject sky, people, lines.
  [int]$LumMin = 20,
  [int]$LumMax = 235,

  # Sampling stride: sample every Nth pixel in x and y.
  [ValidateRange(1, 16)]
  [int]$Step = 4,

  # Luminance the recommended albedo is normalised to (0..255).
  # Pinned so a reference set re-shot in unusual light still yields a usable
  # albedo; 135 sits within a couple of levels of this set's raw pooled
  # measurement and matches published dry red-clay albedos (sRGB L ~116-135).
  [int]$AlbedoLumaTarget = 135
)

$ErrorActionPreference = 'Stop'
$ProgressPreference    = 'SilentlyContinue'
Add-Type -AssemblyName System.Drawing

# ---------------------------------------------------------------- paths -----
$RefsDir  = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\RawAssets\Refs'))
$JsonPath = Join-Path $RefsDir 'palette.json'
$MdPath   = Join-Path $RefsDir 'PALETTE.md'
$SwPath   = Join-Path $RefsDir 'swatch.png'

# ------------------------------------------------------------ colour math ---
# Luminance with Rec.709 weights, pre-multiplied by 256 so the inner loop can
# stay on integers: L256 = 54*R + 183*G + 19*B  (real L = L256/256).
function Get-Luma256 { param([int]$r, [int]$g, [int]$b) return (54 * $r + 183 * $g + 19 * $b) }

function Get-Luma { param([double]$r, [double]$g, [double]$b) return (54.0 * $r + 183.0 * $g + 19.0 * $b) / 256.0 }

function ConvertTo-Srgb { param([double]$c)   # linear 0..1 -> sRGB 0..1
  if ($c -le 0) { return 0.0 }
  if ($c -ge 1) { return 1.0 }
  if ($c -le 0.0031308) { return $c * 12.92 }
  return 1.055 * [math]::Pow($c, 1.0 / 2.4) - 0.055
}

function ConvertTo-Linear { param([double]$c)   # sRGB 0..1 -> linear 0..1
  if ($c -le 0) { return 0.0 }
  if ($c -le 0.04045) { return $c / 12.92 }
  return [math]::Pow(($c + 0.055) / 1.055, 2.4)
}

function Clamp255 { param([double]$v)
  if ($v -lt 0) { return 0 }
  if ($v -gt 255) { return 255 }
  return [int][math]::Round($v)
}

function Get-RgbHex { param([int]$r, [int]$g, [int]$b)
  return ('#{0:X2}{1:X2}{2:X2}' -f (Clamp255 $r), (Clamp255 $g), (Clamp255 $b))
}

# Rescale a colour so its luminance equals $target, preserving hue/saturation.
# Scaling is done in linear light (bisection on the scale factor) because that
# is what physically changes when exposure/illumination changes.
function Set-Luminance {
  param([int]$r, [int]$g, [int]$b, [double]$target)
  if (($r + $g + $b) -eq 0) { return ,([int[]]@(0, 0, 0)) }
  $lr = ConvertTo-Linear ($r / 255.0)
  $lg = ConvertTo-Linear ($g / 255.0)
  $lb = ConvertTo-Linear ($b / 255.0)

  $lo = 0.0; $hi = 16.0
  for ($i = 0; $i -lt 64; $i++) {
    $k = ($lo + $hi) / 2.0
    # NOTE: a bare `ConvertTo-Srgb X * 255.0` parses `* 255.0` as extra
    # ARGUMENTS to the command, not a multiplication - the parens are mandatory.
    $cr = (ConvertTo-Srgb ($k * $lr)) * 255.0
    $cg = (ConvertTo-Srgb ($k * $lg)) * 255.0
    $cb = (ConvertTo-Srgb ($k * $lb)) * 255.0
    if ((Get-Luma $cr $cg $cb) -lt $target) { $lo = $k } else { $hi = $k }
  }
  $k = ($lo + $hi) / 2.0
  return ,([int[]]@(
    (Clamp255 ((ConvertTo-Srgb ($k * $lr)) * 255.0)),
    (Clamp255 ((ConvertTo-Srgb ($k * $lg)) * 255.0)),
    (Clamp255 ((ConvertTo-Srgb ($k * $lb)) * 255.0))
  ))
}

# Multiply a colour by a factor in linear light (used for the +/-10% swatches).
function Scale-Luminance {
  param([int]$r, [int]$g, [int]$b, [double]$factor)
  $cr = (ConvertTo-Srgb ((ConvertTo-Linear ($r / 255.0)) * $factor)) * 255.0
  $cg = (ConvertTo-Srgb ((ConvertTo-Linear ($g / 255.0)) * $factor)) * 255.0
  $cb = (ConvertTo-Srgb ((ConvertTo-Linear ($b / 255.0)) * $factor)) * 255.0
  return ,([int[]]@((Clamp255 $cr), (Clamp255 $cg), (Clamp255 $cb)))
}

# Blend toward white (or toward mid-grey when $t is negative) in linear light.
function Blend-White {
  param([int]$r, [int]$g, [int]$b, [double]$t)
  $lr = ConvertTo-Linear ($r / 255.0); $lg = ConvertTo-Linear ($g / 255.0); $lb = ConvertTo-Linear ($b / 255.0)
  if ($t -ge 0) {
    $lr = $lr + (1.0 - $lr) * $t; $lg = $lg + (1.0 - $lg) * $t; $lb = $lb + (1.0 - $lb) * $t
  } else {
    $m = 0.5
    $lr = $lr + ($m - $lr) * (-$t); $lg = $lg + ($m - $lg) * (-$t); $lb = $lb + ($m - $lb) * (-$t)
  }
  return ,([int[]]@(
    (Clamp255 ((ConvertTo-Srgb $lr) * 255.0)),
    (Clamp255 ((ConvertTo-Srgb $lg) * 255.0)),
    (Clamp255 ((ConvertTo-Srgb $lb) * 255.0))
  ))
}

function Get-BinRgb { param([int]$idx)
  $rb = ($idx -shr 8) -band 15
  $gb = ($idx -shr 4) -band 15
  $bb = $idx -band 15
  return ,([int[]]@(($rb * 16 + 8), ($gb * 16 + 8), ($bb * 16 + 8)))
}

# Top $n bins of a 4096-entry 4-bit-per-channel histogram.
function Get-TopBins {
  param([int[]]$hist, [int]$n)
  $total = 0
  foreach ($c in $hist) { $total += $c }
  if ($total -eq 0) { return ,@() }

  $order = New-Object 'System.Collections.Generic.List[object]'
  for ($i = 0; $i -lt 4096; $i++) {
    if ($hist[$i] -gt 0) { $order.Add([pscustomobject]@{ idx = $i; c = $hist[$i] }) }
  }
  $top = @($order | Sort-Object -Property c -Descending | Select-Object -First $n)

  $res = @()
  foreach ($t in $top) {
    $rgb = Get-BinRgb $t.idx
    $res += [pscustomobject]@{
      hex   = Get-RgbHex $rgb[0] $rgb[1] $rgb[2]
      r     = $rgb[0]; g = $rgb[1]; b = $rgb[2]
      count = $t.c
      share = [math]::Round($t.c / [double]$total, 4)
    }
  }
  return ,$res
}

# --------------------------------------------------------------- analyse ----
function Get-ImageStats {
  param([string]$Path, [double]$CropFrac, [int]$StepSize, [int]$Lo, [int]$Hi)

  $bmp = New-Object Drawing.Bitmap($Path)
  $packed = $null; $lums = $null; $hist = $null
  try {
    $w = $bmp.Width; $h = $bmp.Height
    $cw = [int]($w * $CropFrac);  $ch = [int]($h * $CropFrac)
    if ($cw % 2 -ne 0) { $cw-- }; if ($ch % 2 -ne 0) { $ch-- }
    $cx = [int](($w - $cw) / 2);  $cy = [int](($h - $ch) / 2)

    $lo256 = $Lo * 256
    $hi256 = $Hi * 256

    $rect = New-Object Drawing.Rectangle($cx, $cy, $cw, $ch)
    $data = $bmp.LockBits($rect, [Drawing.Imaging.ImageLockMode]::ReadOnly,
                           [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
      $stride = $data.Stride
      $bytes  = New-Object byte[] ($stride * $ch)
      [Runtime.InteropServices.Marshal]::Copy($data.Scan0, $bytes, 0, $bytes.Length)

      # Generous upper bound: ceil(cw/Step) * ceil(ch/Step).
      $cap = [int]([math]::Ceiling($cw / [double]$StepSize) * [math]::Ceiling($ch / [double]$StepSize))
      $packed = New-Object 'System.Collections.Generic.List[int]' ($cap)
      $lums   = New-Object 'System.Collections.Generic.List[int]' ($cap)
      $hist   = New-Object int[] 4096

      for ($y = 0; $y -lt $ch; $y += $StepSize) {
        $rowOff = $y * $stride
        for ($x = 0; $x -lt $cw; $x += $StepSize) {
          $o = $rowOff + ($x * 4)
          $b = $bytes[$o]; $g = $bytes[$o + 1]; $r = $bytes[$o + 2]
          $l256 = (54 * $r) + (183 * $g) + (19 * $b)
          if ($l256 -lt $lo256 -or $l256 -gt $hi256) { continue }   # drop sky / people / crushed blacks
          # NOTE: -shl keeps the operand's type, and shifting a [byte] left
          # overflows to 0 (verified: ([byte]180) -shl 16 == 0). Cast to [int]
          # before every left shift or the channels silently collapse.
          $packed.Add(([int]$r -shl 16) -bor ([int]$g -shl 8) -bor [int]$b)
          $lums.Add($l256)
          $hist[(([int]($r -shr 4)) -shl 8) -bor (([int]($g -shr 4)) -shl 4) -bor [int]($b -shr 4)]++
        }
      }
    } finally { $bmp.UnlockBits($data) }

    $n = $packed.Count
    if ($n -lt 32) { throw "Only $n usable pixels in crop - image is not a usable reference." }

    # --- means -------------------------------------------------------------
    $sr = 0L; $sg = 0L; $sb = 0L; $sl = 0L
    for ($i = 0; $i -lt $n; $i++) {
      $p = $packed[$i]
      $sr += (($p -shr 16) -band 255)
      $sg += (($p -shr 8) -band 255)
      $sb += ($p -band 255)
      $sl += $lums[$i]
    }
    $meanR = $sr / [double]$n; $meanG = $sg / [double]$n; $meanB = $sb / [double]$n
    $meanL = ($sl / [double]$n) / 256.0

    # --- contrast (std dev) -------------------------------------------------
    $sq = 0.0
    for ($i = 0; $i -lt $n; $i++) { $d = ($lums[$i] / 256.0) - $meanL; $sq += $d * $d }
    $sd = [math]::Sqrt($sq / $n)

    # --- percentiles --------------------------------------------------------
    $sorted = $lums.ToArray()
    [array]::Sort($sorted)
    $p10 = ($sorted[[int][math]::Floor(0.10 * ($n - 1))]) / 256.0
    $p90 = ($sorted[[int][math]::Floor(0.90 * ($n - 1))]) / 256.0
    $p25 = ($sorted[[int][math]::Floor(0.25 * ($n - 1))]) / 256.0
    $p75 = ($sorted[[int][math]::Floor(0.75 * ($n - 1))]) / 256.0

    # --- sunlit / shadowed sub-populations (for sun + shadow derivation) ----
    $lo256 = [int]($p25 * 256); $hi256 = [int]($p75 * 256)
    $sunR = 0L; $sunG = 0L; $sunB = 0L; $sunN = 0L
    $shR = 0L;  $shG = 0L;  $shB = 0L;  $shN = 0L
    for ($i = 0; $i -lt $n; $i++) {
      $p = $packed[$i]
      $r = ($p -shr 16) -band 255; $g = ($p -shr 8) -band 255; $b = $p -band 255
      if ($r -le $b) { continue }                        # clay is warm: R > B
      $L = $lums[$i]
      if ($L -ge $hi256)     { $sunR += $r; $sunG += $g; $sunB += $b; $sunN++ }
      elseif ($L -le $lo256) { $shR += $r;  $shG += $g;  $shB += $b;  $shN++ }
    }

    $dom = Get-TopBins -hist $hist -n 5

    return [pscustomobject]@{
      width   = $w; height = $h
      crop_x = $cx; crop_y = $cy; crop_w = $cw; crop_h = $ch
      sampled = $packed.Count; accepted = $n
      mean_r = $meanR; mean_g = $meanG; mean_b = $meanB
      mean_lum = $meanL; stddev = $sd; p10 = $p10; p90 = $p90
      warmth = ($meanR - $meanB)
      dominant = $dom
      hist = $hist
      sun_rgb = $(if ($sunN -gt 0) { @([int]($sunR / $sunN), [int]($sunG / $sunN), [int]($sunB / $sunN)) } else { $null })
      sh_rgb  = $(if ($shN -gt 0)  { @([int]($shR / $shN),  [int]($shG / $shN),  [int]($shB / $shN))  } else { $null })
    }
  } finally {
    $bmp.Dispose()
  }
}

# =============================================================== main ========
$files = @(Get-ChildItem -LiteralPath $RefsDir -Filter 'ref_*.jpg' | Sort-Object Name)
if ($files.Count -eq 0) { throw "No ref_*.jpg files in $RefsDir. Run Tools\download_refs.ps1 first." }
Write-Host "Analysing $($files.Count) reference image(s), crop=$CropPercent%, step=$Step, luma=$LumMin..$LumMax"

$stats = @()
$globalHist = New-Object int[] 4096

foreach ($f in $files) {
  $s = $null
  try {
    $s = Get-ImageStats -Path $f.FullName -CropFrac ($CropPercent / 100.0) `
                        -StepSize $Step -Lo $LumMin -Hi $LumMax
  } catch {
    Write-Warning "$($f.Name): $($_.Exception.Message)"
    continue
  }
  $s | Add-Member -NotePropertyName file -NotePropertyValue $f.Name
  $s | Add-Member -NotePropertyName bytes -NotePropertyValue $f.Length
  $stats += $s
  for ($i = 0; $i -lt 4096; $i++) { if ($s.hist[$i] -gt 0) { $globalHist[$i] += $s.hist[$i] } }
  Write-Host ("  {0}: {1} px, mean {2}, lum {3:N1}, warmth {4:N1}" -f `
    $f.Name, $s.accepted, (Get-RgbHex ([int][math]::Round($s.mean_r)) ([int][math]::Round($s.mean_g)) ([int][math]::Round($s.mean_b))),
    $s.mean_lum, $s.warmth)
}

if ($stats.Count -eq 0) { throw "No images could be analysed." }

# ---------------------------------------------------- aggregate statistics ---
$n = $stats.Count
$meanOf = [double]($stats | Measure-Object -Property mean_r -Average).Average
$meanOg = [double]($stats | Measure-Object -Property mean_g -Average).Average
$meanOb = [double]($stats | Measure-Object -Property mean_b -Average).Average

$aggLum = [double]($stats | Measure-Object -Property mean_lum -Average).Average
$aggSd  = [double]($stats | Measure-Object -Property stddev   -Average).Average
$aggP10 = [double]($stats | Measure-Object -Property p10      -Average).Average
$aggP90 = [double]($stats | Measure-Object -Property p90      -Average).Average
$aggWrm = [double]($stats | Measure-Object -Property warmth    -Average).Average

# Clay mass: every warm (R > G > B) mid-luminance bin pooled across all images.
$massN = 0L; $mr = 0L; $mg = 0L; $mb = 0L
for ($i = 0; $i -lt 4096; $i++) {
  $c = $globalHist[$i]; if ($c -eq 0) { continue }
  $rgb = Get-BinRgb $i
  $L = (Get-Luma $rgb[0] $rgb[1] $rgb[2])
  if ($rgb[0] -le $rgb[1]) { continue }
  if ($rgb[1] -le $rgb[2]) { continue }
  if (($rgb[0] - $rgb[2]) -lt 25) { continue }
  if ($L -lt 55 -or $L -gt 215) { continue }
  $mr += $rgb[0] * $c; $mg += $rgb[1] * $c; $mb += $rgb[2] * $c; $massN += $c
}
if ($massN -eq 0) { throw "No warm clay bins found - reference set looks wrong." }
$mass = @([int]($mr / $massN), [int]($mg / $massN), [int]($mb / $massN))

# Recommended albedo: clay mass, exposure-normalised to a fixed diffuse target.
$albedo = Set-Luminance $mass[0] $mass[1] $mass[2] $AlbedoLumaTarget
$albedoLight = Scale-Luminance $albedo[0] $albedo[1] $albedo[2] 1.10
$albedoDark  = Scale-Luminance $albedo[0] $albedo[1] $albedo[2] 0.90

# Sun colour: measured sunlit-clay hue, lifted toward white and normalised.
$sunAcc = @(0.0, 0.0, 0.0); $sunCnt = 0
$shAcc  = @(0.0, 0.0, 0.0); $shCnt = 0
foreach ($s in $stats) {
  if ($s.sun_rgb) { $sunAcc[0] += $s.sun_rgb[0]; $sunAcc[1] += $s.sun_rgb[1]; $sunAcc[2] += $s.sun_rgb[2]; $sunCnt++ }
  if ($s.sh_rgb)  { $shAcc[0]  += $s.sh_rgb[0];  $shAcc[1]  += $s.sh_rgb[1];  $shAcc[2]  += $s.sh_rgb[2];  $shCnt++  }
}
if ($sunCnt -eq 0) { $sunAcc = @($mass) ; $sunCnt = 1 }
if ($shCnt  -eq 0) { $shAcc  = @($mass) ; $shCnt  = 1 }
$sunMean = @([int]($sunAcc[0] / $sunCnt), [int]($sunAcc[1] / $sunCnt), [int]($sunAcc[2] / $sunCnt))
$shMean  = @([int]($shAcc[0] / $shCnt),  [int]($shAcc[1] / $shCnt),  [int]($shAcc[2] / $shCnt))

$sunWhiteBlend = 0.30   # how far the measured sunlit-clay hue is lifted toward white
$sunRaw = Blend-White $sunMean[0] $sunMean[1] $sunMean[2] $sunWhiteBlend
$sun    = Set-Luminance $sunRaw[0] $sunRaw[1] $sunRaw[2] 245

$cool = -0.25
$shRaw = Blend-White $shMean[0] $shMean[1] $shMean[2] $cool
$shTarget = [math]::Max(40.0, [math]::Min(95.0, $aggP10))
$shadow   = Set-Luminance $shRaw[0] $shRaw[1] $shRaw[2] $shTarget

# ---- sanity checks: catch colour-math regressions before anything is written.
function Assert-Colour {
  param([string]$name, [int[]]$rgb, [double]$minL, [double]$maxL, [bool]$requireWarm = $true)
  $L = Get-Luma $rgb[0] $rgb[1] $rgb[2]
  if ($L -lt $minL -or $L -gt $maxL) {
    throw ("{0} luminance {1} outside expected {2}..{3} - colour maths regression." -f `
      $name, [math]::Round($L, 1), $minL, $maxL)
  }
  if ($requireWarm -and ($rgb[0] -le $rgb[2])) {
    throw ("{0} is not warm (R={1} B={2}) - reference set should be orange-red clay." -f `
      $name, $rgb[0], $rgb[2])
  }
}
Assert-Colour 'albedo'        $albedo        ($AlbedoLumaTarget - 20) ($AlbedoLumaTarget + 20)
Assert-Colour 'albedo_light'  $albedoLight   ($AlbedoLumaTarget -  5) ($AlbedoLumaTarget + 40)
Assert-Colour 'albedo_dark'   $albedoDark    ($AlbedoLumaTarget - 45) ($AlbedoLumaTarget +  5)
Assert-Colour 'sun'           $sun           220 255 $false
Assert-Colour 'shadow_tint'   $shadow        25 115 $false
if ((Get-Luma $shadow[0] $shadow[1] $shadow[2]) -ge (Get-Luma $albedo[0] $albedo[1] $albedo[2])) {
  throw 'shadow tint must be darker than the albedo.'
}
if ((Get-Luma $sun[0] $sun[1] $sun[2]) -le (Get-Luma $albedo[0] $albedo[1] $albedo[2])) {
  throw 'sun colour must be brighter than the albedo.'
}

$aggDominant = Get-TopBins -hist $globalHist -n 5

# ------------------------------------------------------------- palette.json --
$imageObjs = @()
foreach ($s in $stats) {
  $imageObjs += [pscustomobject][ordered]@{
    file        = $s.file
    bytes       = $s.bytes
    width       = $s.width
    height      = $s.height
    crop        = [pscustomobject][ordered]@{ x = $s.crop_x; y = $s.crop_y; w = $s.crop_w; h = $s.crop_h; percent = $CropPercent }
    samples     = $s.sampled
    accepted    = $s.accepted
    rejected    = ($s.sampled - $s.accepted)
    mean_srgb   = [pscustomobject][ordered]@{
      r = [math]::Round($s.mean_r, 2); g = [math]::Round($s.mean_g, 2); b = [math]::Round($s.mean_b, 2)
      hex = Get-RgbHex ([int][math]::Round($s.mean_r)) ([int][math]::Round($s.mean_g)) ([int][math]::Round($s.mean_b))
    }
    dominant    = @($s.dominant | ForEach-Object {
                     [pscustomobject][ordered]@{ hex = $_.hex; r = $_.r; g = $_.g; b = $_.b; count = $_.count; share = $_.share }
                   })
    luminance   = [pscustomobject][ordered]@{
      mean = [math]::Round($s.mean_lum, 2); contrast = [math]::Round($s.stddev, 2)
      p10  = [math]::Round($s.p10, 2);      p90 = [math]::Round($s.p90, 2)
    }
    warmth      = [math]::Round($s.warmth, 2)
    sunlit_srgb = $(if ($s.sun_rgb) { Get-RgbHex $s.sun_rgb[0] $s.sun_rgb[1] $s.sun_rgb[2] } else { $null })
    shadow_srgb = $(if ($s.sh_rgb)  { Get-RgbHex $s.sh_rgb[0]  $s.sh_rgb[1]  $s.sh_rgb[2]  } else { $null })
  }
}

$json = [pscustomobject][ordered]@{
  parameters = [pscustomobject][ordered]@{
    crop_percent      = $CropPercent
    sample_step       = $Step
    luminance_min     = $LumMin
    luminance_max     = $LumMax
    histogram_bits    = 4
    histogram_bins    = 4096
    luminance_weights = 'Rec.709 applied to sRGB-encoded values (54/183/19 over 256)'
    albedo_luma_target = $AlbedoLumaTarget
    source            = 'Wikimedia Commons, see CREDITS.md / manifest.json'
  }
  image_count = $n
  images      = $imageObjs
  aggregate   = [pscustomobject][ordered]@{
    image_count = $n
    mean_srgb   = [pscustomobject][ordered]@{
      r = [math]::Round($meanOf, 2); g = [math]::Round($meanOg, 2); b = [math]::Round($meanOb, 2)
      hex = Get-RgbHex ([int][math]::Round($meanOf)) ([int][math]::Round($meanOg)) ([int][math]::Round($meanOb))
    }
    luminance = [pscustomobject][ordered]@{
      mean = [math]::Round($aggLum, 2); contrast = [math]::Round($aggSd, 2)
      p10  = [math]::Round($aggP10, 2); p90 = [math]::Round($aggP90, 2)
    }
    warmth = [math]::Round($aggWrm, 2)
    pooled_clay_mass = [pscustomobject][ordered]@{
      r = $mass[0]; g = $mass[1]; b = $mass[2]
      hex = Get-RgbHex $mass[0] $mass[1] $mass[2]
      pixels = $massN
    }
    dominant = @($aggDominant | ForEach-Object {
                  [pscustomobject][ordered]@{ hex = $_.hex; r = $_.r; g = $_.g; b = $_.b; count = $_.count; share = $_.share }
                })
    recommended = [pscustomobject][ordered]@{
      albedo_hex      = Get-RgbHex $albedo[0] $albedo[1] $albedo[2]
      albedo_light_hex = Get-RgbHex $albedoLight[0] $albedoLight[1] $albedoLight[2]
      albedo_dark_hex  = Get-RgbHex $albedoDark[0] $albedoDark[1] $albedoDark[2]
      sun_color_hex    = Get-RgbHex $sun[0] $sun[1] $sun[2]
      shadow_tint_hex  = Get-RgbHex $shadow[0] $shadow[1] $shadow[2]
      derivation = [pscustomobject][ordered]@{
        albedo       = "pooled warm clay bins (R>G>B, R-B>=25, L 55..215), luminance normalised to $AlbedoLumaTarget"
        albedo_light = "albedo x 1.10 in linear light"
        albedo_dark  = "albedo x 0.90 in linear light"
        sun_color    = "mean of measured sunlit-clay pixels (>= p75 luminance, R>B), blended $($sunWhiteBlend * 100)% to white, luminance 245"
        shadow_tint  = "mean of measured shadowed-clay pixels (<= p25 luminance, R>B), desaturated 25% toward grey, luminance clamped p10 to 40..95"
      }
    }
  }
}

$utf8 = New-Object Text.UTF8Encoding($false)
[IO.File]::WriteAllText($JsonPath, ($json | ConvertTo-Json -Depth 10), $utf8)
Write-Host "Wrote $JsonPath"

# --------------------------------------------------------------- PALETTE.md --
function Row {
  param($f, $hex, $lum, $sd, $p10, $p90, $warm, $acc, $d0, $d1, $d2)
  return ("| `{0}` | `{1}` | {2:N1} | {3:N1} | {4:N1} | {5:N1} | {6:N1} | {7:N0} | `{8}` | `{9}` | `{10}` |" -f `
    $f, $hex, $lum, $sd, $p10, $p90, $warm, $acc, $d0, $d1, $d2)
}

$md = New-Object System.Collections.Generic.List[string]
$md.Add("# Clay court palette -- measured from reference photography")
$md.Add("")
$md.Add("Generated by ``Tools/analyze_refs.ps1`` from the $($n) photographs in this folder.")
$md.Add("Re-running the script reproduces this file byte for byte.")
$md.Add("")
$md.Add("## Method")
$md.Add("")
$md.Add("| Parameter | Value |")
$md.Add("|---|---|")
$md.Add("| Central crop | $CropPercent% of each axis (spec allows 40-60%) |")
$md.Add("| Sampling | every Nth pixel in x and y, N = $Step, via ``LockBits`` ``Format32bppArgb`` |")
$md.Add("| Luminance window | $LumMin..$LumMax (Rec.709 on sRGB) -- rejects sky, people, crushed blacks |")
$md.Add("| Colour histogram | 3D, 4 bits per channel = 16x16x16 = 4096 bins |")
$md.Add("| Aggregation | arithmetic mean over images; ``pooled_clay_mass`` pools every accepted warm pixel |")
$md.Add("")
$md.Add("## Per-image results")
$md.Add("")
$md.Add("| File | Court mean | Mean L | Contrast (sd) | p10 L | p90 L | Warmth R-B | Accepted px | Dominant 1 | Dominant 2 | Dominant 3 |")
$md.Add("|---|---|---|---|---|---|---|---|---|---|---|")

foreach ($s in $stats) {
  $hex = Get-RgbHex ([int][math]::Round($s.mean_r)) ([int][math]::Round($s.mean_g)) ([int][math]::Round($s.mean_b))
  $d = $s.dominant
  $d0 = $(if ($d.Count -gt 0) { $d[0].hex } else { '-' })
  $d1 = $(if ($d.Count -gt 1) { $d[1].hex } else { '-' })
  $d2 = $(if ($d.Count -gt 2) { $d[2].hex } else { '-' })
  $md.Add((Row $s.file $hex $s.mean_lum $s.stddev $s.p10 $s.p90 $s.warmth $s.accepted $d0 $d1 $d2))
}

$md.Add("")
$md.Add("### Dominant colours per image (top 5 of 4096 bins)")
$md.Add("")
foreach ($s in $stats) {
  $parts = @()
  foreach ($d in $s.dominant) { $parts += ("`{0}` {1:P1}" -f $d.hex, $d.share) }
  $md.Add(("- **{0}** -- {1}" -f $s.file, ($parts -join "  ")))
}

$md.Add("")
$md.Add("## Aggregate (mean over $($n) images)")
$md.Add("")
$md.Add("| Metric | Value |")
$md.Add("|---|---|")
$md.Add(("| Mean court sRGB | `{0}`  (R {1:N1}, G {2:N1}, B {3:N1}) |" -f `
  (Get-RgbHex ([int][math]::Round($meanOf)) ([int][math]::Round($meanOg)) ([int][math]::Round($meanOb))), $meanOf, $meanOg, $meanOb))
$md.Add("| Mean luminance | {0:N1} |" -f $aggLum)
$md.Add("| Mean contrast (std dev) | {0:N1} |" -f $aggSd)
$md.Add("| Mean p10 luminance | {0:N1} |" -f $aggP10)
$md.Add("| Mean p90 luminance | {0:N1} |" -f $aggP90)
$md.Add("| Mean warmth (R-B) | {0:N1} |" -f $aggWrm)
$md.Add(("| Pooled clay mass | `{0}` over {1:N0} warm pixels |" -f (Get-RgbHex $mass[0] $mass[1] $mass[2]), $massN))
$md.Add("")
$md.Add("### Aggregate dominant colours")
$md.Add("")
$md.Add("| Rank | Colour | Share of pooled pixels |")
$md.Add("|---|---|---|")
$rank = 0
foreach ($d in $aggDominant) { $rank++; $md.Add(("| $rank | `{0}` | {1:P2} |" -f $d.hex, $d.share)) }

$md.Add("")
$md.Add("## Recommended values for UE 5.5")
$md.Add("")
$md.Add("| Role | Hex | sRGB (0-1) | Linear (0-1) |")
$md.Add("|---|---|---|---|")
$roles = @(
  @{ n = 'Albedo (base colour)'; h = (Get-RgbHex $albedo[0] $albedo[1] $albedo[2]) },
  @{ n = 'Albedo +10% lighter';  h = (Get-RgbHex $albedoLight[0] $albedoLight[1] $albedoLight[2]) },
  @{ n = 'Albedo -10% darker';   h = (Get-RgbHex $albedoDark[0] $albedoDark[1] $albedoDark[2]) },
  @{ n = 'Shadow tint';          h = (Get-RgbHex $shadow[0] $shadow[1] $shadow[2]) },
  @{ n = 'Sun colour';           h = (Get-RgbHex $sun[0] $sun[1] $sun[2]) }
)
foreach ($r in $roles) {
  $rr = [Convert]::ToInt32($r.h.Substring(1, 2), 16)
  $gg = [Convert]::ToInt32($r.h.Substring(3, 2), 16)
  $bb = [Convert]::ToInt32($r.h.Substring(5, 2), 16)
  $md.Add(("| {0} | `{1}` | {2:N3}, {3:N3}, {4:N3} | {5:N3}, {6:N3}, {7:N3} |" -f `
    $r.n, $r.h, ($rr / 255.0), ($gg / 255.0), ($bb / 255.0),
    (ConvertTo-Linear ($rr / 255.0)), (ConvertTo-Linear ($gg / 255.0)), (ConvertTo-Linear ($bb / 255.0))))
}

$md.Add("")
$md.Add("### How each was derived")
$md.Add("")
$md.Add("- **Albedo** -- $($json.aggregate.recommended.derivation.albedo).")
$md.Add("  Normalising the luminance removes the per-photo exposure differences so the")
$md.Add("  value is a material property, not a picture of a lit surface.")
$md.Add("- **Albedo +/-10%** -- $($json.aggregate.recommended.derivation.albedo_light) / $($json.aggregate.recommended.derivation.albedo_dark).")
$md.Add("- **Sun colour** -- $($json.aggregate.recommended.derivation.sun_color).")
$md.Add("- **Shadow tint** -- $($json.aggregate.recommended.derivation.shadow_tint).")
$md.Add("")
$md.Add("![swatch](swatch.png)")
$md.Add("")

[IO.File]::WriteAllText($MdPath, (($md | Out-String).TrimEnd() + "`n"), $utf8)
Write-Host "Wrote $MdPath"

# ---------------------------------------------------------------- swatch.png --
$panels = @(
  @{ label = 'ALBEDO';       hex = (Get-RgbHex $albedo[0] $albedo[1] $albedo[2]);        rgb = $albedo },
  @{ label = 'ALBEDO +10%';  hex = (Get-RgbHex $albedoLight[0] $albedoLight[1] $albedoLight[2]); rgb = $albedoLight },
  @{ label = 'ALBEDO -10%';  hex = (Get-RgbHex $albedoDark[0] $albedoDark[1] $albedoDark[2]);   rgb = $albedoDark },
  @{ label = 'SHADOW TINT';  hex = (Get-RgbHex $shadow[0] $shadow[1] $shadow[2]);        rgb = $shadow },
  @{ label = 'SUN COLOUR';   hex = (Get-RgbHex $sun[0] $sun[1] $sun[2]);                 rgb = $sun }
)

$stripH = 200
$panelW = 320
$W = $panelW * $panels.Count
$bmp = New-Object Drawing.Bitmap($W, $stripH)
$g = [Drawing.Graphics]::FromImage($bmp)
$g.Clear([Drawing.Color]::FromArgb(24, 24, 28))
$g.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
$g.TextRenderingHint = [Drawing.Text.TextRenderingHint]::ClearTypeGridFit

$fHex  = New-Object Drawing.Font('Consolas', 24, [Drawing.FontStyle]::Bold)
$fLbl  = New-Object Drawing.Font('Segoe UI', 11, [Drawing.FontStyle]::Bold)

for ($i = 0; $i -lt $panels.Count; $i++) {
  $p = $panels[$i]
  $x = $i * $panelW
  $col = [Drawing.Color]::FromArgb([int]$p.rgb[0], [int]$p.rgb[1], [int]$p.rgb[2])

  $g.FillRectangle([Drawing.Brushes]::White, (New-Object Drawing.Rectangle($x, 0, $panelW, $stripH)))
  $g.FillRectangle((New-Object Drawing.SolidBrush $col), (New-Object Drawing.Rectangle($x, 0, $panelW, $stripH)))

  # pick readable text colour from the swatch luminance
  $L = Get-Luma $p.rgb[0] $p.rgb[1] $p.rgb[2]
  $textBrush = if ($L -gt 140) { [Drawing.Brushes]::Black } else { [Drawing.Brushes]::White }

  $g.DrawString($p.label, $fLbl, $textBrush, ($x + 16), 26)
  $g.DrawString($p.hex,   $fHex, $textBrush, ($x + 16), 118)

  # separator between panels
  $g.DrawLine((New-Object Drawing.Pen ([Drawing.Color]::FromArgb(24, 24, 28), 2)),
              ($x + $panelW - 1), 0, ($x + $panelW - 1), $stripH)
}
$g.Dispose()
$bmp.Save($SwPath, [Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
Write-Host "Wrote $SwPath  (${W}x${stripH})"

# ------------------------------------------------------------------ summary --
Write-Host ''
Write-Host '================ AGGREGATE ================'
Write-Host ("Mean court sRGB      : {0}" -f (Get-RgbHex ([int][math]::Round($meanOf)) ([int][math]::Round($meanOg)) ([int][math]::Round($meanOb))))
Write-Host ("Pooled clay mass     : {0}" -f (Get-RgbHex $mass[0] $mass[1] $mass[2]))
Write-Host ("Mean luminance       : {0:N1}   contrast(sd): {1:N1}" -f $aggLum, $aggSd)
Write-Host ("p10 / p90 luminance  : {0:N1} / {1:N1}" -f $aggP10, $aggP90)
Write-Host ("Mean warmth (R-B)    : {0:N1}" -f $aggWrm)
Write-Host '-------------- RECOMMENDED ---------------'
Write-Host ("Albedo               : {0}" -f (Get-RgbHex $albedo[0] $albedo[1] $albedo[2]))
Write-Host ("Albedo +10%          : {0}" -f (Get-RgbHex $albedoLight[0] $albedoLight[1] $albedoLight[2]))
Write-Host ("Albedo -10%          : {0}" -f (Get-RgbHex $albedoDark[0] $albedoDark[1] $albedoDark[2]))
Write-Host ("Shadow tint          : {0}" -f (Get-RgbHex $shadow[0] $shadow[1] $shadow[2]))
Write-Host ("Sun colour           : {0}" -f (Get-RgbHex $sun[0] $sun[1] $sun[2]))
Write-Host '-------------------------------------------'
