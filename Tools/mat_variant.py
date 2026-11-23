"""Edit M_ClaySurface in place (no asset delete) with a minimal variant, then test."""
import unreal
import sys

sys.path.insert(0, r"C:\Projects\ClayTennis\Tools")
import ue_common as U

STAGE = "bc"

mat = unreal.EditorAssetLibrary.load_asset("/Game/Materials/M_ClaySurface")
alb = unreal.EditorAssetLibrary.load_asset("/Game/Textures/T_Clay2_Albedo")
rgh = unreal.EditorAssetLibrary.load_asset("/Game/Textures/T_Clay2_Rough")
nrm = unreal.EditorAssetLibrary.load_asset("/Game/Textures/T_Clay2_Normal")

U.MEL.delete_all_material_expressions(mat)

# stage a: plain texture -> base colour
b = U.tex_param(mat, "BaseTex", -600, -200, default=alb)
U.MEL.connect_material_property(b, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)

if STAGE in ("b", "bc", "c", "d"):
    r = U.tex_param(mat, "RoughTex", -600, 60, default=rgh)
    U.MEL.connect_material_property(r, "R", unreal.MaterialProperty.MP_ROUGHNESS)

if STAGE in ("c", "d"):
    n = U.tex_param(mat, "NormalTex", -600, 280, default=nrm)
    n.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    U.MEL.connect_material_property(n, "RGB", unreal.MaterialProperty.MP_NORMAL)

if STAGE == "d":
    tc = U._expr(mat, unreal.MaterialExpressionTextureCoordinate, -1100, -200)
    tc.set_editor_property("utiling", 5.0)
    tc.set_editor_property("vtiling", 2.3)
    U.MEL.connect_material_expressions(tc, "", b, "UVs")

U.compile_and_save(mat)
unreal.log("[VARIANT] stage=%s" % STAGE)
