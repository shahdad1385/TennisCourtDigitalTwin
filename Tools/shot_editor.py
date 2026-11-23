"""Render a look-dev screenshot of the court from the editor viewport."""
import unreal
import math

OUT = r"C:\Projects\ClayTennis\RawAssets\shot_editor_result.txt"
RES = []


def note(s):
    RES.append(str(s))


def look_at(src, dst):
    d = dst - src
    yaw = math.degrees(math.atan2(d.y, d.x))
    pitch = math.degrees(math.atan2(d.z, math.sqrt(d.x * d.x + d.y * d.y)))
    return unreal.Rotator(pitch, yaw, 0)


try:
    loc = unreal.Vector(-1750.0, -420.0, 165.0)
    target = unreal.Vector(200.0, 60.0, 20.0)
    rot = look_at(loc, target)
    unreal.EditorLevelLibrary.set_level_viewport_camera_info(loc, rot)
    note("camera %s %s" % (loc, rot))
except Exception as exc:
    note("camera failed: %s" % exc)

_state = {"n": 0}


def _tick(delta):
    _state["n"] += 1
    if _state["n"] == 150:
        try:
            unreal.AutomationLibrary.take_high_res_screenshot(1920, 1080, "lookdev")
            note("screenshot requested")
        except Exception as exc:
            note("screenshot failed: %s" % exc)
    if _state["n"] == 300:
        with open(OUT, "w", encoding="utf-8") as f:
            f.write("\n".join(RES))
        unreal.SystemLibrary.quit_editor()


unreal.register_slate_post_tick_callback(_tick)
