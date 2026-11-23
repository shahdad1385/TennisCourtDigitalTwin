# SCOPE — TennisCourtDigitalTwin (ClayTennis)

## 1. What this is
A clay-court tennis simulation/game in Unreal Engine 5.5.4 with custom ITF physics and a
visual target of a photoreal terre-battue court.

- Repo: https://github.com/shahdad1385/TennisCourtDigitalTwin (branch `main`)
- Local path: `C:\Projects\ClayTennis`
- Engine: `C:\UE_5.5` (portable 5.5.4 build, NOT in the repo, ~48 GB)
- Game map: `/Game/Maps/TennisCourt` — C++ module: `ClayTennis`

## 2. Current state (honest)
Works: C++ builds clean, game launches, Canvas HUD draws (score / power / spin / serve prompt),
clay court with a 4K texture + broom sweeps, geometric net (65 cords), ITF lines, green windscreen,
trees/floodlights/benches, palette extracted from 10 real photos and applied (#C27A5E).
Incomplete/broken: visible 5 m tile banding on the court, players not framed by the gameplay camera,
lines a bit thin, no ball/net/crowd audio, shipping package untested, level has ~225 actors.

## 3. Hard rules (do not break)
- Toolchain: VS 2022 Build Tools 17.14 + MSVC v143 (14.44) + Windows SDK + .NET Framework 4.8.1
  Developer Pack. VS 2026 (18.x) is NOT supported. Build:
  ```
  & 'C:\UE_5.5\Engine\Build\BatchFiles\Build.bat' ClayTennisEditor Win64 Development `
    -Project='C:\Projects\ClayTennis\ClayTennis.uproject' -WaitMutex
  ```
- Only engine modules: Core, CoreUObject, Engine, InputCore, Slate, SlateCore.
  Do not add plugins without asking.
- Do not require any authored asset (no UMG, Niagara, InputAction/InputMappingContext).
  Everything must work from code + ini + Python.
- Legacy input (BindAxis/BindAction) is intentional. No UMG; UI is `AHUD::DrawHUD` only.
- Never rebuild or modify the engine. Only touch `C:\Projects\ClayTennis`.
- Never commit `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/` (see `.gitignore`).
- Do not download/import assets with unclear licensing. CC0/CC-BY only, with credits.

## 4. Discovered traps (most important section)
1. A `TextureSampleParameter2D` with `sampler_type = SAMPLERTYPE_NORMAL` fails to compile
   ("Failed to compile Material" → WorldGrid fallback). Keep the default sampler and wire RGB
   into `MP_NORMAL`. Always check the log:
   `Select-String Saved\Logs\ClayTennis.log -Pattern 'Failed to compile Material'`
2. `TextureCoordinate` nodes / world-position UV math broke material compilation in this project.
   Current solution: split the court into ~5 m tiles and rotate odd tiles 180°. Do not add UV nodes.
3. Commandlets (`-run=pythonscript`) cannot save `.umap`. Build/save levels inside the editor:
   `UnrealEditor.exe <uproject> -ExecutePythonScript=<script.py>`
4. `LevelEditorSubsystem.new_level(path)` re-opens the existing map and keeps old actors.
   Always destroy all actors after `new_level`, otherwise you get duplicates and giant stale meshes.
5. `GlobalDefaultGameMode` is sometimes ignored; force it in the URL for gameplay tests:
   ```
   -game "/Game/Maps/TennisCourt?game=/Script/ClayTennis.TennisGameMode"
   ```
6. Unattended screenshots only work through the project's own flag (desktop may be locked):
   `-game -autoshot` reads `Saved\LookDev.txt` (`x y z pitch yaw roll`), takes a screenshot
   after 22 s into `Saved\Screenshots\WindowsEditor\AutoShot*.png`, quits at 32 s.
7. Texture files are prefixed `T_`; import via `AssetImportTask` and read `imported_object_paths`.
8. The C++ `ATennisCourt` is gameplay logic/geometry only and is placed in the level as a hidden
   proxy actor (components hidden). The visual court, net and lines are built by Python.
   Do not mix the two.
9. First launch on this laptop (i7-11370H, 4 cores, RTX 3070 Laptop, 16 GB) compiles shaders for a
   long time; do not kill the game process early because the window appears late.

## 5. File map
```
Source/ClayTennis/
  TennisPhysics.*          drag / Magnus / spin decay / clay bounce, RK4 at 240 Hz
  TennisBall.*             custom projectile, trajectory prediction, net collision
  TennisCourt.*            ITF dimensions, in/out, service boxes, net height, bounce marks
  TennisPlayerPawn.*       spring-arm camera, charge/spin, aim
  TennisPlayerController.* legacy input bindings
  TennisAI.*               bounce prediction, target choice, human error model
  TennisScoring.*          15/30/40, deuce/advantage, tiebreak, best-of-3/5
  TennisGameMode.*         rally/serve/point loop + BeginAutoShot (test-only)
  TennisHUD.*              Canvas HUD
Tools/
  build_assets.py, build_materials_v5.py, mat_variant2.py   materials
  build_level_v3.py        level builder (tiles, geometric net, light, PostFX)
  build_editor_run.py      build + save level inside the editor
  gen_textures.ps1, gen_clay_v2.ps1                         procedural textures
  shot.ps1, shot_config.py                                  editor screenshots
  build.ps1                                                 build helper
Config/    DefaultEngine.ini (Lumen/VSM/SM6), DefaultGame.ini, DefaultInput.ini
Content/   Maps/TennisCourt.umap, Materials/MI_*, Textures/T_*
Docs/      CPP_MODULE_REPORT.md, VISUAL_TARGETS.md, SCENE_PROPS.md, preview_court.png
RawAssets/ Polyhaven sandy_gravel 4K, Clay2 (4K + macro), Refs (10 photos + palette.json)
```

## 6. Quality targets
- Palette: albedo `#C27A5E`, shadow `#423B39`, sun `#FFF3E8`, slightly worn white lines.
- Visual acceptance: broom sweeps visible at 3–6 m, lines slightly worn, bounce marks, soft shadows.
- Physics: ball 57 g / 67 mm, Cd = 0.55 + 0.07S, Cl ≤ 0.45, e = 0.78, μ = 0.55, spin up to 4000 rpm.
- Performance: stable 1080p on RTX 3070 Laptop; keep Lumen + VSM + TSR enabled.

## 7. Next priorities (in order)
1. Kill the tile banding: 8K texture, or 4 rotated/mirrored texture variants selected per tile.
2. Make players visible: inspect `BodyMesh` (Cylinder scaled 0.42/0.42/1.8 = 3.6 m tall!),
   gameplay camera framing, and pawn spawn.
3. Lines: 6 → 7 cm and slightly brighter.
4. Audio: ball/net/crowd with CC0 sounds.
5. Full gameplay test (serve, rally, point, AI) and bug fixes.
6. Shipping package via `RunUAT BuildCookRun` + standalone test.
7. Final docs and screenshots.

## 8. Required work loop
Verify every visual change objectively — never by assumption:
1. Build C++ if needed.
2. Rebuild the level with `build_editor_run.py`.
3. Run:
   ```
   UnrealEditor.exe <uproject> "/Game/Maps/TennisCourt?game=/Script/ClayTennis.TennisGameMode" `
     -game -autoshot -windowed -ResX=1280 -ResY=720
   ```
4. Read `AutoShot*.png`, compare with `RawAssets\Refs\ref_*.jpg` and the palette.
5. Check the log for `Failed to compile Material`.
6. Repeat until the render matches the reference.
Set the screenshot camera via `Saved\LookDev.txt` instead of recompiling.

## 9. Ask before
- Changing physics/scoring architecture, adding plugins or commercial assets, changing
  Lumen/platform settings, deleting level actors, editing `.uproject` (Modules/Plugins),
  or force-pushing `main`.

## 10. Definition of done (next version)
- No detectable tile banding at 1080p, color matches the palette.
- Player and racket visible in the gameplay view.
- One full rally (serve → 3 shots → point) without a crash.
- Before/after screenshots updated in `Docs/`.
- Clean build (exit 0) and README with run instructions.
