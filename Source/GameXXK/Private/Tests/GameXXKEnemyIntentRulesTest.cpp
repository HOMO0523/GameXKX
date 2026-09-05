#include "GameXXKCardBattleAdapter.h"

#include "GameXXKCardRules.h"
#include "GameXXKPermanentPartyTestFixtures.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGameXXKBattleRuntimeUnit IntentEnemy(
		const TCHAR* UnitId,
		const TCHAR* DefinitionId,
		const int32 Slot,
		const int32 Speed,
		const int32 Attack = 100,
		const int32 Defense = 50)
	{
		FGameXXKBattleRuntimeUnit Unit;
		Unit.Id = UnitId;
		Unit.DisplayName = FText::FromName(DefinitionId);
		Unit.HP = 1000;
		Unit.MaxHP = 1000;
		Unit.Attack = Attack;
		Unit.Defense = Defense;
		Unit.Speed = Speed;
		Unit.Shield = 0;
		Unit.bEnemy = true;
		Unit.EnemyDefinitionId = DefinitionId;
		Unit.BattleSlotNumber = Slot;
		Unit.CombatLevel = 100;
		return Unit;
	}

	bool BeginIntentBattle(
		FGameXXKRuntimeState& State,
		TArray<FGameXXKBattleRuntimeUnit> Enemies,
		FString& Error,
		const int32 DifficultyPercent = 150)
	{
		State = GameXXKPermanentPartyTestFixtures::MakeStartedState();
		State.PlayerLevel = 100;
		if (!FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error))
		{
			return false;
		}
		State.ActiveBattleEnemies = MoveTemp(Enemies);
		State.bHasActiveBattle = true;
		State.ActiveBattleNodeId = 9941;
		if (!FGameXXKCardBattleAdapter::BeginCardBattle(
			State,
			EGameXXKNodeKind::Elite,
			EGameXXKCardTerrain::Plain,
			9941,
			&Error,
			DifficultyPercent))
		{
			return false;
		}
		for (FGameXXKCardCombatUnit& Unit : State.CardRun.ActiveBattle.Units)
		{
			if (Unit.Side == EGameXXKCardTargetSide::Party)
			{
				Unit.MaxHP = 10000;
				Unit.HP = 10000;
				Unit.Defense = 0;
				Unit.CombatLevel = 100;
				Unit.bLiving = true;
			}
		}
		return true;
	}

	FGameXXKCardCombatUnit* IntentUnit(FGameXXKRuntimeState& State, const FName UnitId)
	{
		return State.CardRun.ActiveBattle.Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == UnitId;
		});
	}

	bool EnterEnemyPhase(FGameXXKRuntimeState& State, FString& Error)
	{
		State.CardRun.EnemyIntents.Reset();
		State.CardRun.NextEnemyIntentIndex = 0;
		TArray<FGameXXKCardDamageResult> EndResults;
		return FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, EndResults, &Error);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKApprovedBluehornRetentionTest,
	"GameXXK.Battle.EnemyIntentRules.Approved.BluehornRetentionByPhase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKApprovedBluehornRetentionTest::RunTest(const FString& Parameters)
{
	struct FCase { int32 Phase; int32 ExpectedArmor; };
	for (const FCase& Case : {FCase{1, 50}, {2, 75}, {3, 100}})
	{
		FString Error;
		FGameXXKRuntimeState State;
		if (!TestTrue(TEXT("Bluehorn fixture begins"), BeginIntentBattle(State, {
			IntentEnemy(TEXT("Bluehorn"), TEXT("Enemy.Ch1.BluehornGoatKing"), 1, 7)}, Error)))
		{
			AddError(Error);
			return false;
		}
		FGameXXKEnemyBattleState& EnemyState = State.CardRun.ActiveBattle.EnemyStates.FindChecked(TEXT("Bluehorn"));
		EnemyState.CurrentPhase = Case.Phase;
		EnemyState.TotalPhases = 3;
		EnemyState.bPhaseTwo = Case.Phase >= 2;
		State.CardRun.ActiveBattle.LockedEnemyIntents.Reset();
		IntentUnit(State, TEXT("Bluehorn"))->Armor = 100;
		if (!TestTrue(TEXT("Bluehorn reaches its enemy-phase armor boundary"), EnterEnemyPhase(State, Error)))
		{
			AddError(Error);
			return false;
		}
		TestEqual(TEXT("phase-specific armor retention is exact"), IntentUnit(State, TEXT("Bluehorn"))->Armor, Case.ExpectedArmor);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKApprovedMoneyRatInterestTest,
	"GameXXK.Battle.EnemyIntentRules.Approved.MoneyRatCompoundInterest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKApprovedMoneyRatInterestTest::RunTest(const FString& Parameters)
{
	FString Error;
	FGameXXKRuntimeState State;
	if (!TestTrue(TEXT("compound-interest fixture begins"), BeginIntentBattle(State, {
		IntentEnemy(TEXT("Rooster"), TEXT("Enemy.Ch1.Rooster"), 1, 12),
		IntentEnemy(TEXT("Goat"), TEXT("Enemy.Ch1.Goat"), 2, 11),
		IntentEnemy(TEXT("MoneyRat"), TEXT("Enemy.Ch1.MoneyRat"), 3, 10)}, Error)))
	{
		AddError(Error);
		return false;
	}
	FGameXXKEnemyBattleState& MoneyState = State.CardRun.ActiveBattle.EnemyStates.FindChecked(TEXT("MoneyRat"));
	MoneyState.CurrentPhase = 2;
	MoneyState.TotalPhases = 3;
	MoneyState.bPhaseTwo = true;
	FGameXXKCardCombatUnit* MoneyRat = IntentUnit(State, TEXT("MoneyRat"));
	GameXXKCardRules::ConsumeCombatStatus(*MoneyRat, EGameXXKCardStatus::Wealth, MAX_int32);
	for (FGameXXKCardCombatUnit& Unit : State.CardRun.ActiveBattle.Units)
	{
		if (Unit.Side == EGameXXKCardTargetSide::Party)
		{
			GameXXKCardRules::AddCombatStatus(Unit, EGameXXKCardStatus::Burn, 5);
		}
	}
	State.CardRun.ActiveBattle.LockedEnemyIntents = {
		{TEXT("Rooster"), TEXT("Peck"), 1, 1},
		{TEXT("Goat"), TEXT("Horn"), 1, 1},
		{TEXT("MoneyRat"), TEXT("CloseGateCollection"), 2, 1}};
	if (!TestTrue(TEXT("compound-interest enemy phase is forecast"), EnterEnemyPhase(State, Error)))
	{
		AddError(Error);
		return false;
	}
	FGameXXKCardEnemyIntent Resolved;
	TArray<FGameXXKCardDamageResult> Damage;
	bool bFinished = false;
	TestTrue(TEXT("first faster ally attacks"), FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, Resolved, Damage, bFinished, &Error));
	TestEqual(TEXT("first qualifying ally card grants one Wealth"), GameXXKCardRules::GetCombatStatusStacks(*IntentUnit(State, TEXT("MoneyRat")), EGameXXKCardStatus::Wealth), 1);
	TestTrue(TEXT("second faster ally attacks"), FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, Resolved, Damage, bFinished, &Error));
	TestEqual(TEXT("phase-two per-enemy-phase budget allows the second grant"), GameXXKCardRules::GetCombatStatusStacks(*IntentUnit(State, TEXT("MoneyRat")), EGameXXKCardStatus::Wealth), 2);
	TestEqual(TEXT("trigger budget is recorded"), State.CardRun.ActiveBattle.EnemyStates.FindChecked(TEXT("MoneyRat")).PhasePassiveTriggerCount, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKApprovedDeerCooldownFallbackTest,
	"GameXXK.Battle.EnemyIntentRules.Approved.DeerCooldownFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKApprovedDeerCooldownFallbackTest::RunTest(const FString& Parameters)
{
	FString Error;
	FGameXXKRuntimeState State;
	if (!TestTrue(TEXT("Deer cooldown fixture begins"), BeginIntentBattle(State, {
		IntentEnemy(TEXT("Deer"), TEXT("Enemy.Ch3.SpiralHornDeer"), 1, 11)}, Error)))
	{
		AddError(Error);
		return false;
	}
	FGameXXKEnemyBattleState& DeerState = State.CardRun.ActiveBattle.EnemyStates.FindChecked(TEXT("Deer"));
	DeerState.CurrentPhase = 2;
	DeerState.TotalPhases = 3;
	DeerState.bPhaseTwo = true;
	DeerState.IntentCursor = 2;
	DeerState.HealingCooldownRounds = 2;
	State.CardRun.ActiveBattle.LockedEnemyIntents.Reset();
	if (!TestTrue(TEXT("Deer cooldown forecast is built"), EnterEnemyPhase(State, Error)))
	{
		AddError(Error);
		return false;
	}
	if (!TestEqual(TEXT("one Deer intent is visible"), State.CardRun.EnemyIntents.Num(), 1))
	{
		return false;
	}
	TestEqual(TEXT("cooling phase-two heal deterministically falls back to Spiral Horn Intercepts"),
		State.CardRun.EnemyIntents[0].IntentDefinitionId,
		FName(TEXT("SpiralHornInterceptsHunt")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKApprovedChargeFallbackTest,
	"GameXXK.Battle.EnemyIntentRules.Approved.ChargeRetargetsInvalidLock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKApprovedChargeFallbackTest::RunTest(const FString& Parameters)
{
	FString Error;
	FGameXXKRuntimeState State;
	if (!TestTrue(TEXT("charge fallback fixture begins"), BeginIntentBattle(State, {
		IntentEnemy(TEXT("Goat"), TEXT("Enemy.Ch1.Goat"), 1, 6)}, Error)))
	{
		AddError(Error);
		return false;
	}
	FGameXXKEnemyBattleState& GoatState = State.CardRun.ActiveBattle.EnemyStates.FindChecked(TEXT("Goat"));
	GoatState.PendingChargedIntentId = TEXT("Charge");
	GoatState.ChargeRoundsRemaining = 0;
	GoatState.PendingChargeTargetUnitIds = {TEXT("Defeated.Target")};
	State.CardRun.ActiveBattle.LockedEnemyIntents.Reset();
	if (!TestTrue(TEXT("executing charge is reforecast"), EnterEnemyPhase(State, Error)))
	{
		AddError(Error);
		return false;
	}
	const FGameXXKCardEnemyIntent& Intent = State.CardRun.EnemyIntents[0];
	const FGameXXKResolvedEnemyIntentEffect* Direct = Intent.Effects.FindByPredicate([](const FGameXXKResolvedEnemyIntentEffect& Effect)
	{
		return Effect.Type == EGameXXKEnemyIntentEffectType::DirectDamage;
	});
	TestEqual(TEXT("saved charge identity remains Charge"), Intent.IntentDefinitionId, FName(TEXT("Charge")));
	TestTrue(TEXT("finished charge is ready to execute"), !Intent.bCharging);
	TestTrue(TEXT("invalid target lock is replaced by a living party target"), Direct && Direct->TargetUnitIds.Num() == 1 && IntentUnit(State, Direct->TargetUnitIds[0]) != nullptr);
	TestEqual(TEXT("saved target is refreshed with the visible target"),
		State.CardRun.ActiveBattle.EnemyStates.FindChecked(TEXT("Goat")).PendingChargeTargetUnitIds,
		Direct ? Direct->TargetUnitIds : TArray<FName>());
	return true;
}

#endif
