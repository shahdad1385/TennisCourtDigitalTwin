import json
import math
import unreal

CFG = r"C:\Projects\ClayTennis\RawAssets\shot_cfg.json"
with open(CFG, "r", encoding="utf-8") as f:
    cfg = json.load(f)

loc = unreal.Vector(*cfg["loc"])
target = unreal.Vector(*cfg["target"])
d = target - loc
yaw = math.degrees(math.atan2(d.y, d.x))
pitch = math.degrees(math.atan2(d.z, math.sqrt(d.x * d.x + d.y * d.y)))
rot = unreal.Rotator(pitch=pitch, yaw=yaw, roll=0.0)

unreal.EditorLevelLibrary.set_level_viewport_camera_info(loc, rot)
world = unreal.EditorLevelLibrary.get_editor_world()

for cmd in cfg.get("commands", []):
    unreal.SystemLibrary.execute_console_command(world, cmd)

unreal.SystemLibrary.execute_console_command(world, "HighResShot 1920x1080")
with open(r"C:\Projects\ClayTennis\RawAssets\shot_requested.txt", "w", encoding="utf-8") as f:
    f.write(json.dumps(cfg) + "\n")
