"""Scene props for the ClayTennis project: net, perimeter fence, club surround.

Imported and executed inside the Unreal 5.5 editor by the build pipeline
(do not run directly; it only imports `unreal` inside the editor).

Conventions (matching ue_common.py / build_level.py):
  - locations in cm, mesh scale in metres (BasicShapes sizes)
  - UE Z-up, court centred at the origin, length along X, width along Y
  - unreal.Rotator(pitch, yaw, roll) positional order, same as build_level.py
  - BasicShapes dims: Plane 100x100 cm, Cube 100^3 cm, Cylinder r50 x h200 cm,
    Cone base r50 x h100 cm, Sphere r50 cm

Only /Engine/BasicShapes/{Cube,Plane,Cylinder,Cone,Sphere} are used.
Every function is defensive: a missing (None) material skips the mesh instead
of crashing, and every editor_property set is wrapped so a renamed property
cannot abort the whole build.
"""
import math
import traceback

import unreal

import ue_common as U

# Material instances loaded by build_all(); each value may be None if missing.
MATERIAL_NAMES = (
    "MI_Net", "MI_Metal", "MI_CourtLine", "MI_Windscreen", "MI_Floodlight",
    "MI_Hedge", "MI_Tree", "MI_Carbon", "MI_Concrete",
)

NET_HALF_LENGTH = 640.0     # cm, net spans y = -640 .. +640 (1280 cm)
NET_HEIGHT = 107.0          # cm
GATE_WIDTH = 120.0          # cm, gate gap in the middle of the -X fence side
HEDGE_OUT_Y = 1020.0        # cm, hedge line outside the fence (950 + 70)


# ---------------------------------------------------------------- small helpers

def _load_material(name):
    path = "/Game/Materials/" + name
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        U.log("scene_props: material missing " + path)
        return None
    return unreal.EditorAssetLibrary.load_asset(path)


def _mat(materials, *names):
    """First existing material from `names`, else None."""
    for name in names:
        m = materials.get(name)
        if m:
            return m
    return None


def _spawn(label, shape, loc, scale, mat, rot=(0, 0, 0), shadow=True):
    if not mat:
        U.log("scene_props: skip %s (material missing)" % label)
        return None
    return U.spawn_mesh(label, shape, loc, scale, mat, rotation=rot,
                        cast_shadow=shadow)


def _plane(label, loc, size_m, mat, rot=(0, 0, 0)):
    if not mat:
        U.log("scene_props: skip %s (material missing)" % label)
        return None
    return U.spawn_plane(label, loc, size_m, mat, rotation=rot)


def _ensure_two_sided(mi, label):
    """M_Windscreen is created single-sided in build_assets.py, but the fence
    windscreen planes must read from both sides. Enable two_sided on the
    parent material if possible; defensive no-op on failure."""
    if not mi:
        return
    try:
        parent = mi.get_editor_property("parent")
        if parent and not parent.get_editor_property("two_sided"):
            parent.set_editor_property("two_sided", True)
            U.compile_and_save(parent)
            U.log("scene_props: enabled two_sided on %s parent" % label)
    except Exception as exc:  # noqa: BLE001
        U.log("scene_props: two_sided %s failed: %s" % (label, exc))


def _find_actor(label=None, cls=None):
    """First level actor matching label and/or class, else None."""
    for actor in U.actors().get_all_level_actors():
        try:
            if label is not None and actor.get_actor_label() != label:
                continue
            if cls is not None and not actor.is_a(cls):
                continue
            return actor
        except Exception:  # noqa: BLE001
            continue
    return None


def _set_props(comp, props):
    for key, value in props.items():
        try:
            comp.set_editor_property(key, value)
        except Exception as exc:  # noqa: BLE001
            U.log("scene_props: prop %s failed: %s" % (key, exc))


def _look_at_rotator(from_loc, to_loc):
    """Rotator (pitch, yaw, roll) pointing from from_loc at to_loc."""
    delta = to_loc - from_loc
    try:
        return delta.rotation()
    except Exception:  # noqa: BLE001
        dx, dy, dz = delta.x, delta.y, delta.z
        length = max(1e-6, math.sqrt(dx * dx + dy * dy + dz * dz))
        pitch = math.degrees(math.asin(max(-1.0, min(1.0, dz / length))))
        yaw = math.degrees(math.atan2(dy, dx))
        return unreal.Rotator(pitch, yaw, 0.0)


# ------------------------------------------------------------------------- 1

