"""Import the user's realistic male character GLB as SM_PlayerMale.
Runs inside the editor; every step is reported to a result file."""
import unreal
import traceback

OUT = r"C:\Projects\ClayTennis\RawAssets\import_character_result.txt"
RES = []


def note(s):
    RES.append(str(s))
    try:
        with open(OUT, "w", encoding="utf-8") as f:
            f.write("\n".join(RES))
    except Exception:
        pass


GLB = r"C:\Projects\ClayTennis\RawAssets\Sprites\character\realistic_male_character.glb"
DEST = "/Game/Sprites"
NAME = "SM_PlayerMale"

try:
    at = unreal.AssetToolsHelpers.get_asset_tools()

    # A stale asset blocks the rename; drop it first.
    if unreal.EditorAssetLibrary.does_asset_exist(DEST + "/" + NAME):
        note("removed stale " + NAME)
        unreal.EditorAssetLibrary.delete_asset(DEST + "/" + NAME)

    task = unreal.AssetImportTask()
    task.filename = GLB
    task.destination_path = DEST
    task.destination_name = NAME
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.options = None
    at.import_asset_tasks([task])

    # Resolve what Interchange actually created (name is not deterministic).
    created = []
    if task.imported_object_paths:
        for p in task.imported_object_paths:
            created.append(p)

    candidates = [p for p in created if ".StaticMesh" in p or ".SkeletalMesh" in p]
    note("imported_object_paths: " + "; ".join(created))

    mesh = None
    for p in candidates:
        obj = unreal.EditorAssetLibrary.load_asset(p.split(".")[0])
        if obj is not None:
            mesh = obj
            break
    if mesh is None:
        # Last resort: scan the folder for a mesh whose name matches the file.
        for a in unreal.EditorAssetLibrary.list_assets(DEST):
            if NAME.lower() in a.lower() and (".StaticMesh" in a or ".SkeletalMesh" in a):
                mesh = unreal.EditorAssetLibrary.load_asset(a.split(".")[0])
                if mesh:
                    break

    if mesh is None:
        note("failed: no mesh resolved after import")
    else:
        real_name = mesh.get_name()
        real_path = mesh.get_path_name().split(".")[0]
        if real_name != NAME:
            new_path = DEST + "/" + NAME
            if unreal.EditorAssetLibrary.does_asset_exist(new_path):
                unreal.EditorAssetLibrary.delete_asset(new_path)
            dup = unreal.EditorAssetLibrary.duplicate_asset(real_path, new_path)
            if dup:
                note("duplicated %s -> %s" % (real_path, new_path))
            else:
                note("failed: rename/duplicate to " + NAME)
        else:
            note("imported: " + real_path)

        cls = mesh.get_class().get_name()
        note("mesh class: " + cls)
    # The GLB imports as 8 body-part meshes sharing one local frame. Rename
    # them to stable names so C++ can attach them all.
    parts = []
    for p in unreal.EditorAssetLibrary.list_assets(DEST):
        base = p.split(".")[0]
        obj = unreal.EditorAssetLibrary.load_asset(base)
        if obj is not None and obj.get_class().get_name() == "StaticMesh" \
                and "tripo_node" in base.lower():
            parts.append(base)
    parts.sort()
    for idx, src_path in enumerate(parts):
        dst_path = "%s/SM_PlayerPart_%02d" % (DEST, idx)
        if unreal.EditorAssetLibrary.does_asset_exist(dst_path):
            unreal.EditorAssetLibrary.delete_asset(dst_path)
        if unreal.EditorAssetLibrary.rename_asset(src_path, dst_path):
            note("renamed part: %s -> SM_PlayerPart_%02d" % (src_path, idx))
        else:
            note("failed rename: " + src_path)
    note("character parts renamed: %d" % len(parts))
except Exception:
    note("EXC\n" + traceback.format_exc())

unreal.log("[IMPCHAR] " + RES[-1][:160])
