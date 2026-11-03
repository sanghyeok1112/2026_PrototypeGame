// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MyProjectPrototype : ModuleRules
{
	public MyProjectPrototype(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"GameplayTasks",
			"NavigationSystem",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] {
				"UnrealEd", "BehaviorTreeEditor", "AIGraph", "AssetRegistry"
			});
		}

		PublicIncludePaths.AddRange(new string[] {
			"MyProjectPrototype",
			"MyProjectPrototype/Variant_Platforming",
			"MyProjectPrototype/Variant_Platforming/Animation",
			"MyProjectPrototype/Variant_Combat",
			"MyProjectPrototype/Variant_Combat/AI",
			"MyProjectPrototype/Variant_Combat/Animation",
			"MyProjectPrototype/Variant_Combat/Gameplay",
			"MyProjectPrototype/Variant_Combat/Interfaces",
			"MyProjectPrototype/Variant_Combat/UI",
			"MyProjectPrototype/Variant_SideScrolling",
			"MyProjectPrototype/Variant_SideScrolling/AI",
			"MyProjectPrototype/Variant_SideScrolling/Gameplay",
			"MyProjectPrototype/Variant_SideScrolling/Interfaces",
			"MyProjectPrototype/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}

