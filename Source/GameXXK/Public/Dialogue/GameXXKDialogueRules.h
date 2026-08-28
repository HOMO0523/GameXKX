#pragma once

#include "CoreMinimal.h"
#include "Dialogue/GameXXKDialogueTypes.h"
#include "GameXXKMVPRules.h"

class UGameXXKDialogueAsset;

struct GAMEXXK_API FGameXXKDialogueConditionContext
{
	TSet<FName> Flags;
	TMap<FName, int32> ItemCounts;
	int32 Gold = 0;
	TSet<FName> UnlockedCompanionIds;
	TSet<FName> SelectedOptionIds;
	TSet<FName> SeenNodeIds;
	EGameXXKTutorialQuestState TutorialState = EGameXXKTutorialQuestState::NotStarted;
	TMap<FName, int32> TaskStateValues;
};

class GAMEXXK_API FGameXXKDialogueRules final
{
public:
	static bool Start(
		const UGameXXKDialogueAsset& Asset,
		const FGameXXKDialogueStartContext& Context,
		FGameXXKDialogueSessionState& InOutSession,
		FGameXXKDialogueOutput& OutOutput,
		FString* OutError = nullptr,
		const FGameXXKDialogueConditionContext* ConditionContext = nullptr);

	static bool CompletePresentedNode(
		const UGameXXKDialogueAsset& Asset,
		FGameXXKDialogueSessionState& InOutSession,
		FGameXXKDialogueOutput& OutOutput,
		FString* OutError = nullptr,
		const FGameXXKDialogueConditionContext* ConditionContext = nullptr);

	static bool Choose(
		const UGameXXKDialogueAsset& Asset,
		FName OptionId,
		FGameXXKDialogueSessionState& InOutSession,
		FGameXXKDialogueOutput& OutOutput,
		FString* OutError = nullptr,
		const FGameXXKDialogueConditionContext* ConditionContext = nullptr);

	static bool Resume(
		const UGameXXKDialogueAsset& Asset,
		FGameXXKDialogueSessionState& InOutSession,
		FGameXXKDialogueOutput& OutOutput,
		FString* OutError = nullptr,
		const FGameXXKDialogueConditionContext* ConditionContext = nullptr);

	static bool EvaluateConditions(
		const TMap<FName, FString>& Conditions,
		const FGameXXKDialogueConditionContext& Context,
		FString* OutError = nullptr);
};
