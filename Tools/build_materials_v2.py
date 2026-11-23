"""Materials v2 - single-sample clay with strong terracotta tint (no checker tiling)."""
import unreal
import os
import sys

sys.path.insert(0, r"C:\Projects\ClayTennis\Tools")
import ue_common as U

TEX = "/Game/Textures"


def tex(name):
    return unreal.EditorAssetLibrary.load_asset(TEX + "/" + name)


def build():
    for a in U.EAL.list_assets("/Game/Materials"):
        U.EAL.delete_asset(a)

    clay_d = tex("T_Clay_Diffuse")
    clay_r = tex("T_Clay_Roughness")
    clay_n = tex("T_Clay_Normal")
    line_d = tex("T_LinePaint_Albedo")
    line_n = tex("T_LinePaint_Normal")
    net_d = tex("T_Net_Albedo")
    net_n = tex("T_Net_Normal")
    ball_d = tex("T_Ball_Albedo")
    ball_n = tex("T_Ball_Normal")
    ball_r = tex("T_Ball_Rough")
    strings = tex("T_RacketStrings_Albedo")
    crowd = tex("T_Crowd_Albedo")
    logo = tex("T_Court_Logo_Albedo")

    # ---------------------------------------------------------------- clay
    m = U.new_material("M_ClaySurface")
    uv = U.world_uv(m, -1700, -420, 1.0 / 420.0)
    b = U.tex_param(m, "BaseTex", -1350, -420, default=clay_d)
    U.MEL.connect_material_expressions(uv, "", b, "UVs")
    bm = U._expr(m, unreal.MaterialExpressionComponentMask, -1100, -420)
    bm.set_editor_property("a", False)
    U.MEL.connect_material_expressions(b, "RGB", bm, "")
    tint = U.vector_param(m, "Tint", (1.0, 0.46, 0.28), -1100, -280)
    gain = U.scalar_param(m, "Gain", 1.55, -1100, -190)
    c1 = U.mul(m, bm, tint, -900, -380)
    c2 = U.mul(m, c1, gain, -730, -330)
    U.MEL.connect_material_property(c2, "", unreal.MaterialProperty.MP_BASE_COLOR)

    r = U.tex_param(m, "RoughTex", -1350, -60, default=clay_r)
    U.MEL.connect_material_expressions(uv, "", r, "UVs")
    rr = U.lerp(m, U.scalar_param(m, "RoughMin", 0.82, -1350, 80),
                U.scalar_param(m, "RoughMax", 0.97, -1350, 150), r, -1000, 80)
    U.MEL.connect_material_property(rr, "", unreal.MaterialProperty.MP_ROUGHNESS)

    if clay_n:
        n = U.tex_param(m, "NormalTex", -1350, 250, default=clay_n)
        n.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        U.MEL.connect_material_expressions(uv, "", n, "UVs")
        U.MEL.connect_material_property(n, "RGB", unreal.MaterialProperty.MP_NORMAL)
    U.compile_and_save(m)
    U.new_instance("MI_ClayCourt", m, textures={"BaseTex": clay_d, "RoughTex": clay_r},
                   scalars={"RoughMin": 0.82, "RoughMax": 0.97, "Gain": 1.55},
                   vectors={"Tint": (1.0, 0.46, 0.28)})
    U.new_instance("MI_ClayApron", m, textures={"BaseTex": clay_d, "RoughTex": clay_r},
                   scalars={"RoughMin": 0.85, "RoughMax": 0.98, "Gain": 1.35},
                   vectors={"Tint": (1.0, 0.52, 0.34)})

    # ---------------------------------------------------------------- lines
    m = U.new_material("M_LinePaint")
    base = U.vector_param(m, "BaseColor", (0.86, 0.85, 0.82), -800, -220)
    rough = U.scalar_param(m, "Roughness", 0.68, -800, -100)
    if line_d:
        uv = U.world_uv(m, -1200, 60, 1.0 / 90.0)
        t = U.tex_param(m, "PaintTex", -1000, 60, default=line_d)
        U.MEL.connect_material_expressions(uv, "", t, "UVs")
        dirty = U.mul(m, t, base, -600, -160)
        U.MEL.connect_material_property(dirty, "", unreal.MaterialProperty.MP_BASE_COLOR)
    else:
        U.MEL.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    U.MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    if line_n:
        n = U.tex_param(m, "NormalTex", -1000, 240, default=line_n)
        n.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        U.MEL.connect_material_property(n, "RGB", unreal.MaterialProperty.MP_NORMAL)
    U.compile_and_save(m)
    U.new_instance("MI_CourtLine", m)

    # ------------------------------------------------------------------ net
    m = U.new_material("M_Net", two_sided=True, blend=unreal.BlendMode.BLEND_MASKED)
    if net_d:
        uv = U._expr(m, unreal.MaterialExpressionTextureCoordinate, -1200, -300)
        uv.set_editor_property("utiling", 14.0)
        uv.set_editor_property("vtiling", 1.0)
        t = U.tex_param(m, "NetTex", -900, -300, default=net_d)
        U.MEL.connect_material_expressions(uv, "", t, "UVs")
        U.MEL.connect_material_property(t, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
        U.MEL.connect_material_property(t, "A", unreal.MaterialProperty.MP_OPACITY_MASK)
    U.MEL.connect_material_property(U.scalar_param(m, "Roughness", 0.7, -900, -100), "",
                                    unreal.MaterialProperty.MP_ROUGHNESS)
    if net_n:
        n = U.tex_param(m, "NormalTex", -900, 120, default=net_n)
        n.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        U.MEL.connect_material_property(n, "RGB", unreal.MaterialProperty.MP_NORMAL)
    U.compile_and_save(m)
    U.new_instance("MI_Net", m)

    # ----------------------------------------------------------------- ball
    m = U.new_material("M_TennisBall")
    if ball_d:
        U.MEL.connect_material_property(U.tex_param(m, "BallTex", -900, -300, default=ball_d), "RGB",
                                        unreal.MaterialProperty.MP_BASE_COLOR)
    if ball_r:
        U.MEL.connect_material_property(
            U.lerp(m, U.scalar_param(m, "RMin", 0.8, -900, 60),
                   U.scalar_param(m, "RMax", 0.95, -900, 130),
                   U.tex_param(m, "BallRough", -1100, 60, default=ball_r), -700, 80), "",
            unreal.MaterialProperty.MP_ROUGHNESS)
    if ball_n:
        n = U.tex_param(m, "NormalTex", -900, 260, default=ball_n)
        n.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        U.MEL.connect_material_property(n, "RGB", unreal.MaterialProperty.MP_NORMAL)
    U.compile_and_save(m)
    U.new_instance("MI_TennisBall", m)

    # -------------------------------------------------------------- strings
    m = U.new_material("M_RacketStrings", two_sided=True, blend=unreal.BlendMode.BLEND_MASKED)
    if strings:
        uv = U._expr(m, unreal.MaterialExpressionTextureCoordinate, -1000, -200)
        uv.set_editor_property("utiling", 2.4)
        uv.set_editor_property("vtiling", 3.0)
        t = U.tex_param(m, "StringTex", -750, -200, default=strings)
        U.MEL.connect_material_expressions(uv, "", t, "UVs")
        U.MEL.connect_material_property(t, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
        U.MEL.connect_material_property(t, "A", unreal.MaterialProperty.MP_OPACITY_MASK)
    U.MEL.connect_material_property(U.scalar_param(m, "Roughness", 0.5, -750, 0), "",
                                    unreal.MaterialProperty.MP_ROUGHNESS)
    U.compile_and_save(m)

    # ------------------------------------------------------------- simple
    def simple(name, color, rough, metal=0.0, two_sided=False):
        mm = U.new_material(name, two_sided=two_sided)
        U.MEL.connect_material_property(U.vector_param(mm, "BaseColor", color, -600, -200), "",
                                        unreal.MaterialProperty.MP_BASE_COLOR)
        U.MEL.connect_material_property(U.scalar_param(mm, "Roughness", rough, -600, -60), "",
                                        unreal.MaterialProperty.MP_ROUGHNESS)
        U.MEL.connect_material_property(U.scalar_param(mm, "Metallic", metal, -600, 60), "",
                                        unreal.MaterialProperty.MP_METALLIC)
        U.compile_and_save(mm)
        return U.new_instance("MI_" + name[2:], mm)

    simple("M_Carbon", (0.03, 0.032, 0.038), 0.3, 0.0)
    simple("M_Metal", (0.5, 0.51, 0.53), 0.22, 1.0)
    simple("M_Concrete", (0.22, 0.21, 0.2), 0.9)
    simple("M_Windscreen", (0.02, 0.075, 0.045), 0.92)
    simple("M_Hedge", (0.035, 0.10, 0.03), 0.95)
    simple("M_Tree", (0.025, 0.075, 0.03), 0.95)

    m = U.new_material("M_Crowd", two_sided=True)
    if crowd:
        U.MEL.connect_material_property(U.tex_param(m, "CrowdTex", -700, -200, default=crowd), "RGB",
                                        unreal.MaterialProperty.MP_BASE_COLOR)
    U.MEL.connect_material_property(U.scalar_param(m, "Roughness", 1.0, -700, 0), "",
                                    unreal.MaterialProperty.MP_ROUGHNESS)
    U.compile_and_save(m)
    U.new_instance("MI_Crowd", m)

    m = U.new_material("M_Floodlight", two_sided=True,
                       shading=unreal.MaterialShadingModel.MSM_UNLIT)
    U.MEL.connect_material_property(U.vector_param(m, "EmitColor", (1.0, 0.95, 0.85), -600, -200), "",
                                    unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    U.compile_and_save(m)
    U.new_instance("MI_Floodlight", m)

    if logo:
        m = U.new_material("M_CourtLogo", two_sided=True, blend=unreal.BlendMode.BLEND_MASKED)
        t = U.tex_param(m, "LogoTex", -700, -200, default=logo)
        U.MEL.connect_material_property(t, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
        U.MEL.connect_material_property(t, "A", unreal.MaterialProperty.MP_OPACITY_MASK)
        U.compile_and_save(m)
        U.new_instance("MI_CourtLogo", m)

    unreal.log("[MAT] materials v2 done")


build()
