#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "MVP/GameXXKSaveMigration.h"
#include "GameXXKTrainingSettlementRules.h"
#include "UI/GameXXKOneGameRouteMapWidget.h"
#include "UI/GameXXKRouteMerchantWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "GameXXKPermanentPartyTestFixtures.h"
#include "UI/GameXXKTrainingSettlementWidget.h"
#include "Components/TextBlock.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKSoloChallengeSettlementTest,
    "GameXXK.ChallengeFlow.SoloBossUnlocksNextStage",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKSoloChallengeSettlementTest::RunTest(const FString&)
{
    auto* M=NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
    M->StartGame();
    TestTrue(TEXT("1-2 is available from the initial 1-1 clear"),FGameXXKTrainingRules::CanChallenge(M->GetRuntimeState().Training,TEXT("Training.Normal.1-2")));
    if(!TestTrue(TEXT("Start first challenge with hero alone"),M->StartTrainingChallenge(TEXT("Training.Normal.1-1"))))return false;
    auto& S=M->GetMutableRuntimeState();
    const auto* Boss=S.RouteMapNodes.FindByPredicate([](const auto& N){return N.NodeKind==EGameXXKNodeKind::Boss;});
    if(!TestNotNull(TEXT("Route has a Boss"),Boss))return false;
    const int32 BossId=Boss->NodeId;S.ReachableRouteNodeIds={BossId};
    if(!TestTrue(TEXT("Enter Boss"),M->SelectRouteNodeById(BossId)))return false;
    auto& Battle=M->GetMutableRuntimeState().CardRun.ActiveBattle;
    for(auto& U:Battle.Units)if(U.Side==EGameXXKCardTargetSide::Enemy){U.HP=0;U.bLiving=false;}
    Battle.Phase=EGameXXKCardBattlePhase::Victory;
    int32 Writes=0;
    M->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda([&](USaveGame*,const FString&,int32){++Writes;return true;}));
    bool Completed=false;FGameXXKTrainingReward Reward;
    if(!TestTrue(TEXT("Solo Boss victory settles"),M->AdvanceTrainingChallengeEncounter(Completed,Reward)))return false;
    TestTrue(TEXT("A pending settlement is presented"),M->HasPendingTrainingSettlement());
    const auto Receipt=M->GetPendingTrainingSettlementCopy();
    TestEqual(TEXT("Only the deployed hero has a growth row"),Receipt.Members.Num(),1);
    TestFalse(TEXT("Initial 1-1 was already cleared, so this is a replay"),Receipt.bFirstClear);
    TestTrue(TEXT("Replay does not announce an already-open stage as newly unlocked"),Receipt.UnlockedStageId.IsNone());
    TestTrue(TEXT("Next stage is now unlocked"),FGameXXKTrainingRules::CanChallenge(M->GetRuntimeState().Training,TEXT("Training.Normal.1-2")));
    TestEqual(TEXT("Awards committed once"),Writes,1);
    const int32 Gold=M->GetRuntimeState().PlayerGold;
    TestFalse(TEXT("Boss result cannot settle twice"),M->AdvanceTrainingChallengeEncounter(Completed,Reward));
    TestEqual(TEXT("No duplicate gold"),M->GetRuntimeState().PlayerGold,Gold);
    FString Error;TestTrue(TEXT("Solo receipt is save-valid"),FGameXXKSaveMigration::ValidateRuntimeState(M->GetRuntimeState(),Error));
    TestTrue(TEXT("Confirm receipt"),M->ConfirmTrainingSettlement(Receipt.ReceiptId));
    TestTrue(TEXT("Can enter unlocked 1-2"),M->StartTrainingChallenge(TEXT("Training.Normal.1-2")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKMerchantNodeVisualTest,
    "GameXXK.ChallengeFlow.MerchantExitUpdatesNodeVisual",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKMerchantNodeVisualTest::RunTest(const FString&)
{
    for (const bool ThroughWidget : {true, false})
    {
    auto* M=NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());M->StartGame();
    if(!M->StartTrainingChallenge(TEXT("Training.Normal.1-1")))return false;
    auto& S=M->GetMutableRuntimeState();
    int32 ShopIndex=S.RouteMapNodes.IndexOfByPredicate([](const auto& N){return N.NodeKind==EGameXXKNodeKind::Merchant && N.OutgoingNodeIds.Num()>0;});
    // A fixed intermediate node provides a merchant fixture even for a route roll without one.
    if(ShopIndex==INDEX_NONE){ShopIndex=1;S.RouteMapNodes[ShopIndex].NodeKind=EGameXXKNodeKind::Merchant;}
    const int32 ShopId=S.RouteMapNodes[ShopIndex].NodeId;
    const int32 NextId=S.RouteMapNodes[ShopIndex].OutgoingNodeIds[0];
    S.ReachableRouteNodeIds={ShopId};
    auto* PC=NewObject<AGameXXKMVPPlayerController>();PC->SetMVPSubsystemForTest(M);
    PC->SetDesktopTrainingBootProfileForTest(true);PC->EnsurePlayerFlowWidgetsForTest();PC->RefreshPlayerFlowWidgetsForTest();
    auto* Map=PC->GetRouteMapWidgetForTest();if(!TestNotNull(TEXT("Route map"),Map))return false;
    if(!TestTrue(TEXT("Enter merchant through map"),Map->ExecuteRouteNodeById(ShopId)))return false;
    auto* Shop=PC->GetRouteMerchantWidgetForTest();if(!TestNotNull(TEXT("Merchant panel"),Shop))return false;
    if(!TestTrue(TEXT("Leave merchant through its actual action"),(ThroughWidget ? Shop->LeaveMerchant() : M->ResolveMerchantRouteNode())))return false;
    TestTrue(TEXT("Next node is reachable"),M->GetRuntimeState().ReachableRouteNodeIds.Contains(NextId));
    TestEqual(TEXT("Merchant returns to the same desktop challenge context"),M->GetRuntimeState().CurrentMapId,FName(TEXT("DesktopTrainingHUD")));
    const int32 NextIndex=M->GetRuntimeState().RouteMapNodes.IndexOfByPredicate([&](const auto& N){return N.NodeId==NextId;});
    auto* Icon=Cast<UImage>(Map->WidgetTree->FindWidget(*FString::Printf(TEXT("RouteNodeFallbackIcon%d"),NextIndex)));
    if(!TestNotNull(TEXT("Next node visible icon"),Icon))return false;
    if (ThroughWidget) TestEqual(TEXT("Reachable next node is fully opaque after leaving shop"),Icon->GetColorAndOpacity().A,1.0f);
    Map->NativeTick(FGeometry(),0.1f);
    TestEqual(TEXT("Next frame retains enabled tint"),Icon->GetColorAndOpacity().A,1.0f);
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKChallengePartySettlementMatrixTest,
    "GameXXK.ChallengeFlow.SettlementPartyMatrix",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKChallengePartySettlementMatrixTest::RunTest(const FString&)
{
    auto* Page=NewObject<UGameXXKTrainingSettlementWidget>();Page->TakeWidget();
    for(int32 Mask : {3,0,1,2})
    {
        auto* M=NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());M->StartGame();
        auto State=GameXXKPermanentPartyTestFixtures::MakeStartedState();
        if(!TestEqual(TEXT("Established-party fixture"),State.CardRun.OrderedFormation.Members.Num(),3))return false;
        State.CardRun.OrderedFormation.Members.RemoveAll([Mask](const auto& R)
        {return (R.Kind==EGameXXKPartyMemberKind::PermanentCompanion && !(Mask&1)) || (R.Kind==EGameXXKPartyMemberKind::QuestNpc && !(Mask&2));});
        FGameXXKPartyFormationRules::ProjectCompatibility(State);
        const auto Deployed=State.CardRun.OrderedFormation.Members;
        M->GetMutableRuntimeState()=State;
        if(!TestTrue(TEXT("Start challenge with chosen legal party"),M->StartTrainingChallenge(TEXT("Training.Normal.1-1"))))return false;
        auto& Active=M->GetMutableRuntimeState();
        const auto* Boss=Active.RouteMapNodes.FindByPredicate([](const auto& N){return N.NodeKind==EGameXXKNodeKind::Boss;});
        if(!Boss)return false;const int32 Id=Boss->NodeId;Active.ReachableRouteNodeIds={Id};
        if(!M->SelectRouteNodeById(Id))return false;
        auto& Battle=M->GetMutableRuntimeState().CardRun.ActiveBattle;
        for(auto& U:Battle.Units)if(U.Side==EGameXXKCardTargetSide::Enemy){U.HP=0;U.bLiving=false;}
        Battle.Phase=EGameXXKCardBattlePhase::Victory;
        bool Done=false;FGameXXKTrainingReward Reward;
        if(!TestTrue(TEXT("Party settlement succeeds"),M->AdvanceTrainingChallengeEncounter(Done,Reward)))return false;
        const auto Receipt=M->GetPendingTrainingSettlementCopy();
        TestEqual(TEXT("Growth row count equals deployed count"),Receipt.Members.Num(),Deployed.Num());
        for(int32 I=0;I<Deployed.Num() && I<Receipt.Members.Num();++I)
            TestEqual(TEXT("Growth rows keep deployment order"),Receipt.Members[I].MemberId,Deployed[I].MemberId);
        Page->SetReceipt(Receipt);
        for(int32 I=0;I<3;++I)
        {
            auto* Name=Page->WidgetTree->FindWidget(*FString::Printf(TEXT("TrainingSettlementMember%d"),I));
            TestEqual(TEXT("Unused previous-party rows are hidden"),Name->GetVisibility()==ESlateVisibility::Collapsed,I>=Deployed.Num());
        }
        auto* Stats=Cast<UTextBlock>(Page->WidgetTree->FindWidget(TEXT("TrainingSettlementStats")));
        TestTrue(TEXT("Stats use actual party denominator"),Stats && Stats->GetText().ToString().Contains(FString::Printf(TEXT("/ %d"),Deployed.Num())));
        auto Broken=M->GetRuntimeStateCopy();Broken.Training.PendingSettlement.Members.Reset();
        TestFalse(TEXT("Empty settlement party rejected"),FGameXXKTrainingSettlementRules::ValidatePending(Broken));
    }
    return true;
}
#endif
