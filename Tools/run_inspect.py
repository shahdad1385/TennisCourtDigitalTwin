"""Run inspect_lines.py in the editor, then quit."""
import unreal
import runpy

runpy.run_path(r"C:\Projects\ClayTennis\Tools\inspect_character.py", run_name="__main__")
unreal.SystemLibrary.quit_editor()
