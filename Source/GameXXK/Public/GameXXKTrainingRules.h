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

	/** Canonical Inventory item mirror for a successfully rolled chest. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FName ChestItemId = NAME_None;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	bool bChestRolled = false;
};

/** Aggregated deterministic result for a closed-window Travel simulation. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKTrainingOfflineReward
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 Gold = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 Experience = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 NormalChestCount = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 AdvancedChestCount = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 CompletedEncounters = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 CompletedStages = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 SimulatedSeconds = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	bool bStoppedAtDefeat = false;
};

/**
 * Runtime read model for one Travel loop tick.  It is intentionally not a
 * SaveGame struct: the save stores the durable stage/encounter cursor, while
 * this presentation/combat snapshot is rebuilt after load.
 */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKTrainingTravelEnemyRuntime
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FName EnemyDefinitionId = NAME_None;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 HP = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 MaxHP = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 Attack = 0;
};

/** One authoritative member of the fixed hero + companion + NPC Travel party. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKTrainingTravelPartyUnitRuntime
{
	GENERATED_BODY()

	FGameXXKTrainingTravelPartyUnitRuntime() = default;

	FGameXXKTrainingTravelPartyUnitRuntime(
		const FName InUnitId,
		const int32 InHP,
		const int32 InMaxHP,
		const int32 InAttack)
		: UnitId(InUnitId)
		, HP(InHP)
		, MaxHP(InMaxHP)
		, Attack(InAttack)
	{
	}

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FName UnitId = NAME_None;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 HP = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 MaxHP = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 Attack = 0;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKTrainingTravelRuntime
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FName StageId = NAME_None;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 EncounterIndex = INDEX_NONE;

	/** All authored enemies in the current wave, in the same left-to-right order used by challenge combat. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	TArray<FGameXXKTrainingTravelEnemyRuntime> Enemies;

	/** The living enemy currently exchanging attacks with the lightweight Travel runner. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 ActiveEnemyIndex = INDEX_NONE;

	/** Compatibility mirror of Enemies[ActiveEnemyIndex].EnemyDefinitionId. */
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

	/** Hero is always slot zero; the active permanent companion and NPC occupy slots one and two. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	TArray<FGameXXKTrainingTravelPartyUnitRuntime> PartyUnits;

	/** Next living party member that will perform an automatic attack. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 ActivePartyIndex = INDEX_NONE;

	/** Round-robin party target used by the next enemy retaliation. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 NextEnemyTargetPartyIndex = 0;

	/** Immutable mutation metadata consumed by the presentation queue. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 LastAttackingPartyIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 LastDamagedPartyIndex = INDEX_NONE;

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

	/** Shared left-to-right wave formation used by both challenge and Travel. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	TArray<FName> EnemyDefinitionIds;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	EGameXXKTrainingEncounterKind Kind = EGameXXKTrainingEncounterKind::Normal;

	/** Shared encounter health used by both active challenge and Travel presentation. */
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

	/** Logical seconds remaining before the next Travel normal chest may roll. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 TravelNormalChestCooldownRemainingSeconds = 0;

	/** Logical seconds remaining before the next Travel advanced chest may roll. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 TravelAdvancedChestCooldownRemainingSeconds = 0;

	/** Persisted deterministic sequence used by both challenge and Travel settlement. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 ChallengeRewardSeed = 0;

	/** Durable reward ledger produced while the desktop window was closed. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 PendingTravelGold = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 PendingTravelExperience = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 PendingTravelNormalChestCount = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 PendingTravelAdvancedChestCount = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 PendingTravelCompletedEncounters = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 PendingTravelCompletedStages = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 PendingTravelSimulatedSeconds = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bTravelPausedAtDefeat = false;

	/** UTC Unix seconds at the last persisted Travel update; zero means no offline baseline yet. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int64 TravelLastUpdatedUnixSeconds = 0;
};

/** Pure, deterministic rules for Training.  UI and save code call this API instead of mutating flags ad hoc. */
class GAMEXXK_API FGameXXKTrainingRules final
{
public:
	static constexpr int32 StagesPerDifficulty = 9;
	/** Travel chest cooldowns are durable gameplay constants, expressed in logical seconds. */
	static constexpr int32 TravelNormalChestCooldownSeconds = 4 * 60;
	static constexpr int32 TravelAdvancedChestCooldownSeconds = 6 * 60;
	/** Closed-window simulation is intentionally bounded to one day per collect. */
	static constexpr int32 MaxTravelOfflineSimulationSeconds = 24 * 60 * 60;

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
	static bool InitializeTravelRunner(
		const FGameXXKTrainingProgress& Progress,
		FGameXXKTrainingTravelRuntime& OutRuntime,
		const TArray<FGameXXKTrainingTravelPartyUnitRuntime>& PartyUnits);
	static bool AdvanceTravelRunner(
		FGameXXKTrainingProgress& Progress,
		FGameXXKTrainingTravelRuntime& InOutRuntime,
		bool& bOutEncounterCompleted,
		bool& bOutStageCompleted,
		bool& bOutDefeated,
		FGameXXKTrainingReward& OutReward,
		int32 ElapsedSeconds = 1);
	static bool AdvanceTravelEncounter(
		FGameXXKTrainingProgress& Progress,
		bool& bOutStageCompleted,
		FGameXXKTrainingReward& OutReward);
	static bool AdvanceTravelOffline(
		FGameXXKTrainingProgress& Progress,
		FGameXXKTrainingTravelRuntime& InOutRuntime,
		int32 ElapsedSeconds,
		FGameXXKTrainingOfflineReward& OutReward);
	static bool AccumulatePendingTravelReward(
		FGameXXKTrainingProgress& Progress,
		const FGameXXKTrainingOfflineReward& Reward);
	static bool GetPendingTravelReward(
		const FGameXXKTrainingProgress& Progress,
		FGameXXKTrainingOfflineReward& OutReward);
	static bool ConsumePendingTravelReward(
		FGameXXKTrainingProgress& Progress,
		FGameXXKTrainingOfflineReward& OutReward);
	static bool ResolveTravelFailure(FGameXXKTrainingProgress& Progress);

