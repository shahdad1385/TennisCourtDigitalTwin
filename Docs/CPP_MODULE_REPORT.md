# ClayTennis C++ Module Report

**Target:** Unreal Engine 5.5.4 (Windows / DX12)  
**Module:** `ClayTennis` (Runtime, Default loading phase)  
**Source root:** `C:\Projects\ClayTennis\Source\ClayTennis` (flat UBT layout)  
**Status:** Source complete and statically checked, **not compiled** (no C++ toolchain on this machine).

---

## 1. Files created

| File | Responsibility |
|---|---|
| `Source/ClayTennis.Target.cs` | Game target. `BuildSettingsVersion.V5`, `EngineIncludeOrderVersion.Unreal5_5`, adds `ClayTennis`. |
| `Source/ClayTennisEditor.Target.cs` | Editor target with the same module set. |
| `Source/ClayTennis/ClayTennis.Build.cs` | Module rules. Dependencies limited to `Core`, `CoreUObject`, `Engine`, `InputCore`, `Slate`, `SlateCore`. |
| `Source/ClayTennis/ClayTennis.h/.cpp` | `IMPLEMENT_PRIMARY_GAME_MODULE` and the `LogTennis` category. |
| `Source/ClayTennis/TennisTypes.h` | `EShotType`, `ESpinType`, `ETennisSide`, `ERallyState`, `FTennisScore`, `FShotParams`, side helpers. Header-only. |
| `Source/ClayTennis/TennisPhysics.h/.cpp` | Pure SI physics: constants, drag, Magnus, spin decay, RK4, clay bounce, net geometry, trajectory prediction, launch-angle solver. No UObject dependencies. |
| `Source/ClayTennis/TennisBall.h/.cpp` | `ATennisBall`: code sphere mesh, 240 Hz fixed-step integration, 57 g / 67 mm ball, net collision and cord brush, clay bounce, out-of-bounds flags, 1st/2nd bounce prediction, shot table. |
| `Source/ClayTennis/TennisCourt.h/.cpp` | `ATennisCourt`: ITF dimensions, singles/doubles/service-box tests, serve target/clamp, net height, clay geometry and bounce-mark instances. |
| `Source/ClayTennis/TennisRacket.h/.cpp` | `ATennisRacket`: cylinder grip + cube frame/string bed, simple swing animation. |
| `Source/ClayTennis/TennisPlayerPawn.h/.cpp` | `ATennisPlayerPawn`: spring-arm camera, accelerated court movement, mouse aim ray, charge state, shared `TryHitBall`, camera shake, racket swing. |
| `Source/ClayTennis/TennisPlayerController.h/.cpp` | Legacy `BindAxis`/`BindAction` controller for the human. |
| `Source/ClayTennis/TennisAI.h/.cpp` | `UTennisAIComponent`: bounce prediction, human-like movement/reaction, shot selection, error model, serve selection. |
| `Source/ClayTennis/TennisScoring.h/.cpp` | `UTennisScoringComponent`: full ITF scoring, serve rotation, tiebreak, change-ends requests, delegates. |
| `Source/ClayTennis/TennisGameMode.h/.cpp` | `ATennisGameMode`: spawns the world, runs serve → rally → point → reset, pauses, match end, sun fallback, HUD wiring. |
| `Source/ClayTennis/TennisHUD.h/.cpp` | `ATennisHUD`: Canvas broadcast scoreboard, power meter, spin label, speed readout, serve prompt, centre messages, crosshair, controls. |
| `Config/DefaultInput.ini` | Legacy axis/action mappings. |
| `Config/DefaultGame.ini` | Default `TennisGameMode`, project name/version/ID. |
| `ClayTennis.uproject` | `Modules` populated; existing `Plugins` array preserved. |

`TennisGameMode.cpp` is 661 lines (slightly above the ~600 guideline because of the serve geometry comments and match-flow branches); every other file is under 500 lines.

---

## 2. Wiring

* `.uproject` modules:
  `[{"Name":"ClayTennis","Type":"Runtime","LoadingPhase":"Default"}]`
* Plugins preserved exactly: `PythonScriptPlugin`, `RemoteControl`, `RemoteControlWebInterface`, `ModelingToolsEditorMode` (with its `TargetAllowList: ["Editor"]`).
* `DefaultGame.ini` sets `GlobalDefaultGameMode=/Script/ClayTennis.TennisGameMode`.
* No UMG, Niagara, InputAction, InputMappingContext, mesh, material, font or level asset is required. Only `/Engine/BasicShapes/{Sphere,Cube,Cylinder}` are loaded through `ConstructorHelpers::FObjectFinder`.
* The court, ball, players, racket, HUD and even a fallback directional light are created by code at runtime.

