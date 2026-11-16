using UnrealBuildTool;

public class NoSecondChance : ModuleRules
{
	public NoSecondChance(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
			"UMG", "Slate", "SlateCore",
			"AIModule", "NavigationSystem", "GameplayTasks",
			"PhysicsCore", "NetCore",
			"OnlineSubsystem", "OnlineSubsystemUtils", "EngineSettings"
		});

		DynamicallyLoadedModuleNames.Add("OnlineSubsystemNull");
	}
}
