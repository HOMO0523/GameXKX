#pragma once

#include "CoreMinimal.h"
#include "GameXXKTrainingRules.h"
#include "GameXXKTrainingChestRules.generated.h"

struct FGameXXKRuntimeState;

UENUM(BlueprintType)
enum class EGameXXKTrainingChestOpenError : uint8
{
	None,
	NoChest,
	BackpackFull,
	InvalidToken,
	LootInvalid,
	Overflow
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
};

/** Deterministic, save-authoritative chest opening into Backpack only. */
class GAMEXXK_API FGameXXKTrainingChestRules final
{
public:
	static bool OpenOne(FGameXXKRuntimeState& InOutState, EGameXXKTrainingRewardTier Tier, FGameXXKTrainingChestOpenResult& OutResult);
	static bool OpenAll(FGameXXKRuntimeState& InOutState, EGameXXKTrainingRewardTier Tier, FGameXXKTrainingChestOpenResult& OutResult);
};
