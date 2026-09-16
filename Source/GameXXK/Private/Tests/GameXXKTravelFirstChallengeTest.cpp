#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "GameXXKPartyFormationRules.h"
#include "UI/GameXXKDesktopTrainingWorkbenchWidget.h"
#include "UI/GameXXKInterfaceHelpWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTravelFirstChallengeGateTest,"GameXXK.TravelFirstChallenge.InitialClearAndTravelMilestone",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTravelFirstChallengeGateTest::RunTest(const FString&)
{
    auto* M=NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());M->StartGame();
    auto& S=M->GetMutableRuntimeState();auto& P=S.Training;
    const int32 Count=FGameXXKTrainingRules::BuildEncounterSequence(TEXT("Training.Normal.1-1"),true).Num();
    TestTrue(TEXT("Initial 1-1 challenge clear already opens 1-2"),FGameXXKTrainingRules::CanChallenge(P,TEXT("Training.Normal.1-2")));
    TestFalse(TEXT("Challenge access does not grant 1-2 travel"),FGameXXKTrainingRules::CanTravel(P,TEXT("Training.Normal.1-2")));
    for(int32 I=0;I<Count;++I)
    {
        bool Done=false;FGameXXKTrainingReward Reward;
        if(!TestTrue(TEXT("Advance authored travel encounter"),FGameXXKTrainingRules::AdvanceTravelEncounter(P,Done,Reward)))return false;
        TestEqual(TEXT("Only last encounter completes the loop"),Done,I==Count-1);
        TestTrue(TEXT("1-2 challenge stays available before and after travel"),FGameXXKTrainingRules::CanChallenge(P,TEXT("Training.Normal.1-2")));
        TestEqual(TEXT("Only a complete loop earns the guide milestone"),P.TravelVictories,I==Count-1?1:0);
    }
    TestTrue(TEXT("Companion talent becomes available at arrival to 1-2"),FGameXXKPartyFormationRules::IsSlotEligible(S,EGameXXKPartyMemberKind::PermanentCompanion));
    TestFalse(TEXT("Companion slot still requires its purchase"),FGameXXKPartyFormationRules::IsSlotUnlocked(S,EGameXXKPartyMemberKind::PermanentCompanion));
    TestFalse(TEXT("Travel does not open 1-3"),FGameXXKTrainingRules::CanChallenge(P,TEXT("Training.Normal.1-3")));
    TestFalse(TEXT("1-2 has not been cleared by travel in 1-1"),FGameXXKTrainingRules::CanTravel(P,TEXT("Training.Normal.1-2")));
    TestEqual(TEXT("Challenge-clear milestones are not fabricated"),P.PartyProgressionStep,0);
    TestTrue(TEXT("Guidance eligibility leaves travel running"),P.bTravelActive);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTravelFirstChallengeGuideTest,"GameXXK.TravelFirstChallenge.DefaultGuideTwoClicks",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTravelFirstChallengeGuideTest::RunTest(const FString&)
{
    auto* M=NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());M->StartGame();
    M->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda([](USaveGame*,const FString&,int32){return true;}));
    auto& S=M->GetMutableRuntimeState();S.Training.TravelVictories=1;S.PlayerGold=0;
    TestEqual(TEXT("Uses the actual new-game guide preference"),S.GuideProgress.Preference,EGameXXKGuidePreference::Unset);
    auto* Host=NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();Host->SetMVPSubsystem(M);Host->ConstructForTest();Host->OpenWorkbench();
    Host->OfferPartyProgressionGuide(true);
    auto* Help=Cast<UGameXXKInterfaceHelpWidget>(Host->WidgetTree->FindWidget(TEXT("DesktopInterfaceHelp")));
    if(!TestNotNull(TEXT("Travel success offers the next challenge"),Help)||!TestTrue(TEXT("Guide opens for default player"),Help->IsOpen()))return false;
    TestEqual(TEXT("Only two actions are introduced"),Help->GetStepCountForTest(),2);
    Help->NativeTick(FGeometry(),.2f);
    auto* Select=Cast<UButton>(Help->GetCurrentTargetForTest());
    if(!TestNotNull(TEXT("First points at the 1-2 map node"),Select))return false;
    TestEqual(TEXT("Correct stage node"),Select->GetFName(),FName(TEXT("TrainingNode_2")));
    TestTrue(TEXT("Opening guide leaves current 1-1 travel active"),S.Training.bTravelActive);
    Select->OnClicked.Broadcast();Help->NativeTick(FGeometry(),.2f);Help->NativeTick(FGeometry(),.2f);
    TestEqual(TEXT("Player selects 1-2"),S.Training.SelectedStageId,FName(TEXT("Training.Normal.1-2")));
    TestFalse(TEXT("Selecting the node does not auto-start challenge"),S.Training.bChallengeActive);
    auto* Challenge=Cast<UButton>(Help->GetCurrentTargetForTest());
    if(!TestNotNull(TEXT("Second points to the real challenge button"),Challenge))return false;
    TestEqual(TEXT("Actual challenge action"),Challenge->GetFName(),FName(TEXT("TrainingChallengeButton")));
    TestTrue(TEXT("Challenge is really enabled"),Challenge->GetIsEnabled());
    Challenge->OnClicked.Broadcast();
    TestTrue(TEXT("The player starts 1-2"),S.Training.bChallengeActive&&S.Training.ActiveChallengeStageId==TEXT("Training.Normal.1-2"));
    TestFalse(TEXT("Only entering challenge pauses travel"),S.Training.bTravelActive);
    TestTrue(TEXT("Entry is remembered before the desktop hides"),S.GuideProgress.CompletedGuideStepIds.Contains(TEXT("UI.Progression.V1.FirstChallenge12.Commit")));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTravelFirstChallengeLaterReplayTest,"GameXXK.TravelFirstChallenge.Initial11IsAlreadyCleared",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTravelFirstChallengeLaterReplayTest::RunTest(const FString&)
{
    auto* M=NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());M->StartGame();
    M->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda([](USaveGame*,const FString&,int32){return true;}));
    M->GetMutableRuntimeState().Training.TravelVictories=1;
    int32 Run=0;
    for(FName Stage:{FName(TEXT("Training.Normal.1-2")),FName(TEXT("Training.Normal.1-1")),FName(TEXT("Training.Normal.1-1"))})
    {
        if(!TestTrue(TEXT("Requested stage is challengeable"),M->StartTrainingChallenge(Stage)))return false;
        auto& S=M->GetMutableRuntimeState();
        const auto* Boss=S.RouteMapNodes.FindByPredicate([](const auto& N){return N.NodeKind==EGameXXKNodeKind::Boss;});
        if(!Boss)return false;const int32 NodeId=Boss->NodeId;S.ReachableRouteNodeIds={NodeId};
        if(!M->SelectRouteNodeById(NodeId))return false;
        auto& B=M->GetMutableRuntimeState().CardRun.ActiveBattle;
        for(auto& U:B.Units)if(U.Side==EGameXXKCardTargetSide::Enemy){U.HP=0;U.bLiving=false;}
        B.Phase=EGameXXKCardBattlePhase::Victory;
        bool Done=false;FGameXXKTrainingReward Reward;
        if(!TestTrue(TEXT("Normal settlement succeeds"),M->AdvanceTrainingChallengeEncounter(Done,Reward)))return false;
        const auto Receipt=M->GetPendingTrainingSettlementCopy();
        TestEqual(TEXT("1-2 is a first clear; initial 1-1 is always a replay"),Receipt.bFirstClear,Run==0);
        if(!M->ConfirmTrainingSettlement(Receipt.ReceiptId))return false;
        ++Run;
    }
    return true;
}
#endif
