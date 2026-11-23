import unreal, os, traceback
out=[]
try:
    src=r"C:\Projects\ClayTennis\RawAssets\Textures\T_Net_Albedo.png"
    out.append("exists="+str(os.path.isfile(src)))
    t=unreal.AssetImportTask()
    t.filename=src
    t.destination_path="/Game/Textures"
    t.destination_name="T_Net_Albedo"
    t.automated=True
    t.replace_existing=True
    t.save=True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
    out.append("imported_paths="+str(t.imported_object_paths))
    a=unreal.EditorAssetLibrary.load_asset("/Game/Textures/T_Net_Albedo")
    out.append("loaded="+str(a))
    if a:
        out.append("alpha="+str(a.get_editor_property("has_alpha")))
    out.append("list="+",".join([x.split(".")[-1] for x in unreal.EditorAssetLibrary.list_assets("/Game/Textures")]))
except Exception:
    out.append("EXC\n"+traceback.format_exc())
open(r"C:\Projects\ClayTennis\RawAssets\test_import.txt","w").write("\n".join(out))
