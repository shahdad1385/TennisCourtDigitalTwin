"""ClayTennis level v2: clean, photo-driven layout."""
import unreal
import os
import sys
import traceback
import math

sys.path.insert(0, r"C:\Projects\ClayTennis\Tools")
import ue_common as U

OUT = r"C:\Projects\ClayTennis\RawAssets\build_level_v2_result.txt"
RES = []


def note(s):
    RES.append(str(s))


def mi(name):
    return unreal.EditorAssetLibrary.load_asset("/Game/Materials/" + name)


def look_at(src, dst):
    d = dst - src
    yaw = math.degrees(math.atan2(d.y, d.x))
    pitch = math.degrees(math.atan2(d.z, math.sqrt(d.x * d.x + d.y * d.y)))
    return unreal.Rotator(pitch=pitch, yaw=yaw, roll=0.0)


def build():
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    les.new_level("/Game/Maps/TennisCourt")

    clay = mi("MI_ClayCourt")
    apron = mi("MI_ClayApron")
    line = mi("MI_CourtLine")
    net = mi("MI_Net")
    metal = mi("MI_Metal")
    screen = mi("MI_Windscreen")
    hedge = mi("MI_Hedge")
    tree = mi("MI_Tree")
    carbon = mi("MI_Carbon")
    concrete = mi("MI_Concrete")
    flood = mi("MI_Floodlight")

    # --------------------------------------------------------------- ground
    U.spawn_plane("Ground_Outer", (0, 0, -8.0), (500, 500), apron)
    U.spawn_plane("Ground_Apron", (0, 0, -0.5), (40.0, 24.0), apron)
    U.spawn_plane("Court_Surface", (0, 0, 0.0), (23.77, 10.97), clay)

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
                try:
                    comp.set_visibility(False, True)
                    comp.set_editor_property("cast_shadow", False)
                    hidden += 1
                except Exception:
                    pass
            for comp in gc.get_components_by_class(unreal.InstancedStaticMeshComponent):
                try:
                    comp.set_visibility(False, True)
                    hidden += 1
                except Exception:
                    pass
            note("court components hidden: %d" % hidden)
        except Exception as exc:
            note("court hide: %s" % exc)
        note("gameplay court added")

    # ---------------------------------------------------------------- lines
    L, W, SW, SL, T, Z = 1188.5, 548.5, 411.5, 640.0, 0.05, 0.30

    def lx(label, x, y, length):
        U.spawn_mesh(label, "Cube", (x, y, Z), (T, length, 0.003), line)

    def ly(label, x, y, length):
        U.spawn_mesh(label, "Cube", (x, y, Z), (length, T, 0.003), line)

    ly("Line_Baseline_N", L, 0, 10.97)
    ly("Line_Baseline_S", -L, 0, 10.97)
    lx("Line_Doubles_E", 0, W, 23.77)
    lx("Line_Doubles_W", 0, -W, 23.77)
    lx("Line_Singles_E", 0, SW, 23.77)
    lx("Line_Singles_W", 0, -SW, 23.77)
    ly("Line_Service_N", SL, 0, 8.23)
    ly("Line_Service_S", -SL, 0, 8.23)
    lx("Line_CentreService", 0, 0, 12.8)
    ly("Line_CentreMark_N", L, 0, 0.10)
    ly("Line_CentreMark_S", -L, 0, 0.10)

    logo = mi("MI_CourtLogo")
    if logo:
        U.spawn_plane("Court_Logo", (0, 0, 0.4), (3.4, 3.4), logo)

    # ------------------------------------------------------------------ net
    U.spawn_mesh("Net_Mesh", "Plane", (0, 0, 53.5), (12.8, 1.07, 1), net,
                 rotation=(0, 90, 90))
    U.spawn_mesh("Net_Post_E", "Cylinder", (0, 640, 53.5), (0.07, 0.07, 1.07), metal)
    U.spawn_mesh("Net_Post_W", "Cylinder", (0, -640, 53.5), (0.07, 0.07, 1.07), metal)
    U.spawn_mesh("Net_TopBand", "Cube", (0, 0, 104.0), (0.02, 12.8, 0.06), line)
    U.spawn_mesh("Net_CentreStrap", "Cube", (0, 0, 53.5), (0.03, 0.05, 1.07), line)

    # ----------------------------------------------------------------- fence
    FX, FY, FH = 2300.0, 1350.0, 400.0
    U.spawn_mesh("Fence_N", "Cube", (FX, 0, FH / 2), (0.05, 27.0, 4.0), screen)
    U.spawn_mesh("Fence_S", "Cube", (-FX, 0, FH / 2), (0.05, 27.0, 4.0), screen)
    U.spawn_mesh("Fence_E", "Cube", (0, FY, FH / 2), (46.0, 0.05, 4.0), screen)
    U.spawn_mesh("Fence_W", "Cube", (0, -FY, FH / 2), (46.0, 0.05, 4.0), screen)

    n = 0
    for x in (-FX, FX):
        for y in range(-1200, 1201, 400):
            n += 1
            U.spawn_mesh("FPost_%d" % n, "Cylinder", (x, y, FH / 2), (0.06, 0.06, 4.0), metal)
    for y in (-FY, FY):
        for x in range(-2000, 2001, 400):
            n += 1
            U.spawn_mesh("FPost_%d" % n, "Cylinder", (x, y, FH / 2), (0.06, 0.06, 4.0), metal)
    for y in (-FY, FY):
        U.spawn_mesh("FRail_%.0f" % y, "Cylinder", (0, y, 400), (0.05, 46.0, 0.05), metal,
                     rotation=(0, 90, 0))
    for x in (-FX, FX):
        U.spawn_mesh("FRail_%.0f" % x, "Cylinder", (x, 0, 400), (0.05, 27.0, 0.05), metal,
                     rotation=(90, 0, 0))

    # --------------------------------------------------------------- hedges
    for y in (-1520, 1520):
        U.spawn_mesh("Hedge_%.0f" % y, "Cube", (0, y, 60), (40.0, 1.1, 1.2), hedge)

    # ---------------------------------------------------------------- trees
    for i, y in enumerate((-1600, -1050, -500, 500, 1050, 1600)):
        x = 2700 + (i % 2) * 220
        U.spawn_mesh("TreeTrunk_%d" % i, "Cylinder", (x, y, 180), (0.3, 0.3, 3.6), carbon)
        U.spawn_mesh("TreeCrown_%d" % i, "Cone", (x, y, 560), (2.6, 2.6, 5.5), tree)

    # ------------------------------------------------------------- furniture
    for i, y in enumerate((-430, 430)):
        U.spawn_mesh("Bench_%d" % i, "Cube", (-1430, y, 24), (2.2, 0.55, 0.45), carbon)
        U.spawn_mesh("BenchBack_%d" % i, "Cube", (-1530, y, 52), (0.08, 0.55, 0.55), carbon)
        for dx in (-90, 90):
            U.spawn_mesh("BenchLeg_%d_%d" % (i, dx), "Cube", (-1430 + dx, y, 11),
                         (0.12, 0.5, 0.22), metal)

    U.spawn_mesh("Umpire_Base", "Cube", (0, -860, 35), (1.0, 1.0, 0.7), concrete)
    U.spawn_mesh("Umpire_Seat", "Cube", (0, -860, 95), (0.55, 0.55, 0.5), carbon)
    U.spawn_mesh("Umpire_Back", "Cube", (0, -885, 130), (0.55, 0.1, 0.75), carbon)

    U.spawn_mesh("BallCart", "Cube", (-1300, 520, 35), (0.9, 0.6, 0.7), metal)
    for i, (dx, dy) in enumerate(((-0.25, -0.2), (0.25, -0.2), (-0.25, 0.2), (0.25, 0.2))):
        U.spawn_mesh("Ball_%d" % i, "Sphere", (-1300 + dx * 100, 520 + dy * 100, 78),
                     (0.067, 0.067, 0.067), mi("MI_TennisBall"))

    # ------------------------------------------------------------ floodlights
    for x in (-2500, 2500):
        for y in (-1550, 1550):
            U.spawn_mesh("FloodPole_%.0f_%.0f" % (x, y), "Cylinder", (x, y, 600),
                         (0.3, 0.3, 12.0), metal)
            U.spawn_mesh("FloodHead_%.0f_%.0f" % (x, y), "Cube", (x, y, 1230),
                         (0.35, 2.2, 0.9), metal)
            U.spawn_mesh("FloodPanel_%.0f_%.0f" % (x, y), "Cube", (x, y, 1230),
                         (0.12, 2.0, 0.7), flood, cast_shadow=False)

    # --------------------------------------------------------------- lighting
    sun = actors.spawn_actor_from_class(
        unreal.DirectionalLight, unreal.Vector(0, 0, 1500),
        unreal.Rotator(pitch=-22.0, yaw=40.0, roll=0.0))
    sun.set_actor_label("Sun")
    sc = sun.get_component_by_class(unreal.DirectionalLightComponent)
    sc.set_mobility(unreal.ComponentMobility.MOVABLE)
    for k, v in {
        "intensity": 10.0,
        "use_temperature": True,
        "temperature": 4200.0,
        "atmosphere_sun_light": True,
        "atmosphere_sun_light_index": 0,
        "light_source_angle": 0.8,
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
    for k, v in {"real_time_capture": True, "intensity": 1.0,
                 "source_type": unreal.SkyLightSourceType.SLS_CAPTURED_SCENE}.items():
        try:
            skc.set_editor_property(k, v)
        except Exception as exc:
            note("sky %s: %s" % (k, exc))

    fog = actors.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0),
                                        unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0))
    fog.set_actor_label("Fog")
    fc = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    for k, v in {"fog_density": 0.004, "fog_height_falloff": 0.12,
                 "enable_volumetric_fog": True,
                 "volumetric_fog_extinction_scale": 0.3}.items():
        try:
            fc.set_editor_property(k, v)
        except Exception as exc:
            note("fog %s: %s" % (k, exc))

    # --------------------------------------------------------------- camera
    loc = unreal.Vector(-1600.0, -700.0, 300.0)
    tgt = unreal.Vector(150.0, 40.0, 5.0)
    rot = look_at(loc, tgt)
    cam = actors.spawn_actor_from_class(unreal.CineCameraActor, loc, rot)
    cam.set_actor_label("PhotoCam")
    cc = cam.get_component_by_class(unreal.CineCameraComponent)
    try:
        cc.set_editor_property("current_focal_length", 30.0)
        cc.set_editor_property("current_aperture", 5.6)
        cam.set_editor_property("auto_activate_for_player", unreal.AutoReceiveInput.PLAYER0)
    except Exception as exc:
        note("cam %s" % exc)

    ps = actors.spawn_actor_from_class(unreal.PlayerStart, loc, rot)
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

    setp("bloom_intensity", 0.55, "override_bloom_intensity")
    setp("vignette_intensity", 0.32, "override_vignette_intensity")
    setp("scene_fringe_intensity", 0.2, "override_scene_fringe_intensity")
    setp("film_grain_intensity", 0.05, "override_film_grain_intensity")
    setp("motion_blur_amount", 0.35, "override_motion_blur_amount")
    setp("auto_exposure_method", unreal.AutoExposureMethod.AEM_HISTOGRAM,
         "override_auto_exposure_method")
    setp("auto_exposure_bias", -0.8, "override_auto_exposure_bias")
    setp("auto_exposure_min_brightness", 0.03, "override_auto_exposure_min_brightness")
    setp("auto_exposure_max_brightness", 2.0, "override_auto_exposure_max_brightness")
    setp("color_saturation", unreal.Vector4(1.08, 1.08, 1.08, 1.0), "override_color_saturation")
    setp("color_contrast", unreal.Vector4(1.1, 1.1, 1.1, 1.0), "override_color_contrast")
    setp("color_gain", unreal.Vector4(1.04, 1.0, 0.96, 1.0), "override_color_gain")
    ppv.set_editor_property("settings", st)

    les.save_current_level()
    note("saved, actors=%d" % len(actors.get_all_level_actors()))


try:
    build()
except Exception:
    note("EXC\n" + traceback.format_exc())

with open(OUT, "w", encoding="utf-8") as f:
    f.write("\n".join(RES))
unreal.log("[LEVELV2] " + RES[-1][:160])
