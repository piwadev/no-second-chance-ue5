using UnrealBuildTool;

public class NoSecondChanceTarget : TargetRules
{
	public NoSecondChanceTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("NoSecondChance");
	}
}
