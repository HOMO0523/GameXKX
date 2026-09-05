#include "GameXXKTrainingRules.h"

#include "GameXXKEnemyCatalog.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTrainingFormationCatalogTest,
	"GameXXK.Training.FormationCatalog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTrainingFormationCatalogTest::RunTest(const FString& Parameters)
{
	const TArray<FGameXXKTrainingStageDefinition>& Stages = FGameXXKTrainingRules::GetStageDefinitions();
	TestEqual(TEXT("there are twenty-seven authored stages"), Stages.Num(), 27);
	TSet<FName> SeenEnemyIds;
	int32 FormationCount = 0;
	for (int32 StageIndex = 0; StageIndex < Stages.Num(); ++StageIndex)
	{
		const FGameXXKTrainingStageDefinition& Stage = Stages[StageIndex];
		TestEqual(Stage.StageId.ToString() + TEXT(" fixed combat level"), Stage.CombatLevel, (StageIndex + 1) * 5);
		TestEqual(Stage.StageId.ToString() + TEXT(" has four ordinary identities"), Stage.NormalEnemyPool.Num(), 4);
		TestEqual(Stage.StageId.ToString() + TEXT(" has two elite identities"), Stage.EliteEnemyPool.Num(), 2);
		const TArray<FGameXXKTrainingEncounterDefinition> Encounters = FGameXXKTrainingRules::BuildEncounterSequence(Stage.StageId);
		TestEqual(Stage.StageId.ToString() + TEXT(" has seven authored encounters"), Encounters.Num(), 7);
		for (int32 EncounterIndex = 0; EncounterIndex < Encounters.Num(); ++EncounterIndex)
		{
			const FGameXXKTrainingEncounterDefinition& Encounter = Encounters[EncounterIndex];
			++FormationCount;
			TestEqual(TEXT("encounter uses stage combat level"), Encounter.CombatLevel, Stage.CombatLevel);
			TestEqual(TEXT("encounter has three left-center-right IDs"), Encounter.EnemyDefinitionIds.Num(), 3);
			TestEqual(TEXT("encounter has three opening-intent slots"), Encounter.EnemySlots.Num(), 3);
			for (int32 SlotIndex = 0; SlotIndex < Encounter.EnemySlots.Num(); ++SlotIndex)
			{
				const FGameXXKTrainingEnemySlotDefinition& Slot = Encounter.EnemySlots[SlotIndex];
				TestEqual(TEXT("slot compatibility identity matches"), Slot.EnemyDefinitionId, Encounter.EnemyDefinitionIds[SlotIndex]);
				const FGameXXKEnemyDefinition* Definition = FGameXXKEnemyCatalog::Find(Slot.EnemyDefinitionId);
				TestNotNull(TEXT("slot enemy exists"), Definition);
				const TArray<FGameXXKEnemyIntentDefinition>* PhaseOne = Definition
					? FGameXXKEnemyCatalog::GetPhaseIntents(*Definition, 1)
					: nullptr;
				TestTrue(TEXT("opening intent exists in phase one"), PhaseOne && PhaseOne->ContainsByPredicate([&Slot](const FGameXXKEnemyIntentDefinition& Intent)
				{
					return Intent.Id == Slot.OpeningIntentId;
				}));
				SeenEnemyIds.Add(Slot.EnemyDefinitionId);
			}
			const EGameXXKTrainingEncounterKind ExpectedKind = EncounterIndex < 4
				? EGameXXKTrainingEncounterKind::Normal
				: EncounterIndex < 6 ? EGameXXKTrainingEncounterKind::Elite : EGameXXKTrainingEncounterKind::Boss;
			TestEqual(TEXT("encounter kind follows 4/2/1 order"), Encounter.Kind, ExpectedKind);
		}
	}
	TestEqual(TEXT("all 189 formations are materialized"), FormationCount, 189);
	TestEqual(TEXT("all twenty-one enemies appear"), SeenEnemyIds.Num(), 21);

	const FName HellThreeOne = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Hell, 7);
	FGameXXKTrainingStageDefinition HellThreeOneStage;
	TestTrue(TEXT("Hell 3-1 stage exists"), FGameXXKTrainingRules::TryGetStageDefinition(HellThreeOne, HellThreeOneStage));
	TestEqual(TEXT("Hell 3-1 is level 125"), HellThreeOneStage.CombatLevel, 125);
	const TArray<FGameXXKTrainingEncounterDefinition> HellThreeOneEncounters = FGameXXKTrainingRules::BuildEncounterSequence(HellThreeOne);
	if (TestEqual(TEXT("Hell 3-1 exposes seven encounters"), HellThreeOneEncounters.Num(), 7))
	{
		const FGameXXKTrainingEncounterDefinition& Boss = HellThreeOneEncounters[6];
		TestEqual(TEXT("Hell 3-1 boss left is Vulture"), Boss.EnemySlots[0].EnemyDefinitionId, FName(TEXT("Enemy.Ch3.Vulture")));
		TestEqual(TEXT("Hell 3-1 boss center is White Ape"), Boss.EnemySlots[1].EnemyDefinitionId, FName(TEXT("Enemy.Ch3.WhiteApe")));
		TestEqual(TEXT("Hell 3-1 boss right is Giant Toad"), Boss.EnemySlots[2].EnemyDefinitionId, FName(TEXT("Enemy.Ch3.GiantToad")));
		int64 RawPhaseHealth = 0;
		for (const FGameXXKTrainingEnemySlotDefinition& Slot : Boss.EnemySlots)
		{
			const FGameXXKEnemyDefinition* Definition = FGameXXKEnemyCatalog::Find(Slot.EnemyDefinitionId);
			const FGameXXKEnemyComputedStats Stats = FGameXXKEnemyCatalog::ComputeStats(Slot.EnemyDefinitionId, 125);
			RawPhaseHealth += static_cast<int64>(Stats.MaxHP)
				* (Definition ? FGameXXKEnemyCatalog::ResolveTotalPhases(Definition->Tier, EGameXXKEnemyDifficulty::Hell) : 1);
		}
		TestEqual(TEXT("Hell 3-1 boss formation raw phase health"), RawPhaseHealth, static_cast<int64>(11178));
	}

	const FName HellThreeThree = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Hell, 9);
	FGameXXKTrainingStageDefinition HellThreeThreeStage;
	TestTrue(TEXT("Hell 3-3 stage exists"), FGameXXKTrainingRules::TryGetStageDefinition(HellThreeThree, HellThreeThreeStage));
	TestEqual(TEXT("Hell 3-3 is level 135"), HellThreeThreeStage.CombatLevel, 135);
	return true;
}

#endif
