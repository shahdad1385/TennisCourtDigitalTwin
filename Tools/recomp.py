import unreal
m = unreal.EditorAssetLibrary.load_asset("/Game/Materials/M_ClaySurface")
unreal.MaterialEditingLibrary.recompile_material(m)
unreal.log("[RECOMP] done")
unreal.SystemLibrary.quit_editor()
