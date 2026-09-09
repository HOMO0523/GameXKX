#pragma once
#include "CoreMinimal.h"
#include "GameXXKTalentTypes.h"

struct FGameXXKTalentTotalEntry
{
	FName Id;
	FText Label;
	FText Value;
	int32 Amount=0;
};

struct FGameXXKTalentTotalGroup
{
	FText Title;
	TArray<FGameXXKTalentTotalEntry> Entries;
};

/** Read-only, merged effective bonuses from learned talents, excluding gear and migration floors. */
namespace GameXXKTalentTotals
{
	GAMEXXK_API bool Build(const FGameXXKTalentProgress& Progress,TArray<FGameXXKTalentTotalGroup>& OutGroups,FString* Error=nullptr);
}
