#pragma once

#include "CoreMinimal.h"
#include "GameXXKCardTypes.h"
#include "GameXXKRelicTypes.generated.h"

UENUM(BlueprintType)
enum class EGameXXKRelicTrigger : uint8
{
	BattleStart,
	PlayerRoundStart,
	PlayerRoundEnd,
	CardPlayed,
	DamageTaken,
	EnemyDefeated,
	RouteNodeCompleted
};

UENUM(BlueprintType)
enum class EGameXXKRelicEffectKind : uint8
{
	GainPartyArmor,
	GainHeroArmor,
	HealParty,
	RestorePartyMana,
	GainSharedEnergy,
	IncreasePartyAttack,
	IncreasePartyDefense,
	DamageAllEnemies,
	PoisonAllEnemies,
	BleedAllEnemies,
	DrawCards,
	RevealEnemyIntent,
	HealDamagedUnit,
	ArmorDamagedUnit,
	RestoreHeroMana,
	GainGold,
	HealPlayer,
	GainRouteMaxHealth,
	GainRouteMaxMana,
	GainRouteAttack,
	GainRouteDefense,
	GainRouteSpeed,
	GainRouteTravelMoney,
	EmergencyHealPartyPercent,
	/** High-tier effects with explicit per-turn ledgers, authored in the synergy catalogue. */
	Synergy
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKRelicDefinition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FName Id = NAME_None;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FText Description;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FSoftObjectPath IconTexturePath;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	EGameXXKCardQuality BaseQuality = EGameXXKCardQuality::Common;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	EGameXXKRelicTrigger Trigger = EGameXXKRelicTrigger::BattleStart;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	EGameXXKRelicEffectKind EffectKind = EGameXXKRelicEffectKind::GainPartyArmor;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 Magnitude = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 SecondaryMagnitude = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FName SynergyKey = NAME_None;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FText DetailedDescription;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	bool bStackable = false;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	bool bOfferEligible = true;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKRelicInstance
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName RelicId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 Stacks = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 AcquisitionOrdinal = 0;

	/** Serialized trigger ledger: resuming a battle must not grant another use in the same turn. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 SynergyRound = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 SynergyUses = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 LastSynergyActiveCardOrdinal = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TSet<FName> SynergyOwners;
};

/** Immutable primary-action evidence, persisted while a discard/search/replay choice is open. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKRelicActionEvidence
{
	GENERATED_BODY()
	UPROPERTY(SaveGame) bool bPending = false;
	UPROPERTY(SaveGame) TArray<FGameXXKCardCombatUnit> BeforeUnits;
	UPROPERTY(SaveGame) EGameXXKCardTerrain BeforeTerrain = EGameXXKCardTerrain::Invalid;
	UPROPERTY(SaveGame) EGameXXKCardTerrain AfterTerrain = EGameXXKCardTerrain::Invalid;
	UPROPERTY(SaveGame) FName PreviousOwner = NAME_None;
	UPROPERTY(SaveGame) int32 CountBefore = 0;
	UPROPERTY(SaveGame) int32 CountAfter = 0;
	UPROPERTY(SaveGame) FGameXXKCardPlayResult Primary;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKPendingRelicOffer
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 SourceNodeId = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 ChoiceSeed = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FName> RelicIds;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKRouteAttributeBonuses
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 MaxHealth = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 MaxMana = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 Attack = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 Defense = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 Speed = 0;
};
