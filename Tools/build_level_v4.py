"""ClayTennis level v4 (rewrite): ITF-exact tennis court, lines, net and posts.

Run INSIDE the Unreal 5.5 editor (build_editor_run.py already does this).

WHAT THIS BUILDS (and nothing else): playing surface, run-off apron, court
lines, net, net posts, singles sticks, centre strap, winder, lighting/sky,
post-process, PlayerStart, a photo camera and the hidden gameplay proxy.
All props were removed on purpose: fence/windscreen, hedges, trees, rocks,
benches, umpire chair, floodlight poles.

Reference dimensions (ITF Rules of Tennis, Appendix II / Rule 1-3):
  - Court 23.77 x 10.97 m (singles width 8.23 m), measured to the OUTSIDE
    edge of the lines. Every line belongs to the area it bounds.
  - Baselines up to 10 cm wide (we use 10 cm); every other line 5 cm.
  - Service line: 6.40 m from the net, measured to its OUTER edge (the edge
    away from the net), so the painted line occupies 6.35 - 6.40 m.
  - Centre service line: net -> service line, 5 cm wide, centred on the court.
  - Centre mark: 10 cm long x 5 cm wide, inside the court at each baseline.
  - Net: 1.07 m at the posts / singles sticks, 0.914 m at the centre,
    held down by a white centre strap (max 5 cm wide). Top band 5-6.35 cm,
    white. Net cord/cable max 0.8 cm. Mesh fine enough to stop a ball.
  - Posts: centred 0.914 m outside the doubles sidelines, max 15 cm across,
    top no more than 2.5 cm above the net cord. Singles sticks: max 7.5 cm
    across, centred 0.914 m outside the singles sidelines, 1.07 m high.
  - Recommended run-off: 6.40 m behind baselines, 3.66 m beside the sidelines
    (36.57 x 18.29 m total).

Axes match ATennisCourt (C++): long axis = X, net at X = 0 spanning Y,
baselines at X = +/-1188.5 cm, posts at Y = +/-639.9 cm. Units: cm.

Project traps respected (AGENTS.md): no TextureCoordinate / world-UV nodes,
flat parameter materials only, level purged after new_level().
"""
import math
import sys
import traceback

import unreal

sys.path.insert(0, r"C:\Projects\ClayTennis\Tools")
import ue_common as U  # noqa: E402

OUT = r"C:\Projects\ClayTennis\RawAssets\build_level_v4_result.txt"
RES = []


def note(s):
    RES.append(str(s))


# ----------------------------------------------------------------- switches
SINGLES_STICKS = True   # True: singles-match net (sticks at 1.07 m, net flat
                        # 1.07 m from stick to post). False: doubles net.

# ------------------------------------------------------------- ITF dims (cm)
HALF_LEN = 1188.5       # 23.77 m / 2
HALF_DBL = 548.5        # 10.97 m / 2
HALF_SGL = 411.5        # 8.23 m / 2
SVC = 640.0             # service line OUTER edge, 6.40 m from the net
W_BASE = 10.0           # baseline width
W_LINE = 5.0            # every other line
CENTER_MARK_LEN = 10.0

POST_OFFSET = 91.4      # 0.914 m outside the sideline
POST_Y = HALF_DBL + POST_OFFSET        # 639.9, post axis
STICK_Y = HALF_SGL + POST_OFFSET       # 502.9, singles stick axis
POST_DIA = 10.0         # round post (ITF max 15 cm)
POST_R = POST_DIA / 2.0
STICK_DIA = 7.5         # ITF max 7.5 cm

NET_POST_H = 107.0      # 1.07 m
NET_CTR_H = 91.4        # 0.914 m
POST_TOP = NET_POST_H + 1.5            # <= 2.5 cm above the cord
BAND_H = 6.0            # white top band (ITF 5 - 6.35 cm)
BAND_T = 0.8            # band thickness (folded fabric)
STRAP_W = 5.0           # centre strap (ITF max 5 cm)
STRAP_T = 0.5
NET_BOTTOM = 1.2        # net lower edge above the surface
NET_HALF = POST_Y - POST_R             # net spans post surface to post surface

TOTAL_L = 3657.0        # recommended playing area
TOTAL_W = 1829.0

# Z layering (cm). Planes sit a few mm apart to avoid z-fighting while still
# looking like paint on the surface.
Z_APRON, Z_IN, Z_LINE = 0.0, 0.3, 0.6
Z_GROUND = -1.0