	static FGameXXKTrainingReward BuildTravelReward(FName StageId);
	static FGameXXKTrainingReward BuildChallengeReward(
		FName StageId,
		EGameXXKTrainingEncounterKind EncounterKind,
		bool bChestRolled,
		float TalentChestDropBonus = 0.0f);
	/**
	 * Resolves a seeded active-challenge reward.  The caller owns the persisted
	 * seed and advances it once the encounter settlement is committed.
	 */
	static FGameXXKTrainingReward ResolveChallengeReward(
		FName StageId,
		EGameXXKTrainingEncounterKind EncounterKind,
		int32 RewardSeed,
		float TalentChestDropBonus = 0.0f);
	/**
	 * Resolves a seeded Travel reward using the same configured chance as an
	 * active challenge.  A ready cooldown is required before a chest can be
	 * granted; cooldown state is owned by FGameXXKTrainingProgress and is
	 * advanced by AdvanceTravelChestCooldown.
	 */
	static FGameXXKTrainingReward ResolveTravelReward(
		FName StageId,
		EGameXXKTrainingEncounterKind EncounterKind,
		int32 RewardSeed,
		int32 NormalChestCooldownRemainingSeconds,
		int32 AdvancedChestCooldownRemainingSeconds,
		float TalentChestDropBonus = 0.0f,
		bool bIncludeStageReward = false);
	static int32 DefaultChallengeRewardSeed();
	static int32 NextChallengeRewardSeed(int32 RewardSeed);
	static int32 AdvanceTravelChestCooldown(int32 RemainingSeconds, int32 ElapsedSeconds);
	static int32 TravelChestCooldownSeconds(EGameXXKTrainingRewardTier ChestTier);
	static FName ChestItemIdForTier(EGameXXKTrainingRewardTier ChestTier);
	static FText BuildStageTooltip(const FGameXXKTrainingProgress& Progress, FName StageId);
};
