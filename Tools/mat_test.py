import unreal

out = []
mi = unreal.EditorAssetLibrary.load_asset("/Game/Materials/MI_ClayCourt")
out.append("load=%s" % mi)

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
a = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 400),
                                  unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0))
a.set_actor_label("MatTest")
smc = a.static_mesh_component
out.append("smc=%s" % smc)
mesh = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Plane")
smc.set_static_mesh(mesh)
out.append("mesh=%s" % smc.get_editor_property("static_mesh"))
if mi:
    smc.set_material(0, mi)
out.append("mat0=%s" % smc.get_material(0))
out.append("override_count=%s" % smc.get_num_materials())

# also check an existing court actor's material
for act in actors.get_all_level_actors():
    if act.get_actor_label() == "Court_Surface":
        c = act.static_mesh_component
        out.append("court mat0=%s" % c.get_material(0))
        break

with open(r"C:\Projects\ClayTennis\RawAssets\mat_test.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(out))
unreal.SystemLibrary.quit_editor()
