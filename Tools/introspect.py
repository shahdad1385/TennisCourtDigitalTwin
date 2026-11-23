import unreal

lines = []
lines.append("K2NODES: " + ",".join(n for n in dir(unreal) if n.startswith("K2Node")))
bel = unreal.BlueprintEditorLibrary
lines.append("BEL: " + ",".join(n for n in dir(bel) if not n.startswith("_")))
mef = unreal.MaterialEditingLibrary
lines.append("MEL: " + ",".join(n for n in dir(mef) if not n.startswith("_")))
lines.append("MATEXPR: " + str(len([n for n in dir(unreal) if n.startswith("MaterialExpression")])))
lines.append("HAS_EdGraph: " + str(hasattr(unreal, "EdGraph")))
lines.append("HAS_BlueprintFactory: " + str(hasattr(unreal, "BlueprintFactory")))
lines.append("HAS_EditorLevelLibrary: " + str(hasattr(unreal, "EditorLevelLibrary")))
lines.append("HAS_LevelEditorSubsystem: " + str(hasattr(unreal, "LevelEditorSubsystem")))
lines.append("HAS_AssetImportTask: " + str(hasattr(unreal, "AssetImportTask")))
lines.append("HAS_Niagara: " + str(hasattr(unreal, "NiagaraSystemFactoryNew")))
lines.append("HAS_UMGWidget: " + str(hasattr(unreal, "WidgetBlueprintFactory")))
lines.append("HAS_Sequencer: " + str(hasattr(unreal, "LevelSequenceFactoryNew")))
lines.append("HAS_Slate: " + str(hasattr(unreal, "SlateBrush")))

with open(r"C:\Projects\ClayTennis\RawAssets\introspect.txt", "w") as f:
    f.write("\n".join(lines))
unreal.log("[INTROSPECT] done")
