#pragma once

#include "CoreMinimal.h"
#include "GameXXKCardTypes.h"
#include "GameXXKCompanionTypes.h"
#include "GameXXKCardRunTypes.generated.h"

/** A deterministic, serializable enemy action prepared for the current enemy phase. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKCardEnemyIntent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName SourceUnitId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName SuggestedTargetUnitId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 Damage = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCardDamageKind Kind = EGameXXKCardDamageKind::SingleTargetAttack;
};

/** A pending post-battle reward. It stays stable until the player picks, replaces a route card, or skips. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKPendingRouteCardReward
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 SourceNodeId = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 ChoiceSeed = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FName> CardIds;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bRequiresRouteCardReplacement = false;
};

/** A pending non-combat route event that can optionally attach a task NPC for this route. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKPendingRouteEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 SourceNodeId = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 ChoiceSeed = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName EventNpcId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bCanRecruitPermanentCompanion = false;
};

/**
 * Route-local and permanent card-system state.  The legacy MVP battle arrays are a projection only;
 * this structure owns card zones, combat state, party configuration, deterministic offers, and seeds.
 */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKCardRunState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FName> HeroUnlockedCardIds;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FName> HeroSelectedCardIds;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKCompanionRosterState CompanionRoster;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKCompanionPartySelection PartySelection;

	/** Explicit route provenance prevents a stale saved NPC selection from joining an unrelated route. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName ActiveTemporaryQuestNpcId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bLoadoutLockedForRoute = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 RouteRandomSeed = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 NextRewardOrdinal = 0;

	/** Only temporary route rewards live here; permanent hero/partner/NPC configuration remains elsewhere. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FName> RouteCardIds;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bHasActiveCardBattle = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 ActiveBattleSourceNodeId = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKCardBattleRuntime ActiveBattle;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FGameXXKCardEnemyIntent> EnemyIntents;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 NextEnemyIntentIndex = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKPendingRouteCardReward PendingReward;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKPendingRouteEvent PendingEvent;
};
