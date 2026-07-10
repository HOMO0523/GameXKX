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
			"PaperZD",
			"PCG"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"AssetRegistry",
			"BlueprintGraph",
			"Json",
			"KismetCompiler",
			"UMG",
			"UnrealEd",
			"Landscape"
		});
	}
}
