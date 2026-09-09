#pragma once

#include "CoreMinimal.h"
#include "GameXXKTrainingRules.generated.h"

UENUM(BlueprintType)
enum class EGameXXKTrainingDifficulty : uint8
{
	Normal,
	Hard,
	Hell
};

UENUM(BlueprintType)
enum class EGameXXKTrainingEncounterKind : uint8
{
	Normal,
	Elite,
	Boss
};

UENUM(BlueprintType)
enum class EGameXXKTrainingRewardTier : uint8
{
	None,
	NormalChest,
	AdvancedChest
};

USTRUCT(BlueprintType)
struct FGameXXKTrainingReward
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 Gold = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 Experience = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	EGameXXKTrainingRewardTier ChestTier = EGameXXKTrainingRewardTier::None;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	bool bChestRolled = false;
};

USTRUCT(BlueprintType)
struct FGameXXKTrainingEncounterDefinition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FName EnemyDefinitionId = NAME_None;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	EGameXXKTrainingEncounterKind Kind = EGameXXKTrainingEncounterKind::Normal;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 BaseHealth = 1;
};

USTRUCT(BlueprintType)
struct FGameXXKTrainingStageDefinition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FName StageId = NAME_None;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	EGameXXKTrainingDifficulty Difficulty = EGameXXKTrainingDifficulty::Normal;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 Chapter = 1;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 StageNumber = 1;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	TArray<FName> NormalEnemyPool;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	TArray<FName> EliteEnemyPool;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FName BossEnemyId = NAME_None;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FText BossDisplayName;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 TravelGold = 12;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 TravelExperience = 8;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	bool bOneHealthTravelException = false;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	float NormalChestChance = 0.25f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	float AdvancedChestChance = 0.50f;
};

USTRUCT(BlueprintType)
struct FGameXXKTrainingProgress
{
	GENERATED_BODY()

	/** Cleared IDs use stable names such as Training.Normal.1-1. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TSet<FName> ClearedStageIds;

	/** Unlocked IDs are Normal, Hard, and Hell. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TSet<FName> UnlockedDifficultyIds;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName SelectedStageId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName CurrentTravelStageId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bTravelActive = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bRetryOnFailure = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bChallengeActive = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName ActiveChallengeStageId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 ActiveChallengeEncounterIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bChallengeAutoBattle = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 TravelVictories = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 TravelFailures = 0;
};

/** Pure, data-driven rules for the desktop Training/历练 workbench. */
class GAMEXXK_API FGameXXKTrainingRules final
{
public:
	static constexpr int32 StagesPerDifficulty = 9;

	static FName DifficultyId(EGameXXKTrainingDifficulty Difficulty);
	static FName MakeStageId(EGameXXKTrainingDifficulty Difficulty, int32 StageNumber);
	static EGameXXKTrainingDifficulty DifficultyFromStageId(FName StageId);
	static TArray<FGameXXKTrainingStageDefinition> GetStageDefinitions();
	static bool TryGetStageDefinition(FName StageId, FGameXXKTrainingStageDefinition& OutDefinition);
	static TArray<FGameXXKTrainingEncounterDefinition> BuildEncounterSequence(FName StageId);
	static void InitializeNewGame(FGameXXKTrainingProgress& Progress);
	static bool IsDifficultyUnlocked(const FGameXXKTrainingProgress& Progress, EGameXXKTrainingDifficulty Difficulty);
	static bool IsStageCleared(const FGameXXKTrainingProgress& Progress, FName StageId);
	static bool CanChallenge(const FGameXXKTrainingProgress& Progress, FName StageId, FString* OutError = nullptr);
	static bool CanTravel(const FGameXXKTrainingProgress& Progress, FName StageId, FString* OutError = nullptr);
	static bool StartChallenge(FGameXXKTrainingProgress& Progress, FName StageId, FString* OutError = nullptr);
	static bool CompleteChallenge(FGameXXKTrainingProgress& Progress, FName StageId, FString* OutError = nullptr);
	static bool StartTravel(FGameXXKTrainingProgress& Progress, FName StageId, FString* OutError = nullptr);
	static bool ResolveTravelFailure(FGameXXKTrainingProgress& Progress);
	static FGameXXKTrainingReward BuildTravelReward(FName StageId);
	static FGameXXKTrainingReward BuildChallengeReward(FName StageId, EGameXXKTrainingEncounterKind EncounterKind, bool bChestRolled, float TalentChestBonus = 0.0f);
	static FText BuildStageTooltip(const FGameXXKTrainingProgress& Progress, FName StageId);

private:
	static bool AreAllStagesCleared(const FGameXXKTrainingProgress& Progress, EGameXXKTrainingDifficulty Difficulty);
	static void SetError(FString* OutError, const TCHAR* Message);
};
