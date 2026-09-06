#pragma once

#include "CoreMinimal.h"
#include "GameXXKTrainingSettlementTypes.generated.h"

/** Effective health loss/healing and generated armor during the final Boss battle. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKBattleSessionStats
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) bool bComplete = true;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 Rounds = 0;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 ActiveCardsPlayed = 0;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int64 PartyDamageDealt = 0;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int64 PartyDamageTaken = 0;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int64 HealingDone = 0;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int64 ArmorGenerated = 0;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 SurvivingPartyUnits = 0;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int64 PartyEndingHealth = 0;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int64 PartyEndingMaxHealth = 0;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKTrainingSettlementMember
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FName MemberId = NAME_None;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FText DisplayName;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 LevelBefore = 1;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 LevelAfter = 1;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 ExperienceBefore = 0;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 ExperienceAfter = 0;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 ExperienceGained = 0;
};

/** Already-applied clear rewards retained independently from route-local cleanup. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKTrainingSettlementReceipt
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FGuid ReceiptId;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FName StageId = NAME_None;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FText StageDisplayName;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 Gold = 0;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 RouteGold = 0;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 SourceTravelMoney = 0;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 Experience = 0;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 NormalChestCount = 0;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 AdvancedChestCount = 0;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 ChestItemLevel = 0;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FName GrantedEquipmentInstanceId = NAME_None;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) bool bFirstClear = false;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FName UnlockedStageId = NAME_None;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) bool bUnlockedNextDifficulty = false;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) TArray<FGameXXKTrainingSettlementMember> Members;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FGameXXKBattleSessionStats Stats;
};