def build_net(materials):
    """Real tennis net at x=0, plus posts, white top band and centre strap.

    Net plane: 1280 x 107 cm, standing vertical, normal facing +X, bottom edge
    at z=0 and top at z=107. Uses MI_Net (masked, two-sided, 13x1 UV tiling is
    baked into the material). Posts r=3.5 cm, h=107 cm at y=+-640 (MI_Metal);
    band and strap use MI_CourtLine.
    Returns the list of created actors.
    """
    made = []
    net = _mat(materials, "MI_Net")
    metal = _mat(materials, "MI_Metal")
    line = _mat(materials, "MI_CourtLine")

    # Plane local X (height axis after pitch -90) scales the 107 cm height,
    # local Y stays the 1280 cm width. Centre z = 53.5 -> bottom 0, top 107.
    actor = _plane("Net_Mesh", (0, 0, NET_HEIGHT * 0.5), (1.07, 12.8),
                   net, rot=(-90, 0, 0))
    if actor:
        made.append(actor)

    # posts: Cylinder is r50 x h200 cm -> r3.5 cm, h107 cm
    for side, y in (("East", NET_HALF_LENGTH), ("West", -NET_HALF_LENGTH)):
        actor = _spawn("Net_Post_%s" % side, "Cylinder",
                       (0, y, NET_HEIGHT * 0.5), (0.07, 0.07, 0.535), metal)
        if actor:
            made.append(actor)

    # white top band: 3 cm thick, 1280 cm wide, 6 cm tall at the very top
    actor = _spawn("Net_Band", "Cube", (0, 0, NET_HEIGHT - 3.0),
                   (0.03, 12.8, 0.06), line)
    if actor:
        made.append(actor)

    # centre strap: 5 cm wide, full net height, down the middle
    actor = _spawn("Net_Strap", "Cube", (0, 0, NET_HEIGHT * 0.5),
                   (0.02, 0.05, 1.07), line)
    if actor:
        made.append(actor)

    U.log("scene_props: net built (%d actors)" % len(made))
    return made


# ------------------------------------------------------------------------- 2

def build_fence(materials, x_half=1750.0, y_half=950.0):
    """Perimeter fence: 4 m windscreen planes on all four sides, steel posts
    every ~3 m, a top rail and a white sponsor band on the long sides.
    A 1.2 m gate gap is left in the middle of the -X side.
    Returns the list of created actors.
    """
    made = []
    screen = _mat(materials, "MI_Windscreen")
    metal = _mat(materials, "MI_Metal")
    line = _mat(materials, "MI_CourtLine")
    _ensure_two_sided(screen, "MI_Windscreen")

    zc = 200.0                       # screen centre: 0..400 cm
    screen_len_m = 2.0 * y_half / 100.0
    side_len_m = 2.0 * x_half / 100.0

    # +X end screen (solid), -X end split around the 1.2 m gate gap
    actor = _plane("Fence_End_Plus", (x_half, 0, zc),
                   (4.0, screen_len_m), screen, rot=(-90, 0, 0))
    if actor:
        made.append(actor)
    seg_len_cm = (2.0 * y_half - GATE_WIDTH) * 0.5        # 890 cm each
    seg_centre = GATE_WIDTH * 0.5 + seg_len_cm * 0.5      # y = +-505
    for sy in (-1, 1):
        actor = _plane("Fence_End_Minus_Seg_%.0f" % (sy * seg_centre),
                       (-x_half, sy * seg_centre, zc),
                       (4.0, seg_len_cm / 100.0), screen, rot=(-90, 0, 0))
        if actor:
            made.append(actor)

    # long side screens (rotated so the 4 m edge is vertical, length along X)
    for sy in (-1, 1):
        actor = _plane("Fence_Side_%.0f" % (sy * y_half),
                       (0, sy * y_half, zc), (4.0, side_len_m), screen,
                       rot=(-90, 90, 0))
        if actor:
            made.append(actor)

    # galvanized posts: corners + every 300 cm strictly inside each side
    posts = []
    for sx in (-x_half, x_half):
        for k in range(-5, 6):                     # -1500 .. 1500 step 300
            if abs(k * 300.0) < x_half - 100.0:
                posts.append((sx, k * 300.0))
    for sy in (-y_half, y_half):
        for k in range(-3, 4):                     # -900 .. 900 step 300
            if abs(k * 300.0) < y_half - 100.0:
                posts.append((k * 300.0, sy))
    for cx in (-x_half, x_half):
        for cy in (-y_half, y_half):
            posts.append((cx, cy))
    for idx, (px, py) in enumerate(posts):
        actor = _spawn("FencePost_%d" % idx, "Cylinder", (px, py, 210.0),
                       (0.06, 0.06, 2.1), metal)
        if actor:
            made.append(actor)

    # gate frame posts on the -X side (slimmer)
    for sy in (-1, 1):
        actor = _spawn("GatePost_%.0f" % (sy * 60.0), "Cylinder",
                       (-x_half, sy * GATE_WIDTH * 0.5, 210.0),
                       (0.05, 0.05, 2.1), metal)
        if actor:
            made.append(actor)

    # top rails: horizontal cylinders (axis Z rotated onto X or Y)
    # Cylinder is h200 cm, so scale z = length_cm / 200.
    for sy in (-1, 1):
        actor = _spawn("FenceRail_X_%.0f" % (sy * y_half), "Cylinder",
                       (0, sy * y_half, 412.0),
                       (0.04, 0.04, 2.0 * x_half / 200.0), metal, rot=(90, 0, 0))
        if actor:
            made.append(actor)
    # +X rail (full) and -X rails split around the gate
    actor = _spawn("FenceRail_Y_Plus", "Cylinder", (x_half, 0, 412.0),
                   (0.04, 0.04, 2.0 * y_half / 200.0), metal, rot=(90, 90, 0))
    if actor:
        made.append(actor)
    for sy in (-1, 1):
        actor = _spawn("FenceRail_Y_Minus_Seg_%.0f" % (sy * seg_centre),
                       "Cylinder", (-x_half, sy * seg_centre, 412.0),
                       (0.04, 0.04, seg_len_cm / 200.0), metal, rot=(90, 90, 0))
        if actor:
            made.append(actor)

    # white sponsor band at 1.1 m on the two long sides (thin box)
    for sy in (-1, 1):
        actor = _spawn("SponsorBand_%.0f" % (sy * y_half), "Cube",
                       (0, sy * (y_half - 2.0), 110.0), (15.0, 0.04, 0.30), line)
        if actor:
            made.append(actor)

    U.log("scene_props: fence built (%d actors)" % len(made))
    return made


