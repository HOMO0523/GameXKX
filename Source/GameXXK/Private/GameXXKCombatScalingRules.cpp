#include "GameXXKCombatScalingRules.h"

namespace
{
	int32 CeilPositiveRatio(const int64 Numerator, const int64 Denominator)
	{
		if (Numerator <= 0 || Denominator <= 0)
		{
			return 0;
		}
		return static_cast<int32>(FMath::Min<int64>(
			MAX_int32,
			(Numerator + Denominator - 1) / Denominator));
	}
}

int32 FGameXXKCombatScalingRules::GetQualityPercent(const EGameXXKCardQuality Quality)
{
	switch (Quality)
	{
	case EGameXXKCardQuality::Rare:
		return 120;
	case EGameXXKCardQuality::Epic:
		return 140;
	case EGameXXKCardQuality::Common:
		return 100;
	default:
		return 0;
	}
}

int32 FGameXXKCombatScalingRules::ScaleContinuousCeil(
	const int32 BaseValue,
	const EGameXXKCardQuality Quality)
{
	return ScaleByPercentCeil(BaseValue, GetQualityPercent(Quality));
}

int32 FGameXXKCombatScalingRules::ScaleByPercentCeil(const int32 BaseValue, const int32 Percent)
{
	return CeilPositiveRatio(
		static_cast<int64>(FMath::Max(0, BaseValue)) * FMath::Max(0, Percent),
		100);
}

int32 FGameXXKCombatScalingRules::ResolveDotAddition(
	const int32 BaseCoefficient,
	const EGameXXKCardQuality Quality,
	const int32 TeamMaxLevel)
{
	const int64 Numerator = static_cast<int64>(FMath::Max(0, BaseCoefficient))
		* GetQualityPercent(Quality)
		* (FMath::Clamp(TeamMaxLevel, 1, 135) + 25);
	return CeilPositiveRatio(Numerator, 2500);
}

int32 FGameXXKCombatScalingRules::ResolveManaOverflowArmor(
	const int32 OverflowMana,
	const int32 ConversionPercent,
	const EGameXXKCardQuality Quality,
	const int32 TeamMaxLevel)
{
	const int64 GenerationFactor = static_cast<int64>(GetQualityPercent(Quality))
		* (FMath::Clamp(TeamMaxLevel, 1, 135) + 25);
	if (GenerationFactor <= 0)
	{
		return 0;
	}
	const int64 BaseProduct = static_cast<int64>(FMath::Max(0, OverflowMana))
		* FMath::Max(0, ConversionPercent);
	constexpr int64 Denominator = 250000;
	// Saturate before the remaining multiplication; round the complete grant once.
	if (BaseProduct > static_cast<int64>(MAX_int32) * Denominator / GenerationFactor)
	{
		return MAX_int32;
	}
	return CeilPositiveRatio(BaseProduct * GenerationFactor, Denominator);
}

int32 FGameXXKCombatScalingRules::ResolveMedicineHealing(
	const int32 BaseCoefficient,
	const int32 Medicine,
	const EGameXXKCardQuality Quality,
	const int32 TeamMaxLevel,
	const EGameXXKCardQuality /*LegacyCoefficientReferenceQuality*/)
{
	// Every healing coefficient is a raw value. Native card quality never cancels scaling.
	const int64 CombinedCoefficient = static_cast<int64>(FMath::Max(0, BaseCoefficient))
		+ static_cast<int64>(FMath::Max(0, Medicine));
	return CeilPositiveRatio(CombinedCoefficient * GetQualityPercent(Quality)
		* (FMath::Clamp(TeamMaxLevel, 1, 135) + 25), 2500);
}

int32 FGameXXKCombatScalingRules::ResolveDotCap(const int32 TeamMaxLevel)
{
	const int32 Level = FMath::Clamp(TeamMaxLevel, 1, 135);
	return FMath::Max(25, 25 * FMath::DivideAndRoundUp(Level, 25));
}

int32 FGameXXKCombatScalingRules::ResolvePrintedCostArmor(
	const int32 CasterDefense,
	const int32 PrintedEnergyCost,
	const EGameXXKCardQuality Quality)
{
	const int32 CostPercent = PrintedEnergyCost <= 0
		? 40
		: PrintedEnergyCost == 1
			? 80
			: PrintedEnergyCost == 2
				? 140
				: 200;
	const int64 Numerator = static_cast<int64>(FMath::Max(0, CasterDefense))
		* CostPercent
		* GetQualityPercent(Quality);
	return CeilPositiveRatio(Numerator, 10000);
}

int32 FGameXXKCombatScalingRules::ApplyLevelDifferenceCeil(
	const int32 Damage,
	const int32 SourceLevel,
	const int32 TargetLevel)
{
	const int32 Percent = 100 + FMath::Clamp(SourceLevel - TargetLevel, -50, 50);
	return ScaleByPercentCeil(Damage, Percent);
}
