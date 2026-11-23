"""Import the user's realistic tennis racquet GLB as SM_RacketReal (+ parts).
Runs inside the editor; every step is reported to a result file."""
import os
import shutil
import traceback
import unreal

OUT = r"C:\Projects\ClayTennis\RawAssets\import_racket_real_result.txt"
RES = []


def note(s):
    RES.append(str(s))
    try:
        with open(OUT, "w", encoding="utf-8") as f:
            f.write("\n".join(RES))
    except Exception:
        pass


SRC = r"C:\Users\shahd\Desktop\Shahdad\Code\TennisRobotDigitalTwin\tennis_racquet.glb"
GLB = r"C:\Projects\ClayTennis\RawAssets\Sprites\racket_real\tennis_racquet.glb"
DEST = "/Game/Sprites"
NAME = "SM_RacketReal"

try:
    # Copy the source GLB into RawAssets (skip if already there).
    if not os.path.exists(SRC):
        note("failed: source GLB not found: " + SRC)
    else:
        os.makedirs(os.path.dirname(GLB), exist_ok=True)
        if os.path.exists(GLB):
            note("copied: skipped (already exists) " + GLB)
        else:
            shutil.copyfile(SRC, GLB)
            note("copied: %s (%d bytes)" % (GLB, os.path.getsize(GLB)))

        at = unreal.AssetToolsHelpers.get_asset_tools()

        task = unreal.AssetImportTask()
        task.filename = GLB
        task.destination_path = DEST
        task.destination_name = NAME
        task.automated = True
        task.replace_existing = True
        task.save = True
        task.options = None
        at.import_asset_tasks([task])

        # Resolve what Interchange actually created. The GLB may come in as
        # several static meshes (multi-part like the character GLB was), so
        # collect ALL of them in import order.
        meshes = []
        created = []
        if task.imported_object_paths:
            for p in task.imported_object_paths:
                created.append(p)
        note("imported_object_paths: " + "; ".join(created))
        for p in created:
            obj = unreal.EditorAssetLibrary.load_asset(p.split(".")[0])
            if obj is not None and obj.get_class().get_name() == "StaticMesh":
                meshes.append(obj)
        note("static meshes resolved: %d" % len(meshes))

        if not meshes:
            note("failed: no StaticMesh resolved after import")
        else:
            # First mesh -> SM_RacketReal, the rest -> SM_RacketReal_1, _2, ...
            for idx, mesh in enumerate(meshes):
                target = NAME if idx == 0 else "%s_%d" % (NAME, idx)
                cur_path = mesh.get_path_name().split(".")[0]
                dst_path = DEST + "/" + target
                if mesh.get_name() == target:
                    note("imported: " + cur_path + " (already named " + target + ")")
                    continue
                if unreal.EditorAssetLibrary.does_asset_exist(dst_path):
                    note("removed stale " + target)
                    unreal.EditorAssetLibrary.delete_asset(dst_path)
                if unreal.EditorAssetLibrary.rename_asset(cur_path, dst_path):
                    note("renamed: %s -> %s" % (cur_path, dst_path))
                else:
                    note("failed: rename " + cur_path + " -> " + dst_path)

            # Report bounds of every final mesh so C++ can orient/normalize.
            for idx in range(len(meshes)):
                target = NAME if idx == 0 else "%s_%d" % (NAME, idx)
                final_path = DEST + "/" + target
                final = unreal.EditorAssetLibrary.load_asset(final_path)
                if final is None:
                    note("failed: final load " + target)
                    continue
                b = final.get_bounds()  # unreal.BoxSphereBounds
                o = b.origin
                e = b.box_extent
                note("bounds: %s origin=(%.3f, %.3f, %.3f) box_extent=(%.3f, %.3f, %.3f)"
                     % (target, o.x, o.y, o.z, e.x, e.y, e.z))
except Exception:
    note("EXC\n" + traceback.format_exc())

unreal.log("[IMPRKT] " + RES[-1].replace("\n", " ")[:160])
