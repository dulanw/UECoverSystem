// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NavigationCoverSystem : ModuleRules
{
	public NavigationCoverSystem(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject", 
				"Engine",
				"InputCore", 
				"Landscape",
				"Navmesh", 
				"NavigationSystem",
				"AIModule"
				// ... add other public dependencies that you statically link with here ...
			});
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"GameplayTasks", 
				"UMG" 
				// ... add private dependencies that you statically link with here ...	
			});
		
		PublicDefinitions.Add("DEBUG_RENDERING=!(UE_BUILD_SHIPPING || UE_BUILD_TEST) || WITH_EDITOR");
	}
}
