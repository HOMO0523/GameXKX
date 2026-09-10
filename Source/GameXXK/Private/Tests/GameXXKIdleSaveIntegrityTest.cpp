#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Kismet/GameplayStatics.h"
#include "MVP/GameXXKSaveGame.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "GameXXKTalentRules.h"
#include "GameXXKPermanentPartyTestFixtures.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
    UGameXXKMVPSubsystem* NewSaveTestSubsystem()
    {
        return NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
    }

    bool WriteIdleSaveFixture(const FString& Slot, const int64 LastUpdated)
    {
        auto* Source = NewSaveTestSubsystem();
        if (!Source->StartGame() || !Source->StartTrainingTravel(TEXT("Training.Normal.1-1"))) return false;
        auto State = Source->GetRuntimeStateCopy();
        State.Training.TravelLastUpdatedUnixSeconds = LastUpdated;
        auto* Save = NewObject<UGameXXKSaveGame>();
        Save->SaveState = UGameXXKMVPRules::MakeSaveState(State);
        return UGameplayStatics::SaveGameToSlot(Save, Slot, 0);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKIdleSaveOfflineUnlockTest,
    "GameXXK.SaveIntegrity.OfflineRespectsTalentUnlock", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKIdleSaveOfflineUnlockTest::RunTest(const FString&)
{
    const FString Slot(TEXT("GameXXK_Automation_IdleSave_Unlock"));
    ON_SCOPE_EXIT { UGameplayStatics::DeleteGameInSlot(Slot, 0); };
    if (!TestTrue(TEXT("Write isolated idle save"), WriteIdleSaveFixture(Slot, FDateTime::UtcNow().ToUnixTimestamp()-3600))) return false;
    auto* Loader = NewSaveTestSubsystem();
    if (!TestTrue(TEXT("Load valid save"), Loader->LoadGameFromSlot(Slot, 0))) return false;
    FGameXXKTalentProjection Projection;
    FGameXXKTalentRules::BuildProjection(Loader->GetRuntimeState().Talents, Projection);
    TestFalse(TEXT("Fixture has not unlocked offline income"), Projection.bOfflineRewardsUnlocked);
    const auto Reward = Loader->GetPendingTrainingTravelRewardCopy();
    TestEqual(TEXT("Locked offline income grants no Gold"), Reward.Gold, 0);
    TestEqual(TEXT("Locked offline income grants no XP"), Reward.Experience, 0);
    TestEqual(TEXT("Locked offline income grants no chests"), Reward.NormalChestCount+Reward.AdvancedChestCount+Reward.HuntChestCount, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKIdleSaveClockRollbackTest,
    "GameXXK.SaveIntegrity.ClockRollbackKeepsHighWatermark", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKIdleSaveClockRollbackTest::RunTest(const FString&)
{
    const FString Slot(TEXT("GameXXK_Automation_IdleSave_ClockRollback"));
    ON_SCOPE_EXIT { UGameplayStatics::DeleteGameInSlot(Slot, 0); };
    const int64 HighWatermark = FDateTime::UtcNow().ToUnixTimestamp()+3600;
    if (!TestTrue(TEXT("Write future clock fixture"), WriteIdleSaveFixture(Slot, HighWatermark))) return false;
    auto* Loader = NewSaveTestSubsystem();
    if (!TestTrue(TEXT("Clock rollback still allows valid load"), Loader->LoadGameFromSlot(Slot, 0))) return false;
    TestEqual(TEXT("Clock rollback must not reopen an already accounted interval"), Loader->GetRuntimeState().Training.TravelLastUpdatedUnixSeconds, HighWatermark);
    TestEqual(TEXT("Clock rollback pays no income"), Loader->GetPendingTrainingTravelRewardCopy().Gold, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKIdleSaveClockSaveTest,
    "GameXXK.SaveIntegrity.SaveKeepsClockHighWatermark",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKIdleSaveClockSaveTest::RunTest(const FString&)
{
    const FString Slot=TEXT("GameXXK_Automation_IdleSave_Future_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
    auto* Subsystem=NewSaveTestSubsystem();
    ON_SCOPE_EXIT {Subsystem->DeleteSaveGame(Slot,0);};
    if(!TestTrue(TEXT("Create travel fixture"),Subsystem->StartGame()&&Subsystem->StartTrainingTravel(TEXT("Training.Normal.1-1"))))return false;
    const int64 Watermark=FDateTime::UtcNow().ToUnixTimestamp()+3600;
    Subsystem->GetMutableRuntimeState().Training.TravelLastUpdatedUnixSeconds=Watermark;
    if(!TestTrue(TEXT("Manual save succeeds during a clock rollback"),Subsystem->SaveCurrentGame(Slot,0)))return false;
    const auto* Written=Cast<UGameXXKSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot,0));
    if(!TestNotNull(TEXT("Saved slot reads"),Written))return false;
    TestEqual(TEXT("Saving cannot reopen consumed offline time"),Written->SaveState.RuntimeState.Training.TravelLastUpdatedUnixSeconds,Watermark);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKIdleSaveLoadCommitFailureTest,
    "GameXXK.SaveIntegrity.LoadRejectsOfflineCommitFailure", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKIdleSaveLoadCommitFailureTest::RunTest(const FString&)
{
    const FString Slot(TEXT("GameXXK_Automation_IdleSave_CommitFailure"));
    ON_SCOPE_EXIT { UGameplayStatics::DeleteGameInSlot(Slot, 0); };
    if (!TestTrue(TEXT("Write isolated save"), WriteIdleSaveFixture(Slot, FDateTime::UtcNow().ToUnixTimestamp()-3600))) return false;
    auto* Loader = NewSaveTestSubsystem();
    if (!TestTrue(TEXT("Create current runtime"), Loader->StartGame())) return false;
    Loader->GetMutableRuntimeState().PlayerGold=731;
    int32 Attempts=0;
    Loader->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda(
        [&Attempts](USaveGame*, const FString&, int32) { ++Attempts; return false; }));
    TestFalse(TEXT("An unsaved offline transaction must not become the active runtime"), Loader->LoadGameFromSlot(Slot, 0));
    TestTrue(TEXT("Load attempted its durable commit"), Attempts>0);
    TestEqual(TEXT("Failed load preserves the previous runtime"), Loader->GetRuntimeState().PlayerGold, 731);
    Loader->ResetSaveSlotWriteDelegateForTest();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKIdleSaveConsumedTimeTest,
    "GameXXK.SaveIntegrity.LoadPersistsConsumedTime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKIdleSaveConsumedTimeTest::RunTest(const FString&)
{
    const FString Slot(TEXT("GameXXK_Automation_IdleSave_ConsumedTime"));
    ON_SCOPE_EXIT { UGameplayStatics::DeleteGameInSlot(Slot, 0); };
    const int64 BeforeLoad=FDateTime::UtcNow().ToUnixTimestamp();
    if (!TestTrue(TEXT("Write old timestamp fixture"), WriteIdleSaveFixture(Slot, BeforeLoad-7200))) return false;
    auto* Loader=NewSaveTestSubsystem();
    if (!TestTrue(TEXT("Load succeeds"), Loader->LoadGameFromSlot(Slot, 0))) return false;
    const auto* Written=Cast<UGameXXKSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot,0));
    if (!TestNotNull(TEXT("Read committed source slot"), Written)) return false;
    TestTrue(TEXT("Observed offline interval is consumed in the original slot before load returns"), Written->SaveState.RuntimeState.Training.TravelLastUpdatedUnixSeconds>=BeforeLoad);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKIdleSaveClaimCommitFailureTest,
    "GameXXK.SaveIntegrity.ClaimFailureKeepsPendingReward", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKIdleSaveClaimCommitFailureTest::RunTest(const FString&)
{
    auto* Subsystem=NewSaveTestSubsystem();
    if (!TestTrue(TEXT("Create independent player runtime"), Subsystem->StartGame())) return false;
    auto& State=Subsystem->GetMutableRuntimeState();
    State.Training.PendingTravelGold=123;
    State.Training.PendingTravelNormalChestCount=1;
    const int32 GoldBefore=State.PlayerGold;
    const int32 ChestsBefore=State.Training.OwnedChestTokens.Num();
    Subsystem->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda(
        [](USaveGame*, const FString&, int32) { return false; }));
    FGameXXKTrainingOfflineReward Reward;
    TestFalse(TEXT("Failed durable claim cannot report success"), Subsystem->CollectTrainingTravelRewards(Reward));
    TestEqual(TEXT("Unclaimed Gold stays available"), Subsystem->GetPendingTrainingTravelRewardCopy().Gold, 123);
    TestEqual(TEXT("Failed claim keeps wallet"), Subsystem->GetRuntimeState().PlayerGold, GoldBefore);
    TestEqual(TEXT("Failed claim creates no chest"), Subsystem->GetRuntimeState().Training.OwnedChestTokens.Num(), ChestsBefore);
    Subsystem->ResetSaveSlotWriteDelegateForTest();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKIdleSaveChestCommitFailureTest,
    "GameXXK.SaveIntegrity.ChestFailureKeepsTokenAndSeed", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKIdleSaveChestCommitFailureTest::RunTest(const FString&)
{
    auto* Subsystem=NewSaveTestSubsystem();
    if (!TestTrue(TEXT("Create independent player runtime"), Subsystem->StartGame())) return false;
    auto& State=Subsystem->GetMutableRuntimeState();
    FString Error;
    if (!TestTrue(TEXT("Add source-aware chest"), FGameXXKTrainingRules::AppendChestToken(State.Training,EGameXXKTrainingRewardTier::AdvancedChest,TEXT("Training.Normal.3-3"),1,&Error))) return false;
    const auto Before=State;
    Subsystem->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda(
        [](USaveGame*, const FString&, int32) { return false; }));
    FGameXXKTrainingChestOpenResult Result;
    TestFalse(TEXT("Chest opening must wait for durable commit"), Subsystem->OpenOneTrainingChest(EGameXXKTrainingRewardTier::AdvancedChest,Result));
    const auto& After=Subsystem->GetRuntimeState();
    TestEqual(TEXT("The unopened chest remains"), After.Training.OwnedChestTokens.Num(), Before.Training.OwnedChestTokens.Num());
    TestEqual(TEXT("Failure cannot reroll the reward seed"), After.Training.ChallengeRewardSeed, Before.Training.ChallengeRewardSeed);
    TestTrue(TEXT("Failure cannot grant item rewards"), After.Inventory.OrderIndependentCompareEqual(Before.Inventory));
    Subsystem->ResetSaveSlotWriteDelegateForTest();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKIdleSaveOverflowTest,
    "GameXXK.SaveIntegrity.PendingRewardOverflowIsAtomic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKIdleSaveOverflowTest::RunTest(const FString&)
{
    FGameXXKTrainingProgress Progress;
    Progress.PendingTravelGold=MAX_int32-2;
    Progress.PendingTravelExperience=31;
    FGameXXKTrainingOfflineReward Reward;
    Reward.Gold=10;
    Reward.Experience=7;
    TestFalse(TEXT("Reject overflow instead of wrapping the player's accumulated income"), FGameXXKTrainingRules::AccumulatePendingTravelReward(Progress,Reward));
    TestEqual(TEXT("Overflow preserves existing Gold"), Progress.PendingTravelGold, MAX_int32-2);
    TestEqual(TEXT("Overflow does not partly add XP"), Progress.PendingTravelExperience, 31);
    return true;
}
#endif
