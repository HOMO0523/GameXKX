#pragma once

#include "CoreMinimal.h"

struct FGameXXKCardEnemyIntent;
struct FGameXXKRuntimeState;

/** Pure, authoritative Chinese presentation for saved enemy intents. */
struct GAMEXXK_API FGameXXKEnemyText
{
	static FString FormatIntentCard(const FGameXXKRuntimeState& State, const FGameXXKCardEnemyIntent& Intent);
	static FString FormatIntentTooltip(const FGameXXKRuntimeState& State, const FGameXXKCardEnemyIntent& Intent);
};
