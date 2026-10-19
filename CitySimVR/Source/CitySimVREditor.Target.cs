using UnrealBuildTool;

public class CitySimVREditorTarget : TargetRules
{
	public CitySimVREditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
		ExtraModuleNames.Add("CitySimVR");
	}
}
