using UnrealBuildTool;

public class GameXXKDesktopOverlay : ModuleRules
{
	public GameXXKDesktopOverlay(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bUseUnity = false;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"RHI"
		});
		PrivateDependencyModuleNames.AddRange(new[]
		{
			"ApplicationCore",
			"CoreUObject",
			"Projects",
			"RenderCore"
		});

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicSystemLibraries.AddRange(new[]
			{
				"dcomp.lib",
				"dwmapi.lib",
				"dxgi.lib"
			});
		}
	}
}