# ------------------------------------------------------------------------- 3

def build_landscape(materials):
    """Club surround: hedge lines outside the +Y/-Y fence, 7 cypress trees on
    the far +X side, 3 floodlight towers, 2 player benches, a ball cart and a
    water cooler. Total stays well under 120 actors.
    Returns the list of created actors.
    """
    made = []
    metal = _mat(materials, "MI_Metal")
    line = _mat(materials, "MI_CourtLine")
    carbon = _mat(materials, "MI_Carbon", "MI_Metal")

    hedge = _mat(materials, "MI_Hedge", "MI_Windscreen")
    _ensure_two_sided(_mat(materials, "MI_Windscreen"), "MI_Windscreen")
    tree = _mat(materials, "MI_Tree", "MI_Windscreen")
    flood = _mat(materials, "MI_Floodlight")

    # ---- hedge: 120x80x90 cm boxes along the outside of +-Y fence sides
    for sy in (-1, 1):
        for i in range(9):
            hx = -1700.0 + i * 425.0
            actor = _spawn("Hedge_%.0f_%d" % (sy * HEDGE_OUT_Y, i), "Cube",
                           (hx, sy * HEDGE_OUT_Y, 45.0), (1.2, 0.8, 0.9), hedge)
            if actor:
                made.append(actor)

    # ---- 7 cypress trees on the far +X side (trunk cylinder + cone canopy)
    tree_spots = ((2350, -870), (2550, -580), (2350, -290), (2550, 0),
                  (2350, 290), (2550, 580), (2350, 870))
    tree_heights = (7.0, 7.6, 8.2, 8.8, 7.4, 8.0, 8.6)   # metres, 7-9 m
    for i, ((tx, ty), height) in enumerate(zip(tree_spots, tree_heights)):
        trunk_h = height * 0.35                          # metres
        canopy_h = height * 0.72                         # metres, overlaps trunk
        actor = _spawn("Cypress_Trunk_%d" % i, "Cylinder",
                       (tx, ty, trunk_h * 50.0),
                       (0.15, 0.15, trunk_h / 2.0), tree)
        if actor:
            made.append(actor)
        # Cone is base r50 x h100 cm -> scale (1.25, 1.25) = 2.5 m canopy width
        actor = _spawn("Cypress_Canopy_%d" % i, "Cone",
                       (tx, ty, (trunk_h + canopy_h * 0.5) * 100.0),
                       (1.25, 1.25, canopy_h), tree)
        if actor:
            made.append(actor)

    # ---- 3 floodlight towers, 14 m poles, emissive panel facing the court
    for tx, ty in ((2100, 1250), (-2100, 1250), (2100, -1250)):
        tag = "%+.0f_%+.0f" % (tx, ty)
        yaw = math.degrees(math.atan2(-ty, -tx))         # face court centre
        dist = max(1.0, math.hypot(tx, ty))
        ux, uy = -tx / dist, -ty / dist
        actor = _spawn("TowerPole_%s" % tag, "Cylinder", (tx, ty, 700.0),
                       (0.25, 0.25, 7.0), metal)
        if actor:
            made.append(actor)
        actor = _spawn("TowerHead_%s" % tag, "Cube", (tx, ty, 1430.0),
                       (0.4, 2.8, 1.3), metal, rot=(0, yaw, 0))
        if actor:
            made.append(actor)
        actor = _spawn("TowerPanel_%s" % tag, "Plane",
                       (tx + ux * 45.0, ty + uy * 45.0, 1400.0),
                       (1.3, 2.6, 1), flood, rot=(-90, yaw, 0), shadow=False)
        if actor:
            made.append(actor)

    # ---- 2 player benches with legs (west side, behind the baselines)
    for i, sy in enumerate((-1, 1)):
        by = sy * 320.0
        actor = _spawn("Bench_%d_Seat" % i, "Cube", (-1500, by, 45.0),
                       (2.4, 0.55, 0.08), carbon)
        if actor:
            made.append(actor)
        actor = _spawn("Bench_%d_Back" % i, "Cube", (-1620, by, 75.0),
                       (0.08, 0.55, 0.50), carbon)
        if actor:
            made.append(actor)
        for leg_x in (-1610, -1390):
            actor = _spawn("Bench_%d_Leg_%.0f" % (i, leg_x), "Cube",
                           (leg_x, by, 22.5), (0.08, 0.50, 0.45), metal)
            if actor:
                made.append(actor)

    # ---- ball cart: box + 4 small sphere wheels
    cart_x, cart_y = -1380.0, -470.0
    actor = _spawn("BallCart_Box", "Cube", (cart_x, cart_y, 37.0),
                   (0.9, 0.6, 0.6), metal)
    if actor:
        made.append(actor)
    for wx in (-1, 1):
        for wy in (-1, 1):
            actor = _spawn("BallCart_Wheel_%+.0f_%+.0f" % (wx, wy), "Sphere",
                           (cart_x + wx * 34.0, cart_y + wy * 24.0, 7.0),
                           (0.14, 0.14, 0.14), carbon)
            if actor:
                made.append(actor)

    # ---- water cooler (single white box)
    actor = _spawn("WaterCooler", "Cube", (-1420, 470, 45.0),
                   (0.5, 0.45, 0.9), _mat(materials, "MI_CourtLine",
                                          "MI_Concrete"))
    if actor:
        made.append(actor)

    U.log("scene_props: landscape built (%d actors)" % len(made))
    return made


