# scene_props.py — Scene Props Builder

`Tools/scene_props.py` builds the net, perimeter fence and club surround on top
of the clay court level, entirely from `/Engine/BasicShapes/{Cube,Plane,Cylinder,
Cone,Sphere}`. It is meant to be imported by the build pipeline inside the UE 5.5
editor (same style as `build_level.py`):

```python
import sys
sys.path.insert(0, r"C:\Projects\ClayTennis\Tools")
import scene_props
counts = scene_props.build_all()
```

Conventions: locations in **cm**, mesh scales in **metres** (BasicShapes sizes),
UE **Z-up**, court centred at the origin, length along **X**, width along **Y**.
Every function is defensive — a missing material skips that mesh (and is logged
as `skip <label> (material missing)`) instead of crashing, and every
`set_editor_property` call is individually guarded.

## Function reference

### 1. `build_net(materials)` → list of actors
A real tennis net at `x=0` (court length axis), normal facing **+X**:

| Actor | Shape | Location (cm) | Scale (m) | Material |
|---|---|---|---|---|
| `Net_Mesh` | Plane (rot -90 pitch) | (0, 0, 53.5) | (1.07, 12.8) → 1280 × 107 cm | `MI_Net` |
| `Net_Post_East` | Cylinder | (0, +640, 53.5) | (0.07, 0.07, 0.535) → r3.5, h107 | `MI_Metal` |
| `Net_Post_West` | Cylinder | (0, −640, 53.5) | (0.07, 0.07, 0.535) | `MI_Metal` |
| `Net_Band` | Cube | (0, 0, 104) | (0.03, 12.8, 0.06) → 6 cm tall white band | `MI_CourtLine` |
| `Net_Strap` | Cube | (0, 0, 53.5) | (0.02, 0.05, 1.07) → centre strap | `MI_CourtLine` |

Bottom edge at z=0, top at z=107. The 13×1 UV tiling is baked into `M_Net`
(nothing set here). 5 actors.

### 2. `build_fence(materials, x_half=1750, y_half=950)` → list of actors
4.0 m tall dark-green windscreen on all four sides at the given half extents
(defaults match `build_level.py`), two-sided `MI_Windscreen`:

- `Fence_End_Plus` — Plane at (+1750, 0, 200), 4.0 × 19.0 m, rot (−90,0,0)
- `Fence_End_Minus_Seg_*` — two Planes at (−1750, ±505, 200), 4.0 × 8.9 m each —
  leaves the **1.2 m gate gap** centred at y=0 on the −X side
- `Fence_Side_*` — Planes at (0, ±950, 200), 4.0 × 35.0 m, rot (−90,90,0)
- `FencePost_*` — 40 cylinders (r0.06 m scale, h4.2 m, z=210): every 300 cm
  strictly inside each side (11 per X side, 7 per Y side) + 4 corner posts
- `GatePost_*` — 2 slim cylinders framing the gate at (−1750, ±60)
- `FenceRail_*` — 5 horizontal cylinders (top rail, z=412, r≈4 cm): 2 full X
  rails, 1 full +X rail, 2 short −X segments flanking the gate
- `SponsorBand_*` — 2 white thin boxes (15 × 0.04 × 0.30 m) at z=110 on the two
  long sides (`MI_CourtLine`)

Total: 54 actors.

### 3. `build_landscape(materials)` → list of actors
- **Hedge** — 18 boxes 1.2 × 0.8 × 0.9 m at y=±1020 (outside the ±950 fence),
  x from −1700 to +1700, `MI_Hedge` if it exists else `MI_Windscreen`.
- **7 cypress trees** on the far +X side (x ≈ 2350/2550, y spread ±870):
  trunk cylinder (Ø30 cm) + cone canopy (2.5 m wide), total height 7.0–8.8 m,
  `MI_Tree` fallback `MI_Windscreen`. 14 actors.
- **3 floodlight towers** at (±2100, +1250) and (2100, −1250): 14 m pole
  cylinder, head cube, emissive Plane panel facing the court
  (`MI_Floodlight`, cast shadow off). 9 actors.
- **2 player benches** at (−1500, ±320): seat, back, 2 legs each (8 actors;
  seat/back `MI_Carbon` fallback `MI_Metal`, legs `MI_Metal`).
- **Ball cart** at (−1380, −470): box + 4 sphere wheels (5 actors).
- **Water cooler** at (−1420, 470): one white box (`MI_CourtLine`
  fallback `MI_Concrete`). 1 actor.

Total: 55 actors. Grand total worst case (everything missing and all 4 light
actors new): **119 < 120**.

### 4. `build_lighting(materials)` → list of *newly spawned* actors
Searches the level first (`EditorActorSubsystem.get_all_level_actors()` filtered
by label **and** class via `is_a()`), so nothing is duplicated:

- `Sun` (DirectionalLight): rotation (pitch −11, yaw 205), `intensity 9.0`,
  `use_temperature True`, `temperature 3600`, `atmosphere_sun_light True`,
  `atmosphere_sun_light_index 0`, `light_source_angle 0.7`, mobility MOVABLE.
