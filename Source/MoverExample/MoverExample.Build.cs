// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MoverExample : ModuleRules
{
	public MoverExample(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" });
		PublicDependencyModuleNames.AddRange(new string[] { "Mover" });
	}
}
