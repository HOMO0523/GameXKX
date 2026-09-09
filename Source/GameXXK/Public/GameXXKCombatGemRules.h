#pragma once

#include "CoreMinimal.h"
#include "GameXXKCardTypes.h"

/** One authoritative gem settlement shared by damage packets, status reservoirs and UI. */
class GAMEXXK_API FGameXXKCombatGemRules final
{
public:
	static int64 GetRawBonus(const FGameXXKCardCombatUnit* Source, EGameXXKGemType Type);
	static bool IsCardBodyOrigin(EGameXXKCardResolutionOrigin Origin);
	static EGameXXKCardDamageElement GetCardElement(const FGameXXKCardDefinition& Definition);
	static FString GetElementLabel(EGameXXKCardDamageElement Element);
	static int64 GetDirectPool(const FGameXXKCardCombatUnit* Source, const FGameXXKCardDamageContext& Context);
	static int32 ApplyHealing(const FGameXXKCardCombatUnit& Source, int32 Amount, EGameXXKCardResolutionOrigin Origin);
	static bool TracksSource(EGameXXKCardStatus Status);
	static void AddSource(FGameXXKCardStatusStack& Stack, FName Source, int32 Added);
	/** Proportional, deterministic retention; call before changing Stack.Stacks. */
	static void RetainSources(FGameXXKCardStatusStack& Stack, int32 Remaining);
	static bool ValidateSources(const FGameXXKCardStatusStack& Stack);
	static EGameXXKCardStatus StatusForCause(EGameXXKCardDamageCause Cause);
	static int64 GetDotPool(const FGameXXKCardCombatUnit* Source, EGameXXKCardStatus Status);
	static int32 NaturalDot(const TArray<FGameXXKCardCombatUnit>& Units, const FGameXXKCardCombatUnit& Target,
		EGameXXKCardStatus Status, int32 BaseStacks);
	static int32 TriggeredDot(const TArray<FGameXXKCardCombatUnit>& Units, FName Source,
		EGameXXKCardStatus Status, int32 BaseStacks);
};
