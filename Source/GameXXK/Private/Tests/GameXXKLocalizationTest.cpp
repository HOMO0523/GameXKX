#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/FileManager.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Culture.h"
#include "UI/GameXXKLocalization.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
    struct FRestoreGameLanguage
    {
        FString Previous = GameXXKLocalization::GetLanguage();
        ~FRestoreGameLanguage() { GameXXKLocalization::SetLanguage(Previous, false); }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKLocalizationLiveTextTest,
    "GameXXK.Localization.LiveTextAndParameters", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKLocalizationLiveTextTest::RunTest(const FString&)
{
    FRestoreGameLanguage Restore;
    const FString EditorLanguage = FInternationalization::Get().GetCurrentLanguage()->GetName();
    TestTrue(TEXT("Chinese can be selected without writing preferences"), GameXXKLocalization::SetLanguage(TEXT("zh-Hans"), false));
    const FText LiveTitle = GameXXKLocalization::Text(TEXT("Settings.Title"));
    TestEqual(TEXT("Chinese title"), LiveTitle.ToString(), FString(TEXT("设置")));
    FFormatNamedArguments Args;
    Args.Add(TEXT("Amount"), FText::AsCultureInvariant(TEXT("123456")));
    const FText LiveReward = GameXXKLocalization::Format(TEXT("Academy.Reward"), Args);
    TestTrue(TEXT("English can be selected"), GameXXKLocalization::SetLanguage(TEXT("en"), false));
    TestEqual(TEXT("an existing FText changes without being recreated"), LiveTitle.ToString(), FString(TEXT("Settings")));
    TestEqual(TEXT("formatted text keeps its quantity and rebuilds its language"), LiveReward.ToString(),
        FString(TEXT("First-clear reward: 123456 Gold")));
    TestEqual(TEXT("game language does not change the editor language"), FInternationalization::Get().GetCurrentLanguage()->GetName(), EditorLanguage);
	const FText NormalDifficulty = GameXXKLocalization::Text(TEXT("Difficulty.Normal"));
	TestEqual(TEXT("same Chinese word keeps its difficulty identity instead of becoming a quality"),
		GameXXKLocalization::Localize(NormalDifficulty).ToString(), FString(TEXT("Normal")));
    TestFalse(TEXT("unsupported languages cannot become active"), GameXXKLocalization::SetLanguage(TEXT("ja"), false));
    TestEqual(TEXT("rejected language retains English"), GameXXKLocalization::GetLanguage(), FString(TEXT("en")));
    TestTrue(TEXT("switch back"), GameXXKLocalization::SetLanguage(TEXT("zh-Hans"), false));
    TestEqual(TEXT("the same title switches back"), LiveTitle.ToString(), FString(TEXT("设置")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKLocalizationPreferenceTest,
    "GameXXK.Localization.PreferenceRoundTripAndFallback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKLocalizationPreferenceTest::RunTest(const FString&)
{
    const FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/Localization"));
    IFileManager::Get().MakeDirectory(*Dir, true);
    const FString Path = FPaths::CreateTempFilename(*Dir, TEXT("language-"), TEXT(".ini"));
    FString Error;
    TestEqual(TEXT("missing settings start in Chinese"), GameXXKLocalization::ReadPreference(Path), FString(TEXT("zh-Hans")));
    FConfigFile Config;
    Config.SetString(TEXT("Untouched"), TEXT("Draft"), TEXT("7/8"));
    Config.Dirty = true;
    TestTrue(TEXT("fixture creates an unrelated setting"), Config.Write(Path));
    TestTrue(TEXT("English is written to the supplied isolated path"), GameXXKLocalization::WritePreference(TEXT("en"), Path, &Error));
    TestEqual(TEXT("fresh settings read restores English"), GameXXKLocalization::ReadPreference(Path), FString(TEXT("en")));
    Config.Empty(); Config.Read(Path);
    FString Draft;
    Config.GetString(TEXT("Untouched"), TEXT("Draft"), Draft);
    TestEqual(TEXT("saving a language preserves unrelated configuration"), Draft, FString(TEXT("7/8")));
    TestFalse(TEXT("unsupported preference is refused"), GameXXKLocalization::WritePreference(TEXT("ko"), Path, &Error));
    TestEqual(TEXT("failed save leaves the last preference intact"), GameXXKLocalization::ReadPreference(Path), FString(TEXT("en")));
    Config.SetString(TEXT("GameXXK.Localization"), TEXT("Language"), TEXT("invalid-language"));
    Config.Dirty = true; Config.Write(Path);
    TestEqual(TEXT("corrupt language falls back to Chinese"), GameXXKLocalization::ReadPreference(Path), FString(TEXT("zh-Hans")));
    IFileManager::Get().Delete(*Path);
    return true;
}
#endif
