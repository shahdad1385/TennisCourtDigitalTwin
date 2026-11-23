import unreal
out=[]
def p(s): out.append(str(s))
t=unreal.EditorAssetLibrary.load_asset("/Game/Textures/T_Net_Albedo")
p("net tex: %s" % t)
if t:
    p("has_alpha=%s compression=%s srgb=%s size=%sx%s" % (t.get_editor_property("has_alpha") if hasattr(t,"has_alpha") else "?", t.get_editor_property("compression_settings"), t.get_editor_property("srgb"), t.blueprint_get_size_x() if hasattr(t,"blueprint_get_size_x") else "?", t.blueprint_get_size_y() if hasattr(t,"blueprint_get_size_y") else "?"))
m=unreal.EditorAssetLibrary.load_asset("/Game/Materials/M_Net")
if m:
    p("M_Net blend=%s twosided=%s exprs=%d" % (m.get_editor_property("blend_mode"), m.get_editor_property("two_sided"), unreal.MaterialEditingLibrary.get_num_material_expressions(m)))
    p("opacity_mask_node=%s" % unreal.MaterialEditingLibrary.get_material_property_input_node(m, unreal.MaterialProperty.MP_OPACITY_MASK))
    p("base_node=%s" % unreal.MaterialEditingLibrary.get_material_property_input_node(m, unreal.MaterialProperty.MP_BASE_COLOR))
for n in ["MI_ClayCourt","MI_Net","MI_Crowd","MI_TennisBall","MI_CourtLine"]:
    a=unreal.EditorAssetLibrary.load_asset("/Game/Materials/"+n)
    p("%s -> %s" % (n, a))
open(r"C:\Projects\ClayTennis\RawAssets\inspect_mats.txt","w").write("\n".join(out))
