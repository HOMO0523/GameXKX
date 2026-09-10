#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "UI/GameXXKBattleAnimationPresentation.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKGemstyleKeyframePilotTest,
    "GameXXK.Presentation.BattleAnimation.GemstyleKeyframePilot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKGemstyleKeyframePilotTest::RunTest(const FString& Parameters)
{
    IConsoleVariable* Toggle = IConsoleManager::Get().FindConsoleVariable(TEXT("GameXXK.BattleAnimation.GemstyleKeyframes"));
    if (!TestNotNull(TEXT("A reversible keyframe pilot can be enabled"), Toggle)) return false;
    const int32 Previous = Toggle->GetInt();
    Toggle->Set(0, ECVF_SetByCode);
    const auto Original = FGameXXKBattleAnimationPresentation::ResolveClip(TEXT("Player"), false, EGameXXKBattleAnimationAction::Idle);
    const auto Guard = FGameXXKBattleAnimationPresentation::ResolveClip(TEXT("Companion_Guard_1"), false, EGameXXKBattleAnimationAction::Idle);
    Toggle->Set(1, ECVF_SetByCode);
    const auto Idle = FGameXXKBattleAnimationPresentation::ResolveClip(TEXT("Player"), false, EGameXXKBattleAnimationAction::Idle);
    const auto Attack = FGameXXKBattleAnimationPresentation::ResolveAttackClipForEvent(TEXT("Player"), NAME_None, false, 1);
    const auto OtherAttack = FGameXXKBattleAnimationPresentation::ResolveAttackClipForEvent(TEXT("Player"), NAME_None, false, 2);
    const auto Rooster = FGameXXKBattleAnimationPresentation::ResolveClipForDefinition(TEXT("OpaqueEnemy"), TEXT("Enemy.Ch1.Rooster"), true, EGameXXKBattleAnimationAction::Attack);
    TestEqual(TEXT("Idle uses only eight keyframes"), Idle.FrameCount, 8);
    TestEqual(TEXT("Idle holds each pose for a quarter second"), Idle.SourceFramesPerSecond, 4.0f);
    TestEqual(TEXT("Attack uses only ten keyframes"), Attack.FrameCount, 10);
    TestEqual(TEXT("Rooster uses ten attack keyframes"), Rooster.FrameCount, 10);
    TestEqual(TEXT("Both legacy hero variants resolve one pilot attack"), Attack.TexturePath, OtherAttack.TexturePath);
    TestTrue(TEXT("Pilot uses sibling assets"), Idle.TexturePath.ToString().Contains(TEXT("/GemstylePilot/")));
    TestEqual(TEXT("Idle wraps without sampling the next empty cell"), FGameXXKBattleAnimationPresentation::CalculateFrameIndex(Idle, 2.0f, true), 0);
    TestEqual(TEXT("Attack holds its last populated cell"), FGameXXKBattleAnimationPresentation::CalculateFrameIndex(Attack, 5.0f, false), 9);
    const auto Fitted = FGameXXKBattleAnimationPresentation::FitClipToDuration(Attack, 0.82f);
    TestEqual(TEXT("Fitted attack still reaches the last keyframe"), FGameXXKBattleAnimationPresentation::CalculateFrameIndex(Fitted, 0.819f, false), 9);
    TestEqual(TEXT("Unselected identities are unchanged"), FGameXXKBattleAnimationPresentation::ResolveClip(TEXT("Companion_Guard_1"), false, EGameXXKBattleAnimationAction::Idle).TexturePath, Guard.TexturePath);
    Toggle->Set(0, ECVF_SetByCode);
    TestEqual(TEXT("Disabling restores original clip"), FGameXXKBattleAnimationPresentation::ResolveClip(TEXT("Player"), false, EGameXXKBattleAnimationAction::Idle).TexturePath, Original.TexturePath);
    Toggle->Set(Previous, ECVF_SetByCode);
    return true;
}
#endif
