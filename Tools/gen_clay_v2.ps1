# =============================================================================
# gen_clay_v2.ps1
# -----------------------------------------------------------------------------
# Photoreal-quality, tileable CRUSHED-BRICK CLAY COURT texture set (terre
# battue / French-Open style) for the ClayTennis UE 5.5 project.
#
# Visual goal (rescue of the "flat beige sand" look):
#   * crushed-brick orange-brown base, mean sRGB ~ (176, 74, 44)
#   * large damp/dry blotches (+-10%), fine 1-3 mm grain (+-4%)
#   * BROOM SWEEP PATTERN: wide, wavy, overlapping drag-broom bands
#     (40-80 cm), low contrast (4-8% luminance), with thin 1-2 px rake
#     streaks inside the bands -> must read as swept clay, never as stripes
#   * subtle lighter/desaturated dust near the tile borders
#
# Outputs (into RawAssets\Clay2):
#   T_Clay2_Albedo.png  4096x4096  seamless (sRGB-encoded content)
#   T_Clay2_Rough.png   4096x4096  seamless (linear, no sRGB)
#   T_Clay2_Normal.png  4096x4096  seamless (tangent space, OpenGL/Y+,
#                                   Polyhaven nor_gl style, Z ~0.85-1.0)
#   T_Line2_Albedo.png  1024x1024  seamless worn white line paint
#   T_Line2_Rough.png   1024x1024  seamless (0.55-0.85)
#   T_Clay2_Macro.png   1024x1024  very-low-freq multiplier (0.85-1.15,
#                                   128 = x1.0) to break tiling at distance
#   preview_clay_v2.png            contact sheet (albedo tiling at 2 zooms,
#                                   normal, rough, lambert-lit mockup, stats)
#   stats_clay_v2.txt              full measured stats of the last run
#
# Texture space: the 4096 map represents 4 m x 4 m (1 px ~ 0.98 mm).
#
# Tileability: every noise lattice wraps (its cell count spans the whole
# map, so sampling is periodic with the map size). Sweep bands are pure
# functions of wrapped coordinates (sin of integer harmonics + wrapping
# warp noise), so all maps are exactly periodic. Seam continuity is
# MEASURED per output (left/right and top/bottom mean difference, target
# < 2% of 255) and compared against the average interior pixel step.
#
# Determinism: one fixed seed drives our own xorshift PRNG; re-running
# with identical parameters produces byte-identical PNGs (SHA-256 proof).
#
# Pixel kernels are C# compiled in-memory via Add-Type (System.Drawing
# LockBits + byte[] buffers). No Unreal files are touched.
#
# Tuning knobs (sweep iteration params, see REPORT_CLAY_V2.md):
#   -SweepMainContrast   main horizontal broom-band luminance contrast
#   -SweepH2Contrast     second horizontal pass (66 cm bands)
#   -SweepVContrast      vertical pass (80 cm bands)
#   -SweepDiagContrast   diagonal pass (71 cm bands)
#   -SweepWarpAmpPx      band-edge waviness amplitude in px
#   -StreakContrast      rake-line (1-2 px streak) contrast
#   -NormalScale         height->normal strength
# =============================================================================

[CmdletBinding()]
param(
    [int]$Seed = 20261123,
    [string]$OutDir = 'C:\Projects\ClayTennis\RawAssets\Clay2',
    [double]$SweepMainContrast = 0.085,
    [double]$SweepH2Contrast = 0.032,
    [double]$SweepVContrast = 0.065,
    [double]$SweepDiagContrast = 0.040,
    [double]$SweepWarpAmpPx = 115,
    [double]$StreakContrast = 0.045,
    [double]$NormalScale = 0.45
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ([System.IntPtr]::Size -ne 8) {
    throw 'gen_clay_v2 requires 64-bit PowerShell (the 4096 buffers need ~300 MB).'
}

Add-Type -AssemblyName System.Drawing

$scriptTimer = [System.Diagnostics.Stopwatch]::StartNew()

if (-not (Test-Path -LiteralPath $OutDir)) {
    New-Item -ItemType Directory -Path $OutDir -Force | Out-Null
}

# =============================================================================
# C# pixel kernels (compiled in-memory by Add-Type)
# =============================================================================
$csharpSource = @'
using System;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Imaging;
using System.Globalization;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;

// ---------------------------------------------------------------------------
// Deterministic xorshift64 PRNG. Fixed seed -> identical lattices every run.
// ---------------------------------------------------------------------------
public class Rng
{
    private ulong s;
    public Rng(ulong seed)
    {
        s = seed ^ 0x9E3779B97F4A7C15UL;
        if (s == 0UL) s = 1UL;
        for (int i = 0; i < 16; i++) Next();
    }
    public ulong Next() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; }
    public float NextFloat() { return (float)((double)(Next() >> 11) * (1.0 / 9007199254740992.0)); }
}

// ---------------------------------------------------------------------------
// Wrapping bilinear value noise. The lattice has NCX x NCY cells across the
// FULL map, so Sample() is periodic with the map size (exact tileability).
// ---------------------------------------------------------------------------
public class TileNoise
{
    public int NCX, NCY;
    public float[] Lat;
    private float Sx, Sy;
    public TileNoise(Rng r, int ncx, int ncy, int w, int h)
    {
        NCX = ncx; NCY = ncy;
        Lat = new float[ncx * ncy];
        for (int i = 0; i < Lat.Length; i++) Lat[i] = r.NextFloat();
        Sx = (float)ncx / (float)w;
        Sy = (float)ncy / (float)h;
    }
    public float Sample(float x, float y)
    {
        float fx = x * Sx, fy = y * Sy;
        int ix = (int)fx, iy = (int)fy;
        float tx = fx - ix, ty = fy - iy;
        tx = tx * tx * (3f - 2f * tx);
        ty = ty * ty * (3f - 2f * ty);
        int x0 = ix % NCX; if (x0 < 0) x0 += NCX;
        int y0 = iy % NCY; if (y0 < 0) y0 += NCY;
        int x1 = (x0 + 1) % NCX;
        int y1 = (y0 + 1) % NCY;
        float a = Lat[y0 * NCX + x0], b = Lat[y0 * NCX + x1];
        float c = Lat[y1 * NCX + x0], d = Lat[y1 * NCX + x1];
        float t = a + (b - a) * tx, u = c + (d - c) * tx;
        return t + (u - t) * ty;
    }
}

// Multi-octave FBM built from wrapping TileNoise octaves. Still periodic.
public class Fbm
{
    private TileNoise[] o;
    private float[] a;
    private float tot;
    public Fbm(Rng r, int w, int h, int[] ncx, int[] ncy, float[] amps)
    {
        o = new TileNoise[amps.Length]; a = amps; tot = 0f;
        for (int i = 0; i < amps.Length; i++)
        {
            o[i] = new TileNoise(r, ncx[i], ncy[i], w, h);
            tot += amps[i];
        }
    }
    public float Sample(float x, float y)
    {
        float s = 0f;
        for (int i = 0; i < o.Length; i++) s += a[i] * o[i].Sample(x, y);
        return s / tot;
    }
}

