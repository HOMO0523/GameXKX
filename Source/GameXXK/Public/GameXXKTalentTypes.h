#pragma once

#include "CoreMinimal.h"

#include "GameXXKTalentTypes.generated.h"

UENUM(BlueprintType)
enum class EGameXXKTalentBranch : uint8
{
	None,
	Combat,
	CapacityChest,
	IdleOffline,
	Tools
};

UENUM(BlueprintType)
enum class EGameXXKTalentEffect : uint8
{
	None,
	UnlockWarehousePage,
	UnlockOfflineRewards,
	UnlockTools,
	CombatFoundation,
	FlatAttack,
	FlatMaxHP,
	FlatDefense,
	RouteAttackPercent,
	RouteFinalDamagePercent,
	RouteDefensePercent,
	RouteMaxHPPercent,
	CriticalChancePercent,
	CriticalDamagePercent,
	TravelMovementRank,
	BackpackSlots,
	OnlineGoldPercent,
	OnlineExperiencePercent,
	OfflineGoldPercent,
	OfflineExperiencePercent,
	OfflineGoldTimePercent,
	OfflineExperienceTimePercent,
	NormalChestDropPercent,
	AdvancedChestDropPercent,
	OfflineChestMinutes,
	ToolExperiencePercent,
	ToolGoldPercent
};

/** One reusable cell in the 4x4 talent icon atlas. */
UENUM(BlueprintType)
enum class EGameXXKTalentIcon : uint8
{
	Root,
	Attack,
	Health,
	Defense,
	Critical,
	Movement,
	Backpack,
	Warehouse,
	Gold,
	Experience,
	Offline,
	Time,
	Chest,
	ChestTime,
	Tools,
	ToolReward
};

UENUM(BlueprintType)
enum class EGameXXKTalentNodeState : uint8
{
	Hidden,
	Locked,
	Available,
	Purchased,
	Maxed
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKTalentNodeDefinition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FName Id = NAME_None;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FText Description;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	EGameXXKTalentBranch Branch = EGameXXKTalentBranch::None;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	EGameXXKTalentEffect Effect = EGameXXKTalentEffect::None;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	EGameXXKTalentIcon Icon = EGameXXKTalentIcon::Root;

	/** Integer stat points, percentage points, minutes, or movement ranks. */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 EffectPerRank = 0;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 MaxRank = 5;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 CostTier = 0;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 GraphLayer = 0;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FVector2D GraphPosition = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	TArray<FName> PrerequisiteIds;

	/** Sources used only to draw graph connections; progression remains governed by PrerequisiteIds. */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	TArray<FName> VisualConnectionIds;

	/** Used by the four warehouse milestone nodes. */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 RequiredBackpackCapacity = 0;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	bool bRoot = false;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	bool bBranchEntry = false;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	bool bMilestone = false;
};

/** Save-authoritative source of truth. Aggregate effects are always derived. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKTalentProgress
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TMap<FName, int32> NodeRanks;

	/** Compatibility floors preserve occupied cells in saves created before talents. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 MinimumBackpackCapacity = 20;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 MinimumWarehousePages = 1;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKTalentProjection
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere) int32 FlatAttack = 0;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere) int32 FlatMaxHP = 0;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere) int32 FlatDefense = 0;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere) int32 RouteAttackPercent = 0;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere) int32 RouteFinalDamagePercent = 0;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere) int32 RouteDefensePercent = 0;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere) int32 RouteMaxHPPercent = 0;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere) int32 CriticalChancePercent = 0;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere) int32 CriticalDamagePercent = 0;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere) int32 TravelMovementRank = 0;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere) int32 BackpackCapacity = 20;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere) int32 WarehousePageCount = 1;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere) bool bOfflineRewardsUnlocked = false;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere) bool bToolsUnlocked = false;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere) int32 OnlineGoldPercent = 0;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere) int32 OnlineExperiencePercent = 0;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere) int32 OfflineGoldPercent = 0;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere) int32 OfflineExperiencePercent = 0;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere) int32 OfflineGoldTimePercent = 0;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere) int32 OfflineExperienceTimePercent = 0;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere) int32 NormalChestDropPercent = 0;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere) int32 AdvancedChestDropPercent = 0;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere) int32 OfflineChestMinutes = 0;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere) int32 ToolExperiencePercent = 0;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere) int32 ToolGoldPercent = 0;

	float GetTravelWalkSeconds() const
	{
		return FMath::Clamp(5.0f - 0.5f * TravelMovementRank, 2.5f, 5.0f);
	}

	float GetOnlineGoldMultiplier() const { return 1.0f + OnlineGoldPercent / 100.0f; }
	float GetOnlineExperienceMultiplier() const { return 1.0f + OnlineExperiencePercent / 100.0f; }
	float GetOfflineGoldMultiplier() const { return 1.0f + OfflineGoldPercent / 100.0f; }
	float GetOfflineExperienceMultiplier() const { return 1.0f + OfflineExperiencePercent / 100.0f; }
	float GetNormalChestMultiplier() const { return 1.0f + NormalChestDropPercent / 100.0f; }
	float GetAdvancedChestMultiplier() const { return 1.0f + AdvancedChestDropPercent / 100.0f; }
	float GetToolExperienceMultiplier() const { return 1.0f + ToolExperiencePercent / 100.0f; }
	float GetToolGoldMultiplier() const { return 1.0f + ToolGoldPercent / 100.0f; }

	int32 GetOfflineGoldCapSeconds() const
	{
		return FMath::RoundToInt(24.0 * 60.0 * 60.0 * (1.0 + OfflineGoldTimePercent / 100.0));
	}

	int32 GetOfflineExperienceCapSeconds() const
	{
		return FMath::RoundToInt(24.0 * 60.0 * 60.0 * (1.0 + OfflineExperienceTimePercent / 100.0));
	}

	int32 GetOfflineChestCapSeconds() const
	{
		return 8 * 60 * 60 + OfflineChestMinutes * 60;
	}
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKTalentPurchaseResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere) bool bPurchased = false;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere) FName NodeId = NAME_None;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere) int64 Price = 0;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere) int32 RankAfter = 0;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere) FText Message;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKTalentNodeView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere) FGameXXKTalentNodeDefinition Definition;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere) EGameXXKTalentNodeState State = EGameXXKTalentNodeState::Hidden;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere) int32 Rank = 0;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere) int64 NextPrice = 0;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere) FText LockReason;
};
