"""ClayTennis level v4: court rebuilt from zero to exact ITF specs.

Sources: ITF Rules of Tennis 2025 (Rule 1 - The Court), verified via Wikipedia:
  - Court 23.77 x 10.97 m (singles 8.23 m), measured to the OUTSIDE of lines
  - Service line 6.40 m from the net; centre service line 5 cm wide
  - Other lines 2.5-5 cm; baseline up to 10 cm; centre mark 10 cm (4 in)
  - Net 1.07 m at the posts, 0.914 m at the centre, posts 0.914 m outside the
    doubles court (12.8 m span); netting sags from posts to centre strap
  - Recommended run-off: total playing area 36.57 x 18.29 m (6.40 m run-back,
    3.66 m side run-off) - implemented exactly

Axes match the gameplay C++ (ATennisCourt): long axis = X, net at X=0 across Y,
posts at Y = +/-640, baselines at X = +/-1188.5.

No UV nodes / no TextureCoordinate expressions (project traps 1-2 in
AGENTS.md): flat acrylic materials on single planes, so no tiling banding is
possible by construction.
"""
import unreal
import sys
import traceback
import math

sys.path.insert(0, r"C:\Projects\ClayTennis\Tools")
import ue_common as U

OUT = r"C:\Projects\ClayTennis\RawAssets\build_level_v4_result.txt"
RES = []


def note(s):
    RES.append(str(s))


def mi(name):
    return unreal.EditorAssetLibrary.load_asset("/Game/Materials/" + name)


# ------------------------------------------------------------------ palette
# Tuned to the Outsy blue-court reference (hard acrylic court).
COL_IN = (0.045, 0.15, 0.42)      # inner court blue
COL_APRON = (0.16, 0.34, 0.22)    # surrounding acrylic green
COL_LINE = (0.96, 0.96, 0.94)     # line paint
ROUGH_IN, ROUGH_APRON = 0.58, 0.62

# ------------------------------------------------------------- ITF dims (cm)
HALF_LEN = 1188.5          # 23.77 / 2
HALF_DBL = 548.5           # 10.97 / 2
HALF_SGL = 411.5           # 8.23 / 2
SVC = 640.0                # service line, net-side edge, 6.40 m from net
POST_Y = 640.0             # posts 0.914 m outside doubles sideline
NET_POST_H = 107.0         # 1.07 m at posts
NET_CTR_H = 91.4           # 0.914 m at centre
TOTAL_L = 3657.0           # recommended playing area 36.57 m
TOTAL_W = 1829.0           # 18.29 m

W_BASE, W_LINE = 0.10, 0.05  # baseline 10 cm, all other lines 5 cm
# Line cubes sit 1 cm ABOVE the inner plane (bottom 1.0, top 2.0): coplanar
# geometry z-fights and the lines render black/patchy.
Z_APRON, Z_IN, Z_LINE = 0.0, 0.5, 1.5


def look_at(src, dst):
    d = dst - src
    yaw = math.degrees(math.atan2(d.y, d.x))
    pitch = math.degrees(math.atan2(d.z, math.sqrt(d.x * d.x + d.y * d.y)))
    return unreal.Rotator(pitch=pitch, yaw=yaw, roll=0.0)


