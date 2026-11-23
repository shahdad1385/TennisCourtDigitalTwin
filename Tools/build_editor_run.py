"""Run the level builder inside the editor (maps can only be saved with an editor world)."""
import unreal
import runpy

runpy.run_path(r"C:\Projects\ClayTennis\Tools\build_level_v3.py", run_name="__main__")

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
out = []
try:
    out.append("current_level=%s" % les.get_current_level())
except Exception as exc:
    out.append("get_current_level: %s" % exc)
try:
    out.append("save_current_level=%s" % les.save_current_level())
except Exception as exc:
    out.append("save_current_level EXC: %s" % exc)
try:
    out.append("save_asset=%s" % unreal.EditorAssetLibrary.save_asset("/Game/Maps/TennisCourt"))
except Exception as exc:
    out.append("save_asset EXC: %s" % exc)

with open(r"C:\Projects\ClayTennis\RawAssets\save_result.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(out))

unreal.SystemLibrary.quit_editor()
