"""Materials v5: in-place clay (base+rough+normal, no UV nodes) + simple supporting materials."""
import unreal
import sys

sys.path.insert(0, r"C:\Projects\ClayTennis\Tools")
import ue_common as U

T = "/Game/Textures"


def tex(n):
    return unreal.EditorAssetLibrary.load_asset(T + "/" + n)


def build():
    alb, rgh, nrm = tex("T_Clay2_Albedo"), tex("T_Clay2_Rough"), tex("T_Clay2_Normal")

    # clay: edit in place so existing level references stay valid
    mat = unreal.EditorAssetLibrary.load_asset("/Game/Materials/M_ClaySurface")
    U.MEL.delete_all_material_expressions(mat)
    b = U.tex_param(mat, "BaseTex", -700, -250, default=alb)
    U.MEL.connect_material_property(b, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    r = U.tex_param(mat, "RoughTex", -700, 30, default=rgh)
    U.MEL.connect_material_property(r, "R", unreal.MaterialProperty.MP_ROUGHNESS)
    n = U.tex_param(mat, "NormalTex", -700, 260, default=nrm)
    n.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    U.MEL.connect_material_property(n, "RGB", unreal.MaterialProperty.MP_NORMAL)
    U.compile_and_save(mat)

    for name in ("MI_ClayCourt", "MI_ClayApron"):
        p = "/Game/Materials/" + name
        if unreal.EditorAssetLibrary.does_asset_exist(p):
            mi = unreal.EditorAssetLibrary.load_asset(p)
        else:
            mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
                name, "/Game/Materials", unreal.MaterialInstanceConstant,
                unreal.MaterialInstanceConstantFactoryNew())
        U.MEL.set_material_instance_parent(mi, mat)
        U.MEL.set_material_instance_texture_parameter_value(mi, "BaseTex", alb)
        U.MEL.set_material_instance_texture_parameter_value(mi, "RoughTex", rgh)
        U.MEL.set_material_instance_texture_parameter_value(mi, "NormalTex", nrm)
        U.MEL.update_material_instance(mi)
        U.EAL.save_loaded_asset(mi, False)

    def ensure(name, color, rough, metal=0.0, two_sided=False, emissive=None, unlit=False):
        p = "/Game/Materials/" + name
        if unreal.EditorAssetLibrary.does_asset_exist(p):
            m = unreal.EditorAssetLibrary.load_asset(p)
            U.MEL.delete_all_material_expressions(m)
        else:
            m = U.new_material(name, two_sided=two_sided,
                               shading=unreal.MaterialShadingModel.MSM_UNLIT if unlit else None)
        if emissive:
            U.MEL.connect_material_property(
                U.vector_param(m, "EmitColor", emissive, -500, -200), "",
                unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        else:
            U.MEL.connect_material_property(
                U.vector_param(m, "BaseColor", color, -500, -200), "",
                unreal.MaterialProperty.MP_BASE_COLOR)
        if not unlit:
            U.MEL.connect_material_property(
                U.scalar_param(m, "Roughness", rough, -500, -60), "",
                unreal.MaterialProperty.MP_ROUGHNESS)
            if metal:
                U.MEL.connect_material_property(
                    U.scalar_param(m, "Metallic", metal, -500, 60), "",
                    unreal.MaterialProperty.MP_METALLIC)
        U.compile_and_save(m)
        inst = "/Game/Materials/MI_" + name[2:]
        if not unreal.EditorAssetLibrary.does_asset_exist(inst):
            U.new_instance("MI_" + name[2:], m)

    ensure("M_LinePaint", (0.88, 0.87, 0.84), 0.66)
    ensure("M_NetLine", (0.035, 0.035, 0.04), 0.72)
    ensure("M_Carbon", (0.03, 0.032, 0.038), 0.3)
    ensure("M_Metal", (0.55, 0.56, 0.58), 0.25, 1.0)
    ensure("M_Concrete", (0.22, 0.21, 0.2), 0.9)
    ensure("M_Windscreen", (0.02, 0.07, 0.04), 0.9, 0.0, True)
    ensure("M_Hedge", (0.03, 0.09, 0.025), 0.95)
    ensure("M_Tree", (0.02, 0.07, 0.025), 0.95)
    ensure("M_Floodlight", None, 0, emissive=(6.0, 5.6, 5.0), unlit=True, two_sided=True)

    # ball texture material
    bd = tex("T_Ball_Albedo")
    p = "/Game/Materials/M_TennisBall"
    if unreal.EditorAssetLibrary.does_asset_exist(p):
        mb = unreal.EditorAssetLibrary.load_asset(p)
        U.MEL.delete_all_material_expressions(mb)
    else:
        mb = U.new_material("M_TennisBall")
    if bd:
        U.MEL.connect_material_property(U.tex_param(mb, "BallTex", -600, -200, default=bd), "RGB",
                                        unreal.MaterialProperty.MP_BASE_COLOR)
    U.MEL.connect_material_property(U.scalar_param(mb, "Roughness", 0.85, -600, 0), "",
                                    unreal.MaterialProperty.MP_ROUGHNESS)
    U.compile_and_save(mb)
    if not unreal.EditorAssetLibrary.does_asset_exist("/Game/Materials/MI_TennisBall"):
        U.new_instance("MI_TennisBall", mb)

    unreal.log("[MAT5] done")


build()
