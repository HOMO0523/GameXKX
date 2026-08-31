#pragma once

#include "CoreMinimal.h"
#include "Dialogue/GameXXKDialogueTypes.h"
#include "GameFramework/Actor.h"
#include "Interaction/GameXXKInteractable.h"
#include "Prologue/GameXXKPrologueAftermathTypes.h"
#include "UObject/SoftObjectPtr.h"

#include "GameXXKPrologueAftermathController.generated.h"

class AGameXXKHeroCharacter;
class AGameXXKMVPPlayerController;
class AGameXXKPrologueCarriageRig;
class AGameXXKTownNpcCharacter;
class UGameXXKDialogueAsset;
class UGameXXKDialogueCoordinator;
class UGameXXKDialoguePanelWidget;
class UGameXXKGuidePreferenceWidget;
class UGameXXKInteractableComponent;
class UGameXXKMVPSubsystem;
class UGameXXKPrologueMapWidget;
class UGameXXKProloguePauseWidget;
class UGameXXKPrologueYueBaiWidget;
class UGameXXKSpeechBubbleWidget;
class UGameXXKTutorial01SessionSubsystem;
class USceneComponent;
class USphereComponent;
class UTexture2D;
class UWidgetComponent;
struct FInputKeyEventArgs;
enum class EGameXXKGuidePreference : uint8;
enum class EGameXXKTutorial01ReturnReason : uint8;

UCLASS()
class GAMEXXK_API AGameXXKPrologueAftermathController : public AActor, public IGameXXKInteractable
{
	GENERATED_BODY()

public:
	AGameXXKPrologueAftermathController();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;
	virtual void NotifyActorEndOverlap(AActor* OtherActor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;
	virtual void Interact_Implementation(APawn* InstigatorPawn) override;

	bool HandleInputKey(const FInputKeyEventArgs& Params);
	bool TogglePauseFromController();
	bool CancelPresentation();
	bool IsBlockingPresentation() const;

	FVector GetYueBaiRevealOffsetForTest() const;
	FVector GetStatueInteractionOffsetForTest() const;
	float GetStatueInteractionRadiusForTest() const;
	TSubclassOf<UGameXXKGuidePreferenceWidget> GetGuidePreferenceWidgetClassForTest() const
	{
		return GuidePreferenceWidgetClass;
	}
	bool CanOpenGuideChoiceForTest() const;
	FText GetStatuePromptTextForTest() const { return StatuePromptText; }
	FName GetNoticeDialogueIdForTest() const { return NoticeDialogueId; }
	FName GetMeetingDialogueIdForTest() const { return MeetingDialogueId; }
	TSubclassOf<UGameXXKPrologueMapWidget> GetMapWidgetClassForTest() const
	{
		return MapWidgetClass;
	}
	TSubclassOf<UGameXXKPrologueYueBaiWidget> GetYueBaiWidgetClassForTest() const
	{
		return YueBaiWidgetClass;
	}
	bool ShouldActivateForOptionsForTest(const FString& Options) const;
	bool ApplyTutorialReturnReasonForTest(
		EGameXXKTutorial01ReturnReason ReturnReason);
	bool StartRulesForTest();
	bool ApplyEventForTest(EGameXXKPrologueAftermathEvent Event);
	bool PrepareTutorial01TravelForTest(
		EGameXXKGuidePreference Preference,
		UGameXXKMVPSubsystem* MVPSubsystem,
		UGameXXKTutorial01SessionSubsystem* TutorialSession,
		const FTransform& StatueReturnTransform);
	FGameXXKPrologueAftermathState GetAftermathStateForTest() const
	{
		return AftermathState;
	}
	bool IsBlockingInputForTest() const { return IsBlockingPresentation(); }
	static AGameXXKTownNpcCharacter* FindUniqueYueBaiForTest(
		const TArray<AGameXXKTownNpcCharacter*>& Candidates);

private:
	void HandleCarriageFinished();
	bool ResumeFromTutorialReturn();
	bool StartPresentation();
	bool ResolveRuntimeActors();
	bool EnsurePresentationWidgets();
	bool StartDialogue(FName DialogueId);
	UGameXXKDialogueAsset* ResolveDialogueAsset(FName DialogueId) const;
	void HandleDialogueFinished(FName DialogueId, FName OutcomeId);
	void RefreshDialoguePhaseFromCurrentNode();
	void ShowMapCard();
	void HandleMapInspectRequested();
	void HandleMapCloseRequested();
	void HandleMapContinueRequested();
	bool BeginYueBaiIntro();
	void AdvanceYueBaiIntro(float DeltaSeconds);
	void FinishYueBaiIntro();
	UGameXXKPrologueYueBaiWidget* ResolveYueBaiIntroWidget();
	void StartFollowingAndPrompt();
	bool ShowPassivePrompt();
	void UpdatePassivePrompt();
	void HidePassivePrompt();
	bool ShowGuideChoice();
	bool BeginTutorial01Travel(EGameXXKGuidePreference Preference);
	void DismissGuideChoice(bool bRestorePrompt);
	void HandleGuidePreferenceChosen(EGameXXKGuidePreference Preference);
	void SetStatueInteractionEnabled(bool bEnabled);
	bool SetPaused(bool bPaused);
	bool ShowPauseOverlay();
	void HidePauseOverlay();
	void HandleResumeRequested();
	void HandleReturnDesktopRequested();
	void CleanupPresentation(bool bKeepFollower);

	UPROPERTY(VisibleAnywhere, Category = "GameXXK|Prologue|Aftermath")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "GameXXK|Prologue|Aftermath")
	TObjectPtr<USceneComponent> YueBaiReveal;

