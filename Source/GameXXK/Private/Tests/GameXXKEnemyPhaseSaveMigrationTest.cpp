#include "MVP/GameXXKSaveMigration.h"

#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCardRules.h"
#include "GameXXKPermanentPartyTestFixtures.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEnemyPhaseSaveMigrationTest,
	"GameXXK.SaveMigration.EnemyPhase.Version35To36",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEnemyPhaseSaveMigrationTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = GameXXKPermanentPartyTestFixtures::MakeStartedState();
	FString Error;
	if (!TestTrue(TEXT("migration fixture initializes its card run"), FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error)))
	{
		AddError(Error);
		return false;
	}
	FGameXXKBattleRuntimeUnit Enemy;
	Enemy.Id = TEXT("BlackBear");
	Enemy.DisplayName = FText::FromString(TEXT("黑熊"));
	Enemy.HP = 100;
	Enemy.MaxHP = 100;
	Enemy.Attack = 20;
	Enemy.Defense = 12;
	Enemy.Speed = 7;
	Enemy.Shield = 0;
	Enemy.bEnemy = true;
	Enemy.EnemyDefinitionId = TEXT("Enemy.Ch2.BlackBear");
	Enemy.BattleSlotNumber = 1;
	Enemy.CombatLevel = 100;
	State.ActiveBattleEnemies = {Enemy};
	State.bHasActiveBattle = true;
	State.ActiveBattleNodeId = 9931;
	if (!TestTrue(TEXT("migration fixture begins a Hard battle"), FGameXXKCardBattleAdapter::BeginCardBattle(
		State,
		EGameXXKNodeKind::Boss,
		EGameXXKCardTerrain::Cave,
		9931,
		&Error,
		125)))
	{
		AddError(Error);
		return false;
	}
	FGameXXKCardCombatUnit* BlackBear = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("BlackBear");
	});
	if (!TestNotNull(TEXT("migration fixture exposes Black Bear"), BlackBear))
	{
		return false;
	}
	BlackBear->HP = 40;
	BlackBear->Armor = 17;
	BlackBear->Attack = 26;
	BlackBear->Defense = 9;
	GameXXKCardRules::AddCombatStatus(*BlackBear, EGameXXKCardStatus::Poison, 5);
	GameXXKCardRules::AddCombatStatus(*BlackBear, EGameXXKCardStatus::Rage, 2);
	FGameXXKEnemyBattleState& LegacyState = State.CardRun.ActiveBattle.EnemyStates.FindChecked(TEXT("BlackBear"));
	LegacyState.bPhaseTwo = true;
	LegacyState.CurrentPhase = 1;
	LegacyState.TotalPhases = 1;
	LegacyState.bPhaseStatModifiersApplied = true;
	LegacyState.PhaseAttackModifier = 6;
	LegacyState.PhaseDefenseModifier = -3;
	LegacyState.PendingChargedIntentId = TEXT("RetiredPhaseCharge");
	LegacyState.ChargeRoundsRemaining = 1;
	LegacyState.PendingChargeTargetUnitIds = {TEXT("Player")};
	State.CardRun.EnemyIntents.Reset();
	State.CardRun.ActiveBattle.LockedEnemyIntents.Reset();
	State.CardRun.ActiveBattleSourceNodeId = INDEX_NONE;
	State.CurrentRouteNodeId = INDEX_NONE;
	State.PendingRouteNodeId = INDEX_NONE;

	FGameXXKSaveState Source;
	Source.SaveVersion = 35;
	Source.RuntimeState = State;
	FGameXXKSaveState Migrated;
	FGameXXKSaveMigrationReport Report;
	if (!TestTrue(TEXT("version 35 enemy phase state migrates"), FGameXXKSaveMigration::MigrateToCurrent(Source, Migrated, Report)))
	{
		AddError(Report.Error);
		return false;
	}
	TestEqual(TEXT("migration writes the current version"), Migrated.SaveVersion, FGameXXKSaveMigration::CurrentSaveVersion);
	const FGameXXKCardBattleRuntime& Battle = Migrated.RuntimeState.CardRun.ActiveBattle;
	TestEqual(TEXT("difficulty enum is restored from 125 percent"), Battle.EnemyDifficulty, EGameXXKEnemyDifficulty::Hard);
	const FGameXXKEnemyBattleState& NewState = Battle.EnemyStates.FindChecked(TEXT("BlackBear"));
	TestEqual(TEXT("legacy phase-two flag becomes current phase two"), NewState.CurrentPhase, 2);
	TestEqual(TEXT("Hard Boss has two total phases"), NewState.TotalPhases, 2);
	TestTrue(TEXT("compatibility phase flag agrees"), NewState.bPhaseTwo);
	TestFalse(TEXT("old stat mutation flag is retired"), NewState.bPhaseStatModifiersApplied);
	TestEqual(TEXT("old attack modifier is cleared"), NewState.PhaseAttackModifier, 0);
	TestEqual(TEXT("old defense modifier is cleared"), NewState.PhaseDefenseModifier, 0);
	TestTrue(TEXT("invalid old-phase charge is cancelled"), NewState.PendingChargedIntentId.IsNone());
	const FGameXXKCardCombatUnit* MigratedBear = Battle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("BlackBear");
	});
	if (!TestNotNull(TEXT("migrated Black Bear remains present"), MigratedBear))
	{
		return false;
	}
	TestEqual(TEXT("migration removes the old phase attack mutation"), MigratedBear->Attack, 20);
	TestEqual(TEXT("migration removes the old phase defense mutation"), MigratedBear->Defense, 12);
	TestEqual(TEXT("migration does not heal"), MigratedBear->HP, 40);
	TestEqual(TEXT("migration preserves armor"), MigratedBear->Armor, 17);
	TestEqual(TEXT("migration preserves negative status without retroactive transition"), GameXXKCardRules::GetCombatStatusStacks(*MigratedBear, EGameXXKCardStatus::Poison), 5);
	TestEqual(TEXT("migration preserves positive status"), GameXXKCardRules::GetCombatStatusStacks(*MigratedBear, EGameXXKCardStatus::Rage), 2);
	TestEqual(TEXT("migration rebuilds one visible phase-two forecast"), Migrated.RuntimeState.CardRun.EnemyIntents.Num(), 1);
	if (!Migrated.RuntimeState.CardRun.EnemyIntents.IsEmpty())
	{
		TestEqual(TEXT("reforecast starts from phase two's first card"),
			Migrated.RuntimeState.CardRun.EnemyIntents[0].IntentDefinitionId,
			FName(TEXT("BloodClawRend")));
		TestEqual(TEXT("reforecast identifies phase two"), Migrated.RuntimeState.CardRun.EnemyIntents[0].PhaseNumber, 2);
	}
	return true;
}

#endif