public static class ClayGen
{
    private const float TwoPi = 6.28318531f;
    private static readonly float BR = 176f, BG = 74f, BB = 44f; // target mean sRGB
    private static readonly CultureInfo Inv = CultureInfo.InvariantCulture;

    // ------------------------------------------------------------------ utils
    private static float Clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
    private static float Smoothstep(float e0, float e1, float x)
    {
        float t = Clampf((x - e0) / (e1 - e0), 0f, 1f); return t * t * (3f - 2f * t);
    }
    private static float Lerpf(float a, float b, float t) { return a + (b - a) * t; }
    private static byte ToB(float v) { if (!(v > 0f)) return 0; if (v > 255f) return 255; return (byte)(v + 0.5f); }
    private static byte ScaleByte(byte v, double k)
    {
        double q = (double)v * k; if (q < 0.0) return 0; if (q > 255.0) return 255; return (byte)(q + 0.5);
    }
    private static string F2(double v) { return v.ToString("F2", Inv); }

    // State-free per-pixel white noise (deterministic hash; no big lattices).
    private static uint PHash(uint x, uint y, uint s)
    {
        uint h = s * 0x9E3779B9u + x * 0x85EBCA77u + y * 0xC2B2AE3Du;
        h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
        return h;
    }
    private static float PHash01(uint x, uint y, uint s) { return PHash(x, y, s) * (1f / 4294967296f); }

    // ------------------------------------------------------- save / load 24bpp
    // Internal buffers are packed RGB triplets; GDI+ wants BGR rows.
    private static void SaveRgb24(string path, byte[] rgb, int w, int h)
    {
        using (Bitmap bm = new Bitmap(w, h, PixelFormat.Format24bppRgb))
        {
            BitmapData bd = bm.LockBits(new Rectangle(0, 0, w, h), ImageLockMode.WriteOnly, PixelFormat.Format24bppRgb);
            byte[] rowBuf = new byte[w * 3];
            for (int y = 0; y < h; y++)
            {
                int src = y * w * 3;
                for (int x = 0; x < w; x++)
                {
                    rowBuf[x * 3 + 0] = rgb[src + x * 3 + 2];
                    rowBuf[x * 3 + 1] = rgb[src + x * 3 + 1];
                    rowBuf[x * 3 + 2] = rgb[src + x * 3 + 0];
                }
                Marshal.Copy(rowBuf, 0, (IntPtr)(bd.Scan0.ToInt64() + (long)y * bd.Stride), w * 3);
            }
            bm.UnlockBits(bd);
            bm.Save(path, ImageFormat.Png);
        }
    }

    private static byte[] LoadRgb24(string path, out int w, out int h)
    {
        using (Bitmap bm0 = new Bitmap(path))
        {
            w = bm0.Width; h = bm0.Height;
            Bitmap bm = bm0;
            bool cloned = false;
            if (bm0.PixelFormat != PixelFormat.Format24bppRgb)
            {
                bm = bm0.Clone(new Rectangle(0, 0, w, h), PixelFormat.Format24bppRgb);
                cloned = true;
            }
            byte[] dst = new byte[w * h * 3];
            BitmapData bd = bm.LockBits(new Rectangle(0, 0, w, h), ImageLockMode.ReadOnly, PixelFormat.Format24bppRgb);
            byte[] rowBuf = new byte[w * 3];
            for (int y = 0; y < h; y++)
            {
                Marshal.Copy((IntPtr)(bd.Scan0.ToInt64() + (long)y * bd.Stride), rowBuf, 0, w * 3);
                long dstOff = (long)y * w * 3;
                for (int x = 0; x < w; x++)
                {
                    dst[dstOff + x * 3 + 0] = rowBuf[x * 3 + 2];
                    dst[dstOff + x * 3 + 1] = rowBuf[x * 3 + 1];
                    dst[dstOff + x * 3 + 2] = rowBuf[x * 3 + 0];
                }
            }
            bm.UnlockBits(bd);
            if (cloned) bm.Dispose();
            return dst;
        }
    }

    // --------------------------------------------------------------- statistics
    // Full-channel stats + numeric seam continuity:
    //   LR = mean|col0 - col(W-1)| / 255 * 100  (wrapped seam)
    //   TB = mean|row0 - row(H-1)| / 255 * 100
    //   interior = mean adjacent-column step (sampled), for comparison.
    private static void StatsRGB(string name, byte[] px, int w, int h, StringBuilder log)
    {
        long cnt = (long)w * h;
        double[] sum = new double[3], sq = new double[3];
        double[] mn = new double[] { 255, 255, 255 }, mx = new double[] { 0, 0, 0 };
        for (long i = 0; i < cnt; i++)
        {
            int o = (int)(i * 3);
            for (int c = 0; c < 3; c++)
            {
                double v = px[o + c];
                sum[c] += v; sq[c] += v * v;
                if (v < mn[c]) mn[c] = v;
                if (v > mx[c]) mx[c] = v;
            }
        }
        double[] seamLR = new double[3], seamTB = new double[3];
        for (int y = 0; y < h; y++)
        {
            long a = (long)y * w * 3, b = ((long)y * w + w - 1) * 3;
            for (int c = 0; c < 3; c++) seamLR[c] += Math.Abs(px[a + c] - px[b + c]);
        }
        for (int x = 0; x < w; x++)
        {
            long a = (long)x * 3, b = ((long)(h - 1) * w + x) * 3;
            for (int c = 0; c < 3; c++) seamTB[c] += Math.Abs(px[a + c] - px[b + c]);
        }
        double[] adj = new double[3]; long adjCnt = 0;
        for (int x = 0; x < w - 1; x += 97)
            for (int y = 0; y < h; y += 7)
            {
                long i1 = ((long)y * w + x) * 3, i2 = ((long)y * w + x + 1) * 3;
                for (int c = 0; c < 3; c++) adj[c] += Math.Abs(px[i1 + c] - px[i2 + c]);
                adjCnt++;
            }
        double lrPct = ((seamLR[0] + seamLR[1] + seamLR[2]) / (3.0 * h)) / 255.0 * 100.0;
        double tbPct = ((seamTB[0] + seamTB[1] + seamTB[2]) / (3.0 * w)) / 255.0 * 100.0;
        double adjPct = ((adj[0] + adj[1] + adj[2]) / (3.0 * Math.Max(1, adjCnt))) / 255.0 * 100.0;
        double ratioLR = adjPct > 0.0001 ? lrPct / adjPct : 0.0;
        double ratioTB = adjPct > 0.0001 ? tbPct / adjPct : 0.0;

        double m0 = sum[0] / cnt, m1 = sum[1] / cnt, m2 = sum[2] / cnt;
        double s0 = Math.Sqrt(Math.Max(0.0, sq[0] / cnt - m0 * m0));
        double s1 = Math.Sqrt(Math.Max(0.0, sq[1] / cnt - m1 * m1));
        double s2 = Math.Sqrt(Math.Max(0.0, sq[2] / cnt - m2 * m2));
        StringBuilder sb = new StringBuilder();
        sb.Append("STAT ").Append(name).Append(' ').Append(w).Append('x').Append(h);
        sb.Append(" mean=[").Append(F2(m0)).Append(',').Append(F2(m1)).Append(',').Append(F2(m2)).Append(']');
        sb.Append(" std=[").Append(F2(s0)).Append(',').Append(F2(s1)).Append(',').Append(F2(s2)).Append(']');
        sb.Append(" min=[").Append(mn[0].ToString("F0", Inv)).Append(',').Append(mn[1].ToString("F0", Inv)).Append(',').Append(mn[2].ToString("F0", Inv)).Append(']');
        sb.Append(" max=[").Append(mx[0].ToString("F0", Inv)).Append(',').Append(mx[1].ToString("F0", Inv)).Append(',').Append(mx[2].ToString("F0", Inv)).Append(']');
        log.AppendLine(sb.ToString());
        log.AppendLine(String.Format(Inv,
            "SEAM {0} LR={1:F3}% TB={2:F3}% (target<2.0) interior_step={3:F3}% ratioLR={4:F2} ratioTB={5:F2} {6}",
            name, lrPct, tbPct, adjPct, ratioLR, ratioTB,
            (lrPct < 2.0 && tbPct < 2.0) ? "OK" : "CHECK"));
    }

