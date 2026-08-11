#pragma once

#include "CoreMinimal.h"
#include "GameXXKEquipmentTypes.h"

#include "GameXXKEquipmentSetCatalog.generated.h"

UENUM(BlueprintType)
enum class EGameXXKEquipmentSetBonusScope : uint8
{
	Invalid = 0 UMETA(Hidden),
	Owner = 1,
	Team = 2
};

UENUM(BlueprintType)
enum class EGameXXKEquipmentSetBonusHook : uint8
{
	Invalid = 0 UMETA(Hidden),
	Passive = 1,
	BattleStart = 2,
	RoundStart = 3,
	MultiHit = 4,
	FirstAttackPerRound = 5,
	FirstAllyHealthDamagePerRound = 6,
	CleanseOrOverheal = 7,
	FirstHealPerRound = 8,
	LowCostStreak = 9,
	ComboThreshold = 10,
	MultipleDamageOverTime = 11,
	RoundEnd = 12,
	TerrainSynergyCard = 13,
	PoJunChargeConsumed = 14,
	PoJunBladeFinish = 15,
	PoJunFirstActiveNextRound = 16,
	QingNangHighCostActive = 17,
	ShiGuDotApplied = 18,
	ShiGuDualDotEstablished = 19,
	ShiGuToxicExplosion = 20,
	ZhuiFengActiveCardCount = 21
};

UENUM(BlueprintType)
enum class EGameXXKEquipmentSetBonusKind : uint8
{
	Invalid = 0 UMETA(Hidden),
	PoJunDirectDamage = 1,
	PoJunMultiHitArmorBreak = 2,
	PoJunFirstAttackFollowUp = 3,
	XuanJiaArmorGain = 4,
	XuanJiaArmorRetentionCounter = 5,
	XuanJiaTeamGuard = 6,
	QingNangHealingCleanse = 7,
	QingNangCleanseOverheal = 8,
	QingNangTeamHealEnergy = 9,
	ZhuiFengSpeedOpeningDraw = 10,
	ZhuiFengLowCostEnergy = 11,
	ZhuiFengComboFreeCard = 12,
	ShiGuDamageOverTimeStacks = 13,
	ShiGuMixedDamageOverTime = 14,
	ShiGuExtraDamageOverTimeTick = 15,
	ShanHeTerrainPower = 16,
	ShanHeTerrainCardFormation = 17,
	ShanHeTeamFormationCore = 18,
	PoJunChargeDraw = 19,
	PoJunFinishStoresCharge = 20,
	PoJunOpeningFinishReplay = 21,
	QingNangHighCostDraw = 22,
	QingNangHighCostBloodCycle = 23,
	QingNangHighCostEnergyCycle = 24,
	ShiGuCardTargetRot = 25,
	ShiGuFirstDualDotExplosion = 26,
	ShiGuFirstExplosionPreservesDots = 27,
	ZhuiFengPairDraw = 28,
	ZhuiFengSecondCardEnergy = 29,
	ZhuiFengFourthCardCycle = 30
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKEquipmentSetBonusDefinition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FName Id = NAME_None;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FText Description;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	EGameXXKEquipmentSet Set = EGameXXKEquipmentSet::Invalid;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 RequiredPieces = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	EGameXXKEquipmentSetBonusKind BonusKind = EGameXXKEquipmentSetBonusKind::Invalid;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	EGameXXKEquipmentSetBonusScope Scope = EGameXXKEquipmentSetBonusScope::Invalid;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	EGameXXKEquipmentSetBonusHook Hook = EGameXXKEquipmentSetBonusHook::Invalid;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	EGameXXKEquipmentMagnitudeUnit Unit = EGameXXKEquipmentMagnitudeUnit::Invalid;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 Value = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 TriggersPerRound = 0;
};

class GAMEXXK_API FGameXXKEquipmentSetCatalog final
{
public:
	static const TArray<FGameXXKEquipmentSetBonusDefinition>& GetDefinitions();
	static const FGameXXKEquipmentSetBonusDefinition* FindDefinition(FName BonusId);

	/** Approved Chinese display name for a gear set (破军/玄甲/青囊/追风/蚀骨/山河). */
	static FText GetSetDisplayName(EGameXXKEquipmentSet Set);
};