def srgb(hexstr):
    """'#RRGGBB' (sRGB, how colours are quoted) -> linear RGB tuple for UE."""
    h = hexstr.lstrip("#")
    out = []
    for i in (0, 2, 4):
        c = int(h[i:i + 2], 16) / 255.0
        out.append(c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4)
    return tuple(out)


COL_IN = srgb("#2D5FA3")        # hard-court blue (inside the court)
COL_APRON = srgb("#3E7B4F")     # hard-court green (run-off)
COL_LINE = srgb("#F3F3EF")      # painted white line
COL_BAND = srgb("#F2F2EE")      # net band / strap (white)
COL_CORD = (0.008, 0.008, 0.009)   # dark net twine
COL_POST = (0.020, 0.022, 0.025)   # graphite powder coat
COL_STEEL = (0.55, 0.56, 0.58)


def mi(name):
    return unreal.EditorAssetLibrary.load_asset("/Game/Materials/" + name)


def ensure(name, color, rough, metal=0.0):
    """Flat parameter material M_<x> + instance MI_<x>. No UV nodes."""
    p = "/Game/Materials/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(p):
        m = unreal.EditorAssetLibrary.load_asset(p)
        U.MEL.delete_all_material_expressions(m)
    else:
        m = U.new_material(name)
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
    inst_name = "MI_" + name[2:]
    if not unreal.EditorAssetLibrary.does_asset_exist("/Game/Materials/" + inst_name):
        U.new_instance(inst_name, m)
    return mi(inst_name) or m


# ------------------------------------------------------------- net geometry
def net_top(y):
    """Top of the net band (cm) at lateral position y.

    Straight lines from the centre strap (0.914 m) up to 1.07 m at the
    singles sticks (flat 1.07 m from the sticks to the posts), or straight to
    the posts when SINGLES_STICKS is False. Keep ATennisCourt /
    TennisPhysics::GetNetHeightAtY identical to this function.
    """
    a = abs(y)
    if SINGLES_STICKS:
        t = min(a / STICK_Y, 1.0)
    else:
        t = min(a / POST_Y, 1.0)
    return NET_CTR_H + (NET_POST_H - NET_CTR_H) * t


def sample_ys(step):
    """Symmetric y samples across the net, always including the kinks."""
    n = max(1, int(math.ceil(NET_HALF / step)))
    pos = {round(NET_HALF * i / n, 3) for i in range(n + 1)}
    if SINGLES_STICKS:
        pos.add(round(STICK_Y, 3))
    pos = sorted(pos)
    return [-y for y in reversed(pos[1:])] + pos


class Batch(object):
    """Collects many identical-mesh pieces. Uses ONE instanced-mesh actor when
    the editor allows it, otherwise falls back to one actor per piece."""

    def __init__(self, label, shape, material, shadow, use_ism):
        self.label, self.shape, self.material = label, shape, material
        self.shadow, self.use_ism = shadow, use_ism
        self.items = []

    def add(self, loc, scale, pitch=0.0, yaw=0.0):
        """loc in cm, scale in metres (mesh is 100 cm), rotation in degrees."""
        self.items.append((loc, scale, pitch, yaw))

    def flush(self):
        if not self.items:
            return 0
        if self.use_ism:
            try:
                return self._flush_ism()
            except Exception as exc:  # noqa: BLE001
                note("ISM failed for %s (%s); per-actor fallback" % (self.label, exc))
        for i, (loc, scale, pitch, yaw) in enumerate(self.items):
            U.spawn_mesh("%s_%d" % (self.label, i), self.shape, loc, scale,
                         self.material, rotation=(pitch, yaw, 0.0),
                         cast_shadow=self.shadow)
        return len(self.items)

    def _flush_ism(self):
        actor = U.actors().spawn_actor_from_class(
            unreal.StaticMeshActor, unreal.Vector(0, 0, 0),
            unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
        actor.set_actor_label(self.label)
        comp = actor.add_component_by_class(
            unreal.InstancedStaticMeshComponent, False, unreal.Transform(), False)
        comp.set_static_mesh(U.basic_mesh(self.shape))
        comp.set_material(0, self.material)
        for k, v in (("cast_shadow", self.shadow),):
            try:
                comp.set_editor_property(k, v)
            except Exception:  # noqa: BLE001
                pass
        try:
            comp.set_collision_profile_name("NoCollision")
        except Exception:  # noqa: BLE001
            pass
        tfs = []
        for loc, scale, pitch, yaw in self.items:
            tfs.append(unreal.Transform(
                location=unreal.Vector(*loc),
                rotation=unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw),
                scale=unreal.Vector(*scale)))
        try:
            comp.add_instances(tfs, False)
        except AttributeError:
            for t in tfs:
                comp.add_instance(t)
        return len(tfs)


