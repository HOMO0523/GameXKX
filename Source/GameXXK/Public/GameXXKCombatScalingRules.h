#pragma once

#include "CoreMinimal.h"
#include "GameXXKCardTypes.h"

class GAMEXXK_API FGameXXKCombatScalingRules final
{
public:
	static int32 GetQualityPercent(EGameXXKCardQuality Quality);
	static int32 ScaleContinuousCeil(int32 BaseValue, EGameXXKCardQuality Quality);
	static int32 ScaleByPercentCeil(int32 BaseValue, int32 Percent);
	static int32 ResolveDotAddition(int32 BaseCoefficient, EGameXXKCardQuality Quality, int32 TeamMaxLevel);
	static int32 ResolveMedicineHealing(int32 BaseCoefficient, int32 Medicine, EGameXXKCardQuality Quality, int32 TeamMaxLevel);
	static int32 ResolveDotCap(int32 TeamMaxLevel);
	static int32 ResolvePrintedCostArmor(int32 CasterDefense, int32 PrintedEnergyCost, EGameXXKCardQuality Quality);
	static int32 ApplyLevelDifferenceCeil(int32 Damage, int32 SourceLevel, int32 TargetLevel);
};
