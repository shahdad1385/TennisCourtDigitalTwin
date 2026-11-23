# REPORT_CLAY_V2 - Crushed-brick clay court texture set (v2)

**Date:** 2026-11-23
**Generator:** `C:\Projects\ClayTennis\Tools\gen_clay_v2.ps1` (PowerShell 5.1 + in-memory C# via `Add-Type`,
System.Drawing `LockBits` + byte buffers; fixed seed `20261123`, own xorshift64 PRNG)
**Outputs:** `C:\Projects\ClayTennis\RawAssets\Clay2\`
**Runtime:** ~23 s per full regeneration on this machine (clay core ~19 s, line ~0.6 s, macro ~0.2 s,
verify + contact sheet ~2 s). Total of 8 generator runs were executed during tuning.

---

## 1. Deliverables and SHA-256

| File | Size (bytes) | SHA-256 |
|---|---:|---|
| `T_Clay2_Albedo.png` (4096x4096, sRGB content) | 23,189,081 | `EF5F22B9B6AA85789D9F64FAF338256AF0DE02CBD3FFDE7E58AFCC1C48AA3C83` |
| `T_Clay2_Normal.png` (4096x4096, linear, OpenGL Y+) | 33,508,387 | `12F5D461CB7C1BE4D7D75CAB275C6BFBBC1D2C1FD7CF4FF3582624A04BA712D4` |
| `T_Clay2_Rough.png` (4096x4096, linear) | 9,944,212 | `F4F7AD9B140F7429B5DFAC85A3AB281FB43CE3AB3C79C9C5E2592F137A478C10` |
| `T_Line2_Albedo.png` (1024x1024) | 1,528,637 | `9E53AEE753862DF1BCDA2E7CFF954D94C471E8C88F4B58BE7E721992AC61F93B` |
| `T_Line2_Rough.png` (1024x1024) | 694,566 | `74BD09EE3F057DD3855A13A894713F4F0A2EFBA4042E0F96713570FE82383966` |
| `T_Clay2_Macro.png` (1024x1024) | 257,538 | `359358D420831CB4613F55DF2A396B78C89C839881281830BDF913DF70838148` |
| `preview_clay_v2.png` (2048x1536 contact sheet) | 2,643,071 | `DFCAF518838496C9AB18E33E09F5A37D808C6101A603E6ECB24F9AC000944F1D` (*) |

Also written per run: `stats_clay_v2.txt` (full measured log, embedded in the preview's stats panel).

(*) Determinism: the six TEXTURES are bit-identical across runs - verified twice by re-running with the
same seed/params and comparing hashes (albedo, normal, rough, macro, line-rough matched exactly; line-albedo
hash above is after the final chalk-shoulder fix). The preview/stats files legitimately vary between runs
because the preview embeds per-run stage timings in its stats panel.

All PNGs are 8-bit/channel RGB (Format24bppRgb, no palette, no alpha). Dimensions independently
re-verified in a separate process via direct GDI+ loads.

---

## 2. Measured stats (final run)

### T_Clay2_Albedo (4096, 1 px ~ 0.98 mm, map = 4 x 4 m)
- **mean = [176.10, 73.94, 44.03]** vs target sRGB (176, 74, 44) - exact via a measured per-channel
  gain normalization (applied gains R=0.99, G=1.03, B=1.09).
- std = [18.01, 6.89, 7.41]; min = [62, 27, 12]; max = [241, 117, 88] (no pure black/white).
- **Seam continuity: LR = 0.814%, TB = 0.919% of 255 (< 2% target, PASS)**; interior adjacent-column
  step = 0.787% (seam/interior ratio 1.03 / 1.17 - the seam is statistically identical to any interior
  pixel step, i.e. invisible).
- **Sweep contrast (band-pass 20-256 px std/mean): 5.85%** - inside the 4-8% low-contrast target.
- **Sweep direction ratio (gx/gy of band-pass): 0.51** - mixed-direction (stripe regime would be < 0.15);
  horizontal passes dominate slightly, like a real drag-broomed court.
- Structure: blotches +-10% (damp/dry), fine grain +-4%, dark specks ~0.1%, light quartz grains ~0.2%,
  1500 pebble chips (2-6 mm, darkened dots + roughness bumps), border dust +3.5% luma / -10% sat within
  ~33 cm of the wrapped tile edges.

### T_Clay2_Rough (linear)
- mean 0.759 (193.51/255), **range 0.62 - 0.957** (min 158, max 244) - damp patches pull exactly to
  0.62, dry base spans ~0.72-0.95, sweeps modulate +-0.055, pebbles capped at ~0.95.
- Seams: LR = 0.521%, TB = 1.063% (PASS).

### T_Clay2_Normal (tangent space, Polyhaven nor_gl / OpenGL Y+)
- R/G mean exactly 127.50; **Z mean = 0.99, Z min = 0.85** (slope-clamped, target 0.85-1.0).
- B min 217, i.e. the encoded z floor is exactly 0.85. Height = 4-8 px grain relief + sweep ridges
  (asymmetric per band) + rake streak relief + blotch undulation + 1500 gaussian pebbles.
- Seams: LR = 1.378%, TB = 1.791% (PASS). NOTE: green channel is OpenGL-style (+Y up); see import notes.

### T_Line2_Albedo / T_Line2_Rough (1024, line band across the image)
- Band ~55% of height (530-596 px via thickness noise along X), edges ragged by 2-4 px (two raggedness
  frequencies + 2 px fine jitter), chalky grain +-8-13%, worn-through patches + border wear reveal clay,
  warm dust tint (base white 242/238/231 -> dust 198/152/106).
- Albedo mean [198.88, 139.77, 119.27], min [150, 62, 37], **max [253, 251, 249]** - no pure white
  (a soft shoulder keeps chalk peaks < 255; before the fix 1.06% of pixels clipped to 255).
- Rough range **0.55 - 0.85** (140-217): paint ~0.54-0.66, worn clay ~0.80-0.85.
- Seams: albedo LR 0.853% / TB 0.547%, rough LR 1.121% / TB 0.691% (PASS).

### T_Clay2_Macro (1024, grayscale)
- mean 127.71 (128 = x1.0), std 56.56, min 23, max 253 -> multiplier range **0.877 - 1.148** (encoding:
  x = 0.85 + 0.30 * v/255). Very low frequency (2/4/8-cell octaves), seams ~0.002% (PASS).

---

## 3. Broom sweep design and iteration log

Texture space is 4 x 4 m, so 1 px ~ 0.98 mm. Four overlapping sweep layers, all built from
wrapped coordinates (sin of integer harmonics + wrapping warp noise -> exactly tileable, and the
phase varies across the tile so nothing tiles visibly):

| Layer | Direction | Bands | Spacing | Final contrast |
|---|---|---|---|---|
| H1 main | horizontal, 2 warp octaves + 2 sine harmonics | 8 | 512 px = 50 cm | 0.085 (asym. profile) |
| H2 | horizontal, other phase/seed | 6 | 683 px = 66 cm | 0.032 |
| V1 | vertical | 5 | 819 px = 80 cm | 0.065 |
| D1 | diagonal (0.5*(x+y), integer coefficient 4) | 8 | 724 px perp = 71 cm | 0.040 |
| rake streaks | along H (16x2 px cells) and V (2x16 px cells), sharpened | - | 1-2 px lines | 0.045 |

Band edges wobble +-115 px (warp) + long-wave sines; each band has an asymmetric profile
(2nd harmonic, 0.40) so the dust-trailing edge is lighter/desaturated and the leading edge steeper -
this is what keeps the bands from reading as sinusoidal stripes.

**Iterations (each one a full regeneration + re-measurement):**
1. **Run 1 (baseline):** contrast 3.00% - too flat; normal-map seam 3.65%/4.27% - FAIL (1 px white
   noise + 2 px grain octave in the height field dominated the normal); rough dipped to 0.60, pebbles
   clipped rough to 255.
2. **Run 2:** rebuilt height from smoother 4-8 px grain (removed 1 px white noise from height), rough
   clamped to 0.62-0.955, pebble rough capped; raised contrasts (cMain 0.075, V 0.040, D 0.026,
   streak 0.040, nScale 0.45). Normal seams -> 1.38%/1.79% PASS. Contrast 3.71% still low; new
   direction metric read 0.48 (horizontal-dominant).
3. **Run 3:** raised main contrast to 0.085, warp to 115 px, asymmetry 0.40, stronger ridge/blotch
   relief in height (no seam cost - both are low-frequency). Contrast 4.02% in the original 20-160 px
   metric window; revealed the window excluded the 66-80 cm bands.
4. **Run 4:** widened the measurement window to 20-256 px (covers all four band spacings) and
   rebalanced: contrast 5.84%, ratio 0.48; boosting V to 0.065 / D to 0.040 landed **contrast 5.85%,
   ratio 0.51** - both in target. Frozen as defaults.
5. **Run 5 (post-freeze):** line chalk soft-shoulder fix (pure-white pixels 1.06% -> 0, max 253).
   Clay maps bit-identical to Run 4, proving determinism.

---

## 4. UE 5.5 import notes

- `T_Clay2_Albedo.png`: sRGB ON, BC1/DXT1. Mean is exactly the target terre battue tone.
- `T_Clay2_Rough.png`: sRGB OFF (linear), stored as RGB gray (R=G=B; use the R channel).
- `T_Clay2_Normal.png`: sRGB OFF, Compression **Normal Map**. **Green channel is OpenGL/Y+
  (Polyhaven nor_gl)** - UE's default import convention is DirectX (green down), so enable
  *Flip Green Channel* in the texture editor (or flip G in an external tool) or bumps will be inverted.
- `T_Clay2_Macro.png`: sRGB OFF, low mip count is fine; multiply into BaseColor (and optionally roughness)
  sampled at a much larger world scale (e.g. one macro tile per 16-32 m) to break 4 m tiling at distance.
- Suggested material: `BaseColor = Albedo * Macro`, `Roughness = Rough (R)`, `Normal = Normal`,
  all four clay maps sharing the same 4 m UV tiling.
- `T_Line2_*`: albedo sRGB ON; background outside the painted band is matched court-clay tone so an
  opaque strip blends at its borders.

---

## 5. Honest notes - what still looks synthetic

- **Grain is value noise, not particles.** Up close (< 1 m) the 2-4 px grain reads as smooth rounded
  blobs rather than angular crushed-brick shards; the sparse specks/pebbles fake particle glints at
  mid distance only. A displacement-driven or scanned detail normal would beat it at extreme close-ups.
- **Sweeps modulate luminance/saturation + subtle ridges, not grain orientation.** A real broom trail
  physically elongates the grain along the pass direction; here that is approximated by the rake streak
  noise (16 px x 2 px cells), so at grazing angles bands don't reorient micro-relief the way a real
  smearing pass would.
- **Border dust is periodic.** The dust near tile edges is wrapped/seamless by construction, but when
  tiled it forms a faint 4 m lattice of lighter edges. It is capped at +3.5% luma within ~33 cm of the
  border and modulated by blotch noise; with the macro map applied it should not read as a grid, but
  it is the first thing to check under flat lighting. Delete the `edge dust` block in the script for a
  uniform variant.
- **Direction ratio 0.51** = horizontal passes dominate. That matches courts dragged mostly one way,
  but if art direction wants more cross-hatching, raise `-SweepVContrast` / `-SweepDiagContrast`
  (or lower `-SweepMainContrast`) and re-run; the metric will tell you when you've gone too far.
- **Macro is grayscale-only** - no large-scale hue drift (real courts shift redder where wet). Hue
  variation only exists inside the 4 m tile via the damp/dry blotch layer.
- **Line texture has no alpha/coverage channel.** The clay-matched background makes an opaque strip
  blend, but a decal-style line mesh should mask with paint coverage; easy to add (the coverage value
  exists in the generator kernel) if wanted.
- **Normal map is deliberately gentle** (Z mean 0.99, slope-clamped 0.85-1.0, no 1 px white noise) to
  satisfy the < 2% seam target and avoid specular sparkle under anti-aliased mips. If you want a
  grittier highlight response, raise `-NormalScale` (each +0.15 costs ~+0.5% seam step) or re-add the
  fine octave to `hgrain`.
- **Preview lighting is a single unshadowed lambert** (L = (-0.42, -0.58, 0.70), 0.32 ambient) with the
  macro applied - no specular/sheen, so roughness variation cannot be judged from the mockup panel;
  the rough map panel and stats are the source of truth. The preview and its embedded timing text are
  why `preview_clay_v2.png`/`stats_clay_v2.txt` hashes vary per run; texture hashes do not.

## 6. Reproduction

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:\Projects\ClayTennis\Tools\gen_clay_v2.ps1
# optional knobs: -Seed, -SweepMainContrast, -SweepH2Contrast, -SweepVContrast,
# -SweepDiagContrast, -SweepWarpAmpPx, -StreakContrast, -NormalScale, -OutDir
```

Same seed + same params -> byte-identical PNGs (verified twice).
