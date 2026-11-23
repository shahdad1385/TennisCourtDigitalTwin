# =============================================================================
# gen_textures.ps1
# -----------------------------------------------------------------------------
# Procedural PBR texture set for the ClayTennis UE 5.5 clay-court simulation.
#
#   * T_Clay_Detail_*     2048x2048 seamless crushed-brick clay (albedo/normal/rough)
#   * T_LinePaint_*       1024x1024 seamless worn white line paint
#   * T_Net_*             1024x1024 RGBA tennis net + normal
#   * T_Ball_*            1024x1024 optic-yellow felt ball + seam
#   * T_RacketStrings_*    512x512  RGBA string bed
#   * T_Crowd_*           1024x512  blurred stadium crowd
#   * T_Court_Logo_*      1024x1024 RGBA tournament logo
#
# The per-pixel kernels are written in C# and compiled once with Add-Type, then
# driven from PowerShell. This keeps the script a single .ps1 while still using
# System.Drawing LockBits + byte[] buffers (GetPixel/SetPixel would be far too
# slow at 2048x2048). All noise is our own lattice value noise, generated from a
# fixed System.Random seed so re-running produces identical files.
#
# The script also verifies every output (size, alpha, mean), writes a manifest
# with SHA-256 hashes, and renders a contact-sheet preview.
# =============================================================================

