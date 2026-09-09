#include "GameXXKTrainingRules.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTrainingGoldCurveTest,
	"GameXXK.Training.GoldCurve.NormalTenfoldAndContinuousDifficultyGrowth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTrainingGoldCurveTest::RunTest(const FString& Parameters)
{
	// Approved per-wave amounts: Normal x10, then 450 * 1.05^step across Hard/Hell.
	const int32 ExpectedGold[3][9] = {
		{210, 240, 270, 300, 330, 360, 390, 420, 450},
		{473, 496, 521, 547, 574, 603, 633, 665, 698},
		{733, 770, 808, 849, 891, 936, 982, 1031, 1083}
	};
	const int32 ExpectedExperience[3][9] = {
		{12, 14, 16, 18, 20, 22, 24, 26, 28},
		{22, 24, 26, 28, 30, 32, 34, 36, 38},
		{32, 34, 36, 38, 40, 42, 44, 46, 48}
	};
	const EGameXXKTrainingDifficulty Difficulties[] = {
		EGameXXKTrainingDifficulty::Normal,
		EGameXXKTrainingDifficulty::Hard,
		EGameXXKTrainingDifficulty::Hell
	};
	int32 PreviousGold = 0;
	for (int32 DifficultyIndex = 0; DifficultyIndex < UE_ARRAY_COUNT(Difficulties); ++DifficultyIndex)
	{
		for (int32 StageIndex = 0; StageIndex < 9; ++StageIndex)
		{
			const FName StageId = FGameXXKTrainingRules::MakeStageId(Difficulties[DifficultyIndex], StageIndex + 1);
			const FString Label = StageId.ToString();
			FGameXXKTrainingStageDefinition Stage;
			if (!TestTrue(Label + TEXT(" is an authored stage"),
				FGameXXKTrainingRules::TryGetStageDefinition(StageId, Stage)))
			{
				return false;
			}
			const FGameXXKTrainingReward TravelReward = FGameXXKTrainingRules::BuildTravelReward(StageId);
			TestEqual(Label + TEXT(" pays the approved per-wave gold"),
				TravelReward.Gold, ExpectedGold[DifficultyIndex][StageIndex]);
			TestEqual(Label + TEXT(" keeps its experience reward"),
				TravelReward.Experience, ExpectedExperience[DifficultyIndex][StageIndex]);
			TestEqual(Label + TEXT(" exposes the same gold in its stage definition"), Stage.TravelGold, TravelReward.Gold);
			TestTrue(Label + TEXT(" increases gold across stage and difficulty boundaries"), TravelReward.Gold > PreviousGold);
			PreviousGold = TravelReward.Gold;

			const FGameXXKTrainingReward ResolvedTravel = FGameXXKTrainingRules::ResolveTravelReward(
				StageId, EGameXXKTrainingEncounterKind::Boss,
				FGameXXKTrainingRules::DefaultChallengeRewardSeed(), 1, 1, 0.0f, true);
			TestEqual(Label + TEXT(" actual Travel settlement uses the stage gold"), ResolvedTravel.Gold, TravelReward.Gold);
			const FGameXXKTrainingReward ChallengeReward = FGameXXKTrainingRules::BuildChallengeReward(
				StageId, EGameXXKTrainingEncounterKind::Boss, false);
			TestEqual(Label + TEXT(" preserves the challenge double-gold relationship"), ChallengeReward.Gold, TravelReward.Gold * 2);
			TestEqual(Label + TEXT(" preserves the challenge experience reward"),
				ChallengeReward.Experience, ExpectedExperience[DifficultyIndex][StageIndex] * 2);
		}
	}
	return true;
}

#endif
