"""Minimal, robust materials v4: plain texture sample + params only (no node math)."""
import unreal
import sys

sys.path.insert(0, r"C:\Projects\ClayTennis\Tools")
import ue_common as U

T = "/Game/Textures"


def tex(n):
    return unreal.EditorAssetLibrary.load_asset(T + "/" + n)


def uv_node(mat, u, v, x=-1200, y=-300):
    node = U._expr(mat, unreal.MaterialExpressionTextureCoordinate, x, y)
    node.set_editor_property("utiling", float(u))
    node.set_editor_property("vtiling", float(v))
    return node


def build():
    for a in U.EAL.list_assets("/Game/Materials"):
        U.EAL.delete_asset(a)

    alb = tex("T_Clay2_Albedo")
    rgh = tex("T_Clay2_Rough")
    nrm = tex("T_Clay2_Normal")

    def clay_material(name, u, v):
        m = U.new_material(name)
        uv = uv_node(m, u, v)
        b = U.tex_param(m, "BaseTex", -950, -320, default=alb)
        U.MEL.connect_material_expressions(uv, "", b, "UVs")
        U.MEL.connect_material_property(b, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)

        r = U.tex_param(m, "RoughTex", -950, -80, default=rgh)
        r.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        U.MEL.connect_material_expressions(uv, "", r, "UVs")
        U.MEL.connect_material_property(r, "R", unreal.MaterialProperty.MP_ROUGHNESS)

        if nrm:
            n = U.tex_param(m, "NormalTex", -950, 150, default=nrm)
            n.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
            U.MEL.connect_material_expressions(uv, "", n, "UVs")
            U.MEL.connect_material_property(n, "RGB", unreal.MaterialProperty.MP_NORMAL)
        U.compile_and_save(m)
        return m

    m = clay_material("M_ClaySurface", 5.0, 2.3)
    U.new_instance("MI_ClayCourt", m, textures={"BaseTex": alb, "RoughTex": rgh})
    U.new_instance("MI_ClayApron", m, textures={"BaseTex": alb, "RoughTex": rgh})

    def const_mat(name, color, rough, metal=0.0, two_sided=False):
        mm = U.new_material(name, two_sided=two_sided)
        U.MEL.connect_material_property(U.vector_param(mm, "BaseColor", color, -500, -200), "",
                                        unreal.MaterialProperty.MP_BASE_COLOR)
        U.MEL.connect_material_property(U.scalar_param(mm, "Roughness", rough, -500, -60), "",
                                        unreal.MaterialProperty.MP_ROUGHNESS)
        if metal:
            U.MEL.connect_material_property(U.scalar_param(mm, "Metallic", metal, -500, 60), "",
                                            unreal.MaterialProperty.MP_METALLIC)
        U.compile_and_save(mm)
        return U.new_instance("MI_" + name[2:], mm)

    const_mat("M_LinePaint", (0.88, 0.87, 0.84), 0.66)
    const_mat("M_Carbon", (0.03, 0.032, 0.038), 0.3)
    const_mat("M_Metal", (0.55, 0.56, 0.58), 0.25, 1.0)
    const_mat("M_Concrete", (0.22, 0.21, 0.2), 0.9)
    const_mat("M_Windscreen", (0.02, 0.07, 0.04), 0.9, 0.0, True)
    const_mat("M_Hedge", (0.03, 0.09, 0.025), 0.95)
    const_mat("M_Tree", (0.02, 0.07, 0.025), 0.95)

    # net: masked texture
    nd = tex("T_Net_Albedo")
    mn = U.new_material("M_Net", two_sided=True, blend=unreal.BlendMode.BLEND_MASKED)
    if nd:
        uv = uv_node(mn, 14.0, 1.0)
        t = U.tex_param(mn, "NetTex", -900, -300, default=nd)
        t.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        U.MEL.connect_material_expressions(uv, "", t, "UVs")
        U.MEL.connect_material_property(t, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
        U.MEL.connect_material_property(t, "A", unreal.MaterialProperty.MP_OPACITY_MASK)
    U.MEL.connect_material_property(U.scalar_param(mn, "Roughness", 0.7, -900, -100), "",
                                    unreal.MaterialProperty.MP_ROUGHNESS)
    U.compile_and_save(mn)
    U.new_instance("MI_Net", mn)

    # ball
    bd = tex("T_Ball_Albedo")
    mb = U.new_material("M_TennisBall")
    if bd:
        U.MEL.connect_material_property(U.tex_param(mb, "BallTex", -700, -200, default=bd), "RGB",
                                        unreal.MaterialProperty.MP_BASE_COLOR)
    U.MEL.connect_material_property(U.scalar_param(mb, "Roughness", 0.85, -700, 0), "",
                                    unreal.MaterialProperty.MP_ROUGHNESS)
    U.compile_and_save(mb)
    U.new_instance("MI_TennisBall", mb)

    # strings
    sd = tex("T_RacketStrings_Albedo")
    ms = U.new_material("M_RacketStrings", two_sided=True, blend=unreal.BlendMode.BLEND_MASKED)
    if sd:
        uv = uv_node(ms, 2.4, 3.0, -900, -200)
        t = U.tex_param(ms, "StringTex", -650, -200, default=sd)
        t.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        U.MEL.connect_material_expressions(uv, "", t, "UVs")
        U.MEL.connect_material_property(t, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
        U.MEL.connect_material_property(t, "A", unreal.MaterialProperty.MP_OPACITY_MASK)
    U.compile_and_save(ms)

    # floodlight emissive
    mf = U.new_material("M_Floodlight", two_sided=True,
                        shading=unreal.MaterialShadingModel.MSM_UNLIT)
    U.MEL.connect_material_property(U.vector_param(mf, "EmitColor", (5.0, 4.7, 4.2), -500, -200), "",
                                    unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    U.compile_and_save(mf)
    U.new_instance("MI_Floodlight", mf)

    # crowd
    cd = tex("T_Crowd_Albedo")
    mc = U.new_material("M_Crowd", two_sided=True)
    if cd:
        U.MEL.connect_material_property(U.tex_param(mc, "CrowdTex", -700, -200, default=cd), "RGB",
                                        unreal.MaterialProperty.MP_BASE_COLOR)
    U.compile_and_save(mc)
    U.new_instance("MI_Crowd", mc)

    lg = tex("T_Court_Logo_Albedo")
    if lg:
        ml = U.new_material("M_CourtLogo", two_sided=True, blend=unreal.BlendMode.BLEND_MASKED)
        t = U.tex_param(ml, "LogoTex", -600, -200, default=lg)
        t.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        U.MEL.connect_material_property(t, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
        U.MEL.connect_material_property(t, "A", unreal.MaterialProperty.MP_OPACITY_MASK)
        U.compile_and_save(ml)
        U.new_instance("MI_CourtLogo", ml)

    unreal.log("[MAT4] minimal materials built")


build()
