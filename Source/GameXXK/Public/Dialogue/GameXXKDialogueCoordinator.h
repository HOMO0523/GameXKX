#pragma once

#include "CoreMinimal.h"
#include "Dialogue/GameXXKDialogueRules.h"
#include "UObject/Object.h"

#include "GameXXKDialogueCoordinator.generated.h"

class UGameXXKDialogueAsset;
class UGameXXKDialogueHistoryWidget;
class UGameXXKDialoguePanelWidget;
class UGameXXKSpeechBubbleWidget;
class USceneComponent;

DECLARE_DELEGATE_TwoParams(FGameXXKDialogueFinished, FName, FName);
DECLARE_DELEGATE_RetVal_OneParam(USceneComponent*, FGameXXKDialogueBubbleAnchorResolver, FName);
DECLARE_DELEGATE(FGameXXKDialoguePresenterPaused);

UCLASS(BlueprintType)
class GAMEXXK_API UGameXXKDialogueCoordinator : public UObject
{
	GENERATED_BODY()

public:
	void Bind(
		FGameXXKDialogueSessionState& InSession,
		UGameXXKDialoguePanelWidget* InPanel,
		UGameXXKSpeechBubbleWidget* InBubble,
		UGameXXKDialogueHistoryWidget* InHistory);
	void SetBubbleAnchorResolver(FGameXXKDialogueBubbleAnchorResolver InResolver);
	void SetPresenterPausedDelegate(FGameXXKDialoguePresenterPaused InDelegate);

	bool StartDialogue(
		const UGameXXKDialogueAsset& Asset,
		const FGameXXKDialogueStartContext& Context,
		FGameXXKDialogueFinished OnFinished,
		FString* OutError = nullptr);
	bool ResumeDialogue(const UGameXXKDialogueAsset& Asset, FString* OutError = nullptr);
	bool Advance(FString* OutError = nullptr);
	bool ChooseOption(FName OptionId, FString* OutError = nullptr);
	bool SkipSeenCurrentNode(FString* OutError = nullptr);
	void PauseAndExit();

	void SetAutoEnabled(bool bEnabled);
	bool IsAutoEnabled() const;
	void SetPresentationDurations(float VoiceDurationSeconds, float AnimationDurationSeconds);
	bool TickAuto(float DeltaSeconds, FString* OutError = nullptr);
	static float ComputeAutoDelayForTest(
		int32 VisibleCharacters,
		float VoiceDurationSeconds,
		float AnimationDurationSeconds);

	bool IsBlockingPresentation() const;
	FName GetCurrentNodeIdForTest() const;
	const FGameXXKDialogueOutput& GetCurrentOutputForTest() const;

private:
	bool PresentOutput(const FGameXXKDialogueOutput& Output, FString* OutError);
	FGameXXKDialoguePresentationView BuildPresentationView(const FGameXXKDialogueOutput& Output) const;
	void RefreshHistory();
	void FinishOnce(FName OutcomeId);
	void HidePresenters();
	void HandlePanelAdvance();
	void HandlePanelOption(FName OptionId);
	static int32 CountVisibleCharacters(const FText& Text);

	FGameXXKDialogueSessionState* Session = nullptr;
	TWeakObjectPtr<UGameXXKDialoguePanelWidget> Panel;
	TWeakObjectPtr<UGameXXKSpeechBubbleWidget> Bubble;
	TWeakObjectPtr<UGameXXKDialogueHistoryWidget> History;
	TWeakObjectPtr<const UGameXXKDialogueAsset> ActiveAsset;
	FGameXXKDialogueOutput CurrentOutput;
	FName ActiveDialogueId;
	FGameXXKDialogueFinished FinishedDelegate;
	FGameXXKDialogueBubbleAnchorResolver BubbleAnchorResolver;
	FGameXXKDialoguePresenterPaused PresenterPausedDelegate;
	bool bBlockingPresentation = false;
	bool bPaused = false;
	bool bAutoEnabled = false;
	bool bFinishDispatched = false;
	float AutoElapsedSeconds = 0.0f;
	float VoiceDurationSeconds = 0.0f;
	float AnimationDurationSeconds = 0.0f;
};
