"""Clay material variant 2: base texture + constant roughness + optional normal."""
import unreal
import sys

sys.path.insert(0, r"C:\Projects\ClayTennis\Tools")
import ue_common as U

MODE = "normal"  # "plain" | "normal"

mat = unreal.EditorAssetLibrary.load_asset("/Game/Materials/M_ClaySurface")
alb = unreal.EditorAssetLibrary.load_asset("/Game/Textures/T_Clay2_Albedo")
nrm = unreal.EditorAssetLibrary.load_asset("/Game/Textures/T_Clay2_Normal")

U.MEL.delete_all_material_expressions(mat)

b = U.tex_param(mat, "BaseTex", -700, -250, default=alb)
U.MEL.connect_material_property(b, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)

r = U.scalar_param(mat, "Roughness", 0.92, -700, 20)
U.MEL.connect_material_property(r, "", unreal.MaterialProperty.MP_ROUGHNESS)

if MODE == "normal":
    n = U.tex_param(mat, "NormalTex", -700, 240, default=nrm)
    pass
    U.MEL.connect_material_property(n, "RGB", unreal.MaterialProperty.MP_NORMAL)

U.compile_and_save(mat)
unreal.log("[VARIANT2] mode=%s" % MODE)