def probe_ism():
    """True if this editor can build instanced-mesh components from Python."""
    try:
        actor = U.actors().spawn_actor_from_class(
            unreal.StaticMeshActor, unreal.Vector(0, 0, 0),
            unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
        comp = actor.add_component_by_class(
            unreal.InstancedStaticMeshComponent, False, unreal.Transform(), False)
        ok = comp is not None
        U.actors().destroy_actor(actor)
        return ok
    except Exception as exc:  # noqa: BLE001
        note("ISM probe failed: %s" % exc)
        return False


def segment(batch, y0, z0, y1, z1, thick_cm, width_cm, extra=0.3, dz_center=0.0):
    """Cube whose long axis follows (y0,z0)->(y1,z1) in the net plane.

    Rotation yaw=90 points local X along +Y; pitch tilts it up with the slope
    (UE pitch>0 = up). Local Y then runs through the net (world -X), local Z is
    the segment's height. scale = (length, through-net thickness, height).
    """
    dy, dz = y1 - y0, z1 - z0
    length = math.hypot(dy, dz) + extra
    pitch = math.degrees(math.atan2(dz, dy))
    batch.add(((0.0), (y0 + y1) / 2.0, (z0 + z1) / 2.0 + dz_center),
              (length / 100.0, thick_cm / 100.0, width_cm / 100.0),
              pitch=pitch, yaw=90.0)


def build_net(mats, use_ism):
    if use_ism:
        step_v, step_seg, step_row = 4.0, 8.0, 4.0     # real-looking 4 cm mesh
    else:
        step_v, step_seg, step_row = 12.0, 20.0, 12.0  # coarse, actor budget
    ys = sample_ys(step_seg)
    counts = {}

    # ---- white top band: follows the net top exactly (kinks included)
    band = Batch("NetBand", "Cube", mats["band"], True, use_ism)
    for a, b in zip(ys[:-1], ys[1:]):
        segment(band, a, net_top(a), b, net_top(b), BAND_T, BAND_H,
                dz_center=-BAND_H / 2.0)
    counts["band"] = band.flush()

    # ---- vertical twine every step_v cm
    cord = Batch("NetCordV", "Cube", mats["cord"], False, use_ism)
    n = int(NET_HALF // step_v)
    for k in range(-n, n + 1):
        y = k * step_v
        top = net_top(y) - BAND_H + 0.6
        h = top - NET_BOTTOM
        cord.add((0.0, y, NET_BOTTOM + h / 2.0), (0.0035, 0.0040, h / 100.0))
    counts["vertical"] = cord.flush()

    # ---- horizontal twine rows, proportional to the sagging top
    rows = max(4, int(round((NET_CTR_H - BAND_H - NET_BOTTOM) / step_row)))
    horiz = Batch("NetCordH", "Cube", mats["cord"], False, use_ism)
    for j in range(rows):
        f = j / float(rows)
        for a, b in zip(ys[:-1], ys[1:]):
            za = NET_BOTTOM + f * (net_top(a) - BAND_H - NET_BOTTOM)
            zb = NET_BOTTOM + f * (net_top(b) - BAND_H - NET_BOTTOM)
            thick = 0.30
            wid = 0.60 if j == 0 else 0.40      # bottom edge is a heavier cord
            segment(horiz, a, za, b, zb, thick, wid, extra=0.2)
    counts["horizontal"] = horiz.flush()

    # ---- vertical edge tape where the net meets each post
    edge = Batch("NetEdge", "Cube", mats["band"], True, use_ism)
    for s in (-1.0, 1.0):
        y = s * (NET_HALF - 0.8)
        h = NET_POST_H - NET_BOTTOM
        edge.add((0.0, y, NET_BOTTOM + h / 2.0), (0.006, 0.016, h / 100.0))
    counts["edge"] = edge.flush()

    # ---- centre strap: white, 5 cm, ground anchor to the net band
    strap_top = NET_CTR_H - BAND_H + 0.5
    strap_h = strap_top - Z_LINE
    U.spawn_mesh("Net_CentreStrap", "Cube", (0.0, 0.0, Z_LINE + strap_h / 2.0),
                 (STRAP_T / 100.0 + 0.0, STRAP_W / 100.0, strap_h / 100.0), mats["band"])
    # strap is a loop: a second, slightly offset face so it reads from both sides
    U.spawn_mesh("Net_CentreStrap_B", "Cube", (-0.7, 0.0, Z_LINE + strap_h / 2.0),
                 (STRAP_T / 100.0, STRAP_W / 100.0, strap_h / 100.0), mats["band"])
    U.spawn_mesh("Net_StrapClip", "Cube", (0.0, 0.0, strap_top - 3.0),
                 (0.012, 0.060, 0.045), mats["steel"])
    U.spawn_mesh("Net_StrapAnchor", "Cylinder", (0.0, 0.0, 0.5),
                 (0.06, 0.06, 0.005), mats["steel"], cast_shadow=False)
    note("net: %s" % counts)


def build_posts(mats):
    for s, tag in ((1.0, "E"), (-1.0, "W")):
        y = s * POST_Y
        U.spawn_mesh("Net_Post_" + tag, "Cylinder", (0.0, y, POST_TOP / 2.0),
                     (POST_DIA / 100.0, POST_DIA / 100.0, POST_TOP / 200.0), mats["post"])
        # domed cap (flattened sphere), centred on the top of the post
        U.spawn_mesh("Net_PostCap_" + tag, "Sphere", (0.0, y, POST_TOP),
                     (POST_DIA / 100.0 * 1.06, POST_DIA / 100.0 * 1.06, 0.03), mats["post"])
        # ground sleeve (flush steel collar)
        U.spawn_mesh("Net_PostSleeve_" + tag, "Cylinder", (0.0, y, 0.6),
                     (0.14, 0.14, 0.006), mats["steel"], cast_shadow=False)
        # pulley head just under the cap
        U.spawn_mesh("Net_PostHead_" + tag, "Cylinder", (0.0, y, NET_POST_H - 1.0),
                     (POST_DIA / 100.0 * 1.12, POST_DIA / 100.0 * 1.12, 0.010), mats["steel"])
    # net winder on the +Y post: hub + folded handle, on the outer face
    yb = POST_Y + POST_R + 2.0
    U.spawn_mesh("Net_Winder_Hub", "Cylinder", (0.0, yb, 75.0), (0.06, 0.06, 0.02),
                 mats["steel"], rotation=(0.0, 0.0, 90.0))
    U.spawn_mesh("Net_Winder_Handle", "Cube", (0.0, yb + 2.6, 70.0), (0.012, 0.012, 0.10),
                 mats["steel"])
    if SINGLES_STICKS:
        for s, tag in ((1.0, "E"), (-1.0, "W")):
            U.spawn_mesh("Net_Stick_" + tag, "Cylinder",
                         (0.0, s * STICK_Y, NET_POST_H / 2.0),
                         (STICK_DIA / 100.0, STICK_DIA / 100.0, NET_POST_H / 200.0),
                         mats["post"])
    note("posts: 2 + winder%s" % (" + 2 singles sticks" if SINGLES_STICKS else ""))


# ------------------------------------------------------------------- lines
LINE_MAT = None
LINE_COUNT = 0


def paint(label, cx, cy, len_x, wid_y):
    """Paint stripe: a flat plane, centre (cx,cy) cm, size len_x x wid_y cm."""
    global LINE_COUNT
    U.spawn_mesh(label, "Plane", (cx, cy, Z_LINE),
                 (len_x / 100.0, wid_y / 100.0, 1.0), LINE_MAT, cast_shadow=False)
    LINE_COUNT += 1


def build_lines():
    inner_x = HALF_LEN - W_BASE           # baseline inner edge (1178.5)
    side_len = 2.0 * inner_x              # sidelines stop at the baselines
    for s, tag in ((1.0, "E"), (-1.0, "W")):
        # baselines: outer edge on +/-1188.5, full doubles width
        paint("Line_Baseline_" + tag, s * (HALF_LEN - W_BASE / 2.0), 0.0,
              W_BASE, 2.0 * HALF_DBL)
        # service lines: OUTER edge at 6.40 m from the net, singles-width
        paint("Line_Service_" + tag, s * (SVC - W_LINE / 2.0), 0.0,
              W_LINE, 2.0 * (HALF_SGL - W_LINE))
        # centre service line: net -> inner edge of the service line
        cs = SVC - W_LINE
        paint("Line_CentreService_" + tag, s * cs / 2.0, 0.0, cs, W_LINE)
        # centre mark: 10 cm long, just inside the baseline
        paint("Line_CentreMark_" + tag,
              s * (inner_x - CENTER_MARK_LEN / 2.0), 0.0, CENTER_MARK_LEN, W_LINE)
    for s, tag in ((1.0, "N"), (-1.0, "S")):
        paint("Line_Doubles_" + tag, 0.0, s * (HALF_DBL - W_LINE / 2.0), side_len, W_LINE)
        paint("Line_Singles_" + tag, 0.0, s * (HALF_SGL - W_LINE / 2.0), side_len, W_LINE)
    note("lines: %d painted stripes (ITF)" % LINE_COUNT)


# ------------------------------------------------------------------- scene
def set_props(obj, props, tag):
    for k, v in props.items():
        try:
            obj.set_editor_property(k, v)
        except Exception as exc:  # noqa: BLE001
            note("%s %s: %s" % (tag, k, exc))


def build():
    global LINE_MAT
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    les.new_level("/Game/Maps/TennisCourt")

    # new_level() can re-open the existing map and keep old actors: purge.
    purged = 0
    for old in list(actors.get_all_level_actors()):
        try:
            actors.destroy_actor(old)
            purged += 1
        except Exception:  # noqa: BLE001
            pass
    note("purged actors: %d" % purged)

    # ----------------------------------------------------------- materials
    inner = ensure("M_CourtHardInner", COL_IN, 0.80)
    apron = ensure("M_CourtHardApron", COL_APRON, 0.85)
    LINE_MAT = ensure("M_CourtLinePaint", COL_LINE, 0.55)
    mats = {
        "band": ensure("M_NetBand", COL_BAND, 0.85),
        "cord": ensure("M_NetCord", COL_CORD, 0.90),
        "post": ensure("M_NetPost", COL_POST, 0.40, 0.5),
        "steel": ensure("M_NetSteel", COL_STEEL, 0.30, 1.0),
    }
    ground = mi("MI_Concrete")

    # --------------------------------------------------------------- floor
    U.spawn_mesh("Ground_Outer", "Plane", (0, 0, Z_GROUND), (400.0, 400.0, 1.0),
                 ground, cast_shadow=False)
    U.spawn_mesh("Apron_Acrylic", "Plane", (0, 0, Z_APRON),
                 (TOTAL_L / 100.0, TOTAL_W / 100.0, 1.0), apron, cast_shadow=False)
    U.spawn_mesh("Court_Inner", "Plane", (0, 0, Z_IN),
                 (2.0 * HALF_LEN / 100.0, 2.0 * HALF_DBL / 100.0, 1.0), inner,
                 cast_shadow=False)

    build_lines()

    # ------------------------------------------------------ gameplay proxy
    court_cls = getattr(unreal, "TennisCourt", None)
    if court_cls:
        gc = actors.spawn_actor_from_class(court_cls, unreal.Vector(0, 0, 0),
                                           unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
        gc.set_actor_label("GameplayCourt")
        try:
            gc.set_actor_hidden_in_game(True)
            gc.set_is_temporarily_hidden_in_editor(True)
            hidden = 0
            for cls in (unreal.StaticMeshComponent, unreal.InstancedStaticMeshComponent):
                for comp in gc.get_components_by_class(cls):
                    comp.set_visibility(False, True)
                    comp.set_editor_property("cast_shadow", False)
                    hidden += 1
            note("gameplay proxy components hidden: %d" % hidden)
        except Exception as exc:  # noqa: BLE001
            note("court hide: %s" % exc)

    # ----------------------------------------------------------- net + posts
    use_ism = probe_ism()
    note("instanced meshes available: %s" % use_ism)
    build_net(mats, use_ism)
    build_posts(mats)

    # ------------------------------------------------------------ lighting
    sun = actors.spawn_actor_from_class(
        unreal.DirectionalLight, unreal.Vector(0, 0, 1500),
        unreal.Rotator(roll=0.0, pitch=-55.0, yaw=155.0))
    sun.set_actor_label("Sun")
    sc = sun.get_component_by_class(unreal.DirectionalLightComponent)
    sc.set_mobility(unreal.ComponentMobility.MOVABLE)
    set_props(sc, {
        "intensity": 10.0,
        "use_temperature": True,
        "temperature": 5600.0,
        "atmosphere_sun_light": True,
        "atmosphere_sun_light_index": 0,
        "light_source_angle": 0.5357,       # real angular diameter of the sun
        "cast_shadows": True,
    }, "sun")

    sa = actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0),
                                       unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
    sa.set_actor_label("SkyAtmosphere")
    cl = actors.spawn_actor_from_class(unreal.VolumetricCloud, unreal.Vector(0, 0, 3000),
                                       unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
    cl.set_actor_label("Clouds")
    sl = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 900),
                                       unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
    sl.set_actor_label("SkyLight")
    skc = sl.get_component_by_class(unreal.SkyLightComponent)
    skc.set_mobility(unreal.ComponentMobility.MOVABLE)
    set_props(skc, {"real_time_capture": True, "intensity": 2.0,
                    "source_type": unreal.SkyLightSourceType.SLS_CAPTURED_SCENE}, "sky")

    fog = actors.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0),
                                        unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
    fog.set_actor_label("Fog")
    set_props(fog.get_component_by_class(unreal.ExponentialHeightFogComponent), {
        "fog_density": 0.003, "fog_height_falloff": 0.1,
        "enable_volumetric_fog": True, "volumetric_fog_extinction_scale": 0.25,
    }, "fog")

    # ------------------------------------------------------------- cameras
    cam_loc = (-1600.0, -700.0, 300.0)
    tgt = (150.0, 40.0, 5.0)
    dx, dy, dz = tgt[0] - cam_loc[0], tgt[1] - cam_loc[1], tgt[2] - cam_loc[2]
    yaw = math.degrees(math.atan2(dy, dx))
    pitch = math.degrees(math.atan2(dz, math.hypot(dx, dy)))
    cam = actors.spawn_actor_from_class(
        unreal.CineCameraActor, unreal.Vector(*cam_loc),
        unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw))
    cam.set_actor_label("PhotoCam")
    set_props(cam.get_component_by_class(unreal.CineCameraComponent), {
        "current_focal_length": 30.0, "current_aperture": 5.6}, "cam")
    # no auto_activate_for_player: the pawn spring arm owns the -game view

    ps = actors.spawn_actor_from_class(
        unreal.PlayerStart, unreal.Vector(-1308.0, -100.0, 90.0),
        unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
    ps.set_actor_label("PlayerStart")

    # --------------------------------------------------------- post process
    ppv = actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0),
                                        unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
    ppv.set_actor_label("PostProcess")
    ppv.set_editor_property("unbound", True)
    st = ppv.get_editor_property("settings")

    def setp(key, value, override_key):
        try:
            st.set_editor_property(override_key, True)
            st.set_editor_property(key, value)
        except Exception as exc:  # noqa: BLE001
            note("pp %s: %s" % (key, exc))

    setp("bloom_intensity", 0.30, "override_bloom_intensity")
    setp("vignette_intensity", 0.25, "override_vignette_intensity")
    setp("scene_fringe_intensity", 0.0, "override_scene_fringe_intensity")  # no fake CA
    setp("film_grain_intensity", 0.02, "override_film_grain_intensity")
    setp("motion_blur_amount", 0.25, "override_motion_blur_amount")
    setp("auto_exposure_method", unreal.AutoExposureMethod.AEM_HISTOGRAM,
         "override_auto_exposure_method")
    setp("auto_exposure_bias", -0.5, "override_auto_exposure_bias")
    setp("auto_exposure_min_brightness", 0.05, "override_auto_exposure_min_brightness")
    setp("auto_exposure_max_brightness", 1.5, "override_auto_exposure_max_brightness")
    ppv.set_editor_property("settings", st)

    les.save_current_level()
    note("saved, actors=%d" % len(actors.get_all_level_actors()))


try:
    build()
except Exception:  # noqa: BLE001
    note("EXC\n" + traceback.format_exc())

with open(OUT, "w", encoding="utf-8") as f:
    f.write("\n".join(RES))
unreal.log("[LEVELV4] " + (RES[-1][:160] if RES else "no result"))