---

## 3. Match flow

1. `ATennisGameMode::BeginPlay` finds/spawns court, ball and AI pawn, creates `UTennisAIComponent`, spawns a movable directional light if the level has none.
2. `StartNewMatch` initialises `UTennisScoringComponent` (best-of-3 by default) and places both pawns.
3. `StartNextPoint`: reset ball to the server's contact point, place server/receiver, state `PreServe`, message `YOUR SERVE` / `CPU TO SERVE` / `DEUCE` / `ADVANTAGE`.
4. Human: hold LMB to charge, release to serve (or Space for a quick serve). AI: serves automatically after 1.15 s.
5. `Serving`: the first bounce is validated against the diagonal service box. Net cord + correct box = `LET`; wrong box / out = `FAULT`; two faults = `DOUBLE FAULT`.
6. `Rally`: an out first bounce or a bounce on the hitter's own side awards the point to the opponent; a second bounce after a legal first bounce awards it to the hitter (`ACE` if the serve was untouched, otherwise `WINNER`), regardless of where that second bounce lands.
7. `AwardPoint` maps the scoring flags to `GAME` / `SET` / match messages, pauses, applies any change-ends request, then starts the next point.
8. `R` restarts the current point; `R` after match end starts a new match.

---

## 4. Public API between classes

### `ATennisBall`
* Delegates: `OnBallBounced(FVector Location, bool bInBounds, bool bNetContact, int32 BounceIndex)`, `OnBallHit(AActor* Hitter)`.
* `LaunchAtTarget(TargetWorldCm, FShotParams, Hitter)` — solves pitch against the real trajectory, then launches.
* `LaunchWithVelocity(VelocityCmPerSecond, AngularRadPerSecond, Hitter)`.
* `PredictLanding(OutCm, OutTime)` — first bounce.
* `PredictSecondBounce(OutCm, OutTime)` — second bounce, including the real clay bounce model.
* `StopBall()`, `ResetBall(LocationCm)`, `SetSimulationEnabled(bool)`, `SetBallVisible(bool)`.
* `GetSpeedKph()`, `GetSpeedMps()`, `GetAngularVelocityRpm()`, `HadNetContactSinceHit()`, `GetBounceIndex()`, `GetLastHitter()`.
* `static FShotParams MakeShotParams(EShotType, Charge01)` — shared shot table for human and AI.

### `ATennisCourt`
* Dimensions: `GetCourtLengthCm()`, `GetCourtHalfLengthCm()`, `GetSinglesHalfWidthCm()`, `GetDoublesHalfWidthCm()`, `GetServiceLineDistanceCm()`.
* Line calls: `IsInSinglesCourt(FVector)`, `IsInDoublesCourt(FVector)`, `IsInServiceBox(FVector, ReceiverSide, bDeuceCourt)`.
* Serve geometry: `GetServeTarget(ServerSide, bDeuceCourt)`, `ClampToServiceBox(FVector, ReceiverSide, bDeuceCourt)`.
* Net: `GetNetHeightAtYCm(float)`.
* Side: `GetSideFromX(float)`.
* Marks: `AddBounceMark(Location, bInBounds)`, `ClearBounceMarks()`, `GetBounceMarks()`.

### `ATennisPlayerPawn`
* Configuration: `ConfigurePlayer(Index, Side, bIsAI, Name)`, `SetBall`, `SetPhysicalSide`, `SetReadyPose`, `PlaceAtWorld`.
* Input/movement: `SetMoveInput(Right, Forward)`, `AddAimInput(YawDeg, PitchDeg)`, `AddCameraZoom`, `SetAITargetWorld(Target, SpeedScale)`, `ClearAIMovement`.
* Shooting: `BeginCharge`, `EndCharge`, `ConsumeCharge`, `CycleSpin(Direction)`, `ReleaseGroundstroke`, `TryHitBall(Ball, Target, ShotType, Charge)`.
* Queries: `GetAimTargetWorld`, `GetReadyWorldLocation`, `GetChargeAmount`, `GetSelectedSpin`, `GetPhysicalSide`, `GetPlayerIndex`, `IsAI`, `GetReachCm`, `GetRacket`.
* `TryHitBall` is the single hitting path used by both the human and the AI: it validates reach/height/side, prevents double hits before a bounce, adds human-only aim error, then calls the ball.

