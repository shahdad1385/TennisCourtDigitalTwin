"""Build the clay tennis court level with lighting, set dressing and post FX."""
import unreal
import os
import sys
import traceback

sys.path.insert(0, r"C:\Projects\ClayTennis\Tools")
import ue_common as U

OUT = r"C:\Projects\ClayTennis\RawAssets\build_level_result.txt"
RESULT = []


def note(msg):
    RESULT.append(str(msg))


def load(path):
    return unreal.EditorAssetLibrary.load_asset(path)


def mi(name):
    return load("/Game/Materials/" + name)


# --------------------------------------------------------------------- level
def build():
    note("start")
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    map_path = "/Game/Maps/TennisCourt"
    if unreal.EditorAssetLibrary.does_asset_exist(map_path):
        # fresh empty level, the old one is rebuilt from scratch
        pass
    les.new_level(map_path)
    note("new_level ok")

    clay = mi("MI_ClayCourt")
    ground = mi("MI_ClayCourt")
    line = mi("MI_CourtLine")
    net = mi("MI_Net")
    metal = mi("MI_Metal")
    concrete = mi("MI_Concrete")
    screen = mi("MI_Windscreen")
    crowd = mi("MI_Crowd")
    flood = mi("MI_Floodlight")
    carbon = mi("MI_Carbon")
    logo = None
    try:
        logo = mi("MI_CourtLogo")
    except Exception:
        pass

    # ---------------------------------------------------------------- surfaces
    # main runoff apron (clay), 36.6 x 18.3 m, slightly below the playing surface
    U.spawn_plane("Ground_Apron", (0, 0, -1.0), (44.0, 26.0), clay)
    # outer ground - huge, darker clay to fade into the distance
    U.spawn_plane("Ground_Outer", (0, 0, -6.0), (500, 500), ground)

    # playing court surface 23.77 x 10.97 - 1 cm above the apron
    U.spawn_plane("Court_Surface", (0, 0, 0.0), (23.77, 10.97), clay)

    # invisible gameplay court (C++ ATennisCourt) so the game mode reuses it
    court_cls = getattr(unreal, "TennisCourt", None)
    if court_cls:
        gc = actors.spawn_actor_from_class(court_cls, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
        gc.set_actor_label("GameplayCourt")
        try:
            gc.set_actor_hidden_in_game(True)
            gc.set_is_temporarily_hidden_in_editor(True)
        except Exception as exc:
            note("court hide: %s" % exc)
        note("gameplay court added")
    else:
        note("WARNING: ATennisCourt class not found")

    L = 1188.5   # half length (cm)
    W = 548.5    # half doubles width
    SW = 411.5   # half singles width
    SL = 640.0   # service line distance from net
    t = 0.06     # line thickness in metres
    z = 0.35     # line height in cm

    def line_x(label, x, y, length):
        U.spawn_mesh(label, "Cube", (x, y, z), (t, length, 0.004), line)

    def line_y(label, x, y, length):
        U.spawn_mesh(label, "Cube", (x, y, z), (length, t, 0.004), line)

    line_y("Line_Baseline_North", L, 0, 10.97)
    line_y("Line_Baseline_South", -L, 0, 10.97)
    line_x("Line_Sideline_Doubles_East", 0, W, 23.77)
    line_x("Line_Sideline_Doubles_West", 0, -W, 23.77)
    line_x("Line_Sideline_Singles_East", 0, SW, 23.77)
    line_x("Line_Sideline_Singles_West", 0, -SW, 23.77)
    line_y("Line_Service_North", SL, 0, 8.23)
    line_y("Line_Service_South", -SL, 0, 8.23)
    line_x("Line_CenterService", 0, 0, 12.8)
    line_y("Line_CenterMark_North", L, 0, 0.10)
    line_y("Line_CenterMark_South", -L, 0, 0.10)

    if logo:
        # subtle centre-court logo, transparent decal-ish plane
        U.spawn_plane("Court_Logo", (0, 0, 0.45), (3.2, 3.2), logo)

    # -------------------------------------------------------------------- net
    U.spawn_mesh("Net_Mesh", "Plane", (0, 0, 55.0), (12.8, 1.07, 1), net,
                 rotation=(0, 90, 90), cast_shadow=True)
    U.spawn_mesh("Net_Post_East", "Cylinder", (0, 639.0, 53.5), (0.07, 0.07, 1.07), metal)
    U.spawn_mesh("Net_Post_West", "Cylinder", (0, -639.0, 53.5), (0.07, 0.07, 1.07), metal)
    U.spawn_mesh("Net_Band", "Cube", (0, 0, 103.0), (0.03, 12.8, 0.06), concrete)

    # --------------------------------------------------------------- umpire
    U.spawn_mesh("Umpire_Base", "Cube", (0, -760, 40), (1.3, 1.3, 0.8), concrete)
    U.spawn_mesh("Umpire_Seat", "Cube", (0, -760, 105), (0.6, 0.6, 0.5), carbon)
    U.spawn_mesh("Umpire_Back", "Cube", (0, -790, 150), (0.6, 0.12, 0.9), carbon)
    U.spawn_mesh("Umpire_Umbrella", "Cone", (0, -760, 245), (2.6, 2.6, 0.7), concrete)

    # -------------------------------------------------------------- benches
    for i, y in enumerate((-320, 320)):
        U.spawn_mesh("Bench_%d" % i, "Cube", (-1500, y, 22), (2.4, 0.55, 0.44), carbon)
        U.spawn_mesh("Bench_Back_%d" % i, "Cube", (-1620, y, 48), (0.08, 0.55, 0.5), carbon)
    U.spawn_mesh("Cooler", "Cube", (-1430, 470, 30), (0.6, 0.5, 0.6), concrete)

    # --------------------------------------------------------------- fences
    fx = 2300.0
    fy = 1350.0
    for i, x in enumerate((-fx, fx)):
        U.spawn_mesh("Fence_End_%d" % i, "Cube", (x, 0, 200), (0.04, 27.0, 4.0), screen)
    for i, y in enumerate((-fy, fy)):
        U.spawn_mesh("Fence_Side_%d" % i, "Cube", (0, y, 200), (46.0, 0.04, 4.0), screen)
    # fence posts
    for x in (-fx, fx):
        for y in (-fy, -475, 0, 475, fy):
            U.spawn_mesh("Post_%.0f_%.0f" % (x, y), "Cylinder", (x, y, 200), (0.08, 0.08, 4.0), metal)

    # --------------------------------------------------------------- stands
    # two low spectator banks along the long sides, crowd texture on risers
    for side in (-1, 1):
        for row in range(5):
            y = side * (1520 + row * 90)
            U.spawn_mesh("Stand_%d_%d" % (side, row), "Cube",
                         (0, y, 25 + row * 45), (34.0, 0.9, 0.45 + row * 0.9), concrete)
            U.spawn_mesh("Crowd_%d_%d" % (side, row), "Plane",
                         (0, y - side * 46, 100 + row * 45), (34.0, 2.4, 1), crowd,
                         rotation=(0, 0, 90 if side < 0 else -90),
                         cast_shadow=False)
        # back wall of the stand
        U.spawn_mesh("StandWall_%d" % side, "Cube", (0, side * 2000, 380), (34.0, 0.3, 7.6), concrete)

    # --------------------------------------------------------- floodlights
    for x in (-2600, 2600):
        for y in (-1500, 1500):
            U.spawn_mesh("FloodPole_%.0f_%.0f" % (x, y), "Cylinder", (x, y, 700), (0.35, 0.35, 14.0), metal)
            U.spawn_mesh("FloodHead_%.0f_%.0f" % (x, y), "Cube", (x, y, 1430), (0.5, 3.2, 1.6), metal)
            U.spawn_mesh("FloodPanel_%.0f_%.0f" % (x, y), "Cube", (x, y, 1352), (0.18, 2.9, 1.3), flood,
                         cast_shadow=False)

    # ------------------------------------------------------------ scoreboard
    U.spawn_mesh("Scoreboard_Frame", "Cube", (0, -1250, 190), (5.0, 0.3, 2.8), carbon)
    U.spawn_mesh("Scoreboard_Screen", "Cube", (0, -1232, 190), (4.6, 0.06, 2.4), flood)

    # ------------------------------------------------------------- lighting
    sun = actors.spawn_actor_from_class(unreal.DirectionalLight,
                                        unreal.Vector(0, 0, 1200),
                                        unreal.Rotator(pitch=-10.0, yaw=200.0, roll=0.0))
    sun.set_actor_label("Sun")
    sc = sun.get_component_by_class(unreal.DirectionalLightComponent)
    sc.set_mobility(unreal.ComponentMobility.MOVABLE)
    for k, v in {
        "intensity": 9.0,
        "use_temperature": True,
        "temperature": 3600.0,
        "atmosphere_sun_light": True,
        "atmosphere_sun_light_index": 0,
        "light_source_angle": 0.6,
        "cast_shadows": True,
        "dynamic_shadow_distance_movable_light": 30000.0,
    }.items():
        try:
            sc.set_editor_property(k, v)
        except Exception as exc:
            note("sun prop %s: %s" % (k, exc))

    sky_atmo = actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0),
                                             unreal.Rotator(0, 0, 0))
    sky_atmo.set_actor_label("SkyAtmosphere")

    clouds = actors.spawn_actor_from_class(unreal.VolumetricCloud, unreal.Vector(0, 0, 3000),
                                          unreal.Rotator(0, 0, 0))
    clouds.set_actor_label("VolumetricClouds")

    skylight = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 800),
                                             unreal.Rotator(0, 0, 0))
    skylight.set_actor_label("SkyLight")
    skc = skylight.get_component_by_class(unreal.SkyLightComponent)
    skc.set_mobility(unreal.ComponentMobility.MOVABLE)
    for k, v in {
        "real_time_capture": True,
        "intensity": 1.0,
        "source_type": unreal.SkyLightSourceType.SLS_CAPTURED_SCENE,
    }.items():
        try:
            skc.set_editor_property(k, v)
        except Exception as exc:
            note("sky prop %s: %s" % (k, exc))
    try:
        skc.recapture_sky()
    except Exception as exc:
        note("recapture: %s" % exc)

    fog = actors.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0),
                                        unreal.Rotator(0, 0, 0))
    fog.set_actor_label("HeightFog")
    fc = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    for k, v in {
        "fog_density": 0.006,
        "fog_height_falloff": 0.15,
        "enable_volumetric_fog": True,
        "volumetric_fog_scattering_distribution": 0.55,
        "volumetric_fog_extinction_scale": 0.35,
    }.items():
        try:
            fc.set_editor_property(k, v)
        except Exception as exc:
            note("fog prop %s: %s" % (k, exc))

    # ----------------------------------------------------------------- camera
    cam = actors.spawn_actor_from_class(unreal.CineCameraActor,
                                        unreal.Vector(-1500, -1000, 165),
                                        unreal.Rotator(pitch=-4.0, yaw=-33.0, roll=0.0))
    cam.set_actor_label("CinematicCamera")
    cc = cam.get_component_by_class(unreal.CineCameraComponent)
    cc.set_editor_property("current_focal_length", 32.0)
    cc.set_editor_property("current_aperture", 4.0)
    try:
        cc.set_editor_property("focus_settings",
                               unreal.CameraFocusSettings())
    except Exception:
        pass

    try:
        cam.set_editor_property("auto_activate_for_player", unreal.AutoReceiveInput.PLAYER0)
        note("camera auto activate set")
    except Exception as exc:
        note("camera auto activate: %s" % exc)

    # level player start (used by the standalone game view)
    ps = actors.spawn_actor_from_class(unreal.PlayerStart,
                                       unreal.Vector(-1240, -130, 110),
                                       unreal.Rotator(pitch=-6.0, yaw=3.0, roll=0.0))
    ps.set_actor_label("PlayerStart")

    # ------------------------------------------------------------ postprocess
    ppv = actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0),
                                        unreal.Rotator(0, 0, 0))
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

    setp("bloom_intensity", 0.75, "override_bloom_intensity")
    setp("vignette_intensity", 0.38, "override_vignette_intensity")
    setp("scene_fringe_intensity", 0.28, "override_scene_fringe_intensity")
    setp("film_grain_intensity", 0.06, "override_film_grain_intensity")
    setp("motion_blur_amount", 0.55, "override_motion_blur_amount")
    setp("auto_exposure_method", unreal.AutoExposureMethod.AEM_HISTOGRAM,
         "override_auto_exposure_method")
    setp("auto_exposure_min_brightness", 0.05, "override_auto_exposure_min_brightness")
    setp("auto_exposure_max_brightness", 3.0, "override_auto_exposure_max_brightness")
    setp("auto_exposure_bias", 0.0, "override_auto_exposure_bias")
    setp("color_saturation", unreal.Vector4(1.03, 1.03, 1.03, 1.0), "override_color_saturation")
    setp("color_contrast", unreal.Vector4(1.06, 1.06, 1.06, 1.0), "override_color_contrast")
    setp("color_gain", unreal.Vector4(1.02, 1.0, 0.97, 1.0), "override_color_gain")
    setp("dynamic_global_illumination_method", unreal.DynamicGlobalIlluminationMethod.LUMEN,
         "override_dynamic_global_illumination_method")
    setp("reflection_method", unreal.ReflectionMethod.LUMEN, "override_reflection_method")

    ppv.set_editor_property("settings", st)

    # -------------------------------------------------------------- save/view
    les.save_current_level()
    note("level saved")

    try:
        unreal.EditorLevelLibrary.set_level_viewport_camera_info(
            unreal.Vector(-1850, 120, 900), unreal.Rotator(-12, 8, 0))
        note("viewport set")
    except Exception as exc:
        note("viewport: %s" % exc)

    note("actors: %d" % len(actors.get_all_level_actors()))


try:
    build()
except Exception:
    note("EXCEPTION\n" + traceback.format_exc())

with open(OUT, "w", encoding="utf-8") as f:
    f.write("\n".join(RESULT))
unreal.log("[LEVEL] " + RESULT[-1][:200])

# let the viewport finish rendering, grab a reference shot, then close
_cmdline = unreal.SystemLibrary.get_command_line().lower()
if "pythonscript" in _cmdline:
    unreal.SystemLibrary.quit_editor()
else:
    _ticks = {"n": 0}

    def _tick(delta):
        _ticks["n"] += 1
        n = _ticks["n"]
        if n == 260:
            try:
                unreal.AutomationLibrary.take_high_res_screenshot(1920, 1080, "court_ref")
                note("screenshot requested")
            except Exception as exc:
                note("screenshot failed: %s" % exc)
        if n == 420:
            unreal.SystemLibrary.quit_editor()

    unreal.register_slate_post_tick_callback(_tick)
