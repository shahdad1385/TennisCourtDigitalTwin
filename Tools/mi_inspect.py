import unreal
out=[]
mi=unreal.EditorAssetLibrary.load_asset("/Game/Materials/MI_ClayCourt")
out.append("mi=%s"%mi)
if mi:
    try: out.append("parent=%s"%mi.get_editor_property("parent"))
    except Exception as e: out.append("parent err %s"%e)
    for p in ["BaseTex","MacroTex","RoughTex"]:
        try: out.append("%s=%s"%(p, unreal.MaterialEditingLibrary.get_material_instance_texture_parameter_value(mi,p)))
        except Exception as e: out.append("%s err %s"%(p,e))
    for p in ["Gain","RoughMin","RoughMax"]:
        try: out.append("%s=%s"%(p, unreal.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(mi,p)))
        except Exception as e: out.append("%s err %s"%(p,e))
    try: out.append("Tint=%s"%unreal.MaterialEditingLibrary.get_material_instance_vector_parameter_value(mi,"Tint"))
    except Exception as e: out.append("tint err %s"%e)
t=unreal.EditorAssetLibrary.load_asset("/Game/Textures/T_Clay2_Albedo")
out.append("clay2=%s"%t)
if t:
    out.append("size=%sx%s srgb=%s"%(t.blueprint_get_size_x(), t.blueprint_get_size_y(), t.get_editor_property("srgb")))
open(r"C:\Projects\ClayTennis\RawAssets\mi_inspect.txt","w",encoding="utf-8").write("\n".join(out))