    // Band-pass (blur r=5 minus blur r=40 on a 1024 downsample = 20..160 px
    // at full res) isolates the sweep scale.
    //   contrastPct = std/mean of the band-pass          -> sweep strength
    //   dirRatio    = mean|d/dx| / mean|d/dy| of it      -> stripe detector:
    //                  ~0 = pure horizontal stripes, ~inf = pure vertical,
    //                  0.5..2.0 = mixed-direction sweeps (a real drag-broomed
    //                  court keeps a dominant direction; < 0.15 = stripes)
    // The window (blur5 - blur64 at 1024 scale = 20..256 px full res)
    // covers all four broom band spacings (50-80 cm).
    private static float[] BoxBlurF(float[] src, int w, int h, int r)
    {
        int win = 2 * r + 1;
        float[] tmp = new float[w * h];
        for (int y = 0; y < h; y++)
        {
            int row = y * w;
            float acc = 0f;
            for (int i = -r; i <= r; i++) { int xx = i % w; if (xx < 0) xx += w; acc += src[row + xx]; }
            for (int x = 0; x < w; x++)
            {
                tmp[row + x] = acc / win;
                int add = x + r + 1; while (add >= w) add -= w;
                int sub = x - r; while (sub < 0) sub += w;
                acc += src[row + add] - src[row + sub];
            }
        }
        float[] dst = new float[w * h];
        for (int x = 0; x < w; x++)
        {
            float acc = 0f;
            for (int i = -r; i <= r; i++) { int yy = i % h; if (yy < 0) yy += h; acc += tmp[yy * w + x]; }
            for (int y = 0; y < h; y++)
            {
                dst[y * w + x] = acc / win;
                int add = y + r + 1; while (add >= h) add -= h;
                int sub = y - r; while (sub < 0) sub += h;
                acc += tmp[add * w + x] - tmp[sub * w + x];
            }
        }
        return dst;
    }

    private static void SweepMetrics(byte[] alb, int W, int H, out double contrastPct, out double dirRatio)
    {
        int dw = W / 4, dh = H / 4;
        float[] L = new float[dw * dh];
        for (int y = 0; y < dh; y++)
            for (int x = 0; x < dw; x++)
            {
                double acc = 0.0;
                for (int yy = 0; yy < 4; yy++)
                    for (int xx = 0; xx < 4; xx++)
                    {
                        int o = ((y * 4 + yy) * W + x * 4 + xx) * 3;
                        acc += 0.299 * alb[o] + 0.587 * alb[o + 1] + 0.114 * alb[o + 2];
                    }
                L[y * dw + x] = (float)(acc / 16.0);
            }
        float[] b1 = BoxBlurF(L, dw, dh, 5);
        float[] b2 = BoxBlurF(b1, dw, dh, 64);
        int cnt = dw * dh;
        double mean = 0.0;
        for (int i = 0; i < cnt; i++) mean += L[i];
        mean /= cnt;
        double s2 = 0.0, gx = 0.0, gy = 0.0;
        for (int y = 0; y < dh; y++)
            for (int x = 0; x < dw; x++)
            {
                int i = y * dw + x;
                double d = b1[i] - b2[i];
                s2 += d * d;
                int xm = (x + dw - 2) % dw, xp = (x + 2) % dw;
                int ym = (y + dh - 2) % dh, yp = (y + 2) % dh;
                gx += Math.Abs((b1[y * dw + xp] - b2[y * dw + xp]) - (b1[y * dw + xm] - b2[y * dw + xm]));
                gy += Math.Abs((b1[yp * dw + x] - b2[yp * dw + x]) - (b1[ym * dw + x] - b2[ym * dw + x]));
            }
        contrastPct = 100.0 * Math.Sqrt(s2 / cnt) / mean;
        dirRatio = gx / Math.Max(1e-9, gy);
    }

