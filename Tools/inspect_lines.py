"""Dump every Line_* actor in the level: location, scale, material. Read-only."""
import unreal
import sys

OUT = r"C:\Projects\ClayTennis\RawAssets\inspect_lines_result.txt"
RES = ["=== Line actors ==="]

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for a in actors.get_all_level_actors():
    label = a.get_actor_label()
    if label.startswith("Line_") or label.startswith("NetBand") or label.startswith("Net_Centre"):
        loc = a.get_actor_location()
        scale = a.get_actor_scale3d()
        mats = []
        smc = a.get_component_by_class(unreal.StaticMeshComponent)
        if smc:
            for i in range(smc.get_num_materials()):
                m = smc.get_material(i)
                mats.append(m.get_path_name() if m else "NONE")
        RES.append("%s loc=(%.1f, %.1f, %.1f) scale=(%.3f, %.3f, %.3f) mat=%s"
                   % (label, loc.x, loc.y, loc.z, scale.x, scale.y, scale.z,
                      "|".join(mats) if mats else "no-smc"))

RES.append("=== sun ===")
for a in actors.get_all_level_actors():
    if a.get_actor_label() == "Sun":
        comp = a.get_component_by_class(unreal.DirectionalLightComponent)
        RES.append("sun intensity=%s" % comp.get_editor_property("intensity"))

with open(OUT, "w", encoding="utf-8") as f:
    f.write("\n".join(RES))

unreal.log("[INSPECTLINES] wrote %d entries" % len(RES))
