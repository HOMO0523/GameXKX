#include "Misc/AutomationTest.h"
#include "Narrative/GameXXKMainStoryCatalog.h"
#include "Narrative/GameXXKMainStoryRules.h"
#include "GameXXKTrainingRules.h"
#include "Narrative/GameXXKMainStorySubsystem.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKSaveMigration.h"
#include "Engine/GameInstance.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKMainStoryCatalogContractTest, "GameXXK.MainStory.CatalogContract", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKMainStoryCatalogContractTest::RunTest(const FString& Parameters)
{
	FString Error;
	TestTrue(TEXT("authored catalog validates"), FGameXXKMainStoryCatalog::Validate(&Error));
	TestEqual(TEXT("six chapters"), FGameXXKMainStoryCatalog::Chapters().Num(), 6);
	TestEqual(TEXT("sixty-one nodes"), FGameXXKMainStoryCatalog::Nodes().Num(), 61);
	TestEqual(TEXT("final spirit name"), FGameXXKMainStoryCatalog::CharacterName(TEXT("you_bai")).ToString(), FString(TEXT("幽白")));
	TestEqual(TEXT("final heroine name"), FGameXXKMainStoryCatalog::CharacterName(TEXT("qiong_yao_er")).ToString(), FString(TEXT("琼幺儿")));
	int64 Gold = 0; int32 Normal = 0, Advanced = 0, Battles = 0;
	for (const auto& Node : FGameXXKMainStoryCatalog::Nodes())
	{
		Gold += Node.Gold; Normal += Node.NormalBoxes; Advanced += Node.AdvancedBoxes;
		Battles += Node.Kind == EGameXXKMainStoryNodeKind::JourneyBattle ? 1 : 0;
	}
	TestEqual(TEXT("gold total"), Gold, int64(6100000));
	TestEqual(TEXT("normal box total"), Normal, 60);
	TestEqual(TEXT("advanced box total"), Advanced, 60);
	TestEqual(TEXT("four real task battles"), Battles, 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKMainStoryOpeningAndRewardTest, "GameXXK.MainStory.OpeningAndRewardOnce", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKMainStoryOpeningAndRewardTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State;
	FGameXXKTrainingRules::InitializeNewGame(State.Training);
	State.PlayerGold = 123;
	TestTrue(TEXT("first chapter opens without requiring a new first clear"), FGameXXKMainStoryRules::IsChapterUnlocked(State, TEXT("S00")));
	TestFalse(TEXT("next chapter waits for mainline completion"), FGameXXKMainStoryRules::IsChapterUnlocked(State, TEXT("S01")));
	TestTrue(TEXT("new chapter red dot"), FGameXXKMainStoryRules::HasUnseenChapter(State));
	TestTrue(TEXT("view chapter records notification"), FGameXXKMainStoryRules::MarkChapterSeen(State, TEXT("S00")));
	TestFalse(TEXT("viewed chapter clears new notification"), FGameXXKMainStoryRules::HasUnseenChapter(State));
	TestFalse(TEXT("cannot skip entry prerequisites"), FGameXXKMainStoryRules::StartNode(State, TEXT("S00-04")));
	TestFalse(TEXT("cannot claim an unfinished node"), FGameXXKMainStoryRules::ClaimReward(State, TEXT("S00-01")));
	TestTrue(TEXT("start first node"), FGameXXKMainStoryRules::StartNode(State, TEXT("S00-01")));
	for (int32 I=0; I<20 && !FGameXXKMainStoryRules::IsNodeCompleted(State, TEXT("S00-01")); ++I)
		FGameXXKMainStoryRules::AdvanceDialogue(State);
	TestTrue(TEXT("dialogue completes the actual node"), FGameXXKMainStoryRules::IsNodeCompleted(State, TEXT("S00-01")));
	TestTrue(TEXT("unclaimed reward visible"), FGameXXKMainStoryRules::HasUnclaimedReward(State));
	TestTrue(TEXT("claim first reward"), FGameXXKMainStoryRules::ClaimReward(State, TEXT("S00-01")));
	TestEqual(TEXT("exact one hundred thousand awarded"), State.PlayerGold, 100123);
	TestFalse(TEXT("duplicate claim rejected"), FGameXXKMainStoryRules::ClaimReward(State, TEXT("S00-01")));
	TestEqual(TEXT("duplicate keeps wallet unchanged"), State.PlayerGold, 100123);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKMainStoryGateRowTest, "GameXXK.MainStory.GateRowCannotBeBypassed", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKMainStoryGateRowTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State;
	FGameXXKTrainingRules::InitializeNewGame(State.Training);
	State.Training.bChallengeActive = true;
	State.Training.ActiveChallengeStageId = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1);
	State.bHasGeneratedRouteMap = true; State.bDungeonActive = true; State.Screen = EGameXXKScreen::DungeonMap;
	State.RouteMapNodes = {
		FGameXXKRouteMapNode(0,0,0,EGameXXKNodeKind::Start,FVector2D(.5,0),{1}),
		FGameXXKRouteMapNode(1,1,0,EGameXXKNodeKind::Battle,FVector2D(.5,.2),{2}),
		FGameXXKRouteMapNode(2,2,0,EGameXXKNodeKind::Battle,FVector2D(.5,.4),{3,4,5,6}),
		FGameXXKRouteMapNode(3,3,0,EGameXXKNodeKind::Battle,FVector2D(.1,.6),{7}),
		FGameXXKRouteMapNode(4,3,1,EGameXXKNodeKind::Camp,FVector2D(.35,.6),{7}),
		FGameXXKRouteMapNode(5,3,2,EGameXXKNodeKind::Chest,FVector2D(.65,.6),{7}),
		FGameXXKRouteMapNode(6,3,3,EGameXXKNodeKind::Merchant,FVector2D(.9,.6),{7}),
		FGameXXKRouteMapNode(7,4,0,EGameXXKNodeKind::Boss,FVector2D(.5,.8),{})};
	TestTrue(TEXT("all four nodes become the authored story row"), FGameXXKMainStoryRules::InjectJourneyGate(State, TEXT("S00-04")));
	TestEqual(TEXT("row count follows generated map, not a hard-coded pair"), State.NarrativeProgress.MainStory.GateNodeIds.Num(), 4);
	for (int32 Id=3; Id<=6; ++Id)
	{
		TestTrue(TEXT("each branch is recognized as the same story gate"), FGameXXKMainStoryRules::IsJourneyGate(State, Id));
		TestEqual(TEXT("story gate renders as an event/question node"), State.RouteMapNodes[Id].NodeKind, EGameXXKNodeKind::Event);
	}
	FGameXXKRuntimeState Bypass = State;
	Bypass.RouteMapNodes[2].OutgoingNodeIds.Add(7);
	TestFalse(TEXT("malformed bypass is rejected rather than silently allowing it"), FGameXXKMainStoryRules::InjectJourneyGate(Bypass, TEXT("S00-04")));
	return true;
}
namespace
{
bool FinishStoryNode(FGameXXKRuntimeState& S,FName Id)
{
	if(!FGameXXKMainStoryRules::StartNode(S,Id))return false;
	const auto* N=FGameXXKMainStoryCatalog::FindNode(Id); if(!N)return false;
	for(int32 I=0;I<N->Lines.Num();++I)FGameXXKMainStoryRules::AdvanceDialogue(S);
	if(N->IsInvestigation()) for(int32 I=0;I<N->Options.Num();++I)if(N->Options[I].bCorrect){FText Feedback;FGameXXKMainStoryRules::ChooseAnswer(S,I,Feedback);break;}
	return FGameXXKMainStoryRules::IsNodeCompleted(S,Id);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKMainStoryTransactionTest,"GameXXK.MainStory.AtomicRewardAndMigration",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKMainStoryTransactionTest::RunTest(const FString& Parameters)
{
	auto* Instance=NewObject<UGameInstance>(); auto* M=NewObject<UGameXXKMVPSubsystem>(Instance);
	if(!TestTrue(TEXT("start valid party fixture"),M->StartGame()))return false;
	auto* Story=NewObject<UGameXXKMainStorySubsystem>(Instance);Story->SetMVPForTest(M);
	TestTrue(TEXT("actual subsystem starts task"),Story->StartTask(TEXT("S00-01")));
	for(int32 I=0;I<12&&!FGameXXKMainStoryRules::IsNodeCompleted(M->GetRuntimeState(),TEXT("S00-01"));++I)Story->AdvanceDialogue();
	if(!TestTrue(TEXT("subsystem dialogue completes"),FGameXXKMainStoryRules::IsNodeCompleted(M->GetRuntimeState(),TEXT("S00-01")))){AddError(Story->Feedback().ToString());return false;}
	const int32 Gold=M->GetRuntimeState().PlayerGold;
	M->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda([](USaveGame*,const FString&,int32){return false;}));
	TestFalse(TEXT("failed persistence rejects reward"),Story->ClaimReward(TEXT("S00-01")));
	TestEqual(TEXT("failed persistence keeps wallet"),M->GetRuntimeState().PlayerGold,Gold);
	TestEqual(TEXT("failed persistence leaves claim available"),FGameXXKMainStoryRules::NodeState(M->GetRuntimeState(),TEXT("S00-01")),EGameXXKTaskState::Completed);
	M->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda([](USaveGame*,const FString&,int32){return true;}));
	TestTrue(TEXT("retry saves and pays"),Story->ClaimReward(TEXT("S00-01")));
	TestEqual(TEXT("retry pays exactly once"),M->GetRuntimeState().PlayerGold,Gold+100000);
	const auto Saved=UGameXXKMVPRules::MakeSaveState(M->GetRuntimeState()); FGameXXKRuntimeState Restored;FGameXXKSaveMigrationReport Report;
	TestTrue(TEXT("current save round trips"),FGameXXKSaveMigration::TryRestoreRuntimeState(Saved,Restored,Report));
	TestFalse(TEXT("reload cannot pay again"),FGameXXKMainStoryRules::ClaimReward(Restored,TEXT("S00-01")));
	FGameXXKSaveState Old=Saved,Migrated;Old.SaveVersion=37;Old.RuntimeState.NarrativeProgress=FGameXXKNarrativeProgress();
	TestTrue(TEXT("v37 migration succeeds"),FGameXXKSaveMigration::MigrateToCurrent(Old,Migrated,Report));
	TestEqual(TEXT("migration preserves gold"),Migrated.RuntimeState.PlayerGold,Old.RuntimeState.PlayerGold);
	TestTrue(TEXT("migration does not invent completed tasks"),Migrated.RuntimeState.NarrativeProgress.TaskProgressById.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKMainStoryInvestigationTest,"GameXXK.MainStory.InvestigationHintsAndChapterBranches",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKMainStoryInvestigationTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState S;FGameXXKTrainingRules::InitializeNewGame(S.Training);
	TestTrue(TEXT("opening 1 completes"),FinishStoryNode(S,TEXT("S00-01")));
	TestTrue(TEXT("opening 2 completes"),FinishStoryNode(S,TEXT("S00-02")));
	TestTrue(TEXT("start investigation"),FGameXXKMainStoryRules::StartNode(S,TEXT("S00-03")));
	const auto* N=FGameXXKMainStoryCatalog::FindNode(TEXT("S00-03"));
	for(int32 I=0;I<N->Lines.Num();++I)FGameXXKMainStoryRules::AdvanceDialogue(S);
	const int32 Gold=S.PlayerGold;FText Feedback;int32 Right=INDEX_NONE,Wrong=INDEX_NONE;
	for(int32 I=0;I<N->Options.Num();++I)if(N->Options[I].bCorrect)Right=I;else Wrong=I;
	TestFalse(TEXT("wrong answer stays in investigation"),FGameXXKMainStoryRules::ChooseAnswer(S,Wrong,Feedback));
	TestFalse(TEXT("wrong answer does not complete"),FGameXXKMainStoryRules::IsNodeCompleted(S,N->Id));
	for(int32 I=0;I<3;++I)TestTrue(TEXT("free progressive hint"),FGameXXKMainStoryRules::RevealHint(S,Feedback));
	TestEqual(TEXT("third hint gives the final answer"),Feedback.ToString(),N->Hints.Last().ToString());
	TestEqual(TEXT("hint has no currency cost"),S.PlayerGold,Gold);
	TestTrue(TEXT("correct answer completes"),FGameXXKMainStoryRules::ChooseAnswer(S,Right,Feedback));
	TestTrue(TEXT("full reward still available"),FGameXXKMainStoryRules::ClaimReward(S,N->Id));
	TestEqual(TEXT("hint does not reduce reward"),S.PlayerGold,Gold+100000);
	TestTrue(TEXT("optional branch can still start"),FinishStoryNode(S,TEXT("S00-07")));
	TestTrue(TEXT("optional branch pays separately"),FGameXXKMainStoryRules::ClaimReward(S,TEXT("S00-07")));
	TestEqual(TEXT("branch is additional reward"),S.PlayerGold,Gold+200000);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKMainStoryJourneyTest,"GameXXK.MainStory.JourneyResumeAndBattleEvidence",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKMainStoryJourneyTest::RunTest(const FString& Parameters)
{
	auto* Instance=NewObject<UGameInstance>();auto* M=NewObject<UGameXXKMVPSubsystem>(Instance);if(!M->StartGame())return false;
	auto* Story=NewObject<UGameXXKMainStorySubsystem>(Instance);Story->SetMVPForTest(M);
	for(FName Id:{FName(TEXT("S00-01")),FName(TEXT("S00-02")),FName(TEXT("S00-03"))})if(!FinishStoryNode(M->GetMutableRuntimeState(),Id))return false;
	TestTrue(TEXT("task starts the corresponding route"),Story->StartTask(TEXT("S00-04")));
	const auto Journey=M->GetRuntimeState().NarrativeProgress.MainStory.JourneyId;
	const auto Gates=M->GetRuntimeState().NarrativeProgress.MainStory.GateNodeIds;
	if(!TestFalse(TEXT("gate row exists"),Gates.IsEmpty()))return false;
	TestTrue(TEXT("pausing and resuming uses existing route"),Story->StartTask(TEXT("S00-04")));
	TestEqual(TEXT("journey identity retained"),M->GetRuntimeState().NarrativeProgress.MainStory.JourneyId,Journey);
	TestFalse(TEXT("cannot enter a distant gate"),Story->EnterJourneyGate(Gates.Array()[0]));
	const int32 Gate=Gates.Array()[0];M->GetMutableRuntimeState().ReachableRouteNodeIds={Gate};
	TestTrue(TEXT("actual reachable story gate opens"),Story->EnterJourneyGate(Gate));
	const auto* N=FGameXXKMainStoryCatalog::FindNode(TEXT("S00-04"));for(int32 I=0;I<N->Lines.Num();++I)Story->AdvanceDialogue();
	TestFalse(TEXT("dialogue is not battle victory"),FGameXXKMainStoryRules::IsNodeCompleted(M->GetRuntimeState(),N->Id));
	const auto Formation=M->GetRuntimeState().CardRun.OrderedFormation;
	TestTrue(TEXT("uses real training battle entry"),Story->BeginTaskBattle());
	if(!TestTrue(TEXT("real card battle exists"),M->GetRuntimeState().CardRun.bHasActiveCardBattle)){AddError(Story->Feedback().ToString());return false;}
	TestEqual(TEXT("battle belongs to selected story gate"),M->GetRuntimeState().CardRun.ActiveBattleSourceNodeId,Gate);
	auto& S=M->GetMutableRuntimeState();
	S.CardRun.ActiveBattle.Phase=EGameXXKCardBattlePhase::Defeat;
	TestFalse(TEXT("defeat never satisfies story battle"),FGameXXKMainStoryRules::ObserveBattleVictory(S));
	S.CardRun.ActiveBattle.Phase=EGameXXKCardBattlePhase::Victory;S.CardRun.ActiveBattleSourceNodeId=Gate+10000;
	TestFalse(TEXT("unrelated victory never satisfies story battle"),FGameXXKMainStoryRules::ObserveBattleVictory(S));
	S.CardRun.ActiveBattleSourceNodeId=Gate;
	TestTrue(TEXT("matching terminal victory completes once"),FGameXXKMainStoryRules::ObserveBattleVictory(S));
	TestFalse(TEXT("duplicate victory is ignored"),FGameXXKMainStoryRules::ObserveBattleVictory(S));
	const int32 Normal=FGameXXKTrainingRules::CountChestTokens(S.Training,EGameXXKTrainingRewardTier::NormalChest);
	const int32 Advanced=FGameXXKTrainingRules::CountChestTokens(S.Training,EGameXXKTrainingRewardTier::AdvancedChest);
	TestTrue(TEXT("journey reward can be claimed once"),FGameXXKMainStoryRules::ClaimReward(S,N->Id));
	TestEqual(TEXT("ten ordinary chests"),FGameXXKTrainingRules::CountChestTokens(S.Training,EGameXXKTrainingRewardTier::NormalChest),Normal+10);
	TestEqual(TEXT("ten advanced chests"),FGameXXKTrainingRules::CountChestTokens(S.Training,EGameXXKTrainingRewardTier::AdvancedChest),Advanced+10);
	for(int32 I=S.Training.OwnedChestTokens.Num()-20;I<S.Training.OwnedChestTokens.Num();++I)TestEqual(TEXT("first chapter chest item level"),S.Training.OwnedChestTokens[I].SourceItemLevel,5);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKMainStoryCampaignTest,"GameXXK.MainStory.FullCampaignBothBranchesAndSixJourneys",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKMainStoryCampaignTest::RunTest(const FString& Parameters)
{
	auto* Instance=NewObject<UGameInstance>();auto* M=NewObject<UGameXXKMVPSubsystem>(Instance);if(!M->StartGame())return false;
	auto* Story=NewObject<UGameXXKMainStorySubsystem>(Instance);Story->SetMVPForTest(M);
	// Stage availability is a fixture input. Story prerequisites, dialogue, choices,
	// route creation, battle bridge, receipts and save validation run normally.
	for(int32 I=1;I<=6;++I)M->GetMutableRuntimeState().Training.ClearedStageIds.Add(FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal,I));
	int64 TaskGold=0;int32 Completed=0;TSet<FGuid> Journeys;
	const int32 BaseBoxes=M->GetRuntimeState().Training.OwnedChestTokens.Num();
	for(const auto& C:FGameXXKMainStoryCatalog::Chapters())
	{
		if(!TestTrue(*FString::Printf(TEXT("%s unlocks through previous mainline"),*C.Id.ToString()),Story->OpenChapter(C.Id))){AddError(Story->Feedback().ToString()+TEXT(" / ")+Story->GetProgressJson());return false;}
		for(int32 Done=0;Done<C.Nodes.Num();++Done)
		{
			FName Next=FGameXXKMainStoryRules::NextMainlineNode(M->GetRuntimeState(),C.Id);
			if(Next.IsNone())for(FName Id:C.Nodes)if(FGameXXKMainStoryRules::NodeState(M->GetRuntimeState(),Id)==EGameXXKTaskState::Available){Next=Id;break;}
			if(!TestFalse(TEXT("chapter has no dead end"),Next.IsNone()))return false;
			const auto* N=FGameXXKMainStoryCatalog::FindNode(Next);
			if(!Story->StartTask(Next)){AddError(Next.ToString()+TEXT(": ")+Story->Feedback().ToString());return false;}
			if(N->IsJourney())
			{
				const auto& Session=M->GetRuntimeState().NarrativeProgress.MainStory;Journeys.Add(Session.JourneyId);
				if(Session.GateNodeIds.IsEmpty()){AddError(TEXT("missing story gate"));return false;}
				const int32 Gate=Session.GateNodeIds.Array()[0];M->GetMutableRuntimeState().ReachableRouteNodeIds={Gate};
				if(!Story->EnterJourneyGate(Gate)){AddError(Story->Feedback().ToString());return false;}
			}
			for(int32 I=0;I<N->Lines.Num();++I)if(!Story->AdvanceDialogue()){AddError(Next.ToString()+TEXT(": ")+Story->Feedback().ToString());return false;}
			if(N->IsInvestigation())for(int32 I=0;I<N->Options.Num();++I)if(N->Options[I].bCorrect){if(!Story->ChooseAnswer(I)){AddError(Story->Feedback().ToString());return false;}break;}
			if(N->Kind==EGameXXKMainStoryNodeKind::JourneyBattle)
			{
				if(!Story->BeginTaskBattle()){AddError(Story->Feedback().ToString());return false;}
				// Inject a terminal outcome only in this automation fixture, through the real battle instance.
				M->GetMutableRuntimeState().CardRun.ActiveBattle.Phase=EGameXXKCardBattlePhase::Victory;
				bool StageDone=false;FGameXXKTrainingReward Reward;FString Error;
				if(!M->AdvanceTrainingChallengeEncounter(StageDone,Reward)){AddError(TEXT("battle completion bridge failed"));return false;}
				if(!M->SkipPendingRouteRewardAndFinish(&Error)){AddError(Error);return false;}
			}
			if(!TestTrue(*FString::Printf(TEXT("%s actual objective completed"),*Next.ToString()),FGameXXKMainStoryRules::IsNodeCompleted(M->GetRuntimeState(),Next)))return false;
			const int32 Gold=M->GetRuntimeState().PlayerGold;
			if(!Story->ClaimReward(Next)){AddError(Next.ToString()+TEXT(": ")+Story->Feedback().ToString());return false;}
			TaskGold+=M->GetRuntimeState().PlayerGold-Gold;++Completed;
			TestFalse(TEXT("each node refuses a second claim"),Story->ClaimReward(Next));
			FGameXXKRuntimeState Restored;FGameXXKSaveMigrationReport Report;
			if(!FGameXXKSaveMigration::TryRestoreRuntimeState(UGameXXKMVPRules::MakeSaveState(M->GetRuntimeState()),Restored,Report)){AddError(Next.ToString()+TEXT(": ")+Report.Error);return false;}
			if(N->Id==C.MainlineEnd)TestTrue(TEXT("mainline completes while unfinished side branches remain available"),Restored.NarrativeProgress.MainStory.MainlineCompletedChapters.Contains(C.Id));
		}
		if(M->GetRuntimeState().Training.bChallengeActive)TestTrue(TEXT("one chapter returns without rolling back completed nodes"),M->CancelTrainingChallengeToWorkbench());
		TestTrue(TEXT("ordinary replay still starts after story completion"),M->StartTrainingChallenge(FGameXXKMainStoryCatalog::StageId(C)));
		for(const auto& RouteNode:M->GetRuntimeState().RouteMapNodes)TestFalse(TEXT("ordinary replay never inherits a story row"),FGameXXKMainStoryRules::IsJourneyGate(M->GetRuntimeState(),RouteNode.NodeId));
		TestTrue(TEXT("ordinary replay can return before the next chapter"),M->CancelTrainingChallengeToWorkbench());
	}
	TestEqual(TEXT("all authored nodes and both branches completed"),Completed,61);
	TestEqual(TEXT("task-only gold total"),TaskGold,int64(6100000));
	TestEqual(TEXT("one journey per authored linear chapter"),Journeys.Num(),6);
	TestEqual(TEXT("six chapter bundles total 120 chests"),M->GetRuntimeState().Training.OwnedChestTokens.Num()-BaseBoxes,120);
	for(int32 Level:{5,10,15,20,25,30})
	{
		int32 Count=0;const auto& Tokens=M->GetRuntimeState().Training.OwnedChestTokens;
		for(int32 I=BaseBoxes;I<Tokens.Num();++I)if(Tokens[I].SourceItemLevel==Level)++Count;
		TestEqual(TEXT("each chapter gives twenty chests at its own level"),Count,20);
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKMainStoryExitSaveTest,"GameXXK.MainStory.RouteExitAtomicity",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKMainStoryExitSaveTest::RunTest(const FString& Parameters)
{
	auto* Instance=NewObject<UGameInstance>();auto* M=NewObject<UGameXXKMVPSubsystem>(Instance);if(!M->StartGame())return false;
	auto* Story=NewObject<UGameXXKMainStorySubsystem>(Instance);Story->SetMVPForTest(M);
	for(FName Id:{FName(TEXT("S00-01")),FName(TEXT("S00-02")),FName(TEXT("S00-03"))})if(!FinishStoryNode(M->GetMutableRuntimeState(),Id))return false;
	if(!Story->StartTask(TEXT("S00-04")))return false;
	const int32 Gold=M->GetRuntimeState().PlayerGold;
	M->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda([](USaveGame*,const FString&,int32){return false;}));
	FGameXXKRouteSettlementReceipt Receipt;FString Error;
	TestFalse(TEXT("story route cannot appear closed before its save succeeds"),M->SettleAndExitActiveRoute(Receipt,Error));
	TestTrue(TEXT("failed exit keeps the route open"),M->GetRuntimeState().Training.bChallengeActive);
	TestEqual(TEXT("failed exit does not publish ordinary route money"),M->GetRuntimeState().PlayerGold,Gold);
	M->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda([](USaveGame*,const FString&,int32){return true;}));
	TestTrue(TEXT("retry saves the return to desktop"),M->SettleAndExitActiveRoute(Receipt,Error));
	TestFalse(TEXT("saved exit is no longer a challenge"),M->GetRuntimeState().Training.bChallengeActive);
	TestTrue(TEXT("completed outside dialogue survives exit"),FGameXXKMainStoryRules::IsNodeCompleted(M->GetRuntimeState(),TEXT("S00-03")));
	return true;
}
#endif