def ensure(name, color, rough, metal=0.0, two_sided=False, emissive=None, unlit=False):
    """Flat material (vector param base colour + scalar roughness); no UV nodes."""
    p = "/Game/Materials/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(p):
        m = unreal.EditorAssetLibrary.load_asset(p)
        U.MEL.delete_all_material_expressions(m)
    else:
        m = U.new_material(name, two_sided=two_sided,
                           shading=unreal.MaterialShadingModel.MSM_UNLIT if unlit else None)
    if emissive:
        U.MEL.connect_material_property(
            U.vector_param(m, "EmitColor", emissive, -500, -200), "",
            unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    else:
        U.MEL.connect_material_property(
            U.vector_param(m, "BaseColor", color, -500, -200), "",
            unreal.MaterialProperty.MP_BASE_COLOR)
        U.MEL.connect_material_property(
            U.scalar_param(m, "Roughness", rough, -500, -60), "",
            unreal.MaterialProperty.MP_ROUGHNESS)
        if metal:
            U.MEL.connect_material_property(
                U.scalar_param(m, "Metallic", metal, -500, 60), "",
                unreal.MaterialProperty.MP_METALLIC)
    U.compile_and_save(m)
    inst = "/Game/Materials/MI_" + name[2:]
    if not unreal.EditorAssetLibrary.does_asset_exist(inst):
        U.new_instance("MI_" + name[2:], m)
    return m


def line_x(label, x, y, length, w=W_LINE):
    """Line running along X (perpendicular to the net)."""
    U.spawn_mesh(label, "Cube", (x, y, Z_LINE), (length, w, 0.01), LINE,
                 cast_shadow=False)


def line_y(label, x, y, length, w=W_LINE):
    """Line running along Y (parallel to the net)."""
    U.spawn_mesh(label, "Cube", (x, y, Z_LINE), (w, length, 0.01), LINE,
                 cast_shadow=False)


def net_height_at(y):
    """Net surface height: 0.914 m centre sagging to 1.07 m at the posts."""
    return NET_CTR_H + (NET_POST_H - NET_CTR_H) * (y / POST_Y) ** 2


def build():
    global LINE
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    les.new_level("/Game/Maps/TennisCourt")

    # new_level() can re-open the existing map, leaving old actors behind: purge.
    purged = 0
    for old_actor in list(actors.get_all_level_actors()):
        try:
            actors.destroy_actor(old_actor)
            purged += 1
        except Exception:
            pass
    note("purged actors: %d" % purged)

    # ------------------------------------------------------------ materials
    inner = ensure("M_CourtInner", COL_IN, ROUGH_IN)
    apron_m = ensure("M_ApronAcrylic", COL_APRON, ROUGH_APRON)
    inner = mi("MI_CourtInner")
    apron_m = mi("MI_ApronAcrylic")
    LINE = mi("MI_LinePaint")
    try:
        U.MEL.set_material_instance_vector_parameter_value(
            LINE, "BaseColor", unreal.LinearColor(*COL_LINE, 1.0))
    except Exception as exc:
        note("line brighten: %s" % exc)
    metal = mi("MI_Metal")
    screen = mi("MI_Windscreen")
    hedge = mi("MI_Hedge")
    tree = mi("MI_Tree")
    carbon = mi("MI_Carbon")
    concrete = mi("MI_Concrete")
    flood = mi("MI_Floodlight")
    netline = mi("MI_NetLine")

    # ---------------------------------------------------------------- floor
    # Single planes => no texture repetition, no banding (flat acrylic look).
    U.spawn_plane("Apron_Acrylic", (0, 0, Z_APRON), (TOTAL_L / 100.0, TOTAL_W / 100.0), apron_m)
    U.spawn_plane("Court_Inner", (0, 0, Z_IN), (2 * HALF_LEN / 100.0, 2 * HALF_DBL / 100.0), inner)
    U.spawn_plane("Ground_Outer", (0, 0, -8.0), (600, 600), concrete)

    # ---------------------------------------------------------------- lines
    # Court measured to the OUTSIDE of the lines (ITF Rule 1). Lengths in
    # metres (spawn_mesh scale unit); thicknesses/positions in the constants.
    HL, HD, HS, SV = 2 * HALF_LEN / 100.0, 2 * HALF_DBL / 100.0, 2 * HALF_SGL / 100.0, SVC / 100.0
    line_y("Line_Baseline_E", HALF_LEN - W_BASE / 2, 0, HD, W_BASE)
    line_y("Line_Baseline_W", -HALF_LEN + W_BASE / 2, 0, HD, W_BASE)
    line_x("Line_Doubles_N", 0, HALF_DBL - W_LINE / 2, HL)
    line_x("Line_Doubles_S", 0, -HALF_DBL + W_LINE / 2, HL)
    line_x("Line_Singles_N", 0, HALF_SGL - W_LINE / 2, HL)
    line_x("Line_Singles_S", 0, -HALF_SGL + W_LINE / 2, HL)
    # Service lines: net-side edge exactly 6.40 m from the net.
    line_y("Line_Service_E", SVC + W_LINE / 2, 0, HS)
    line_y("Line_Service_W", -SVC - W_LINE / 2, 0, HS)
    # Centre service lines: net -> service line, 5 cm wide.
    line_x("Line_CentreSvc_E", SVC / 2, 0, SV)
    line_x("Line_CentreSvc_W", -SVC / 2, 0, SV)
    # Centre marks: 10 cm ticks inside each baseline.
    line_x("Line_CentreMark_E", HALF_LEN - W_BASE - 5.0, 0, 0.10, w=0.05)
    line_x("Line_CentreMark_W", -HALF_LEN + W_BASE + 5.0, 0, 0.10, w=0.05)
    note("lines: 12 ITF markings")

    # -------------------------------------------------------- gameplay proxy
    court_cls = getattr(unreal, "TennisCourt", None)
    if court_cls:
        gc = actors.spawn_actor_from_class(court_cls, unreal.Vector(0, 0, 0),
                                           unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0))
        gc.set_actor_label("GameplayCourt")
        try:
            gc.set_actor_hidden_in_game(True)
            gc.set_is_temporarily_hidden_in_editor(True)
            hidden = 0
            for comp in gc.get_components_by_class(unreal.StaticMeshComponent):
                comp.set_visibility(False, True)
                comp.set_editor_property("cast_shadow", False)
                hidden += 1
            for comp in gc.get_components_by_class(unreal.InstancedStaticMeshComponent):
                comp.set_visibility(False, True)
                hidden += 1
            note("court components hidden: %d" % hidden)
        except Exception as exc:
            note("court hide: %s" % exc)

    # ------------------------------------------------------ net (ITF sagged)
    # Posts 0.914 m outside the doubles court; band sags to 0.914 m at centre.
    U.spawn_mesh("Net_Post_E", "Cylinder", (0, POST_Y, NET_POST_H / 2), (0.04, 0.04, NET_POST_H / 200.0), metal)
    U.spawn_mesh("Net_Post_W", "Cylinder", (0, -POST_Y, NET_POST_H / 2), (0.04, 0.04, NET_POST_H / 200.0), metal)

    # Vertical cords every 40 cm, each following the sag profile.
    cords = 0
    y = -POST_Y
    while y <= POST_Y + 0.01:
        h = net_height_at(abs(y))
        if abs(y) > 1.0:  # centre strap occupies y = 0
            U.spawn_mesh("NetCord_%d" % cords, "Cube", (0, y, h / 2),
                         (0.015, 0.015, h / 100.0), netline, cast_shadow=False)
        cords += 1
        y += 40.0

    # Horizontal mesh rows: 4 rows x per-bay segments following the sag.
    bays = 32
    bay_w = 2 * POST_Y / bays
    rows = 0
    for f in (0.20, 0.45, 0.70, 0.95):
        for b in range(bays):
            y_mid = -POST_Y + bay_w * (b + 0.5)
            z = f * net_height_at(abs(y_mid))
            U.spawn_mesh("NetHoriz_%d_%d" % (rows, b), "Cube", (0, y_mid, z),
                         (0.012, bay_w / 100.0 - 0.01, 0.012), netline, cast_shadow=False)
        rows += 1

    # White top band: per-bay segments stepping along the sag curve.
    for b in range(bays):
        y0 = -POST_Y + bay_w * b
        y_mid = y0 + bay_w / 2
        z = net_height_at(abs(y_mid)) - 3.0
        U.spawn_mesh("NetBand_%d" % b, "Cube", (0, y_mid, z),
                     (0.05, bay_w / 100.0, 0.06), LINE, cast_shadow=False)

    # Centre strap: 91.4 cm at y = 0 (NOT post height - that was the old bug).
    U.spawn_mesh("Net_CentreStrap", "Cube", (0, 0, NET_CTR_H / 2),
                 (0.035, 0.05, NET_CTR_H / 100.0), LINE)
    note("net: %d cords, %d rows, %d band segments" % (cords, rows, bays))

    # ----------------------------------------------------------------- fence
    FX, FY, FH = 2100.0, 1250.0, 400.0
    U.spawn_mesh("Fence_E", "Cube", (FX, 0, FH / 2), (0.05, 2 * FY / 100.0, FH / 100.0), screen)
    U.spawn_mesh("Fence_W", "Cube", (-FX, 0, FH / 2), (0.05, 2 * FY / 100.0, FH / 100.0), screen)
    U.spawn_mesh("Fence_N", "Cube", (0, FY, FH / 2), (2 * FX / 100.0, 0.05, FH / 100.0), screen)
    U.spawn_mesh("Fence_S", "Cube", (0, -FY, FH / 2), (2 * FX / 100.0, 0.05, FH / 100.0), screen)

    n = 0
    for xx in (-FX, FX):
        for yy in range(-1200, 1201, 400):
            n += 1
            U.spawn_mesh("FPost_%d" % n, "Cylinder", (xx, yy, FH / 2), (0.05, 0.05, FH / 200.0), metal)
    for yy in (-FY, FY):
        for xx in range(-2000, 2001, 400):
            n += 1
            U.spawn_mesh("FPost_%d" % n, "Cylinder", (xx, yy, FH / 2), (0.05, 0.05, FH / 200.0), metal)
    note("fence posts: %d" % n)

    # --------------------------------------------------------------- hedges
    # Pushed outside the fence so their shadows never reach the court.
    for yy in (-1900, 1900):
        U.spawn_mesh("Hedge_%.0f" % yy, "Cube", (0, yy, 60), (40.0, 1.1, 1.2), hedge)

    # ---------------------------------------------------------------- trees
    for i, yy in enumerate((-1600, -1050, -500, 500, 1050, 1600)):
        xx = 2750 + (i % 2) * 200
        U.spawn_mesh("TreeTrunk_%d" % i, "Cylinder", (xx, yy, 180), (0.28, 0.28, 3.6), carbon)
        U.spawn_mesh("TreeCrown_%d" % i, "Cone", (xx, yy, 620), (2.4, 2.4, 6.4), tree)

    # ------------------------------------------------- imported sprite assets
    # Rocks from the user's sprite pack (spawned only if import succeeded).
    rock = unreal.EditorAssetLibrary.load_asset("/Game/Sprites/SM_Rock")
    if rock:
        for i, (rx, ry) in enumerate(((-1900, -900), (1950, 700), (700, 1100))):
            actor = actors.spawn_actor_from_class(
                unreal.StaticMeshActor, unreal.Vector(rx, ry, 0),
                unreal.Rotator(pitch=0.0, yaw=30.0 * i, roll=0.0))
            actor.set_actor_label("SpriteRock_%d" % i)
            smc = actor.static_mesh_component
            smc.set_mobility(unreal.ComponentMobility.STATIC)
            smc.set_static_mesh(rock)
            actor.set_actor_scale3d(unreal.Vector(1.5, 1.5, 1.5))
            smc.set_collision_profile_name("NoCollision")
        note("sprite rocks: 3")

    # ------------------------------------------------------------- furniture
    for i, yy in enumerate((-430, 430)):
        U.spawn_mesh("Bench_%d" % i, "Cube", (-1430, yy, 24), (2.2, 0.55, 0.45), carbon)
        U.spawn_mesh("BenchBack_%d" % i, "Cube", (-1530, yy, 52), (0.08, 0.55, 0.55), carbon)

    U.spawn_mesh("Umpire_Base", "Cube", (0, -860, 35), (1.0, 1.0, 0.7), concrete)
    U.spawn_mesh("Umpire_Seat", "Cube", (0, -860, 95), (0.55, 0.55, 0.5), carbon)
    U.spawn_mesh("Umpire_Back", "Cube", (0, -885, 130), (0.55, 0.1, 0.75), carbon)

    # ------------------------------------------------------------ floodlights
    for x in (-2500, 2500):
        for yy in (-1550, 1550):
            U.spawn_mesh("FloodPole_%.0f_%.0f" % (x, yy), "Cylinder", (x, yy, 600),
                         (0.28, 0.28, 12.0), metal)
            U.spawn_mesh("FloodHead_%.0f_%.0f" % (x, yy), "Cube", (x, yy, 1230),
                         (0.35, 2.2, 0.9), metal)
            U.spawn_mesh("FloodPanel_%.0f_%.0f" % (x, yy), "Cube", (x, yy, 1230),
                         (0.1, 2.0, 0.7), flood, cast_shadow=False)

    # --------------------------------------------------------------- lighting
    sun = actors.spawn_actor_from_class(
        unreal.DirectionalLight, unreal.Vector(0, 0, 1500),
        unreal.Rotator(pitch=-55.0, yaw=155.0, roll=0.0))
    sun.set_actor_label("Sun")
    sc = sun.get_component_by_class(unreal.DirectionalLightComponent)
    sc.set_mobility(unreal.ComponentMobility.MOVABLE)
    for k, v in {
        "intensity": 10.0,
        "use_temperature": True,
        "temperature": 5400.0,
        "atmosphere_sun_light": True,
        "atmosphere_sun_light_index": 0,
        "light_source_angle": 0.9,
        "cast_shadows": True,
    }.items():
        try:
            sc.set_editor_property(k, v)
        except Exception as exc:
            note("sun %s: %s" % (k, exc))

    sa = actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0),
                                       unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0))
    sa.set_actor_label("SkyAtmosphere")
    cl = actors.spawn_actor_from_class(unreal.VolumetricCloud, unreal.Vector(0, 0, 3000),
                                       unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0))
    cl.set_actor_label("Clouds")
    sl = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 900),
                                       unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0))
    sl.set_actor_label("SkyLight")
    skc = sl.get_component_by_class(unreal.SkyLightComponent)
    skc.set_mobility(unreal.ComponentMobility.MOVABLE)
    for k, v in {"real_time_capture": True, "intensity": 2.0,
                 "source_type": unreal.SkyLightSourceType.SLS_CAPTURED_SCENE}.items():
        try:
            skc.set_editor_property(k, v)
        except Exception as exc:
            note("sky %s: %s" % (k, exc))

    fog = actors.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0),
                                        unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0))
    fog.set_actor_label("Fog")
    fc = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    for k, v in {"fog_density": 0.003, "fog_height_falloff": 0.1,
                 "enable_volumetric_fog": True,
                 "volumetric_fog_extinction_scale": 0.25}.items():
        try:
            fc.set_editor_property(k, v)
        except Exception as exc:
            note("fog %s: %s" % (k, exc))

    # ----------------------------------------------------------------- cameras
    loc = unreal.Vector(-1600.0, -700.0, 300.0)
    rot = look_at(loc, unreal.Vector(150.0, 40.0, 5.0))
    cam = actors.spawn_actor_from_class(unreal.CineCameraActor, loc, rot)
    cam.set_actor_label("PhotoCam")
    cc = cam.get_component_by_class(unreal.CineCameraComponent)
    try:
        cc.set_editor_property("current_focal_length", 30.0)
        cc.set_editor_property("current_aperture", 5.6)
        # No auto_activate_for_player: the pawn spring arm owns the -game view.
    except Exception as exc:
        note("cam %s" % exc)

    ps = actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-1308.0, -100.0, 90.0),
                                       unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0))
    ps.set_actor_label("PlayerStart")

    # ------------------------------------------------------------ postprocess
    ppv = actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0),
                                        unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0))
    ppv.set_actor_label("PostProcess")
    ppv.set_editor_property("unbound", True)
    st = ppv.get_editor_property("settings")

    def setp(key, value, override_key=None):
        try:
            if override_key:
                st.set_editor_property(override_key, True)
            st.set_editor_property(key, value)
        except Exception as exc:
            note("pp %s: %s" % (key, exc))

    setp("bloom_intensity", 0.45, "override_bloom_intensity")
    setp("vignette_intensity", 0.3, "override_vignette_intensity")
    setp("scene_fringe_intensity", 0.15, "override_scene_fringe_intensity")
    setp("film_grain_intensity", 0.04, "override_film_grain_intensity")
    setp("motion_blur_amount", 0.3, "override_motion_blur_amount")
    setp("auto_exposure_method", unreal.AutoExposureMethod.AEM_HISTOGRAM,
         "override_auto_exposure_method")
    setp("auto_exposure_bias", -0.5, "override_auto_exposure_bias")
    setp("auto_exposure_min_brightness", 0.05, "override_auto_exposure_min_brightness")
    setp("auto_exposure_max_brightness", 1.5, "override_auto_exposure_max_brightness")
    setp("color_saturation", unreal.Vector4(0.98, 0.98, 0.98, 1.0), "override_color_saturation")
    setp("color_contrast", unreal.Vector4(1.12, 1.12, 1.12, 1.0), "override_color_contrast")
    ppv.set_editor_property("settings", st)

    les.save_current_level()
    note("saved, actors=%d" % len(actors.get_all_level_actors()))


try:
    build()
except Exception:
    note("EXC\n" + traceback.format_exc())

with open(OUT, "w", encoding="utf-8") as f:
    f.write("\n".join(RES))
unreal.log("[LEVELV4] " + RES[-1][:160])
