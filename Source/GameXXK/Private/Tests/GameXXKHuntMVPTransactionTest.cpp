#include "Misc/AutomationTest.h"
#include "Kismet/GameplayStatics.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKSaveGame.h"
#include "MVP/GameXXKSaveMigration.h"
#include "GameXXKHuntRules.h"
#include "GameXXKRelicRules.h"
#include "GameXXKPermanentPartyTestFixtures.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKHuntMVPTransactionTest,
    "GameXXK.Hunt.MVPThreeBossTransactionAndResume",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKHuntMVPTransactionTest::RunTest(const FString&)
{
    auto* Sub=NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
    if(!TestTrue(TEXT("Create independent player runtime"),Sub->StartGame()))return false;
    auto& Initial=Sub->GetMutableRuntimeState();
    for(int32 Stage=1;Stage<=9;++Stage)Initial.Training.ClearedStageIds.Add(FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal,Stage));
    const FName Hunt(TEXT("Training.Normal.3-4")),Source(TEXT("Training.Normal.3-3"));
    const FName Order=FGameXXKHuntRules::RequiredOrder(Hunt);
    FString Error;
    if(!TestTrue(TEXT("First clear provides the only ticket"),FGameXXKHuntRules::GrantFirstClear(Initial,Source,&Error)))return false;
    if(!TestTrue(TEXT("MVP enters the ticketed Hunt"),Sub->StartTrainingChallenge(Hunt)))return false;
    FGameXXKRelicRules::AcquireRelic(Sub->GetMutableRuntimeState(),TEXT("Relic.TigerSeal"),&Error);
    const int32 GoldBefore=Sub->GetRuntimeState().PlayerGold;
    TArray<uint8> DurableBytes;int32 Writes=0;
    const auto WriteMemory=[&](USaveGame* Save,const FString&,int32)
    {++Writes;DurableBytes.Reset();return UGameplayStatics::SaveGameToMemory(Save,DurableBytes);};

    for(int32 Chapter=1;Chapter<=3;++Chapter)
    {
        auto& State=Sub->GetMutableRuntimeState();
        TestEqual(TEXT("MVP retained the expected chapter"),State.CardRun.RouteProgress.CurrentChapter,Chapter);
        const auto* Boss=State.RouteMapNodes.FindByPredicate([](const auto& Node){return Node.NodeKind==EGameXXKNodeKind::Boss;});
        if(!TestNotNull(TEXT("Each generated map has a Boss"),Boss))return false;
        const int32 BossId=Boss->NodeId;
        // This test exercises the real entry and settlement boundaries, not enemy balance.
        State.ReachableRouteNodeIds={BossId};
        if(!TestTrue(TEXT("Enter the authored Boss through MVP"),Sub->SelectRouteNodeById(BossId)))return false;
        Sub->GetMutableRuntimeState().CardRun.ActiveBattle.Phase=EGameXXKCardBattlePhase::Victory;
        Sub->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda([](USaveGame*,const FString&,int32){return false;}));
        bool Complete=false;FGameXXKTrainingReward Reward;
        TestFalse(TEXT("A failed chapter/final commit leaves the battle retryable"),Sub->AdvanceTrainingChallengeEncounter(Complete,Reward));
        TestEqual(TEXT("Failed commit keeps the ticket"),FGameXXKHuntRules::Balance(Sub->GetRuntimeState(),Order),1);
        TestEqual(TEXT("Failed commit cannot advance the chapter"),Sub->GetRuntimeState().CardRun.RouteProgress.CurrentChapter,Chapter);
        TestEqual(TEXT("Failed commit grants no Gold"),Sub->GetRuntimeState().PlayerGold,GoldBefore);
        Sub->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda(WriteMemory));
        const bool Advanced=Sub->AdvanceTrainingChallengeEncounter(Complete,Reward);
        if(!TestTrue(FString::Printf(TEXT("Retry commits the complete transition: %s"),*Sub->GetLastSaveLoadError().ToString()),Advanced))return false;
        TestEqual(TEXT("One successful write per chapter"),Writes,Chapter);
        TestEqual(TEXT("Only the last Boss completes the Hunt"),Complete,Chapter==3);
        auto* Saved=Cast<UGameXXKSaveGame>(UGameplayStatics::LoadGameFromMemory(DurableBytes));
        if(!TestNotNull(TEXT("Actual saved UObject reads"),Saved))return false;
        FGameXXKSaveState Restored;FGameXXKSaveMigrationReport Report;
        if(!TestTrue(TEXT("Committed chapter passes full save migration/validation"),FGameXXKSaveMigration::MigrateToCurrent(Saved->SaveState,Restored,Report)))return false;
        if(Chapter<3)
        {
            TestFalse(TEXT("Intermediate Boss creates no final receipt"),Sub->HasPendingTrainingSettlement());
            TestEqual(TEXT("Intermediate Boss keeps the ticket"),FGameXXKHuntRules::Balance(Restored.RuntimeState,Order),1);
            TestEqual(TEXT("Intermediate Boss grants no Gold"),Restored.RuntimeState.PlayerGold,GoldBefore);
            TestEqual(TEXT("Relic persists in the saved next chapter"),Restored.RuntimeState.CardRun.Relics.Num(),1);
            TestEqual(TEXT("Intermediate Boss grants no Hunt Chest"),Sub->GetTrainingChestCount(EGameXXKTrainingRewardTier::HuntChest),0);
            Sub->GetMutableRuntimeState()=Restored.RuntimeState;
        }
    }
    const auto Receipt=Sub->GetPendingTrainingSettlementCopy();
    TestTrue(TEXT("Final receipt exists"),Receipt.ReceiptId.IsValid());
    TestEqual(TEXT("One complete run spends one ticket"),FGameXXKHuntRules::Balance(Sub->GetRuntimeState(),Order),0);
    TestEqual(TEXT("Final receipt contains exactly one Hunt Chest"),Receipt.HuntChestCount,1);
    const auto Base=FGameXXKTrainingRules::BuildTravelReward(Source);
    TestEqual(TEXT("Whole run Gold is150% of one3-3 challenge"),Sub->GetRuntimeState().PlayerGold-GoldBefore,Base.Gold*3);
    if(!TestTrue(TEXT("Final receipt closes without another ticket"),Sub->ConfirmTrainingSettlement(Receipt.ReceiptId)))return false;
    TestFalse(TEXT("The last ticket cannot start unpaid travel"),Sub->GetRuntimeState().Training.bTravelActive);
    TestFalse(TEXT("The same receipt cannot pay again"),Sub->ConfirmTrainingSettlement(Receipt.ReceiptId));
    TestEqual(TEXT("Duplicate confirmation gives no extra chest"),Sub->GetTrainingChestCount(EGameXXKTrainingRewardTier::HuntChest),1);
    TestEqual(TEXT("Three chapter commits plus one acknowledgement"),Writes,4);
    Sub->ResetSaveSlotWriteDelegateForTest();
    return true;
}
#endif