- `SkyAtmosphere`: spawned only if no actor of that class exists.
- `SkyLight`: `real_time_capture True`, `intensity 1.0`, captured-scene source,
  then `recapture_sky()`.
- `HeightFog` (ExponentialHeightFog): `fog_density 0.006`,
  `fog_height_falloff 0.12`, `enable_volumetric_fog True`,
  `volumetric_fog_extinction_scale 0.35`.

If `build_level.py` already ran, all four exist and the returned list is empty
(existing actors are still re-configured).

### 5. `build_photo_camera()` → `[PhotoCam]`
`CineCameraActor` labelled `PhotoCam` at (−1500, −1050, 170), rotation computed
as a look-at of the net centre (0, 0, 53.5) via `Vector.rotation()`
(≈ pitch −3.6°, yaw 35°). `current_focal_length 35`, `current_aperture 4.0`,
`auto_activate_for_player = unreal.AutoReceiveInput.PLAYER0`. Re-uses an
existing `PhotoCam` instead of spawning a second one.

### 6. `build_all()` → dict of counts
Loads `/Game/Materials/MI_{Net,Metal,CourtLine,Windscreen,Floodlight,Hedge,Tree,
Carbon,Concrete}` (missing ones → `None`, logged), then runs 1–5 with each step
wrapped in its own try/except so one failure cannot abort the rest. Logs and
returns:

```python
{"net": n, "fence": n, "landscape": n, "lighting_new": n, "camera": n,
 "total_actors_spawned": n, "materials_loaded": n, "materials_missing": [...]}
```

## Things to verify in UE 5.5 (uncertain / risky API calls)

1. **`unreal.Rotator(pitch, yaw, roll)` positional order** — assumed to be
   (pitch, yaw, roll); matches the usage in `build_level.py` (sun rotator
   `Rotator(-9, 205, 0)`). If it is actually (roll, pitch, yaw), every rotation
   in this file must be reordered.
2. **Plane pitch −90 → normal facing +X** — with pitch −90 the Plane's +Z normal
   rotates onto +X and its local X (the first scale axis) becomes the *vertical*
   axis. That is why e.g. the net plane is scaled `(1.07, 12.8)`. If pitch sign
   convention is flipped, planes face −X instead (harmless for the two-sided
   net, matters for the single-sided case, see next item).
3. **`M_Windscreen` is built single-sided in `build_assets.py`** — this module
   enables `two_sided` on the *parent material* at runtime via
   `MaterialInstanceConstant.parent` + `set_editor_property("two_sided", True)`
   + `MaterialEditingLibrary.recompile_material` (through `ue_common.compile_and_save`).
   If property names differ in 5.5, this is a logged no-op and the outer fence
   faces will render only from one side.
4. **`unreal.Vector.rotation()`** — used for the camera look-at; falls back to a
   manual atan2/asin computation if the method is missing.
5. **`CineCameraComponent.current_focal_length` / `current_aperture`** — same
   names already used in `build_level.py`; if the camera spawns with defaults
   unchanged, these property names are the first thing to check.
6. **`CineCameraActor.auto_activate_for_player` + `unreal.AutoReceiveInput.PLAYER0`**
   — wrapped in try/except; verify it actually took effect on the spawned actor.
7. **`SkyLightComponent.real_time_capture` and `recapture_sky()`** — as used in
   `build_level.py`; `source_type` enum `SLS_CAPTURED_SCENE`.
8. **`ExponentialHeightFogComponent` property names** — `fog_density`,
   `fog_height_falloff`, `enable_volumetric_fog`,
   `volumetric_fog_extinction_scale` (the last one stands in for "extinction").
9. **`DirectionalLightComponent`** — `use_temperature`/`temperature` pair is
   required for 3600 K to apply; `atmosphere_sun_light` links the sun to the
   SkyAtmosphere.
10. **BasicShapes dimensions** — Plane 100×100 cm, Cube 100³, Cylinder r50 × h200,
    Cone base r50 × h100, Sphere r50. All scales in this file derive from these.
11. **`Actor.is_a(cls)` / `get_actor_label()`** for the duplicate search — both
    are standard, but the search is wrapped so any mismatch degrades to "not
    found" (→ possible duplicate spawn if labels/classes differ).
12. **`set_actor_location(loc, sweep, teleport)`** argument order for the
    PhotoCam re-position path.

## Notes for the caller

- `build_level.py` already spawns its own simplified net/fence/benches; run
  `scene_props` on a **fresh level** (or accept duplicates — nothing here deletes
  existing actors except re-using `Sun`/`SkyLight`/`PhotoCam`).
- Validation performed: PowerShell bracket balance (`()[] {}` all matched),
  no tabs, indentation multiples of 4 for statement lines (multi-line
  paren continuations are aligned to the open bracket). Python is not installed
  on the build machine, so `py_compile` could not be run locally — worth running
  inside the UE Python environment before executing.
- `MI_Hedge` and `MI_Tree` do not currently exist in `/Game/Materials` (they are
  not produced by `build_assets.py`); the module falls back to `MI_Windscreen`
  for both and reports them in `materials_missing`.
