#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "GameXXKGemRules.h"
#include "UI/GameXXKLocalization.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKGemCompactPresentationTest,
    "GameXXK.Localization.Surfaces.CompactGemNamesAndDescriptions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKGemCompactPresentationTest::RunTest(const FString&)
{
    const FString Previous = GameXXKLocalization::GetLanguage();
    ON_SCOPE_EXIT { GameXXKLocalization::SetLanguage(Previous, false); };
    for (const FString Language : {FString(TEXT("zh-Hans")), FString(TEXT("en"))})
    {
        GameXXKLocalization::SetLanguage(Language, false);
        for (int32 T = 1; T <= FGameXXKGemRules::MaximumTypeRank; ++T)
        {
            const auto Type = static_cast<EGameXXKGemType>(T);
            const FString Header = FGameXXKGemRules::GetDisplayName(Type, EGameXXKGemQuality::Common).ToString();
            TestFalse(TEXT("translated short name exists"), Header.IsEmpty() || Header.StartsWith(TEXT("Gem.")));
            TestTrue(TEXT("short header fits"), Header.Len() <= (Language == TEXT("en") ? 12 : 6));
            for (int32 Q = 1; Q <= FGameXXKGemRules::MaximumQualityRank; ++Q)
            {
                const auto Quality = static_cast<EGameXXKGemQuality>(Q);
                TestEqual(TEXT("quality is not part of the title"), FGameXXKGemRules::GetDisplayName(Type, Quality).ToString(), Header);
                const FString Body = FGameXXKGemRules::GetDescription(Type, Quality).ToString();
                TArray<FString> Lines; Body.ParseIntoArrayLines(Lines, false);
                if (!TestTrue(TEXT("description has quality and stats"), Lines.Num() >= 2)) continue;
                TestEqual(TEXT("quality occupies its own line"), Lines[0], FGameXXKGemRules::GetQualityDisplayName(Quality).ToString());
                TestTrue(TEXT("bonus remains visible"), Lines[1].Contains(FGameXXKGemRules::GetBonusText(Type, Quality).ToString()));
                TestTrue(TEXT("description stays concise"), Body.Len() <= (Language == TEXT("en") ? 220 : 100));
                TestEqual(TEXT("cap displayed only for percentage bonuses"), Body.Contains(TEXT("75%")), FGameXXKGemRules::IsPercentType(Type));
                if (Language == TEXT("en"))
                    for (TCHAR C : Body) if (C >= 0x3400 && C <= 0x9fff) { AddError(TEXT("Chinese remained in English gem description")); break; }
            }
        }
        const FString Fire = FGameXXKGemRules::GetDescription(EGameXXKGemType::FireDamage, EGameXXKGemQuality::Common).ToString();
        TestTrue(TEXT("fire scope preserved"), Fire.Contains(Language == TEXT("en") ? TEXT("Burn") : TEXT("灼烧")));
    }
    return true;
}
#endif
