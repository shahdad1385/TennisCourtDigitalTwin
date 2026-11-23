import unreal

out = []
sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = sub.get_all_level_actors()
out.append("actors=%d" % len(actors))
for a in actors:
    try:
        o, e = a.get_actor_bounds(False)
        if abs(o.x) < 250 and o.z < 350:
            out.append("%s | %s | org=(%.0f,%.0f,%.0f) ext=(%.0f,%.0f,%.0f)" % (
                a.get_actor_label(), a.get_class().get_name(), o.x, o.y, o.z, e.x, e.y, e.z))
    except Exception as exc:
        pass

with open(r"C:\Projects\ClayTennis\RawAssets\actors_near_net.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(out))

unreal.SystemLibrary.quit_editor()
