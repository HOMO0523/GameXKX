#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKSaveMigration.h"
#include "Narrative/GameXXKMainStorySubsystem.h"
#include "UI/GameXXKMainStoryDialoguePresentation.h"
#include "MVP/GameXXKSaveStorage.h"
#include "MVP/GameXXKSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ScopeExit.h"
#include "UI/GameXXKOneGameRouteMapWidget.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
bool CompleteOutsidePrerequisites(FGameXXKRuntimeState& State)
{
	for(FName Id:{FName(TEXT("S00-01")),FName(TEXT("S00-02")),FName(TEXT("S00-03"))})
	{
		if(!FGameXXKMainStoryRules::StartNode(State,Id))return false;
		const auto* Node=FGameXXKMainStoryCatalog::FindNode(Id);
		for(int32 I=0;I<Node->Lines.Num();++I)if(!FGameXXKMainStoryRules::AdvanceDialogue(State))return false;
		if(Node->IsInvestigation())for(int32 I=0;I<Node->Options.Num();++I)if(Node->Options[I].bCorrect)
		{FText Feedback;if(!FGameXXKMainStoryRules::ChooseAnswer(State,I,Feedback))return false;break;}
	}
	return true;
}
bool ReadPrelude(UGameXXKMainStorySubsystem* Story)
{
	const auto* Node=FGameXXKMainStoryCatalog::FindNode(TEXT("S00-04"));
	for(int32 I=0;I<Node->Lines.Num();++I)if(!Story->AdvanceDialogue())return false;
	return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKMainStoryOutsideDepartureTest,
	"GameXXK.MainStory.OutsidePreludeExplicitDepartureAndAutomaticBattle",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKMainStoryOutsideDepartureTest::RunTest(const FString&)
{
	auto* Instance=NewObject<UGameInstance>();auto* MVP=NewObject<UGameXXKMVPSubsystem>(Instance);
	if(!MVP->StartGame())return false;
	auto* Story=NewObject<UGameXXKMainStorySubsystem>(Instance);Story->SetMVPForTest(MVP);
	TestFalse(TEXT("task cannot bypass earlier outside prerequisites"),Story->StartTask(TEXT("S00-04")));
	if(!CompleteOutsidePrerequisites(MVP->GetMutableRuntimeState())||!Story->StartTask(TEXT("S00-04")))return false;
	TestFalse(TEXT("starting the prelude does not enter the stage"),MVP->GetRuntimeState().Training.bChallengeActive);
	TestEqual(TEXT("prelude opens as dialogue"),MVP->GetRuntimeState().NarrativeProgress.MainStory.Phase,EGameXXKMainStoryActivityPhase::Dialogue);
	TestFalse(TEXT("cannot depart before reading the prelude"),Story->BeginTaskJourney());
	if(!ReadPrelude(Story))return false;
	TestFalse(TEXT("finishing the prelude still waits outside"),MVP->GetRuntimeState().Training.bChallengeActive);
	TestEqual(TEXT("completed prelude offers explicit departure"),MVP->GetRuntimeState().NarrativeProgress.MainStory.Phase,EGameXXKMainStoryActivityPhase::ReadyToTravel);
	const auto View=GameXXKMainStoryDialoguePresentation::Build(MVP->GetRuntimeState(),FText::GetEmpty());
	if(!TestEqual(TEXT("one departure action is shown"),View.Options.Num(),1))return false;
	TestEqual(TEXT("action dispatches travel, not battle"),View.Options[0].OptionId,FName(TEXT("MainStory.Travel")));
	TestEqual(TEXT("action names the stage"),View.Options[0].Text.ToString(),FString(TEXT("进入1-1")));
	Story->PauseActivity();if(!Story->StartTask(TEXT("S00-04")))return false;
	TestEqual(TEXT("resume does not repeat the prelude"),MVP->GetRuntimeState().NarrativeProgress.MainStory.Phase,EGameXXKMainStoryActivityPhase::ReadyToTravel);
	if(!TestTrue(TEXT("explicit departure enters stage"),Story->BeginTaskJourney()))return false;
	TestTrue(TEXT("task uses its own route identity"),FGameXXKMainStoryRules::IsDedicatedJourney(MVP->GetRuntimeState()));
	TestEqual(TEXT("short task map contains only departure and its objective"),MVP->GetRuntimeState().RouteMapNodes.Num(),2);
	auto* RouteView=NewObject<UGameXXKOneGameRouteMapWidget>();RouteView->SetMVPSubsystem(MVP);
	RouteView->SetRouteMapViewportGeometry(FVector2D::ZeroVector,FVector2D(1280,720));
	RouteView->TakeWidget();RouteView->RefreshFromState();
	TestTrue(TEXT("task map is visibly labelled separately"),RouteView->GetRouteSummaryViewForTest().StageName.ToString().StartsWith(TEXT("任务1-1")));
	TestTrue(TEXT("short task route fits in one viewport"),RouteView->GetRouteContentSizeForTest().Y<=720.f);
	auto Ordinary=MVP->GetRuntimeStateCopy();
	FGameXXKTrainingRules::GenerateChallengeRouteMap(Ordinary,TEXT("Training.Normal.1-1"),12345);
	TestFalse(TEXT("ordinary 1-1 uses a different route identity"),FGameXXKMainStoryRules::IsDedicatedJourney(Ordinary));
	TestTrue(TEXT("ordinary 1-1 retains its full map"),Ordinary.RouteMapNodes.Num()>2);
	const auto Journey=MVP->GetRuntimeState().NarrativeProgress.MainStory.JourneyId;
	TestFalse(TEXT("double click cannot start another route"),Story->BeginTaskJourney());
	if(!Story->StartTask(TEXT("S00-04")))return false;
	TestEqual(TEXT("same journey is resumed"),MVP->GetRuntimeState().NarrativeProgress.MainStory.JourneyId,Journey);
	const int32 Gate=MVP->GetRuntimeState().NarrativeProgress.MainStory.GateNodeIds.Array()[0];
	MVP->GetMutableRuntimeState().ReachableRouteNodeIds={Gate};
	if(!TestTrue(TEXT("clicking the task gate enters its real battle immediately"),Story->EnterJourneyGate(Gate)))return false;
	TestTrue(TEXT("gate click creates the battle"),MVP->GetRuntimeState().CardRun.bHasActiveCardBattle);
	TestEqual(TEXT("battle screen replaces the map dialogue"),MVP->GetRuntimeState().Screen,EGameXXKScreen::Battle);
	TestEqual(TEXT("battle is attached to this gate"),MVP->GetRuntimeState().CardRun.ActiveBattleSourceNodeId,Gate);
	TestFalse(TEXT("no stale objective dialogue over the battle"),GameXXKMainStoryDialoguePresentation::IsActive(MVP->GetRuntimeState()));
	TestFalse(TEXT("entry is not victory"),FGameXXKMainStoryRules::IsNodeCompleted(MVP->GetRuntimeState(),TEXT("S00-04")));
	TestFalse(TEXT("cannot click the gate twice during battle"),Story->EnterJourneyGate(Gate));
	const auto ClearedBefore=MVP->GetRuntimeState().Training.ClearedStageIds;
	const auto SelectedBefore=MVP->GetRuntimeState().Training.SelectedStageId;
	const int32 GoldBefore=MVP->GetRuntimeState().PlayerGold;
	MVP->GetMutableRuntimeState().CardRun.ActiveBattle.Phase=EGameXXKCardBattlePhase::Victory;
	bool Cleared=false;FGameXXKTrainingReward Reward;
	if(!TestTrue(TEXT("task victory resolves through its own receipt"),MVP->AdvanceTrainingChallengeEncounter(Cleared,Reward)))return false;
	TestFalse(TEXT("task victory does not clear ordinary 1-1"),Cleared);
	TestTrue(TEXT("ordinary unlocked stages are unchanged"),MVP->GetRuntimeState().Training.ClearedStageIds.Difference(ClearedBefore).IsEmpty()
		&&ClearedBefore.Difference(MVP->GetRuntimeState().Training.ClearedStageIds).IsEmpty());
	TestEqual(TEXT("ordinary stage selection is unchanged"),MVP->GetRuntimeState().Training.SelectedStageId,SelectedBefore);
	TestEqual(TEXT("no ordinary battle gold is granted"),MVP->GetRuntimeState().PlayerGold,GoldBefore);
	TestTrue(TEXT("no ordinary card reward is granted"),MVP->GetRuntimeState().CardRun.PendingReward.Options.IsEmpty());
	TestTrue(TEXT("victory opens the aftermath"),FGameXXKMainStoryRules::HasPendingAfterBattleDialogue(MVP->GetRuntimeState(),TEXT("S00-04")));
	TestFalse(TEXT("reward cannot bypass the aftermath"),Story->ClaimReward(TEXT("S00-04")));
	const auto* BattleNode=FGameXXKMainStoryCatalog::FindNode(TEXT("S00-04"));
	for(int32 I=0;I<BattleNode->AfterBattleLines.Num();++I)if(!Story->AdvanceDialogue())return false;
	TestTrue(TEXT("task completes after the aftermath"),FGameXXKMainStoryRules::IsNodeCompleted(MVP->GetRuntimeState(),TEXT("S00-04")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKMainStoryAtomicGateEntryTest,
	"GameXXK.MainStory.AutomaticGateBattleSaveFailureRollsBack",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKMainStoryAtomicGateEntryTest::RunTest(const FString&)
{
	auto* Instance=NewObject<UGameInstance>();auto* MVP=NewObject<UGameXXKMVPSubsystem>(Instance);
	if(!MVP->StartGame())return false;
	auto* Story=NewObject<UGameXXKMainStorySubsystem>(Instance);Story->SetMVPForTest(MVP);
	if(!CompleteOutsidePrerequisites(MVP->GetMutableRuntimeState())||!Story->StartTask(TEXT("S00-04"))||!ReadPrelude(Story)||!Story->BeginTaskJourney())return false;
	const int32 Gate=MVP->GetRuntimeState().NarrativeProgress.MainStory.GateNodeIds.Array()[0];
	MVP->GetMutableRuntimeState().ReachableRouteNodeIds={Gate};
	const auto Before=MVP->GetRuntimeStateCopy();
	MVP->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda([](USaveGame*,const FString&,int32){return false;}));
	TestFalse(TEXT("failed checkpoint does not publish battle entry"),Story->EnterJourneyGate(Gate));
	TestFalse(TEXT("failed write leaves the hero before the gate"),MVP->GetRuntimeState().NarrativeProgress.MainStory.bGateEntered);
	TestFalse(TEXT("failed write creates no live battle"),MVP->GetRuntimeState().CardRun.bHasActiveCardBattle);
	TestEqual(TEXT("failed write keeps the original route seed"),MVP->GetRuntimeState().RouteSeed,Before.RouteSeed);
	TestEqual(TEXT("failed write keeps the player on the map"),MVP->GetRuntimeState().Screen,Before.Screen);
	MVP->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda([](USaveGame*,const FString&,int32){return true;}));
	TestTrue(TEXT("retry enters the same encounter"),Story->EnterJourneyGate(Gate));
	TestTrue(TEXT("successful retry publishes the battle"),MVP->GetRuntimeState().CardRun.bHasActiveCardBattle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKMainStoryLegacyGateDialogueTest,
	"GameXXK.MainStory.LegacyGateDialogueFinishesIntoBattle",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKMainStoryLegacyGateDialogueTest::RunTest(const FString&)
{
	auto* Instance=NewObject<UGameInstance>();auto* MVP=NewObject<UGameXXKMVPSubsystem>(Instance);
	if(!MVP->StartGame())return false;
	auto* Story=NewObject<UGameXXKMainStorySubsystem>(Instance);Story->SetMVPForTest(MVP);
	if(!CompleteOutsidePrerequisites(MVP->GetMutableRuntimeState())||!Story->StartTask(TEXT("S00-04"))||!ReadPrelude(Story)||!Story->BeginTaskJourney())return false;
	auto& State=MVP->GetMutableRuntimeState();
	const int32 Gate=State.NarrativeProgress.MainStory.GateNodeIds.Array()[0];State.ReachableRouteNodeIds={Gate};
	if(!FGameXXKMainStoryRules::EnterJourneyGate(State,Gate))return false;
	const auto Journey=State.NarrativeProgress.MainStory.JourneyId;
	const auto* Node=FGameXXKMainStoryCatalog::FindNode(TEXT("S00-04"));
	State.NarrativeProgress.MainStory.LineIndex=Node->Lines.Num()-1;
	State.NarrativeProgress.TaskProgressById.FindChecked(Node->Id).ObjectiveCounts.Add(TEXT("MainStory.DialogueLine"),Node->Lines.Num()-1);
	State.NarrativeProgress.MainStory.Phase=EGameXXKMainStoryActivityPhase::Dialogue;
	if(!TestTrue(TEXT("old at-gate dialogue can finish normally"),Story->AdvanceDialogue()))return false;
	TestTrue(TEXT("last old dialogue line directly enters battle"),MVP->GetRuntimeState().CardRun.bHasActiveCardBattle);
	TestEqual(TEXT("old save keeps its original journey"),MVP->GetRuntimeState().NarrativeProgress.MainStory.JourneyId,Journey);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKMainStoryGateRealStorageTest,
	"GameXXK.MainStory.AutomaticGateBattleRealStorageRoundTrip",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKMainStoryGateRealStorageTest::RunTest(const FString&)
{
	const FString Slot=TEXT("GameXXK_Automation_StoryGateStorage_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
	ON_SCOPE_EXIT
	{
		UGameplayStatics::DeleteGameInSlot(Slot,0);
		for(int32 I=1;I<=FGameXXKSaveStorage::BackupCount;++I)
			UGameplayStatics::DeleteGameInSlot(Slot+FString::Printf(TEXT(".Previous%d"),I),0);
	};
	auto* Instance=NewObject<UGameInstance>();auto* MVP=NewObject<UGameXXKMVPSubsystem>(Instance);
	if(!MVP->StartGame())return false;
	FString StorageError;
	// Route the real checkpoint object through the real file writer in a unique
	// test slot. A successful stub would hide a serialization/checksum failure.
	MVP->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda(
		[&](USaveGame* Save,const FString&,int32 UserIndex){return FGameXXKSaveStorage::Write(Save,Slot,UserIndex,&StorageError);}));
	auto* Story=NewObject<UGameXXKMainStorySubsystem>(Instance);Story->SetMVPForTest(MVP);
	if(!CompleteOutsidePrerequisites(MVP->GetMutableRuntimeState())||!Story->StartTask(TEXT("S00-04"))||!ReadPrelude(Story)||!Story->BeginTaskJourney())
	{AddError(StorageError+TEXT(" / ")+Story->Feedback().ToString());return false;}
	const int32 Gate=MVP->GetRuntimeState().NarrativeProgress.MainStory.GateNodeIds.Array()[0];
	MVP->GetMutableRuntimeState().ReachableRouteNodeIds={Gate};
	const bool Entered=Story->EnterJourneyGate(Gate);
	if(!TestTrue(*FString::Printf(TEXT("real task-battle checkpoint persists: %s"),*StorageError),Entered))return false;
	bool Recovered=false;
	auto* Loaded=FGameXXKSaveStorage::Load(Slot,0,Recovered,&StorageError);
	if(!TestNotNull(TEXT("battle checkpoint reloads"),Loaded))return false;
	TestFalse(TEXT("reads the battle primary, not an earlier map backup"),Recovered);
	TestTrue(TEXT("battle checkpoint seal survives actual serialization"),FGameXXKSaveStorage::Verify(Loaded));
	TestTrue(TEXT("reloaded state contains a real battle"),Loaded->SaveState.RuntimeState.CardRun.bHasActiveCardBattle);
	TestEqual(TEXT("reloaded battle belongs to the clicked gate"),Loaded->SaveState.RuntimeState.CardRun.ActiveBattleSourceNodeId,Gate);
	TestEqual(TEXT("unit names survive the checkpoint"),Loaded->SaveState.RuntimeState.ActiveBattleEnemies[0].DisplayName.ToString(),
		MVP->GetRuntimeState().ActiveBattleEnemies[0].DisplayName.ToString());
	FGameXXKRuntimeState Restored;FGameXXKSaveMigrationReport Migration;
	TestTrue(TEXT("battle still passes normal restore validation"),FGameXXKSaveMigration::TryRestoreRuntimeState(Loaded->SaveState,Restored,Migration));
	Loaded->SaveState.RuntimeState.PlayerGold+=1;
	TestFalse(TEXT("payload tampering still invalidates the seal"),FGameXXKSaveStorage::Verify(Loaded));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKMainStoryAftermathResumeTest,
	"GameXXK.MainStory.AftermathPauseExitDiskResumeAndLegacyCompletion",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKMainStoryAftermathResumeTest::RunTest(const FString&)
{
	const FString Slot=TEXT("GameXXK_Automation_StoryAftermath_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
	ON_SCOPE_EXIT
	{
		UGameplayStatics::DeleteGameInSlot(Slot,0);
		for(int32 I=1;I<=FGameXXKSaveStorage::BackupCount;++I)
			UGameplayStatics::DeleteGameInSlot(Slot+FString::Printf(TEXT(".Previous%d"),I),0);
	};
	auto* Instance=NewObject<UGameInstance>();auto* MVP=NewObject<UGameXXKMVPSubsystem>(Instance);
	if(!MVP->StartGame())return false;
	FString Error;
	MVP->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda(
		[&](USaveGame* Save,const FString&,int32 UserIndex){return FGameXXKSaveStorage::Write(Save,Slot,UserIndex,&Error);}));
	auto* Story=NewObject<UGameXXKMainStorySubsystem>(Instance);Story->SetMVPForTest(MVP);
	const FName Id(TEXT("S00-04"));const auto* Node=FGameXXKMainStoryCatalog::FindNode(Id);
	if(!CompleteOutsidePrerequisites(MVP->GetMutableRuntimeState())||!Story->StartTask(Id)||!ReadPrelude(Story)||!Story->BeginTaskJourney())return false;
	const auto Journey=MVP->GetRuntimeState().NarrativeProgress.MainStory.JourneyId;
	const int32 Gate=MVP->GetRuntimeState().NarrativeProgress.MainStory.GateNodeIds.Array()[0];
	if(!Story->EnterJourneyGate(Gate))return false;
	MVP->GetMutableRuntimeState().CardRun.ActiveBattle.Phase=EGameXXKCardBattlePhase::Victory;
	bool Cleared=false;FGameXXKTrainingReward Reward;
	if(!MVP->AdvanceTrainingChallengeEncounter(Cleared,Reward))return false;
	TestFalse(TEXT("next story cannot bypass the aftermath"),Story->StartTask(TEXT("S00-05")));
	const auto Live=GameXXKMainStoryDialoguePresentation::Build(MVP->GetRuntimeState(),FText::GetEmpty());
	TestEqual(TEXT("victory shows the first aftermath line"),Live.Text.ToString(),Node->AfterBattleLines[0].Text.ToString());
	if(!Story->AdvanceDialogue())return false;
	Story->PauseActivity();
	if(!TestTrue(TEXT("can close the task route during aftermath"),MVP->CancelTrainingChallengeToWorkbench()))return false;
	if(!TestTrue(*FString::Printf(TEXT("paused aftermath saves: %s"),*Error),MVP->SaveCurrentGame(Slot,0)&&MVP->LoadGameFromSlot(Slot,0)))return false;
	if(!TestTrue(TEXT("reopening task resumes its aftermath"),Story->StartTask(Id)))return false;
	TestEqual(TEXT("resumes the unread line"),MVP->GetRuntimeState().NarrativeProgress.MainStory.LineIndex,1);
	TestEqual(TEXT("original journey identity is retained"),MVP->GetRuntimeState().NarrativeProgress.MainStory.JourneyId,Journey);
	TestFalse(TEXT("resume stays outside"),MVP->GetRuntimeState().Training.bChallengeActive);
	TestFalse(TEXT("cannot depart again after winning"),Story->BeginTaskJourney());
	TestFalse(TEXT("cannot start another battle after winning"),Story->BeginTaskBattle());
	TestFalse(TEXT("cannot claim before the last line"),Story->ClaimReward(Id));
	const auto Resumed=MVP->GetRuntimeStateCopy();
	for(int32 Invalid:{-1,Node->AfterBattleLines.Num()+1})
	{
		auto Broken=Resumed;Broken.NarrativeProgress.TaskProgressById.FindChecked(Id).ObjectiveCounts.Add(TEXT("MainStory.PostBattleLine"),Invalid);
		TestFalse(TEXT("invalid aftermath cursor is rejected"),FGameXXKMainStoryRules::ValidateState(Broken,&Error));
	}
	auto Unwon=Resumed;Unwon.NarrativeProgress.MainStory.bBattleWon=false;
	Unwon.NarrativeProgress.TaskProgressById.FindChecked(Id).ObjectiveCounts.Remove(TEXT("MainStory.BattleWon"));
	TestFalse(TEXT("aftermath cannot exist without a victory"),FGameXXKMainStoryRules::ValidateState(Unwon,&Error));
	for(int32 I=1;I<Node->AfterBattleLines.Num();++I)if(!Story->AdvanceDialogue())return false;
	TestTrue(TEXT("last line unlocks the next story"),FGameXXKMainStoryRules::NodeState(MVP->GetRuntimeState(),TEXT("S00-05"))==EGameXXKTaskState::Available);
	if(!TestTrue(TEXT("aftermath completion unlocks reward"),Story->ClaimReward(Id)))return false;
	const int32 Gold=MVP->GetRuntimeState().PlayerGold;
	TestFalse(TEXT("reward stays one-time"),Story->ClaimReward(Id));
	TestEqual(TEXT("duplicate claim changes no gold"),MVP->GetRuntimeState().PlayerGold,Gold);
	for(const auto OldState:{EGameXXKTaskState::Completed,EGameXXKTaskState::Rewarded})
	{
		auto Legacy=MVP->GetRuntimeStateCopy();auto& Progress=Legacy.NarrativeProgress.TaskProgressById.FindChecked(Id);
		Progress.State=OldState;Progress.ObjectiveCounts.Remove(TEXT("MainStory.BattleWon"));Progress.ObjectiveCounts.Remove(TEXT("MainStory.PostBattleLine"));
		Legacy.NarrativeProgress.MainStory.bBattleWon=false;
		if(!TestTrue(TEXT("old completed save reopens"),FGameXXKMainStoryRules::StartNode(Legacy,Id,&Error)))return false;
		TestEqual(TEXT("old completion goes straight to result"),Legacy.NarrativeProgress.MainStory.Phase,EGameXXKMainStoryActivityPhase::Result);
		TestFalse(TEXT("old completion never requires new aftermath"),FGameXXKMainStoryRules::HasPendingAfterBattleDialogue(Legacy,Id));
		TestTrue(TEXT("old completion still validates"),FGameXXKMainStoryRules::ValidateState(Legacy,&Error));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKMainStoryIllustrationFormationTest,
	"GameXXK.MainStory.IllustrationEnemyFormationsAndReplay",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKMainStoryIllustrationFormationTest::RunTest(const FString&)
{
	const TMap<FName,TArray<FName>> Expected={
		{TEXT("S00-04"),{TEXT("Enemy.Ch1.Rooster"),TEXT("Enemy.Ch1.Weasel")}},
		{TEXT("S01-07"),{TEXT("Enemy.Ch1.Weasel"),TEXT("Enemy.Ch1.Rooster"),TEXT("Enemy.Ch1.Civet")}},
		{TEXT("S03-10"),{TEXT("Enemy.Ch2.GrayWolf")}},
		{TEXT("S05-06"),{TEXT("Enemy.Ch2.GrayWolf"),TEXT("Enemy.Ch2.Porcupine")}}};
	for(const auto& Entry:Expected)
	{
		const auto* Node=FGameXXKMainStoryCatalog::FindNode(Entry.Key);
		if(!TestNotNull(TEXT("battle story exists"),Node))return false;
		TestTrue(TEXT("authored enemies match approved artwork"),Node->EnemyDefinitionIds==Entry.Value);
		FGameXXKTrainingEncounterDefinition Encounter;
		if(!TestTrue(TEXT("authored encounter builds"),FGameXXKTrainingRules::BuildFormationEncounter(
			FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal,Node->StageNumber),Node->EnemyDefinitionIds,Encounter)))return false;
		TestEqual(TEXT("enemy intent slots match the new formation"),Encounter.EnemySlots.Num(),Entry.Value.Num());
		for(int32 I=0;I<Encounter.EnemySlots.Num();++I)
		{
			TestEqual(TEXT("intent belongs to the correct actor"),Encounter.EnemySlots[I].EnemyDefinitionId,Entry.Value[I]);
			TestFalse(TEXT("opening intent is defined"),Encounter.EnemySlots[I].OpeningIntentId.IsNone());
			if(Entry.Value[I]==TEXT("Enemy.Ch2.GrayWolf"))TestEqual(TEXT("single wolf does not call a missing pack"),Encounter.EnemySlots[I].OpeningIntentId,FName(TEXT("Bite")));
		}
		for(int32 I=0;I<Node->AfterBattleLines.Num();++I)
			TestEqual(TEXT("replay includes every aftermath line after prelude"),
				GameXXKMainStoryDialoguePresentation::LineView(*Node,Node->Lines.Num()+I).Text.ToString(),Node->AfterBattleLines[I].Text.ToString());
		TestTrue(TEXT("replay ends after the last aftermath line"),Node->ReplayLine(Node->ReplayLineCount())==nullptr);
	}
	const auto Ordinary=FGameXXKTrainingRules::BuildEncounterSequence(TEXT("Training.Normal.1-1"));
	if(!TestFalse(TEXT("ordinary stage retains its encounters"),Ordinary.IsEmpty()))return false;
	TestTrue(TEXT("ordinary 1-1 is not rewritten by story art"),Ordinary[0].EnemyDefinitionIds==TArray<FName>{TEXT("Enemy.Ch1.Rooster"),TEXT("Enemy.Ch1.Goat"),TEXT("Enemy.Ch1.Civet")});
	return true;
}
#endif
