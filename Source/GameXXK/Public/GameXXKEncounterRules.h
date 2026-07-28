#pragma once

#include "CoreMinimal.h"

#include "GameXXKEnemyCatalog.h"
#include "GameXXKMVPRules.h"

/**
 * One explicit enemy placement in the three-slot battle presentation contract.
 * BattleSlotNumber is always one of 1P, 2P, or 3P; no caller may infer it from an array index.
 */
struct GAMEXXK_API FGameXXKEncounterSlot
{
	FName EnemyDefinitionId = NAME_None;
	int32 BattleSlotNumber = INDEX_NONE;
	int32 CombatLevel = 1;
};

/**
 * Authored route difficulty multiplier, keyed by chapter and route node kind.
 * The scale is applied to every enemy occupying that encounter; it does not alter catalog data.
 */
struct GAMEXXK_API FGameXXKEncounterStatScale
{
	int32 MaxHPPercent = 100;
	int32 AttackPercent = 100;
	int32 DefensePercent = 100;
};

/** Deterministic route encounter selection.  All random streams are local to BuildFormation. */
class GAMEXXK_API FGameXXKEncounterRules final
{
public:
	static int32 DeriveChapterSeed(int32 RootSeed, int32 Chapter);
	static int32 GetCombatLevel(EGameXXKEnemyTier Tier, int32 RouteCombatLevel);
	static FGameXXKEncounterStatScale GetAuthoredStatScale(int32 Chapter, EGameXXKNodeKind NodeKind);
	static int32 ScaleStat(int32 Value, int32 Percent, int32 Minimum);
	static bool BuildFormation(
		int32 Chapter,
		EGameXXKNodeKind NodeKind,
		int32 ChapterSeed,
		int32 NodeId,
		int32 RouteCombatLevel,
		TArray<FGameXXKEncounterSlot>& OutSlots,
		FString* OutError = nullptr);
};