[CmdletBinding()]
param(
    [string]$Root = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Resolve the project root once, so the script works from any working directory.
if ([string]::IsNullOrWhiteSpace($Root)) {
    if ([string]::IsNullOrWhiteSpace($PSScriptRoot)) {
        $Root = (Get-Location).Path
    }
    else {
        $Root = Split-Path -Parent $PSScriptRoot
    }
}

Add-Type -AssemblyName System.Drawing

$scriptTimer = [System.Diagnostics.Stopwatch]::StartNew()

$textureDir = Join-Path $Root 'RawAssets\Textures'
if (-not (Test-Path -LiteralPath $textureDir)) {
    New-Item -ItemType Directory -Path $textureDir -Force | Out-Null
}

# =============================================================================
# C# pixel kernels (compiled in-memory by Add-Type)
# =============================================================================
$csharpSource = @'
using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Drawing.Text;
using System.IO;
using System.Runtime.InteropServices;

namespace ClayCourt
{
    // Seamless value noise on a rectangular lattice. Coordinates are given in
    // lattice units; wrapping happens modulo the lattice period, so a texture
    // sampled over u,v in [0,1) tiles perfectly.
    public sealed class NoiseLayer
    {
        private readonly int px;
        private readonly int py;
        private readonly float[] lat;

        // Fills the lattice with deterministic values from the seeded Random.
        public NoiseLayer(int seed, int periodX, int periodY)
        {
            px = periodX;
            py = periodY;
            lat = new float[px * py];
            Random rng = new Random(seed);
            for (int i = 0; i < lat.Length; i++)
            {
                lat[i] = (float)rng.NextDouble();
            }
        }

        // Smoothstep interpolation weight (C1 continuous).
        private static float Fade(float t)
        {
            return t * t * (3f - 2f * t);
        }

        // Bilinear value-noise sample with wrap-around at the lattice border.
        public float Sample(float x, float y)
        {
            int ix = (int)Math.Floor(x);
            int iy = (int)Math.Floor(y);
            float fx = x - ix;
            float fy = y - iy;

            int x0 = ix % px;
            if (x0 < 0) x0 += px;
            int y0 = iy % py;
            if (y0 < 0) y0 += py;
            int x1 = x0 + 1;
            if (x1 >= px) x1 = 0;
            int y1 = y0 + 1;
            if (y1 >= py) y1 = 0;

            float c00 = lat[y0 * px + x0];
            float c10 = lat[y0 * px + x1];
            float c01 = lat[y1 * px + x0];
            float c11 = lat[y1 * px + x1];

            float u = Fade(fx);
            float v = Fade(fy);
            float a = c00 + (c10 - c00) * u;
            float b = c01 + (c11 - c01) * u;
            return a + (b - a) * v;
        }
    }

    // Fractal Brownian motion built from noise octaves. Periods double per
    // octave and are integer factors of the image size, so the sum is tileable.
    public sealed class Fbm
    {
        private readonly NoiseLayer[] layers;
        private readonly int[] periodX;
        private readonly int[] periodY;
        private readonly float normalizer;

        // Builds "octaves" layers starting at baseX x baseY.
        public Fbm(int seed, int baseX, int baseY, int octaves)
        {
            layers = new NoiseLayer[octaves];
            periodX = new int[octaves];
            periodY = new int[octaves];
            float amp = 1f;
            float total = 0f;
            for (int i = 0; i < octaves; i++)
            {
                periodX[i] = baseX << i;
                periodY[i] = baseY << i;
                layers[i] = new NoiseLayer(seed + i * 7919, periodX[i], periodY[i]);
                total += amp;
                amp *= 0.5f;
            }
            normalizer = 1f / total;
        }

        // Samples the octave stack at normalised UV coordinates.
        public float Sample(float u, float v)
        {
            float sum = 0f;
            float amp = 1f;
            for (int i = 0; i < layers.Length; i++)
            {
                sum += layers[i].Sample(u * periodX[i], v * periodY[i]) * amp;
                amp *= 0.5f;
            }
            return sum * normalizer;
        }
    }

    // All texture generators and shared helpers.
    public static class TexGen
    {
        public const int MasterSeed = 1337;

        // Clamps a float to [0,1].
        private static float Clamp01(float v)
        {
            if (v < 0f) return 0f;
            if (v > 1f) return 1f;
            return v;
        }

        // Clamps a float to an arbitrary range.
        private static float Clamp(float v, float lo, float hi)
        {
            if (v < lo) return lo;
            if (v > hi) return hi;
            return v;
        }

        // Linear interpolation.
        private static float Lerp(float a, float b, float t)
        {
            return a + (b - a) * t;
        }

        // Hermite smoothstep between two edges.
        private static float SmoothStep(float e0, float e1, float x)
        {
            float t = Clamp01((x - e0) / (e1 - e0));
            return t * t * (3f - 2f * t);
        }

        // Rounds to an 8-bit channel value with saturation.
        private static byte B(float v)
        {
            if (v <= 0f) return 0;
            if (v >= 255f) return 255;
            return (byte)(v + 0.5f);
        }

        // Writes one RGB pixel into a BGR byte buffer (LockBits layout).
        private static void Put3(byte[] buf, int o, float r, float g, float b)
        {
            buf[o] = B(b);
            buf[o + 1] = B(g);
            buf[o + 2] = B(r);
        }

        // Writes one RGBA pixel into a BGRA byte buffer (LockBits layout).
        private static void Put4(byte[] buf, int o, float r, float g, float b, float a)
        {
            buf[o] = B(b);
            buf[o + 1] = B(g);
            buf[o + 2] = B(r);
            buf[o + 3] = B(a);
        }

        // Saves a packed buffer as an 8-bit PNG (3 = RGB, 4 = RGBA) via LockBits.
        public static void SavePng(string path, int w, int h, byte[] data, int channels)
        {
            PixelFormat pf = channels == 4 ? PixelFormat.Format32bppArgb : PixelFormat.Format24bppRgb;
            int stride = ((w * channels + 3) / 4) * 4;

            using (Bitmap bmp = new Bitmap(w, h, pf))
            {
                BitmapData bd = bmp.LockBits(new Rectangle(0, 0, w, h), ImageLockMode.WriteOnly, pf);
                try
                {
                    if (stride == w * channels)
                    {
                        Marshal.Copy(data, 0, bd.Scan0, w * h * channels);
                    }
                    else
                    {
                        byte[] padded = new byte[stride * h];
                        for (int y = 0; y < h; y++)
                        {
                            Buffer.BlockCopy(data, y * w * channels, padded, y * stride, w * channels);
                        }
                        Marshal.Copy(padded, 0, bd.Scan0, padded.Length);
                    }
                }
                finally
                {
                    bmp.UnlockBits(bd);
                }
                bmp.Save(path, ImageFormat.Png);
            }
        }

        // Builds a tangent-space normal map from a height field using a wrapped
        // 3x3 Sobel gradient. Z is kept in [minZ,1] and the vector is renormalised.
        public static void BuildNormalMap(string path, int w, int h, float[] height, float strength, float minZ)
        {
            byte[] data = new byte[w * h * 3];
            for (int y = 0; y < h; y++)
            {
                int ym = y == 0 ? h - 1 : y - 1;
                int yp = y == h - 1 ? 0 : y + 1;
                for (int x = 0; x < w; x++)
                {
                    int xm = x == 0 ? w - 1 : x - 1;
                    int xp = x == w - 1 ? 0 : x + 1;

                    float h00 = height[ym * w + xm];
                    float h01 = height[ym * w + x];
                    float h02 = height[ym * w + xp];
                    float h10 = height[y * w + xm];
                    float h12 = height[y * w + xp];
                    float h20 = height[yp * w + xm];
                    float h21 = height[yp * w + x];
                    float h22 = height[yp * w + xp];

                    float gx = ((h02 + 2f * h12 + h22) - (h00 + 2f * h10 + h20)) * 0.125f * strength;
                    float gy = ((h20 + 2f * h21 + h22) - (h00 + 2f * h01 + h02)) * 0.125f * strength;

                    float g2 = gx * gx + gy * gy;
                    float nz = 1f / (float)Math.Sqrt(1f + g2);
                    if (nz < minZ) nz = minZ;
                    float horiz = (float)Math.Sqrt(Math.Max(0f, 1f - nz * nz));
                    float glen = (float)Math.Sqrt(g2);
                    float nx = 0f;
                    float ny = 0f;
                    if (glen > 1e-8f)
                    {
                        nx = -(gx / glen) * horiz;
                        ny = -(gy / glen) * horiz;
                    }

                    int o = (y * w + x) * 3;
                    Put3(data, o,
                         (nx * 0.5f + 0.5f) * 255f,
                         (ny * 0.5f + 0.5f) * 255f,
                         (nz * 0.5f + 0.5f) * 255f);
                }
            }
            SavePng(path, w, h, data, 3);
        }

        // Stamps one soft round speck into the clay albedo (BGR) and height.
        private static void StampSpeck(byte[] albedo, float[] height, int w, int h,
                                       float cx, float cy, float radius, float strength, bool lighter)
        {
            int x0 = (int)Math.Floor(cx - radius - 1f);
            int x1 = (int)Math.Ceiling(cx + radius + 1f);
            int y0 = (int)Math.Floor(cy - radius - 1f);
            int y1 = (int)Math.Ceiling(cy + radius + 1f);

            for (int y = y0; y <= y1; y++)
            {
                int yy = y % h;
                if (yy < 0) yy += h;
                for (int x = x0; x <= x1; x++)
                {
                    float dx = x - cx;
                    float dy = y - cy;
                    float d = (float)Math.Sqrt(dx * dx + dy * dy);
                    if (d > radius) continue;

                    float f = 1f - d / radius;
                    f = f * f * (3f - 2f * f) * strength;
                    int xx = x % w;
                    if (xx < 0) xx += w;
                    int i = yy * w + xx;
                    int o = i * 3;

                    if (lighter)
                    {
                        albedo[o] = B(albedo[o] * (1f + 0.20f * f));
                        albedo[o + 1] = B(albedo[o + 1] * (1f + 0.16f * f));
                        albedo[o + 2] = B(albedo[o + 2] * (1f + 0.10f * f));
                        height[i] += 0.10f * f;
                    }
                    else
                    {
                        albedo[o] = B(albedo[o] * (1f - 0.36f * f));
                        albedo[o + 1] = B(albedo[o + 1] * (1f - 0.34f * f));
                        albedo[o + 2] = B(albedo[o + 2] * (1f - 0.32f * f));
                        height[i] -= 0.14f * f;
                    }
                }
            }
        }

        // T_Clay_Detail_* : crushed-brick clay grain, 2048x2048, seamless.
        public static void GenerateClay(string outDir)
        {
            int w = 2048;
            int h = 2048;

            Fbm coarse = new Fbm(1101, 16, 16, 5);      // multi-octave grain
            Fbm micro = new Fbm(1102, 512, 512, 1);     // 4 px grain detail
            Fbm sweep = new Fbm(1103, 4, 40, 3);        // broom streaks along X
            Fbm roughNoise = new Fbm(1104, 8, 8, 4);
            Fbm tintNoise = new Fbm(1105, 4, 4, 3);

            byte[] albedo = new byte[w * h * 3];
            byte[] rough = new byte[w * h * 3];
            float[] height = new float[w * h];

            for (int y = 0; y < h; y++)
            {
                float v = y / (float)h;
                for (int x = 0; x < w; x++)
                {
                    float u = x / (float)w;
                    float n = coarse.Sample(u, v);
                    float m = micro.Sample(u, v);
                    float s = sweep.Sample(u, v);
                    float rn = roughNoise.Sample(u, v);
                    float tn = tintNoise.Sample(u, v);

                    // Grain is the sum of the octave stack and the fine layer;
                    // +/-0.5 maps to the requested +/-12 % albedo variation.
                    float grain = (n - 0.5f) * 0.62f + (m - 0.5f) * 0.38f;
                    float streak = s - 0.5f;
                    float shade = 1f + grain * 0.24f + streak * 0.02f;

                    float r = 196f * shade + (tn - 0.5f) * 8f;
                    float g = 112f * shade + (tn - 0.5f) * 5f;
                    float b = 78f * shade + (tn - 0.5f) * 3f;

                    int i = y * w + x;
                    int o = i * 3;
                    Put3(albedo, o, r, g, b);

                    float roughV = 0.82f + rn * 0.13f + grain * 0.015f;
                    roughV = Clamp(roughV, 0.82f, 0.95f);
                    byte rb = B(roughV * 255f);
                    Put3(rough, o, rb, rb, rb);

                    height[i] = 0.5f + (n - 0.5f) * 0.30f + (m - 0.5f) * 0.36f + streak * 0.08f;
                }
            }

            // Sparse darker grit pits and a few tiny lighter grains.
            Random rng = new Random(MasterSeed + 21);
            int darkCount = (int)(w * h / 3800.0);
            for (int i = 0; i < darkCount; i++)
            {
                float cx = (float)(rng.NextDouble() * w);
                float cy = (float)(rng.NextDouble() * h);
                float rad = 0.7f + (float)rng.NextDouble() * 2.1f;
                float strength = 0.55f + (float)rng.NextDouble() * 0.45f;
                StampSpeck(albedo, height, w, h, cx, cy, rad, strength, false);
            }
            int lightCount = (int)(w * h / 21000.0);
            for (int i = 0; i < lightCount; i++)
            {
                float cx = (float)(rng.NextDouble() * w);
                float cy = (float)(rng.NextDouble() * h);
                float rad = 0.6f + (float)rng.NextDouble() * 1.3f;
                float strength = 0.6f + (float)rng.NextDouble() * 0.4f;
                StampSpeck(albedo, height, w, h, cx, cy, rad, strength, true);
            }

            SavePng(Path.Combine(outDir, "T_Clay_Detail_Albedo.png"), w, h, albedo, 3);
            SavePng(Path.Combine(outDir, "T_Clay_Detail_Rough.png"), w, h, rough, 3);
            BuildNormalMap(Path.Combine(outDir, "T_Clay_Detail_Normal.png"), w, h, height, 9.0f, 0.88f);
        }

        // T_LinePaint_* : worn, chalky white court paint, 1024x1024, seamless.
        public static void GenerateLinePaint(string outDir)
        {
            int w = 1024;
            int h = 1024;

            Fbm paintNoise = new Fbm(2101, 8, 8, 5);
            Fbm chalk = new Fbm(2102, 64, 64, 4);
            Fbm blotch = new Fbm(2103, 3, 3, 4);
            Fbm brush = new Fbm(2104, 2, 96, 3);        // faint horizontal brush streaks
            Fbm tint = new Fbm(2105, 6, 6, 3);

            byte[] albedo = new byte[w * h * 3];
            byte[] rough = new byte[w * h * 3];
            float[] height = new float[w * h];

            for (int y = 0; y < h; y++)
            {
                float v = y / (float)h;
                for (int x = 0; x < w; x++)
                {
                    float u = x / (float)w;
                    float n = paintNoise.Sample(u, v);
                    float c = chalk.Sample(u, v);
                    float bl = blotch.Sample(u, v);
                    float br = brush.Sample(u, v);
                    float tn = tint.Sample(u, v);

                    // Low-frequency blotches of clay dust soaked into the paint.
                    float dirt = SmoothStep(0.50f, 0.80f, bl) * 0.75f;

                    float shade = 1f + (n - 0.5f) * 0.09f + (c - 0.5f) * 0.16f + (br - 0.5f) * 0.05f;
                    float r = 232f * shade;
                    float g = 230f * shade;
                    float b = 222f * shade;

                    r = Lerp(r, 148f + (tn - 0.5f) * 18f, dirt * 0.55f);
                    g = Lerp(g, 118f + (tn - 0.5f) * 12f, dirt * 0.55f);
                    b = Lerp(b, 94f + (tn - 0.5f) * 10f, dirt * 0.55f);

                    int i = y * w + x;
                    int o = i * 3;
                    Put3(albedo, o, r, g, b);

                    float roughV = 0.55f + c * 0.22f + dirt * 0.06f;
                    roughV = Clamp(roughV, 0.55f, 0.80f);
                    byte rb = B(roughV * 255f);
                    Put3(rough, o, rb, rb, rb);

                    height[i] = 0.5f + (c - 0.5f) * 0.40f + (br - 0.5f) * 0.10f + (n - 0.5f) * 0.10f;
                }
            }

            SavePng(Path.Combine(outDir, "T_LinePaint_Albedo.png"), w, h, albedo, 3);
            SavePng(Path.Combine(outDir, "T_LinePaint_Rough.png"), w, h, rough, 3);
            BuildNormalMap(Path.Combine(outDir, "T_LinePaint_Normal.png"), w, h, height, 7.0f, 0.88f);
        }

        // T_Net_Albedo / T_Net_Normal : woven tennis net with alpha, 1024x1024.
        public static void GenerateNet(string outDir)
        {
            int w = 1024;
            int h = 1024;
            int spacing = 32;                          // 4 cm mesh (1.25 mm / px)
            float halfCord = 1.15f;                    // ~2.3 px cord (~2.9 mm)
            int bandH = (int)Math.Ceiling(h * 0.06);   // top canvas band = 6 % of height
            int halfCell = spacing / 2;

            Fbm fibre = new Fbm(3101, 64, 64, 4);
            Fbm fibreFine = new Fbm(3102, 192, 192, 2);
            Fbm canvasNoise = new Fbm(3103, 96, 96, 3);

            byte[] albedo = new byte[w * h * 4];
            float[] height = new float[w * h];

            for (int y = 0; y < h; y++)
            {
                float v = y / (float)h;
                for (int x = 0; x < w; x++)
                {
                    float u = x / (float)w;
                    int px = ((x % spacing) + spacing) % spacing;
                    int py = ((y % spacing) + spacing) % spacing;
                    float dx = Math.Abs(px - halfCell);
                    float dy = Math.Abs(py - halfCell);
                    float covX = Clamp01(halfCord + 0.5f - dx);
                    float covY = Clamp01(halfCord + 0.5f - dy);

                    float fib = fibre.Sample(u, v);
                    float fib2 = fibreFine.Sample(u, v);
                    int i = y * w + x;
                    int o = i * 4;

                    if (y < bandH)
                    {
                        // White canvas headband: fully opaque, subtle weave and stitches.
                        float n = (fib - 0.5f) * 9f;
                        float weave = (y % 3 == 0) ? -5f : 0f;
                        float stitch = 0f;
                        if (y >= bandH - 7 && y <= bandH - 4 && (x % 10) < 5) stitch = -30f;
                        float lip = (y >= bandH - 2) ? -26f : 0f;
                        float c = 236f + n + weave + stitch + lip + (canvasNoise.Sample(u, v) - 0.5f) * 8f;
                        Put4(albedo, o, c, c - 2f, c - 8f, 255f);
                        height[i] = 0.80f + (fib - 0.5f) * 0.04f - (y >= bandH - 2 ? 0.30f : 0f);
                    }
                    else
                    {
                        float cov = Math.Max(covX, covY);
                        float alpha = cov <= 0f ? 0f : cov * 255f;

                        float profX = 0f;
                        if (dx < halfCord) profX = (float)Math.Sqrt(1f - (dx / halfCord) * (dx / halfCord));
                        float profY = 0f;
                        if (dy < halfCord) profY = (float)Math.Sqrt(1f - (dy / halfCord) * (dy / halfCord));

                        // Plain weave: the visible cord alternates at every crossing.
                        int cellX = (int)Math.Floor((x - halfCell) / (double)spacing);
                        int cellY = (int)Math.Floor((y - halfCell) / (double)spacing);
                        bool verticalOnTop = ((cellX + cellY) & 1) == 0;

                        float shade = 1f + (fib - 0.5f) * 0.30f + (fib2 - 0.5f) * 0.14f;
                        bool crossing = covX > 0f && covY > 0f;
                        if (crossing)
                        {
                            if (verticalOnTop) shade *= 1.12f;
                            else shade *= 0.90f;
                        }
                        float charcoal = 41f * shade;
                        Put4(albedo, o, charcoal, charcoal + 1f, charcoal + 4f, alpha);

                        if (crossing)
                        {
                            height[i] = verticalOnTop ? 0.40f + 0.34f * profX : 0.40f + 0.34f * profY;
                        }
                        else
                        {
                            height[i] = 0.34f * Math.Max(profX, profY);
                        }
                    }
                }
            }

            SavePng(Path.Combine(outDir, "T_Net_Albedo.png"), w, h, albedo, 4);
            BuildNormalMap(Path.Combine(outDir, "T_Net_Normal.png"), w, h, height, 4.0f, 0.85f);
        }

        // Adds one soft fibre dab to the ball albedo and height field (wrapped).
        private static void StampFibre(byte[] albedo, float[] height, int w, int h,
                                       float fx, float fy, float radius,
                                       float dr, float dg, float db, float dh)
        {
            int cx = (int)Math.Floor(fx);
            int cy = (int)Math.Floor(fy);
            int rr = (int)Math.Ceiling(radius);
            for (int oy = -rr; oy <= rr; oy++)
            {
                int yy = (cy + oy) % h;
                if (yy < 0) yy += h;
                for (int ox = -rr; ox <= rr; ox++)
                {
                    float d = (float)Math.Sqrt(ox * ox + oy * oy);
                    if (d > radius) continue;
                    int xx = (cx + ox) % w;
                    if (xx < 0) xx += w;
                    float f = 1f - d / radius;
                    int i = yy * w + xx;
                    int o = i * 3;
                    albedo[o] = B(albedo[o] + db * f);
                    albedo[o + 1] = B(albedo[o + 1] + dg * f);
                    albedo[o + 2] = B(albedo[o + 2] + dr * f);
                    height[i] += dh * f;
                }
            }
        }

        // T_Ball_* : optic-yellow felt with short fibres and a sinusoidal seam.
        public static void GenerateBall(string outDir)
        {
            int w = 1024;
            int h = 1024;

            Fbm felt = new Fbm(4101, 16, 16, 5);
            Fbm fuzz = new Fbm(4102, 256, 256, 2);
            Fbm seamNoise = new Fbm(4103, 96, 96, 3);

            byte[] albedo = new byte[w * h * 3];
            byte[] rough = new byte[w * h * 3];
            float[] height = new float[w * h];

            for (int y = 0; y < h; y++)
            {
                float v = y / (float)h;
                for (int x = 0; x < w; x++)
                {
                    float u = x / (float)w;
                    float n = felt.Sample(u, v);
                    float f = fuzz.Sample(u, v);

                    float shade = 1f + (n - 0.5f) * 0.10f + (f - 0.5f) * 0.22f;
                    float r = 205f * shade + (f - 0.5f) * 10f;
                    float g = 225f * shade + (f - 0.5f) * 8f;
                    float b = 58f * shade + (f - 0.5f) * 10f;

                    int i = y * w + x;
                    int o = i * 3;
                    Put3(albedo, o, r, g, b);

                    float roughV = 0.78f + (f - 0.5f) * 0.14f + (n - 0.5f) * 0.05f;
                    roughV = Clamp(roughV, 0.76f, 0.95f);
                    byte rb = B(roughV * 255f);
                    Put3(rough, o, rb, rb, rb);

                    height[i] = 0.48f + (n - 0.5f) * 0.16f + (f - 0.5f) * 0.30f;
                }
            }

            // Scatter thousands of short curved felt fibres (6-14 px strokes).
            Random rng = new Random(MasterSeed + 41);
            int strokeCount = 9000;
            for (int s = 0; s < strokeCount; s++)
            {
                float cx = (float)(rng.NextDouble() * w);
                float cy = (float)(rng.NextDouble() * h);
                float ang = (float)(rng.NextDouble() * Math.PI * 2.0);
                float len = 6f + (float)rng.NextDouble() * 8f;
                float curve = ((float)rng.NextDouble() - 0.5f) * 0.22f;
                float radius = 0.75f + (float)rng.NextDouble() * 0.6f;
                float delta = 14f + (float)rng.NextDouble() * 16f;
                if (rng.NextDouble() < 0.5) delta = -delta * 0.85f;

                for (float t = 0f; t < len; t += 0.55f)
                {
                    StampFibre(albedo, height, w, h, cx, cy, radius,
                               delta, delta * 0.85f, delta * 0.35f, 0.05f);
                    cx += (float)Math.Cos(ang) * 0.55f;
                    cy += (float)Math.Sin(ang) * 0.55f;
                    ang += curve * 0.55f;
                }
            }

            // White seam band, ~4 px wide, sinusoidal around the middle.
            for (int x = 0; x < w; x++)
            {
                float phase = (float)(Math.PI * 2.0) * 2f * x / w + 0.6f;
                float yc = h * 0.5f + 70f * (float)Math.Sin(phase);
                int y0 = (int)Math.Floor(yc - 3f);
                int y1 = (int)Math.Ceiling(yc + 3f);
                for (int y = y0; y <= y1; y++)
                {
                    int yy = y % h;
                    if (yy < 0) yy += h;
                    float d = Math.Abs(y - yc);
                    float cov = Clamp01(2.1f - d);
                    if (cov <= 0f) continue;

                    float sn = seamNoise.Sample(x / (float)w, y / (float)h) - 0.5f;
                    int i = yy * w + x;
                    int o = i * 3;
                    albedo[o + 2] = B(Lerp(albedo[o + 2], 238f + sn * 8f, cov));
                    albedo[o + 1] = B(Lerp(albedo[o + 1], 237f + sn * 8f, cov));
                    albedo[o] = B(Lerp(albedo[o], 227f + sn * 6f, cov));

                    byte rv = B(Lerp(rough[o], 0.80f * 255f, cov));
                    rough[o] = rv;
                    rough[o + 1] = rv;
                    rough[o + 2] = rv;

                    height[i] = Lerp(height[i], 0.64f + sn * 0.03f, cov);
                }
            }

            SavePng(Path.Combine(outDir, "T_Ball_Albedo.png"), w, h, albedo, 3);
            SavePng(Path.Combine(outDir, "T_Ball_Rough.png"), w, h, rough, 3);
            BuildNormalMap(Path.Combine(outDir, "T_Ball_Normal.png"), w, h, height, 8.0f, 0.86f);
        }

        // T_RacketStrings_Albedo : RGBA string bed, 512x512.
        public static void GenerateStrings(string outDir)
        {
            int w = 512;
            int h = 512;
            int spacing = 32;       // ~7 mm spacing equivalent
            float halfStr = 2.8f;   // ~5.6 px string diameter
            int halfCell = spacing / 2;

            Fbm fibre = new Fbm(5101, 32, 32, 4);
            byte[] albedo = new byte[w * h * 4];

            for (int y = 0; y < h; y++)
            {
                float v = y / (float)h;
                for (int x = 0; x < w; x++)
                {
                    float u = x / (float)w;
                    int px = ((x % spacing) + spacing) % spacing;
                    int py = ((y % spacing) + spacing) % spacing;
                    float dx = Math.Abs(px - halfCell);
                    float dy = Math.Abs(py - halfCell);
                    float covX = Clamp01(halfStr + 0.5f - dx);
                    float covY = Clamp01(halfStr + 0.5f - dy);
                    float cov = Math.Max(covX, covY);
                    int i = y * w + x;
                    int o = i * 4;

                    if (cov <= 0f)
                    {
                        Put4(albedo, o, 30f, 30f, 34f, 0f);
                        continue;
                    }

                    float profX = 0f;
                    if (dx < halfStr) profX = (float)Math.Sqrt(1f - (dx / halfStr) * (dx / halfStr));
                    float profY = 0f;
                    if (dy < halfStr) profY = (float)Math.Sqrt(1f - (dy / halfStr) * (dy / halfStr));

                    int cellX = (int)Math.Floor((x - halfCell) / (double)spacing);
                    int cellY = (int)Math.Floor((y - halfCell) / (double)spacing);
                    bool verticalOnTop = ((cellX + cellY) & 1) == 0;

                    float prof;
                    bool lightString;
                    if (covX > 0f && covY > 0f)
                    {
                        prof = verticalOnTop ? profX : profY;
                        lightString = verticalOnTop;
                    }
                    else if (covX > 0f)
                    {
                        prof = profX;
                        lightString = true;
                    }
                    else
                    {
                        prof = profY;
                        lightString = false;
                    }

                    float fib = fibre.Sample(u, v);
                    float shade = 0.78f + 0.30f * prof + (fib - 0.5f) * 0.10f;
                    float r;
                    float g;
                    float b;
                    if (lightString)
                    {
                        r = 206f;
                        g = 203f;
                        b = 194f;
                    }
                    else
                    {
                        r = 88f;
                        g = 88f;
                        b = 94f;
                    }

                    Put4(albedo, o, r * shade, g * shade, b * shade, cov * 255f);
                }
            }

            SavePng(Path.Combine(outDir, "T_RacketStrings_Albedo.png"), w, h, albedo, 4);
        }

        // Blends one soft ellipse into the crowd colour buffers.
        private static void DrawBlob(float[] rr, float[] gg, float[] bb, int w, int h,
                                     float cx, float cy, float rx, float ry, Color c, float opacity)
        {
            int x0 = (int)Math.Floor(cx - rx);
            int x1 = (int)Math.Ceiling(cx + rx);
            int y0 = (int)Math.Floor(cy - ry);
            int y1 = (int)Math.Ceiling(cy + ry);

            for (int y = y0; y <= y1; y++)
            {
                if (y < 0 || y >= h) continue;
                float ny = (y - cy) / ry;
                for (int x = x0; x <= x1; x++)
                {
                    if (x < 0 || x >= w) continue;
                    float nx = (x - cx) / rx;
                    float d2 = nx * nx + ny * ny;
                    if (d2 >= 1f) continue;

                    float a = opacity * Clamp01((1f - d2) * 1.7f);
                    int i = y * w + x;
                    rr[i] = rr[i] * (1f - a) + c.R * a;
                    gg[i] = gg[i] * (1f - a) + c.G * a;
                    bb[i] = bb[i] * (1f - a) + c.B * a;
                }
            }
        }

        // Separable box blur (in place, clamped borders) used to defocus the crowd.
        private static void BoxBlur(float[] data, int w, int h, int radius)
        {
            float[] tmp = new float[data.Length];
            float inv = 1f / (radius * 2 + 1);

            for (int y = 0; y < h; y++)
            {
                int row = y * w;
                for (int x = 0; x < w; x++)
                {
                    float sum = 0f;
                    for (int k = -radius; k <= radius; k++)
                    {
                        int xx = x + k;
                        if (xx < 0) xx = 0;
                        if (xx >= w) xx = w - 1;
                        sum += data[row + xx];
                    }
                    tmp[row + x] = sum * inv;
                }
            }

            for (int y = 0; y < h; y++)
            {
                for (int x = 0; x < w; x++)
                {
                    float sum = 0f;
                    for (int k = -radius; k <= radius; k++)
                    {
                        int yy = y + k;
                        if (yy < 0) yy = 0;
                        if (yy >= h) yy = h - 1;
                        sum += tmp[yy * w + x];
                    }
                    data[y * w + x] = sum * inv;
                }
            }
        }

        // T_Crowd_Albedo : abstract, defocused stadium crowd, 1024x512.
        public static void GenerateCrowd(string outDir)
        {
            int w = 1024;
            int h = 512;
            float[] rr = new float[w * h];
            float[] gg = new float[w * h];
            float[] bb = new float[w * h];

            Fbm bgNoise = new Fbm(6101, 32, 32, 4);

            // Dark seating background with a little low-frequency variation.
            for (int y = 0; y < h; y++)
            {
                float v = y / (float)h;
                for (int x = 0; x < w; x++)
                {
                    float u = x / (float)w;
                    float n = bgNoise.Sample(u, v) - 0.5f;
                    int i = y * w + x;
                    rr[i] = 26f + n * 8f;
                    gg[i] = 24f + n * 8f;
                    bb[i] = 30f + n * 8f;
                }
            }

            Random rng = new Random(MasterSeed + 61);
            Color[] bodies = new Color[]
            {
                Color.FromArgb(150, 58, 52), Color.FromArgb(120, 45, 45),
                Color.FromArgb(42, 56, 86), Color.FromArgb(38, 78, 80),
                Color.FromArgb(150, 120, 55), Color.FromArgb(110, 110, 115),
                Color.FromArgb(190, 185, 175), Color.FromArgb(95, 70, 55),
                Color.FromArgb(85, 95, 60), Color.FromArgb(76, 58, 88),
                Color.FromArgb(110, 135, 155), Color.FromArgb(175, 90, 70)
            };
            Color[] skins = new Color[]
            {
                Color.FromArgb(230, 190, 160), Color.FromArgb(205, 160, 125),
                Color.FromArgb(170, 120, 90), Color.FromArgb(120, 80, 60),
                Color.FromArgb(85, 58, 45), Color.FromArgb(240, 205, 180)
            };

            // Rows of blob "spectators"; lower rows are drawn later so they occlude.
            int rows = 11;
            for (int r = 0; r < rows; r++)
            {
                float rowY = 30f + r * 42f;
                float x = -10f + (float)rng.NextDouble() * 20f;
                while (x < w + 10f)
                {
                    x += 17f + (float)rng.NextDouble() * 17f;
                    if (rng.NextDouble() < 0.12) continue;

                    float scale = 0.8f + (float)rng.NextDouble() * 0.55f;
                    float bw = (9f + (float)rng.NextDouble() * 8f) * scale;
                    float bh = (13f + (float)rng.NextDouble() * 10f) * scale;
                    float cy = rowY + 10f * scale + (float)rng.NextDouble() * 4f - 2f;
                    Color body = bodies[rng.Next(bodies.Length)];
                    Color skin = skins[rng.Next(skins.Length)];

                    DrawBlob(rr, gg, bb, w, h, x, cy, bw, bh, body, 0.92f);
                    DrawBlob(rr, gg, bb, w, h, x, cy - bh * 0.85f, bw * 0.42f, bh * 0.40f, skin, 0.95f);
                }
            }

            // Heavy defocus removes any hint of facial features.
            BoxBlur(rr, w, h, 4);
            BoxBlur(gg, w, h, 4);
            BoxBlur(bb, w, h, 4);

            byte[] albedo = new byte[w * h * 3];
            for (int y = 0; y < h; y++)
            {
                float depth = 0.55f + 0.45f * (1f - y / (float)(h - 1));
                for (int x = 0; x < w; x++)
                {
                    int i = y * w + x;
                    float n = (bgNoise.Sample(x / (float)w, y / (float)h) - 0.5f) * 5f;
                    Put3(albedo, i * 3,
                         Clamp(rr[i] * depth + n, 0f, 255f),
                         Clamp(gg[i] * depth + n, 0f, 255f),
                         Clamp(bb[i] * depth + n, 0f, 255f));
                }
            }

            SavePng(Path.Combine(outDir, "T_Crowd_Albedo.png"), w, h, albedo, 3);
        }

        // Chooses the first installed bold-serif family from a priority list.
        private static FontFamily PickSerifFamily()
        {
            string[] wanted = new string[] { "Georgia", "Times New Roman", "Cambria", "Constantia", "DejaVu Serif" };
            FontFamily[] installed = FontFamily.Families;
            for (int i = 0; i < wanted.Length; i++)
            {
                for (int j = 0; j < installed.Length; j++)
                {
                    if (string.Equals(installed[j].Name, wanted[i], StringComparison.OrdinalIgnoreCase))
                    {
                        return installed[j];
                    }
                }
            }
            return FontFamily.GenericSerif;
        }

        // T_Court_Logo_Albedo : transparent RGBA tournament badge, 1024x1024.
        public static void GenerateLogo(string outDir)
        {
            int size = 1024;
            using (Bitmap bmp = new Bitmap(size, size, PixelFormat.Format32bppArgb))
            {
                using (Graphics g = Graphics.FromImage(bmp))
                {
                    g.Clear(Color.Transparent);
                    g.SmoothingMode = SmoothingMode.AntiAlias;
                    g.TextRenderingHint = TextRenderingHint.AntiAliasGridFit;
                    g.InterpolationMode = InterpolationMode.HighQualityBicubic;
                    g.PixelOffsetMode = PixelOffsetMode.HighQuality;
                    g.CompositingQuality = CompositingQuality.HighQuality;

                    // Clay-orange disc.
                    using (SolidBrush clay = new SolidBrush(Color.FromArgb(255, 196, 112, 78)))
                    {
                        g.FillEllipse(clay, new RectangleF(30f, 30f, 964f, 964f));
                    }

                    // Thin white ring.
                    using (Pen ring = new Pen(Color.FromArgb(255, 250, 248, 242), 7f))
                    {
                        g.DrawEllipse(ring, new RectangleF(100f, 100f, 824f, 824f));
                    }

                    // "CLAY OPEN" in bold serif, kept near 120 px but fitted to the ring.
                    FontFamily family = PickSerifFamily();
                    string text = "CLAY OPEN";
                    float fontSize = 120f;
                    using (Font probe = new Font(family, fontSize, FontStyle.Bold, GraphicsUnit.Pixel))
                    {
                        SizeF measured = g.MeasureString(text, probe);
                        if (measured.Width > 680f)
                        {
                            fontSize = fontSize * 680f / measured.Width;
                        }
                    }

                    using (Font font = new Font(family, fontSize, FontStyle.Bold, GraphicsUnit.Pixel))
                    using (SolidBrush white = new SolidBrush(Color.FromArgb(255, 250, 248, 242)))
                    using (StringFormat sf = new StringFormat())
                    {
                        sf.Alignment = StringAlignment.Center;
                        sf.LineAlignment = StringAlignment.Center;
                        sf.FormatFlags = StringFormatFlags.NoWrap;
                        g.DrawString(text, font, white, new RectangleF(0f, 0f, size, size), sf);
                    }
                }

                bmp.Save(Path.Combine(outDir, "T_Court_Logo_Albedo.png"), ImageFormat.Png);
            }
        }

        // Generates the complete texture set into outDir.
        public static void GenerateAll(string outDir)
        {
            GenerateClay(outDir);
            GenerateLinePaint(outDir);
            GenerateNet(outDir);
            GenerateBall(outDir);
            GenerateStrings(outDir);
            GenerateCrowd(outDir);
            GenerateLogo(outDir);
        }
    }
}
'@

if (-not ('ClayCourt.TexGen' -as [type])) {
    Add-Type -TypeDefinition $csharpSource -ReferencedAssemblies 'System.Drawing.dll' -Language CSharp
}

# =============================================================================
# Generation
# =============================================================================
$genTimer = [System.Diagnostics.Stopwatch]::StartNew()
Write-Host ''
Write-Host 'Generating procedural textures...' -ForegroundColor Cyan
[ClayCourt.TexGen]::GenerateAll($textureDir)
$genTimer.Stop()
Write-Host ('  generation finished in {0:N2} s' -f $genTimer.Elapsed.TotalSeconds) -ForegroundColor DarkGray

# =============================================================================
# Contact sheet preview (4 x 2 grid, 256 px thumbnails with labels)
# =============================================================================
function New-ContactSheet {
    param(
        [Parameter(Mandatory = $true)][string]$OutPath,
        [Parameter(Mandatory = $true)][string]$BaseDir,
        [Parameter(Mandatory = $true)][string[]]$Items
    )

    $cell = 256
    $labelH = 22
    $pad = 10
    $cols = 4
    $rows = 2
    $sheetW = $cols * $cell + ($cols + 1) * $pad
    $sheetH = $rows * ($cell + $labelH) + ($rows + 1) * $pad

    $bmp = New-Object System.Drawing.Bitmap($sheetW, $sheetH, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    try {
        $g.Clear([System.Drawing.Color]::FromArgb(255, 24, 24, 28))
        $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias

        $font = New-Object System.Drawing.Font('Segoe UI', 8.5)
        $textBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 228, 228, 232))
        $checkerA = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 58, 58, 63))
        $checkerB = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 76, 76, 82))
        $borderPen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(255, 110, 110, 118), 1.0)
        $sf = New-Object System.Drawing.StringFormat
        $sf.Alignment = [System.Drawing.StringAlignment]::Center
        $sf.LineAlignment = [System.Drawing.StringAlignment]::Center

        for ($i = 0; $i -lt $Items.Count; $i++) {
            $col = $i % $cols
            $row = [int][Math]::Floor($i / $cols)
            $bx = $pad + $col * ($cell + $pad)
            $by = $pad + $row * ($cell + $labelH + $pad)

            # Checkerboard so transparent textures are visible.
            for ($cy = 0; $cy -lt $cell; $cy += 16) {
                for ($cx = 0; $cx -lt $cell; $cx += 16) {
                    $brush = if ((([int]($cx / 16) + [int]($cy / 16)) % 2) -eq 0) { $checkerA } else { $checkerB }
                    $g.FillRectangle($brush, $bx + $cx, $by + $cy, 16, 16)
                }
            }

            $filePath = Join-Path $BaseDir $Items[$i]
            if (Test-Path -LiteralPath $filePath) {
                $img = [System.Drawing.Image]::FromFile($filePath)
                try {
                    $scale = [Math]::Min($cell / [double]$img.Width, $cell / [double]$img.Height)
                    $dw = [int][Math]::Round($img.Width * $scale)
                    $dh = [int][Math]::Round($img.Height * $scale)
                    $dx = $bx + [int][Math]::Floor(($cell - $dw) / 2)
                    $dy = $by + [int][Math]::Floor(($cell - $dh) / 2)
                    $g.DrawImage($img, $dx, $dy, $dw, $dh)
                }
                finally {
                    $img.Dispose()
                }
            }

            $g.DrawRectangle($borderPen, $bx, $by, $cell - 1, $cell - 1)
            $labelRect = New-Object System.Drawing.RectangleF($bx, ($by + $cell + 1), $cell, ($labelH - 2))
            $g.DrawString($Items[$i], $font, $textBrush, $labelRect, $sf)
        }
    }
    finally {
        $g.Dispose()
        $bmp.Save($OutPath, [System.Drawing.Imaging.ImageFormat]::Png)
        $bmp.Dispose()
        if ($null -ne $font) { $font.Dispose() }
        if ($null -ne $textBrush) { $textBrush.Dispose() }
        if ($null -ne $checkerA) { $checkerA.Dispose() }
        if ($null -ne $checkerB) { $checkerB.Dispose() }
        if ($null -ne $borderPen) { $borderPen.Dispose() }
        if ($null -ne $sf) { $sf.Dispose() }
    }
}

