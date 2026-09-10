#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UI/GameXXKLocalization.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKExplicitUiFormatHistoryTest,
    "GameXXK.Localization.ExplicitFormatKeepsCompactIdentity",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKExplicitUiFormatHistoryTest::RunTest(const FString&)
{
    const FString Previous=GameXXKLocalization::GetLanguage();
    ON_SCOPE_EXIT{GameXXKLocalization::SetLanguage(Previous,false);};
    GameXXKLocalization::SetLanguage(TEXT("en"),false);
    const FText Chapter=GameXXKLocalization::Compact(FText::Format(
        GameXXKLocalization::Text(TEXT("Training.ChapterLabel")),FText::AsNumber(2)));
    TestEqual(TEXT("Formatting retains the explicit short chapter key"),Chapter.ToString(),FString(TEXT("Ch. 2")));
    const FText Difficulty=GameXXKLocalization::Localize(FText::Format(
        GameXXKLocalization::Text(TEXT("Training.DifficultySelector")),GameXXKLocalization::Source(TEXT("普通"))));
    TestFalse(TEXT("Difficulty prefix never returns to Chinese"),Difficulty.ToString().Contains(TEXT("难度")));
    GameXXKLocalization::SetLanguage(TEXT("zh-Hans"),false);
    TestEqual(TEXT("The existing compact format still switches back"),Chapter.ToString(),FString(TEXT("第2章")));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKLocalizationDynamicSentenceTest,
    "GameXXK.Localization.DynamicSentenceHistory", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKLocalizationDynamicSentenceTest::RunTest(const FString&)
{
    const FString Previous=GameXXKLocalization::GetLanguage();
    ON_SCOPE_EXIT {GameXXKLocalization::SetLanguage(Previous,false);};
    GameXXKLocalization::SetLanguage(TEXT("zh-Hans"),false);
    const FText Positive=GameXXKLocalization::Source(TEXT("攻击 +17"));
    const FText Negative=GameXXKLocalization::Source(TEXT("攻击 -9"));
    const FText Large=GameXXKLocalization::Source(TEXT("金币 +9000000000"));
    const FText Nested=GameXXKLocalization::Source(TEXT("类型：护甲"));
    GameXXKLocalization::SetLanguage(TEXT("en"),false);
    TestEqual(TEXT("Existing positive sentence updates without reconstruction"),Positive.ToString(),FString(TEXT("Attack +17")));
    TestEqual(TEXT("Signed captures preserve their sign"),Negative.ToString(),FString(TEXT("Attack -9")));
    TestEqual(TEXT("Large integer stays exact without narrowing"),Large.ToString(),FString(TEXT("Gold +9000000000")));
    TestEqual(TEXT("String arguments resolve their own display identity"),Nested.ToString(),FString(TEXT("Type: Armor")));
    GameXXKLocalization::SetLanguage(TEXT("zh-Hans"),false);
    TestEqual(TEXT("Switch back preserves original values"),Positive.ToString(),FString(TEXT("攻击 +17")));
    TestEqual(TEXT("Nested argument switches back"),Nested.ToString(),FString(TEXT("类型：护甲")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKLocalizationWholeSentenceTest,
    "GameXXK.Localization.TemplateMatchingIsWholeSentence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKLocalizationWholeSentenceTest::RunTest(const FString&)
{
    const FString Previous=GameXXKLocalization::GetLanguage();
    ON_SCOPE_EXIT {GameXXKLocalization::SetLanguage(Previous,false);};
    GameXXKLocalization::SetLanguage(TEXT("en"),false);
    const FString Unknown(TEXT("攻击 +17未配置尾部"));
    TestEqual(TEXT("Unmatched suffix cannot be silently translated by fragments"),GameXXKLocalization::Source(Unknown).ToString(),Unknown);
    TestTrue(TEXT("Missing source is auditable"),GameXXKLocalization::GetMissingSources().Contains(Unknown));
    TestEqual(TEXT("Decimal percentage keeps authored precision"),GameXXKLocalization::Source(TEXT("+12.50个百分点")).ToString(),FString(TEXT("+12.50 percentage points")));
    TestEqual(TEXT("A percent escape becomes a literal percent"),GameXXKLocalization::Source(TEXT("HUD缩放已切换为 75%")).ToString(),FString(TEXT("HUD scale set to 75%")));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKLocalizationParagraphOrderTest,
    "GameXXK.Localization.ParagraphKeepsIndependentEffectOrder",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKLocalizationParagraphOrderTest::RunTest(const FString&)
{
    const FString Previous=GameXXKLocalization::GetLanguage();
    ON_SCOPE_EXIT{GameXXKLocalization::SetLanguage(Previous,false);};
    GameXXKLocalization::SetLanguage(TEXT("en"),false);
    const FText Paragraph=GameXXKLocalization::Source(TEXT("出牌者获得2点护甲\n出牌者恢复5点生命"));
    TestEqual(TEXT("A later line cannot consume an earlier effect as its target"),Paragraph.ToString(),FString(TEXT("Caster: +2 Armor\nRestore 5 HP to Caster")));
    GameXXKLocalization::SetLanguage(TEXT("zh-Hans"),false);
    TestEqual(TEXT("Both effect lines retain reversible history"),Paragraph.ToString(),FString(TEXT("出牌者获得2点护甲\n出牌者恢复5点生命")));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKLocalizationCompactDamageTest,
    "GameXXK.Localization.CompactDamageAndFollowupRemainComplete",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKLocalizationCompactDamageTest::RunTest(const FString&)
{
    const FString Previous=GameXXKLocalization::GetLanguage();
    ON_SCOPE_EXIT{GameXXKLocalization::SetLanguage(Previous,false);};
    GameXXKLocalization::SetLanguage(TEXT("en"),false);
    const FText Body=GameXXKLocalization::Source(TEXT("造成21点物理伤害；其他角色的下一张主动牌气力-1"));
    TestEqual(TEXT("Real compact separators retain both effects and translate the resolved damage"),Body.ToString(),
        FString(TEXT("Deal 21 Physical damage.\nThe next active card from another character costs 1 less AP.")));
    TestFalse(TEXT("No mitigation header is inserted"),Body.ToString().Contains(TEXT("type")));
    const FString Multi=GameXXKLocalization::Source(TEXT("攻击3次，每次造成116点物理伤害")).ToString();
    TestEqual(TEXT("Multi-hit count and per-hit damage stay separate"),Multi,FString(TEXT("Hit 3 times for 116 Physical damage each.")));
    return true;
}
#endif
