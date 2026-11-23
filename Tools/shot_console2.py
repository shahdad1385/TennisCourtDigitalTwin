import time
import math
import unreal

STATE = {"t0": None, "done": False}
MARK = r"C:\Projects\ClayTennis\RawAssets\shot_requested.txt"


def look_at(src, dst):
    d = dst - src
    yaw = math.degrees(math.atan2(d.y, d.x))
    pitch = math.degrees(math.atan2(d.z, math.sqrt(d.x * d.x + d.y * d.y)))
    return unreal.Rotator(pitch=pitch, yaw=yaw, roll=0.0)


loc = unreal.Vector(-1600.0, -700.0, 300.0)
rot = look_at(loc, unreal.Vector(150.0, 40.0, 5.0))

try:
    unreal.EditorLevelLibrary.set_level_viewport_camera_info(loc, rot)
    unreal.log("[SHOT2] viewport camera set")
except Exception as exc:
    unreal.log("[SHOT2] camera failed: %s" % exc)


def tick(delta):
    if STATE["t0"] is None:
        STATE["t0"] = time.time()
        return
    elapsed = time.time() - STATE["t0"]
    if elapsed > 10.0 and not STATE["done"]:
        STATE["done"] = True
        world = unreal.EditorLevelLibrary.get_editor_world()
        for cmd in ("r.EyeAdaptation.ExposureCompensation -0.6", "HighResShot 1920x1080"):
            try:
                unreal.SystemLibrary.execute_console_command(world, cmd)
            except Exception as exc:
                unreal.log("[SHOT2] cmd failed: %s" % exc)
        with open(MARK, "w", encoding="utf-8") as f:
            f.write("requested at %.1fs\n" % elapsed)
        unreal.log("[SHOT2] screenshot requested at %.1fs" % elapsed)


unreal.register_slate_post_tick_callback(tick)