$previewName = 'preview_contact_sheet.png'
$previewItems = @(
    'T_Clay_Detail_Albedo.png',
    'T_Clay_Detail_Normal.png',
    'T_LinePaint_Albedo.png',
    'T_Net_Albedo.png',
    'T_Ball_Albedo.png',
    'T_RacketStrings_Albedo.png',
    'T_Crowd_Albedo.png',
    'T_Court_Logo_Albedo.png'
)
$previewPath = Join-Path $textureDir $previewName
New-ContactSheet -OutPath $previewPath -BaseDir $textureDir -Items $previewItems
Write-Host ('  contact sheet written: {0}' -f $previewName) -ForegroundColor DarkGray

# =============================================================================
# Verification helpers
# =============================================================================
function Get-ImageStats {
    param([Parameter(Mandatory = $true)][string]$Path)

    $bmp = [System.Drawing.Bitmap]::FromFile($Path)
    try {
        $stepX = [Math]::Max(1, [int][Math]::Floor($bmp.Width / 48.0))
        $stepY = [Math]::Max(1, [int][Math]::Floor($bmp.Height / 48.0))
        $sum = 0.0
        $alphaSum = 0.0
        $count = 0
        $visibleSum = 0.0
        $visibleCount = 0

        for ($y = 0; $y -lt $bmp.Height; $y += $stepY) {
            for ($x = 0; $x -lt $bmp.Width; $x += $stepX) {
                $c = $bmp.GetPixel($x, $y)
                $lum = ($c.R + $c.G + $c.B) / 3.0
                $sum += $lum
                $alphaSum += $c.A
                $count++
                if ($c.A -gt 0) {
                    $visibleSum += $lum
                    $visibleCount++
                }
            }
        }

        $meanRgb = [Math]::Round($sum / $count, 2)
        $meanAlpha = [Math]::Round($alphaSum / $count, 2)
        $coverage = [Math]::Round($visibleCount / [double]$count, 4)
        if ($visibleCount -gt 0) {
            $visibleMean = [Math]::Round($visibleSum / $visibleCount, 2)
        }
        else {
            $visibleMean = 0.0
        }

        [PSCustomObject]@{
            Width          = $bmp.Width
            Height         = $bmp.Height
            PixelFormat    = [string]$bmp.PixelFormat
            HasAlpha       = [System.Drawing.Image]::IsAlphaPixelFormat($bmp.PixelFormat)
            MeanRgb        = $meanRgb
            MeanAlpha      = $meanAlpha
            AlphaCoverage  = $coverage
            VisibleMeanRgb = $visibleMean
            Samples        = $count
        }
    }
    finally {
        $bmp.Dispose()
    }
}

