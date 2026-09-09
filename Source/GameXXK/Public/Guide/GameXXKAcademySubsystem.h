#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Guide/GameXXKAcademyRules.h"
#include "GameXXKMVPRules.h"
#include "UI/GameXXKDesktopWorkbenchSessionState.h"
#include "GameXXKAcademySubsystem.generated.h"

class UGameXXKBattleBoardWidget;
class UTextBlock;
class UCanvasPanel;
class UBorder;
class UGameXXKGuideSpotlightWidget;

/** Owns isolated teaching state; reward persistence is performed only after restoring the original state. */
UCLASS()
class GAMEXXK_API UGameXXKAcademySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual void Deinitialize() override;
	UFUNCTION(BlueprintCallable,Category="GameXXK|Academy")
	bool BeginCourse(FName CourseId,int32 LessonIndex=-1);
	UFUNCTION(BlueprintCallable,Category="GameXXK|Academy")
	void CancelCourse();
	UFUNCTION(BlueprintCallable,Category="GameXXK|Academy")
	bool RetryLesson();
	bool IsActive() const {return Original.IsSet();}
	const FGameXXKAcademyCourse* Course() const {return FGameXXKAcademyRules::Find(ActiveCourseId);}
	const FGameXXKAcademyLesson* Lesson() const;
	const FGameXXKAcademyEvidence& GetEvidence() const {return Evidence;}
	FText GetMessage() const {return Message;}
	void ObserveCommittedResult(const FGameXXKCardPlayResult& Result);
	bool AllowsCard(FName Id) const {return !IsActive() || CueMode==5 || CueMode==4 || (CueMode==0 && Id==CueCard);}
	bool AllowsTarget(FName Id) const {return !IsActive() || CueMode==5 || (CueMode==1 && CueTargets.Contains(Id));}
	bool AllowsEndTurn() const {return !IsActive() || CueMode==5 || CueMode==2;}
	void Observe(const FGameXXKCardBattleRuntime& Before,const FGameXXKCardBattleRuntime& After,const TArray<FGameXXKCardDamageResult>& Damage,FName PlayedInstance);
	bool HandleTerminal(EGameXXKCardBattlePhase Phase);
	void RefreshOverlay(UGameXXKBattleBoardWidget* Board);
private:
	void RestoreOriginal();
	void RefreshPlayerUi();
	void UpdateGuidance(UGameXXKBattleBoardWidget* Board,bool bWatching);
	int32 CueMode=0;
	FName CueCard;
	TArray<FName> CueTargets;
	FText CueText;
	bool bGuideCueDirty=true;
	bool bLastGuideTargeting=false;
	bool bLastGuideWatching=false;
	double GuideObserveUntil=0;
	FName ActiveCourseId;
	int32 ActiveLessonIndex=0;
	FName FocusUnitId;
	FGameXXKAcademyEvidence Evidence;
	TOptional<FGameXXKRuntimeState> Original;
	FGameXXKTrainingTravelRuntime OriginalTravel;
	FGameXXKDesktopWorkbenchSessionState OriginalWorkbench;
	FText Message;
	TWeakObjectPtr<UGameXXKBattleBoardWidget> OverlayBoard;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> Overlay;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> GoalText;
	UPROPERTY(Transient) TObjectPtr<UBorder> GuideCaption;
	UPROPERTY(Transient) TObjectPtr<UGameXXKGuideSpotlightWidget> GuideSpotlight;
	UFUNCTION() void OnExit();
	UFUNCTION() void OnRetry();
};
