using UnrealBuildTool;

public class GameXXKEditor : ModuleRules
{
	public GameXXKEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");

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
			"LevelEditor",
			"Slate",
			"SlateCore",
			"UMG",
			"UnrealEd",
			"Landscape"
		});
	}
}