	UPROPERTY(VisibleAnywhere, Category = "GameXXK|Prologue|Aftermath")
	TObjectPtr<UWidgetComponent> YueBaiIntroDisplay;

	UPROPERTY(VisibleAnywhere, Category = "GameXXK|Prologue|Aftermath")
	TObjectPtr<USphereComponent> StatueInteractionArea;

	UPROPERTY(VisibleAnywhere, Category = "GameXXK|Prologue|Aftermath")
	TObjectPtr<UGameXXKInteractableComponent> StatueInteractionMetadata;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|Prologue|Aftermath")
	TSubclassOf<UGameXXKPrologueMapWidget> MapWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|Prologue|Aftermath")
	TSubclassOf<UGameXXKPrologueYueBaiWidget> YueBaiWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|Prologue|Aftermath")
	TSubclassOf<UGameXXKGuidePreferenceWidget> GuidePreferenceWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|Prologue|Aftermath")
	TSoftObjectPtr<UTexture2D> YueBaiIntroTexture2K;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|Prologue|Aftermath")
	TSoftObjectPtr<UTexture2D> YueBaiIntroTexture1K;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|Prologue|Aftermath")
	FName NoticeDialogueId = TEXT("Dialogue.Tutorial.CarriageNotice");

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|Prologue|Aftermath")
	FName MeetingDialogueId = TEXT("Dialogue.Tutorial.YueBaiFirstMeeting");

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|Prologue|Aftermath")
	FText StatuePromptText;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKDialogueCoordinator> DialogueCoordinator;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKDialoguePanelWidget> DialoguePanel;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKPrologueMapWidget> MapWidget;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKSpeechBubbleWidget> PassivePromptWidget;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKProloguePauseWidget> PauseWidget;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKGuidePreferenceWidget> GuidePreferenceWidget;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> LoadedYueBaiIntroTexture;

	UPROPERTY(Transient)
	TWeakObjectPtr<AGameXXKMVPPlayerController> PlayerController;

	UPROPERTY(Transient)
	TWeakObjectPtr<AGameXXKHeroCharacter> Hero;

	UPROPERTY(Transient)
	TWeakObjectPtr<AGameXXKTownNpcCharacter> YueBai;

	UPROPERTY(Transient)
	FGameXXKPrologueAftermathState AftermathState;

	FGameXXKDialogueSessionState TransientDialogueSession;
	FTransform YueBaiOriginalTransform;
	FDelegateHandle CarriageFinishedHandle;
	TWeakObjectPtr<AGameXXKPrologueCarriageRig> BoundCarriageRig;
	float YueBaiIntroElapsedSeconds = 0.0f;
	bool bYueBaiWasHidden = false;
	bool bYueBaiSnapshotValid = false;
	bool bPresentationActive = false;
	bool bCleanupInProgress = false;
	bool bFoodGestureRequested = false;
	bool bTutorialTravelPending = false;
	EGameXXKGuidePreference SelectedGuidePreference;
};
