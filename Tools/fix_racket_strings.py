"""Fix M_RacketStrings in place: the TextureCoordinate node (trap #2: UV-node
math breaks material compilation in this project) made it fail to compile for
PCD3D_SM6, so the string bed fell back to the default material in game.
Rebuild the material with plain default UVs and verify it compiles.
"""
import unreal
import sys

sys.path.insert(0, r"C:\Projects\ClayTennis\Tools")
import ue_common as U

OUT = r"C:\Projects\ClayTennis\RawAssets\fix_racket_strings_result.txt"
RES = []


def note(s):
    RES.append(str(s))


def build():
    sd = unreal.EditorAssetLibrary.load_asset("/Game/Textures/T_RacketStrings_Albedo")
    ms = unreal.EditorAssetLibrary.load_asset("/Game/Materials/M_RacketStrings")
    if not ms:
        note("material missing")
        return

    U.MEL.delete_all_material_expressions(ms)
    if sd:
        # Default sampler type (trap #1) and default 0..1 UVs (trap #2).
        t = U.tex_param(ms, "StringTex", -600, -200, default=sd)
        U.MEL.connect_material_property(t, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
        U.MEL.connect_material_property(t, "A", unreal.MaterialProperty.MP_OPACITY_MASK)
    U.MEL.connect_material_property(
        U.scalar_param(ms, "Roughness", 0.85, -600, 60), "",
        unreal.MaterialProperty.MP_ROUGHNESS)
    U.compile_and_save(ms)

    try:
        unreal.EditorAssetLibrary.save_asset("/Game/Materials/M_RacketStrings")
        note("saved OK")
    except Exception as exc:  # noqa: BLE001
        note("save failed: %s" % exc)


try:
    build()
except Exception:  # noqa: BLE001
    import traceback
    note("EXC\n" + traceback.format_exc())

with open(OUT, "w", encoding="utf-8") as f:
    f.write("\n".join(RES))

unreal.log("[FIXSTRINGS] " + RES[-1][:160])
