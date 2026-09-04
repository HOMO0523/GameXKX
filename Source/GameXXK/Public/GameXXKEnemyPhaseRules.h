#pragma once

#include "CoreMinimal.h"
#include "GameXXKCardTypes.h"

/** Audit payload emitted when one damage packet consumes an enemy phase. */
struct GAMEXXK_API FGameXXKEnemyPhaseTransitionResult
{
	bool bTransitioned = false;
	FName EnemyUnitId = NAME_None;
	int32 PreviousPhase = 1;
	int32 NewPhase = 1;
	int32 Healing = 0;
	TArray<EGameXXKCardStatus> ClearedStatuses;
};

/** Packet boundary rules shared by direct damage, reactions, and damage-over-time. */
class GAMEXXK_API FGameXXKEnemyPhaseRules final
{
public:
	static int32 ClampHealthDamageForRemainingPhase(
		const FGameXXKCardBattleRuntime& Runtime,
		const FGameXXKCardCombatUnit& Target,
		int32 RequestedHealthDamage);

	static bool ResolveAfterDamagePacket(
		FGameXXKCardBattleRuntime& InOutRuntime,
		FName EnemyUnitId,
		FGameXXKEnemyPhaseTransitionResult& OutResult,
		FString* OutError = nullptr);
};
