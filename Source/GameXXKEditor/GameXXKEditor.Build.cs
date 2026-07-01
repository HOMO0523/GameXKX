using UnrealBuildTool;

public class GameXXKEditor : ModuleRules
{
	public GameXXKEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Landscape",
			"Paper2D",
			"PaperZD"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"AssetRegistry",
			"KismetCompiler",
			"UnrealEd",
			"Landscape"
		});
	}
}
