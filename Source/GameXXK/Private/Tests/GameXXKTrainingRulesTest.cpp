#include "GameXXKTrainingRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKSaveMigration.h"
#include "MVP/GameXXKSaveGame.h"

#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTrainingNewGameTest,
	"GameXXK.Training.NewGameDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTrainingNewGameTest::RunTest(const FString& Parameters)
{
	FGameXXKTrainingProgress Progress;
	FGameXXKTrainingRules::InitializeNewGame(Progress);
	const FName StageOne = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1);
	TestTrue(TEXT("normal difficulty is unlocked"), FGameXXKTrainingRules::IsDifficultyUnlocked(Progress, EGameXXKTrainingDifficulty::Normal));
	TestTrue(TEXT("normal 1-1 starts cleared"), FGameXXKTrainingRules::IsStageCleared(Progress, StageOne));
	TestTrue(TEXT("normal 1-1 starts as current travel"), Progress.CurrentTravelStageId == StageOne);
	TestTrue(TEXT("cleared 1-1 can travel"), FGameXXKTrainingRules::CanTravel(Progress, StageOne));
	TestFalse(TEXT("cleared 1-1 is not a new challenge"), FGameXXKTrainingRules::CanChallenge(Progress, StageOne));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTrainingDifficultyUnlockTest,
	"GameXXK.Training.DifficultyUnlocks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTrainingDifficultyUnlockTest::RunTest(const FString& Parameters)
{
	FGameXXKTrainingProgress Progress;
	FGameXXKTrainingRules::InitializeNewGame(Progress);
	for (int32 StageNumber = 2; StageNumber <= 9; ++StageNumber)
	{
		const FName StageId = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, StageNumber);
		TestTrue(TEXT("next normal stage is challengeable"), FGameXXKTrainingRules::CanChallenge(Progress, StageId));
		TestTrue(TEXT("challenge starts"), FGameXXKTrainingRules::StartChallenge(Progress, StageId));
		TestTrue(TEXT("challenge completes"), FGameXXKTrainingRules::CompleteChallenge(Progress, StageId));
	}
	TestTrue(TEXT("hard unlocks after nine normal clears"), FGameXXKTrainingRules::IsDifficultyUnlocked(Progress, EGameXXKTrainingDifficulty::Hard));
	const FName HardOne = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Hard, 1);
	TestTrue(TEXT("hard 1-1 is challengeable after normal completion"), FGameXXKTrainingRules::CanChallenge(Progress, HardOne));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTrainingFailurePolicyTest,
	"GameXXK.Training.FailureRetryPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTrainingFailurePolicyTest::RunTest(const FString& Parameters)
{
	FGameXXKTrainingProgress Progress;
	FGameXXKTrainingRules::InitializeNewGame(Progress);
	const FName StageTwo = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 2);
	Progress.ClearedStageIds.Add(StageTwo);
	Progress.CurrentTravelStageId = StageTwo;
	Progress.SelectedStageId = StageTwo;
	Progress.bTravelActive = true;
	Progress.bRetryOnFailure = false;
	TestTrue(TEXT("failure resolves"), FGameXXKTrainingRules::ResolveTravelFailure(Progress));
	TestEqual(TEXT("failure-off returns to the previous stage"), Progress.CurrentTravelStageId, FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1));
	Progress.CurrentTravelStageId = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1);
	Progress.bRetryOnFailure = false;
	FGameXXKTrainingRules::ResolveTravelFailure(Progress);
	TestEqual(TEXT("normal 1-1 failure remains 1-1"), Progress.CurrentTravelStageId, FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTrainingChapterOneCompositionTest,
	"GameXXK.Training.ChapterOneComposition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTrainingChapterOneCompositionTest::RunTest(const FString& Parameters)
{
	FGameXXKTrainingProgress Progress;
	FGameXXKTrainingRules::InitializeNewGame(Progress);
	const FName StageOne = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1);
	FGameXXKTrainingStageDefinition Definition;
	TestTrue(TEXT("1-1 definition exists"), FGameXXKTrainingRules::TryGetStageDefinition(StageOne, Definition));
	TestEqual(TEXT("1-1 ordinary pool contains rooster and civet only"), Definition.NormalEnemyPool.Num(), 2);
	TestTrue(TEXT("1-1 ordinary pool contains rooster"), Definition.NormalEnemyPool.Contains(FName(TEXT("Enemy.Ch1.Rooster"))));
	TestTrue(TEXT("1-1 ordinary pool contains civet"), Definition.NormalEnemyPool.Contains(FName(TEXT("Enemy.Ch1.Civet"))));
	TestEqual(TEXT("1-1 has two sub-elite entries"), Definition.EliteEnemyPool.Num(), 2);
	TestTrue(TEXT("1-1 elite pool contains goat"), Definition.EliteEnemyPool.Contains(FName(TEXT("Enemy.Ch1.Goat"))));
	TestTrue(TEXT("1-1 elite pool contains weasel"), Definition.EliteEnemyPool.Contains(FName(TEXT("Enemy.Ch1.Weasel"))));
	TestTrue(TEXT("1-1 travel uses the one-health exception"), Definition.bOneHealthTravelException);
	const TArray<FGameXXKTrainingEncounterDefinition> Encounters = FGameXXKTrainingRules::BuildEncounterSequence(StageOne, false);
	const TArray<FGameXXKTrainingEncounterDefinition> TravelEncounters = FGameXXKTrainingRules::BuildEncounterSequence(StageOne, true);
	TestTrue(TEXT("encounter sequence is non-empty"), Encounters.Num() >= 7);
	int32 EliteCount = 0;
	for (const FGameXXKTrainingEncounterDefinition& Encounter : Encounters)
	{
		if (Encounter.Kind == EGameXXKTrainingEncounterKind::Elite) ++EliteCount;
	}
	TestEqual(TEXT("route exposes two sub-elite encounters"), EliteCount, 2);
	TestTrue(TEXT("route ends with a boss encounter"), Encounters.Last().Kind == EGameXXKTrainingEncounterKind::Boss);
	TestTrue(TEXT("challenge 1-1 keeps normal combat health"), Encounters[0].BaseHealth > 1);
	TestTrue(TEXT("travel 1-1 uses one health for normal encounters"), TravelEncounters[0].BaseHealth == 1);
	TestTrue(TEXT("travel 1-1 uses one health for the boss"), TravelEncounters.Last().BaseHealth == 1);
	TestEqual(TEXT("1-1 boss tooltip identity is goat boss"), Definition.BossEnemyId, FName(TEXT("Enemy.Ch1.Goat")));
	const FName StageTwo = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 2);
	const FName StageThree = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 3);
	FGameXXKTrainingStageDefinition StageTwoDefinition;
	FGameXXKTrainingStageDefinition StageThreeDefinition;
	TestTrue(TEXT("1-2 definition exists"), FGameXXKTrainingRules::TryGetStageDefinition(StageTwo, StageTwoDefinition));
	TestTrue(TEXT("1-3 definition exists"), FGameXXKTrainingRules::TryGetStageDefinition(StageThree, StageThreeDefinition));
	TestEqual(TEXT("1-2 reuses the weasel as the boss"), StageTwoDefinition.BossEnemyId, FName(TEXT("Enemy.Ch1.Weasel")));
	TestEqual(TEXT("1-3 uses the first-chapter boss identity"), StageThreeDefinition.BossEnemyId, FName(TEXT("Enemy.Ch1.BluehornGoatKing")));
	const FString Tooltip = FGameXXKTrainingRules::BuildStageTooltip(Progress, StageOne).ToString();
	TestTrue(TEXT("boss tooltip includes the authored boss name"), Tooltip.Contains(Definition.BossDisplayName.ToString()));
	TestEqual(TEXT("base travel reward leaves chest resolution to the encounter resolver"), FGameXXKTrainingRules::BuildTravelReward(StageOne).ChestTier, EGameXXKTrainingRewardTier::None);
	const FGameXXKTrainingReward BossReward = FGameXXKTrainingRules::BuildChallengeReward(StageOne, EGameXXKTrainingEncounterKind::Boss, true);
	TestEqual(TEXT("boss reward uses advanced chest tier"), BossReward.ChestTier, EGameXXKTrainingRewardTier::AdvancedChest);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTrainingRewardResolverTest,
	"GameXXK.Training.RewardResolver",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTrainingRewardResolverTest::RunTest(const FString& Parameters)
{
	const FName StageOne = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1);
	const int32 Seed = 0x13579BDF;
	const FGameXXKTrainingReward First = FGameXXKTrainingRules::ResolveChallengeReward(
		StageOne,
		EGameXXKTrainingEncounterKind::Elite,
		Seed,
		0.0f);
	const FGameXXKTrainingReward Repeat = FGameXXKTrainingRules::ResolveChallengeReward(
		StageOne,
		EGameXXKTrainingEncounterKind::Elite,
		Seed,
		0.0f);
	TestEqual(TEXT("the same reward seed produces the same gold"), Repeat.Gold, First.Gold);
	TestEqual(TEXT("the same reward seed produces the same experience"), Repeat.Experience, First.Experience);
	TestEqual(TEXT("the same reward seed produces the same chest roll"), Repeat.bChestRolled, First.bChestRolled);
	TestEqual(TEXT("the same reward seed produces the same chest tier"), Repeat.ChestTier, First.ChestTier);

	const FGameXXKTrainingReward NoChest = FGameXXKTrainingRules::ResolveChallengeReward(
		StageOne,
		EGameXXKTrainingEncounterKind::Normal,
		Seed,
		-1.0f);
	TestFalse(TEXT("a fully clamped zero chance never rolls a chest"), NoChest.bChestRolled);
	TestEqual(TEXT("a failed normal roll has no chest tier"), NoChest.ChestTier, EGameXXKTrainingRewardTier::None);

	const FGameXXKTrainingReward BossChest = FGameXXKTrainingRules::ResolveChallengeReward(
		StageOne,
		EGameXXKTrainingEncounterKind::Boss,
		Seed,
		1.0f);
	TestTrue(TEXT("a fully clamped one chance always rolls a boss chest"), BossChest.bChestRolled);
	TestEqual(TEXT("a boss roll resolves to the advanced chest tier"), BossChest.ChestTier, EGameXXKTrainingRewardTier::AdvancedChest);

	const FGameXXKTrainingReward EliteChest = FGameXXKTrainingRules::ResolveChallengeReward(
		StageOne,
		EGameXXKTrainingEncounterKind::Elite,
		Seed,
		1.0f);
	TestEqual(TEXT("an elite roll resolves to the advanced chest tier"), EliteChest.ChestTier, EGameXXKTrainingRewardTier::AdvancedChest);

	const FGameXXKTrainingReward TravelNormalChest = FGameXXKTrainingRules::ResolveTravelReward(
		FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 2),
		EGameXXKTrainingEncounterKind::Normal,
		Seed,
		0,
		0,
		1.0f);
	TestTrue(TEXT("Travel can roll a normal chest when its cooldown is ready"), TravelNormalChest.bChestRolled);
	TestEqual(TEXT("Travel normal encounters use the normal chest tier"), TravelNormalChest.ChestTier, EGameXXKTrainingRewardTier::NormalChest);
	TestEqual(TEXT("normal chest resolves to the canonical inventory item"), TravelNormalChest.ChestItemId, FName(TEXT("Item.TrainingNormalChest")));
	bool bNormalChestItemFound = false;
	UGameXXKMVPRules::GetItemDef(TravelNormalChest.ChestItemId, bNormalChestItemFound);
	TestTrue(TEXT("normal chest item is registered in the inventory catalog"), bNormalChestItemFound);
	const FGameXXKTrainingReward TravelOneOneNormal = FGameXXKTrainingRules::ResolveTravelReward(
		StageOne,
		EGameXXKTrainingEncounterKind::Normal,
		Seed,
		0,
		0,
		1.0f);
	TestFalse(TEXT("the one-health 1-1 Travel exception never rolls a normal chest"), TravelOneOneNormal.bChestRolled);
	TestEqual(TEXT("the one-health 1-1 Travel exception has no chest tier"), TravelOneOneNormal.ChestTier, EGameXXKTrainingRewardTier::None);
	const FGameXXKTrainingReward TravelOneOneElite = FGameXXKTrainingRules::ResolveTravelReward(
		StageOne,
		EGameXXKTrainingEncounterKind::Elite,
		Seed,
		0,
		0,
		1.0f);
	TestFalse(TEXT("the one-health 1-1 Travel exception never rolls an elite chest"), TravelOneOneElite.bChestRolled);
	const FGameXXKTrainingReward TravelOneOneBoss = FGameXXKTrainingRules::ResolveTravelReward(
		StageOne,
		EGameXXKTrainingEncounterKind::Boss,
		Seed,
		0,
		0,
		1.0f,
		true);
	TestFalse(TEXT("the one-health 1-1 Travel exception never rolls a boss chest"), TravelOneOneBoss.bChestRolled);
	TestEqual(TEXT("the one-health 1-1 boss still keeps its gold/experience stage reward"), TravelOneOneBoss.Gold, FGameXXKTrainingRules::BuildTravelReward(StageOne).Gold);
	const FGameXXKTrainingReward TravelNormalOnCooldown = FGameXXKTrainingRules::ResolveTravelReward(
		FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 2),
		EGameXXKTrainingEncounterKind::Normal,
		Seed,
		FGameXXKTrainingRules::TravelNormalChestCooldownSeconds,
		0,
		1.0f);
	TestFalse(TEXT("Travel normal chest cooldown blocks an otherwise guaranteed roll"), TravelNormalOnCooldown.bChestRolled);

	const FGameXXKTrainingReward TravelEliteChest = FGameXXKTrainingRules::ResolveTravelReward(
		FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 2),
		EGameXXKTrainingEncounterKind::Elite,
		Seed,
		0,
		0,
		1.0f);
	TestTrue(TEXT("Travel can roll an advanced elite chest when its cooldown is ready"), TravelEliteChest.bChestRolled);
	TestEqual(TEXT("Travel elite encounters use the advanced chest tier"), TravelEliteChest.ChestTier, EGameXXKTrainingRewardTier::AdvancedChest);
	TestEqual(TEXT("advanced chest resolves to the canonical inventory item"), TravelEliteChest.ChestItemId, FName(TEXT("Item.TrainingAdvancedChest")));
	bool bAdvancedChestItemFound = false;
	UGameXXKMVPRules::GetItemDef(TravelEliteChest.ChestItemId, bAdvancedChestItemFound);
	TestTrue(TEXT("advanced chest item is registered in the inventory catalog"), bAdvancedChestItemFound);
	const FGameXXKTrainingReward TravelEliteOnCooldown = FGameXXKTrainingRules::ResolveTravelReward(
		FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 2),
		EGameXXKTrainingEncounterKind::Elite,
		Seed,
		0,
		FGameXXKTrainingRules::TravelAdvancedChestCooldownSeconds,
		1.0f);
	TestFalse(TEXT("Travel advanced chest cooldown blocks an otherwise guaranteed roll"), TravelEliteOnCooldown.bChestRolled);
	TestEqual(TEXT("normal chest cooldown is four minutes"),
		FGameXXKTrainingRules::TravelChestCooldownSeconds(EGameXXKTrainingRewardTier::NormalChest),
		FGameXXKTrainingRules::TravelNormalChestCooldownSeconds);
	TestEqual(TEXT("advanced chest cooldown is six minutes"),
		FGameXXKTrainingRules::TravelChestCooldownSeconds(EGameXXKTrainingRewardTier::AdvancedChest),
		FGameXXKTrainingRules::TravelAdvancedChestCooldownSeconds);
	TestEqual(TEXT("cooldown advances without going below zero"),
		FGameXXKTrainingRules::AdvanceTravelChestCooldown(120, 200),
		0);
	TestEqual(TEXT("cooldown ignores negative elapsed time"),
		FGameXXKTrainingRules::AdvanceTravelChestCooldown(120, -5),
		120);

	const int32 NextSeed = FGameXXKTrainingRules::NextChallengeRewardSeed(Seed);
	TestTrue(TEXT("the reward sequence advances to a non-zero seed"), NextSeed != 0);
	TestTrue(TEXT("the reward sequence advances to a different seed"), NextSeed != Seed);
	TestEqual(
		TEXT("the default reward seed is stable and non-zero"),
		FGameXXKTrainingRules::DefaultChallengeRewardSeed(),
		FGameXXKTrainingRules::DefaultChallengeRewardSeed());
	TestTrue(TEXT("the default reward seed is non-zero"), FGameXXKTrainingRules::DefaultChallengeRewardSeed() != 0);

	FGameXXKTrainingProgress NewGameProgress;
	FGameXXKTrainingRules::InitializeNewGame(NewGameProgress);
	TestEqual(TEXT("new Training progress starts with the default reward seed"),
		NewGameProgress.ChallengeRewardSeed,
		FGameXXKTrainingRules::DefaultChallengeRewardSeed());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTrainingRewardCooldownMigrationTest,
	"GameXXK.Training.RewardCooldownMigration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTrainingRewardCooldownMigrationTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState LegacyRuntime = UGameXXKMVPRules::CreateNewGame();
	LegacyRuntime.Training.ChallengeRewardSeed = 0;
	LegacyRuntime.Training.TravelNormalChestCooldownRemainingSeconds = 0;
	LegacyRuntime.Training.TravelAdvancedChestCooldownRemainingSeconds = 0;
	FGameXXKSaveState LegacySave = UGameXXKMVPRules::MakeSaveState(LegacyRuntime);
	LegacySave.SaveVersion = FGameXXKSaveMigration::DesktopTrainingWorkbenchIntroducedSaveVersion;

	FGameXXKSaveState Migrated;
	FGameXXKSaveMigrationReport Report;
	TestTrue(TEXT("v18 Training save migrates through the v20 reward/cooldown/offline schema"),
		FGameXXKSaveMigration::MigrateToCurrent(LegacySave, Migrated, Report));
	TestEqual(TEXT("reward/cooldown migration writes the current version"),
		Migrated.SaveVersion,
		FGameXXKSaveMigration::CurrentSaveVersion);
	TestEqual(TEXT("v18 gets the default deterministic reward seed"),
		Migrated.RuntimeState.Training.ChallengeRewardSeed,
		FGameXXKTrainingRules::DefaultChallengeRewardSeed());
	TestEqual(TEXT("legacy normal chest cooldown starts ready"),
		Migrated.RuntimeState.Training.TravelNormalChestCooldownRemainingSeconds,
		0);
	TestEqual(TEXT("legacy advanced chest cooldown starts ready"),
		Migrated.RuntimeState.Training.TravelAdvancedChestCooldownRemainingSeconds,
		0);

	FGameXXKSaveState CurrentSave = UGameXXKMVPRules::MakeSaveState(UGameXXKMVPRules::CreateNewGame());
	CurrentSave.RuntimeState.Training.TravelNormalChestCooldownRemainingSeconds =
		FGameXXKTrainingRules::TravelNormalChestCooldownSeconds;
	CurrentSave.RuntimeState.Training.TravelAdvancedChestCooldownRemainingSeconds =
		FGameXXKTrainingRules::TravelAdvancedChestCooldownSeconds;
	FGameXXKSaveState CurrentRoundTrip;
	FGameXXKSaveMigrationReport CurrentReport;
	CurrentSave.RuntimeState.Training.PendingTravelGold = 77;
	CurrentSave.RuntimeState.Training.PendingTravelExperience = 33;
	CurrentSave.RuntimeState.Training.PendingTravelNormalChestCount = 2;
	CurrentSave.RuntimeState.Training.PendingTravelAdvancedChestCount = 1;
	CurrentSave.RuntimeState.Training.PendingTravelCompletedEncounters = 7;
	CurrentSave.RuntimeState.Training.PendingTravelCompletedStages = 1;
	CurrentSave.RuntimeState.Training.PendingTravelSimulatedSeconds = 3600;
	CurrentSave.RuntimeState.Training.TravelLastUpdatedUnixSeconds = 123456789;
	TestTrue(TEXT("current v20 cooldown and offline ledger state round-trips"),
		FGameXXKSaveMigration::MigrateToCurrent(CurrentSave, CurrentRoundTrip, CurrentReport));
	TestEqual(TEXT("normal cooldown survives a v20 round-trip"),
		CurrentRoundTrip.RuntimeState.Training.TravelNormalChestCooldownRemainingSeconds,
		FGameXXKTrainingRules::TravelNormalChestCooldownSeconds);
	TestEqual(TEXT("advanced cooldown survives a v20 round-trip"),
		CurrentRoundTrip.RuntimeState.Training.TravelAdvancedChestCooldownRemainingSeconds,
		FGameXXKTrainingRules::TravelAdvancedChestCooldownSeconds);
	TestEqual(TEXT("pending travel gold survives a v20 round-trip"),
		CurrentRoundTrip.RuntimeState.Training.PendingTravelGold,
		77);
	TestEqual(TEXT("pending advanced chests survive a v20 round-trip"),
		CurrentRoundTrip.RuntimeState.Training.PendingTravelAdvancedChestCount,
		1);
	TestEqual(TEXT("travel offline timestamp survives a v20 round-trip"),
		CurrentRoundTrip.RuntimeState.Training.TravelLastUpdatedUnixSeconds,
		int64(123456789));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTrainingMutuallyExclusiveModesTest,
	"GameXXK.Training.MutuallyExclusiveModes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTrainingMutuallyExclusiveModesTest::RunTest(const FString& Parameters)
{
	FGameXXKTrainingProgress Progress;
	FGameXXKTrainingRules::InitializeNewGame(Progress);
	const FName StageTwo = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 2);
	TestTrue(TEXT("challenge can start from a cleared-next-stage fixture"), FGameXXKTrainingRules::StartChallenge(Progress, StageTwo));
	TestFalse(TEXT("challenge start pauses travel"), Progress.bTravelActive);
	TestFalse(TEXT("travel cannot start while challenge is active"), FGameXXKTrainingRules::StartTravel(Progress, StageTwo));
	TestTrue(TEXT("challenge completion closes the challenge mode"), FGameXXKTrainingRules::CompleteChallenge(Progress, StageTwo));
	TestTrue(TEXT("travel can start after challenge closes"), FGameXXKTrainingRules::StartTravel(Progress, StageTwo));
	TestTrue(TEXT("travel becomes active"), Progress.bTravelActive);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTrainingFailurePausesWhenRetryDisabledTest,
	"GameXXK.Training.FailurePausesWhenRetryDisabled",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTrainingFailurePausesWhenRetryDisabledTest::RunTest(const FString& Parameters)
{
	FGameXXKTrainingProgress Progress;
	FGameXXKTrainingRules::InitializeNewGame(Progress);
	Progress.bRetryOnFailure = false;
	TestTrue(TEXT("failure resolves at 1-1"), FGameXXKTrainingRules::ResolveTravelFailure(Progress));
	TestFalse(TEXT("retry-off failure pauses travel"), Progress.bTravelActive);
	TestEqual(TEXT("retry-off 1-1 stays at 1-1"), Progress.CurrentTravelStageId, FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTrainingTravelRunnerLoopTest,
	"GameXXK.Training.TravelRunnerLoop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTrainingTravelRunnerLoopTest::RunTest(const FString& Parameters)
{
	FGameXXKTrainingProgress Progress;
	FGameXXKTrainingRules::InitializeNewGame(Progress);
	const FName StageOne = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1);
	FGameXXKTrainingTravelRuntime Runner;
	TestTrue(TEXT("the new-game travel runner initializes"), FGameXXKTrainingRules::InitializeTravelRunner(Progress, Runner, 100, 100, 100));
	TestEqual(TEXT("runner starts in walking phase"), Runner.Phase, EGameXXKTrainingTravelPhase::Walking);

	bool bEncounterCompleted = false;
	bool bStageCompleted = false;
	bool bDefeated = false;
	FGameXXKTrainingReward Reward;
	TestTrue(TEXT("first walk step advances"), FGameXXKTrainingRules::AdvanceTravelRunner(
		Progress, Runner, bEncounterCompleted, bStageCompleted, bDefeated, Reward));
	TestFalse(TEXT("first walk step does not complete an encounter"), bEncounterCompleted);
	TestEqual(TEXT("first walk step remains walking"), Runner.Phase, EGameXXKTrainingTravelPhase::Walking);
	TestTrue(TEXT("second walk step reaches the encounter"), FGameXXKTrainingRules::AdvanceTravelRunner(
		Progress, Runner, bEncounterCompleted, bStageCompleted, bDefeated, Reward));
	TestEqual(TEXT("second walk step enters combat"), Runner.Phase, EGameXXKTrainingTravelPhase::Combat);

	int32 CompletedEncounters = 0;
	for (int32 Guard = 0; Guard < 64 && !bStageCompleted; ++Guard)
	{
		bEncounterCompleted = false;
		bStageCompleted = false;
		bDefeated = false;
		TestTrue(TEXT("travel runner advances a deterministic tick"), FGameXXKTrainingRules::AdvanceTravelRunner(
			Progress, Runner, bEncounterCompleted, bStageCompleted, bDefeated, Reward));
		TestFalse(TEXT("high attack runner does not die"), bDefeated);
		if (bEncounterCompleted)
		{
			++CompletedEncounters;
		}
	}

	TestEqual(TEXT("all seven encounters complete as one travel stage"), CompletedEncounters, 7);
	TestTrue(TEXT("travel stage completes at the boss"), bStageCompleted);
	TestEqual(TEXT("travel victory loops back to encounter zero"), Progress.ActiveTravelEncounterIndex, 0);
	TestEqual(TEXT("travel victory count increments once"), Progress.TravelVictories, 1);
	TestTrue(TEXT("1-1 travel settlement still grants the base travel reward"), Reward.Gold > 0);
	TestTrue(TEXT("1-1 travel settlement may resolve a chest independently of the base reward"),
		Reward.ChestTier == EGameXXKTrainingRewardTier::None
		|| Reward.ChestTier == EGameXXKTrainingRewardTier::NormalChest
		|| Reward.ChestTier == EGameXXKTrainingRewardTier::AdvancedChest);
	TestTrue(TEXT("travel reward grants gold"), Reward.Gold > 0);
	TestTrue(TEXT("travel runner continues walking after settlement"), Runner.Phase == EGameXXKTrainingTravelPhase::Walking);
	TestEqual(TEXT("runner remains on the selected stage"), Runner.StageId, StageOne);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTrainingTravelRunnerFailureTest,
	"GameXXK.Training.TravelRunnerFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTrainingTravelRunnerFailureTest::RunTest(const FString& Parameters)
{
	FGameXXKTrainingProgress Progress;
	FGameXXKTrainingRules::InitializeNewGame(Progress);
	const FName StageTwo = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 2);
	Progress.ClearedStageIds.Add(StageTwo);
	TestTrue(TEXT("stage two travel starts"), FGameXXKTrainingRules::StartTravel(Progress, StageTwo));
	FGameXXKTrainingTravelRuntime Runner;
	TestTrue(TEXT("stage two runner initializes"), FGameXXKTrainingRules::InitializeTravelRunner(Progress, Runner, 1, 1, 1));

	bool bEncounterCompleted = false;
	bool bStageCompleted = false;
	bool bDefeated = false;
	FGameXXKTrainingReward Reward;
	for (int32 Guard = 0; Guard < 3 && Runner.Phase != EGameXXKTrainingTravelPhase::Combat; ++Guard)
	{
		TestTrue(TEXT("failure fixture reaches combat"), FGameXXKTrainingRules::AdvanceTravelRunner(
			Progress, Runner, bEncounterCompleted, bStageCompleted, bDefeated, Reward));
	}
	TestEqual(TEXT("failure fixture is in combat"), Runner.Phase, EGameXXKTrainingTravelPhase::Combat);
	TestTrue(TEXT("enemy attack defeats the one-health player"), FGameXXKTrainingRules::AdvanceTravelRunner(
		Progress, Runner, bEncounterCompleted, bStageCompleted, bDefeated, Reward));
	TestTrue(TEXT("runner reports defeat"), bDefeated);
	TestEqual(TEXT("defeated runner records zero player HP"), Runner.PlayerHP, 0);
	TestEqual(TEXT("defeated runner stays on the current encounter"), Progress.ActiveTravelEncounterIndex, 0);

	Progress.bRetryOnFailure = true;
	TestTrue(TEXT("retry-on failure resets the travel encounter"), FGameXXKTrainingRules::ResolveTravelFailure(Progress));
	TestEqual(TEXT("retry-on keeps the selected stage"), Progress.CurrentTravelStageId, StageTwo);
	TestEqual(TEXT("retry-on resets the encounter index"), Progress.ActiveTravelEncounterIndex, 0);
	TestTrue(TEXT("travel remains active after retry"), Progress.bTravelActive);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTrainingTravelSubsystemBridgeTest,
	"GameXXK.Training.TravelSubsystemBridge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTrainingTravelSubsystemBridgeTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestNotNull(TEXT("travel bridge subsystem exists"), Subsystem);
	if (!Subsystem)
	{
		return false;
	}
	TestTrue(TEXT("travel bridge starts a new game"), Subsystem->StartGame());
	const FName StageOne = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1);
	TestTrue(TEXT("travel bridge starts cleared 1-1"), Subsystem->StartTrainingTravel(StageOne));
	TestEqual(TEXT("travel bridge exposes the walking phase"), Subsystem->GetTrainingTravelRuntimeCopy().Phase, EGameXXKTrainingTravelPhase::Walking);

	const int32 GoldBefore = Subsystem->GetRuntimeState().PlayerGold;
	bool bStageCompleted = false;
	int32 CompletedEncounters = 0;
	FGameXXKTrainingReward Reward;
	for (int32 Guard = 0; Guard < 64 && !bStageCompleted; ++Guard)
	{
		bool bEncounterCompleted = false;
		bool bDefeated = false;
		TestTrue(TEXT("subsystem advances the travel runner"), Subsystem->AdvanceTrainingTravelStep(
			bEncounterCompleted, bStageCompleted, bDefeated, Reward));
		TestFalse(TEXT("default player survives the 1-1 travel loop"), bDefeated);
		if (bEncounterCompleted)
		{
			++CompletedEncounters;
		}
	}

	TestEqual(TEXT("subsystem settles seven encounters"), CompletedEncounters, 7);
	TestTrue(TEXT("subsystem reports boss settlement"), bStageCompleted);
	TestTrue(TEXT("subsystem writes travel gold at settlement"), Subsystem->GetRuntimeState().PlayerGold > GoldBefore);
	TestEqual(TEXT("subsystem restarts the travel strip at walking"), Subsystem->GetTrainingTravelRuntimeCopy().Phase, EGameXXKTrainingTravelPhase::Walking);
	TestEqual(TEXT("subsystem leaves travel mode active after looping"), Subsystem->GetTrainingProgressCopy().bTravelActive, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTrainingRealCardBattleBridgeTest,
	"GameXXK.Training.RealCardBattleBridge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTrainingRealCardBattleBridgeTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestNotNull(TEXT("training bridge subsystem exists"), Subsystem);
	if (!Subsystem)
	{
		return false;
	}
	const bool bNewGameStarted = Subsystem->StartGame();
	TestTrue(TEXT("training bridge starts from a normal new game"), bNewGameStarted);
	if (!bNewGameStarted)
	{
		return false;
	}

	const FName StageTwo = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 2);
	const bool bStarted = Subsystem->StartTrainingChallenge(StageTwo);
	TestTrue(TEXT("training bridge starts an unlocked challenge"), bStarted);
	if (!bStarted)
	{
		return false;
	}
	TestTrue(TEXT("training challenge owns a real card battle"), Subsystem->IsTrainingChallengeBattleActive());
	TestEqual(TEXT("training battle enters the real Battle screen"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Battle);
	const int32 EnemyCount = Subsystem->GetRuntimeState().ActiveBattleEnemies.Num();
	TestEqual(TEXT("one training enemy appears per encounter"), EnemyCount, 1);
	if (EnemyCount != 1)
	{
		return false;
	}
	const FName ExpectedEnemy = FGameXXKTrainingRules::BuildEncounterSequence(StageTwo)[0].EnemyDefinitionId;
	TestEqual(TEXT("training battle uses the authored encounter enemy"), Subsystem->GetRuntimeState().ActiveBattleEnemies[0].EnemyDefinitionId, ExpectedEnemy);

	bool bStageCompleted = false;
	FGameXXKTrainingReward Reward;
	TestTrue(TEXT("one auto step advances the real card runtime"), Subsystem->AdvanceTrainingChallengeEncounter(bStageCompleted, Reward));
	TestTrue(TEXT("training challenge remains active until the encounter is terminal"), Subsystem->GetRuntimeState().Training.bChallengeActive);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTrainingTravelChestInventoryBridgeTest,
	"GameXXK.Training.TravelChestInventoryBridge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTrainingTravelChestInventoryBridgeTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestNotNull(TEXT("travel chest inventory bridge subsystem exists"), Subsystem);
	if (!Subsystem || !Subsystem->StartGame())
	{
		return false;
	}

	const FName StageTwo = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 2);
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	State.Training.ClearedStageIds.Add(StageTwo);
	State.Training.CurrentTravelStageId = StageTwo;
	State.Training.SelectedStageId = StageTwo;
	State.Training.TravelNormalChestCooldownRemainingSeconds = 0;
	State.Training.ChallengeRewardSeed = FGameXXKTrainingRules::DefaultChallengeRewardSeed();
	int32 GuaranteedSeed = State.Training.ChallengeRewardSeed;
	for (int32 Attempt = 0; Attempt < 10000; ++Attempt)
	{
		if (FGameXXKTrainingRules::ResolveTravelReward(
			StageTwo,
			EGameXXKTrainingEncounterKind::Normal,
			GuaranteedSeed,
			0,
			0,
			0.0f).bChestRolled)
		{
			break;
		}
		GuaranteedSeed = FGameXXKTrainingRules::NextChallengeRewardSeed(GuaranteedSeed);
	}
	State.Training.ChallengeRewardSeed = GuaranteedSeed;

	TestTrue(TEXT("travel chest inventory bridge starts a cleared stage"), Subsystem->StartTrainingTravel(StageTwo));
	const int32 NormalChestBefore = UGameXXKMVPRules::GetItemCount(State, UGameXXKMVPRules::ItemTrainingNormalChest());
	FGameXXKTrainingReward Reward;
	bool bEncounterCompleted = false;
	bool bStageCompleted = false;
	bool bDefeated = false;
	for (int32 Guard = 0; Guard < 64 && !bEncounterCompleted; ++Guard)
	{
		TestTrue(TEXT("travel chest inventory bridge advances"), Subsystem->AdvanceTrainingTravelStep(
			bEncounterCompleted,
			bStageCompleted,
			bDefeated,
			Reward));
	}
	TestFalse(TEXT("guaranteed chest fixture does not defeat the player"), bDefeated);
	TestTrue(TEXT("travel chest inventory bridge settles an encounter"), bEncounterCompleted);
	TestTrue(TEXT("travel chest inventory bridge reports the normal chest"), Reward.bChestRolled);
	TestEqual(TEXT("travel chest inventory bridge reports the canonical item"), Reward.ChestItemId, UGameXXKMVPRules::ItemTrainingNormalChest());
	TestEqual(TEXT("travel chest inventory bridge writes one normal chest to inventory"),
		UGameXXKMVPRules::GetItemCount(Subsystem->GetRuntimeState(), UGameXXKMVPRules::ItemTrainingNormalChest()),
		NormalChestBefore + 1);
	TestEqual(TEXT("normal Travel chest settlement starts the four-minute cooldown"),
		Subsystem->GetRuntimeState().Training.TravelNormalChestCooldownRemainingSeconds,
		FGameXXKTrainingRules::TravelNormalChestCooldownSeconds);
	bool bCooldownEncounterCompleted = false;
	bool bCooldownStageCompleted = false;
	bool bCooldownDefeated = false;
	FGameXXKTrainingReward CooldownTickReward;
	TestTrue(TEXT("Travel cooldown can advance during the next logical tick"), Subsystem->AdvanceTrainingTravelStep(
		bCooldownEncounterCompleted,
		bCooldownStageCompleted,
		bCooldownDefeated,
		CooldownTickReward,
		30));
	TestEqual(TEXT("normal Travel cooldown decrements by elapsed logical seconds"),
		Subsystem->GetRuntimeState().Training.TravelNormalChestCooldownRemainingSeconds,
		FGameXXKTrainingRules::TravelNormalChestCooldownSeconds - 30);
	const FGameXXKSaveState SaveState = UGameXXKMVPRules::MakeSaveState(Subsystem->GetRuntimeState());
	FGameXXKSaveState RoundTrip;
	FGameXXKSaveMigrationReport MigrationReport;
	TestTrue(TEXT("travel chest inventory save round-trip succeeds"),
		FGameXXKSaveMigration::MigrateToCurrent(SaveState, RoundTrip, MigrationReport));
	TestEqual(TEXT("travel chest inventory survives save round-trip"),
		UGameXXKMVPRules::GetItemCount(RoundTrip.RuntimeState, UGameXXKMVPRules::ItemTrainingNormalChest()),
		NormalChestBefore + 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTrainingTravelOfflineRulesTest,
	"GameXXK.Training.TravelOfflineRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTrainingTravelOfflineRulesTest::RunTest(const FString& Parameters)
{
	FGameXXKTrainingProgress Progress;
	FGameXXKTrainingRules::InitializeNewGame(Progress);
	FGameXXKTrainingTravelRuntime Runtime;
	TestTrue(TEXT("offline rules initialize the cleared 1-1 runner"),
		FGameXXKTrainingRules::InitializeTravelRunner(Progress, Runtime, 100, 100, 100));

	FGameXXKTrainingOfflineReward SimulatedReward;
	TestTrue(TEXT("offline rules simulate a bounded elapsed window"),
		FGameXXKTrainingRules::AdvanceTravelOffline(Progress, Runtime, 64, SimulatedReward));
	TestTrue(TEXT("offline rules record simulated logical seconds"), SimulatedReward.SimulatedSeconds > 0);
	TestTrue(TEXT("offline rules settle at least one 1-1 encounter"), SimulatedReward.CompletedEncounters > 0);
	TestTrue(TEXT("offline rules settle the 1-1 stage with base gold"), SimulatedReward.Gold > 0);
	TestEqual(TEXT("offline 1-1 travel never grants a normal chest"), SimulatedReward.NormalChestCount, 0);
	TestEqual(TEXT("offline 1-1 travel never grants an advanced chest"), SimulatedReward.AdvancedChestCount, 0);

	TestTrue(TEXT("offline rules move the result into the pending reward ledger"),
		FGameXXKTrainingRules::AccumulatePendingTravelReward(Progress, SimulatedReward));
	FGameXXKTrainingOfflineReward PendingReward;
	TestTrue(TEXT("pending travel reward can be read back"),
		FGameXXKTrainingRules::GetPendingTravelReward(Progress, PendingReward));
	TestEqual(TEXT("pending travel gold matches the simulated result"), PendingReward.Gold, SimulatedReward.Gold);
	TestEqual(TEXT("pending travel encounter count matches the simulated result"), PendingReward.CompletedEncounters, SimulatedReward.CompletedEncounters);

	FGameXXKTrainingOfflineReward ConsumedReward;
	TestTrue(TEXT("pending travel reward can be consumed exactly once"),
		FGameXXKTrainingRules::ConsumePendingTravelReward(Progress, ConsumedReward));
	TestEqual(TEXT("consumed travel gold matches the pending result"), ConsumedReward.Gold, SimulatedReward.Gold);
	FGameXXKTrainingOfflineReward EmptyReward;
	TestFalse(TEXT("consumed travel reward ledger is empty"),
		FGameXXKTrainingRules::GetPendingTravelReward(Progress, EmptyReward));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTrainingTravelOfflineSubsystemBridgeTest,
	"GameXXK.Training.TravelOfflineSubsystemBridge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTrainingTravelOfflineSubsystemBridgeTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestNotNull(TEXT("offline travel bridge subsystem exists"), Subsystem);
	if (!Subsystem || !Subsystem->StartGame())
	{
		return false;
	}

	const FName StageOne = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1);
	TestTrue(TEXT("offline travel bridge starts 1-1"), Subsystem->StartTrainingTravel(StageOne));
	FGameXXKTrainingOfflineReward SimulatedReward;
	TestTrue(TEXT("offline travel bridge simulates elapsed travel"),
		Subsystem->SimulateTrainingTravelOffline(64, SimulatedReward));
	TestTrue(TEXT("offline travel bridge exposes pending gold"),
		Subsystem->GetPendingTrainingTravelRewardCopy().Gold > 0);

	const int32 GoldBeforeCollect = Subsystem->GetRuntimeState().PlayerGold;
	FGameXXKTrainingOfflineReward CollectedReward;
	TestTrue(TEXT("offline travel bridge collects pending rewards"),
		Subsystem->CollectTrainingTravelRewards(CollectedReward));
	TestEqual(TEXT("offline collect returns the simulated gold"), CollectedReward.Gold, SimulatedReward.Gold);
	TestEqual(TEXT("offline collect writes gold to the runtime inventory"),
		Subsystem->GetRuntimeState().PlayerGold,
		GoldBeforeCollect + SimulatedReward.Gold);
	TestEqual(TEXT("offline collect clears the pending ledger"),
		Subsystem->GetPendingTrainingTravelRewardCopy().Gold,
		0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTrainingSaveValidationTest,
	"GameXXK.Training.SaveValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTrainingSaveValidationTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	FString Error;
	TestTrue(TEXT("new-game Training state validates"), FGameXXKSaveMigration::ValidateRuntimeState(State, Error));

	State.Training.ActiveTravelEncounterIndex = 999;
	TestFalse(TEXT("invalid travel encounter index is rejected"), FGameXXKSaveMigration::ValidateRuntimeState(State, Error));

	FGameXXKTrainingRules::InitializeNewGame(State.Training);
	State.Training.bTravelActive = false;
	State.Training.ActiveTravelEncounterIndex = INDEX_NONE;
	State.Training.bChallengeActive = true;
	State.Training.ActiveChallengeStageId = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 2);
	State.Training.ActiveChallengeEncounterIndex = 999;
	TestFalse(TEXT("invalid challenge encounter index is rejected"), FGameXXKSaveMigration::ValidateRuntimeState(State, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTrainingTravelOfflineLoadTest,
	"GameXXK.Training.TravelOfflineLoad",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTrainingTravelOfflineLoadTest::RunTest(const FString& Parameters)
{
	const FString SlotName = TEXT("GameXXK_TrainingTravelOfflineLoadTest");
	const int32 UserIndex = 0;
	UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex);

	UGameInstance* SourceGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* SourceSubsystem = NewObject<UGameXXKMVPSubsystem>(SourceGameInstance);
	TestNotNull(TEXT("offline load source subsystem exists"), SourceSubsystem);
	if (!SourceSubsystem || !SourceSubsystem->StartGame())
	{
		return false;
	}
	const FName StageOne = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1);
	TestTrue(TEXT("offline load source starts 1-1 travel"), SourceSubsystem->StartTrainingTravel(StageOne));

	FGameXXKRuntimeState SourceState = SourceSubsystem->GetRuntimeStateCopy();
	SourceState.Training.TravelLastUpdatedUnixSeconds = FDateTime::UtcNow().ToUnixTimestamp() - 64;
	UGameXXKSaveGame* SaveGame = Cast<UGameXXKSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UGameXXKSaveGame::StaticClass()));
	TestNotNull(TEXT("offline load save object exists"), SaveGame);
	if (!SaveGame)
	{
		return false;
	}
	SaveGame->SaveState = UGameXXKMVPRules::MakeSaveState(SourceState);
	TestTrue(TEXT("offline load source save writes"), UGameplayStatics::SaveGameToSlot(SaveGame, SlotName, UserIndex));

	UGameInstance* LoadedGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* LoadedSubsystem = NewObject<UGameXXKMVPSubsystem>(LoadedGameInstance);
	TestTrue(TEXT("offline load restores the saved travel"), LoadedSubsystem->LoadGameFromSlot(SlotName, UserIndex));
	TestTrue(TEXT("offline load creates pending travel gold"),
		LoadedSubsystem->GetPendingTrainingTravelRewardCopy().Gold > 0);
	TestTrue(TEXT("offline load keeps travel active after simulating"),
		LoadedSubsystem->GetTrainingProgressCopy().bTravelActive);

	UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex);
	return true;
}

#endif