    // ======================================================================
    //  CLAY CORE: albedo + roughness + height/normal (4096, = 4 m x 4 m)
    // ======================================================================
    private static void GenerateAndSaveClay(string outDir, int seed,
        float cMain, float cH2, float cV, float cD, float warpAmp, float streakC, float nScale,
        StringBuilder log)
    {
        Stopwatch st = Stopwatch.StartNew();
        int W = 4096, H = 4096;
        long n = (long)W * H;
        byte[] alb = new byte[n * 3];
        byte[] rgh = new byte[n * 3];
        float[] hgt = new float[n];
        uint seedBase = (uint)seed;

        Rng rng = new Rng((ulong)seed);

        // ---- wrapping noise stacks (cell counts are periods over the map) ----
        Fbm blotch = new Fbm(rng, W, H, new int[] { 4, 8, 16 }, new int[] { 4, 8, 16 }, new float[] { 0.5f, 0.3f, 0.2f });   // damp/dry blotches
        Fbm midro = new Fbm(rng, W, H, new int[] { 32, 64 }, new int[] { 32, 64 }, new float[] { 0.6f, 0.4f });              // mid-freq roughness
        Fbm grain = new Fbm(rng, W, H, new int[] { 1024, 2048 }, new int[] { 1024, 2048 }, new float[] { 0.45f, 0.55f });    // 2-4 px brick grain (albedo/rough)
        Fbm hgrain = new Fbm(rng, W, H, new int[] { 512, 1024 }, new int[] { 512, 1024 }, new float[] { 0.55f, 0.45f });     // 4-8 px grain relief for height (smooth slopes)
        TileNoise warpH1 = new TileNoise(rng, 6, 48, W, H);      // broom band waviness (slow)
        TileNoise warpH1f = new TileNoise(rng, 48, 192, W, H);    // finer waviness
        TileNoise warpH2 = new TileNoise(rng, 5, 40, W, H);
        TileNoise warpV = new TileNoise(rng, 48, 5, W, H);
        TileNoise warpD = new TileNoise(rng, 10, 10, W, H);
        TileNoise streakH = new TileNoise(rng, 256, 2048, W, H);  // rake lines: 16 px x 2 px cells
        TileNoise streakV = new TileNoise(rng, 2048, 256, W, H);  // rake lines: 2 px x 16 px cells
        Fbm edgeBlotch = new Fbm(rng, W, H, new int[] { 16, 32 }, new int[] { 16, 32 }, new float[] { 0.5f, 0.5f });

        // Fixed long-wave warp (integer harmonics => exactly periodic).
        float[] sxA1 = new float[W], sxA2 = new float[W], sxB1 = new float[W], sxB2 = new float[W];
        for (int x = 0; x < W; x++)
        {
            float fx = x;
            sxA1[x] = (float)Math.Sin(TwoPi * 3 * fx / W + 1.1);
            sxA2[x] = (float)Math.Sin(TwoPi * 7 * fx / W + 4.2);
            sxB1[x] = (float)Math.Sin(TwoPi * 2 * fx / W + 0.7);
            sxB2[x] = (float)Math.Sin(TwoPi * 5 * fx / W + 3.3);
        }
        float[] syA = new float[H], syB = new float[H];
        for (int y = 0; y < H; y++)
        {
            float fy = y;
            syA[y] = (float)Math.Sin(TwoPi * 4 * fy / H + 2.0);
            syB[y] = (float)Math.Sin(TwoPi * 9 * fy / H + 5.1);
        }

        float Wf = (float)W;
        float k1 = 8f / Wf;    // H1: 8 bands -> 512 px = 50 cm
        float k2 = 6f / Wf;     // H2: 6 bands -> 683 px = 66 cm
        float k3 = 5f / Wf;     // V1: 5 bands -> 819 px = 80 cm
        float k4 = 8f / Wf;     // D1: 8 bands along 0.5*(x+y) -> 724 px perp = 71 cm
        float gray = 0.299f * BR + 0.587f * BG + 0.114f * BB;

        // ------------------------------------------------------ main pass ---
        for (int y = 0; y < H; y++)
        {
            float fy = y;
            float edgeY = y < H - 1 - y ? y : (H - 1 - y);
            int row = y * W;
            for (int x = 0; x < W; x++)
            {
                int idx = row + x;
                int o = idx * 3;
                float fx = x;

                // damp/dry blotches (large scale, +-10%)
                float b2 = Clampf((blotch.Sample(x, fy) - 0.5f) * 2.6f + 0.5f, 0f, 1f);
                float wet = Smoothstep(0.58f, 0.80f, b2);
                float lum = 1f + (b2 - 0.5f) * 0.18f - 0.07f * wet;
                float sat = 1f + 0.30f * wet - 0.08f * Smoothstep(0.45f, 0.90f, b2);

                // fine grain (+-4%) + 1 px white noise
                float gAmp = (grain.Sample(x, fy) - 0.5f) * 2f;
                lum *= 1f + 0.075f * gAmp;
                float wh = PHash01((uint)x, (uint)y, seedBase ^ 0xA5u) - 0.5f;
                lum *= 1f + 0.02f * wh;

                // ---- broom sweep layers (wrapped coordinates => tileable) ----
                // H1 main drag-broom: wavy bands, asymmetric profile
                float d1 = fy + (warpH1.Sample(x, fy) - 0.5f) * (2f * warpAmp)
                         + (warpH1f.Sample(x, fy) - 0.5f) * 70f
                         + sxA1[x] * 38f + sxA2[x] * 20f;
                float u1 = d1 * k1 + 0.13f;
                float s1 = (float)Math.Sin(u1 * TwoPi) + 0.40f * (float)Math.Sin(u1 * 2f * TwoPi + 0.9f);
                // H2 second horizontal pass, different spacing/phase
                float d2 = fy + (warpH2.Sample(x, fy) - 0.5f) * (1.5f * warpAmp) + sxB1[x] * 30f + sxB2[x] * 16f;
                float u2 = d2 * k2 + 0.62f;
                float s2 = (float)Math.Sin(u2 * TwoPi) + 0.30f * (float)Math.Sin(u2 * 2f * TwoPi + 2.1f);
                // V1 vertical pass
                float dv = fx + (warpV.Sample(x, fy) - 0.5f) * (1.6f * warpAmp) + syA[y] * 34f + syB[y] * 18f;
                float u3 = dv * k3 + 0.35f;
                float s3 = (float)Math.Sin(u3 * TwoPi) + 0.30f * (float)Math.Sin(u3 * 2f * TwoPi + 4.4f);
                // D1 diagonal pass
                float dd = (fx + fy) * 0.5f + (warpD.Sample(x, fy) - 0.5f) * 110f;
                float u4 = dd * k4 + 0.77f;
                float s4 = (float)Math.Sin(u4 * TwoPi) + 0.25f * (float)Math.Sin(u4 * 2f * TwoPi + 1.7f);

                // rake lines: 1-2 px streaks ALONG each pass direction,
                // sharpened so they read as thin dragged lines
                float stHs = Smoothstep(-0.38f, 0.38f, (streakH.Sample(x, fy) - 0.5f) * 2f) - 0.5f;
                float stVs = Smoothstep(-0.38f, 0.38f, (streakV.Sample(x, fy) - 0.5f) * 2f) - 0.5f;
                float sweepLum = cMain * s1 + cH2 * s2 + cV * s3 + cD * s4
                    + streakC * 2f * stHs * (0.55f + 0.45f * (0.5f + 0.5f * s1))
                    + streakC * 1.6f * stVs * (0.55f + 0.45f * (0.5f + 0.5f * s3));
                sweepLum = Clampf(sweepLum, -0.10f, 0.10f);
                lum *= 1f + sweepLum;
                sat *= 1f - 0.25f * Math.Max(0f, sweepLum);   // dust side lighter + desaturated

                // dust accumulation near the tile border (subtle, wrapped)
                float dxE = x < W - 1 - x ? x : (W - 1 - x);
                float dEdge = dxE < edgeY ? dxE : edgeY;
                float e = Smoothstep(340f, 30f, dEdge) * (0.35f + 0.65f * edgeBlotch.Sample(x, fy));
                lum *= 1f + 0.035f * e;
                sat *= 1f - 0.10f * e;

                // sparse dark brick specks + tiny light quartz grains
                float spk = PHash01((uint)x, (uint)y, seedBase ^ 0x51u);
                if (spk < 0.0010f)
                {
                    lum *= 0.45f + 0.35f * PHash01((uint)x, (uint)y, seedBase ^ 0x52u);
                    sat *= 1.05f;
                }
                else
                {
                    float lgr = PHash01((uint)x, (uint)y, seedBase ^ 0x77u);
                    if (lgr < 0.0020f) { lum *= 1.16f; sat *= 0.82f; }
                }

                // compose albedo (mean later gain-normalized to the target)
                float cr = (gray + (BR - gray) * sat) * (1f + 0.025f * (b2 - 0.5f)) * lum;
                float cg = (gray + (BG - gray) * sat) * lum;
                float cb = (gray + (BB - gray) * sat) * (1f - 0.035f * (b2 - 0.5f)) * lum;
                alb[o] = ToB(cr); alb[o + 1] = ToB(cg); alb[o + 2] = ToB(cb);

                // roughness 0.72-0.95, sweeps +-0.05, damp down to 0.62
                float rough = 0.835f + (midro.Sample(x, fy) - 0.5f) * 0.40f + gAmp * 0.045f + wh * 0.012f + sweepLum * 0.55f;
                rough = Lerpf(rough, 0.62f, wet);
                rough = Clampf(rough, 0.62f, 0.955f);
                byte rb = ToB(rough * 255f);
                rgh[o] = rb; rgh[o + 1] = rb; rgh[o + 2] = rb;

                // height = smooth grain relief + sweep ridges (subtle) + rake relief.
                // No 1 px white noise here: it makes the normal map sparkle
                // and pushes the seam/interior step metric above 2%.
                float hg = (hgrain.Sample(x, fy) - 0.5f) * 2f;
                float ridge = (0.5f + 0.5f * s1) * 0.50f + (0.5f + 0.5f * s3) * 0.18f;
                hgt[idx] = hg * 1.15f * (1f - 0.45f * wet)   // damp clay is compacted/smoother
                         + (b2 - 0.5f) * 1.5f + ridge
                         + stHs * 0.25f + stVs * 0.18f;
            }
        }

        // sparse pebbles (2-6 mm chips): stamped into height + tiny dark dots
        int pebCount = 1500;
        for (int i = 0; i < pebCount; i++)
        {
            float px = rng.NextFloat() * W, py = rng.NextFloat() * H;
            float rad = 2.0f + rng.NextFloat() * 3.5f;
            float amp = 1.4f + rng.NextFloat() * 1.1f;
            int ix = (int)px, iy = (int)py;
            int r = (int)Math.Ceiling(rad * 1.6f);
            for (int ddy = -r; ddy <= r; ddy++)
                for (int ddx = -r; ddx <= r; ddx++)
                {
                    float rx = ix + ddx - px, ry = iy + ddy - py;
                    float q = (rx * rx + ry * ry) / (rad * rad);
                    if (q > 2.6f) continue;
                    float fall = (float)Math.Exp(-q * 1.4f);
                    int xx = (ix + ddx + W) % W, yy = (iy + ddy + H) % H;
                    long id = (long)yy * W + xx;
                    hgt[id] += amp * fall;
                    int o = (int)(id * 3);
                    float k = 1f - 0.16f * fall;
                    alb[o] = ScaleByte(alb[o], k);
                    alb[o + 1] = ScaleByte(alb[o + 1], k);
                    alb[o + 2] = ScaleByte(alb[o + 2], k);
                    for (int c = 0; c < 3; c++)
                    {
                        byte bumped = ScaleByte(rgh[o + c], 1.0 + 0.07 * fall);
                        if (bumped > 243) bumped = 243;          // keep rough <= ~0.95
                        rgh[o + c] = bumped;
                    }
                }
        }

        // ------------------------------------------------- normal from height
        // Tangent space, OpenGL/Y+ (Polyhaven nor_gl): R = X, G = +Y with
        // image-down flipped (flip green for DirectX/UE default import).
        byte[] nrm = new byte[n * 3];
        float maxS = 0.6196f; // keeps Z >= 0.85
        for (int y = 0; y < H; y++)
        {
            int yu = (y + H - 1) % H, yd = (y + 1) % H;
            int rowU = yu * W, rowD = yd * W, row = y * W;
            for (int x = 0; x < W; x++)
            {
                int xl = (x + W - 1) % W, xr = (x + 1) % W;
                float hl = hgt[row + xl], hr = hgt[row + xr];
                float hT = hgt[rowU + x], hB = hgt[rowD + x];
                float nx = nScale * (hl - hr);
                float ny = nScale * (hB - hT);
                float sl2 = nx * nx + ny * ny;
                if (sl2 > maxS * maxS) { float scl = maxS / (float)Math.Sqrt(sl2); nx *= scl; ny *= scl; }
                float nz = 1f / (float)Math.Sqrt(nx * nx + ny * ny + 1f);
                int o = (row + x) * 3;
                nrm[o] = ToB(127.5f + nx * 127.5f);
                nrm[o + 1] = ToB(127.5f + ny * 127.5f);
                nrm[o + 2] = ToB(nz * 255f);
            }
        }

        // -------------------------------------- albedo mean gain-normalization
        double sr = 0.0, sg = 0.0, sb = 0.0;
        for (long i = 0; i < n; i++)
        {
            int o = (int)(i * 3);
            sr += alb[o]; sg += alb[o + 1]; sb += alb[o + 2];
        }
        double gR = (176.0 * n) / Math.Max(1.0, sr);
        double gG = (74.0 * n) / Math.Max(1.0, sg);
        double gB = (44.0 * n) / Math.Max(1.0, sb);
        for (long i = 0; i < n; i++)
        {
            int o = (int)(i * 3);
            alb[o] = ScaleByte(alb[o], gR);
            alb[o + 1] = ScaleByte(alb[o + 1], gG);
            alb[o + 2] = ScaleByte(alb[o + 2], gB);
        }
        log.AppendLine("ALBEDO_MEAN_GAIN R=" + F2(gR) + " G=" + F2(gG) + " B=" + F2(gB) + " (1.00 = untouched)");

        // ------------------------------------------------------ stats + save --
        StatsRGB("T_Clay2_Albedo", alb, W, H, log);
        double sc, dr;
        SweepMetrics(alb, W, H, out sc, out dr);
        log.AppendLine("SWEEP_CONTRAST_PERCENT " + F2(sc) + " (bandpass 20-256px std/mean, target 4-8%)");
        log.AppendLine("SWEEP_DIRECTION_RATIO " + F2(dr) + " (gx/gy of bandpass; 0.5-2.0 mixed OK, <0.15 or >6 = stripes)");
        log.AppendLine("SWEEP_BANDS H1=8/512px/50cm H2=6/683px/66cm V=5/819px/80cm DIAG=8/724px/71cm");
        StatsRGB("T_Clay2_Rough", rgh, W, H, log);
        double zmin = 1.0, zsum = 0.0;
        for (long i = 0; i < n; i++)
        {
            double z = nrm[(int)(i * 3) + 2] / 255.0;
            zsum += z; if (z < zmin) zmin = z;
        }
        log.AppendLine("NORMAL_Z mean=" + F2(zsum / n) + " min=" + F2(zmin) + " (target 0.85-1.0, OpenGL Y+)");
        StatsRGB("T_Clay2_Normal", nrm, W, H, log);

        SaveRgb24(Path.Combine(outDir, "T_Clay2_Albedo.png"), alb, W, H);
        SaveRgb24(Path.Combine(outDir, "T_Clay2_Rough.png"), rgh, W, H);
        SaveRgb24(Path.Combine(outDir, "T_Clay2_Normal.png"), nrm, W, H);
        st.Stop();
        log.AppendLine("STAGE clay_core_s " + st.Elapsed.TotalSeconds.ToString("F1", Inv));
    }

