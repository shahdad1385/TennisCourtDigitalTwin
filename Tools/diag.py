import unreal
out=[]
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
out.append("count=%d"%len(actors))
for a in actors:
    try:
        loc=a.get_actor_location()
        s=a.get_actor_scale3d()
        if abs(loc.x)<500 and loc.z<500:
            out.append("%s | %s | loc=(%.0f,%.0f,%.0f) scale=(%.2f,%.2f,%.2f)" % (a.get_actor_label(), a.get_class().get_name(), loc.x, loc.y, loc.z, s.x, s.y, s.z))
    except Exception as e:
        out.append("ERR "+str(e))
open(r"C:\Projects\ClayTennis\RawAssets\diag.txt","w",encoding="utf-8").write("\n".join(out))
