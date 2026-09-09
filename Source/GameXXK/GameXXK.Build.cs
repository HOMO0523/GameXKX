// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GameXXK : ModuleRules
{
	public GameXXK(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bUseUnity = false;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "EngineSettings", "InputCore", "EnhancedInput", "UMG", "Slate", "SlateCore", "Paper2D" });

		PrivateDependencyModuleNames.AddRange(new string[] { "Json", "JsonUtilities", "ApplicationCore" });
		// The keyed language catalogue is read through Unreal's UFS in both editor and packaged games.
		RuntimeDependencies.Add("$(ProjectDir)/Content/Localization/GameXXK/strings.json", StagedFileType.UFS);
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PrivateDependencyModuleNames.Add("GameXXKDesktopOverlay");
		}

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
