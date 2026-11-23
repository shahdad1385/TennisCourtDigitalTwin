import unreal
import math

loc = unreal.Vector(-1600.0, -700.0, 300.0)
target = unreal.Vector(150.0, 40.0, 5.0)
d = target - loc
yaw = math.degrees(math.atan2(d.y, d.x))
pitch = math.degrees(math.atan2(d.z, math.sqrt(d.x * d.x + d.y * d.y)))
rot = unreal.Rotator(pitch=pitch, yaw=yaw, roll=0.0)

unreal.EditorLevelLibrary.set_level_viewport_camera_info(loc, rot)
world = unreal.EditorLevelLibrary.get_editor_world()
for cmd in ("r.EyeAdaptation.ExposureCompensation -0.6", "HighResShot 1920x1080"):
    unreal.SystemLibrary.execute_console_command(world, cmd)

with open(r"C:\Projects\ClayTennis\RawAssets\shot_requested.txt", "w", encoding="utf-8") as f:
    f.write("loc=%s rot=%s\n" % (loc, rot))
