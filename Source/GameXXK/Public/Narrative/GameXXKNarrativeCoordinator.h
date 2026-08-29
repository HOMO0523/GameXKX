#pragma once

#include "CoreMinimal.h"
#include "Narrative/GameXXKNarrativeCommandExecutor.h"
#include "Narrative/GameXXKNarrativeSequenceTypes.h"
#include "UObject/Object.h"

#include "GameXXKNarrativeCoordinator.generated.h"

class UGameXXKNarrativeSequenceAsset;
struct FGameXXKRuntimeState;

/** Explicit presenter families; desktop dialogue never falls back to the NPC viewport host. */
enum class EGameXXKNarrativeDialogueHost : uint8
{
	None,
	Desktop2D,
	LegacyNpc3D
};

UCLASS()
class GAMEXXK_API UGameXXKNarrativeCoordinator : public UObject
{
	GENERATED_BODY()

public:
	virtual void BeginDestroy() override;

	void BindState(
		FGameXXKRuntimeState& InRuntimeState,
		FGameXXKNarrativeSequenceSessionState& InSessionState);

	bool RegisterExecutor(
		FName CommandType,
		TSharedRef<IGameXXKNarrativeCommandExecutor> Executor);

	void SetDialogueStartDelegate(FGameXXKNarrativeDialogueStartRequest Delegate);
	void SetDialogueStartDelegate(
		EGameXXKNarrativeDialogueHost Host,
		FGameXXKNarrativeDialogueStartRequest Delegate);
	void ClearDialogueStartDelegate(EGameXXKNarrativeDialogueHost Host);
	void ClearDialogueStartDelegates();
	bool SelectDialogueHost(
		EGameXXKNarrativeDialogueHost Host,
		FString* OutError = nullptr);
	EGameXXKNarrativeDialogueHost GetDialogueHost() const { return DialogueHost; }
	EGameXXKNarrativeDialogueHost GetActiveDialogueHostAffinity() const
	{
		return ActiveDialogueHostAffinity;
	}
#if WITH_DEV_AUTOMATION_TESTS
	FGameXXKNarrativeDialogueCompleted GetLastIssuedDialogueCompletionForTest() const
	{
		return LastIssuedDialogueCompletionForTest;
	}
#endif
	void SetCandidateValidator(FGameXXKNarrativeCandidateValidator Delegate);

	bool StartSequence(
		UGameXXKNarrativeSequenceAsset& Asset,
		const FGameXXKNarrativeStartContext& Context,
		FString* OutError = nullptr);

	bool Resume(FString* OutError = nullptr);
	bool ResumeSequence(
		UGameXXKNarrativeSequenceAsset& Asset,
		FString* OutError = nullptr);
	bool CompletePendingCommand(
		EGameXXKNarrativeCommandStatus Status,
		FString* OutError = nullptr);
	bool CompletePendingWait(FString* OutError = nullptr);

	void PauseAndRelease();
	void CancelForMapTravel();
	bool IsInputTokenHeld() const;

private:
	bool DispatchRequest(const FGameXXKNarrativeRequest& Request, FString* OutError);
	bool DispatchCommand(const FGameXXKNarrativeRequest& Request, FString* OutError);
	bool CommitAdvancedCandidate(
		FGameXXKRuntimeState&& RuntimeCandidate,
		FGameXXKNarrativeSequenceSessionState&& SessionCandidate,
		const FGameXXKNarrativeRequest& NextRequest,
		FString* OutError);
	void HandleDialogueCompleted(FName OutcomeId, uint64 CompletionGeneration);
	void ReleaseInputToken();
	void CancelPendingExecutor();
	void InvalidateDialogueCompletion();
	FGameXXKNarrativeDialogueStartRequest* ResolveDialogueStartDelegate(
		EGameXXKNarrativeDialogueHost Host);

	FGameXXKRuntimeState* RuntimeState = nullptr;
	FGameXXKNarrativeSequenceSessionState* SessionState = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKNarrativeSequenceAsset> ActiveAsset;

	TMap<FName, TSharedPtr<IGameXXKNarrativeCommandExecutor>> Executors;
	FGameXXKNarrativeDialogueStartRequest DesktopDialogueStartDelegate;
	FGameXXKNarrativeDialogueStartRequest LegacyDialogueStartDelegate;
	FGameXXKNarrativeCandidateValidator CandidateValidator;
	FName PendingCommandType;
	EGameXXKNarrativeDialogueHost DialogueHost = EGameXXKNarrativeDialogueHost::None;
	EGameXXKNarrativeDialogueHost ActiveDialogueHostAffinity =
		EGameXXKNarrativeDialogueHost::None;
	uint64 DialogueCompletionGenerationCounter = 0;
	uint64 ActiveDialogueCompletionGeneration = 0;
	bool bInputTokenHeld = false;
	int32 DispatchDepth = 0;
#if WITH_DEV_AUTOMATION_TESTS
	FGameXXKNarrativeDialogueCompleted LastIssuedDialogueCompletionForTest;
#endif
};
