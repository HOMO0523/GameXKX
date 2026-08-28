#pragma once

#include "CoreMinimal.h"
#include "Narrative/GameXXKNarrativeCommandExecutor.h"
#include "Narrative/GameXXKNarrativeSequenceTypes.h"
#include "UObject/Object.h"

#include "GameXXKNarrativeCoordinator.generated.h"

class UGameXXKNarrativeSequenceAsset;
struct FGameXXKRuntimeState;

UCLASS()
class GAMEXXK_API UGameXXKNarrativeCoordinator : public UObject
{
	GENERATED_BODY()

public:
	void BindState(
		FGameXXKRuntimeState& InRuntimeState,
		FGameXXKNarrativeSequenceSessionState& InSessionState);

	bool RegisterExecutor(
		FName CommandType,
		TSharedRef<IGameXXKNarrativeCommandExecutor> Executor);

	void SetDialogueStartDelegate(FGameXXKNarrativeDialogueStartRequest Delegate);
	void SetCandidateValidator(FGameXXKNarrativeCandidateValidator Delegate);

	bool StartSequence(
		UGameXXKNarrativeSequenceAsset& Asset,
		const FGameXXKNarrativeStartContext& Context,
		FString* OutError = nullptr);

	bool Resume(FString* OutError = nullptr);
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
	void HandleDialogueCompleted(FName OutcomeId);
	void ReleaseInputToken();
	void CancelPendingExecutor();

	FGameXXKRuntimeState* RuntimeState = nullptr;
	FGameXXKNarrativeSequenceSessionState* SessionState = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKNarrativeSequenceAsset> ActiveAsset;

	TMap<FName, TSharedPtr<IGameXXKNarrativeCommandExecutor>> Executors;
	FGameXXKNarrativeDialogueStartRequest DialogueStartDelegate;
	FGameXXKNarrativeCandidateValidator CandidateValidator;
	FName PendingCommandType;
	bool bInputTokenHeld = false;
	int32 DispatchDepth = 0;
};
