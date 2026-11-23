# Iteration report — 2026-11-23

Scope items completed (AGENTS.md §7 priorities 1–3) and verified with the
`-autoshot` work loop (§8). Builds: clean (exit 0). Log check: no material
compile failures after the fixes below.

## 1. Tile banding killed (priority 1)

Before: court 5×2 and apron 8×5 tiles of ~5 m, every tile sampling the same 4K
clay texture at 0°/180° only. The broom-sweep blotches repeated in a visible
grid (see `Docs/banding_before.png`, taken 06:00 from the lookdev position).

After: court is 2×1 tiles (~11.9 × 11.0 m) and apron 3×2 tiles (~14.7 × 13.0 m),
each tile yawed 0/90/180/270 by a spatial hash `((i*73856093) ^ (j*19349663)) & 3`.
A 90/270 rotation swaps the plane footprint, so the scale axes swap to
compensate. The texture pattern now repeats once per half-court instead of every
5 m, and adjacent tiles never share an orientation. No UV nodes (trap 2), no new
textures; normal maps stay valid because the tangent frame rotates with the
actor. Result: `Docs/banding_after.png` (same lookdev position) — the 5 m grid
is gone.

## 2. Player visible in the gameplay view (priority 2)

Three root causes fixed:

- `ATennisPlayerPawn::BodyMesh` was scaled 0.42/0.42/1.8 on the r50×h200 cm
  Cylinder → 3.6 m tall, centered on the 90 cm root → half-buried. Now
  0.36/0.36/0.90 → 1.80 m, spanning exactly 0–180 cm.
- The level's lookdev CineCamera (`PhotoCam`) had `auto_activate_for_player =
  PLAYER0` and stole the view target in `-game`. Removed; the pawn spring arm is
  authoritative.
- `BeginAutoShot` always overrode the view target, so the real gameplay framing
  could not be verified. LookDev.txt now also accepts the single token `pawn`,
  which skips the camera override.

Evidence: `Docs/gameplay_view.png` — pawn camera, player + racket center-frame,
log line `[AUTOSHOT] view target=TennisPlayerPawn_0 loc=X=-1258.5 Y=-193.8 Z=90`
(ready position, deuce stance).

## 3. Lines 7 cm and brighter (priority 3)

`T = 0.06 → 0.07` in `Tools/build_level_v3.py`; lines now use `MI_LinePaint`
with a brightened `BaseColor` override (0.94, 0.93, 0.90). `MI_CourtLine` was a
legacy instance with unknown params.

## 4. Bonus: M_RacketStrings compile failure fixed

The standing log check (§8 step 5) caught, on every run:

```
M_RacketStrings.uasset: Failed to compile Material for platform PCD3D_SM6
(Node TextureSampleParameter2D) Sampler type is Masks, should be Color for
/Game/Textures/T_RacketStrings_Albedo
```

Two project-specific traps in one material: `SAMPLERTYPE_MASKS` on a default
sRGB Color texture (trap 1 variant) and a `TextureCoordinate` node (trap 2).
Rebuilt in place with `Tools/fix_racket_strings.py` (default sampler, default
UVs, RGB → BaseColor, A → OpacityMask); `build_materials_v4.py` patched so a
rebuild does not regress. The string bed now renders with its texture.

## Verification loop notes (§8)

- Level rebuilds: `UnrealEditor.exe <uproject> -ExecutePythonScript=
  Tools/build_editor_run.py` — purged 225 stale actors, saved, quit.
- `save_current_level=False` in `RawAssets/save_result.txt` after a build is
  benign: the script already saved inside `build()`; the redundant follow-up
  saves report "nothing dirty".
- **Ops trap found:** launching the game while a prior engine instance is still
  tearing down kills the new process silently mid-init (no crash folder, no
  WER event, log just stops at D3D/LiveCoding init). Always wait until no
  `UnrealEditor` process remains before the next launch.
- Screenshot numbering: each run writes up to two files (`RequestScreenshot`
  + `HighResShot`); the newest file by write time is the latest run's.

## Remaining (§7)

4. Audio (ball/net/crowd, CC0) · 5. Full gameplay test (serve → rally → point;
   the match loop already runs autonomously in the autoshot runs) ·
   6. Shipping package · 7. README polish.
