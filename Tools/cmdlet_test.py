import unreal, traceback
res = []
try:
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    res.append("les=" + str(les is not None))
    les.new_level("/Game/Maps/TestMap")
    res.append("new_level ok")
    acts = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    a = acts.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    res.append("actor=" + str(a is not None))
    les.save_current_level()
    res.append("saved")
except Exception:
    res.append(traceback.format_exc())
open(r"C:\Projects\ClayTennis\RawAssets\cmdlet_level_test.txt", "w").write("\n".join(res))