    // ======================================================================
    //  LINE: worn white court-line paint (1024, band across the image)
    // ======================================================================
    private static void GenerateAndSaveLine(string outDir, int seed, StringBuilder log)
    {
        Stopwatch st = Stopwatch.StartNew();
        int W = 1024, H = 1024;
        long n = (long)W * H;
        byte[] alb = new byte[n * 3];
        byte[] rgh = new byte[n * 3];

        Rng rng = new Rng((ulong)seed + 7777UL);
        Fbm lbg = new Fbm(rng, W, H, new int[] { 4, 8, 16 }, new int[] { 4, 8, 16 }, new float[] { 0.45f, 0.33f, 0.22f });
        TileNoise lgrain = new TileNoise(rng, 512, 512, W, H);   // 2 px chalk grain
        Fbm lmott = new Fbm(rng, W, H, new int[] { 64, 128 }, new int[] { 64, 128 }, new float[] { 0.5f, 0.5f });
        TileNoise lthick = new TileNoise(rng, 4, 1, W, H);       // thickness varies ALONG X only
        TileNoise lrag1 = new TileNoise(rng, 64, 256, W, H);     // ragged edge, 16 px cells
        TileNoise lrag2 = new TileNoise(rng, 512, 1024, W, H);   // fine 2 px jitter
        Fbm lwear = new Fbm(rng, W, H, new int[] { 16, 32 }, new int[] { 16, 32 }, new float[] { 0.55f, 0.45f });
        Fbm ldust = new Fbm(rng, W, H, new int[] { 8, 32 }, new int[] { 8, 32 }, new float[] { 0.5f, 0.5f });

        for (int y = 0; y < H; y++)
        {
            float fy = y;
            for (int x = 0; x < W; x++)
            {
                int o = (y * W + x) * 3;
                float bg = Clampf((lbg.Sample(x, fy) - 0.5f) * 2f + 0.5f, 0f, 1f);
                float chalk = lgrain.Sample(x, fy);
                float mott = lmott.Sample(x, fy);

                // clay background (matches court mean tone so strip edges blend)
                float clayL = 1f + (bg - 0.5f) * 0.12f + (chalk - 0.5f) * 0.10f;
                float cr = 168f * clayL, cg = 70f * clayL, cb = 42f * clayL;

                // line band: ~55% of height, thickness varies along x,
                // edges ragged by 2-4 px (two raggedness frequencies)
                float ay = Math.Abs(fy - 512f);
                float halfT = 281f * (0.94f + 0.12f * (lthick.Sample(x, 0.5f) - 0.5f) * 2f);
                float rag = (lrag1.Sample(x, fy) - 0.5f) * 5.2f + (lrag2.Sample(x, fy) - 0.5f) * 2.8f;
                float dist = ay - halfT - rag;
                float cov = Smoothstep(1.5f, -2.0f, dist);

                // worn patches + extra wear near the band borders
                float wear = Clampf((lwear.Sample(x, fy) - 0.52f) * 4.5f, 0f, 1f);
                float ewear = Smoothstep(halfT - 26f, halfT + 2f, ay) * 0.55f;
                float wearT = Clampf(wear * 0.9f + ewear, 0f, 1f);
                float p = cov * (1f - 0.88f * wearT);

                // chalky paint + warm dust tint (dustier near edges/patches)
                float pl = 1f + (chalk - 0.5f) * 0.16f + (mott - 0.5f) * 0.10f;
                float pr = 242f * pl, pg = 238f * pl, pb = 231f * pl;
                float dust = 0.10f + 0.45f * Smoothstep(0.55f, 0.85f, ldust.Sample(x, fy))
                           + 0.20f * Smoothstep(halfT - 45f, halfT - 5f, ay);
                dust = Clampf(dust, 0f, 0.85f);
                pr = Lerpf(pr, 198f, dust * 0.5f);
                pg = Lerpf(pg, 152f, dust * 0.5f);
                pb = Lerpf(pb, 106f, dust * 0.5f);
                // soft shoulder: chalky grain peaks stay below pure 255
                if (pr > 248f) pr = 248f + (pr - 248f) * 0.25f;
                if (pg > 248f) pg = 248f + (pg - 248f) * 0.25f;
                if (pb > 248f) pb = 248f + (pb - 248f) * 0.25f;

                alb[o] = ToB(Lerpf(cr, pr, p));
                alb[o + 1] = ToB(Lerpf(cg, pg, p));
                alb[o + 2] = ToB(Lerpf(cb, pb, p));

                // rough: paint 0.55-0.66, worn-through clay up to 0.85
                float paintR = 0.60f + (chalk - 0.5f) * 0.12f + (mott - 0.5f) * 0.06f;
                float clayR = 0.84f + (bg - 0.5f) * 0.10f + (chalk - 0.5f) * 0.08f;
                float rough = Clampf(Lerpf(clayR, paintR, p), 0.55f, 0.85f);
                byte rb = ToB(rough * 255f);
                rgh[o] = rb; rgh[o + 1] = rb; rgh[o + 2] = rb;
            }
        }
        StatsRGB("T_Line2_Albedo", alb, W, H, log);
        StatsRGB("T_Line2_Rough", rgh, W, H, log);
        SaveRgb24(Path.Combine(outDir, "T_Line2_Albedo.png"), alb, W, H);
        SaveRgb24(Path.Combine(outDir, "T_Line2_Rough.png"), rgh, W, H);
        st.Stop();
        log.AppendLine("STAGE line_s " + st.Elapsed.TotalSeconds.ToString("F1", Inv));
    }