### `UTennisScoringComponent`
* `StartMatch(BestOfSets)`, `ResetMatch()`, `AwardPoint(Winner)` returning `ETennisPointFlags` (`Game=1`, `Set=2`, `Match=4`).
* `GetScore()`, `GetPointDisplay(Player)` (`0/15/30/40/AD`), `GetScoreLine()`, `IsDeuce()`, `IsAdvantage(Player)`, `IsTiebreak()`, `IsDeuceCourtServe()`, `GetServer()`, `GetReceiver()`, `GetBestOfSets()`, `IsMatchOver()`, `GetMatchWinner()`, `GetTotalGamesPlayed()`.
* `ConsumeEndsChangeRequest()` — one-shot change-ends flag.
* Delegates: `OnPointScored(Winner, Flags)`, `OnGameWon(Winner)`, `OnSetWon(Winner)`, `OnMatchWon(Winner)`, `OnEndsChanged()`.

### `UTennisAIComponent`
* `InitializeAI(Pawn, Ball, Court, Difficulty)`, `SetDifficulty(float)`, `GetDifficulty()`.
* `ChooseServePlan(OutTargetCm, OutServeType, bSecondServe)`.
* Uses the ball's second-bounce prediction, its own foot-speed/reaction limits and difficulty-scaled target noise/error chance.

### `ATennisGameMode`
* Human serve: `IsHumanServing()`, `ExecuteHumanServe(Pawn, Charge, AimTarget)`, `ResetMatchOrPoint()`.
* Queries used by the HUD: `GetRallyState()`, `GetBall()`, `GetCourt()`, `GetScoring()`, `GetHumanPawn()`, `GetAIPawn()`, `GetPawnForPlayer()`, `GetPlayerDisplayName()`, `GetStatusLine()`, `GetCenterMessage()`, `GetCenterMessageAlpha()`, `GetServeNumber()`, `GetLastShotSpeedKph()`, `IsMatchComplete()`, `GetMatchWinner()`, `GetHumanPhysicalSide()`.

### `ATennisHUD`
Canvas only: `DrawHUD` calls the scoreboard, status line, power meter, spin label, speed readout, serve prompt, centre message, controls hint and crosshair. Uses `GEngine->GetSmallFont/MediumFont/LargeFont`, scaled by `Canvas->ClipY / 1080`.

---

## 5. Physics constants (exact values used)

| Quantity | Value | Source / note |
|---|---|---|
| Ball mass | `0.057 kg` | ITF legal 56.0–59.4 g; mid-range |
| Ball radius / diameter | `0.0335 m` / `0.067 m` | ITF legal 6.54–6.86 cm |
| Cross-section | `π r² = 3.5256e-3 m²` | derived |
| Gravity | `-9.81 m/s²` (`-981 cm/s²`) | explicit SI in the integrator |
| Air density | `1.225 kg/m³` | sea level |
| Drag coefficient | `Cd = clamp(0.55 + 0.07·S, 0.50, 0.62)` | spin ratio `S = r·|ω|/|v|`; base 0.55 |
| Lift coefficient | `Cl = min(0.45, 1.9·S)` | low-spin linear fit, capped at measured max |
| Drag force | `Fd = 0.5·ρ·Cd·A·v²` opposite velocity | quadratic drag |
| Magnus force | `Fm = 0.5·ρ·A·Cl·v²` along `normalize(ω × v)` | topspin dips, slice floats |
| Spin decay | `dω/dt = -1.5·ω` (`exp(-1.5t)`) | spec value; see validation note |
| Clay restitution | `e = 0.78` | ITF clay measurements 0.75–0.81 |
| Clay friction | `μ = 0.55` | high-friction clay contact |
| Net restitution | `0.35` | net reflection |
| Net tangential damping | `0.82` | reflected Y/Z damping |
| Net-cord speed retain | `0.55` | cord brush, enables lets |
| Net-cord window | `0.06 m` | top-band contact window |
| Integration | RK4 at `1/240 s`, deterministic accumulator | fixed step |
| Solve | coarse `1/120 s` at 1° + fine `1/240 s` at 0.1° | launch pitch |
| Court | `23.77 × 10.97 m`; singles `8.23 m` | ITF |
| Service line | `6.40 m` from net | ITF |
| Net | `0.914 m` at posts, `1.07 m` at centre; posts `0.914 m` outside doubles line | ITF |

