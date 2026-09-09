#include "Narrative/GameXXKMainStoryRules.h"
#include "Narrative/GameXXKStoryCatalog.h"
#include "Narrative/GameXXKStoryRules.h"

namespace
{
const FName LineKey(TEXT("MainStory.DialogueLine"));
const FName HintKey(TEXT("MainStory.HintLevel"));
bool Fail(FString* Error,const FString& Text){if(Error)*Error=Text;return false;}
void Clear(FString* Error){if(Error)Error->Reset();}
bool Done(const FGameXXKTaskProgress* P){return P&&(P->State==EGameXXKTaskState::Completed||P->State==EGameXXKTaskState::Rewarded);}
bool Requirements(const FGameXXKRuntimeState& State,const FGameXXKMainStoryNode& Node)
{
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
 if(Node.Kind==EGameXXKMainStoryNodeKind::JourneyBattle)S.Phase=EGameXXKMainStoryActivityPhase::ReadyToBattle;
 else if(Node.IsInvestigation())S.Phase=EGameXXKMainStoryActivityPhase::Choice;
}
}
bool FGameXXKMainStoryRules::IsChapterUnlocked(const FGameXXKRuntimeState& State,FName Id)
{
 const auto* C=FGameXXKMainStoryCatalog::FindChapter(Id);if(!C)return false;
 if(!FGameXXKTrainingRules::CanChallenge(State.Training,FGameXXKMainStoryCatalog::StageId(*C)))return false;
 if(C->StageNumber==1)return true;
 const int32 Previous=C->StageNumber-2;
 return FGameXXKMainStoryCatalog::Chapters().IsValidIndex(Previous)
  &&State.NarrativeProgress.MainStory.MainlineCompletedChapters.Contains(FGameXXKMainStoryCatalog::Chapters()[Previous].Id);
}
bool FGameXXKMainStoryRules::IsNodeCompleted(const FGameXXKRuntimeState& State,FName Id){return Done(State.NarrativeProgress.TaskProgressById.Find(Id));}
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
 S.LineIndex=FMath::Clamp(P.TaskProgressById.FindChecked(Id).ObjectiveCounts.FindRef(LineKey),0,N->Lines.Num());
 if(IsNodeCompleted(State,Id))S.Phase=EGameXXKMainStoryActivityPhase::Result;
 else if(N->IsJourney()&&!(S.JourneyNodeId==Id&&S.bGateEntered))S.Phase=EGameXXKMainStoryActivityPhase::ReadyToTravel;
 else if(S.LineIndex>=N->Lines.Num())AfterDialogue(State,*N);
 else S.Phase=EGameXXKMainStoryActivityPhase::Dialogue;
 MarkChapterSeen(State,N->ChapterId);++S.Revision;Clear(Error);return true;
}
bool FGameXXKMainStoryRules::AdvanceDialogue(FGameXXKRuntimeState& State,FString* Error)
{
 auto& S=State.NarrativeProgress.MainStory;const auto* N=FGameXXKMainStoryCatalog::FindNode(S.ActiveNodeId);
 if(!N||S.Phase!=EGameXXKMainStoryActivityPhase::Dialogue)return Fail(Error,TEXT("当前没有可以推进的对白。"));
 auto* P=State.NarrativeProgress.TaskProgressById.Find(N->Id);
 if(!P||P->State!=EGameXXKTaskState::Active)return Fail(Error,TEXT("这段任务没有处于进行状态。"));
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
 S.bBattleWon=true;FString Error;if(!CompleteNode(State,*N,&Error)){S.bBattleWon=false;return false;}return true;
}
void FGameXXKMainStoryRules::PauseActivity(FGameXXKRuntimeState& State)
{
 auto& S=State.NarrativeProgress.MainStory;S.ActiveNodeId=NAME_None;S.Phase=EGameXXKMainStoryActivityPhase::None;++S.Revision;
}
bool FGameXXKMainStoryRules::ValidateState(const FGameXXKRuntimeState& State,FString* Error)
{
 const auto& S=State.NarrativeProgress.MainStory;
 if(S.Revision<0||S.LineIndex<0)return Fail(Error,TEXT("Main-story session counters are invalid."));
 for(FName Id:S.SeenChapters)if(!FGameXXKMainStoryCatalog::FindChapter(Id))return Fail(Error,TEXT("Viewed story chapter does not exist."));
 for(FName Id:S.MainlineCompletedChapters){const auto* C=FGameXXKMainStoryCatalog::FindChapter(Id);if(!C||!IsNodeCompleted(State,C->MainlineEnd))return Fail(Error,TEXT("Mainline completion has no completed ending node."));}
 for(FName Id:S.JourneyStartedNodes){const auto* N=FGameXXKMainStoryCatalog::FindNode(Id);if(!N||!N->IsJourney())return Fail(Error,TEXT("Story journey identity is invalid."));}
 if(!S.ActiveNodeId.IsNone())
 {
  const auto* N=FGameXXKMainStoryCatalog::FindNode(S.ActiveNodeId);const auto* P=State.NarrativeProgress.TaskProgressById.Find(S.ActiveNodeId);
  if(!N||!P||S.LineIndex>N->Lines.Num())return Fail(Error,TEXT("Active story node or dialogue position is invalid."));
  if(S.Phase==EGameXXKMainStoryActivityPhase::Result&&!Done(P))return Fail(Error,TEXT("Story result requires completed node evidence."));
  if(S.Phase!=EGameXXKMainStoryActivityPhase::Result&&S.Phase!=EGameXXKMainStoryActivityPhase::None&&P->State!=EGameXXKTaskState::Active)return Fail(Error,TEXT("Active story activity is not attached to an active task."));
 }
 else if(S.Phase!=EGameXXKMainStoryActivityPhase::None)return Fail(Error,TEXT("Story activity phase has no node."));
 if(!S.JourneyNodeId.IsNone())
 {
  const auto* N=FGameXXKMainStoryCatalog::FindNode(S.JourneyNodeId);
  if(!N||!N->IsJourney()||S.JourneyChapterId!=N->ChapterId||S.JourneyStageId!=FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal,N->StageNumber))return Fail(Error,TEXT("Story journey context does not match its content."));
  if(S.bBattleWon&&!IsNodeCompleted(State,N->Id))return Fail(Error,TEXT("Story battle receipt is detached from node completion."));
 }
 Clear(Error);return true;
}
FName FGameXXKMainStoryRules::NextMainlineNode(const FGameXXKRuntimeState& State,FName ChapterId)
{
 const auto* C=FGameXXKMainStoryCatalog::FindChapter(ChapterId);if(!C)return NAME_None;
 for(FName Id:C->Nodes){const auto* N=FGameXXKMainStoryCatalog::FindNode(Id);if(!N||N->bOptional)continue;const auto S=NodeState(State,Id);if(S==EGameXXKTaskState::Available||S==EGameXXKTaskState::Active)return Id;}
 return NAME_None;
}