    // ======================================================================
    //  MACRO: very-low-frequency multiplier map (1024, 128 = x1.0)
    // ======================================================================
    private static void GenerateAndSaveMacro(string outDir, int seed, StringBuilder log)
    {
        Stopwatch st = Stopwatch.StartNew();
        int W = 1024, H = 1024;
        byte[] mac = new byte[W * H * 3];
        Rng rng = new Rng((ulong)seed + 9999UL);
        Fbm mf = new Fbm(rng, W, H, new int[] { 2, 4, 8 }, new int[] { 2, 4, 8 }, new float[] { 0.5f, 0.3f, 0.2f });
        for (int y = 0; y < H; y++)
        {
            float fy = y;
            for (int x = 0; x < W; x++)
            {
                float m = Clampf((mf.Sample(x, fy) - 0.5f) * 2.4f + 0.5f, 0f, 1f);
                byte v = ToB(128f + (m - 0.5f) * 250f);
                int o = (y * W + x) * 3;
                mac[o] = v; mac[o + 1] = v; mac[o + 2] = v;
            }
        }
        StatsRGB("T_Clay2_Macro", mac, W, H, log);
        SaveRgb24(Path.Combine(outDir, "T_Clay2_Macro.png"), mac, W, H);
        st.Stop();
        log.AppendLine("STAGE macro_s " + st.Elapsed.TotalSeconds.ToString("F1", Inv));
    }

