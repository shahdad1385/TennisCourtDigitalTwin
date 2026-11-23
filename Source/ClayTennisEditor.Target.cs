// Copyright (c) 2026 ClayTennis. All rights reserved.

using UnrealBuildTool;
using System.Collections.Generic;

/**
 * Editor target. Same module set as the game target so that the editor
 * builds the exact code that ships.
 */
public class ClayTennisEditorTarget : TargetRules
{
	public ClayTennisEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_5;

		ExtraModuleNames.AddRange(new string[] { "ClayTennis" });
	}
}
