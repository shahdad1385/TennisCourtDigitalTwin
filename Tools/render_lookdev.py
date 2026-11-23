"""Headless look-dev render: SceneCapture2D -> PNG, time-based (works with locked desktop)."""
import unreal
import math
import traceback

OUT = r"C:\Projects\ClayTennis\RawAssets\render_lookdev_result.txt"


def log(msg):
    unreal.log("[LOOKDEV] " + str(msg))


def look_at(src, dst):
    d = dst - src
    yaw = math.degrees(math.atan2(d.y, d.x))
    pitch = math.degrees(math.atan2(d.z, math.sqrt(d.x * d.x + d.y * d.y)))
    return unreal.Rotator(pitch, yaw, 0)


state = {"t": 0.0, "captured": False, "exported": False, "done": False}


def main():
    world = unreal.EditorLevelLibrary.get_editor_world()
    log("world=%s" % world)

    path = "/Game/LookDev/RT_LookDev"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        rt = unreal.EditorAssetLibrary.load_asset(path)
    else:
        rt = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "RT_LookDev", "/Game/LookDev", unreal.TextureRenderTarget2D,
            unreal.TextureRenderTargetFactoryNew())
    rt.set_editor_property("size_x", 1920)
    rt.set_editor_property("size_y", 1080)
    rt.set_editor_property("render_target_format", unreal.TextureRenderTargetFormat.RTF_RGBA8)
    rt.set_editor_property("clear_color", unreal.LinearColor(0.05, 0.05, 0.06, 1.0))

    loc = unreal.Vector(-1650.0, -520.0, 160.0)
    target = unreal.Vector(250.0, 80.0, 15.0)
    rot = look_at(loc, target)
    log("camera %s %s" % (loc, rot))

    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.SceneCapture2D, loc, rot)
    actor.set_actor_label("LookDevCapture")
    comp = actor.get_component_by_class(unreal.SceneCaptureComponent2D)
    comp.set_editor_property("texture_target", rt)
    comp.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    comp.set_editor_property("fov_angle", 58.0)
    comp.set_editor_property("capture_every_frame", False)
    state["comp"] = comp
    state["rt"] = rt
    state["world"] = world
    log("capture component ready")


def tick(delta):
    state["t"] += delta
    if state["t"] > 12.0 and not state["captured"]:
        state["captured"] = True
        try:
            state["comp"].capture_scene()
            log("capture_scene called at t=%.1f" % state["t"])
        except Exception:
            log("capture EXC: " + traceback.format_exc())
    if state["t"] > 16.0 and not state["exported"]:
        state["exported"] = True
        try:
            ok = unreal.RenderingLibrary.export_render_target(
                state["world"], state["rt"], r"C:\Projects\ClayTennis\RawAssets", "lookdev.png")
            log("export ok=%s" % ok)
        except Exception:
            log("export EXC: " + traceback.format_exc())
    if state["t"] > 20.0 and not state["done"]:
        state["done"] = True
        try:
            with open(OUT, "w", encoding="utf-8") as f:
                f.write("t=%.1f captured=%s exported=%s\n" % (state["t"], state["captured"], state["exported"]))
        except Exception:
            log("write EXC: " + traceback.format_exc())
        log("quitting")
        unreal.SystemLibrary.quit_editor()


try:
    main()
    unreal.register_slate_post_tick_callback(tick)
    log("tick registered")
except Exception:
    log("MAIN EXC: " + traceback.format_exc())
    with open(OUT, "w", encoding="utf-8") as f:
        f.write("main failed\n" + traceback.format_exc())
    unreal.SystemLibrary.quit_editor()