function Get-SeamRatio {
    param([Parameter(Mandatory = $true)][string]$Path)

    $bmp = [System.Drawing.Bitmap]::FromFile($Path)
    try {
        $w = $bmp.Width
        $h = $bmp.Height
        $step = [Math]::Max(1, [int]($h / 128))
        $edge = 0.0
        $inner = 0.0
        $n = 0
        $midX = [int]($w / 2)

        for ($y = 0; $y -lt $h; $y += $step) {
            $a = $bmp.GetPixel(0, $y)
            $b = $bmp.GetPixel($w - 1, $y)
            $edge += ([Math]::Abs($a.R - $b.R) + [Math]::Abs($a.G - $b.G) + [Math]::Abs($a.B - $b.B)) / 3.0

            $c = $bmp.GetPixel($midX, $y)
            $d = $bmp.GetPixel($midX + 1, $y)
            $inner += ([Math]::Abs($c.R - $d.R) + [Math]::Abs($c.G - $d.G) + [Math]::Abs($c.B - $d.B)) / 3.0
            $n++
        }

        if ($inner -le 0.0001) {
            return 1.0
        }
        return [Math]::Round(($edge / $n) / ($inner / $n), 3)
    }
    finally {
        $bmp.Dispose()
    }
}

# =============================================================================
# Verification
# =============================================================================
$textureSpecs = @(
    @{ File = 'T_Clay_Detail_Albedo.png';   W = 2048; H = 2048; Alpha = $false; Seamless = $true;  MinCov = 0.0; MaxCov = 1.0 },
    @{ File = 'T_Clay_Detail_Normal.png';   W = 2048; H = 2048; Alpha = $false; Seamless = $false; MinCov = 0.0; MaxCov = 1.0 },
    @{ File = 'T_Clay_Detail_Rough.png';    W = 2048; H = 2048; Alpha = $false; Seamless = $false; MinCov = 0.0; MaxCov = 1.0 },
    @{ File = 'T_LinePaint_Albedo.png';     W = 1024; H = 1024; Alpha = $false; Seamless = $true;  MinCov = 0.0; MaxCov = 1.0 },
    @{ File = 'T_LinePaint_Normal.png';     W = 1024; H = 1024; Alpha = $false; Seamless = $false; MinCov = 0.0; MaxCov = 1.0 },
    @{ File = 'T_LinePaint_Rough.png';      W = 1024; H = 1024; Alpha = $false; Seamless = $false; MinCov = 0.0; MaxCov = 1.0 },
    @{ File = 'T_Net_Albedo.png';           W = 1024; H = 1024; Alpha = $true;  Seamless = $true;  MinCov = 0.10; MaxCov = 0.60 },
    @{ File = 'T_Net_Normal.png';           W = 1024; H = 1024; Alpha = $false; Seamless = $false; MinCov = 0.0; MaxCov = 1.0 },
    @{ File = 'T_Ball_Albedo.png';          W = 1024; H = 1024; Alpha = $false; Seamless = $false; MinCov = 0.0; MaxCov = 1.0 },
    @{ File = 'T_Ball_Normal.png';          W = 1024; H = 1024; Alpha = $false; Seamless = $false; MinCov = 0.0; MaxCov = 1.0 },
    @{ File = 'T_Ball_Rough.png';           W = 1024; H = 1024; Alpha = $false; Seamless = $false; MinCov = 0.0; MaxCov = 1.0 },
    @{ File = 'T_RacketStrings_Albedo.png'; W = 512;  H = 512;  Alpha = $true;  Seamless = $true;  MinCov = 0.10; MaxCov = 0.80 },
    @{ File = 'T_Crowd_Albedo.png';         W = 1024; H = 512;  Alpha = $false; Seamless = $false; MinCov = 0.0; MaxCov = 1.0 },
    @{ File = 'T_Court_Logo_Albedo.png';    W = 1024; H = 1024; Alpha = $true;  Seamless = $false; MinCov = 0.30; MaxCov = 0.95 }
)

