#include "Narrative/GameXXKMainStorySubsystem.h"

#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "MVP/GameXXKSaveMigration.h"
#include "UI/GameXXKMainStoryPanelWidget.h"
#include "UI/GameXXKDialoguePanelWidget.h"
#include "UI/GameXXKMainStoryDialoguePresentation.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "TimerManager.h"

UGameXXKMVPSubsystem* UGameXXKMainStorySubsystem::MVP() const
{
	return MVPOverride ? MVPOverride.Get() : GetGameInstance() ? GetGameInstance()->GetSubsystem<UGameXXKMVPSubsystem>() : nullptr;
}
const FGameXXKRuntimeState* UGameXXKMainStorySubsystem::State() const { auto* M=MVP(); return M ? &M->GetRuntimeState() : nullptr; }
const FGameXXKMainStoryNode* UGameXXKMainStorySubsystem::ActiveNode() const
{
	const auto* S=State(); return S ? FGameXXKMainStoryCatalog::FindNode(S->NarrativeProgress.MainStory.ActiveNodeId) : nullptr;
}
FName UGameXXKMainStorySubsystem::PreferredChapter() const
{
	const auto* S=State(); if (!S) return TEXT("S00");
	if (!SelectedChapterId.IsNone() && FGameXXKMainStoryRules::IsChapterUnlocked(*S,SelectedChapterId)) return SelectedChapterId;
	if (const auto* N=ActiveNode()) return N->ChapterId;
	for (const auto& C:FGameXXKMainStoryCatalog::Chapters())
		if (FGameXXKMainStoryRules::IsChapterUnlocked(*S,C.Id) && !S->NarrativeProgress.MainStory.MainlineCompletedChapters.Contains(C.Id)) return C.Id;
	return TEXT("S00");
}
void UGameXXKMainStorySubsystem::SetError(const FString& Error) { LastFeedback=FText::FromString(Error); OnChanged.Broadcast(); }
bool UGameXXKMainStorySubsystem::Commit(FGameXXKRuntimeState&& Candidate, const bool bRefreshFlow)
{
	auto* M=MVP(); if (!M || M->IsAcademySessionActive()) { SetError(TEXT("先结束当前演武教学，再继续主线。")); return false; }
	FString Error;
	if (!FGameXXKSaveMigration::ValidateRuntimeState(Candidate,Error)) { SetError(TEXT("任务进度暂未保存：")+Error); return false; }
	if (!M->PersistTrainingCheckpoint(Candidate)) { SetError(TEXT("任务进度未能保存，请稍后重试。报酬仍可领取。")); return false; }
	M->GetMutableRuntimeState()=MoveTemp(Candidate);
	RefreshViews(bRefreshFlow); return true;
}
bool UGameXXKMainStorySubsystem::OpenChapter(const FName Id)
{
	const auto* S=State(); if (!S) return false;
	FGameXXKRuntimeState Candidate=*S;
	if (!FGameXXKMainStoryRules::MarkChapterSeen(Candidate,Id)) { SetError(TEXT("先完成上一章主线，并开放这一章对应的游历关卡。")); return false; }
	FGameXXKMainStoryRules::PauseActivity(Candidate);
	if (!Commit(MoveTemp(Candidate))) return false;
	SelectedChapterId=Id; LastFeedback=FText::GetEmpty(); OnChanged.Broadcast(); return true;
}
bool UGameXXKMainStorySubsystem::BeginJourney(FGameXXKRuntimeState& Candidate,const FGameXXKMainStoryNode& Node,FString& Error)
{
	auto& S=Candidate.NarrativeProgress.MainStory;
	const FName Stage=FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal,Node.StageNumber);
	if (Candidate.Training.bChallengeActive)
	{
		if (S.JourneyNodeId==Node.Id && S.JourneyStageId==Stage && !S.GateNodeIds.IsEmpty())
		{
			if(!FGameXXKMainStoryRules::IsDedicatedJourney(Candidate))
				return FGameXXKMainStoryRules::GenerateDedicatedJourneyMap(Candidate,Node.Id,S.JourneySeed,&Error);
			S.Phase=S.bGateEntered ? (S.LineIndex<Node.Lines.Num() ? EGameXXKMainStoryActivityPhase::Dialogue : Node.IsInvestigation() ? EGameXXKMainStoryActivityPhase::Choice : EGameXXKMainStoryActivityPhase::ReadyToBattle) : EGameXXKMainStoryActivityPhase::AwaitingGate;
			S.bShowJourneyTree=S.bGateEntered; return true;
		}
		Error=TEXT("当前游历尚未结束，回到桌面后再开启这段行程。"); return false;
	}
	if (Candidate.CardRun.bHasActiveCardBattle || Candidate.Training.PendingSettlement.ReceiptId.IsValid())
	{
		Error=TEXT("先处理眼前的战斗与结算，再动身。"); return false;
	}
	const bool Resume=S.JourneyNodeId==Node.Id && S.JourneyId.IsValid() && S.JourneySeed!=0;
	const int32 Seed=Resume ? S.JourneySeed : FMath::Max(1,static_cast<int32>(GetTypeHash(FGuid::NewGuid())&0x7fffffff));
	const FName OrdinarySelection=Candidate.Training.SelectedStageId;
	if (!FGameXXKTrainingRules::StartChallenge(Candidate.Training,Stage)) { Error=TEXT("对应游历关卡还没有开放。"); return false; }
	Candidate.Training.SelectedStageId=OrdinarySelection;
	return FGameXXKMainStoryRules::GenerateDedicatedJourneyMap(Candidate,Node.Id,Seed,&Error);
}
bool UGameXXKMainStorySubsystem::StartTask(const FName Id)
{
	const auto* S=State(); const auto* Node=FGameXXKMainStoryCatalog::FindNode(Id); if (!S || !Node) return false;
	if(S->DialogueSession.bActive || S->NarrativeSequenceSession.bActive){SetError(TEXT("先把眼前的话说完，再接这段主线。"));return false;}
	FGameXXKRuntimeState Candidate=*S; FString Error;
	if (!FGameXXKMainStoryRules::StartNode(Candidate,Id,&Error)) { SetError(Error); return false; }
	// Starting a task only opens its outside dialogue/departure prompt. An
	// already active journey resumes in place without generating another map.
	if (Node->IsJourney() && Candidate.Training.bChallengeActive && !FGameXXKMainStoryRules::IsNodeCompleted(Candidate,Id)
		&&!FGameXXKMainStoryRules::HasBattleVictory(Candidate,Id))
		if (!BeginJourney(Candidate,*Node,Error)) { SetError(Error); return false; }
	if (Candidate.Training.bChallengeActive && Candidate.NarrativeProgress.MainStory.Phase!=EGameXXKMainStoryActivityPhase::AwaitingGate)
		Candidate.NarrativeProgress.MainStory.bShowJourneyTree=true;
	LastFeedback=FText::GetEmpty(); SelectedChapterId=Node->ChapterId;
	return Commit(MoveTemp(Candidate),Node->IsJourney());
}
bool UGameXXKMainStorySubsystem::BeginTaskJourney()
{
	const auto* Current=State();const auto* Node=ActiveNode();
	if(!Current||!Node||!Node->IsJourney()||Current->Training.bChallengeActive
		||Current->NarrativeProgress.MainStory.Phase!=EGameXXKMainStoryActivityPhase::ReadyToTravel)
		return false;
	if(FGameXXKMainStoryRules::HasBattleVictory(*Current,Node->Id))return false;
	if(Node->Kind==EGameXXKMainStoryNodeKind::JourneyBattle
		&&Current->NarrativeProgress.MainStory.LineIndex<Node->Lines.Num())return false;
	FGameXXKRuntimeState Candidate=*Current;FString Error;
	if(!BeginJourney(Candidate,*Node,Error)){SetError(Error);return false;}
	LastFeedback=FText::GetEmpty();return Commit(MoveTemp(Candidate),true);
}
bool UGameXXKMainStorySubsystem::CompleteNonBattleGate(FGameXXKRuntimeState& Candidate,FString& Error)
{
	const auto& S=Candidate.NarrativeProgress.MainStory;
	if (S.ActiveNodeId!=S.JourneyNodeId || !Candidate.Training.bChallengeActive
		|| Candidate.Training.ActiveChallengeStageId!=S.JourneyStageId || Candidate.RouteSeed!=S.JourneySeed) return true;
	const auto* Node=FGameXXKMainStoryCatalog::FindNode(S.JourneyNodeId);
	if (!Node || Node->Kind!=EGameXXKMainStoryNodeKind::JourneyInvestigation || !S.bGateEntered
		|| !FGameXXKMainStoryRules::IsNodeCompleted(Candidate,Node->Id) || Candidate.VisitedRouteNodeIds.Contains(S.GateNodeId)) return true;
	if(FGameXXKMainStoryRules::IsDedicatedJourney(Candidate))return FGameXXKMainStoryRules::FinishDedicatedJourneyNode(Candidate,&Error);
	if (!UGameXXKMVPRules::ResolveStoryRouteNode(Candidate,S.GateNodeId)) { Error=TEXT("剧情已得到答案，但路线尚未接续，请重试。"); return false; }
	return true;
}
bool UGameXXKMainStorySubsystem::AdvanceDialogue()
{
	const auto* S=State(); if (!S) return false;
	FGameXXKRuntimeState Candidate=*S; FString Error;
	if (!FGameXXKMainStoryRules::AdvanceDialogue(Candidate,&Error) || !CompleteNonBattleGate(Candidate,Error)) { SetError(Error); return false; }
	// Old saves may still be partway through the former at-gate dialogue.
	// Finishing it enters the encounter through the same atomic boundary.
	const bool bEnterBattle=Candidate.NarrativeProgress.MainStory.Phase==EGameXXKMainStoryActivityPhase::ReadyToBattle;
	if(bEnterBattle&&!PrepareTaskBattle(Candidate,Error)){SetError(Error);return false;}
	LastFeedback=FText::GetEmpty(); return Commit(MoveTemp(Candidate),bEnterBattle);
}
bool UGameXXKMainStorySubsystem::ChooseAnswer(const int32 Index)
{
	const auto* S=State(); if (!S) return false;
	FGameXXKRuntimeState Candidate=*S; FString Error; FText FeedbackText;
	if (!FGameXXKMainStoryRules::ChooseAnswer(Candidate,Index,FeedbackText,&Error))
	{
		LastFeedback=Error.IsEmpty()?FeedbackText:FText::FromString(Error); OnChanged.Broadcast(); return false;
	}
	if (!CompleteNonBattleGate(Candidate,Error)) { SetError(Error); return false; }
	LastFeedback=FeedbackText; return Commit(MoveTemp(Candidate));
}
bool UGameXXKMainStorySubsystem::RevealHint()
{
	const auto* S=State(); if (!S) return false; FGameXXKRuntimeState Candidate=*S; FText Hint;
	if (!FGameXXKMainStoryRules::RevealHint(Candidate,Hint)) return false;
	LastFeedback=Hint; return Commit(MoveTemp(Candidate));
}
bool UGameXXKMainStorySubsystem::ClaimReward(const FName Id)
{
	const auto* S=State(); if (!S) return false; FGameXXKRuntimeState Candidate=*S; FString Error;
	if (!FGameXXKMainStoryRules::ClaimReward(Candidate,Id,&Error)) { SetError(Error); return false; }
	LastFeedback=FText::FromString(TEXT("金币已入袋。这回不是画饼。"));
	return Commit(MoveTemp(Candidate));
}
bool UGameXXKMainStorySubsystem::EnterJourneyGate(const int32 Id)
{
	const auto* S=State(); if (!S) return false; FGameXXKRuntimeState Candidate=*S; FString Error;
	if (!FGameXXKMainStoryRules::EnterJourneyGate(Candidate,Id,&Error)) { SetError(Error); return false; }
	if(Candidate.NarrativeProgress.MainStory.Phase==EGameXXKMainStoryActivityPhase::ReadyToBattle
		&&!PrepareTaskBattle(Candidate,Error)){SetError(Error);return false;}
	SelectedChapterId=Candidate.NarrativeProgress.MainStory.JourneyChapterId; LastFeedback=FText::GetEmpty();
	return Commit(MoveTemp(Candidate),true);
}
bool UGameXXKMainStorySubsystem::BeginTaskBattle()
{
	const auto* Current=State();if(!Current)return false;
	FGameXXKRuntimeState Candidate=*Current;FString Error;
	if(!PrepareTaskBattle(Candidate,Error)){if(!Error.IsEmpty())SetError(Error);return false;}
	LastFeedback=FText::GetEmpty();return Commit(MoveTemp(Candidate),true);
}
bool UGameXXKMainStorySubsystem::PrepareTaskBattle(FGameXXKRuntimeState& Candidate,FString& Error)
{
	auto* M=MVP();const auto* N=FGameXXKMainStoryCatalog::FindNode(Candidate.NarrativeProgress.MainStory.ActiveNodeId);
	if(!M||!N||N->Kind!=EGameXXKMainStoryNodeKind::JourneyBattle||M->IsAcademySessionActive()
		||Candidate.CardRun.bHasActiveCardBattle
		||FGameXXKMainStoryRules::HasBattleVictory(Candidate,N->Id)
		||Candidate.NarrativeProgress.MainStory.Phase!=EGameXXKMainStoryActivityPhase::ReadyToBattle)return false;
	auto& Session=Candidate.NarrativeProgress.MainStory;
	const int32 Gate=Session.GateNodeId;
	auto* RouteNode=Candidate.RouteMapNodes.FindByPredicate([Gate](const auto& R){return R.NodeId==Gate;});
	if (!RouteNode || !Session.bGateEntered || !FGameXXKMainStoryRules::IsJourneyGate(Candidate,Gate)
		|| !Candidate.ReachableRouteNodeIds.Contains(Gate)) return false;
	RouteNode->NodeKind=EGameXXKNodeKind::Battle;
	Candidate.Training.ChallengeRouteNodeEncounterIndices.Add(Gate,0);
	Session.bBattleStarted=true; Session.bShowJourneyTree=false; Session.Phase=EGameXXKMainStoryActivityPhase::AwaitingBattle; ++Session.Revision;
	const FGameXXKRuntimeState Before=M->GetRuntimeState();
	const bool bDedicatedJourney=FGameXXKMainStoryRules::IsDedicatedJourney(Candidate);
	const FName JourneyMapId=Candidate.CurrentMapId;
	M->GetMutableRuntimeState()=Candidate;
	const bool bPrepared=M->SelectTrainingChallengeRouteNode(Gate);
	if(bPrepared)Candidate=M->GetRuntimeState();
	if(bPrepared&&bDedicatedJourney)Candidate.CurrentMapId=JourneyMapId;
	// The ordinary battle bridge prepares the candidate; publish it only after the same save boundary as dialogue and rewards.
	M->GetMutableRuntimeState()=Before;
	if(!bPrepared)Error=TEXT("遭遇尚未准备好，请重试。");
	return bPrepared;
}
void UGameXXKMainStorySubsystem::PauseActivity()
{
	const auto* S=State(); if (!S) return; FGameXXKRuntimeState Candidate=*S;
	FGameXXKMainStoryRules::PauseActivity(Candidate); LastFeedback=FText::GetEmpty(); Commit(MoveTemp(Candidate));
}
void UGameXXKMainStorySubsystem::OpenJourneyTree()
{
	const auto* S=State(); if (!S || !S->Training.bChallengeActive || S->NarrativeProgress.MainStory.JourneyChapterId.IsNone()
		|| S->NarrativeProgress.MainStory.GateNodeIds.IsEmpty() || S->NarrativeProgress.MainStory.JourneySeed!=S->RouteSeed
		|| S->NarrativeProgress.MainStory.JourneyStageId!=S->Training.ActiveChallengeStageId) return;
	FGameXXKRuntimeState Candidate=*S; Candidate.NarrativeProgress.MainStory.bShowJourneyTree=true;
	SelectedChapterId=Candidate.NarrativeProgress.MainStory.JourneyChapterId; Commit(MoveTemp(Candidate));
}
void UGameXXKMainStorySubsystem::CloseJourneyTree()
{
	const auto* S=State(); if (!S) return; FGameXXKRuntimeState Candidate=*S;
	Candidate.NarrativeProgress.MainStory.bShowJourneyTree=false; Commit(MoveTemp(Candidate));
}
bool UGameXXKMainStorySubsystem::WantsRouteOverlay() const
{
	const auto* S=State(); return S && S->Training.bChallengeActive && S->Screen==EGameXXKScreen::DungeonMap
		&& !S->CardRun.bHasActiveCardBattle && S->NarrativeProgress.MainStory.bShowJourneyTree
		&& !GameXXKMainStoryDialoguePresentation::IsActive(*S)
		&& !(RoutePanel && RoutePanel->IsDialogueReplayActive());
}
bool UGameXXKMainStorySubsystem::WantsRouteDialogue() const
{
	const auto* S=State();return S && S->Training.bChallengeActive && S->Screen==EGameXXKScreen::DungeonMap
		&& !S->CardRun.bHasActiveCardBattle && S->NarrativeProgress.MainStory.bShowJourneyTree
		&& (GameXXKMainStoryDialoguePresentation::IsActive(*S) || (RoutePanel && RoutePanel->IsDialogueReplayActive()));
}
void UGameXXKMainStorySubsystem::RefreshViews(const bool bRefreshFlow)
{
	OnChanged.Broadcast();
	bFlowRefreshPending|=bRefreshFlow;
	if(bPresentationRefreshQueued)return;
	if(UWorld* World=GetWorld())
	{
		bPresentationRefreshQueued=true;
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,[this](){FlushPresentationViews();}));
	}
}
void UGameXXKMainStorySubsystem::FlushPresentationViews()
{
	bPresentationRefreshQueued=false;
	if(bRefreshingViews)return;TGuardValue<bool> Guard(bRefreshingViews,true);
	const bool bRefreshFlow=bFlowRefreshPending;bFlowRefreshPending=false;
	if (auto* World=GetWorld()) if (auto* PC=Cast<AGameXXKMVPPlayerController>(World->GetFirstPlayerController()))
	{
		if (bRefreshFlow) PC->RefreshPlayerFlowWidgetsFromState();
		if (WantsRouteOverlay())
		{
			if (!RoutePanel)
			{
				RoutePanel=CreateWidget<UGameXXKMainStoryPanelWidget>(PC);
			}
			RoutePanel->SetContext(this,PreferredChapter(),true);
			if(!RoutePanel->IsInViewport()){RoutePanel->AddToViewport(280);RoutePanel->SetKeyboardFocus();}
		}
		else if (RoutePanel && RoutePanel->IsInViewport()) RoutePanel->RemoveFromParent();
		if(WantsRouteDialogue())
		{
			if(!RouteDialoguePanel)
			{
				RouteDialoguePanel=CreateWidget<UGameXXKDialoguePanelWidget>(PC);
				RouteDialoguePanel->SetCompactLayout(true);
				RouteDialoguePanel->SetAdvanceRequested(FGameXXKDialogueAdvanceRequested::CreateWeakLambda(this,[this]()
				{
					if(RoutePanel && RoutePanel->IsDialogueReplayActive()){RoutePanel->HandleAction(9);RefreshViews(false);}
					else AdvanceDialogue();
				}));
				RouteDialoguePanel->SetOptionRequested(FGameXXKDialogueOptionRequested::CreateWeakLambda(this,[this](FName Id)
				{
					if(Id==TEXT("MainStory.Travel"))BeginTaskJourney();
					else if(Id==TEXT("MainStory.Battle"))BeginTaskBattle();
					else {int32 I=GameXXKMainStoryDialoguePresentation::ChoiceIndex(Id);if(I!=INDEX_NONE)ChooseAnswer(I);}
				}));
				RouteDialoguePanel->SetHintRequested(FGameXXKDialogueAdvanceRequested::CreateWeakLambda(this,[this](){RevealHint();}));
				RouteDialoguePanel->SetPauseRequested(FGameXXKDialogueAdvanceRequested::CreateWeakLambda(this,[this]()
				{
					if(RoutePanel && RoutePanel->IsDialogueReplayActive())RoutePanel->HandleAction(2);
					else PauseActivity();
					CloseJourneyTree();
				}));
				RouteDialoguePanel->AddToViewport(280);
				RouteDialoguePanel->SetAnchorsInViewport(FAnchors(.12f,.53f,.88f,.96f));
				RouteDialoguePanel->SetKeyboardFocus();
			}
			FGameXXKDialoguePresentationView View;
			const bool bReplay=RoutePanel && RoutePanel->IsDialogueReplayActive();
			if(bReplay)
			{
				if(const auto* N=FGameXXKMainStoryCatalog::FindNode(RoutePanel->GetSelectedNode()))
					View=GameXXKMainStoryDialoguePresentation::LineView(*N,RoutePanel->GetDialogueReplayIndex());
			}
			else View=GameXXKMainStoryDialoguePresentation::Build(*State(),Feedback());
			RouteDialoguePanel->SetHintRequested(bReplay?FGameXXKDialogueAdvanceRequested():FGameXXKDialogueAdvanceRequested::CreateWeakLambda(this,[this](){RevealHint();}));
			RouteDialoguePanel->Present(View);
		}
		else if(RouteDialoguePanel){RouteDialoguePanel->ClearPresentation();RouteDialoguePanel->RemoveFromParent();RouteDialoguePanel=nullptr;}
		if(State() && !State()->Training.bChallengeActive)RoutePanel=nullptr;
	}
}
void UGameXXKMainStorySubsystem::Tick(float DeltaTime)
{
	const auto* S=State(); if (!S || !MVP() || MVP()->IsAcademySessionActive()) return;
	const auto& Journey=S->NarrativeProgress.MainStory;
	if(S->Training.bChallengeActive&&!S->CardRun.bHasActiveCardBattle&&!Journey.GateNodeIds.IsEmpty()
		&&!FGameXXKMainStoryRules::IsDedicatedJourney(*S)&&!FGameXXKMainStoryRules::IsNodeCompleted(*S,Journey.JourneyNodeId)
		&&!FGameXXKMainStoryRules::HasBattleVictory(*S,Journey.JourneyNodeId)
		&&LastLegacyMapAttemptRevision!=Journey.Revision)
	{
		LastLegacyMapAttemptRevision=Journey.Revision;
		FGameXXKRuntimeState Candidate=*S;FString Error;
		if(FGameXXKMainStoryRules::GenerateDedicatedJourneyMap(Candidate,Journey.JourneyNodeId,Journey.JourneySeed,&Error))
			Commit(MoveTemp(Candidate),true);
		else SetError(Error);
		return;
	}
	if (S->CardRun.bHasActiveCardBattle && S->CardRun.ActiveBattle.Phase==EGameXXKCardBattlePhase::Victory)
	{
		FGameXXKRuntimeState Candidate=*S;
		if (FGameXXKMainStoryRules::ObserveBattleVictory(Candidate)) { Commit(MoveTemp(Candidate)); return; }
	}
	if (WantsRouteOverlay()!=(RoutePanel && RoutePanel->IsInViewport()) || WantsRouteDialogue()!=IsValid(RouteDialoguePanel))RefreshViews(false);
}
bool UGameXXKMainStorySubsystem::IsTickable() const { return !IsTemplate() && GetWorld() && GetWorld()->IsGameWorld(); }
TStatId UGameXXKMainStorySubsystem::GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(UGameXXKMainStorySubsystem,STATGROUP_Tickables); }
UWorld* UGameXXKMainStorySubsystem::GetTickableGameObjectWorld() const { return GetWorld(); }
void UGameXXKMainStorySubsystem::Deinitialize()
{
	if (RoutePanel) { RoutePanel->RemoveFromParent(); RoutePanel=nullptr; }
	if(RouteDialoguePanel){RouteDialoguePanel->ClearPresentation();RouteDialoguePanel->RemoveFromParent();RouteDialoguePanel=nullptr;}
	OnChanged.Clear(); Super::Deinitialize();
}
FString UGameXXKMainStorySubsystem::GetProgressJson() const
{
	auto Root=MakeShared<FJsonObject>(); const auto* S=State(); if (!S) return TEXT("{}");
	Root->SetStringField(TEXT("chapter"),PreferredChapter().ToString());
	Root->SetStringField(TEXT("active_node"),S->NarrativeProgress.MainStory.ActiveNodeId.ToString());
	Root->SetNumberField(TEXT("phase"),static_cast<int32>(S->NarrativeProgress.MainStory.Phase));
	Root->SetStringField(TEXT("journey_map"),S->CurrentMapId.ToString());
	Root->SetStringField(TEXT("journey_node"),S->NarrativeProgress.MainStory.JourneyNodeId.ToString());
	Root->SetNumberField(TEXT("gate"),S->NarrativeProgress.MainStory.GateNodeId);
	Root->SetBoolField(TEXT("gate_entered"),S->NarrativeProgress.MainStory.bGateEntered);
	Root->SetBoolField(TEXT("battle_won"),S->NarrativeProgress.MainStory.bBattleWon);
	Root->SetStringField(TEXT("feedback"),LastFeedback.ToString());
	TArray<TSharedPtr<FJsonValue>> Nodes;
	for (const auto& N:FGameXXKMainStoryCatalog::Nodes())
	{
		auto Item=MakeShared<FJsonObject>(); Item->SetStringField(TEXT("id"),N.Id.ToString());
		Item->SetNumberField(TEXT("state"),static_cast<int32>(FGameXXKMainStoryRules::NodeState(*S,N.Id)));
		Nodes.Add(MakeShared<FJsonValueObject>(Item));
	}
	Root->SetArrayField(TEXT("nodes"),Nodes); FString Json; FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Json)); return Json;
}
