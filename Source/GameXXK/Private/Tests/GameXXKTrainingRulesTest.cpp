#include "GameXXKTrainingRules.h"
#include "GameXXKEquipmentRules.h"
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
	TestTrue(TEXT("cleared 1-1 can be replayed as a direct challenge"), FGameXXKTrainingRules::CanChallenge(Progress, StageOne));
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
	TestEqual(TEXT("all three difficulty bands expose nine stages"),
		FGameXXKTrainingRules::GetStageDefinitions().Num(), 27);
	TestTrue(TEXT("Hard unlocks after Normal 3-3"),
		FGameXXKTrainingRules::IsDifficultyUnlocked(Progress, EGameXXKTrainingDifficulty::Hard));
	const FName HardOne = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Hard, 1);
	FGameXXKTrainingStageDefinition HardDefinition;
	TestTrue(TEXT("Hard 1-1 has its own difficulty-stage definition"),
		FGameXXKTrainingRules::TryGetStageDefinition(HardOne, HardDefinition));
	TestTrue(TEXT("Hard 1-1 is challengeable after Normal 3-3"),
		FGameXXKTrainingRules::CanChallenge(Progress, HardOne));
	for (int32 StageNumber = 1; StageNumber <= 9; ++StageNumber)
	{
		const FName StageId = FGameXXKTrainingRules::MakeStageId(
			EGameXXKTrainingDifficulty::Hard,
			StageNumber);
		TestTrue(TEXT("next Hard stage is challengeable"),
			FGameXXKTrainingRules::CanChallenge(Progress, StageId));
		TestTrue(TEXT("Hard challenge starts"), FGameXXKTrainingRules::StartChallenge(Progress, StageId));
		TestTrue(TEXT("Hard challenge completes"), FGameXXKTrainingRules::CompleteChallenge(Progress, StageId));
	}
	TestTrue(TEXT("Hell unlocks after Hard 3-3"),
		FGameXXKTrainingRules::IsDifficultyUnlocked(Progress, EGameXXKTrainingDifficulty::Hell));
	const FName HellOne = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Hell, 1);
	TestTrue(TEXT("Hell 1-1 is challengeable after Hard 3-3"),
		FGameXXKTrainingRules::CanChallenge(Progress, HellOne));
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
	const TArray<FGameXXKTrainingEncounterDefinition> Encounters = FGameXXKTrainingRules::BuildEncounterSequence(StageOne, false);
	const TArray<FGameXXKTrainingEncounterDefinition> TravelEncounters = FGameXXKTrainingRules::BuildEncounterSequence(StageOne, true);
	TestEqual(TEXT("one stage keeps four normal waves, two elite waves and one boss wave"), Encounters.Num(), 7);
	const EGameXXKTrainingEncounterKind ExpectedWaveKinds[] = {
		EGameXXKTrainingEncounterKind::Normal,
		EGameXXKTrainingEncounterKind::Normal,
		EGameXXKTrainingEncounterKind::Elite,
		EGameXXKTrainingEncounterKind::Normal,
		EGameXXKTrainingEncounterKind::Elite,
		EGameXXKTrainingEncounterKind::Normal,
		EGameXXKTrainingEncounterKind::Boss};
	for (int32 WaveIndex = 0; WaveIndex < UE_ARRAY_COUNT(ExpectedWaveKinds); ++WaveIndex)
	{
		TestEqual(
			*FString::Printf(TEXT("wave %d follows the approved normal/elite cadence"), WaveIndex + 1),
			Encounters.IsValidIndex(WaveIndex)
				? Encounters[WaveIndex].Kind
				: EGameXXKTrainingEncounterKind::Boss,
			ExpectedWaveKinds[WaveIndex]);
	}
	int32 EliteCount = 0;
	for (const FGameXXKTrainingEncounterDefinition& Encounter : Encounters)
	{
		TestFalse(TEXT("every authored formation has a primary enemy"), Encounter.EnemyDefinitionId.IsNone());
		TestFalse(TEXT("no formation contains a missing enemy identity"), Encounter.EnemyDefinitionIds.Contains(NAME_None));
		if (Encounter.Kind == EGameXXKTrainingEncounterKind::Normal)
		{
			TestEqual(TEXT("ordinary waves contain two enemies"), Encounter.EnemyDefinitionIds.Num(), 2);
			if (Encounter.EnemyDefinitionIds.Num() == 2)
			{
				TestEqual(TEXT("ordinary wave left slot is rooster"), Encounter.EnemyDefinitionIds[0], FName(TEXT("Enemy.Ch1.Rooster")));
				TestEqual(TEXT("ordinary wave right slot is civet"), Encounter.EnemyDefinitionIds[1], FName(TEXT("Enemy.Ch1.Civet")));
			}
		}
		else if (Encounter.Kind == EGameXXKTrainingEncounterKind::Elite)
		{
			++EliteCount;
			TestEqual(TEXT("elite waves fill all three enemy slots"), Encounter.EnemyDefinitionIds.Num(), 3);
			if (Encounter.EnemyDefinitionIds.Num() == 3)
			{
				TestEqual(TEXT("elite wave keeps rooster on the left flank"), Encounter.EnemyDefinitionIds[0], FName(TEXT("Enemy.Ch1.Rooster")));
				TestEqual(TEXT("elite wave primary enemy occupies the center"), Encounter.EnemyDefinitionIds[1], Encounter.EnemyDefinitionId);
				TestEqual(TEXT("elite wave keeps civet on the right flank"), Encounter.EnemyDefinitionIds[2], FName(TEXT("Enemy.Ch1.Civet")));
			}
		}
	}
	TestEqual(TEXT("route exposes two sub-elite encounters"), EliteCount, 2);
	TestTrue(TEXT("route ends with a boss encounter"), Encounters.Last().Kind == EGameXXKTrainingEncounterKind::Boss);
	TestEqual(TEXT("the 1-1 elite-as-boss wave fills all three slots"), Encounters.Last().EnemyDefinitionIds.Num(), 3);
	if (Encounters.Last().EnemyDefinitionIds.Num() == 3)
	{
		TestEqual(TEXT("the 1-1 boss wave keeps rooster on the left"), Encounters.Last().EnemyDefinitionIds[0], FName(TEXT("Enemy.Ch1.Rooster")));
		TestEqual(TEXT("the 1-1 goat boss occupies the center"), Encounters.Last().EnemyDefinitionIds[1], FName(TEXT("Enemy.Ch1.Goat")));
		TestEqual(TEXT("the 1-1 boss wave keeps civet on the right"), Encounters.Last().EnemyDefinitionIds[2], FName(TEXT("Enemy.Ch1.Civet")));
	}
	TestTrue(TEXT("challenge 1-1 keeps normal combat health"), Encounters[0].BaseHealth > 1);
	TestEqual(TEXT("travel 1-1 normal health matches the challenge encounter"), TravelEncounters[0].BaseHealth, Encounters[0].BaseHealth);
	TestEqual(TEXT("travel 1-1 boss health matches the challenge encounter"), TravelEncounters.Last().BaseHealth, Encounters.Last().BaseHealth);
	TestEqual(TEXT("1-1 boss tooltip identity is goat boss"), Definition.BossEnemyId, FName(TEXT("Enemy.Ch1.Goat")));
	const FName StageTwo = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 2);
	const FName StageThree = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 3);
	FGameXXKTrainingStageDefinition StageTwoDefinition;
	FGameXXKTrainingStageDefinition StageThreeDefinition;
	TestTrue(TEXT("1-2 definition exists"), FGameXXKTrainingRules::TryGetStageDefinition(StageTwo, StageTwoDefinition));
	TestTrue(TEXT("1-3 definition exists"), FGameXXKTrainingRules::TryGetStageDefinition(StageThree, StageThreeDefinition));
	TestEqual(TEXT("1-2 reuses the weasel as the boss"), StageTwoDefinition.BossEnemyId, FName(TEXT("Enemy.Ch1.Weasel")));
	TestEqual(TEXT("1-3 uses the first-chapter boss identity"), StageThreeDefinition.BossEnemyId, FName(TEXT("Enemy.Ch1.BluehornGoatKing")));
	const TArray<FGameXXKTrainingEncounterDefinition> StageThreeEncounters = FGameXXKTrainingRules::BuildEncounterSequence(StageThree, false);
	TestEqual(TEXT("the true chapter-one boss wave fills all three slots"), StageThreeEncounters.Last().EnemyDefinitionIds.Num(), 3);
	if (StageThreeEncounters.Last().EnemyDefinitionIds.Num() == 3)
	{
		TestEqual(TEXT("bluehorn boss wave left flank is goat"), StageThreeEncounters.Last().EnemyDefinitionIds[0], FName(TEXT("Enemy.Ch1.Goat")));
		TestEqual(TEXT("bluehorn boss occupies the center"), StageThreeEncounters.Last().EnemyDefinitionIds[1], FName(TEXT("Enemy.Ch1.BluehornGoatKing")));
		TestEqual(TEXT("bluehorn boss wave right flank is weasel"), StageThreeEncounters.Last().EnemyDefinitionIds[2], FName(TEXT("Enemy.Ch1.Weasel")));
	}
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
	TestTrue(TEXT("1-1 Travel uses the ordinary guaranteed chest path"), TravelOneOneNormal.bChestRolled);
	TestEqual(TEXT("1-1 ordinary Travel resolves a normal chest"), TravelOneOneNormal.ChestTier, EGameXXKTrainingRewardTier::NormalChest);
	const FGameXXKTrainingReward TravelOneOneElite = FGameXXKTrainingRules::ResolveTravelReward(
		StageOne,
		EGameXXKTrainingEncounterKind::Elite,
		Seed,
		0,
		0,
		1.0f);
	TestTrue(TEXT("1-1 elite Travel uses the guaranteed advanced chest path"), TravelOneOneElite.bChestRolled);
	TestEqual(TEXT("1-1 elite Travel resolves an advanced chest"), TravelOneOneElite.ChestTier, EGameXXKTrainingRewardTier::AdvancedChest);
	const FGameXXKTrainingReward TravelOneOneBoss = FGameXXKTrainingRules::ResolveTravelReward(
		StageOne,
		EGameXXKTrainingEncounterKind::Boss,
		Seed,
		0,
		0,
		1.0f,
		true);
	TestTrue(TEXT("1-1 boss Travel uses the guaranteed advanced chest path"), TravelOneOneBoss.bChestRolled);
	TestEqual(TEXT("1-1 boss Travel resolves an advanced chest"), TravelOneOneBoss.ChestTier, EGameXXKTrainingRewardTier::AdvancedChest);
	TestEqual(TEXT("1-1 boss keeps its gold/experience stage reward"), TravelOneOneBoss.Gold, FGameXXKTrainingRules::BuildTravelReward(StageOne).Gold);
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
	const FGameXXKTrainingReward ChallengeNormalParity = FGameXXKTrainingRules::ResolveChallengeReward(
		FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 2),
		EGameXXKTrainingEncounterKind::Normal,
		Seed,
		0.10f);
	const FGameXXKTrainingReward TravelNormalParity = FGameXXKTrainingRules::ResolveTravelReward(
		FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 2),
		EGameXXKTrainingEncounterKind::Normal,
		Seed,
		0,
		0,
		0.10f,
		false);
	TestEqual(TEXT("Travel normal chest probability reuses the in-level resolver"), TravelNormalParity.bChestRolled, ChallengeNormalParity.bChestRolled);
	TestEqual(TEXT("Travel normal chest tier matches the in-level resolver"), TravelNormalParity.ChestTier, ChallengeNormalParity.ChestTier);
	const FGameXXKTrainingReward ChallengeEliteParity = FGameXXKTrainingRules::ResolveChallengeReward(
		FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 2),
		EGameXXKTrainingEncounterKind::Elite,
		Seed,
		0.10f);
	const FGameXXKTrainingReward TravelEliteParity = FGameXXKTrainingRules::ResolveTravelReward(
		FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 2),
		EGameXXKTrainingEncounterKind::Elite,
		Seed,
		0,
		0,
		0.10f,
		false);
	TestEqual(TEXT("Travel elite chest probability reuses the in-level resolver"), TravelEliteParity.bChestRolled, ChallengeEliteParity.bChestRolled);
	TestEqual(TEXT("Travel elite chest tier matches the in-level resolver"), TravelEliteParity.ChestTier, ChallengeEliteParity.ChestTier);
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

	UGameXXKMVPSubsystem* CurrentFixture = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("current cooldown fixture starts with a saveable party"), CurrentFixture && CurrentFixture->StartGame()))
	{
		return false;
	}
	FGameXXKSaveState CurrentSave = UGameXXKMVPRules::MakeSaveState(CurrentFixture->GetRuntimeStateCopy());
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
	TestTrue(TEXT("challenge victory immediately resumes travel"), Progress.bTravelActive);
	TestEqual(TEXT("challenge victory travels the newly cleared stage"), Progress.CurrentTravelStageId, StageTwo);
	TestEqual(TEXT("challenge victory selects the newly cleared stage"), Progress.SelectedStageId, StageTwo);
	TestTrue(TEXT("the current travel stage can deliberately be restarted"), FGameXXKTrainingRules::StartTravel(Progress, StageTwo));
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
	FGameXXKTrainingTravelEncounterSpawnResetTest,
	"GameXXK.Training.TravelEncounterSpawnReset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTrainingTravelEncounterSpawnResetTest::RunTest(const FString& Parameters)
{
	FGameXXKTrainingProgress Progress;
	FGameXXKTrainingRules::InitializeNewGame(Progress);
	const TArray<FGameXXKTrainingTravelPartyUnitRuntime> DamagedParty = {
		FGameXXKTrainingTravelPartyUnitRuntime(TEXT("Hero"), 40, 100, 100000),
		FGameXXKTrainingTravelPartyUnitRuntime(TEXT("Companion.Blade.Test"), 30, 90, 100000),
		FGameXXKTrainingTravelPartyUnitRuntime(TEXT("Npc.TusiChief"), 20, 80, 100000)};
	FGameXXKTrainingTravelRuntime Runner;
	TestTrue(TEXT("damaged party fixture initializes"),
		FGameXXKTrainingRules::InitializeTravelRunner(Progress, Runner, DamagedParty));
	TestEqual(TEXT("spawn delay is five logical seconds"),
		Runner.WalkStepsRequired, FGameXXKTrainingRules::TravelEncounterSpawnDelaySeconds);
	for (const FGameXXKTrainingTravelPartyUnitRuntime& Unit : Runner.PartyUnits)
	{
		TestEqual(TEXT("encounter materialization fully heals every party unit"), Unit.HP, Unit.MaxHP);
	}

	bool bEncounterCompleted = false;
	bool bStageCompleted = false;
	bool bDefeated = false;
	FGameXXKTrainingReward Reward;
	for (int32 Second = 1; Second < FGameXXKTrainingRules::TravelEncounterSpawnDelaySeconds; ++Second)
	{
		TestTrue(TEXT("walking second advances"), FGameXXKTrainingRules::AdvanceTravelRunner(
			Progress, Runner, bEncounterCompleted, bStageCompleted, bDefeated, Reward));
		TestEqual(TEXT("first four seconds remain Walking"), Runner.Phase, EGameXXKTrainingTravelPhase::Walking);
	}
	TestTrue(TEXT("fifth walking second advances"), FGameXXKTrainingRules::AdvanceTravelRunner(
		Progress, Runner, bEncounterCompleted, bStageCompleted, bDefeated, Reward));
	TestEqual(TEXT("fifth second enters Combat"), Runner.Phase, EGameXXKTrainingTravelPhase::Combat);

	for (FGameXXKTrainingTravelPartyUnitRuntime& Unit : Runner.PartyUnits)
	{
		Unit.HP = 1;
	}
	for (int32 Guard = 0; Guard < 8 && !bEncounterCompleted; ++Guard)
	{
		TestTrue(TEXT("high-attack party settles the current encounter"), FGameXXKTrainingRules::AdvanceTravelRunner(
			Progress, Runner, bEncounterCompleted, bStageCompleted, bDefeated, Reward));
	}
	TestTrue(TEXT("the current encounter settles within the bounded guard"), bEncounterCompleted);
	for (const FGameXXKTrainingTravelPartyUnitRuntime& Unit : Runner.PartyUnits)
	{
		TestEqual(TEXT("next encounter fully heals every party unit"), Unit.HP, Unit.MaxHP);
	}
	for (const FGameXXKTrainingTravelEnemyRuntime& Enemy : Runner.Enemies)
	{
		TestEqual(TEXT("next encounter materializes every enemy at full health"), Enemy.HP, Enemy.MaxHP);
	}
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
	TestEqual(TEXT("ordinary travel runner begins with the two-enemy formation"), Runner.Enemies.Num(), 2);
	TestEqual(TEXT("ordinary travel runner targets the first living slot"), Runner.ActiveEnemyIndex, 0);

	bool bEncounterCompleted = false;
	bool bStageCompleted = false;
	bool bDefeated = false;
	FGameXXKTrainingReward Reward;
	for (int32 Second = 1; Second < FGameXXKTrainingRules::TravelEncounterSpawnDelaySeconds; ++Second)
	{
		TestTrue(TEXT("walking step advances"), FGameXXKTrainingRules::AdvanceTravelRunner(
			Progress, Runner, bEncounterCompleted, bStageCompleted, bDefeated, Reward));
		TestFalse(TEXT("walking does not complete an encounter"), bEncounterCompleted);
		TestEqual(TEXT("first four walk steps remain walking"), Runner.Phase, EGameXXKTrainingTravelPhase::Walking);
	}
	TestTrue(TEXT("fifth walk step reaches the encounter"), FGameXXKTrainingRules::AdvanceTravelRunner(
		Progress, Runner, bEncounterCompleted, bStageCompleted, bDefeated, Reward));
	TestEqual(TEXT("fifth walk step enters combat"), Runner.Phase, EGameXXKTrainingTravelPhase::Combat);
	TestTrue(TEXT("first combat hit advances the ordinary wave"), FGameXXKTrainingRules::AdvanceTravelRunner(
		Progress, Runner, bEncounterCompleted, bStageCompleted, bDefeated, Reward));
	TestFalse(TEXT("killing only the first enemy does not settle the wave"), bEncounterCompleted);
	TestEqual(TEXT("ordinary wave advances to the second enemy"), Runner.ActiveEnemyIndex, 1);
	TestEqual(TEXT("the first enemy remains defeated in the formation snapshot"), Runner.Enemies[0].HP, 0);
	TestTrue(TEXT("the second enemy remains alive until its own attack exchange"), Runner.Enemies[1].HP > 0);

	int32 CompletedEncounters = 0;
	// Bound the test by state completion, not by the retired one-hit 1-1 timing.
	for (int32 Guard = 0; Guard < 512 && !bStageCompleted; ++Guard)
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
			TestTrue(TEXT("every completed travel encounter settles gold"), Reward.Gold > 0);
			TestTrue(TEXT("every completed travel encounter settles experience"), Reward.Experience > 0);
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
	TestEqual(TEXT("completed stage runtime loops to encounter zero"), Runner.EncounterIndex, 0);
	TestEqual(TEXT("completed stage rematerializes the first two-enemy formation"), Runner.Enemies.Num(), 2);
	if (Runner.Enemies.Num() == 2)
	{
		TestEqual(TEXT("looped first formation starts with rooster"),
			Runner.Enemies[0].EnemyDefinitionId, FName(TEXT("Enemy.Ch1.Rooster")));
		TestEqual(TEXT("looped first formation continues with civet"),
			Runner.Enemies[1].EnemyDefinitionId, FName(TEXT("Enemy.Ch1.Civet")));
	}
	for (const FGameXXKTrainingTravelPartyUnitRuntime& PartyUnit : Runner.PartyUnits)
	{
		TestEqual(TEXT("looped first encounter fully heals every party unit"), PartyUnit.HP, PartyUnit.MaxHP);
	}
	for (const FGameXXKTrainingTravelEnemyRuntime& Enemy : Runner.Enemies)
	{
		TestEqual(TEXT("looped first encounter fully heals every enemy"), Enemy.HP, Enemy.MaxHP);
	}
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
	for (int32 Guard = 0;
		Guard <= FGameXXKTrainingRules::TravelEncounterSpawnDelaySeconds
			&& Runner.Phase != EGameXXKTrainingTravelPhase::Combat;
		++Guard)
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
	FGameXXKTrainingTravelThreeUnitPartyRuntimeTest,
	"GameXXK.Training.TravelThreeUnitPartyRuntime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTrainingTravelThreeUnitPartyRuntimeTest::RunTest(const FString& Parameters)
{
	FGameXXKTrainingProgress Progress;
	FGameXXKTrainingRules::InitializeNewGame(Progress);

	const TArray<FGameXXKTrainingTravelPartyUnitRuntime> Party = {
		FGameXXKTrainingTravelPartyUnitRuntime(TEXT("Hero"), 100, 100, 1),
		FGameXXKTrainingTravelPartyUnitRuntime(TEXT("Companion.Blade.Test"), 100, 100, 1),
		FGameXXKTrainingTravelPartyUnitRuntime(TEXT("Npc.TusiChief"), 100, 100, 1)};
	FGameXXKTrainingTravelRuntime Runner;
	TestTrue(TEXT("three-unit travel runner initializes"),
		FGameXXKTrainingRules::InitializeTravelRunner(Progress, Runner, Party));
	TestEqual(TEXT("travel runtime keeps all three configured party units"), Runner.PartyUnits.Num(), 3);
	TestEqual(TEXT("hero is the fixed first party unit"), Runner.PartyUnits[0].UnitId, FName(TEXT("Hero")));
	TestEqual(TEXT("Blade companion occupies the second party unit"), Runner.PartyUnits[1].UnitId, FName(TEXT("Companion.Blade.Test")));
	TestEqual(TEXT("Tusi Chief occupies the third party unit"), Runner.PartyUnits[2].UnitId, FName(TEXT("Npc.TusiChief")));

	bool bEncounterCompleted = false;
	bool bStageCompleted = false;
	bool bDefeated = false;
	FGameXXKTrainingReward Reward;
	for (int32 WalkGuard = 0;
		WalkGuard <= FGameXXKTrainingRules::TravelEncounterSpawnDelaySeconds
			&& Runner.Phase != EGameXXKTrainingTravelPhase::Combat;
		++WalkGuard)
	{
		TestTrue(TEXT("party fixture reaches combat"), FGameXXKTrainingRules::AdvanceTravelRunner(
			Progress, Runner, bEncounterCompleted, bStageCompleted, bDefeated, Reward));
	}
	TestEqual(TEXT("party fixture enters combat"), Runner.Phase, EGameXXKTrainingTravelPhase::Combat);

	const int32 HeroHPBefore = Runner.PartyUnits[0].HP;
	TestTrue(TEXT("hero performs the first party action"), FGameXXKTrainingRules::AdvanceTravelRunner(
		Progress, Runner, bEncounterCompleted, bStageCompleted, bDefeated, Reward));
	TestEqual(TEXT("first action belongs to hero"), Runner.LastAttackingPartyIndex, 0);
	TestEqual(TEXT("enemy waits until the whole party has acted"), Runner.LastDamagedPartyIndex, INDEX_NONE);
	TestEqual(TEXT("first action does not damage hero"), Runner.PartyUnits[0].HP, HeroHPBefore);

	TestTrue(TEXT("Blade performs the second party action"), FGameXXKTrainingRules::AdvanceTravelRunner(
		Progress, Runner, bEncounterCompleted, bStageCompleted, bDefeated, Reward));
	TestEqual(TEXT("second action belongs to Blade"), Runner.LastAttackingPartyIndex, 1);
	TestEqual(TEXT("enemy still waits for Tusi Chief"), Runner.LastDamagedPartyIndex, INDEX_NONE);

	TestTrue(TEXT("Tusi Chief performs the third party action"), FGameXXKTrainingRules::AdvanceTravelRunner(
		Progress, Runner, bEncounterCompleted, bStageCompleted, bDefeated, Reward));
	TestEqual(TEXT("third action belongs to Tusi Chief"), Runner.LastAttackingPartyIndex, 2);
	TestEqual(TEXT("enemy retaliates against the deterministic first target"), Runner.LastDamagedPartyIndex, 0);
	TestEqual(TEXT("enemy retaliation updates the hero runtime HP"), Runner.PartyUnits[0].HP, HeroHPBefore - Runner.EnemyAttack);
	TestEqual(TEXT("legacy hero HP mirror follows party slot zero"), Runner.PlayerHP, Runner.PartyUnits[0].HP);
	TestFalse(TEXT("one damaged member is not a party defeat"), bDefeated);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTrainingTravelPartyDefeatRequiresAllUnitsTest,
	"GameXXK.Training.TravelPartyDefeatRequiresAllUnits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTrainingTravelPartyDefeatRequiresAllUnitsTest::RunTest(const FString& Parameters)
{
	FGameXXKTrainingProgress Progress;
	FGameXXKTrainingRules::InitializeNewGame(Progress);
	const TArray<FGameXXKTrainingTravelPartyUnitRuntime> Party = {
		FGameXXKTrainingTravelPartyUnitRuntime(TEXT("Hero"), 1, 1, 1),
		FGameXXKTrainingTravelPartyUnitRuntime(TEXT("Companion.Blade.Test"), 1, 1, 1),
		FGameXXKTrainingTravelPartyUnitRuntime(TEXT("Npc.TusiChief"), 1, 1, 1)};
	FGameXXKTrainingTravelRuntime Runner;
	TestTrue(TEXT("one-health three-unit runner initializes"),
		FGameXXKTrainingRules::InitializeTravelRunner(Progress, Runner, Party));

	bool bEncounterCompleted = false;
	bool bStageCompleted = false;
	bool bDefeated = false;
	FGameXXKTrainingReward Reward;
	for (int32 Guard = 0; Guard < 32 && !bDefeated; ++Guard)
	{
		TestTrue(TEXT("one-health party runner advances"), FGameXXKTrainingRules::AdvanceTravelRunner(
			Progress, Runner, bEncounterCompleted, bStageCompleted, bDefeated, Reward));
		if (Runner.PartyUnits[0].HP == 0 && (Runner.PartyUnits[1].HP > 0 || Runner.PartyUnits[2].HP > 0))
		{
			TestFalse(TEXT("hero death alone does not defeat the party"), bDefeated);
		}
	}
	TestTrue(TEXT("runner reports defeat only after all three party members die"), bDefeated);
	TestTrue(TEXT("every configured party member is defeated"),
		Runner.PartyUnits.ContainsByPredicate([](const FGameXXKTrainingTravelPartyUnitRuntime& Unit)
		{
			return Unit.HP > 0;
		}) == false);
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
	for (int32 Guard = 0; Guard < 512 && !bStageCompleted; ++Guard)
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
	FGameXXKTrainingTravelDefaultPartyBridgeTest,
	"GameXXK.Training.TravelDefaultPartyBridge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTrainingTravelDefaultPartyBridgeTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestNotNull(TEXT("default travel party subsystem exists"), Subsystem);
	if (!Subsystem || !TestTrue(TEXT("default travel party starts a new game"), Subsystem->StartGame()))
	{
		return false;
	}

	const FGameXXKRuntimeState& State = Subsystem->GetRuntimeState();
	const FName ActiveCompanionId = State.CardRun.PartySelection.ActivePermanentCompanionInstanceId;
	const FGameXXKPermanentCompanion* ActiveCompanion =
		State.CardRun.CompanionRoster.PermanentCompanions.FindByPredicate(
			[ActiveCompanionId](const FGameXXKPermanentCompanion& Candidate)
			{
				return Candidate.InstanceId == ActiveCompanionId && Candidate.bIsActive;
			});
	TestNotNull(TEXT("new game selects one active permanent companion"), ActiveCompanion);
	if (!ActiveCompanion)
	{
		return false;
	}
	TestEqual(TEXT("new-game default permanent companion is Blade"), ActiveCompanion->Role, EGameXXKCharacterRole::Blade);
	TestEqual(TEXT("new-game default NPC is Tusi Chief"),
		State.CardRun.ActiveTemporaryQuestNpcId, FName(TEXT("Npc.TusiChief")));

	const FName StageOne = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1);
	TestTrue(TEXT("default three-unit party starts 1-1 travel"), Subsystem->StartTrainingTravel(StageOne));
	const FGameXXKTrainingTravelRuntime Runtime = Subsystem->GetTrainingTravelRuntimeCopy();
	TestEqual(TEXT("subsystem materializes hero, Blade, and Tusi Chief"), Runtime.PartyUnits.Num(), 3);
	if (Runtime.PartyUnits.Num() != 3)
	{
		return false;
	}
	TestEqual(TEXT("subsystem party slot zero is the fixed hero"),
		Runtime.PartyUnits[0].UnitId, FGameXXKEquipmentRules::HeroCharacterId());
	TestEqual(TEXT("subsystem party slot one uses the selected Blade instance"),
		Runtime.PartyUnits[1].UnitId, ActiveCompanionId);
	TestEqual(TEXT("subsystem party slot two uses Tusi Chief"),
		Runtime.PartyUnits[2].UnitId, FName(TEXT("Npc.TusiChief")));
	for (const FGameXXKTrainingTravelPartyUnitRuntime& Unit : Runtime.PartyUnits)
	{
		TestTrue(TEXT("each materialized party member has real maximum HP"), Unit.MaxHP > 1);
		TestTrue(TEXT("each materialized party member has real attack"), Unit.Attack > 0);
	}
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
	TestTrue(TEXT("challenge opens its route map instead of skipping to battle"),
		Subsystem->IsTrainingChallengeRouteMapActive());
	TestEqual(TEXT("challenge route map enters the DungeonMap screen"),
		Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
	TestTrue(TEXT("challenge route map uses the generated multi-node map"),
		Subsystem->GetRuntimeState().RouteMapNodes.Num() > 1);
	TestTrue(TEXT("the generated Start node is reachable first"),
		Subsystem->GetRuntimeState().ReachableRouteNodeIds.Contains(0));
	TestTrue(TEXT("selecting the generated Start node advances the map"),
		Subsystem->SelectRouteNodeById(0));
	const FGameXXKRuntimeState& MapState = Subsystem->GetRuntimeState();
	int32 BattleNodeId = INDEX_NONE;
	for (const int32 ReachableNodeId : MapState.ReachableRouteNodeIds)
	{
		const FGameXXKRouteMapNode* Node = MapState.RouteMapNodes.FindByPredicate(
			[ReachableNodeId](const FGameXXKRouteMapNode& Candidate)
			{
				return Candidate.NodeId == ReachableNodeId;
			});
		if (Node && (Node->NodeKind == EGameXXKNodeKind::Battle || Node->NodeKind == EGameXXKNodeKind::Elite))
		{
			BattleNodeId = Node->NodeId;
			break;
		}
	}
	TestTrue(TEXT("the generated map exposes a reachable battle node"), BattleNodeId != INDEX_NONE);
	TestTrue(TEXT("selecting the reachable battle node opens the authored battle"),
		Subsystem->SelectRouteNodeById(BattleNodeId));
	TestTrue(TEXT("training challenge owns a real card battle"), Subsystem->IsTrainingChallengeBattleActive());
	TestEqual(TEXT("training battle enters the real Battle screen"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Battle);
	const int32 EncounterIndex = Subsystem->GetRuntimeState().Training.ActiveChallengeEncounterIndex;
	const TArray<FGameXXKTrainingEncounterDefinition> EncounterSequence =
		FGameXXKTrainingRules::BuildEncounterSequence(StageTwo, false);
	TestTrue(TEXT("the battle node maps to an authored encounter"),
		EncounterSequence.IsValidIndex(EncounterIndex));
	const int32 EnemyCount = Subsystem->GetRuntimeState().ActiveBattleEnemies.Num();
	TestEqual(TEXT("the first authored challenge wave contains two enemies"), EnemyCount, 2);
	if (EnemyCount != 2)
	{
		return false;
	}
	const TArray<FName> ExpectedFormation = EncounterSequence[EncounterIndex].EnemyDefinitionIds;
	TestEqual(TEXT("training battle uses the authored left enemy"), Subsystem->GetRuntimeState().ActiveBattleEnemies[0].EnemyDefinitionId, ExpectedFormation[0]);
	TestEqual(TEXT("training battle uses the authored right enemy"), Subsystem->GetRuntimeState().ActiveBattleEnemies[1].EnemyDefinitionId, ExpectedFormation[1]);

	bool bStageCompleted = false;
	FGameXXKTrainingReward Reward;
	TestTrue(TEXT("one auto step advances the real card runtime"), Subsystem->AdvanceTrainingChallengeEncounter(bStageCompleted, Reward));
	TestTrue(TEXT("training challenge remains active until the encounter is terminal"), Subsystem->GetRuntimeState().Training.bChallengeActive);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTrainingDirectBattleLoopSettlementTest,
	"GameXXK.Training.DirectBattleLoopSettlement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTrainingDirectBattleLoopSettlementTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("direct-loop fixture starts in Town"), Subsystem->StartGame()))
	{
		return false;
	}

	const FName StageId = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 2);
	const TArray<FGameXXKTrainingEncounterDefinition> Encounters =
		FGameXXKTrainingRules::BuildEncounterSequence(StageId, false);
	if (!TestTrue(TEXT("direct-loop stage has multiple authored encounters"), Encounters.Num() > 1)
		|| !TestTrue(TEXT("direct-loop challenge starts"), Subsystem->StartTrainingChallenge(StageId)))
	{
		return false;
	}
	TestTrue(TEXT("starting a challenge opens the route map"),
		Subsystem->IsTrainingChallengeRouteMapActive());
	TestTrue(TEXT("the player selects the generated Start node"),
		Subsystem->SelectRouteNodeById(0));

	const auto FindReachableNode = [&](const EGameXXKNodeKind Kind, int32& OutNodeId)
	{
		OutNodeId = INDEX_NONE;
		const FGameXXKRuntimeState& State = Subsystem->GetRuntimeState();
		for (const int32 NodeId : State.ReachableRouteNodeIds)
		{
			const FGameXXKRouteMapNode* Node = State.RouteMapNodes.FindByPredicate(
				[NodeId](const FGameXXKRouteMapNode& Candidate)
				{
					return Candidate.NodeId == NodeId;
				});
			if (Node && Node->NodeKind == Kind)
			{
				OutNodeId = NodeId;
				return true;
			}
		}
		return false;
	};
	const auto FindFirstBattleNode = [&](int32& OutNodeId)
	{
		const FGameXXKRuntimeState& State = Subsystem->GetRuntimeState();
		for (const int32 NodeId : State.ReachableRouteNodeIds)
		{
			const FGameXXKRouteMapNode* Node = State.RouteMapNodes.FindByPredicate(
				[NodeId](const FGameXXKRouteMapNode& Candidate)
				{
					return Candidate.NodeId == NodeId;
				});
			if (Node && (Node->NodeKind == EGameXXKNodeKind::Battle || Node->NodeKind == EGameXXKNodeKind::Elite))
			{
				OutNodeId = NodeId;
				return true;
			}
		}
		return false;
	};

	int32 FirstBattleNodeId = INDEX_NONE;
	if (!TestTrue(TEXT("the generated map exposes a reachable battle node"),
		FindFirstBattleNode(FirstBattleNodeId))
		|| !TestTrue(TEXT("the player selects the first reachable battle node"),
			Subsystem->SelectRouteNodeById(FirstBattleNodeId)))
	{
		return false;
	}

	bool bStageCompleted = false;
	FGameXXKTrainingReward Reward;
	FString RewardError;
	Subsystem->GetMutableRuntimeState().CardRun.ActiveBattle.Phase = EGameXXKCardBattlePhase::Victory;
	TestTrue(TEXT("first terminal victory opens the standard tiered reward offer"),
		Subsystem->AdvanceTrainingChallengeEncounter(bStageCompleted, Reward));
	TestEqual(TEXT("the first victory opens three standard reward choices"),
		Subsystem->GetRuntimeState().CardRun.PendingReward.Options.Num(), 3);
	TestFalse(TEXT("the reward offer does not yet complete the battle node"),
		bStageCompleted);
	TestTrue(TEXT("choosing the first standard reward settles the node"),
		Subsystem->ResolvePendingBattleRewardChoiceAndFinish(0, NAME_None, &RewardError));
	TestEqual(TEXT("first victory returns to the route map instead of chaining the next battle"),
		Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
	TestTrue(TEXT("first victory leaves the challenge route map active"),
		Subsystem->IsTrainingChallengeRouteMapActive());
	TestFalse(TEXT("first victory does not auto-create the next CardBattle"),
		Subsystem->IsTrainingChallengeBattleActive());
	TestTrue(TEXT("first victory marks the selected route node as visited"),
		Subsystem->GetRuntimeState().VisitedRouteNodeIds.Contains(FirstBattleNodeId));
	TestTrue(TEXT("first victory advances the generated map cursor"),
		Subsystem->GetRuntimeState().CurrentRouteNodeId != INDEX_NONE);

	// The player picks another authored battle node and the standard reward
	// flow settles it as well.
	int32 SecondBattleNodeId = INDEX_NONE;
	if (!TestTrue(TEXT("a second battle node is reachable"), FindFirstBattleNode(SecondBattleNodeId))
		|| !TestTrue(TEXT("the player selects the second reachable battle node"),
			Subsystem->SelectRouteNodeById(SecondBattleNodeId)))
	{
		return false;
	}
	Subsystem->GetMutableRuntimeState().CardRun.ActiveBattle.Phase = EGameXXKCardBattlePhase::Victory;
	TestTrue(TEXT("second victory opens its standard reward offer"),
		Subsystem->AdvanceTrainingChallengeEncounter(bStageCompleted, Reward));
	TestTrue(TEXT("the second reward choice settles the second node"),
		Subsystem->ResolvePendingBattleRewardChoiceAndFinish(0, NAME_None, &RewardError));
	TestEqual(TEXT("the second victory also returns to the route map"),
		Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);

	// Boss terminal victory completes the stage and returns to the workbench.
	int32 BossNodeId = INDEX_NONE;
	for (const FGameXXKRouteMapNode& Node : Subsystem->GetRuntimeState().RouteMapNodes)
	{
		if (Node.NodeKind == EGameXXKNodeKind::Boss)
		{
			BossNodeId = Node.NodeId;
			break;
		}
	}
	TestTrue(TEXT("the generated map contains a Boss node"), BossNodeId != INDEX_NONE);
	FGameXXKRuntimeState& BossState = Subsystem->GetMutableRuntimeState();
	BossState.ReachableRouteNodeIds = {BossNodeId};
	TestTrue(TEXT("the player selects the Boss node"),
		Subsystem->SelectRouteNodeById(BossNodeId));
	Subsystem->GetMutableRuntimeState().CardRun.ActiveBattle.Phase = EGameXXKCardBattlePhase::Victory;
	TestTrue(TEXT("Boss victory opens the standard Boss reward offer"),
		Subsystem->AdvanceTrainingChallengeEncounter(bStageCompleted, Reward));
	TestTrue(TEXT("choosing the Boss reward finishes the challenge"),
		Subsystem->ResolvePendingBattleRewardChoiceAndFinish(0, NAME_None, &RewardError));
	TestFalse(TEXT("Boss victory closes the challenge state"),
		Subsystem->GetRuntimeState().Training.bChallengeActive);
	TestTrue(TEXT("Boss victory clears the selected stage"),
		FGameXXKTrainingRules::IsStageCleared(Subsystem->GetRuntimeState().Training, StageId));
	TestEqual(TEXT("completed challenge returns to the workbench screen state"),
		Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);
	TestEqual(TEXT("completed challenge targets the desktop workbench projection"),
		Subsystem->GetRuntimeState().CurrentMapId, FName(TEXT("DesktopTrainingHUD")));
	TestFalse(TEXT("completed challenge clears its live CardBattle"),
		Subsystem->GetRuntimeState().CardRun.bHasActiveCardBattle);
	TestFalse(TEXT("completed challenge closes Training challenge state"),
		Subsystem->GetRuntimeState().Training.bChallengeActive);
	TestFalse(TEXT("completed challenge discards the generated route map"),
		Subsystem->GetRuntimeState().bHasGeneratedRouteMap);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTrainingChallengeCancelToWorkbenchTest,
	"GameXXK.Training.ChallengeCancelToWorkbench",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTrainingChallengeCancelToWorkbenchTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("cancel fixture starts in Town"), Subsystem->StartGame()))
	{
		return false;
	}
	const FName StageId = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 2);
	const EGameXXKQuestState QuestBefore = Subsystem->GetRuntimeState().QuestState;
	TestFalse(TEXT("cancel fixture stage starts uncleared"),
		FGameXXKTrainingRules::IsStageCleared(Subsystem->GetRuntimeState().Training, StageId));
	if (!TestTrue(TEXT("cancel fixture starts the direct challenge"), Subsystem->StartTrainingChallenge(StageId)))
	{
		return false;
	}

	TestTrue(TEXT("active challenge can be cancelled back to the workbench"),
		Subsystem->CancelTrainingChallengeToWorkbench());
	const FGameXXKRuntimeState& Cancelled = Subsystem->GetRuntimeState();
	TestEqual(TEXT("cancel returns to the workbench screen state"), Cancelled.Screen, EGameXXKScreen::Town);
	TestEqual(TEXT("cancel targets the desktop workbench projection"),
		Cancelled.CurrentMapId, FName(TEXT("DesktopTrainingHUD")));
	TestFalse(TEXT("cancel clears the live CardBattle"), Cancelled.CardRun.bHasActiveCardBattle);
	TestFalse(TEXT("cancel clears legacy battle projection"), Cancelled.bHasActiveBattle);
	TestFalse(TEXT("cancel closes Training challenge state"), Cancelled.Training.bChallengeActive);
	TestFalse(TEXT("cancel discards the generated challenge route map"), Cancelled.bHasGeneratedRouteMap);
	TestEqual(TEXT("cancel clears challenge route node tracking"),
		Cancelled.Training.ActiveChallengeRouteNodeId, INDEX_NONE);
	TestTrue(TEXT("cancel empties the challenge route node list"), Cancelled.RouteMapNodes.IsEmpty());
	TestFalse(TEXT("cancel does not award a stage clear"),
		FGameXXKTrainingRules::IsStageCleared(Cancelled.Training, StageId));
	TestTrue(TEXT("cancel resumes the previous Travel loop"), Cancelled.Training.bTravelActive);
	TestEqual(TEXT("cancel keeps the previous Travel target"),
		Cancelled.Training.CurrentTravelStageId,
		FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1));
	const FGameXXKTrainingTravelRuntime CancelledTravel = Subsystem->GetTrainingTravelRuntimeCopy();
	TestEqual(TEXT("cancel rebuilds the previous Travel runtime"),
		CancelledTravel.StageId,
		Cancelled.Training.CurrentTravelStageId);
	TestEqual(TEXT("cancel restarts at the walking delay"),
		CancelledTravel.Phase,
		EGameXXKTrainingTravelPhase::Walking);
	TestEqual(TEXT("cancel never changes the town quest"), Cancelled.QuestState, QuestBefore);
	TestFalse(TEXT("an already-cancelled challenge cannot be cancelled twice"),
		Subsystem->CancelTrainingChallengeToWorkbench());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTrainingChallengePendingChoiceAutoBattleTest,
	"GameXXK.Training.ChallengePendingChoiceAutoBattle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTrainingChallengePendingChoiceAutoBattleTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestNotNull(TEXT("pending-choice challenge fixture subsystem exists"), Subsystem);
	if (!Subsystem || !Subsystem->StartGame())
	{
		return false;
	}

	const FName StageTwo = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 2);
	TestTrue(TEXT("pending-choice challenge fixture starts an unlocked stage"), Subsystem->StartTrainingChallenge(StageTwo));
	TestTrue(TEXT("pending-choice fixture selects the generated Start node"),
		Subsystem->SelectRouteNodeById(0));
	const FGameXXKRuntimeState& MapState = Subsystem->GetRuntimeState();
	int32 BattleNodeId = INDEX_NONE;
	for (const int32 NodeId : MapState.ReachableRouteNodeIds)
	{
		const FGameXXKRouteMapNode* Node = MapState.RouteMapNodes.FindByPredicate(
			[NodeId](const FGameXXKRouteMapNode& Candidate)
			{
				return Candidate.NodeId == NodeId;
			});
		if (Node && (Node->NodeKind == EGameXXKNodeKind::Battle || Node->NodeKind == EGameXXKNodeKind::Elite))
		{
			BattleNodeId = Node->NodeId;
			break;
		}
	}
	TestTrue(TEXT("pending-choice fixture finds a reachable battle node"), BattleNodeId != INDEX_NONE);
	TestTrue(TEXT("pending-choice challenge fixture enters the first battle node"),
		Subsystem->SelectRouteNodeById(BattleNodeId));
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	FGameXXKBattleDeckState& Deck = State.CardRun.ActiveBattle.Deck;
	if (!TestTrue(TEXT("pending-choice challenge fixture has a hand to discard from"), !Deck.Hand.IsEmpty()))
	{
		return false;
	}

	Deck.PendingChoice = FGameXXKPendingCardChoice();
	Deck.PendingChoice.Kind = EGameXXKCardPendingChoiceKind::ForcedDiscard;
	Deck.PendingChoice.Candidates = Deck.Hand;
	Deck.PendingChoice.RequiredCount = 1;
	Deck.PendingChoice.RequiredDiscardCount = 1;
	Deck.PendingChoice.RequiredHandPickCount = 0;
	Deck.PendingChoice.bCanCancel = false;
	Deck.PendingChoice.bCancelPreservesDrawTop = true;

	bool bStageCompleted = false;
	FGameXXKTrainingReward Reward;
	TestTrue(TEXT("auto battle resolves a forced-discard choice and keeps advancing"),
		Subsystem->AdvanceTrainingChallengeEncounter(bStageCompleted, Reward));
	TestTrue(TEXT("forced-discard choice is no longer blocking after the auto step"),
		State.CardRun.ActiveBattle.Deck.PendingChoice.Kind != EGameXXKCardPendingChoiceKind::ForcedDiscard);
	TestTrue(TEXT("resolving a pending choice does not end the challenge fixture"), State.Training.bChallengeActive);
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
	State.PlayerLevel = 1;
	State.PlayerXP = 95;

	TestTrue(TEXT("travel chest inventory bridge starts a cleared stage"), Subsystem->StartTrainingTravel(StageTwo));
	const int32 NormalChestBefore = FGameXXKTrainingRules::CountChestTokens(State.Training, EGameXXKTrainingRewardTier::NormalChest);
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
	TestTrue(TEXT("online Travel settlement awards character experience"), Reward.Experience > 0);
	TestTrue(TEXT("online Travel character experience crosses the level-one threshold"),
		Subsystem->GetRuntimeState().PlayerLevel > 1);
	TestTrue(TEXT("online Travel leaves canonical residual character experience"),
		Subsystem->GetRuntimeState().PlayerXP
			< Subsystem->GetRuntimeState().PlayerLevel * 100);
	TestTrue(TEXT("travel chest inventory bridge reports the normal chest"), Reward.bChestRolled);
	TestEqual(TEXT("travel chest inventory bridge reports the canonical item"), Reward.ChestItemId, UGameXXKMVPRules::ItemTrainingNormalChest());
	TestEqual(TEXT("travel chest bridge appends one normal token"),
		FGameXXKTrainingRules::CountChestTokens(Subsystem->GetRuntimeState().Training, EGameXXKTrainingRewardTier::NormalChest),
		NormalChestBefore + 1);
	TestEqual(TEXT("normal Travel chest settlement starts the two-minute cooldown"),
		Subsystem->GetRuntimeState().Training.TravelNormalChestCooldownRemainingSeconds,
		FGameXXKTrainingRules::TravelNormalChestCooldownSeconds);
	TestEqual(TEXT("online chest never enters legacy inventory"),
		UGameXXKMVPRules::GetItemCount(Subsystem->GetRuntimeState(), UGameXXKMVPRules::ItemTrainingNormalChest()), 0);
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
	TestEqual(TEXT("travel chest token survives save round-trip"),
		FGameXXKTrainingRules::CountChestTokens(RoundTrip.RuntimeState.Training, EGameXXKTrainingRewardTier::NormalChest),
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
	TestTrue(TEXT("offline 1-1 normal chest count is a valid non-negative result"), SimulatedReward.NormalChestCount >= 0);
	TestTrue(TEXT("offline 1-1 advanced chest count is a valid non-negative result"), SimulatedReward.AdvancedChestCount >= 0);

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
	Subsystem->GetMutableRuntimeState().PlayerLevel = 1;
	Subsystem->GetMutableRuntimeState().PlayerXP = 95;
	TestTrue(TEXT("offline travel bridge starts 1-1"), Subsystem->StartTrainingTravel(StageOne));
	FGameXXKTrainingOfflineReward SimulatedReward;
	TestTrue(TEXT("offline travel bridge simulates a full-health 1-1 window"),
		Subsystem->SimulateTrainingTravelOffline(512, SimulatedReward));
	TestTrue(TEXT("offline travel bridge exposes pending gold"),
		Subsystem->GetPendingTrainingTravelRewardCopy().Gold > 0);
	TestTrue(TEXT("offline travel bridge exposes pending experience"),
		SimulatedReward.Experience > 0);

	const int32 GoldBeforeCollect = Subsystem->GetRuntimeState().PlayerGold;
	FGameXXKTrainingOfflineReward CollectedReward;
	TestTrue(TEXT("offline travel bridge collects pending rewards"),
		Subsystem->CollectTrainingTravelRewards(CollectedReward));
	TestEqual(TEXT("offline collect returns the simulated gold"), CollectedReward.Gold, SimulatedReward.Gold);
	TestEqual(TEXT("offline collect writes gold to the runtime inventory"),
		Subsystem->GetRuntimeState().PlayerGold,
		GoldBeforeCollect + SimulatedReward.Gold);
	TestTrue(TEXT("offline collect levels the character"),
		Subsystem->GetRuntimeState().PlayerLevel > 1);
	TestTrue(TEXT("offline collect leaves canonical residual character experience"),
		Subsystem->GetRuntimeState().PlayerXP
			< Subsystem->GetRuntimeState().PlayerLevel * 100);
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
	UGameXXKMVPSubsystem* Subsystem =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("save-validation fixture starts a complete current game"),
		Subsystem && Subsystem->StartGame()))
	{
		return false;
	}
	FGameXXKRuntimeState State = Subsystem->GetRuntimeStateCopy();
	FString Error;
	const bool bValidCurrentState = FGameXXKSaveMigration::ValidateRuntimeState(State, Error);
	TestTrue(
		FString::Printf(TEXT("new-game Training state validates: %s"), *Error),
		bValidCurrentState);

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
	SourceState.Training.TravelLastUpdatedUnixSeconds = FDateTime::UtcNow().ToUnixTimestamp() - 512;
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
