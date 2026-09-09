#include "Guide/GameXXKAcademySubsystem.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "UI/GameXXKBattleBoardWidget.h"
#include "UI/GameXXKDesktopTrainingWorkbenchWidget.h"
#include "UI/GameXXKInRunUiStyle.h"
#include "GameXXKCardCatalog.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/Font.h"
#include "Engine/GameInstance.h"

const FGameXXKAcademyLesson* UGameXXKAcademySubsystem::Lesson() const
{
	const auto* C=Course();return C && C->Lessons.IsValidIndex(ActiveLessonIndex) ? &C->Lessons[ActiveLessonIndex] : nullptr;
}
void UGameXXKAcademySubsystem::RefreshPlayerUi()
{
	if(auto* PC=Cast<AGameXXKMVPPlayerController>(GetGameInstance()->GetFirstLocalPlayerController()))
	{
		PC->RefreshPlayerFlowWidgetsFromState();
		if(!IsActive() && OriginalWorkbench.bValid)if(auto* Workbench=PC->GetDesktopTrainingWorkbenchWidgetForTest())Workbench->RestoreSessionStateAfterMapTravel(OriginalWorkbench);
	}
}
bool UGameXXKAcademySubsystem::BeginCourse(FName Id,int32 Index)
{
	if(IsActive()){Message=FText::FromString(TEXT("请先退出当前教程。"));return false;}
	auto* MVP=GetGameInstance()->GetSubsystem<UGameXXKMVPSubsystem>();
	const auto* C=FGameXXKAcademyRules::Find(Id);
	if(!MVP || !C)return false;
	if(MVP->RuntimeState.CardRun.bHasActiveCardBattle){Message=FText::FromString(TEXT("请先结束当前战斗。"));return false;}
	const int32 Completed=MVP->RuntimeState.GuideProgress.AcademyCompletedLessons.FindRef(Id);
	if(Index==INDEX_NONE)Index=Completed<C->Lessons.Num()?Completed:0;
	if(!C->Lessons.IsValidIndex(Index) || Index>Completed){Message=FText::FromString(TEXT("请按顺序完成教程。"));return false;}
	FGameXXKRuntimeState Borrowed;FName Focus;FString Error;
	if(!UGameXXKMVPSubsystem::BuildAcademyBattleState(*C,Index,Borrowed,Focus,Error)){Message=FText::FromString(Error);return false;}
	Original=MVP->RuntimeState;OriginalTravel=MVP->TrainingTravelRuntime;
	if(auto* PC=Cast<AGameXXKMVPPlayerController>(GetGameInstance()->GetFirstLocalPlayerController()))
		if(auto* Workbench=PC->GetDesktopTrainingWorkbenchWidgetForTest())OriginalWorkbench=Workbench->CaptureSessionStateForMapTravel();
	ActiveCourseId=Id;ActiveLessonIndex=Index;FocusUnitId=Focus;Evidence={};Message=FText::GetEmpty();bGuideCueDirty=true;CueMode=0;CueCard=NAME_None;GuideObserveUntil=0;
	MVP->bAcademyWriteGuard=true;MVP->RuntimeState=MoveTemp(Borrowed);MVP->TrainingTravelRuntime={};
	RefreshPlayerUi();return true;
}
void UGameXXKAcademySubsystem::RestoreOriginal()
{
	if(!Original.IsSet())return;
	if(auto* MVP=GetGameInstance()->GetSubsystem<UGameXXKMVPSubsystem>())
	{
		MVP->RuntimeState=Original.GetValue();MVP->TrainingTravelRuntime=OriginalTravel;
		MVP->RuntimeState.Training.TravelLastUpdatedUnixSeconds=FDateTime::UtcNow().ToUnixTimestamp();
		MVP->bAcademyWriteGuard=false;
	}
	Original.Reset();if(Overlay)Overlay->RemoveFromParent();Overlay=nullptr;GoalText=nullptr;GuideCaption=nullptr;GuideSpotlight=nullptr;OverlayBoard.Reset();
}
void UGameXXKAcademySubsystem::CancelCourse(){RestoreOriginal();RefreshPlayerUi();}
void UGameXXKAcademySubsystem::Deinitialize(){RestoreOriginal();Super::Deinitialize();}
bool UGameXXKAcademySubsystem::RetryLesson(){const FName Id=ActiveCourseId;const int32 Index=ActiveLessonIndex;RestoreOriginal();return BeginCourse(Id,Index);}
void UGameXXKAcademySubsystem::OnExit(){CancelCourse();}
void UGameXXKAcademySubsystem::OnRetry()
{
	const auto* MVP=GetGameInstance()->GetSubsystem<UGameXXKMVPSubsystem>();
	if(IsActive() && Lesson() && Evidence.Satisfies(*Lesson()) && MVP && MVP->GetRuntimeState().CardRun.ActiveBattle.Phase==EGameXXKCardBattlePhase::Victory)
		HandleTerminal(EGameXXKCardBattlePhase::Victory);
	else RetryLesson();
}

void UGameXXKAcademySubsystem::Observe(const FGameXXKCardBattleRuntime& Before,const FGameXXKCardBattleRuntime& After,
	const TArray<FGameXXKCardDamageResult>& Damage,FName PlayedInstance)
{
	if(IsActive()){FGameXXKAcademyRules::Observe(Before,After,Damage,PlayedInstance,FocusUnitId,Evidence);bGuideCueDirty=true;}
}

bool UGameXXKAcademySubsystem::HandleTerminal(EGameXXKCardBattlePhase Phase)
{
	if(!IsActive() || (Phase!=EGameXXKCardBattlePhase::Victory && Phase!=EGameXXKCardBattlePhase::Defeat))return false;
	if(Phase==EGameXXKCardBattlePhase::Defeat){Message=FText::FromString(TEXT("本次未能获胜，可重试本节。"));return true;}
	auto* MVP=GetGameInstance()->GetSubsystem<UGameXXKMVPSubsystem>();
	FGameXXKRuntimeState Candidate=Original.GetValue();FString Error;int32 Award=0;
	if(!FGameXXKAcademyRules::CompleteLesson(ActiveCourseId,ActiveLessonIndex,true,Evidence,Candidate.GuideProgress,Candidate.PlayerGold,Award,Error))
	{Message=FText::FromString(Error);return true;}
	MVP->bAcademyWriteGuard=false;
	const bool bSaved=MVP->PersistTrainingCheckpoint(Candidate);
	MVP->bAcademyWriteGuard=true;
	if(!bSaved){Message=FText::FromString(TEXT("进度保存失败，尚未发放奖励。"));return true;}
	const FName Id=ActiveCourseId;const int32 Next=ActiveLessonIndex+1;const int32 Count=Course()->Lessons.Num();
	const auto SavedWorkbench=OriginalWorkbench;
	Original=MoveTemp(Candidate);RestoreOriginal();
	if(Next<Count){BeginCourse(Id,Next);OriginalWorkbench=SavedWorkbench;}else RefreshPlayerUi();
	return true;
}


void UGameXXKAcademySubsystem::ObserveCommittedResult(const FGameXXKCardPlayResult& Result)
{
 if(IsActive()){FGameXXKAcademyRules::ObserveCommittedResult(Result,FocusUnitId,Evidence);bGuideCueDirty=true;GuideObserveUntil=FPlatformTime::Seconds()+0.9;}
}