$failures = New-Object System.Collections.Generic.List[string]
$report = New-Object System.Collections.Generic.List[object]
$statsByFile = @{}

Write-Host ''
Write-Host 'Verifying outputs...' -ForegroundColor Cyan

foreach ($spec in $textureSpecs) {
    $path = Join-Path $textureDir $spec.File
    $status = 'PASS'
    $seamNote = ''

    if (-not (Test-Path -LiteralPath $path)) {
        $failures.Add(("{0}: file is missing" -f $spec.File))
        $report.Add([PSCustomObject]@{
            File = $spec.File; Size = '-'; Dimensions = '-'; Format = '-'
            Alpha = '-'; MeanRGB = '-'; MeanA = '-'; Coverage = '-'; Seam = '-'; Status = 'FAIL'
        })
        continue
    }

    $stats = Get-ImageStats -Path $path
    $statsByFile[$spec.File] = $stats

    if ($stats.Width -ne $spec.W -or $stats.Height -ne $spec.H) {
        $failures.Add(("{0}: expected {1}x{2}, got {3}x{4}" -f $spec.File, $spec.W, $spec.H, $stats.Width, $stats.Height))
        $status = 'FAIL'
    }

    if ($stats.HasAlpha -ne $spec.Alpha) {
        $failures.Add(("{0}: alpha expected={1}, actual={2} ({3})" -f $spec.File, $spec.Alpha, $stats.HasAlpha, $stats.PixelFormat))
        $status = 'FAIL'
    }

    if ($stats.MeanRgb -lt 2.0) {
        $failures.Add(("{0}: mean RGB {1} looks black" -f $spec.File, $stats.MeanRgb))
        $status = 'FAIL'
    }

    if ($spec.Alpha) {
        if ($stats.MeanAlpha -lt 5.0) {
            $failures.Add(("{0}: mean alpha {1} looks fully transparent" -f $spec.File, $stats.MeanAlpha))
            $status = 'FAIL'
        }
        if ($stats.AlphaCoverage -lt $spec.MinCov -or $stats.AlphaCoverage -gt $spec.MaxCov) {
            $failures.Add(("{0}: alpha coverage {1} outside [{2},{3}]" -f $spec.File, $stats.AlphaCoverage, $spec.MinCov, $spec.MaxCov))
            $status = 'FAIL'
        }
    }

    if ($spec.Seamless) {
        $ratio = Get-SeamRatio -Path $path
        $seamNote = ('{0:N2}' -f $ratio)
        if ($ratio -gt 2.0) {
            $failures.Add(("{0}: seam ratio {1} suggests a visible tile seam" -f $spec.File, $ratio))
            $status = 'FAIL'
        }
    }

    $item = Get-Item -LiteralPath $path
    $report.Add([PSCustomObject]@{
        File       = $spec.File
        Size       = ('{0:N0} KB' -f ($item.Length / 1KB))
        Dimensions = ('{0}x{1}' -f $stats.Width, $stats.Height)
        Format     = $stats.PixelFormat.ToString().Replace('Format', '')
        Alpha      = $stats.HasAlpha
        MeanRGB    = $stats.MeanRgb
        MeanA      = $stats.MeanAlpha
        Coverage   = $stats.AlphaCoverage
        Seam       = $seamNote
        Status     = $status
    })
}

