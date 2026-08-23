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
	EmergencyHealPartyPercent
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
