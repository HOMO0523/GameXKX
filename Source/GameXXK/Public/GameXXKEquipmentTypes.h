#pragma once

#include "CoreMinimal.h"
#include "GameXXKEquipmentTypes.generated.h"

UENUM(BlueprintType)
enum class EGameXXKEquipmentSlot : uint8
{
	Invalid = 0 UMETA(Hidden),
	Weapon = 1,
	Head = 2,
	Armor = 3,
	Belt = 4,
	Shoes = 5,
	Accessory = 6
};

UENUM(BlueprintType)
enum class EGameXXKEquipmentSet : uint8
{
	Invalid = 0 UMETA(Hidden),
	Legacy = 1,
	Starter = 2,
	PoJun = 3,
	XuanJia = 4,
	QingNang = 5,
	ZhuiFeng = 6,
	ShiGu = 7,
	ShanHe = 8
};

UENUM(BlueprintType)
enum class EGameXXKEquipmentQuality : uint8
{
	Invalid = 0 UMETA(Hidden),
	Common = 1 UMETA(DisplayName = "普通"),
	Rare = 2 UMETA(DisplayName = "稀有"),
	Epic = 3 UMETA(DisplayName = "珍稀"),
	Legendary = 4 UMETA(DisplayName = "传奇"),
	Immortal = 5 UMETA(DisplayName = "不朽"),
	Treasure = 6 UMETA(DisplayName = "至宝"),
	Transcendent = 7 UMETA(DisplayName = "超凡"),
	Celestial = 8 UMETA(DisplayName = "天界"),
	Ascendant = 9 UMETA(DisplayName = "登神"),
	Cosmic = 10 UMETA(DisplayName = "宇宙")
};

UENUM(BlueprintType)
enum class EGameXXKAffixTier : uint8
{
	Invalid = 0 UMETA(Hidden),
	Common = 1 UMETA(DisplayName = "普通"),
	Rare = 2 UMETA(DisplayName = "稀有"),
	Epic = 3 UMETA(DisplayName = "珍稀"),
	Legendary = 4 UMETA(DisplayName = "传奇"),
	Immortal = 5 UMETA(DisplayName = "不朽"),
	Treasure = 6 UMETA(DisplayName = "至宝"),
	Transcendent = 7 UMETA(DisplayName = "超凡"),
	Celestial = 8 UMETA(DisplayName = "天界"),
	Ascendant = 9 UMETA(DisplayName = "登神"),
	Cosmic = 10 UMETA(DisplayName = "宇宙")
};

UENUM(BlueprintType)
enum class EGameXXKGemType : uint8
{
	Invalid = 0 UMETA(Hidden),
	Attack = 1 UMETA(DisplayName = "攻击"),
	Defense = 2 UMETA(DisplayName = "防御"),
	MaxHealth = 3 UMETA(DisplayName = "生命")
};

UENUM(BlueprintType)
enum class EGameXXKGemQuality : uint8
{
	Invalid = 0 UMETA(Hidden),
	Common = 1 UMETA(DisplayName = "普通"),
	Rare = 2 UMETA(DisplayName = "稀有"),
	Epic = 3 UMETA(DisplayName = "珍稀"),
	Legendary = 4 UMETA(DisplayName = "传奇"),
	Immortal = 5 UMETA(DisplayName = "不朽"),
	Treasure = 6 UMETA(DisplayName = "至宝"),
	Transcendent = 7 UMETA(DisplayName = "超凡"),
	Celestial = 8 UMETA(DisplayName = "天界"),
	Ascendant = 9 UMETA(DisplayName = "登神"),
	Cosmic = 10 UMETA(DisplayName = "宇宙")
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKSocketedGem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKGemType Type = EGameXXKGemType::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKGemQuality Quality = EGameXXKGemQuality::Invalid;

	bool IsEmpty() const
	{
		return Type == EGameXXKGemType::Invalid && Quality == EGameXXKGemQuality::Invalid;
	}
};

/** Single authority for serialized equipment-quality and affix-tier rank semantics. */
class GAMEXXK_API FGameXXKEquipmentQualityRules final
{
public:
	static constexpr int32 MinimumRank = 1;
	static constexpr int32 MaximumRank = 10;
	static constexpr int32 MaximumAffixCount = 5;

	static bool IsValid(EGameXXKEquipmentQuality Quality);
	static bool IsValid(EGameXXKAffixTier Tier);
	static int32 GetRank(EGameXXKEquipmentQuality Quality);
	static int32 GetRank(EGameXXKAffixTier Tier);
	static EGameXXKEquipmentQuality EquipmentQualityFromRank(int32 Rank);
	static EGameXXKAffixTier AffixTierFromRank(int32 Rank);
	static FText GetDisplayName(EGameXXKEquipmentQuality Quality);
	static FText GetDisplayName(EGameXXKAffixTier Tier);
	static EGameXXKEquipmentQuality GetNext(EGameXXKEquipmentQuality Quality);
	static EGameXXKAffixTier GetNext(EGameXXKAffixTier Tier);
	static int32 GetAffixCount(EGameXXKEquipmentQuality Quality);
};

