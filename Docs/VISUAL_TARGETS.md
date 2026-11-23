# Visual targets -- photoreal clay tennis court (UE 5.5)

Concrete, measurable spec for the clay court material and lighting. Every colour
and statistic below was measured from the 10 reference photographs in
`RawAssets/Refs/` (see `PALETTE.md`, `palette.json`, `swatch.png`,
`CREDITS.md`). Re-run `Tools/analyze_refs.ps1` to regenerate them.

Numbers in brackets are the measured mean +/- acceptable spread across the
reference set. Treat them as acceptance criteria, not decoration.

---

## 1. Quick values

| Item | Value |
|---|---|
| Albedo / Base Color | **`#C27A5E`** (linear `0.539, 0.195, 0.112`) |
| Albedo lighter variant | `#CA8062` (dry, sun-facing, +10%) |
| Albedo darker variant | `#B97459` (damp/compacted, -10%) |
| Sun color | `#FFF3E8` (day) -- late afternoon: `#FFDFC2` |
| Shadow tint | `#423B39` |
| Roughness | **0.78 - 0.92**, nominal **0.86** |
| Metallic | `0.0` |
| Line color | `#EFEAE4` warm off-white (never `#FFFFFF`) |
| Line width | **50 mm** (2 in); centre service/centre line 25 mm |
| Sun elevation | **20 deg** (accept 18-24 deg) |
| Sun azimuth | **255 deg** (west-southwest) |
| Exposure target | clay mid-tone at sRGB **125** [125.4 +/- 8] |
| Contrast target | std dev **47** [46.9 +/- 7], p10 **60**, p90 **186** |

---

## 2. Albedo / Base Color

```
Base Color : #C27A5E     <- pooled clay mass, luminance-normalised to 135
Variants   : #CA8062  dry + sunlit
             #B97459  damp / compacted / shadowed
Linear     : 0.539, 0.195, 0.112   (sRGB 0.761, 0.478, 0.369)
```

* Derived from **976,366 warm clay pixels** pooled across all 10 photos
  (bins with `R > G > B`, `R-B >= 25`, luminance 55..215), so sky, white
  lines, green windscreens and clothing are excluded from the estimate.
* The single most frequent clay colour in the set is `#B85838` -- if your
  material reads pinker than `#C27A5E`, you are too light; if browner than
  `#B97459`, you are too dark.
* Measured warmth `R-B` = **51.5** [25.8 - 85.4 across images]. If the
  render's `R-B` comes out under ~30 the clay has gone grey -- check the
  tonemapper and white balance before touching the texture.
* Add low-frequency patchiness: +/- 6% albedo over 2-4 m blobs (dry vs.
  slightly damp). Do **not** add high-frequency value noise; crushed brick
  reads as flat colour with grain, not as mottling.

## 3. Surface (roughness, normal, spec)

| Channel | Setting | Why |
|---|---|---|
| Metallic | `0.0` | mineral surface |
| Roughness | `0.78 - 0.92`, nominal `0.86` | dry crushed brick is near-Lambertian |
| Roughness - swept lanes | `0.82 - 0.88` | compaction from dragging the drag |
| Roughness - damp patches | `0.62 - 0.72` | only where visibly darker; never below 0.6 |
| Specular | `0.5` (UE default) | do not raise; clay has no sheen |
| Normal | intensity `0.3 - 0.5` | fine 2-4 mm granular relief only |

* Grain scale: **2-4 mm**. It must fade out with distance via mips -- if you
  can resolve individual grains at 20 m the normal/roughness maps are too
  strong or lacking mips.
* Keep the normal **flat at court scale**. Real clay is screeded level; big
  undulations read as tarmac, not terre battue.

## 4. Lines

* **Width:** 50 mm for all boundary lines; 25 mm for the centre service line
  and the doubles-alley centre line (ITF). Model them at true width -- clay
  lines look noticeably wider than on hard courts because they are tape or
  paint sitting proud of loose material.
* **Color:** `#EFEAE4` albedo -- a warm off-white. Measured sunlit lines land
  around `#E8E8E8`-`#F0F0F0` and shaded lines around `#B8B8B8`; pure
  `#FFFFFF` immediately reads as a decal.
* **Wear:** lines must be *partially buried*. Break the edges with the clay
  layer so 10-30% of the line is dusted over, worst at the baseline corners
  and where players drag their feet. Feather edge by 2-6 px at 1080p / 5 m.
* **Roughness:** match the court (`~0.86`). Lines painted on clay do not
  gloss.

## 5. Sun / lighting (late-afternoon look)

| Parameter | Value |
|---|---|
| Directional Light rotation | **Pitch `-70`**, **Yaw `255`**, Roll `0` |
| Sun elevation | 20 deg above horizon (accept 18-24) |
| Sun azimuth | 255 deg -- sun in the WSW, shadows rake toward the ENE |
| Sun color (day) | `#FFF3E8` |
| Sun color (late afternoon) | `#FFDFC2` to `#FFD2AE` |
| Light source angle | `1.5 - 3.0` deg for a hazy late-afternoon penumbra |
| SkyLight intensity | `0.6 - 0.9`, recapture on move |
| Shadow tint / ambient floor | `#423B39` |

