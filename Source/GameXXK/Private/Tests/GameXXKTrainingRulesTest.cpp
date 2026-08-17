#include "GameXXKTrainingRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKSaveMigration.h"

#include "Engine/GameInstance.h"

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
	TestEqual(TEXT("1-1 travel reward has no chest"), FGameXXKTrainingRules::BuildTravelReward(StageOne).ChestTier, EGameXXKTrainingRewardTier::None);
	const FGameXXKTrainingReward BossReward = FGameXXKTrainingRules::BuildChallengeReward(StageOne, EGameXXKTrainingEncounterKind::Boss, true);
	TestEqual(TEXT("boss reward uses advanced chest tier"), BossReward.ChestTier, EGameXXKTrainingRewardTier::AdvancedChest);
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
	TestEqual(TEXT("1-1 travel reward remains chest-free"), Reward.ChestTier, EGameXXKTrainingRewardTier::None);
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

#endif
