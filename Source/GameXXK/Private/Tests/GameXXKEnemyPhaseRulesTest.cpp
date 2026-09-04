#include "GameXXKCardTypes.h"
#include "GameXXKEnemyCatalog.h"
#include "GameXXKEnemyTypes.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEnemyPhaseDataContractTest,
	"GameXXK.Battle.EnemyPhase.DataContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEnemyPhaseDataContractTest::RunTest(const FString& Parameters)
{
	FGameXXKEnemyDifficultyInt Values;
	Values.Normal = 11;
	Values.Hard = 22;
	Values.Hell = 33;
	TestEqual(TEXT("Normal difficulty value resolves"), Values.Resolve(EGameXXKEnemyDifficulty::Normal), 11);
	TestEqual(TEXT("Hard difficulty value resolves"), Values.Resolve(EGameXXKEnemyDifficulty::Hard), 22);
	TestEqual(TEXT("Hell difficulty value resolves"), Values.Resolve(EGameXXKEnemyDifficulty::Hell), 33);

	TestEqual(TEXT("Normal Boss has one phase"),
		FGameXXKEnemyCatalog::ResolveTotalPhases(EGameXXKEnemyTier::Boss, EGameXXKEnemyDifficulty::Normal), 1);
	TestEqual(TEXT("Hard Elite has two phases"),
		FGameXXKEnemyCatalog::ResolveTotalPhases(EGameXXKEnemyTier::Elite, EGameXXKEnemyDifficulty::Hard), 2);
	TestEqual(TEXT("Hell Elite has three phases"),
		FGameXXKEnemyCatalog::ResolveTotalPhases(EGameXXKEnemyTier::Elite, EGameXXKEnemyDifficulty::Hell), 3);
	TestEqual(TEXT("Hell ordinary enemy remains one phase"),
		FGameXXKEnemyCatalog::ResolveTotalPhases(EGameXXKEnemyTier::Normal, EGameXXKEnemyDifficulty::Hell), 1);

	FGameXXKEnemyBattleState State;
	TestEqual(TEXT("enemy starts at phase one"), State.CurrentPhase, 1);
	TestEqual(TEXT("enemy defaults to one total phase"), State.TotalPhases, 1);
	TestEqual(TEXT("phase transition serial starts at zero"), State.PhaseTransitionSerial, 0);

	FGameXXKCardBattleRuntime Runtime;
	Runtime.EnemyDifficulty = EGameXXKEnemyDifficulty::Hard;
	Runtime.EnemyDifficultyDamagePercent = 125;
	TestEqual(TEXT("battle stores explicit enemy difficulty"), Runtime.EnemyDifficulty, EGameXXKEnemyDifficulty::Hard);
	return true;
}

#endif