UENUM(BlueprintType)
enum class EGameXXKEquipmentOwnerKind : uint8
{
	Invalid = 0 UMETA(Hidden),
	Warehouse = 1,
	Hero = 2,
	PermanentCompanion = 3,
	QuestNpc = 4
};

UENUM(BlueprintType)
enum class EGameXXKEquipmentScalingRule : uint8
{
	Invalid = 0 UMETA(Hidden),
	ModernPercentBase = 1,
	LegacyFlatPerEnhancement = 2
};

UENUM(BlueprintType)
enum class EGameXXKEquipmentMagnitudeUnit : uint8
{
	Invalid = 0 UMETA(Hidden),
	BasisPoints = 1,
	FlatCount = 2
};

/** Serialized modifier families: five universal stats followed by five families for each modern set. */
UENUM(BlueprintType)
enum class EGameXXKEquipmentModifierKind : uint8
{
	Invalid = 0 UMETA(Hidden),
	MaxHealth = 1,
	MaxMana = 2,
	Attack = 3,
	Defense = 4,
	Speed = 5,
	DirectDamage = 6,
	MultiHitDamage = 7,
	ArmorBreakStacks = 8,
	VulnerableTargetDamage = 9,
	FirstAttackDamage = 10,
	ArmorGain = 11,
	ArmorRetention = 12,
	CounterDamage = 13,
	GuardReduction = 14,
	LowHealthProtection = 15,
	Healing = 16,
	Cleanse = 17,
	OverhealConversion = 18,
	ManaRecovery = 19,
	EmergencyHealing = 20,
	Draw = 21,
	LowCostBonus = 22,
	SharedEnergy = 23,
	ComboCount = 24,
	TemporaryCostReduction = 25,
	Poison = 26,
	Bleed = 27,
	Burn = 28,
	DamageOverTime = 29,
	StatusRetention = 30,
	TerrainPower = 31,
	TerrainCostReduction = 32,
	AdjacentAllyPower = 33,
	FormationPower = 34,
	TeamTerrainPower = 35,
	BladeChargeDraw = 36,
	BladeStoredCharge = 37,
	BladeOpeningReplay = 38,
	QingNangCycle = 39,
	ShiGuCycle = 40,
	ZhuiFengCycle = 41
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKCharacterStats
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

	/** Naked/loadout projection inputs require real resource maxima and never accept negative combat stats. */
	bool IsValidProjectionInput() const
	{
		return MaxHealth > 0
			&& MaxMana > 0
			&& Attack >= 0
			&& Defense >= 0
			&& Speed >= 0;
	}
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKEquipmentAffixRoll
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName AffixId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKAffixTier Tier = EGameXXKAffixTier::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 Magnitude = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKEquipmentMagnitudeUnit Unit = EGameXXKEquipmentMagnitudeUnit::Invalid;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKEquipmentInstance
{
	GENERATED_BODY()

	FGameXXKEquipmentInstance()
	{
		SocketedGems.SetNum(1);
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName InstanceId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName BaseEquipmentId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 ItemLevel = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKEquipmentQuality Quality = EGameXXKEquipmentQuality::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 EnhancementLevel = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FGameXXKEquipmentAffixRoll> RolledAffixes;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FGameXXKSocketedGem> SocketedGems;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 AcquisitionSeed = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKEquipmentScalingRule ScalingRule = EGameXXKEquipmentScalingRule::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKCharacterStats LegacyBaseStatSnapshot;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKEquipmentOwnerKind OwnerKind = EGameXXKEquipmentOwnerKind::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName OwnerCharacterId = NAME_None;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKEquipmentLoadout
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName WeaponInstanceId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName HeadInstanceId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName ArmorInstanceId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName BeltInstanceId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName ShoesInstanceId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName AccessoryInstanceId = NAME_None;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKPendingEquipmentReforge
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bActive = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName InstanceId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 AffixIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKEquipmentAffixRoll OriginalAffix;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKEquipmentAffixRoll CandidateAffix;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 PaidRefinementSand = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 ConsumedReforgeOrdinal = INDEX_NONE;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKEquipmentCollectionState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FGameXXKEquipmentInstance> EquipmentInstances;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FName> WarehouseInstanceIds;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TMap<FName, FGameXXKEquipmentLoadout> CharacterLoadouts;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 RefinementSand = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 CollectionSeed = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 NextInstanceOrdinal = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 NextReforgeOrdinal = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 EquipmentSchemaVersion = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bLegacyWarehouseOverflow = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKPendingEquipmentReforge PendingReforge;
};
