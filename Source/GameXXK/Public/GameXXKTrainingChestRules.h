#pragma once

#include "CoreMinimal.h"
#include "GameXXKTrainingRules.h"
#include "GameXXKTrainingChestRules.generated.h"

struct FGameXXKRuntimeState;
enum class EGameXXKEquipmentQuality : uint8;

UENUM(BlueprintType)
enum class EGameXXKTrainingChestOpenError : uint8
{
	None,
	NoChest,
	BackpackFull,
	InvalidToken,
	LootInvalid,
	Overflow,
	PersistenceFailed
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKTrainingChestOpenReceipt
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) EGameXXKTrainingRewardTier Tier=EGameXXKTrainingRewardTier::NormalChest;
    UPROPERTY(BlueprintReadOnly) int32 OpenOrdinal=0;
    UPROPERTY(BlueprintReadOnly) FName SourceStageId;
    UPROPERTY(BlueprintReadOnly) FName ItemId;
    UPROPERTY(BlueprintReadOnly) FName EquipmentInstanceId;
    UPROPERTY(BlueprintReadOnly) FName EquipmentBaseId;
    UPROPERTY(BlueprintReadOnly) int32 Quantity=0;
    UPROPERTY(BlueprintReadOnly) int32 QualityRank=0;
    UPROPERTY(BlueprintReadOnly) int32 ItemLevel=0;
    UPROPERTY(BlueprintReadOnly) bool bSentToWarehouse=false;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKTrainingChestOpenResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly)
	EGameXXKTrainingChestOpenError Error = EGameXXKTrainingChestOpenError::None;

	UPROPERTY(BlueprintReadOnly)
	int32 OpenedCount = 0;

	UPROPERTY(BlueprintReadOnly)
	TArray<FName> EquipmentInstanceIds;

	UPROPERTY(BlueprintReadOnly)
	TMap<FName, int32> ItemDeltas;

	UPROPERTY(BlueprintReadOnly)
	FText Message;

    /** One immutable result per successfully committed box, in opening order. */
    UPROPERTY(BlueprintReadOnly)
    TArray<FGameXXKTrainingChestOpenReceipt> Receipts;
};

/** Deterministic chest loot: new entries enter Backpack; existing stored stacks retain their home. */
class GAMEXXK_API FGameXXKTrainingChestRules final
{
public:
	/** Exact draw domain for every chest roll, in basis points. Each approved column sums to this. */
	static constexpr int32 LootRollDomain = 10000;

	/**
	 * One exact draw from the approved equipment/gem quality table, in basis points
	 * [0,LootRollDomain-1]. The source difficulty only feeds the 宇宙 row, which is reachable
	 * exclusively from a Hell hunt chest.
	 */
	static EGameXXKEquipmentQuality ResolveLootQuality(EGameXXKTrainingRewardTier Tier, EGameXXKTrainingDifficulty SourceDifficulty, int32 Roll);
	static bool ResolveOrderDrop(EGameXXKTrainingRewardTier Tier, int32 Roll);
	static bool OpenOne(FGameXXKRuntimeState& InOutState, EGameXXKTrainingRewardTier Tier, FGameXXKTrainingChestOpenResult& OutResult);
	static bool OpenAll(FGameXXKRuntimeState& InOutState, EGameXXKTrainingRewardTier Tier, FGameXXKTrainingChestOpenResult& OutResult);
};