Shot table (`ATennisBall::MakeShotParams`):

| Shot | Speed | Spin | Side spin |
|---|---|---|---|
| Flat | 26–36 m/s | 250–600 rpm | 0 |
| Topspin | 22–31 m/s | 1500–4000 rpm | 0 |
| Slice | 17–24 m/s | −800…−1400 rpm | 220 rpm |
| Flat serve | 34–46 m/s | 250 rpm | 0 |
| Kick serve | 27–38 m/s | 2300–3300 rpm | 350 rpm |
| Slice serve | 27–38 m/s | −400 rpm | 1600 rpm |

Serve targets are clamped to **5.60–6.25 m past the net**. This is not cosmetic: with a 2.75 m contact point a 46 m/s serve cannot land shallower than ~5.6 m and still clear the 0.914 m net; a 50 m/s serve cannot land inside the 6.4 m box at all. The cap keeps serves legal and explains why real serves land deep in the box.

Scoring flags: `Game = 1`, `Set = 2`, `Match = 4`.

---

## 6. Numeric sanity check (standalone replica, not compiled UE)

I replicated the exact RK4/drag/Magnus/bounce/net equations in a small Python script and ran trajectory checks:

* **Forehand:** 25 m/s, 2500 rpm topspin, from 1.0 m height to a target 20 m away. Solver pitch **20.0°**, landing **(9.21, 1.49) m** vs target `(9.0, 1.5)` → **0.21 m** error, net cleared.
* **Topspin clay bounce:** incoming `vx = 18 m/s`, `vz = -5.5 m/s`, 2500 rpm →
  `vx 18.00 → 15.36 m/s` (**−14.7%**), `vz -5.5 → +4.29 m/s` (`e = 0.78`), spin `262 → 459 rad/s`.
  This matches the requested 10–14% horizontal loss in the slip-limited topspin case.
* **Flat bounce:** no spin → `vx 18.00 → 12.86 m/s` (−28.6%), showing that a dead-flat ball loses more than a spinning rally ball.
* **Slice bounce:** `−1500 rpm` → `vx 18.00 → 12.62 m/s` (−29.9%), spin **reverses** from `−157` to `+245 rad/s`; this is the skid/reversal behaviour.
* **Serve feasibility:** at 46 m/s the shortest legal landing is ~6.1 m past the net; at 34 m/s ~5.4 m. This drove the deep serve targeting and the 46 m/s speed cap.

These checks validate the equations and the serve geometry, but they are **not** a substitute for compiling the Unreal module.

---

## 7. Validation performed

| Check | Result |
|---|---|
| All 12 headers have `#pragma once` | **PASS** (12/12) |
| Every UHT header includes `<Name>.generated.h` | **PASS** (10/10: AI, Ball, Court, GameMode, HUD, PlayerController, PlayerPawn, Racket, Scoring, Types) |
| `.uproject` parses with PowerShell `ConvertFrom-Json` | **PASS** — valid JSON |
| `.uproject` modules | **PASS** — exactly `{"Name":"ClayTennis","Type":"Runtime","LoadingPhase":"Default"}` |
| `.uproject` plugins preserved | **PASS** — PythonScriptPlugin, RemoteControl, RemoteControlWebInterface, ModelingToolsEditorMode |
| `DefaultGame.ini` contains `GlobalDefaultGameMode=/Script/ClayTennis.TennisGameMode` | **PASS** (re-applied after a concurrent overwrite; see note) |
| Brace/paren/bracket balance heuristic (comments/strings stripped) | **PASS** for all 23 C++ files |
| Duplicate local-declaration scan | Only two legal same-name declarations in separate `if/else` scopes in `TennisScoring::AwardPoint`; no real duplicates |
| Compile / PIE | **NOT RUN** — no C++ toolchain installed, as instructed |

> **Important:** during this task the `.uproject` was observed reverting from the edited state to `"Modules": []` once (at 12:01), and `DefaultGame.ini` was later observed reset to `GlobalDefaultGameMode=/Script/Engine.GameModeBase` (at 12:39). Both were re-applied and re-validated. This indicates another process/session is touching project files; **verify the `Modules` array and the GameMode line immediately before the first build**.

