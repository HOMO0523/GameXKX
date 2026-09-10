#include "Narrative/GameXXKMainStoryRules.h"
#include "Narrative/GameXXKStoryCatalog.h"
#include "Narrative/GameXXKStoryRules.h"
#include "GameXXKCardBattleAdapter.h"
#include "GameXXKRouteEconomyRules.h"

namespace
{
const FName LineKey(TEXT("MainStory.DialogueLine"));
const FName HintKey(TEXT("MainStory.HintLevel"));
const FName BattleWonKey(TEXT("MainStory.BattleWon"));
const FName PostLineKey(TEXT("MainStory.PostBattleLine"));
bool Fail(FString* Error,const FString& Text){if(Error)*Error=Text;return false;}
void Clear(FString* Error){if(Error)Error->Reset();}
bool Done(const FGameXXKTaskProgress* P){return P&&(P->State==EGameXXKTaskState::Completed||P->State==EGameXXKTaskState::Rewarded);}
bool Requirements(const FGameXXKRuntimeState& State,const FGameXXKMainStoryNode& Node)
{
 if(State.NarrativeProgress.MainStory.bDevelopmentUnlockAllTasks)return true;
 for(FName Id:Node.RequiresAll)if(!FGameXXKMainStoryRules::IsNodeCompleted(State,Id))return false;
 if(!Node.RequiresAny.IsEmpty()){bool Any=false;for(FName Id:Node.RequiresAny)Any|=FGameXXKMainStoryRules::IsNodeCompleted(State,Id);if(!Any)return false;}
 return true;
}
bool CompleteNode(FGameXXKRuntimeState& State,const FGameXXKMainStoryNode& Node,FString* Error)
{
 const auto* Task=FGameXXKStoryCatalog::FindTask(Node.Id);
 if(!Task||!Requirements(State,Node)||!FGameXXKStoryRules::CompleteTask(*Task,State.NarrativeProgress,Error))
  return Fail(Error,TEXT("这段任务的目标还没有完成。"));
 auto& S=State.NarrativeProgress.MainStory;S.ActiveNodeId=Node.Id;S.Phase=EGameXXKMainStoryActivityPhase::Result;
 if(const auto* C=FGameXXKMainStoryCatalog::FindChapter(Node.ChapterId))
 {
  if(Node.Id==C->MainlineEnd)S.MainlineCompletedChapters.Add(C->Id);
  bool AllDone=true;for(FName Id:C->Nodes)AllDone&=FGameXXKMainStoryRules::IsNodeCompleted(State,Id);
  if(AllDone)if(auto* Story=State.NarrativeProgress.StoryProgressById.Find(Node.StoryId))Story->State=EGameXXKStoryState::Completed;
 }
 if(State.Training.bChallengeActive&&S.JourneyChapterId==Node.ChapterId)S.bShowJourneyTree=true;
 ++S.Revision;Clear(Error);return true;
}
void AfterDialogue(FGameXXKRuntimeState& State,const FGameXXKMainStoryNode& Node)
{
 auto& S=State.NarrativeProgress.MainStory;
 if(Node.Kind==EGameXXKMainStoryNodeKind::JourneyBattle)
  S.Phase=State.Training.bChallengeActive&&S.JourneyNodeId==Node.Id&&S.bGateEntered
   ? EGameXXKMainStoryActivityPhase::ReadyToBattle : EGameXXKMainStoryActivityPhase::ReadyToTravel;
 else if(Node.IsInvestigation())S.Phase=EGameXXKMainStoryActivityPhase::Choice;
}
}
bool FGameXXKMainStoryRules::IsChapterUnlocked(const FGameXXKRuntimeState& State,FName Id)
{
 const auto* C=FGameXXKMainStoryCatalog::FindChapter(Id);if(!C)return false;
 if(State.NarrativeProgress.MainStory.bDevelopmentUnlockAllTasks)return true;
 if(!FGameXXKTrainingRules::CanChallenge(State.Training,FGameXXKMainStoryCatalog::StageId(*C)))return false;
 if(C->StageNumber==1)return true;
 const int32 Previous=C->StageNumber-2;
 return FGameXXKMainStoryCatalog::Chapters().IsValidIndex(Previous)
  &&State.NarrativeProgress.MainStory.MainlineCompletedChapters.Contains(FGameXXKMainStoryCatalog::Chapters()[Previous].Id);
}
bool FGameXXKMainStoryRules::IsNodeCompleted(const FGameXXKRuntimeState& State,FName Id){return Done(State.NarrativeProgress.TaskProgressById.Find(Id));}
bool FGameXXKMainStoryRules::HasBattleVictory(const FGameXXKRuntimeState& State,FName Id)
{
 const auto* P=State.NarrativeProgress.TaskProgressById.Find(Id);
 const auto& S=State.NarrativeProgress.MainStory;
 return (P&&P->ObjectiveCounts.FindRef(BattleWonKey)==1)||(S.JourneyNodeId==Id&&S.bBattleWon);
}
bool FGameXXKMainStoryRules::HasPendingAfterBattleDialogue(const FGameXXKRuntimeState& State,FName Id)
{
 const auto* N=FGameXXKMainStoryCatalog::FindNode(Id);const auto* P=State.NarrativeProgress.TaskProgressById.Find(Id);
 return N&&N->Kind==EGameXXKMainStoryNodeKind::JourneyBattle&&P&&P->State==EGameXXKTaskState::Active
  &&HasBattleVictory(State,Id)&&P->ObjectiveCounts.FindRef(PostLineKey)<N->AfterBattleLines.Num();
}
EGameXXKTaskState FGameXXKMainStoryRules::NodeState(const FGameXXKRuntimeState& State,FName Id)
{
 const auto* N=FGameXXKMainStoryCatalog::FindNode(Id);if(!N)return EGameXXKTaskState::Locked;
 if(const auto* P=State.NarrativeProgress.TaskProgressById.Find(Id))if(P->State==EGameXXKTaskState::Active||Done(P))return P->State;
 return IsChapterUnlocked(State,N->ChapterId)&&Requirements(State,*N)?EGameXXKTaskState::Available:EGameXXKTaskState::Locked;
}
bool FGameXXKMainStoryRules::HasUnseenChapter(const FGameXXKRuntimeState& State)
{
 for(const auto& C:FGameXXKMainStoryCatalog::Chapters())if(IsChapterUnlocked(State,C.Id)&&!State.NarrativeProgress.MainStory.SeenChapters.Contains(C.Id))return true;
 return false;
}
bool FGameXXKMainStoryRules::HasUnclaimedReward(const FGameXXKRuntimeState& State)
{
 for(const auto& N:FGameXXKMainStoryCatalog::Nodes())if(NodeState(State,N.Id)==EGameXXKTaskState::Completed)return true;
 return false;
}
bool FGameXXKMainStoryRules::MarkChapterSeen(FGameXXKRuntimeState& State,FName Id)
{
 if(!IsChapterUnlocked(State,Id))return false;
 auto& S=State.NarrativeProgress.MainStory;
 if(!S.SeenChapters.Contains(Id)){S.SeenChapters.Add(Id);++S.Revision;}return true;
}
bool FGameXXKMainStoryRules::StartNode(FGameXXKRuntimeState& State,FName Id,FString* Error)
{
 const auto* N=FGameXXKMainStoryCatalog::FindNode(Id);
 if(!N||NodeState(State,Id)==EGameXXKTaskState::Locked)return Fail(Error,TEXT("这段任务尚未开放，请先完成前面的线索。"));
 if(State.CardRun.bHasActiveCardBattle)return Fail(Error,TEXT("先处理眼前的战斗，再继续这段任务。"));
 auto& P=State.NarrativeProgress;
 if(!P.StoryProgressById.Contains(N->StoryId))
 {
  const auto* Story=FGameXXKStoryCatalog::FindStory(N->StoryId);
  if(!Story||!FGameXXKStoryRules::StartStory(*Story,P,Error))return false;
 }
 if(!P.TaskProgressById.Contains(Id)||NodeState(State,Id)==EGameXXKTaskState::Available)
 {
  const auto* Task=FGameXXKStoryCatalog::FindTask(Id);
  if(!Task||!FGameXXKStoryRules::StartTask(*Task,P,Error))return false;
 }
 auto& S=P.MainStory;S.ActiveNodeId=Id;
 if(HasPendingAfterBattleDialogue(State,Id))
 {
  S.LineIndex=FMath::Clamp(P.TaskProgressById.FindChecked(Id).ObjectiveCounts.FindRef(PostLineKey),0,N->AfterBattleLines.Num());
  S.Phase=EGameXXKMainStoryActivityPhase::Dialogue;MarkChapterSeen(State,N->ChapterId);++S.Revision;Clear(Error);return true;
 }
 S.LineIndex=FMath::Clamp(P.TaskProgressById.FindChecked(Id).ObjectiveCounts.FindRef(LineKey),0,N->Lines.Num());
 if(IsNodeCompleted(State,Id))S.Phase=EGameXXKMainStoryActivityPhase::Result;
 else if(N->IsJourney()&&State.Training.bChallengeActive&&S.JourneyNodeId==Id&&!S.bGateEntered)
  S.Phase=EGameXXKMainStoryActivityPhase::AwaitingGate;
 else if(N->IsJourney()&&!State.Training.bChallengeActive)
  S.Phase=N->Kind==EGameXXKMainStoryNodeKind::JourneyBattle&&S.LineIndex<N->Lines.Num()
   ? EGameXXKMainStoryActivityPhase::Dialogue : EGameXXKMainStoryActivityPhase::ReadyToTravel;
 else if(S.LineIndex>=N->Lines.Num())AfterDialogue(State,*N);
 else S.Phase=EGameXXKMainStoryActivityPhase::Dialogue;
 MarkChapterSeen(State,N->ChapterId);++S.Revision;Clear(Error);return true;
}
bool FGameXXKMainStoryRules::AdvanceDialogue(FGameXXKRuntimeState& State,FString* Error)
{
 auto& S=State.NarrativeProgress.MainStory;const auto* N=FGameXXKMainStoryCatalog::FindNode(S.ActiveNodeId);
 if(!N||S.Phase!=EGameXXKMainStoryActivityPhase::Dialogue)return Fail(Error,TEXT("当前没有可以推进的对白。"));
 if(State.CardRun.bHasActiveCardBattle)return Fail(Error,TEXT("先结束眼前的战斗，再继续交流。"));
 auto* P=State.NarrativeProgress.TaskProgressById.Find(N->Id);
 if(!P||P->State!=EGameXXKTaskState::Active)return Fail(Error,TEXT("这段任务没有处于进行状态。"));
 if(HasPendingAfterBattleDialogue(State,N->Id))
 {
  S.LineIndex=FMath::Min(S.LineIndex+1,N->AfterBattleLines.Num());P->ObjectiveCounts.Add(PostLineKey,S.LineIndex);++S.Revision;
  if(S.LineIndex>=N->AfterBattleLines.Num())return CompleteNode(State,*N,Error);
  Clear(Error);return true;
 }
 S.LineIndex=FMath::Min(S.LineIndex+1,N->Lines.Num());P->ObjectiveCounts.Add(LineKey,S.LineIndex);++S.Revision;
 if(S.LineIndex>=N->Lines.Num())
 {
  if(N->IsInvestigation()||N->Kind==EGameXXKMainStoryNodeKind::JourneyBattle)AfterDialogue(State,*N);
  else return CompleteNode(State,*N,Error);
 }
 Clear(Error);return true;
}
bool FGameXXKMainStoryRules::ChooseAnswer(FGameXXKRuntimeState& State,int32 Index,FText& Feedback,FString* Error)
{
 const auto& S=State.NarrativeProgress.MainStory;const auto* N=FGameXXKMainStoryCatalog::FindNode(S.ActiveNodeId);
 if(!N||S.Phase!=EGameXXKMainStoryActivityPhase::Choice||!N->Options.IsValidIndex(Index))return Fail(Error,TEXT("请从当前列出的线索判断中选择。"));
 Feedback=N->Options[Index].Feedback;
 if(!N->Options[Index].bCorrect){Clear(Error);return false;}
 return CompleteNode(State,*N,Error);
}
bool FGameXXKMainStoryRules::RevealHint(FGameXXKRuntimeState& State,FText& Hint)
{
 auto& S=State.NarrativeProgress.MainStory;const auto* N=FGameXXKMainStoryCatalog::FindNode(S.ActiveNodeId);
 if(!N||N->Hints.IsEmpty())return false;
 auto* P=State.NarrativeProgress.TaskProgressById.Find(N->Id);if(!P)return false;
 const int32 Level=FMath::Clamp(P->ObjectiveCounts.FindRef(HintKey),0,N->Hints.Num()-1);
 Hint=N->Hints[Level];P->ObjectiveCounts.Add(HintKey,FMath::Min(Level+1,N->Hints.Num()-1));++S.Revision;return true;
}
bool FGameXXKMainStoryRules::ClaimReward(FGameXXKRuntimeState& State,FName Id,FString* Error)
{
 const auto* N=FGameXXKMainStoryCatalog::FindNode(Id);const auto* Task=FGameXXKStoryCatalog::FindTask(Id);
 if(!N||!Task||NodeState(State,Id)!=EGameXXKTaskState::Completed)return Fail(Error,TEXT("这份报酬尚未可领，或已经领过。"));
 FGameXXKRuntimeState Candidate=State;const int64 Gold=static_cast<int64>(Candidate.PlayerGold)+N->Gold;
 if(Gold>MAX_int32||Gold<0)return Fail(Error,TEXT("钱袋暂时装不下这份报酬，请稍后再领。"));
 const FName Stage=FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal,N->StageNumber);
 for(int32 I=0;I<N->NormalBoxes;++I)if(!FGameXXKTrainingRules::AppendChestToken(Candidate.Training,EGameXXKTrainingRewardTier::NormalChest,Stage,N->BoxLevel,Error))return false;
 for(int32 I=0;I<N->AdvancedBoxes;++I)if(!FGameXXKTrainingRules::AppendChestToken(Candidate.Training,EGameXXKTrainingRewardTier::AdvancedChest,Stage,N->BoxLevel,Error))return false;
 if(!FGameXXKStoryRules::CommitTaskReward(*Task,Candidate.NarrativeProgress,Error))return false;
 Candidate.PlayerGold=static_cast<int32>(Gold);
 if(Candidate.NarrativeProgress.TrackedTaskId==Id)Candidate.NarrativeProgress.TrackedTaskId=NAME_None;
 ++Candidate.NarrativeProgress.MainStory.Revision;State=MoveTemp(Candidate);Clear(Error);return true;
}
bool FGameXXKMainStoryRules::IsDedicatedJourney(const FGameXXKRuntimeState& State)
{
 const auto& S=State.NarrativeProgress.MainStory;
 return State.Training.bChallengeActive&&State.bHasGeneratedRouteMap&&!S.JourneyNodeId.IsNone()
  &&S.JourneyStageId==State.Training.ActiveChallengeStageId&&S.JourneySeed==State.RouteSeed
  &&State.CurrentMapId==FName(*(TEXT("StoryJourney.")+S.JourneyNodeId.ToString()));
}
bool FGameXXKMainStoryRules::BuildJourneyBattleEncounter(const FGameXXKRuntimeState& State,FGameXXKTrainingEncounterDefinition& Out)
{
 const auto& S=State.NarrativeProgress.MainStory;const auto* N=FGameXXKMainStoryCatalog::FindNode(S.JourneyNodeId);
 return IsDedicatedJourney(State)&&N&&N->Kind==EGameXXKMainStoryNodeKind::JourneyBattle&&S.bBattleStarted
  &&FGameXXKTrainingRules::BuildFormationEncounter(S.JourneyStageId,N->EnemyDefinitionIds,Out);
}
bool FGameXXKMainStoryRules::GenerateDedicatedJourneyMap(FGameXXKRuntimeState& State,FName Id,int32 Seed,FString* Error)
{
 const auto* N=FGameXXKMainStoryCatalog::FindNode(Id);
 if(!N||!N->IsJourney()||!State.Training.bChallengeActive||State.CardRun.bHasActiveCardBattle)
  return Fail(Error,TEXT("当前无法建立任务行程。"));
 if(HasBattleVictory(State,Id))return Fail(Error,TEXT("这一战已经得胜，先把余下的话说完。"));
 const FName Stage=FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal,N->StageNumber);
 if(State.Training.ActiveChallengeStageId!=Stage)return Fail(Error,TEXT("任务行程的战斗配置不匹配。"));
 if(!FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State,Error))return false;
 FGameXXKCardBattleAdapter::ClearRouteLocalCardState(State);
 FGameXXKRouteEconomyRules::ClearRouteEconomy(State.CardRun);
 State.CardRun.RouteProgress=FGameXXKRouteProgress();State.CardRun.RouteProgress.CurrentChapter=1;
 if(!FGameXXKRouteEconomyRules::InitializeRoute(State.CardRun,0,Error))return false;
 State.CardRun.bLoadoutLockedForRoute=true;
 State.bHasActiveBattle=false;State.ActiveBattleNodeId=INDEX_NONE;State.ActiveBattleEnemies.Reset();State.ActiveBattleParty.Reset();
 State.BattleEntryCheckpoint=FGameXXKBattleEntryCheckpoint{};
 State.Training.ChallengeRouteNodeEncounterIndices.Reset();State.Training.ActiveChallengeRouteNodeId=INDEX_NONE;
 State.Training.ActiveChallengeEncounterIndex=0;
 State.RouteSeed=FMath::Max(1,Seed);State.CardRun.RouteRandomSeed=State.RouteSeed;
 State.RouteMapNodes={
  FGameXXKRouteMapNode(0,0,0,EGameXXKNodeKind::Start,FVector2D(.35,.25),{1}),
  FGameXXKRouteMapNode(1,1,0,EGameXXKNodeKind::Event,FVector2D(.65,.75),{})};
 State.RouteMapEdges={FGameXXKRouteMapEdge(0,1)};
 State.VisitedRouteNodeIds={0};State.ReachableRouteNodeIds={1};State.CurrentRouteNodeId=1;
 State.PendingRouteNodeId=INDEX_NONE;State.DungeonNodeIndex=1;
 State.bHasGeneratedRouteMap=true;State.bDungeonActive=true;State.Screen=EGameXXKScreen::DungeonMap;
 State.CurrentMapId=FName(*(TEXT("StoryJourney.")+Id.ToString()));State.TownPanelMode=EGameXXKTownPanelMode::None;
 auto& S=State.NarrativeProgress.MainStory;
 if(S.JourneyNodeId!=Id||!S.JourneyId.IsValid())S.JourneyId=FGuid::NewGuid();
 S.JourneyNodeId=Id;S.JourneyChapterId=N->ChapterId;S.JourneyStageId=Stage;S.JourneySeed=State.RouteSeed;
 S.GateNodeIds={1};S.GateNodeId=INDEX_NONE;S.bGateEntered=false;S.bBattleStarted=false;S.bBattleWon=false;
 S.ActiveNodeId=Id;S.Phase=EGameXXKMainStoryActivityPhase::AwaitingGate;S.bShowJourneyTree=false;
 S.JourneyStartedNodes.Add(Id);++S.Revision;Clear(Error);return true;
}
bool FGameXXKMainStoryRules::FinishDedicatedJourneyNode(FGameXXKRuntimeState& State,FString* Error)
{
 auto& S=State.NarrativeProgress.MainStory;
 if(!IsDedicatedJourney(State)||(!IsNodeCompleted(State,S.JourneyNodeId)&&!HasBattleVictory(State,S.JourneyNodeId))||!S.bGateEntered)
  return Fail(Error,TEXT("任务目标尚未完成。"));
 FGameXXKCardBattleAdapter::ClearActiveCardBattle(State);
 State.bHasActiveBattle=false;State.ActiveBattleNodeId=INDEX_NONE;State.ActiveBattleEnemies.Reset();State.ActiveBattleParty.Reset();
 State.BattleEntryCheckpoint=FGameXXKBattleEntryCheckpoint{};
 State.VisitedRouteNodeIds.AddUnique(S.GateNodeId);State.ReachableRouteNodeIds.Reset();
 State.CurrentRouteNodeId=INDEX_NONE;State.PendingRouteNodeId=INDEX_NONE;State.DungeonNodeIndex=State.VisitedRouteNodeIds.Num();
 State.Screen=EGameXXKScreen::DungeonMap;S.bShowJourneyTree=true;
 ++S.Revision;Clear(Error);return true;
}
bool FGameXXKMainStoryRules::InjectJourneyGate(FGameXXKRuntimeState& State,FName Id,FString* Error)
{
 const auto* N=FGameXXKMainStoryCatalog::FindNode(Id);
 if(!N||!N->IsJourney()||!State.Training.bChallengeActive||!State.bHasGeneratedRouteMap)return Fail(Error,TEXT("当前没有可以接续这段任务的历练路线。"));
 const FName Stage=FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal,N->StageNumber);
 if(Stage!=State.Training.ActiveChallengeStageId)return Fail(Error,TEXT("这段任务与当前关卡不对应。"));
 TSet<int32> Gates;for(const auto& R:State.RouteMapNodes)if(R.LayerIndex==3)Gates.Add(R.NodeId);
 if(Gates.IsEmpty())return Fail(Error,TEXT("当前路线没有可放置剧情的必经层。"));
 for(const auto& From:State.RouteMapNodes)for(int32 ToId:From.OutgoingNodeIds)
 {
  const auto* To=State.RouteMapNodes.FindByPredicate([ToId](const auto& Item){return Item.NodeId==ToId;});
  if(!To||To->LayerIndex<=From.LayerIndex||(From.LayerIndex<3&&To->LayerIndex>3))return Fail(Error,TEXT("这条路线存在绕过剧情层的连线，任务尚未开始。"));
 }
 for(auto& R:State.RouteMapNodes)if(Gates.Contains(R.NodeId)){R.NodeKind=EGameXXKNodeKind::Event;State.Training.ChallengeRouteNodeEncounterIndices.Remove(R.NodeId);}
 auto& S=State.NarrativeProgress.MainStory;
 if(S.JourneyNodeId!=Id||!S.JourneyId.IsValid())S.JourneyId=FGuid::NewGuid();
 S.JourneyNodeId=Id;S.JourneyChapterId=N->ChapterId;S.JourneyStageId=Stage;S.GateNodeIds=MoveTemp(Gates);S.GateNodeId=INDEX_NONE;
 S.bGateEntered=false;S.bBattleStarted=false;S.bBattleWon=false;S.bShowJourneyTree=false;S.JourneyStartedNodes.Add(Id);
 if(S.ActiveNodeId==Id)S.Phase=EGameXXKMainStoryActivityPhase::AwaitingGate;
 ++S.Revision;Clear(Error);return true;
}
bool FGameXXKMainStoryRules::IsJourneyGate(const FGameXXKRuntimeState& State,int32 Id)
{
 const auto& S=State.NarrativeProgress.MainStory;
 return State.Training.bChallengeActive&&S.JourneyStageId==State.Training.ActiveChallengeStageId
  &&S.JourneySeed==State.RouteSeed&&S.GateNodeIds.Contains(Id);
}
bool FGameXXKMainStoryRules::EnterJourneyGate(FGameXXKRuntimeState& State,int32 Id,FString* Error)
{
 auto& S=State.NarrativeProgress.MainStory;const auto* N=FGameXXKMainStoryCatalog::FindNode(S.JourneyNodeId);
 if(!N||!IsJourneyGate(State,Id)||State.CardRun.bHasActiveCardBattle||!State.ReachableRouteNodeIds.Contains(Id))return Fail(Error,TEXT("请沿当前可走的路线进入剧情问号。"));
 auto* P=State.NarrativeProgress.TaskProgressById.Find(N->Id);
 if(!P||P->State!=EGameXXKTaskState::Active)return Fail(Error,TEXT("这处剧情任务已经处理，或尚未接取。"));
 if(!S.bGateEntered)S.GateNodeId=Id;
 S.bGateEntered=true;S.ActiveNodeId=N->Id;S.bShowJourneyTree=true;
 S.LineIndex=FMath::Clamp(P->ObjectiveCounts.FindRef(LineKey),0,N->Lines.Num());S.Phase=EGameXXKMainStoryActivityPhase::Dialogue;
 if(S.LineIndex>=N->Lines.Num())AfterDialogue(State,*N);
 ++S.Revision;Clear(Error);return true;
}
bool FGameXXKMainStoryRules::ObserveBattleVictory(FGameXXKRuntimeState& State)
{
 auto& S=State.NarrativeProgress.MainStory;const auto* N=FGameXXKMainStoryCatalog::FindNode(S.JourneyNodeId);
 if(!N||N->Kind!=EGameXXKMainStoryNodeKind::JourneyBattle||!S.bGateEntered||!S.bBattleStarted||S.bBattleWon
  ||!State.CardRun.bHasActiveCardBattle||State.CardRun.ActiveBattle.Phase!=EGameXXKCardBattlePhase::Victory
  ||State.CardRun.ActiveBattleSourceNodeId!=S.GateNodeId||State.Training.ActiveChallengeStageId!=S.JourneyStageId)return false;
 auto* P=State.NarrativeProgress.TaskProgressById.Find(N->Id);if(!P||P->State!=EGameXXKTaskState::Active)return false;
 S.bBattleWon=true;P->ObjectiveCounts.Add(BattleWonKey,1);
 if(!N->AfterBattleLines.IsEmpty())
 {
  P->ObjectiveCounts.Add(PostLineKey,0);S.ActiveNodeId=N->Id;S.LineIndex=0;
  S.Phase=EGameXXKMainStoryActivityPhase::Dialogue;S.bShowJourneyTree=true;++S.Revision;return true;
 }
 FString Error;if(!CompleteNode(State,*N,&Error)){S.bBattleWon=false;P->ObjectiveCounts.Remove(BattleWonKey);return false;}return true;
}
void FGameXXKMainStoryRules::PauseActivity(FGameXXKRuntimeState& State)
{
 auto& S=State.NarrativeProgress.MainStory;S.ActiveNodeId=NAME_None;S.Phase=EGameXXKMainStoryActivityPhase::None;++S.Revision;
}
bool FGameXXKMainStoryRules::ValidateState(const FGameXXKRuntimeState& State,FString* Error)
{
 const auto& S=State.NarrativeProgress.MainStory;
 if(IsDedicatedJourney(State))
 {
  const auto* Start=State.RouteMapNodes.FindByPredicate([](const auto& N){return N.NodeId==0;});
  const auto* Goal=State.RouteMapNodes.FindByPredicate([](const auto& N){return N.NodeId==1;});
  if(State.RouteMapNodes.Num()!=2||!Start||!Goal||Start->NodeKind!=EGameXXKNodeKind::Start
   ||Start->OutgoingNodeIds!=TArray<int32>{1}||!Goal->OutgoingNodeIds.IsEmpty()
   ||(Goal->NodeKind!=EGameXXKNodeKind::Event&&Goal->NodeKind!=EGameXXKNodeKind::Battle)
   ||S.GateNodeIds.Num()!=1||!S.GateNodeIds.Contains(1)
   ||!State.CardRun.PendingReward.Options.IsEmpty()||!State.CardRun.PendingReward.CardIds.IsEmpty())
   return Fail(Error,TEXT("Dedicated story route contains ordinary-route content."));
 }
 if(S.Revision<0||S.LineIndex<0)return Fail(Error,TEXT("Main-story session counters are invalid."));
 for(const auto& Entry:State.NarrativeProgress.TaskProgressById)
 {
  const auto* N=FGameXXKMainStoryCatalog::FindNode(Entry.Key);if(!N)continue;
  const auto& P=Entry.Value;const int32 Won=P.ObjectiveCounts.FindRef(BattleWonKey);
  const int32 Post=P.ObjectiveCounts.FindRef(PostLineKey);
  if(Won<0||Won>1||Post<0||Post>N->AfterBattleLines.Num()
   ||((Won!=0||P.ObjectiveCounts.Contains(PostLineKey))&&N->Kind!=EGameXXKMainStoryNodeKind::JourneyBattle)
   ||(P.ObjectiveCounts.Contains(PostLineKey)&&!HasBattleVictory(State,N->Id))
   ||(Won==1&&P.State==EGameXXKTaskState::Active&&Post>=N->AfterBattleLines.Num()))
   return Fail(Error,TEXT("Story aftermath receipt or dialogue position is invalid."));
 }
 for(FName Id:S.SeenChapters)if(!FGameXXKMainStoryCatalog::FindChapter(Id))return Fail(Error,TEXT("Viewed story chapter does not exist."));
 for(FName Id:S.MainlineCompletedChapters){const auto* C=FGameXXKMainStoryCatalog::FindChapter(Id);if(!C||!IsNodeCompleted(State,C->MainlineEnd))return Fail(Error,TEXT("Mainline completion has no completed ending node."));}
 for(FName Id:S.JourneyStartedNodes){const auto* N=FGameXXKMainStoryCatalog::FindNode(Id);if(!N||!N->IsJourney())return Fail(Error,TEXT("Story journey identity is invalid."));}
 if(!S.ActiveNodeId.IsNone())
 {
  const auto* N=FGameXXKMainStoryCatalog::FindNode(S.ActiveNodeId);const auto* P=State.NarrativeProgress.TaskProgressById.Find(S.ActiveNodeId);
  const bool bAfter=N&&HasPendingAfterBattleDialogue(State,N->Id);
  if(!N||!P||S.LineIndex>(bAfter?N->AfterBattleLines.Num():FMath::Max(N->Lines.Num(),N->AfterBattleLines.Num())))return Fail(Error,TEXT("Active story node or dialogue position is invalid."));
  if(bAfter&&S.Phase==EGameXXKMainStoryActivityPhase::Dialogue&&S.LineIndex!=P->ObjectiveCounts.FindRef(PostLineKey))return Fail(Error,TEXT("Active aftermath position differs from its saved cursor."));
  if(S.Phase==EGameXXKMainStoryActivityPhase::Result&&!Done(P))return Fail(Error,TEXT("Story result requires completed node evidence."));
  if(S.Phase!=EGameXXKMainStoryActivityPhase::Result&&S.Phase!=EGameXXKMainStoryActivityPhase::None&&P->State!=EGameXXKTaskState::Active)return Fail(Error,TEXT("Active story activity is not attached to an active task."));
 }
 else if(S.Phase!=EGameXXKMainStoryActivityPhase::None)return Fail(Error,TEXT("Story activity phase has no node."));
 if(!S.JourneyNodeId.IsNone())
 {
  const auto* N=FGameXXKMainStoryCatalog::FindNode(S.JourneyNodeId);
  if(!N||!N->IsJourney()||S.JourneyChapterId!=N->ChapterId||S.JourneyStageId!=FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal,N->StageNumber))return Fail(Error,TEXT("Story journey context does not match its content."));
  if(S.bBattleWon&&!IsNodeCompleted(State,N->Id)&&!HasPendingAfterBattleDialogue(State,N->Id))return Fail(Error,TEXT("Story battle receipt is detached from node completion or aftermath."));
 }
 Clear(Error);return true;
}
FName FGameXXKMainStoryRules::NextMainlineNode(const FGameXXKRuntimeState& State,FName ChapterId)
{
 const auto* C=FGameXXKMainStoryCatalog::FindChapter(ChapterId);if(!C)return NAME_None;
 for(FName Id:C->Nodes){const auto* N=FGameXXKMainStoryCatalog::FindNode(Id);if(!N||N->bOptional)continue;const auto S=NodeState(State,Id);if(S==EGameXXKTaskState::Available||S==EGameXXKTaskState::Active)return Id;}
 return NAME_None;
}
