using UnrealBuildTool;

public class NoSecondChanceEditorTarget : TargetRules
{
	public NoSecondChanceEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("NoSecondChance");
	}
}
