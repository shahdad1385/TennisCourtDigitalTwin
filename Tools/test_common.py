import sys, os, traceback
sys.path.insert(0, r"C:\Projects\ClayTennis\Tools")
import unreal, ue_common as U
out=[]
src=r"C:\Projects\ClayTennis\RawAssets\Textures\LinePaint_Albedo.png"
out.append("isfile=%s" % os.path.isfile(src))
out.append("PROC exists=%s" % os.path.isdir(r"C:\Projects\ClayTennis\RawAssets\Textures"))
try:
    t=U.import_texture(src, "/Game/Textures", "T_LinePaint_Albedo", srgb=True)
    out.append("returned=%s" % t)
except Exception:
    out.append("EXC\n"+traceback.format_exc())
out.append("listed="+",".join([x.split(".")[-1] for x in unreal.EditorAssetLibrary.list_assets("/Game/Textures")]))
open(r"C:\Projects\ClayTennis\RawAssets\test_common.txt","w").write("\n".join(out))
