"""Import textures and build every material for the ClayTennis project."""
import unreal
import os
import sys

sys.path.insert(0, r"C:\Projects\ClayTennis\Tools")
import ue_common as U

RAW = r"C:\Projects\ClayTennis\RawAssets"
PH = RAW + r"\Polyhaven"
PROC = RAW + r"\Textures"


def p(*parts):
    return os.path.join(*parts)


def build():
    U.log("=== build_assets start ===")
    tex_dir = "/Game/Textures"

    for asset in U.EAL.list_assets("/Game/Materials"):
        U.EAL.delete_asset(asset)

    # ---------------------------------------------------------------- textures
    clay_diff = U.import_texture(p(PH, "sandy_gravel", "sandy_gravel_diff_4k.jpg"),
                                 tex_dir, "T_Clay_Diffuse", srgb=True)
    clay_rough = U.import_texture(p(PH, "sandy_gravel", "sandy_gravel_rough_4k.jpg"),
                                  tex_dir, "T_Clay_Roughness", srgb=False)
    clay_norm = U.import_texture(p(PH, "sandy_gravel", "sandy_gravel_nor_gl_4k.jpg"),
                                 tex_dir, "T_Clay_Normal", srgb=False, normal=True)
    clay_ao = U.import_texture(p(PH, "sandy_gravel", "sandy_gravel_ao_2k.jpg"),
                               tex_dir, "T_Clay_AO", srgb=False)

    # procedural sets (created by gen_textures.ps1) - optional
    def proc(name, **kw):
        return U.import_texture(p(PROC, "T_" + name + ".png"), tex_dir, "T_" + name, **kw)

    line_diff = proc("LinePaint_Albedo", srgb=True)
    line_norm = proc("LinePaint_Normal", srgb=False, normal=True)
    net_diff = proc("Net_Albedo", srgb=True)
    net_norm = proc("Net_Normal", srgb=False, normal=True)
    ball_diff = proc("Ball_Albedo", srgb=True)
    ball_norm = proc("Ball_Normal", srgb=False, normal=True)
    ball_rough = proc("Ball_Rough", srgb=False)
    strings = proc("RacketStrings_Albedo", srgb=True)
    crowd = proc("Crowd_Albedo", srgb=True)
    logo = proc("Court_Logo_Albedo", srgb=True)

    # ------------------------------------------------------------- M_ClaySurface
    mat = U.new_material("M_ClaySurface")
    uv_fine = U.world_uv(mat, -1600, -500, 1.0 / 190.0)
    uv_macro = U.world_uv(mat, -1600, -120, 1.0 / 1100.0)

    b = U.tex_param(mat, "BaseTex", -1250, -520, default=clay_diff)
    U.MEL.connect_material_expressions(uv_fine, "", b, "UVs")
    d = U.tex_param(mat, "DetailTex", -1250, -260, default=clay_diff)
    U.MEL.connect_material_expressions(uv_macro, "", d, "UVs")

    b_rgb = U._expr(mat, unreal.MaterialExpressionComponentMask, -950, -520)
    b_rgb.set_editor_property("r", True)
    b_rgb.set_editor_property("g", True)
    b_rgb.set_editor_property("b", True)
    b_rgb.set_editor_property("a", False)
    U.MEL.connect_material_expressions(b, "RGB", b_rgb, "")

    macro = U.mul(mat, b_rgb, d, -760, -450)
    tint_col = U.vector_param(mat, "Tint", (1.19, 0.80, 0.63), -760, -300)
    tinted = U.mul(mat, macro, tint_col, -560, -420)
    U.MEL.connect_material_property(tinted, "", unreal.MaterialProperty.MP_BASE_COLOR)

    r = U.tex_param(mat, "RoughTex", -1250, 40, default=clay_rough)
    U.MEL.connect_material_expressions(uv_fine, "", r, "UVs")
    r_min = U.scalar_param(mat, "RoughMin", 0.76, -1250, 220)
    r_max = U.scalar_param(mat, "RoughMax", 0.97, -1250, 300)
    rough = U.lerp(mat, r_min, r_max, r, -900, 120)
    U.MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

    if clay_norm:
        n = U.tex_param(mat, "NormalTex", -1250, 420, default=clay_norm)
        n.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        U.MEL.connect_material_expressions(uv_fine, "", n, "UVs")
        U.MEL.connect_material_property(n, "RGB", unreal.MaterialProperty.MP_NORMAL)

    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    U.compile_and_save(mat)
    mi_clay = U.new_instance("MI_ClayCourt", mat, textures={"BaseTex": clay_diff, "RoughTex": clay_rough},
                             scalars={"RoughMin": 0.78, "RoughMax": 0.96})

    # ------------------------------------------------------------- M_LinePaint
    mat = U.new_material("M_LinePaint")
    base = U.vector_param(mat, "BaseColor", (0.93, 0.92, 0.89), -700, -200)
    U.MEL.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = U.scalar_param(mat, "Roughness", 0.62, -700, -80)
    U.MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    if line_diff:
        uv = U.world_uv(mat, -1200, 60, 1.0 / 120.0)
        t = U.tex_param(mat, "PaintTex", -950, 40, default=line_diff)
        U.MEL.connect_material_expressions(uv, "", t, "UVs")
        mask = U._expr(mat, unreal.MaterialExpressionComponentMask, -750, 40)
        mask.set_editor_property("a", False)
        U.MEL.connect_material_expressions(t, "RGB", mask, "")
        painted = U.mul(mat, mask, base, -520, -120)
        U.MEL.connect_material_property(painted, "", unreal.MaterialProperty.MP_BASE_COLOR)
    if line_norm:
        n = U.tex_param(mat, "NormalTex", -950, 260, default=line_norm)
        n.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        U.MEL.connect_material_property(n, "RGB", unreal.MaterialProperty.MP_NORMAL)
    U.compile_and_save(mat)
    mi_line = U.new_instance("MI_CourtLine", mat)

    # ------------------------------------------------------------------ M_Net
    mat = U.new_material("M_Net", two_sided=True, blend=unreal.BlendMode.BLEND_MASKED)
    if net_diff:
        uv = U._expr(mat, unreal.MaterialExpressionTextureCoordinate, -1200, -300)
        uv.set_editor_property("utiling", 13.0)
        uv.set_editor_property("vtiling", 1.0)
        t = U.tex_param(mat, "NetTex", -950, -300, default=net_diff)
        U.MEL.connect_material_expressions(uv, "", t, "UVs")
        U.MEL.connect_material_property(t, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
        U.MEL.connect_material_property(t, "A", unreal.MaterialProperty.MP_OPACITY_MASK)
    else:
        c = U.const3(mat, (0.05, 0.05, 0.06), -700, -200)
        U.MEL.connect_material_property(c, "", unreal.MaterialProperty.MP_BASE_COLOR)
    r = U.scalar_param(mat, "Roughness", 0.72, -700, 0)
    U.MEL.connect_material_property(r, "", unreal.MaterialProperty.MP_ROUGHNESS)
    if net_norm:
        n = U.tex_param(mat, "NormalTex", -950, 200, default=net_norm)
        n.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        U.MEL.connect_material_property(n, "RGB", unreal.MaterialProperty.MP_NORMAL)
    U.compile_and_save(mat)
    mi_net = U.new_instance("MI_Net", mat)

    # ----------------------------------------------------------------- M_Ball
    mat = U.new_material("M_TennisBall")
    ball_col = U.vector_param(mat, "BallColor", (0.77, 0.86, 0.10), -900, -200)
    U.MEL.connect_material_property(ball_col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    if ball_diff:
        t = U.tex_param(mat, "BallTex", -1200, -400, default=ball_diff)
        U.MEL.connect_material_property(t, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    if ball_rough:
        rt = U.tex_param(mat, "BallRough", -1200, 100, default=ball_rough)
        rr = U.lerp(mat, U.const(mat, 0.7, -1200, 220), U.const(mat, 0.95, -1200, 300), rt, -900, 160)
        U.MEL.connect_material_property(rr, "", unreal.MaterialProperty.MP_ROUGHNESS)
    else:
        U.MEL.connect_material_property(U.const(mat, 0.85, -900, 160), "",
                                        unreal.MaterialProperty.MP_ROUGHNESS)
    if ball_norm:
        n = U.tex_param(mat, "NormalTex", -1200, 400, default=ball_norm)
        n.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        U.MEL.connect_material_property(n, "RGB", unreal.MaterialProperty.MP_NORMAL)
    U.compile_and_save(mat)
    mi_ball = U.new_instance("MI_TennisBall", mat)

    # ---------------------------------------------------------------- M_Strings
    mat = U.new_material("M_RacketStrings", two_sided=True, blend=unreal.BlendMode.BLEND_MASKED)
    if strings:
        uv = U._expr(mat, unreal.MaterialExpressionTextureCoordinate, -1000, -200)
        uv.set_editor_property("utiling", 2.2)
        uv.set_editor_property("vtiling", 2.8)
        t = U.tex_param(mat, "StringTex", -700, -200, default=strings)
        U.MEL.connect_material_expressions(uv, "", t, "UVs")
        U.MEL.connect_material_property(t, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
        U.MEL.connect_material_property(t, "A", unreal.MaterialProperty.MP_OPACITY_MASK)
    else:
        U.MEL.connect_material_property(U.const3(mat, (0.8, 0.8, 0.78), -700, -200), "",
                                        unreal.MaterialProperty.MP_BASE_COLOR)
    U.MEL.connect_material_property(U.const(mat, 0.45, -700, 0), "",
                                    unreal.MaterialProperty.MP_ROUGHNESS)
    U.compile_and_save(mat)

    # ----------------------------------------------------------- simple surfs
    mat = U.new_material("M_Carbon")
    U.MEL.connect_material_property(U.const3(mat, (0.035, 0.038, 0.045), -600, -200), "",
                                    unreal.MaterialProperty.MP_BASE_COLOR)
    U.MEL.connect_material_property(U.const(mat, 0.28, -600, -60), "",
                                    unreal.MaterialProperty.MP_ROUGHNESS)
    U.MEL.connect_material_property(U.const(mat, 0.0, -600, 60), "",
                                    unreal.MaterialProperty.MP_METALLIC)
    U.compile_and_save(mat)
    mi_carbon = U.new_instance("MI_Carbon", mat)

    mat = U.new_material("M_Metal")
    U.MEL.connect_material_property(U.const3(mat, (0.55, 0.56, 0.58), -600, -200), "",
                                    unreal.MaterialProperty.MP_BASE_COLOR)
    U.MEL.connect_material_property(U.const(mat, 0.18, -600, -60), "",
                                    unreal.MaterialProperty.MP_ROUGHNESS)
    U.MEL.connect_material_property(U.const(mat, 1.0, -600, 60), "",
                                    unreal.MaterialProperty.MP_METALLIC)
    U.compile_and_save(mat)
    mi_metal = U.new_instance("MI_Metal", mat)

    mat = U.new_material("M_Concrete")
    U.MEL.connect_material_property(U.const3(mat, (0.30, 0.29, 0.27), -600, -200), "",
                                    unreal.MaterialProperty.MP_BASE_COLOR)
    U.MEL.connect_material_property(U.const(mat, 0.85, -600, -60), "",
                                    unreal.MaterialProperty.MP_ROUGHNESS)
    U.compile_and_save(mat)
    mi_concrete = U.new_instance("MI_Concrete", mat)

    mat = U.new_material("M_Windscreen")
    col = U.vector_param(mat, "ScreenColor", (0.02, 0.09, 0.05), -600, -200)
    U.MEL.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    U.MEL.connect_material_property(U.const(mat, 0.92, -600, -60), "",
                                    unreal.MaterialProperty.MP_ROUGHNESS)
    U.compile_and_save(mat)
    mi_screen = U.new_instance("MI_Windscreen", mat)

    mat = U.new_material("M_Crowd", two_sided=True)
    if crowd:
        t = U.tex_param(mat, "CrowdTex", -800, -200, default=crowd)
        U.MEL.connect_material_property(t, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
        U.MEL.connect_material_property(U.const(mat, 1.0, -800, 40), "",
                                        unreal.MaterialProperty.MP_ROUGHNESS)
    else:
        U.MEL.connect_material_property(U.const3(mat, (0.12, 0.12, 0.14), -600, -200), "",
                                        unreal.MaterialProperty.MP_BASE_COLOR)
    U.compile_and_save(mat)
    mi_crowd = U.new_instance("MI_Crowd", mat)

    mat = U.new_material("M_Floodlight", two_sided=True,
                         shading=unreal.MaterialShadingModel.MSM_UNLIT)
    em = U.vector_param(mat, "EmitColor", (1.0, 0.96, 0.88), -600, -200)
    U.MEL.connect_material_property(em, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    U.compile_and_save(mat)
    mi_flood = U.new_instance("MI_Floodlight", mat)

    if logo:
        mat = U.new_material("M_CourtLogo", two_sided=True, blend=unreal.BlendMode.BLEND_MASKED)
        t = U.tex_param(mat, "LogoTex", -700, -200, default=logo)
        U.MEL.connect_material_property(t, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
        U.MEL.connect_material_property(t, "A", unreal.MaterialProperty.MP_OPACITY_MASK)
        U.MEL.connect_material_property(U.const(mat, 0.8, -700, 40), "",
                                        unreal.MaterialProperty.MP_ROUGHNESS)
        U.compile_and_save(mat)
        U.new_instance("MI_CourtLogo", mat)

    U.log("=== build_assets done ===")


build()
