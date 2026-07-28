#pragma once

#include "CoreMinimal.h"
#include "GameXXKCardTypes.h"
#include "GameXXKEquipmentTypes.h"

/** Shared pure naked-stat formulas for the hero and permanent companions. */
class GAMEXXK_API FGameXXKCharacterStatRules final
{
public:
	static constexpr int32 MaxCharacterLevel = 20;

	static FGameXXKCharacterStats GetBareHeroStats(int32 Level);
	static bool GetBareCompanionStats(
		EGameXXKCharacterRole Role,
		int32 Level,
		int32 Star,
		FGameXXKCharacterStats& OutStats,
		FString* OutError = nullptr);
};