    // --------------------------------------------------------- saved verify
    private static void VerifySaved(string outDir, StringBuilder log)
    {
        string[] names = new string[] { "T_Clay2_Albedo.png", "T_Clay2_Rough.png", "T_Clay2_Normal.png",
                                        "T_Line2_Albedo.png", "T_Line2_Rough.png", "T_Clay2_Macro.png" };
        int[] ew = new int[] { 4096, 4096, 4096, 1024, 1024, 1024 };
        for (int i = 0; i < names.Length; i++)
        {
            string p = Path.Combine(outDir, names[i]);
            if (!File.Exists(p)) { log.AppendLine("VERIFY " + names[i] + " MISSING!"); continue; }
            int w, h;
            byte[] px = LoadRgb24(p, out w, out h);
            bool dimsOk = (w == ew[i] && h == ew[i]);
            double mean = 0.0, mn = 255.0, mx = 0.0; long cnt = 0;
            for (long j = 0; j + 2 < px.Length; j += 21)
            {
                double v = px[j];
                mean += v; cnt++;
                if (v < mn) mn = v; if (v > mx) mx = v;
            }
            mean /= Math.Max(1, cnt);
            bool sane = mx > 5.0 && !(mn >= 250.0 && mx <= 255.0);
            log.AppendLine("VERIFY " + names[i] + " dims_ok=" + dimsOk + " sampledMeanR=" + F2(mean)
                + " minR=" + mn.ToString("F0", Inv) + " maxR=" + mx.ToString("F0", Inv) + " sane=" + sane);
        }
    }

    // ======================================================================
    //  PREVIEW contact sheet (2048x1536)
    // ======================================================================
    private static byte[] BoxDown3(byte[] src, int w, int h, int f)
    {
        int dw = w / f, dh = h / f;
        byte[] dst = new byte[dw * dh * 3];
        double inv = 1.0 / (f * f);
        for (int y = 0; y < dh; y++)
            for (int x = 0; x < dw; x++)
            {
                int r = 0, g = 0, b = 0;
                for (int yy = 0; yy < f; yy++)
                {
                    int row = ((y * f + yy) * w + x * f) * 3;
                    for (int xx = 0; xx < f; xx++)
                    {
                        r += src[row]; g += src[row + 1]; b += src[row + 2];
                        row += 3;
                    }
                }
                int o = (y * dw + x) * 3;
                dst[o] = ToB((float)(r * inv));
                dst[o + 1] = ToB((float)(g * inv));
                dst[o + 2] = ToB((float)(b * inv));
            }
        return dst;
    }

    private static byte[] Crop3(byte[] src, int W, int sx, int sy, int cw, int ch)
    {
        byte[] dst = new byte[cw * ch * 3];
        for (int y = 0; y < ch; y++)
        {
            int sOff = ((sy + y) * W + sx) * 3;
            int dOff = y * cw * 3;
            for (int x = 0; x < cw * 3; x++) dst[dOff + x] = src[sOff + x];
        }
        return dst;
    }

    private static void Blit(byte[] dst, int dstW, int dx, int dy, byte[] src, int srcW, int srcH)
    {
        for (int y = 0; y < srcH; y++)
        {
            int d = ((dy + y) * dstW + dx) * 3;
            int s = y * srcW * 3;
            for (int x = 0; x < srcW * 3; x++) dst[d + x] = src[s + x];
        }
    }

    private static void DrawLabel(Graphics g, Font f, string txt, int x, int y)
    {
        using (SolidBrush bg = new SolidBrush(Color.FromArgb(170, 0, 0, 0)))
        {
            g.FillRectangle(bg, (float)x, (float)y, 508f, 20f);
            g.DrawString(txt, f, Brushes.White, (float)x + 6, (float)y + 3);
        }
    }

