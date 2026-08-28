#pragma once

#include "CoreMinimal.h"
#include "Narrative/GameXXKNarrativeSequenceTypes.h"

class UGameXXKNarrativeSequenceAsset;

class GAMEXXK_API FGameXXKNarrativeSequenceRules final
{
public:
	static bool Start(
		const UGameXXKNarrativeSequenceAsset& Asset,
		const FGameXXKNarrativeStartContext& Context,
		FGameXXKNarrativeSequenceSessionState& InOutSession,
		FGameXXKNarrativeRequest& OutRequest,
		FString* OutError = nullptr);

	static bool Resume(
		const UGameXXKNarrativeSequenceAsset& Asset,
		FGameXXKNarrativeSequenceSessionState& InOutSession,
		FGameXXKNarrativeRequest& OutRequest,
		FString* OutError = nullptr);

	static bool CompleteCommand(
		const UGameXXKNarrativeSequenceAsset& Asset,
		EGameXXKNarrativeCommandStatus Status,
		FGameXXKNarrativeSequenceSessionState& InOutSession,
		FGameXXKNarrativeRequest& OutRequest,
		FString* OutError = nullptr);

	static bool CompleteWait(
		const UGameXXKNarrativeSequenceAsset& Asset,
		FGameXXKNarrativeSequenceSessionState& InOutSession,
		FGameXXKNarrativeRequest& OutRequest,
		FString* OutError = nullptr);

	static bool CompleteDialogue(
		const UGameXXKNarrativeSequenceAsset& Asset,
		FName OutcomeId,
		FGameXXKNarrativeSequenceSessionState& InOutSession,
		FGameXXKNarrativeRequest& OutRequest,
		FString* OutError = nullptr);

	static FName MakeCommandKey(FName StoryId, FName TaskId, FName StepId, FName CommandId);
};
