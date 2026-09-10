using UnrealBuildTool;

// An optimized playtest build with the complete F10 workbench.
// The normal GameXXK Shipping target remains free of development entry points.
public class GameXXKDevTarget : TargetRules
{
    public GameXXKDevTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.AddRange(new[] { "GameXXK", "TestMap" });
        ProjectDefinitions.Add("GAMEXXK_SHIPPING_WITH_DEV_TOOLS=1");
    }
}
