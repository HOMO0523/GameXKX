#pragma once

#include "CoreMinimal.h"
#include "GameXXKCardTypes.h"

struct FGameXXKCardEnemyIntent;
struct FGameXXKRuntimeState;

struct FGameXXKEnemyIntentStatusText
{
	EGameXXKCardStatus Status = EGameXXKCardStatus::None;
	int32 Amount = 0;
	FString Target;
};

struct FGameXXKEnemyIntentCardText
{
	FString Title;
	FString Target;
	FString Primary;
	FString PrimaryLabel;
	FString Details;
	bool bDamage = false;
	EGameXXKCardStatus PrimaryStatus = EGameXXKCardStatus::None;
	int32 PrimaryStatusAmount = 0;
	TArray<FGameXXKEnemyIntentStatusText> Statuses;
};

/** Pure, authoritative Chinese presentation for saved enemy intents. */
struct GAMEXXK_API FGameXXKEnemyText
{
	static FString FormatIntentCard(const FGameXXKRuntimeState& State, const FGameXXKCardEnemyIntent& Intent);
	static FGameXXKEnemyIntentCardText BuildIntentCardText(const FGameXXKRuntimeState& State, const FGameXXKCardEnemyIntent& Intent);
	static FString FormatIntentTooltip(const FGameXXKRuntimeState& State, const FGameXXKCardEnemyIntent& Intent);
};
