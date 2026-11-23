<#
.SYNOPSIS
  Downloads the clay-tennis-court reference photographs listed in
  RawAssets\Refs\manifest.json into RawAssets\Refs\ref_NN.jpg.

.DESCRIPTION
  Deterministic and idempotent: the manifest pins ref_NN.jpg to one specific
  Commons file, so re-running never renumbers or duplicates anything. A file
  that already exists and passes image validation is left untouched.

  Wikimedia's image CDN returns HTTP 429 unless the request carries a browser
  User-Agent AND a commons.wikimedia.org Referer, so both are always sent.

.NOTES
  PowerShell 5.1 / Windows. Source is ASCII-only on purpose (PS 5.1 reads
  BOM-less .ps1 as ANSI); all non-ASCII data lives in manifest.json.
#>
[CmdletBinding()]
param(
  # Re-download even when a valid file is already present.
  [switch]$Force,
  # Seconds to wait between requests (be polite to the CDN).
  [int]$ThrottleSeconds = 3
)

$ErrorActionPreference = 'Stop'
$ProgressPreference    = 'SilentlyContinue'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$UserAgent = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36'
$Headers   = @{ Referer = 'https://commons.wikimedia.org/' }

$RefsDir   = Join-Path $PSScriptRoot '..\RawAssets\Refs'
$RefsDir   = [IO.Path]::GetFullPath($RefsDir)
$Manifest  = Join-Path $RefsDir 'manifest.json'

if (-not (Test-Path -LiteralPath $Manifest)) {
  throw "Manifest not found: $Manifest"
}

$entries = [IO.File]::ReadAllText($Manifest, [Text.Encoding]::UTF8) | ConvertFrom-Json
Write-Host "Manifest entries: $($entries.Count)"

Add-Type -AssemblyName System.Drawing

function Test-JpegMagic {
  param([string]$Path)
  try {
    $fs = [IO.File]::OpenRead($Path)
    try {
      if ($fs.Length -lt 4) { return $false }
      $b = New-Object byte[] 4
      [void]$fs.Read($b, 0, 4)
      # FF D8 FF = JPEG SOI + marker. An HTML error page starts with '<'.
      return ($b[0] -eq 0xFF -and $b[1] -eq 0xD8 -and $b[2] -eq 0xFF)
    } finally { $fs.Dispose() }
  } catch { return $false }
}

function Test-ImageFile {
  param([string]$Path, [int]$MinDim = 800)
  if (-not (Test-Path -LiteralPath $Path)) { return @{ ok = $false; why = 'missing' } }
  $len = (Get-Item -LiteralPath $Path).Length
  if ($len -lt 20KB) { return @{ ok = $false; why = "too small ($len bytes)" } }
  if (-not (Test-JpegMagic $Path)) { return @{ ok = $false; why = 'not a JPEG (HTML error page?)' } }
  try {
    $img = [Drawing.Image]::FromFile($Path)
    try {
      $w = $img.Width; $h = $img.Height
      if ($w -le $MinDim -or $h -le $MinDim) {
        return @{ ok = $false; why = "dimensions ${w}x${h} do not exceed $MinDim px" }
      }
      return @{ ok = $true; why = ''; w = $w; h = $h }
    } finally { $img.Dispose() }
  } catch {
    return @{ ok = $false; why = "decode failed: $($_.Exception.Message)" }
  }
}

function Invoke-Download {
  param([string]$Url, [string]$OutFile)

  # Primary: Invoke-WebRequest with the required browser headers.
  try {
    Invoke-WebRequest -Uri $Url -OutFile $OutFile -UserAgent $UserAgent `
      -Headers $Headers -TimeoutSec 120 -UseBasicParsing
    if ((Get-Item -LiteralPath $OutFile).Length -gt 0) { return $true }
  } catch {
    Write-Verbose "Invoke-WebRequest failed: $($_.Exception.Message)"
  }

  # Fallback: curl.exe (ships with Windows 10+).
  $code = curl.exe -sS -L --retry 3 --retry-delay 5 -A $UserAgent `
    -H "Referer: https://commons.wikimedia.org/" -m 180 `
    -o $OutFile -w '%{http_code}' $Url 2>$null
  return ("$code" -eq '200')
}

$ok = 0; $skip = 0; $fail = 0; $problems = @()

foreach ($e in $entries) {
  $dest = Join-Path $RefsDir $e.file

  if (-not $Force -and (Test-Path -LiteralPath $dest)) {
    $v = Test-ImageFile -Path $dest
    if ($v.ok) {
      Write-Host ("[skip] {0}  already present ({1}x{2})" -f $e.file, $v.w, $v.h)
      $skip++
      continue
    }
    Write-Host ("[redo] {0}  existing file invalid: {1}" -f $e.file, $v.why)
    Remove-Item -LiteralPath $dest -Force
  }

  $tmp = "$dest.download"
  if (Test-Path -LiteralPath $tmp) { Remove-Item -LiteralPath $tmp -Force }

  Write-Host ("[get ] {0}  <- {1}" -f $e.file, $e.url)
  $got = $false
  try { $got = Invoke-Download -Url $e.url -OutFile $tmp } catch {
    Write-Host ("      download error: {0}" -f $_.Exception.Message)
  }

  if (-not $got) {
    $fail++; $problems += "$($e.file): transport failure"
    if (Test-Path -LiteralPath $tmp) { Remove-Item -LiteralPath $tmp -Force }
    Start-Sleep -Seconds $ThrottleSeconds
    continue
  }

  $v = Test-ImageFile -Path $tmp
  if (-not $v.ok) {
    $fail++; $problems += "$($e.file): $($v.why)"
    Remove-Item -LiteralPath $tmp -Force
    Start-Sleep -Seconds $ThrottleSeconds
    continue
  }

  Move-Item -LiteralPath $tmp -Destination $dest -Force
  Write-Host ("       ok: {0}x{1}, {2:N0} bytes" -f $v.w, $v.h, (Get-Item -LiteralPath $dest).Length)
  $ok++
  Start-Sleep -Seconds $ThrottleSeconds
}

Write-Host ''
Write-Host ("Downloaded {0}, reused {1}, failed {2}." -f $ok, $skip, $fail)
if ($problems.Count) {
  Write-Host 'Problems:'
  $problems | ForEach-Object { Write-Host "  - $_" }
  exit 1
}
exit 0