# ------------------------------------------------------------------------- 4

def build_lighting(materials=None):
    """Ensure Sun / SkyAtmosphere / SkyLight / ExponentialHeightFog exist with
    the wanted settings; never creates duplicates (searches by label+class
    first). Returns the list of newly spawned actors.
    """
    made = []
    subs = U.actors()

    # --- directional light "Sun"
    sun = _find_actor(label="Sun", cls=unreal.DirectionalLight)
    if sun is None:
        sun = subs.spawn_actor_from_class(
            unreal.DirectionalLight, unreal.Vector(0, 0, 1200),
            unreal.Rotator(-11.0, 205.0, 0))
        sun.set_actor_label("Sun")
        made.append(sun)
    else:
        try:
            sun.set_actor_rotation(unreal.Rotator(-11.0, 205.0, 0))
        except Exception as exc:  # noqa: BLE001
            U.log("scene_props: sun rotation failed: %s" % exc)
    sc = sun.get_component_by_class(unreal.DirectionalLightComponent)
    try:
        sc.set_mobility(unreal.ComponentMobility.MOVABLE)
    except Exception:  # noqa: BLE001
        pass
    _set_props(sc, {
        "intensity": 9.0,
        "use_temperature": True,
        "temperature": 3600.0,
        "atmosphere_sun_light": True,
        "atmosphere_sun_light_index": 0,
        "light_source_angle": 0.7,
        "cast_shadows": True,
    })

    # --- SkyAtmosphere
    if _find_actor(cls=unreal.SkyAtmosphere) is None:
        atmos = subs.spawn_actor_from_class(unreal.SkyAtmosphere,
                                            unreal.Vector(0, 0, 0),
                                            unreal.Rotator(0, 0, 0))
        atmos.set_actor_label("SkyAtmosphere")
        made.append(atmos)

    # --- SkyLight (real-time capture)
    sky = _find_actor(label="SkyLight", cls=unreal.SkyLight)
    if sky is None:
        sky = subs.spawn_actor_from_class(unreal.SkyLight,
                                          unreal.Vector(0, 0, 800),
                                          unreal.Rotator(0, 0, 0))
        sky.set_actor_label("SkyLight")
        made.append(sky)
    skc = sky.get_component_by_class(unreal.SkyLightComponent)
    try:
        skc.set_mobility(unreal.ComponentMobility.MOVABLE)
    except Exception:  # noqa: BLE001
        pass
    _set_props(skc, {
        "real_time_capture": True,
        "intensity": 1.0,
        "source_type": unreal.SkyLightSourceType.SLS_CAPTURED_SCENE,
    })
    try:
        skc.recapture_sky()
    except Exception as exc:  # noqa: BLE001
        U.log("scene_props: skylight recapture failed: %s" % exc)

    # --- ExponentialHeightFog
    fog = _find_actor(cls=unreal.ExponentialHeightFog)
    if fog is None:
        fog = subs.spawn_actor_from_class(unreal.ExponentialHeightFog,
                                          unreal.Vector(0, 0, 0),
                                          unreal.Rotator(0, 0, 0))
        fog.set_actor_label("HeightFog")
        made.append(fog)
    fc = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    _set_props(fc, {
        "fog_density": 0.006,
        "fog_height_falloff": 0.12,
        "enable_volumetric_fog": True,
        "volumetric_fog_extinction_scale": 0.35,
    })

    U.log("scene_props: lighting ensured (%d new actors)" % len(made))
    return made