* **Shadow length check:** at 20 deg elevation, a 1 m object casts a shadow
  `cot(20 deg) = 2.75 m` long. The net-post shadow should measure ~2.75x its
  height. If it is shorter, the sun is too high; longer, too low.
* **Direction check:** with Yaw 255 the shadow of the net post points roughly
  east-northeast across the court. If it points the wrong way, flip Yaw by
  180 -- what matters is the sun sits 18-24 deg up and 30-60 deg off the
  court's long axis.
* Sun elevation below ~15 deg makes the whole court a sliver of light; above
  ~30 deg it stops reading as late afternoon and shadows stop describing the
  surface.
* Keep the shadow floor from crushing: shadowed clay must land near the
  measured **p10 = 60**, not 0-20. Use `#423B39` as the ambient/shadow color
  rather than raising SkyLight intensity (which flattens contrast).

## 6. Exposure and contrast

Targets measured on the court surface (central crop, sky/people rejected):

| Metric | Target | Measured |
|---|---|---|
| Mean luminance | **125** (range 117-133) | 125.4 |
| Contrast (std dev) | **47** (range 40-54) | 46.9 |
| p10 luminance | **60** (range 50-70) | 60.2 |
| p90 luminance | **186** (range 175-200) | 186.3 |
| Mean warmth (R-B) | **51** (range 35-65) | 51.5 |

How to hit them:

* **Disable auto-exposure** for reference/validation shots (or clamp
  MinEV100/MaxEV100 to a 0.4 EV window) so measurements are reproducible.
  Auto-exposure will happily hide a wrong albedo.
* Start at **exposure compensation 0.0 EV**, then adjust until the swept,
  sunlit court reads **125-135** in the middle of frame.
* The clay must span roughly **60 (in shadow) to 186 (in sun)**. If the
  whole court sits in a 40-level band, contrast is too flat -- check
  SkyLight intensity and the tonemapper before raising sun intensity.
* **Tonemapping:** the default ACES curve pushes bright clay toward salmon
  and desaturates the mid-tones. Verify `R-B >= 35` after tonemapping; if it
  drops below that, either lower exposure by 0.3 EV or add a small
  saturation push (+5..10%). Do not fix it by making the albedo more
  saturated -- that breaks the albedo value above.
* Bloom should be barely present (intensity <= 0.35). No lens dirt, no
  heavy vignette, no chromatic aberration on reference captures.

---

## 7. "Does it look real?" -- 5 checks

- **Sweep marks are visible at 3-6 m and gone by 15-20 m.** The drag marks
  from the court drag must show as low-contrast luminance banding (about
  +/- 5-8 levels off the mean) when you stand 3-6 m away, and dissolve into
  flat colour at 15-20 m. If the pattern is crisp at 40 m the texture has no
  mip falloff; if it is invisible at 3 m the contrast or normal is too weak.
  Freshly swept, never a repeating tile.

- **Lines are slightly worn, never razor-straight.** Every line edge is
  broken and partially dusted over (10-30% buried, worst at the baselines),
  coloured `#EFEAE4` rather than white, and feathered 2-6 px at 5 m. A line
  with a clean, uniform, pure-white edge -- or one that is perfectly straight
  all the way to the horizon -- is a decal, not a clay court.

- **Shadow edges have the right softness, and it varies with distance.**
  At 20 deg sun elevation, contact shadows (net post, player feet) should
  have a tight 1-2 cm penumbra, while shadows cast by the fence or
  windscreen 10+ m away should blur to 15-40 cm. Uniformly razor-sharp means
  Source Angle is too small; uniformly blurry means it is too large. Shadow
  interiors must sit near luminance **60**, tinted `#423B39`, never crushed
  to black.

- **No visible tiling, and the grain never crawls.** Inspect at 3 m, 10 m
  and 40 m plus a slow lateral camera move. You should not be able to find a
  repeating motif at any distance, and the 2-4 mm brick grain must fade
  smoothly with distance rather than shimmer/crawl as you move. Crawling
  grain means missing mips or an over-strong normal/roughness map.

- **Colour and exposure land inside the measured window.** Sunlit clay reads
  ~180-200, shaded clay ~55-70, court mean ~125 with `R-B >= 35`. If the
  sunlit court clips toward white, exposure is too high; if it turns salmon
  or pink, the tonemapper is desaturating -- correct with exposure, not with
  a more saturated albedo; if it reads grey-brown, the albedo has drifted
  from `#C27A5E` toward `#B97459` or lower.

---

## 8. Reproducing these numbers

```powershell
# 1. (Re)fetch the reference photos - idempotent, verifies every download
powershell -NoProfile -ExecutionPolicy Bypass -File .\Tools\download_refs.ps1

# 2. Re-measure -> palette.json, PALETTE.md, swatch.png
powershell -NoProfile -ExecutionPolicy Bypass -File .\Tools\analyze_refs.ps1
```

Both scripts are deterministic: re-running never duplicates entries and
reproduces `palette.json`, `PALETTE.md` and `swatch.png` byte for byte.
