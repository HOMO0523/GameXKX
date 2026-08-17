#pragma once

#include "CoreMinimal.h"
#include "GameXXKTrainingRules.generated.h"

/** The three deliberately linear difficulty bands used by the desktop Training map. */
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

/** Runtime-only phase for the low-cost repeating Travel loop. */
UENUM(BlueprintType)
enum class EGameXXKTrainingTravelPhase : uint8
{
	Idle,
	Walking,
	Combat,
	Defeated
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKTrainingReward
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

/**
 * Runtime read model for one Travel loop tick.  It is intentionally not a
 * SaveGame struct: the save stores the durable stage/encounter cursor, while
 * this presentation/combat snapshot is rebuilt after load.
 */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKTrainingTravelRuntime
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FName StageId = NAME_None;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 EncounterIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FName EnemyDefinitionId = NAME_None;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	EGameXXKTrainingEncounterKind EncounterKind = EGameXXKTrainingEncounterKind::Normal;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	EGameXXKTrainingTravelPhase Phase = EGameXXKTrainingTravelPhase::Idle;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 WalkStep = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 WalkStepsRequired = 2;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 PlayerHP = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 PlayerMaxHP = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 PlayerAttack = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 EnemyHP = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 EnemyMaxHP = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 EnemyAttack = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 LastDamageToEnemy = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 LastDamageToPlayer = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	bool bAutoBattle = true;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKTrainingEncounterDefinition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FName EnemyDefinitionId = NAME_None;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	EGameXXKTrainingEncounterKind Kind = EGameXXKTrainingEncounterKind::Normal;

	/** Runtime encounter health.  The cleared Normal 1-1 travel exception intentionally uses 1. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 BaseHealth = 1;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKTrainingStageDefinition
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

	/** Ordinary encounter candidates; a single entry is selected for each ordinary encounter. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	TArray<FName> NormalEnemyPool;

	/** The two chapter sub-elites, exposed separately for route tooltip and challenge composition. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	TArray<FName> EliteEnemyPool;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FName BossEnemyId = NAME_None;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FText BossDisplayName;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 TravelGold = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 TravelExperience = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	bool bOneHealthTravelException = false;

	/** Configurable placeholders until the design spreadsheet supplies final probability values. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	float NormalChestChance = 0.25f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	float AdvancedChestChance = 0.35f;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKTrainingProgress
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TSet<FName> ClearedStageIds;

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

	/** The current encounter within the repeating travel loop. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 ActiveTravelEncounterIndex = INDEX_NONE;
};

/** Pure, deterministic rules for Training.  UI and save code call this API instead of mutating flags ad hoc. */
class GAMEXXK_API FGameXXKTrainingRules final
{
public:
	static constexpr int32 StagesPerDifficulty = 9;

	static FName DifficultyId(EGameXXKTrainingDifficulty Difficulty);
	static FName MakeStageId(EGameXXKTrainingDifficulty Difficulty, int32 StageNumber);
	static EGameXXKTrainingDifficulty DifficultyFromStageId(FName StageId);

	static const TArray<FGameXXKTrainingStageDefinition>& GetStageDefinitions();
	static bool TryGetStageDefinition(FName StageId, FGameXXKTrainingStageDefinition& OutDefinition);
	static TArray<FGameXXKTrainingEncounterDefinition> BuildEncounterSequence(FName StageId, bool bTravelMode = false);

	static void InitializeNewGame(FGameXXKTrainingProgress& Progress);
	static bool IsDifficultyUnlocked(const FGameXXKTrainingProgress& Progress, EGameXXKTrainingDifficulty Difficulty);
	static bool IsStageCleared(const FGameXXKTrainingProgress& Progress, FName StageId);
	static bool AreAllStagesCleared(const FGameXXKTrainingProgress& Progress);
	static bool CanChallenge(const FGameXXKTrainingProgress& Progress, FName StageId);
	static bool CanTravel(const FGameXXKTrainingProgress& Progress, FName StageId);
	static bool StartChallenge(FGameXXKTrainingProgress& Progress, FName StageId);
	static bool CompleteChallenge(FGameXXKTrainingProgress& Progress, FName StageId);
	static bool StartTravel(FGameXXKTrainingProgress& Progress, FName StageId);
	static bool InitializeTravelRunner(
		const FGameXXKTrainingProgress& Progress,
		FGameXXKTrainingTravelRuntime& OutRuntime,
		int32 PlayerHP,
		int32 PlayerMaxHP,
		int32 PlayerAttack);
	static bool AdvanceTravelRunner(
		FGameXXKTrainingProgress& Progress,
		FGameXXKTrainingTravelRuntime& InOutRuntime,
		bool& bOutEncounterCompleted,
		bool& bOutStageCompleted,
		bool& bOutDefeated,
		FGameXXKTrainingReward& OutReward);
	static bool AdvanceTravelEncounter(
		FGameXXKTrainingProgress& Progress,
		bool& bOutStageCompleted,
		FGameXXKTrainingReward& OutReward);
	static bool ResolveTravelFailure(FGameXXKTrainingProgress& Progress);

	static FGameXXKTrainingReward BuildTravelReward(FName StageId);
	static FGameXXKTrainingReward BuildChallengeReward(
		FName StageId,
		EGameXXKTrainingEncounterKind EncounterKind,
		bool bChestRolled,
		float TalentChestDropBonus = 0.0f);
	static FText BuildStageTooltip(const FGameXXKTrainingProgress& Progress, FName StageId);
};
