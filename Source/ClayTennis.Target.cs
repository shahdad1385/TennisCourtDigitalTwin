// Copyright (c) 2026 ClayTennis. All rights reserved.

using UnrealBuildTool;
using System.Collections.Generic;

/**
 * Game target for the clay-court tennis simulation.
 * Kept deliberately conservative: no extra plugins or modules, only the
 * dependencies declared in ClayTennis.Build.cs.
 */
public class ClayTennisTarget : TargetRules
{
	public ClayTennisTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_5;

		ExtraModuleNames.AddRange(new string[] { "ClayTennis" });
	}
}
