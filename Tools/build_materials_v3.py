"""Import clay v2 textures and build materials v3."""
import unreal
import os
import sys

sys.path.insert(0, r"C:\Projects\ClayTennis\Tools")
import ue_common as U

RAW = r"C:\Projects\ClayTennis\RawAssets\Clay2"
TEX = "/Game/Textures"


def imp(src, name, srgb=True, normal=False, flip_green=False):
    tex = U.import_texture(os.path.join(RAW, src), TEX, name, srgb=srgb, normal=normal)
    if tex and normal and flip_green:
        try:
            tex.set_editor_property("flip_green_channel", True)
            unreal.EditorAssetLibrary.save_loaded_asset(tex, False)
        except Exception as exc:
            unreal.log("[IMP] flip green failed: %s" % exc)
    return tex


def build():
    alb = imp("T_Clay2_Albedo.png", "T_Clay2_Albedo")
    rough = imp("T_Clay2_Rough.png", "T_Clay2_Rough", srgb=False)
    norm = imp("T_Clay2_Normal.png", "T_Clay2_Normal", srgb=False, normal=True, flip_green=True)
    macro = imp("T_Clay2_Macro.png", "T_Clay2_Macro", srgb=False)
    line = imp("T_Line2_Albedo.png", "T_Line2_Albedo")

    for a in U.EAL.list_assets("/Game/Materials"):
        U.EAL.delete_asset(a)

    m = U.new_material("M_ClaySurface")
    uv = U.world_uv(m, -1800, -400, 1.0 / 500.0)
    uv2 = U.world_uv(m, -1800, -120, 1.0 / 2200.0)

    b = U.tex_param(m, "BaseTex", -1450, -400, default=alb)
    U.MEL.connect_material_expressions(uv, "", b, "UVs")
    bm = U._expr(m, unreal.MaterialExpressionComponentMask, -1200, -400)
    bm.set_editor_property("a", False)
    U.MEL.connect_material_expressions(b, "RGB", bm, "")

    mc = U.tex_param(m, "MacroTex", -1450, -120, default=macro)
    U.MEL.connect_material_expressions(uv2, "", mc, "UVs")
    mcm = U._expr(m, unreal.MaterialExpressionComponentMask, -1200, -120)
    mcm.set_editor_property("b", False)
    mcm.set_editor_property("a", False)
    U.MEL.connect_material_expressions(mc, "RGB", mcm, "")

    tint = U.vector_param(m, "Tint", (1.0, 1.0, 1.0), -1200, -20)
    gain = U.scalar_param(m, "Gain", 1.0, -1200, 60)
    c = U.mul(m, bm, mcm, -950, -320)
    c = U.mul(m, c, tint, -780, -300)
    c = U.mul(m, c, gain, -620, -280)
    U.MEL.connect_material_property(c, "", unreal.MaterialProperty.MP_BASE_COLOR)

    r = U.tex_param(m, "RoughTex", -1450, 180, default=rough)
    U.MEL.connect_material_expressions(uv, "", r, "UVs")
    rr = U.lerp(m, U.scalar_param(m, "RoughMin", 0.74, -1450, 300),
                U.scalar_param(m, "RoughMax", 0.96, -1450, 380), r, -1100, 300)
    U.MEL.connect_material_property(rr, "", unreal.MaterialProperty.MP_ROUGHNESS)

    if norm:
        n = U.tex_param(m, "NormalTex", -1450, 500, default=norm)
        n.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        U.MEL.connect_material_expressions(uv, "", n, "UVs")
        U.MEL.connect_material_property(n, "RGB", unreal.MaterialProperty.MP_NORMAL)

    U.compile_and_save(m)
    U.new_instance("MI_ClayCourt", m, textures={"BaseTex": alb, "RoughTex": rough, "MacroTex": macro},
                   scalars={"RoughMin": 0.74, "RoughMax": 0.96, "Gain": 1.0},
                   vectors={"Tint": (1.0, 1.0, 1.0)})
    U.new_instance("MI_ClayApron", m, textures={"BaseTex": alb, "RoughTex": rough, "MacroTex": macro},
                   scalars={"RoughMin": 0.78, "RoughMax": 0.98, "Gain": 0.92},
                   vectors={"Tint": (1.0, 1.02, 1.0)})

    # simple supporting materials (unchanged look, rebuilt)
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

    simple("M_Carbon", (0.03, 0.032, 0.038), 0.3)
    simple("M_Metal", (0.55, 0.56, 0.58), 0.25, 1.0)
    simple("M_Concrete", (0.22, 0.21, 0.2), 0.9)
    simple("M_Windscreen", (0.02, 0.07, 0.04), 0.9)
    simple("M_Hedge", (0.03, 0.09, 0.025), 0.95)
    simple("M_Tree", (0.02, 0.07, 0.025), 0.95)

    # line paint
    ml = U.new_material("M_LinePaint")
    lc = U.vector_param(ml, "BaseColor", (0.88, 0.87, 0.84), -600, -200)
    U.MEL.connect_material_property(lc, "", unreal.MaterialProperty.MP_BASE_COLOR)
    U.MEL.connect_material_property(U.scalar_param(ml, "Roughness", 0.66, -600, -60), "",
                                    unreal.MaterialProperty.MP_ROUGHNESS)
    U.compile_and_save(ml)
    U.new_instance("MI_CourtLine", ml)

    # net
    net_d = unreal.EditorAssetLibrary.load_asset(TEX + "/T_Net_Albedo")
    net_n = unreal.EditorAssetLibrary.load_asset(TEX + "/T_Net_Normal")
    mn = U.new_material("M_Net", two_sided=True, blend=unreal.BlendMode.BLEND_MASKED)
    if net_d:
        tc = U._expr(mn, unreal.MaterialExpressionTextureCoordinate, -1200, -300)
        tc.set_editor_property("utiling", 14.0)
        tc.set_editor_property("vtiling", 1.0)
        t = U.tex_param(mn, "NetTex", -900, -300, default=net_d)
        U.MEL.connect_material_expressions(tc, "", t, "UVs")
        U.MEL.connect_material_property(t, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
        U.MEL.connect_material_property(t, "A", unreal.MaterialProperty.MP_OPACITY_MASK)
    U.MEL.connect_material_property(U.scalar_param(mn, "Roughness", 0.7, -900, -100), "",
                                    unreal.MaterialProperty.MP_ROUGHNESS)
    if net_n:
        n2 = U.tex_param(mn, "NormalTex", -900, 120, default=net_n)
        n2.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        U.MEL.connect_material_property(n2, "RGB", unreal.MaterialProperty.MP_NORMAL)
    U.compile_and_save(mn)
    U.new_instance("MI_Net", mn)

    # ball
    ball_d = unreal.EditorAssetLibrary.load_asset(TEX + "/T_Ball_Albedo")
    ball_n = unreal.EditorAssetLibrary.load_asset(TEX + "/T_Ball_Normal")
    ball_r = unreal.EditorAssetLibrary.load_asset(TEX + "/T_Ball_Rough")
    mb = U.new_material("M_TennisBall")
    if ball_d:
        U.MEL.connect_material_property(U.tex_param(mb, "BallTex", -900, -300, default=ball_d), "RGB",
                                        unreal.MaterialProperty.MP_BASE_COLOR)
    if ball_n:
        nb = U.tex_param(mb, "NormalTex", -900, 260, default=ball_n)
        nb.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        U.MEL.connect_material_property(nb, "RGB", unreal.MaterialProperty.MP_NORMAL)
    if ball_r:
        U.MEL.connect_material_property(
            U.lerp(mb, U.scalar_param(mb, "RMin", 0.8, -900, 60),
                   U.scalar_param(mb, "RMax", 0.95, -900, 130),
                   U.tex_param(mb, "BallRough", -1100, 60, default=ball_r), -700, 80), "",
            unreal.MaterialProperty.MP_ROUGHNESS)
    U.compile_and_save(mb)
    U.new_instance("MI_TennisBall", mb)

    # racket strings
    strings = unreal.EditorAssetLibrary.load_asset(TEX + "/T_RacketStrings_Albedo")
    ms = U.new_material("M_RacketStrings", two_sided=True, blend=unreal.BlendMode.BLEND_MASKED)
    if strings:
        tc = U._expr(ms, unreal.MaterialExpressionTextureCoordinate, -1000, -200)
        tc.set_editor_property("utiling", 2.4)
        tc.set_editor_property("vtiling", 3.0)
        t = U.tex_param(ms, "StringTex", -750, -200, default=strings)
        U.MEL.connect_material_expressions(tc, "", t, "UVs")
        U.MEL.connect_material_property(t, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
        U.MEL.connect_material_property(t, "A", unreal.MaterialProperty.MP_OPACITY_MASK)
    U.MEL.connect_material_property(U.scalar_param(ms, "Roughness", 0.5, -750, 0), "",
                                    unreal.MaterialProperty.MP_ROUGHNESS)
    U.compile_and_save(ms)

    # crowd / flood / logo
    crowd = unreal.EditorAssetLibrary.load_asset(TEX + "/T_Crowd_Albedo")
    mcrowd = U.new_material("M_Crowd", two_sided=True)
    if crowd:
        U.MEL.connect_material_property(U.tex_param(mcrowd, "CrowdTex", -700, -200, default=crowd), "RGB",
                                        unreal.MaterialProperty.MP_BASE_COLOR)
    U.compile_and_save(mcrowd)
    U.new_instance("MI_Crowd", mcrowd)

    mfl = U.new_material("M_Floodlight", two_sided=True,
                         shading=unreal.MaterialShadingModel.MSM_UNLIT)
    U.MEL.connect_material_property(U.vector_param(mfl, "EmitColor", (4.0, 3.8, 3.4), -600, -200), "",
                                    unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    U.compile_and_save(mfl)
    U.new_instance("MI_Floodlight", mfl)

    logo = unreal.EditorAssetLibrary.load_asset(TEX + "/T_Court_Logo_Albedo")
    if logo:
        mlg = U.new_material("M_CourtLogo", two_sided=True, blend=unreal.BlendMode.BLEND_MASKED)
        t = U.tex_param(mlg, "LogoTex", -700, -200, default=logo)
        U.MEL.connect_material_property(t, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
        U.MEL.connect_material_property(t, "A", unreal.MaterialProperty.MP_OPACITY_MASK)
        U.compile_and_save(mlg)
        U.new_instance("MI_CourtLogo", mlg)

    unreal.log("[MAT3] done")


build()
