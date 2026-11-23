import unreal, sys
sys.path.insert(0, r"C:\Projects\ClayTennis\Tools")
import ue_common as U
tex = unreal.EditorAssetLibrary.load_asset("/Game/Textures/T_Clay2_Albedo")
m = U.new_material("M_TestTex", folder="/Game/Test")
n = U.tex_param(m, "BaseTex", -600, -200, default=tex)
U.MEL.connect_material_property(n, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
U.compile_and_save(m)
u = unreal.EditorAssetLibrary.load_asset("/Game/Textures/T_Clay2_Albedo")
print("tex compression", u.get_editor_property("compression_settings"), "srgb", u.get_editor_property("srgb"), "lodgroup", u.get_editor_property("lod_group"))
print("tex never_stream", u.get_editor_property("never_stream"))
print("virtual", u.get_editor_property("virtual_texture_streaming"))