# ------------------------------------------------------------------------- 5

def build_photo_camera():
    """CineCameraActor "PhotoCam" at (-1500, -1050, 170) looking at the net
    centre (0, 0, 53.5); 35 mm, f/4.0, auto-activate for PLAYER0.
    Returns the list of created (or re-used) actors.
    """
    loc = unreal.Vector(-1500.0, -1050.0, 170.0)
    target = unreal.Vector(0.0, 0.0, 53.5)
    rot = _look_at_rotator(loc, target)

    cam = _find_actor(label="PhotoCam", cls=unreal.CineCameraActor)
    if cam is None:
        cam = U.actors().spawn_actor_from_class(unreal.CineCameraActor,
                                                loc, rot)
        cam.set_actor_label("PhotoCam")
    else:
        try:
            cam.set_actor_location(loc, False, False)
            cam.set_actor_rotation(rot)
        except Exception as exc:  # noqa: BLE001
            U.log("scene_props: PhotoCam reposition failed: %s" % exc)

    cc = cam.get_component_by_class(unreal.CineCameraComponent)
    _set_props(cc, {
        "current_focal_length": 35.0,
        "current_aperture": 4.0,
    })
    try:
        cam.set_editor_property("auto_activate_for_player",
                                unreal.AutoReceiveInput.PLAYER0)
    except Exception as exc:  # noqa: BLE001
        U.log("scene_props: auto_activate_for_player failed: %s" % exc)

    U.log("scene_props: PhotoCam ready")
    return [cam]


# ------------------------------------------------------------------------- 6

def build_all():
    """Load /Game/Materials/MI_* (missing ones become None), build 1-5, log a
    summary and return a dict of counts."""
    materials = {}
    for name in MATERIAL_NAMES:
        materials[name] = _load_material(name)

    net, fence, landscape, lights, cam = [], [], [], [], []
    for name, fn in (("net", lambda: build_net(materials)),
                     ("fence", lambda: build_fence(materials)),
                     ("landscape", lambda: build_landscape(materials)),
                     ("lighting", lambda: build_lighting(materials)),
                     ("camera", build_photo_camera)):
        try:
            result = fn()
        except Exception:  # noqa: BLE001
            U.log("scene_props: %s failed\n%s" % (name, traceback.format_exc()))
            result = []
        if name == "net":
            net = result
        elif name == "fence":
            fence = result
        elif name == "landscape":
            landscape = result
        elif name == "lighting":
            lights = result
        else:
            cam = result

    counts = {
        "net": len(net),
        "fence": len(fence),
        "landscape": len(landscape),
        "lighting_new": len(lights),
        "camera": len(cam),
        "total_actors_spawned": len(net) + len(fence) + len(landscape)
        + len(lights) + len(cam),
        "materials_loaded": sum(1 for m in materials.values() if m),
        "materials_missing": [k for k, v in materials.items() if not v],
    }
    U.log("scene_props summary: %s" % counts)
    return counts
