#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCardRules.h"
#include "GameXXKPermanentPartyTestFixtures.h"
#include "MVP/GameXXKSaveMigration.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGameXXKBattleRuntimeUnit MakeLegacyUnit(
		const TCHAR* UnitId,
		const TCHAR* DisplayName,
		const bool bEnemy)
	{
		FGameXXKBattleRuntimeUnit Unit;
		Unit.Id = FName(UnitId);
		Unit.DisplayName = FText::FromString(DisplayName);
		Unit.HP = bEnemy ? 300 : 200;
		Unit.MaxHP = Unit.HP;
		Unit.MP = bEnemy ? 0 : 30;
		Unit.MaxMP = Unit.MP;
		Unit.Attack = bEnemy ? 20 : 30;
		Unit.Defense = bEnemy ? 10 : 20;
		Unit.Speed = bEnemy ? 8 : 10;
		Unit.bEnemy = bEnemy;
		Unit.CombatLevel = bEnemy ? 105 : 100;
		return Unit;
	}

	TArray<FName> CollectInstanceIds(const TArray<FGameXXKCardInstance>& Zone)
	{
		TArray<FName> Result;
		for (const FGameXXKCardInstance& Instance : Zone)
		{
			Result.Add(Instance.InstanceId);
		}
		return Result;
	}

	FGameXXKCardCombatUnit* FindUnit(FGameXXKCardBattleRuntime& Runtime, const FName UnitId)
	{
		return Runtime.Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == UnitId;
		});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCombatScalingSaveMigrationTest,
	"GameXXK.SaveMigration.CombatScalingFoundationV33",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCombatScalingSaveMigrationTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("combat scaling foundation owns save version 33"),
		FGameXXKSaveMigration::CombatScalingFoundationIntroducedSaveVersion, 33);
	TestEqual(TEXT("combat scaling foundation is the current save schema"),
		FGameXXKSaveMigration::CurrentSaveVersion, 33);

	FGameXXKRuntimeState Runtime = GameXXKPermanentPartyTestFixtures::MakeStartedState();
	FString Error;
	if (!TestTrue(TEXT("v32 fixture initializes its card run"), FGameXXKCardBattleAdapter::EnsureCardRunInitialized(Runtime, &Error))
		|| !TestTrue(TEXT("v32 fixture selects its permanent NPC"),
			GameXXKPermanentPartyTestFixtures::SelectNpc(Runtime, TEXT("Npc.TusiChief"), &Error)))
	{
		AddError(Error);
		return false;
	}
	Runtime.ActiveBattleParty = {MakeLegacyUnit(TEXT("Player"), TEXT("Hero"), false)};
	Runtime.ActiveBattleEnemies = {MakeLegacyUnit(TEXT("Enemy.Migration"), TEXT("Migration Enemy"), true)};
	Runtime.bHasActiveBattle = true;
	Runtime.ActiveBattleNodeId = 3301;
	if (!TestTrue(TEXT("v32 fixture begins a real card battle"), FGameXXKCardBattleAdapter::BeginCardBattle(
		Runtime,
		EGameXXKNodeKind::Battle,
		EGameXXKCardTerrain::Plain,
		33013301,
		&Error)))
	{
		AddError(Error);
		return false;
	}
	// This fixture exercises a standalone battle rather than a generated route node.
	Runtime.CardRun.ActiveBattleSourceNodeId = INDEX_NONE;

	TArray<FGameXXKCardCombatUnit*> PartyUnits;
	for (FGameXXKCardCombatUnit& Unit : Runtime.CardRun.ActiveBattle.Units)
	{
		if (Unit.Side == EGameXXKCardTargetSide::Party)
		{
			PartyUnits.Add(&Unit);
		}
	}
	PartyUnits.Sort([](const FGameXXKCardCombatUnit& Left, const FGameXXKCardCombatUnit& Right)
	{
		return Left.StableSortOrder < Right.StableSortOrder;
	});
	if (!TestEqual(TEXT("v32 fixture has the approved three-person party"), PartyUnits.Num(), 3))
	{
		return false;
	}
	PartyUnits[0]->CombatLevel = 100;
	PartyUnits[1]->CombatLevel = 80;
	PartyUnits[2]->CombatLevel = 75;

	FGameXXKCardCombatUnit* Enemy = FindUnit(Runtime.CardRun.ActiveBattle, TEXT("Enemy.Migration"));
	if (!TestNotNull(TEXT("v32 fixture retains its enemy"), Enemy))
	{
		return false;
	}
	Enemy->Armor = 99;
	GameXXKCardRules::AddCombatStatus(*Enemy, EGameXXKCardStatus::Bleed, 7);
	GameXXKCardRules::AddCombatStatus(*Enemy, EGameXXKCardStatus::Poison, 11);
	GameXXKCardRules::AddCombatStatus(*Enemy, EGameXXKCardStatus::Burn, 13);
	GameXXKCardRules::AddCombatStatus(*Enemy, EGameXXKCardStatus::DamageOverTime, 17);

	FGameXXKSaveState Source = UGameXXKMVPRules::MakeSaveState(Runtime);
	Source.SaveVersion = 32;
	FGameXXKCardBattleRuntime& LegacyBattle = Source.RuntimeState.CardRun.ActiveBattle;
	LegacyBattle.TeamMaxLevelSnapshot = 0;
	LegacyBattle.EnemyDifficultyDamagePercent = 0;
	LegacyBattle.PendingNextRoundEnergyPenalty = 9;
	const FGameXXKSaveState SourceBefore = Source;
	const TArray<FName> DrawOrderBefore = CollectInstanceIds(LegacyBattle.Deck.DrawPile);
	const TArray<FName> HandOrderBefore = CollectInstanceIds(LegacyBattle.Deck.Hand);
	const TArray<FName> DiscardOrderBefore = CollectInstanceIds(LegacyBattle.Deck.DiscardPile);
	const int32 SharedEnergyBefore = LegacyBattle.Deck.SharedEnergy;
	const int32 DeckRandomStateBefore = LegacyBattle.Deck.CurrentRandomState;
	const int32 CombatRandomStateBefore = LegacyBattle.CombatRandomState;
	const int32 GoldBefore = Source.RuntimeState.PlayerGold;
	TMap<FName, FIntPoint> HealthAndManaBefore;
	for (const FGameXXKCardCombatUnit& Unit : LegacyBattle.Units)
	{
		HealthAndManaBefore.Add(Unit.UnitId, FIntPoint(Unit.HP, Unit.Mana));
	}

	FGameXXKSaveState Migrated;
	FGameXXKSaveMigrationReport Report;
	if (!TestTrue(TEXT("v32 combat scaling fixture migrates to current"),
		FGameXXKSaveMigration::MigrateToCurrent(Source, Migrated, Report)))
	{
		AddError(Report.Error);
		return false;
	}
	TestTrue(TEXT("migration does not mutate its v32 source"),
		FGameXXKSaveState::StaticStruct()->CompareScriptStruct(&Source, &SourceBefore, PPF_None));
	TestEqual(TEXT("migration reports v32 as its source"), Report.SourceVersion, 32);
	TestEqual(TEXT("migration reports v33 as its target"), Report.TargetVersion, 33);
	TestEqual(TEXT("migration writes v33"), Migrated.SaveVersion, 33);

	FGameXXKCardBattleRuntime& Battle = Migrated.RuntimeState.CardRun.ActiveBattle;
	TestEqual(TEXT("migration derives the highest party level once"), Battle.TeamMaxLevelSnapshot, 100);
	TestEqual(TEXT("legacy battles default to normal enemy damage"), Battle.EnemyDifficultyDamagePercent, 100);
	TestEqual(TEXT("legacy battles begin with no queued energy penalty"), Battle.PendingNextRoundEnergyPenalty, 0);
	TestEqual(TEXT("migration preserves shared energy without granting resources"), Battle.Deck.SharedEnergy, SharedEnergyBefore);
	TestEqual(TEXT("migration preserves deck RNG"), Battle.Deck.CurrentRandomState, DeckRandomStateBefore);
	TestEqual(TEXT("migration preserves combat RNG"), Battle.CombatRandomState, CombatRandomStateBefore);
	TestEqual(TEXT("migration preserves draw-pile order"), CollectInstanceIds(Battle.Deck.DrawPile), DrawOrderBefore);
	TestEqual(TEXT("migration preserves hand order"), CollectInstanceIds(Battle.Deck.Hand), HandOrderBefore);
	TestEqual(TEXT("migration preserves discard-pile order"), CollectInstanceIds(Battle.Deck.DiscardPile), DiscardOrderBefore);
	TestEqual(TEXT("migration preserves account currency"), Migrated.RuntimeState.PlayerGold, GoldBefore);

	FGameXXKCardCombatUnit* MigratedEnemy = FindUnit(Battle, TEXT("Enemy.Migration"));
	if (TestNotNull(TEXT("migrated battle retains its enemy"), MigratedEnemy))
	{
		TestEqual(TEXT("legacy Armor99 is preserved exactly"), MigratedEnemy->Armor, 99);
		TestEqual(TEXT("legacy Bleed7 is not re-scaled"), GameXXKCardRules::GetCombatStatusStacks(*MigratedEnemy, EGameXXKCardStatus::Bleed), 7);
		TestEqual(TEXT("legacy Poison11 is not re-scaled"), GameXXKCardRules::GetCombatStatusStacks(*MigratedEnemy, EGameXXKCardStatus::Poison), 11);
		TestEqual(TEXT("legacy Burn13 is not re-scaled"), GameXXKCardRules::GetCombatStatusStacks(*MigratedEnemy, EGameXXKCardStatus::Burn), 13);
		TestEqual(TEXT("legacy Rot17 is not re-scaled"), GameXXKCardRules::GetCombatStatusStacks(*MigratedEnemy, EGameXXKCardStatus::DamageOverTime), 17);
	}
	for (const FGameXXKCardCombatUnit& Unit : Battle.Units)
	{
		const FIntPoint* Before = HealthAndManaBefore.Find(Unit.UnitId);
		if (TestNotNull(FString::Printf(TEXT("%s keeps a health/mana baseline"), *Unit.UnitId.ToString()), Before))
		{
			TestEqual(FString::Printf(TEXT("%s health is not resolved during migration"), *Unit.UnitId.ToString()), Unit.HP, Before->X);
			TestEqual(FString::Printf(TEXT("%s mana is not granted during migration"), *Unit.UnitId.ToString()), Unit.Mana, Before->Y);
		}
	}
	TestTrue(TEXT("the migrated v33 active battle passes normal validation"),
		GameXXKCardRules::ValidateCardBattleRuntime(Battle, &Error));
	return true;
}

#endif
