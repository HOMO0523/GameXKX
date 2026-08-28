#pragma once

#include "CoreMinimal.h"
#include "Dialogue/GameXXKDialogueTypes.h"

class UGameXXKDialogueAsset;

class GAMEXXK_API FGameXXKDialogueRules final
{
public:
	static bool Start(
		const UGameXXKDialogueAsset& Asset,
		const FGameXXKDialogueStartContext& Context,
		FGameXXKDialogueSessionState& InOutSession,
		FGameXXKDialogueOutput& OutOutput,
		FString* OutError = nullptr);

	static bool CompletePresentedNode(
		const UGameXXKDialogueAsset& Asset,
		FGameXXKDialogueSessionState& InOutSession,
		FGameXXKDialogueOutput& OutOutput,
		FString* OutError = nullptr);

	static bool Choose(
		const UGameXXKDialogueAsset& Asset,
		FName OptionId,
		FGameXXKDialogueSessionState& InOutSession,
		FGameXXKDialogueOutput& OutOutput,
		FString* OutError = nullptr);

	static bool Resume(
		const UGameXXKDialogueAsset& Asset,
		FGameXXKDialogueSessionState& InOutSession,
		FGameXXKDialogueOutput& OutOutput,
		FString* OutError = nullptr);
};
