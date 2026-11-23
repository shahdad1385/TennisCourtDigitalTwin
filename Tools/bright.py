import unreal, sys
sys.path.insert(0, r"C:\Projects\ClayTennis\Tools")
import ue_common as U
for name, color in (("M_Windscreen", (0.05, 0.13, 0.07)), ("M_Hedge", (0.05, 0.12, 0.04)), ("M_Tree", (0.04, 0.11, 0.04))):
    m = unreal.EditorAssetLibrary.load_asset("/Game/Materials/" + name)
    U.MEL.delete_all_material_expressions(m)
    U.MEL.connect_material_property(U.vector_param(m, "BaseColor", color, -500, -200), "", unreal.MaterialProperty.MP_BASE_COLOR)
    U.MEL.connect_material_property(U.scalar_param(m, "Roughness", 0.95, -500, -40), "", unreal.MaterialProperty.MP_ROUGHNESS)
    U.compile_and_save(m)
unreal.log("[BRIGHT] done")