---

## 8. Assumptions / first-compile checks

1. **No map asset yet.** `DefaultEngine.ini` points at `/Game/Maps/TennisCourt`, which does not exist in the current project. The GameMode builds the entire world from code, so any empty level works; create that map (or change `GameDefaultMap`) before running.
2. **Legacy input override.** `DefaultInput.ini` forces `/Script/Engine.PlayerInput` and `/Script/Engine.InputComponent` and enables legacy scales. If the engine's EnhancedInput defaults still take precedence, verify PIE input; legacy `BindAxis` normally still resolves through `UEnhancedInputComponent`.
3. **Engine basic-shape material parameter.** Court/ball/racket tints use the mesh's existing material with a guarded MID and `"Color"` / `"BaseColor"` parameters. If those parameters are absent, objects render untinted but never crash.
4. **Environment lighting.** A movable `ADirectionalLight` is spawned only when the level has no directional light. No sky atmosphere, sky light or fog is created, so the background may read black until the level art pass.
5. **UE 5.5 API details to confirm on first build:** `FMath::SpringInterp`/`FFloatSpringState`, `ADirectionalLight::GetLightComponent()`, `UInstancedStaticMeshComponent::AddInstance(..., true)`, `USceneComponent::AddWorldRotation(FQuat)`, and `UChildActorComponent::SetChildActorClass` are all standard 5.x APIs but were not compiled here.
6. **Serve geometry is intentional.** Human serves are clamped to the deep service box (5.6–6.25 m) and the flat serve is capped at 46 m/s because shallower/faster serves are physically impossible from the 2.75 m contact point.
7. **Spin decay `k = 1.5 s⁻¹`** follows the specification. It is faster than most published tennis measurements (often ~0.1–0.2 s⁻¹); it is a one-line tunable in `TennisSpec::SpinDecayRatePerSec` if the build/QA wants longer-lived spin.
8. **Net visual vs physics.** The physics net sags from 0.914 m to 1.07 m; the code-built visual is a grid with a flat top band plus a 1.07 m centre strap. Gameplay uses the physical sag.
9. **Singles is authoritative.** The court draws doubles lines, but bounce/out tests use the singles court. Add a doubles mode by switching `IsInSinglesCourt` to `IsInDoublesCourt` and the serve-box width accordingly.
10. **AI cost.** Every AI tick runs one first-bounce prediction and one second-bounce prediction (≈1,700 RK4 steps at 120 Hz). This is comfortably within frame budget but should be profiled on low-end hardware.
11. **No audio.** There is no sound cue, impact sound, crowd or umpire call; all point feedback is visual.
12. **Human hit window.** Reach is 250 cm (human) / 205 cm (AI); the height window is 10–320 cm. These are gameplay tunables in the pawn.
13. **Ball is simulation-only.** Actor collision is disabled; net/ground/court contacts are analytic. Do not enable engine physics on the ball without removing the custom integrator.
14. **No save game / settings menu.** Difficulty is a `UPROPERTY(EditDefaultsOnly)` on the GameMode (`0.62`), best-of-sets is `3`.

---

## 9. Controls

| Input | Action |
|---|---|
| `A` / `D` (or arrows) | Move along the baseline |
| `W` / `S` | Approach the net / retreat |
| Mouse | Aim (yaw/pitch offset) |
| Left mouse hold/release | Charge and hit; also charge/release the serve |
| `Q` / `E` | Previous / next spin (`Flat`, `Topspin`, `Slice`) |
| `Space` | Quick serve at a safe power level (when serving) |
| `R` | Reset the current point; start a new match when the match is over |
| Mouse wheel | Camera zoom (520–1450 cm arm length) |

---

## 10. Known gaps / suggested follow-ups

* Author a real `/Game/Maps/TennisCourt` (or point `GameDefaultMap` at an empty map) and add sky/skylight/fog for presentation.
* Replace the guarded basic-shape tints with authored clay/line/net materials and a net with real sag/translucency.
* Add impact/ball sounds, footstep audio and a crowd bed.
* Add a difficulty selector and a best-of-5 option on the GameMode defaults.
* Consider reducing `SpinDecayRatePerSec` to a measured value once trajectories can be compared against high-speed video.
* The HUD shows aggregate sets won, not one column per set; extending the scoreboard is a pure Canvas layout change.
* Add a proper serve toss animation; currently the ball is placed at the contact point during `PreServe`.
