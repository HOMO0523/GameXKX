#pragma once

#include "CoreMinimal.h"
#include "Narrative/GameXXKNarrativeSequenceTypes.h"

struct FGameXXKRuntimeState;

struct GAMEXXK_API FGameXXKNarrativeCommandResult
{
	EGameXXKNarrativeCommandStatus Status = EGameXXKNarrativeCommandStatus::Failed;
	FString Error;
};

class GAMEXXK_API IGameXXKNarrativeCommandExecutor
{
public:
	virtual ~IGameXXKNarrativeCommandExecutor() = default;
	virtual bool Supports(FName CommandType) const = 0;
	virtual FGameXXKNarrativeCommandResult Execute(
		const FGameXXKNarrativeCommandDefinition& Command,
		FGameXXKRuntimeState& InOutCandidateState) = 0;
	virtual void CancelPending() = 0;
};

DECLARE_DELEGATE_OneParam(FGameXXKNarrativeDialogueCompleted, FName);
DECLARE_DELEGATE_TwoParams(
	FGameXXKNarrativeDialogueStartRequest,
	FName,
	FGameXXKNarrativeDialogueCompleted);
DECLARE_DELEGATE_RetVal_TwoParams(
	bool,
	FGameXXKNarrativeCandidateValidator,
	const FGameXXKRuntimeState&,
	FString&);
