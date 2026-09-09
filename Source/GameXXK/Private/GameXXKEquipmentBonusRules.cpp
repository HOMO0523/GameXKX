#include "GameXXKEquipmentBonusRules.h"

#include "GameXXKEquipmentRules.h"

double FGameXXKEquipmentBonusRules::GetEffectiveAffixBasisPoints(const int64 RawBasisPoints)
{
	const int64 Raw = FMath::Clamp<int64>(RawBasisPoints, 0, MAX_int32);
	return static_cast<double>(AffixCapBasisPoints) * Raw
		/ (static_cast<int64>(AffixNominalDivisor) * AffixCapBasisPoints + Raw);
}

void FGameXXKEquipmentArmorBonus::AddEffect(
	const FGameXXKEquipmentActiveEffect& Effect,
	const FName ExpectedSource)
{
	if (ExpectedSource.IsNone() || Effect.SourceCharacterId != ExpectedSource
		|| Effect.ModifierKind != EGameXXKEquipmentModifierKind::ArmorGain
		|| Effect.Unit != EGameXXKEquipmentMagnitudeUnit::BasisPoints || Effect.Magnitude <= 0)
	{
		return;
	}
	const bool bRandomAffix = Effect.RequiredPieces == 0
		&& Effect.EffectId.ToString().StartsWith(TEXT("EquipmentAffixAggregate."));
	int64& Total = bRandomAffix ? RawAffixBasisPoints : FixedBasisPoints;
	Total = FMath::Min<int64>(MAX_int32, FMath::Max<int64>(0, Total) + Effect.Magnitude);
}

double FGameXXKEquipmentArmorBonus::GetFinalPercent() const
{
	return (FGameXXKEquipmentBonusRules::GetEffectiveAffixBasisPoints(
		FMath::Max<int64>(0, RawAffixBasisPoints) + 2 * FMath::Max<int64>(0, GemBasisPoints))
		+ FMath::Clamp<int64>(FixedBasisPoints, 0, MAX_int32)) / 100.0;
}

int32 FGameXXKEquipmentArmorBonus::ApplyTo(const int32 BaseArmor) const
{
	if (BaseArmor <= 0) return 0;
	const int64 Raw = FMath::Clamp<int64>(FMath::Max<int64>(0, RawAffixBasisPoints)
		+ 2 * FMath::Max<int64>(0, GemBasisPoints), 0, MAX_int32);
	const int64 Fixed = FMath::Clamp<int64>(FixedBasisPoints, 0, MAX_int32);
	const int64 Numerator = FGameXXKEquipmentBonusRules::AffixCapBasisPoints * Raw;
	const int64 Denominator = static_cast<int64>(FGameXXKEquipmentBonusRules::AffixNominalDivisor)
		* FGameXXKEquipmentBonusRules::AffixCapBasisPoints + Raw;
	// Split the fraction before multiplying by BaseArmor. All products stay in int64,
	// and the only rounding is the original armor settlement's final upward rounding.
	const int64 WholeBasisPoints = Numerator / Denominator;
	const int64 RemainderProduct = static_cast<int64>(BaseArmor) * (Numerator % Denominator);
	const int64 WholeScaledArmor = static_cast<int64>(BaseArmor) * (10000 + Fixed + WholeBasisPoints)
		+ RemainderProduct / Denominator;
	const bool bHasRemainder = WholeScaledArmor % 10000 != 0 || RemainderProduct % Denominator != 0;
	const int64 Result = WholeScaledArmor / 10000 + (bHasRemainder ? 1 : 0);
	return static_cast<int32>(FMath::Min<int64>(Result, MAX_int32));
}
