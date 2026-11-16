using UnrealBuildTool;

// Dedicated server target. Building it needs Unreal Engine compiled from source (GitHub), not the Epic Launcher build.
// See README "Dedicated servers (later)".
public class NoSecondChanceServerTarget : TargetRules
{
	public NoSecondChanceServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("NoSecondChance");
	}
}