$previewPath = Join-Path $textureDir $previewName
if (-not (Test-Path -LiteralPath $previewPath)) {
    $failures.Add(('{0}: preview contact sheet is missing' -f $previewName))
}
else {
    $previewStats = Get-ImageStats -Path $previewPath
    if ($previewStats.Width -ne 1074 -or $previewStats.Height -ne 586) {
        $failures.Add(('{0}: expected 1074x586, got {1}x{2}' -f $previewName, $previewStats.Width, $previewStats.Height))
    }
    if ($previewStats.MeanRgb -lt 2.0) {
        $failures.Add(('{0}: preview looks black' -f $previewName))
    }
}

$report | Format-Table -AutoSize | Out-String -Width 200 | Write-Host

if ($failures.Count -gt 0) {
    Write-Host 'VERIFICATION FAILED:' -ForegroundColor Red
    foreach ($f in $failures) {
        Write-Host ('  - ' + $f) -ForegroundColor Red
    }
    exit 1
}

Write-Host 'All verification checks passed.' -ForegroundColor Green

# =============================================================================
# Manifest (file, width, height, hasAlpha, sha256)
# =============================================================================
$manifest = New-Object System.Collections.Generic.List[object]
$manifestSpecs = New-Object System.Collections.Generic.List[object]
foreach ($spec in $textureSpecs) { $manifestSpecs.Add($spec) }
$manifestSpecs.Add(@{ File = $previewName; W = 1074; H = 586; Alpha = $false; Seamless = $false; MinCov = 0.0; MaxCov = 1.0 })

