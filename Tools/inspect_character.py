"""Inspect the imported character part meshes: class + bounds. Read-only."""
import unreal

OUT = r"C:\Projects\ClayTennis\RawAssets\inspect_char_result.txt"
RES = []

for p in unreal.EditorAssetLibrary.list_assets("/Game/Sprites", recursive=False):
    obj = unreal.EditorAssetLibrary.load_asset(p.split(".")[0])
    if obj is None:
        continue
    cls = obj.get_class().get_name()
    if cls in ("StaticMesh", "SkeletalMesh"):
        b = obj.get_bounds()
        RES.append("%s | %s | origin=(%.1f, %.1f, %.1f) extent=(%.1f, %.1f, %.1f)"
                   % (p, cls, b.origin.x, b.origin.y, b.origin.z,
                      b.box_extent.x, b.box_extent.y, b.box_extent.z))
    else:
        RES.append("%s | %s" % (p, cls))

with open(OUT, "w", encoding="utf-8") as f:
    f.write("\n".join(RES))

unreal.log("[INSPECTCHAR] " + RES[-1][:160])