    private static void BuildPreview(string outDir, string previewPath, StringBuilder log)
    {
        Stopwatch st = Stopwatch.StartNew();
        int aW, aH, nW, nH, rW, rH, lW, lH, mW, mH;
        byte[] alb = LoadRgb24(Path.Combine(outDir, "T_Clay2_Albedo.png"), out aW, out aH);
        byte[] nrm = LoadRgb24(Path.Combine(outDir, "T_Clay2_Normal.png"), out nW, out nH);
        byte[] rgh = LoadRgb24(Path.Combine(outDir, "T_Clay2_Rough.png"), out rW, out rH);
        byte[] lin = LoadRgb24(Path.Combine(outDir, "T_Line2_Albedo.png"), out lW, out lH);
        byte[] mac = LoadRgb24(Path.Combine(outDir, "T_Clay2_Macro.png"), out mW, out mH);

        int SW = 2048, SH = 1536;
        byte[] sheet = new byte[SW * SH * 3];
        for (int i = 0; i < SW * SH; i++)
        {
            sheet[i * 3] = 38; sheet[i * 3 + 1] = 38; sheet[i * 3 + 2] = 42;
        }

        // Row 1: albedo tiled at two zoom levels, full-res crop, lit mockup
        byte[] tileA1 = BoxDown3(alb, aW, aH, 16);   // 256 px per 4 m tile
        for (int ty = 0; ty < 2; ty++)
            for (int tx = 0; tx < 2; tx++) Blit(sheet, SW, tx * 256, ty * 256, tileA1, 256, 256);
        byte[] tileA2 = BoxDown3(alb, aW, aH, 32);   // 128 px per tile
        for (int ty = 0; ty < 4; ty++)
            for (int tx = 0; tx < 4; tx++) Blit(sheet, SW, 512 + tx * 128, ty * 128, tileA2, 128, 128);
        byte[] zoom = Crop3(alb, aW, 1792, 1792, 512, 512);
        Blit(sheet, SW, 1024, 0, zoom, 512, 512);

        // Simulated-lit mockup: albedo x simple lambert x macro multiplier
        byte[] litA = BoxDown3(alb, aW, aH, 8);
        byte[] litN = BoxDown3(nrm, nW, nH, 8);
        byte[] litM = BoxDown3(mac, mW, mH, 2);
        byte[] lit = new byte[512 * 512 * 3];
        double Lx = -0.42, Ly = -0.58, Lz = 0.70;
        for (int i = 0; i < 512 * 512; i++)
        {
            int o = i * 3;
            double nx = litN[o] / 127.5 - 1.0;
            double ny = -(litN[o + 1] / 127.5 - 1.0); // GL Y+ -> image-space y down
            double nz = litN[o + 2] / 255.0;
            double ln = Math.Sqrt(nx * nx + ny * ny + nz * nz);
            if (ln < 0.2) ln = 0.2;
            nx /= ln; ny /= ln; nz /= ln;
            double d = nx * Lx + ny * Ly + nz * Lz;
            if (d < 0) d = 0;
            double shade = 0.32 + 0.75 * d;
            double m = 0.85 + 0.30 * (litM[o] / 255.0);
            for (int c = 0; c < 3; c++) lit[o + c] = ToB((float)(litA[o + c] * shade * m));
        }
        Blit(sheet, SW, 1536, 0, lit, 512, 512);

        // Row 2: normal, roughness, line, macro
        Blit(sheet, SW, 0, 512, BoxDown3(nrm, nW, nH, 8), 512, 512);
        Blit(sheet, SW, 512, 512, BoxDown3(rgh, rW, rH, 8), 512, 512);
        Blit(sheet, SW, 1024, 512, BoxDown3(lin, lW, lH, 2), 512, 512);
        Blit(sheet, SW, 1536, 512, BoxDown3(mac, mW, mH, 2), 512, 512);

        using (Bitmap bm = new Bitmap(SW, SH, PixelFormat.Format24bppRgb))
        {
            BitmapData bd = bm.LockBits(new Rectangle(0, 0, SW, SH), ImageLockMode.WriteOnly, PixelFormat.Format24bppRgb);
            byte[] rowBuf = new byte[SW * 3];
            for (int y = 0; y < SH; y++)
            {
                int src = y * SW * 3;
                for (int x = 0; x < SW; x++)
                {
                    rowBuf[x * 3 + 0] = sheet[src + x * 3 + 2];
                    rowBuf[x * 3 + 1] = sheet[src + x * 3 + 1];
                    rowBuf[x * 3 + 2] = sheet[src + x * 3 + 0];
                }
                Marshal.Copy(rowBuf, 0, (IntPtr)(bd.Scan0.ToInt64() + (long)y * bd.Stride), SW * 3);
            }
            bm.UnlockBits(bd);

            using (Graphics g = Graphics.FromImage(bm))
            {
                using (Font fLabel = new Font("Arial", 11f, FontStyle.Bold))
                using (Font fMono = new Font("Consolas", 12f))
                using (Pen pen = new Pen(Color.FromArgb(90, 255, 255, 255)))
                using (SolidBrush statBg = new SolidBrush(Color.FromArgb(235, 24, 24, 28)))
                {
                    DrawLabel(g, fLabel, "Albedo 2x2 tiles (8x8 m, ~2 m view)", 0, 0);
                    DrawLabel(g, fLabel, "Albedo 4x4 tiles (16x16 m, ~6 m view)", 512, 0);
                    DrawLabel(g, fLabel, "Albedo full-res crop (0.5x0.5 m)", 1024, 0);
                    DrawLabel(g, fLabel, "Simulated lambert light x macro", 1536, 0);
                    DrawLabel(g, fLabel, "Normal (OpenGL Y+, nor_gl style)", 0, 512);
                    DrawLabel(g, fLabel, "Roughness (linear)", 512, 512);
                    DrawLabel(g, fLabel, "T_Line2_Albedo", 1024, 512);
                    DrawLabel(g, fLabel, "T_Clay2_Macro (x1.0 at 128)", 1536, 512);
                    int[,] rects = new int[,] { { 0, 0 }, { 512, 0 }, { 1024, 0 }, { 1536, 0 },
                                                { 0, 512 }, { 512, 512 }, { 1024, 512 }, { 1536, 512 } };
                    for (int i = 0; i < 8; i++) g.DrawRectangle(pen, rects[i, 0], rects[i, 1], 511, 511);

                    g.FillRectangle(statBg, 0f, 1024f, (float)SW, 512f);
                    g.DrawString("MEASURED STATS (full log: stats_clay_v2.txt)",
                        fMono, Brushes.Gold, 14f, 1032f);
                    string[] lines = log.ToString().Split(new char[] { '\r', '\n' }, StringSplitOptions.RemoveEmptyEntries);
                    float ty = 1056f;
                    for (int i = 0; i < lines.Length && i < 26; i++)
                    {
                        g.DrawString(lines[i], fMono, Brushes.Gainsboro, 14f, ty);
                        ty += 17.5f;
                    }
                }
            }
            bm.Save(previewPath, ImageFormat.Png);
        }
        st.Stop();
        log.AppendLine("STAGE preview_s " + st.Elapsed.TotalSeconds.ToString("F1", Inv));
    }

    // ======================================================================
    //  Entry point
    // ======================================================================
    public static string Run(string outDir, int seed,
        float cMain, float cH2, float cV, float cD, float warpAmp, float streakC, float nScale)
    {
        StringBuilder log = new StringBuilder();
        log.AppendLine("gen_clay_v2 seed=" + seed + " outDir=" + outDir);
        log.AppendLine(String.Format(Inv,
            "PARAMS cMain={0} cH2={1} cV={2} cD={3} warpPx={4} streak={5} nScale={6}",
            cMain, cH2, cV, cD, warpAmp, streakC, nScale));
        Directory.CreateDirectory(outDir);
        GenerateAndSaveClay(outDir, seed, cMain, cH2, cV, cD, warpAmp, streakC, nScale, log);
        GenerateAndSaveLine(outDir, seed, log);
        GenerateAndSaveMacro(outDir, seed, log);
        VerifySaved(outDir, log);
        GC.Collect();
        GC.WaitForPendingFinalizers();
        GC.Collect();
        BuildPreview(outDir, Path.Combine(outDir, "preview_clay_v2.png"), log);
        return log.ToString();
    }
}
'@

if (-not ([System.Management.Automation.PSTypeName]'ClayGen').Type) {
    Add-Type -TypeDefinition $csharpSource -ReferencedAssemblies 'System.Drawing'
}

# =============================================================================
# Run the generator
# =============================================================================
$log = [ClayGen]::Run(
    $OutDir, $Seed,
    [single]$SweepMainContrast, [single]$SweepH2Contrast, [single]$SweepVContrast, [single]$SweepDiagContrast,
    [single]$SweepWarpAmpPx, [single]$StreakContrast, [single]$NormalScale)

$scriptTimer.Stop()

# Console + stats file
$logPath = Join-Path $OutDir 'stats_clay_v2.txt'
$log | Out-File -FilePath $logPath -Encoding ascii
$log | Write-Host

("RUNTIME_TOTAL_SECONDS {0:F1}" -f $scriptTimer.Elapsed.TotalSeconds) | Write-Host
"" | Write-Host
"FILES:" | Write-Host
Get-ChildItem $OutDir -File | Sort-Object Name | ForEach-Object {
    $hash = (Get-FileHash -Path $_.FullName -Algorithm SHA256).Hash
    ("{0}  {1,12:N0} bytes  SHA256 {2}" -f $_.Name, $_.Length, $hash) | Write-Host
}
