import time
import unreal

T0 = {"t": None}
DONE = {"v": False}


def tick(delta):
    if T0["t"] is None:
        T0["t"] = time.time()
        unreal.log("[SHOTD] tick started")
        return
    elapsed = time.time() - T0["t"]
    if elapsed > 40.0 and not DONE["v"]:
        DONE["v"] = True
        world = unreal.EditorLevelLibrary.get_editor_world()
        try:
            unreal.SystemLibrary.execute_console_command(world, "HighResShot 1920x1080")
            unreal.log("[SHOTD] HighResShot at %.1f" % elapsed)
        except Exception as exc:
            unreal.log("[SHOTD] failed: %s" % exc)
        with open(r"C:\Projects\ClayTennis\RawAssets\shot_delay.txt", "w", encoding="utf-8") as f:
            f.write("elapsed=%.1f\n" % elapsed)


unreal.register_slate_post_tick_callback(tick)
unreal.log("[SHOTD] registered")
