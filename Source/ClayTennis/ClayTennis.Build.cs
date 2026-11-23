// Copyright (c) 2026 ClayTennis. All rights reserved.

using UnrealBuildTool;

/**
 * Module rules for the tennis simulation.
 *
 * Only stock Engine modules are used. No UMG, no Niagara, no EnhancedInput
 * assets: the HUD is drawn with AHUD/Canvas and input is the legacy
 * axis/action system configured in Config/DefaultInput.ini.
 */
public class ClayTennis : ModuleRules
{
	public ClayTennis(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"Slate",
			"SlateCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });
	}
}