foreach ($spec in $manifestSpecs) {
    $path = Join-Path $textureDir $spec.File
    $bmp = [System.Drawing.Bitmap]::FromFile($path)
    try {
        $imgW = $bmp.Width
        $imgH = $bmp.Height
        $imgAlpha = [System.Drawing.Image]::IsAlphaPixelFormat($bmp.PixelFormat)
    }
    finally {
        $bmp.Dispose()
    }
    $hash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
    $manifest.Add([PSCustomObject]@{
        file     = $spec.File
        width    = $imgW
        height   = $imgH
        hasAlpha = $imgAlpha
        sha256   = $hash
    })
}

$manifestPath = Join-Path $textureDir 'manifest.json'
$json = $manifest | ConvertTo-Json -Depth 3
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($manifestPath, $json + [Environment]::NewLine, $utf8NoBom)

# =============================================================================
# Summary
# =============================================================================
$totalBytes = 0
foreach ($spec in $manifestSpecs) {
    $totalBytes += (Get-Item -LiteralPath (Join-Path $textureDir $spec.File)).Length
}

$scriptTimer.Stop()
$elapsed = $scriptTimer.Elapsed

Write-Host ''
Write-Host 'Generated files:' -ForegroundColor Cyan
$fileList = Get-ChildItem -LiteralPath $textureDir -File | Sort-Object Name
foreach ($f in $fileList) {
    Write-Host ('  {0,-32} {1,10:N0} bytes' -f $f.Name, $f.Length)
}
Write-Host ''
Write-Host ('Output folder : {0}' -f $textureDir)
Write-Host ('Manifest      : {0} ({1} entries)' -f $manifestPath, $manifest.Count)
Write-Host ('Preview       : {0}' -f $previewPath)
Write-Host ('Total size    : {0:N2} MB' -f ($totalBytes / 1MB))
Write-Host ('Runtime       : {0:N2} s' -f $elapsed.TotalSeconds) -ForegroundColor Green
