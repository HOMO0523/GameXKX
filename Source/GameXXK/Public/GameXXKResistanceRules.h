#pragma once
#include "CoreMinimal.h"
#include "GameXXKCardTypes.h"

struct GAMEXXK_API FGameXXKResistanceProfile
{
	int32 FireBasisPoints = 0;
	int32 FrostBasisPoints = 0;
	int32 LightningBasisPoints = 0;
	int32 Get(EGameXXKCardDamageElement Element) const;
};

/** Innate profiles and final per-element mitigation; units retain their battle-entry snapshot. */
class GAMEXXK_API FGameXXKResistanceRules final
{
public:
	static constexpr int32 MaximumBasisPoints = 7500;
	static constexpr int32 MinimumBasisPoints = -2500;
	static bool IsSpell(EGameXXKCardDamageElement Element);
	static EGameXXKGemType GemForElement(EGameXXKCardDamageElement Element);
	static TArray<FName> GetProfileKeys();
	static FGameXXKResistanceProfile GetBaseProfile(FName CharacterId, EGameXXKCharacterRole Role, FName EnemyDefinitionId = NAME_None);
	static void InitializeUnitProfile(FGameXXKCardCombatUnit& Unit);
	static bool ValidateUnitProfile(const FGameXXKCardCombatUnit& Unit);
	static double ResolveBasisPoints(int32 InnateBasisPoints, int64 NominalBonusBasisPoints, int32 ReductionBasisPoints = 0);
	static double GetEffectiveBasisPoints(const FGameXXKCardCombatUnit& Unit, EGameXXKCardDamageElement Element);
	static int32 ApplyDamage(const FGameXXKCardCombatUnit& Target, EGameXXKCardDamageElement Element, int32 Damage);
};
