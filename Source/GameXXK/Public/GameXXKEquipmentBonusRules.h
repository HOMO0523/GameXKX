#pragma once

#include "CoreMinimal.h"

struct FGameXXKEquipmentActiveEffect;

/** Shared final-value calculation for gameplay and character detail presentation. */
struct GAMEXXK_API FGameXXKEquipmentArmorBonus
{
	int64 RawAffixBasisPoints = 0;
	int64 GemBasisPoints = 0;
	int64 FixedBasisPoints = 0;

	void AddEffect(const FGameXXKEquipmentActiveEffect& Effect, FName ExpectedSource);
	double GetFinalPercent() const;
	int32 ApplyTo(int32 BaseArmor) const;
};

class GAMEXXK_API FGameXXKEquipmentBonusRules final
{
public:
	static constexpr int32 AffixCapBasisPoints = 7500;
	static constexpr int32 AffixNominalDivisor = 2;

	/** Raw stored values stay unchanged; apply alpha=0.5 and the 75% soft ceiling here. */
	static double GetEffectiveAffixBasisPoints(int64 RawBasisPoints);
};
