"""Feasibility smoke test: proves Python scripting works in this engine build."""
import unreal

def main():
    version = unreal.SystemLibrary.get_engine_version()
    project = unreal.Paths.project_dir()
    unreal.log("[SMOKE] engine_version=" + version)
    unreal.log("[SMOKE] project_dir=" + project)
    unreal.log("[SMOKE] is_editor=" + str(unreal.is_editor()))

    out = unreal.Paths.project_content_dir() + "../RawAssets/smoke_ok.txt"
    with open(unreal.Paths.convert_relative_path_to_full(out), "w") as f:
        f.write("ok " + version + "\n")
    unreal.log("[SMOKE] wrote marker file")

main()
