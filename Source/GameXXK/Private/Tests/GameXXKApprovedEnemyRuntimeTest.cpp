#include "GameXXKCardBattleAdapter.h"

#include "GameXXKCardRules.h"
#include "GameXXKEnemyCatalog.h"
#include "GameXXKPermanentPartyTestFixtures.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGameXXKBattleRuntimeUnit RuntimeHero()
	{
		FGameXXKBattleRuntimeUnit Unit;
		Unit.Id = TEXT("Player");
		Unit.DisplayName = FText::FromString(TEXT("主角"));
		Unit.HP = 10000;
		Unit.MaxHP = 10000;
		Unit.MP = 40;
		Unit.MaxMP = 40;
		Unit.Attack = 40;
		Unit.Defense = 0;
		Unit.Speed = 10;
		Unit.CombatLevel = 100;
		Unit.Shield = 0;
		return Unit;
	}

	FGameXXKBattleRuntimeUnit RuntimeEnemy(
		const TCHAR* UnitId,
		const TCHAR* DefinitionId,
		const int32 Slot,
		const int32 HP = 1000,
		const int32 Attack = 100,
		const int32 Defense = 50,
		const int32 Speed = 10)
	{
		FGameXXKBattleRuntimeUnit Unit;
		Unit.Id = UnitId;
		Unit.DisplayName = FText::FromName(DefinitionId);
		Unit.HP = HP;
		Unit.MaxHP = HP;
		Unit.Attack = Attack;
		Unit.Defense = Defense;
		Unit.Speed = Speed;
		Unit.bEnemy = true;
		Unit.EnemyDefinitionId = DefinitionId;
		Unit.BattleSlotNumber = Slot;
		Unit.CombatLevel = 100;
		Unit.Shield = 0;
		return Unit;
	}

	bool BeginRuntimeBattle(
		FGameXXKRuntimeState& State,
		TArray<FGameXXKBattleRuntimeUnit> Enemies,
		const int32 DifficultyPercent,
		FString& Error)
	{
		State = GameXXKPermanentPartyTestFixtures::MakeStartedState();
		State.PlayerLevel = 100;
		if (!FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error))
		{
			return false;
		}
		State.ActiveBattleParty = {RuntimeHero()};
		State.ActiveBattleEnemies = MoveTemp(Enemies);
		State.bHasActiveBattle = true;
		State.ActiveBattleNodeId = 9921;
		const bool bStarted = FGameXXKCardBattleAdapter::BeginCardBattle(
			State,
			EGameXXKNodeKind::Elite,
			EGameXXKCardTerrain::Plain,
			9921,
			&Error,
			DifficultyPercent);
		if (bStarted)
		{
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
		}
		return bStarted;
	}

	FGameXXKCardCombatUnit* FindUnit(FGameXXKRuntimeState& State, const FName UnitId)
	{
		return State.CardRun.ActiveBattle.Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == UnitId;
		});
	}

	bool ForceIntentAndEnterEnemyPhase(
		FGameXXKRuntimeState& State,
		const FName SourceUnitId,
		const FName IntentId,
		const int32 PhaseNumber,
		FString& Error)
	{
		FGameXXKEnemyBattleState* EnemyState = State.CardRun.ActiveBattle.EnemyStates.Find(SourceUnitId);
		if (!EnemyState)
		{
			Error = TEXT("force-intent fixture lost its enemy state");
			return false;
		}
		EnemyState->CurrentPhase = PhaseNumber;
		EnemyState->bPhaseTwo = PhaseNumber >= 2;
		EnemyState->IntentCursor = 0;
		EnemyState->PendingChargedIntentId = NAME_None;
		EnemyState->ChargeRoundsRemaining = 0;
		EnemyState->PendingChargeTargetUnitIds.Reset();
		State.CardRun.ActiveBattle.LockedEnemyIntents = {
			{SourceUnitId, IntentId, PhaseNumber, State.CardRun.ActiveBattle.RoundNumber}};
		State.CardRun.EnemyIntents.Reset();
		State.CardRun.NextEnemyIntentIndex = 0;
		TArray<FGameXXKCardDamageResult> EndResults;
		return FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, EndResults, &Error);
	}

	const FGameXXKResolvedEnemyIntentEffect* EffectOf(
		const FGameXXKCardEnemyIntent& Intent,
		const EGameXXKEnemyIntentEffectType Type)
	{
		return Intent.Effects.FindByPredicate([Type](const FGameXXKResolvedEnemyIntentEffect& Effect)
		{
			return Effect.Type == Type;
		});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEnemyDifficultyIntentRuntimeTest,
	"GameXXK.Battle.EnemyApproved.DifficultyDamageDotAndArmor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEnemyDifficultyIntentRuntimeTest::RunTest(const FString& Parameters)
{
	struct FDifficultyCase { int32 Percent; int32 ExpectedPeck; };
	for (const FDifficultyCase& Case : {FDifficultyCase{100, 150}, {125, 288}, {150, 465}})
	{
		FString Error;
		FGameXXKRuntimeState State;
		if (!TestTrue(TEXT("difficulty Rooster fixture begins"), BeginRuntimeBattle(State, {
			RuntimeEnemy(TEXT("Rooster"), TEXT("Enemy.Ch1.Rooster"), 1)}, Case.Percent, Error))
			|| !TestTrue(TEXT("Peck is forced"), ForceIntentAndEnterEnemyPhase(State, TEXT("Rooster"), TEXT("Peck"), 1, Error)))
		{
			AddError(Error);
			return false;
		}
		TestEqual(TEXT("Peck card shows its resolved difficulty damage"), State.CardRun.EnemyIntents[0].Damage, Case.ExpectedPeck);
		FGameXXKCardEnemyIntent Resolved;
		TArray<FGameXXKCardDamageResult> Damage;
		bool bFinished = false;
		TestTrue(TEXT("Peck resolves"), FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, Resolved, Damage, bFinished, &Error));
		TestEqual(TEXT("Peck execution uses the forecasted requested damage"), Damage[0].RequestedDamage, Case.ExpectedPeck);
	}

	FString Error;
	FGameXXKRuntimeState SnakeState;
	if (!TestTrue(TEXT("Hell Snake fixture begins"), BeginRuntimeBattle(SnakeState, {
		RuntimeEnemy(TEXT("Snake"), TEXT("Enemy.Ch3.VenomSnake"), 1)}, 150, Error))
		|| !TestTrue(TEXT("Venom Bite is forced"), ForceIntentAndEnterEnemyPhase(SnakeState, TEXT("Snake"), TEXT("VenomBite"), 1, Error)))
	{
		AddError(Error);
		return false;
	}
	const FGameXXKResolvedEnemyIntentEffect* Venom = EffectOf(SnakeState.CardRun.EnemyIntents[0], EGameXXKEnemyIntentEffectType::DirectDamage);
	FName VenomTargetId = NAME_None;
	if (TestNotNull(TEXT("Venom Bite direct effect exists"), Venom))
	{
		TestEqual(TEXT("Hell Venom Bite coefficient nine becomes forty-five poison at team level 100"), Venom->StatusStacks, 45);
		VenomTargetId = Venom->TargetUnitIds.IsEmpty() ? NAME_None : Venom->TargetUnitIds[0];
	}
	FGameXXKCardEnemyIntent Resolved;
	TArray<FGameXXKCardDamageResult> Damage;
	bool bFinished = false;
	TestTrue(TEXT("Venom Bite resolves"), FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(SnakeState, Resolved, Damage, bFinished, &Error));
	TestEqual(TEXT("the target receives final poison"), GameXXKCardRules::GetCombatStatusStacks(*FindUnit(SnakeState, VenomTargetId), EGameXXKCardStatus::Poison), 45);

	FGameXXKRuntimeState GoatState;
	if (!TestTrue(TEXT("Hell Goat fixture begins"), BeginRuntimeBattle(GoatState, {
		RuntimeEnemy(TEXT("Goat"), TEXT("Enemy.Ch1.Goat"), 1)}, 150, Error))
		|| !TestTrue(TEXT("Stomp is forced"), ForceIntentAndEnterEnemyPhase(GoatState, TEXT("Goat"), TEXT("Stomp"), 1, Error)))
	{
		AddError(Error);
		return false;
	}
	const FGameXXKResolvedEnemyIntentEffect* Stomp = EffectOf(GoatState.CardRun.EnemyIntents[0], EGameXXKEnemyIntentEffectType::AddArmorDefensePercent);
	TestEqual(TEXT("Hell Stomp resolves 360 percent of fifty Defense"), Stomp ? Stomp->Magnitude : 0, 180);
	TestTrue(TEXT("Stomp resolves"), FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(GoatState, Resolved, Damage, bFinished, &Error));
	TestEqual(TEXT("Stomp grants the resolved armor"), FindUnit(GoatState, TEXT("Goat"))->Armor, 180);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEnemyResourceMechanicsRuntimeTest,
	"GameXXK.Battle.EnemyApproved.WealthToadAndTigerResources",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEnemyResourceMechanicsRuntimeTest::RunTest(const FString& Parameters)
{
	FString Error;
	FGameXXKCardEnemyIntent Resolved;
	TArray<FGameXXKCardDamageResult> Damage;
	bool bFinished = false;

	FGameXXKRuntimeState MoneyState;
	if (!TestTrue(TEXT("Money Rat fixture begins"), BeginRuntimeBattle(MoneyState, {
		RuntimeEnemy(TEXT("MoneyRat"), TEXT("Enemy.Ch1.MoneyRat"), 1)}, 150, Error)))
	{
		AddError(Error);
		return false;
	}
	FGameXXKEnemyBattleState& MoneyRuntime = MoneyState.CardRun.ActiveBattle.EnemyStates.FindChecked(TEXT("MoneyRat"));
	MoneyRuntime.TotalPhases = 3;
	FGameXXKCardCombatUnit* MoneyRat = FindUnit(MoneyState, TEXT("MoneyRat"));
	MoneyRat->HP = 500;
	GameXXKCardRules::ConsumeCombatStatus(*MoneyRat, EGameXXKCardStatus::Wealth, MAX_int32);
	GameXXKCardRules::AddCombatStatus(*MoneyRat, EGameXXKCardStatus::Wealth, 3);
	if (!TestTrue(TEXT("phase-two Wealth heal is forced"), ForceIntentAndEnterEnemyPhase(MoneyState, TEXT("MoneyRat"), TEXT("SpendWealthContinueLifeP2"), 2, Error)))
	{
		AddError(Error);
		return false;
	}
	TestTrue(TEXT("phase-two Wealth heal resolves"), FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(MoneyState, Resolved, Damage, bFinished, &Error));
	MoneyRat = FindUnit(MoneyState, TEXT("MoneyRat"));
	TestEqual(TEXT("three Wealth heal fifteen percent maximum HP"), MoneyRat->HP, 650);
	TestEqual(TEXT("the three Wealth are consumed"), GameXXKCardRules::GetCombatStatusStacks(*MoneyRat, EGameXXKCardStatus::Wealth), 0);
	TestEqual(TEXT("the same card grants eighty percent Defense armor to all enemies"), MoneyRat->Armor, 40);

	FGameXXKRuntimeState ToadState;
	if (!TestTrue(TEXT("Giant Toad fixture begins"), BeginRuntimeBattle(ToadState, {
		RuntimeEnemy(TEXT("Toad"), TEXT("Enemy.Ch3.GiantToad"), 1)}, 150, Error)))
	{
		AddError(Error);
		return false;
	}
	FGameXXKCardCombatUnit* Toad = FindUnit(ToadState, TEXT("Toad"));
	Toad->HP = 500;
	ToadState.CardRun.ActiveBattle.EnemyStates.FindChecked(TEXT("Toad")).PendingHealingAmplificationPercent = 6;
	for (FGameXXKCardCombatUnit& Unit : ToadState.CardRun.ActiveBattle.Units)
	{
		if (Unit.bLiving && Unit.Side == EGameXXKCardTargetSide::Party)
		{
			TestEqual(TEXT("Tongue fixture applies poison to each possible target"), GameXXKCardRules::AddCombatStatus(Unit, EGameXXKCardStatus::Poison, 5), 5);
		}
	}
	if (!TestTrue(TEXT("amplified Tongue is forced"), ForceIntentAndEnterEnemyPhase(ToadState, TEXT("Toad"), TEXT("Tongue"), 1, Error)))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("poisoned Hell target makes Tongue show 400 percent before difficulty"),
		EffectOf(ToadState.CardRun.EnemyIntents[0], EGameXXKEnemyIntentEffectType::DirectDamage)->Magnitude, 400);
	TestTrue(TEXT("amplified Tongue resolves"), FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(ToadState, Resolved, Damage, bFinished, &Error));
	Toad = FindUnit(ToadState, TEXT("Toad"));
	TestEqual(TEXT("Tongue heals base six plus refreshed six percent"), Toad->HP, 620);
	TestEqual(TEXT("Tongue consumes the refresh-only amplification"), ToadState.CardRun.ActiveBattle.EnemyStates.FindChecked(TEXT("Toad")).PendingHealingAmplificationPercent, 0);

	FGameXXKRuntimeState TigerState;
	if (!TestTrue(TEXT("Tiger fixture begins"), BeginRuntimeBattle(TigerState, {
		RuntimeEnemy(TEXT("Tiger"), TEXT("Enemy.Ch3.Tiger"), 1)}, 150, Error)))
	{
		AddError(Error);
		return false;
	}
	FGameXXKEnemyBattleState& TigerRuntime = TigerState.CardRun.ActiveBattle.EnemyStates.FindChecked(TEXT("Tiger"));
	TigerRuntime.CurrentPhase = 2;
	TigerRuntime.TotalPhases = 3;
	TigerRuntime.bPhaseTwo = true;
	TigerRuntime.PersistentTargetUnitId = TEXT("Player");
	TigerRuntime.PersistentTargetStatus = static_cast<uint8>(EGameXXKCardStatus::Prey);
	FGameXXKCardCombatUnit* Tiger = FindUnit(TigerState, TEXT("Tiger"));
	Tiger->HP = 500;
	FGameXXKCardCombatUnit* Player = FindUnit(TigerState, TEXT("Player"));
	GameXXKCardRules::AddCombatStatus(*Player, EGameXXKCardStatus::Prey, 1);
	GameXXKCardRules::AddCombatStatus(*Player, EGameXXKCardStatus::Bleed, 45);
	if (!TestTrue(TEXT("phase-two Wound Pursuit is forced"), ForceIntentAndEnterEnemyPhase(TigerState, TEXT("Tiger"), TEXT("WoundPursuit"), 2, Error)))
	{
		AddError(Error);
		return false;
	}
	TestTrue(TEXT("Wound Pursuit resolves"), FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(TigerState, Resolved, Damage, bFinished, &Error));
	Tiger = FindUnit(TigerState, TEXT("Tiger"));
	TestEqual(TEXT("Tiger heals eight percent missing HP only once for the multi-hit card"), Tiger->HP, 540);
	TestEqual(TEXT("two direct hits plus one Bleed trigger are audited"), Damage.Num(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKWhiteApePhaseGuardRuntimeTest,
	"GameXXK.Battle.EnemyApproved.WhiteApePhaseGuard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKWhiteApePhaseGuardRuntimeTest::RunTest(const FString& Parameters)
{
	FString Error;
	FGameXXKRuntimeState State;
	if (!TestTrue(TEXT("White Ape ally-guard fixture begins"), BeginRuntimeBattle(State, {
		RuntimeEnemy(TEXT("WhiteApe"), TEXT("Enemy.Ch3.WhiteApe"), 1),
		RuntimeEnemy(TEXT("Goat"), TEXT("Enemy.Ch1.Goat"), 2)}, 150, Error)))
	{
		AddError(Error);
		return false;
	}
	FGameXXKEnemyBattleState& WhiteApeState = State.CardRun.ActiveBattle.EnemyStates.FindChecked(TEXT("WhiteApe"));
	WhiteApeState.CurrentPhase = 2;
	WhiteApeState.TotalPhases = 3;
	WhiteApeState.bPhaseTwo = true;
	FGameXXKCardCombatUnit* Goat = FindUnit(State, TEXT("Goat"));
	TestEqual(TEXT("the first actual negative status is added"), GameXXKCardRules::AddCombatStatus(*Goat, EGameXXKCardStatus::Weak, 1), 1);
	TestTrue(TEXT("phase-two White Ape guard resolves for an ally"), GameXXKCardRules::ResolveWhiteApeStatusGuardAfterStatusApplied(State.CardRun.ActiveBattle, *Goat, &Error));
	TestEqual(TEXT("phase two grants 100 percent of White Ape Defense"), Goat->Armor, 50);
	TestEqual(TEXT("a second negative status is added"), GameXXKCardRules::AddCombatStatus(*Goat, EGameXXKCardStatus::Mark, 1), 1);
	TestTrue(TEXT("the second status call is valid"), GameXXKCardRules::ResolveWhiteApeStatusGuardAfterStatusApplied(State.CardRun.ActiveBattle, *Goat, &Error));
	TestEqual(TEXT("the same ally guards only once per player round"), Goat->Armor, 50);
	TestTrue(TEXT("a completed enemy phase rearms eligible guards"), GameXXKCardRules::ResetWhiteApeStatusGuardsForPlayerRound(State.CardRun.ActiveBattle, &Error));
	State.CardRun.ActiveBattle.EnemyStates.FindChecked(TEXT("WhiteApe")).CurrentPhase = 3;
	State.CardRun.ActiveBattle.EnemyStates.FindChecked(TEXT("Goat")).bFirstStatusPassiveAvailable = true;
	TestEqual(TEXT("another negative layer is added"), GameXXKCardRules::AddCombatStatus(*Goat, EGameXXKCardStatus::Weak, 1), 1);
	TestTrue(TEXT("phase-three White Ape guard resolves"), GameXXKCardRules::ResolveWhiteApeStatusGuardAfterStatusApplied(State.CardRun.ActiveBattle, *Goat, &Error));
	TestEqual(TEXT("phase three grants 160 percent of White Ape Defense"), Goat->Armor, 130);
	return true;
}

#endif
