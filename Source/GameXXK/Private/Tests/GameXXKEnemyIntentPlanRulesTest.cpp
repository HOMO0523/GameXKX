#include "GameXXKCardBattleAdapter.h"

#include "GameXXKCardRules.h"
#include "GameXXKPermanentPartyTestFixtures.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGameXXKBattleRuntimeUnit PlanHero()
	{
		FGameXXKBattleRuntimeUnit Unit;
		Unit.Id = TEXT("Player");
		Unit.DisplayName = FText::FromString(TEXT("主角"));
		Unit.HP = 500;
		Unit.MaxHP = 500;
		Unit.MP = 40;
		Unit.MaxMP = 40;
		Unit.Attack = 40;
		Unit.Defense = 10;
		Unit.Speed = 10;
		return Unit;
	}

	FGameXXKBattleRuntimeUnit PlanEnemy(
		const TCHAR* UnitId,
		const TCHAR* DefinitionId,
		const int32 Slot,
		const int32 HP,
		const int32 Attack,
		const int32 Defense,
		const int32 Speed,
		const int32 Level)
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
		Unit.CombatLevel = Level;
		return Unit;
	}

	bool BeginPlanBattle(
		FGameXXKRuntimeState& State,
		TArray<FGameXXKBattleRuntimeUnit> Enemies,
		const int32 DifficultyPercent,
		FString& Error)
	{
		State = GameXXKPermanentPartyTestFixtures::MakeStartedState();
		if (!FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error))
		{
			return false;
		}
		State.ActiveBattleParty = {PlanHero()};
		State.ActiveBattleEnemies = MoveTemp(Enemies);
		State.bHasActiveBattle = true;
		State.ActiveBattleNodeId = 9911;
		return FGameXXKCardBattleAdapter::BeginCardBattle(
			State,
			EGameXXKNodeKind::Elite,
			EGameXXKCardTerrain::Plain,
			9911,
			&Error,
			DifficultyPercent);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEnemyIntentOrderedForecastTest,
	"GameXXK.Battle.EnemyIntentPlan.OrderedForecastAndIdentityLock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEnemyIntentOrderedForecastTest::RunTest(const FString& Parameters)
{
	FString Error;
	FGameXXKRuntimeState State;
	if (!TestTrue(TEXT("ordered plan fixture begins"), BeginPlanBattle(State, {
		PlanEnemy(TEXT("Rooster.Left"), TEXT("Enemy.Ch1.Rooster"), 1, 500, 100, 0, 10, 50),
		PlanEnemy(TEXT("Goat.Center"), TEXT("Enemy.Ch1.Goat"), 2, 500, 70, 0, 6, 50),
		PlanEnemy(TEXT("Rooster.Right"), TEXT("Enemy.Ch1.Rooster"), 3, 500, 100, 0, 10, 50)}, 100, Error)))
	{
		AddError(Error);
		return false;
	}
	FGameXXKCardBattleRuntime& Runtime = State.CardRun.ActiveBattle;
	Runtime.LockedEnemyIntents = {
		{TEXT("Rooster.Left"), TEXT("Crow"), 1, 1},
		{TEXT("Rooster.Right"), TEXT("DoublePeck"), 1, 1},
		{TEXT("Goat.Center"), TEXT("Horn"), 1, 1}};
	State.CardRun.EnemyIntents.Reset();
	State.CardRun.NextEnemyIntentIndex = 0;
	TArray<FGameXXKCardDamageResult> EndResults;
	if (!TestTrue(TEXT("ending the player phase rebuilds the locked ordered forecast"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, EndResults, &Error)))
	{
		AddError(Error);
		return false;
	}
	const TArray<FGameXXKCardEnemyIntent>& Intents = State.CardRun.EnemyIntents;
	if (!TestEqual(TEXT("three ordered intents are forecast"), Intents.Num(), 3))
	{
		return false;
	}
	TestEqual(TEXT("equal-speed left slot acts first"), Intents[0].SourceUnitId, FName(TEXT("Rooster.Left")));
	TestEqual(TEXT("equal-speed right slot acts second"), Intents[1].SourceUnitId, FName(TEXT("Rooster.Right")));
	TestEqual(TEXT("slower center slot acts last"), Intents[2].SourceUnitId, FName(TEXT("Goat.Center")));
	TestEqual(TEXT("left identity remains Crow"), Intents[0].IntentDefinitionId, FName(TEXT("Crow")));
	TestEqual(TEXT("right identity remains Double Peck"), Intents[1].IntentDefinitionId, FName(TEXT("DoublePeck")));
	TestEqual(TEXT("Crow's twenty-point forecast buff changes the later per-hit damage"), Intents[1].Damage, 120);
	const FGameXXKCardCombatUnit* ActualRight = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Rooster.Right");
	});
	TestEqual(TEXT("forecast does not commit the temporary attack buff"), ActualRight ? ActualRight->Attack : 0, 100);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEnemyIntentPhaseDeckForecastTest,
	"GameXXK.Battle.EnemyIntentPlan.PhaseTransitionUsesFirstNewCard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEnemyIntentPhaseDeckForecastTest::RunTest(const FString& Parameters)
{
	FString Error;
	FGameXXKRuntimeState State;
	if (!TestTrue(TEXT("phase plan fixture begins"), BeginPlanBattle(State, {
		PlanEnemy(TEXT("Ironfeather"), TEXT("Enemy.Ch1.IronfeatherRooster"), 1, 100, 50, 0, 11, 95)}, 150, Error)))
	{
		AddError(Error);
		return false;
	}
	FGameXXKCardDamageContext Context;
	Context.SourceUnitId = TEXT("Player");
	Context.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
	Context.ResolutionOrigin = EGameXXKCardResolutionOrigin::ActivePlay;
	FGameXXKCardDamageResult Damage;
	if (!TestTrue(TEXT("a lethal packet consumes phase one"),
		GameXXKCardRules::ApplyPlayerCardDirectDamage(
			State.CardRun.ActiveBattle,
			Context,
			TEXT("Ironfeather"),
			1000,
			Damage,
			&Error)))
	{
		AddError(Error);
		return false;
	}
	TestTrue(TEXT("phase transition is audited"), Damage.bTriggeredEnemyPhase);
	State.CardRun.EnemyIntents.Reset();
	State.CardRun.NextEnemyIntentIndex = 0;
	TArray<FGameXXKCardDamageResult> EndResults;
	if (!TestTrue(TEXT("the next enemy phase rebuilds from phase two"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, EndResults, &Error)))
	{
		AddError(Error);
		return false;
	}
	if (!TestEqual(TEXT("one enemy forecast remains"), State.CardRun.EnemyIntents.Num(), 1))
	{
		return false;
	}
	TestEqual(TEXT("phase two uses its first authored card"),
		State.CardRun.EnemyIntents[0].IntentDefinitionId,
		FName(TEXT("IronfeatherBurningFormation")));
	TestEqual(TEXT("forecast carries phase two"), State.CardRun.EnemyIntents[0].PhaseNumber, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKEnemyPoisonPhaseForecastTest,
	"GameXXK.Battle.EnemyIntentPlan.PoisonBoundaryReplacesOldDeck", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKEnemyPoisonPhaseForecastTest::RunTest(const FString&)
{
	FGameXXKRuntimeState State;FString Error;
	if(!BeginPlanBattle(State,{PlanEnemy(TEXT("Ironfeather"),TEXT("Enemy.Ch1.IronfeatherRooster"),1,100,50,0,11,95)},150,Error)){AddError(Error);return false;}
	const auto OldIntent=State.CardRun.EnemyIntents[0].IntentDefinitionId;
	auto* Enemy=State.CardRun.ActiveBattle.Units.FindByPredicate([](const auto& U){return U.UnitId==TEXT("Ironfeather");});
	GameXXKCardRules::AddCombatStatus(*Enemy,EGameXXKCardStatus::Poison,100);
	TArray<FGameXXKCardDamageResult> Damage;
	if(!FGameXXKCardBattleAdapter::EndPlayerCardPhase(State,Damage,&Error)){AddError(Error);return false;}
	TestEqual(TEXT("end-of-player-phase poison consumes phase one"),State.CardRun.ActiveBattle.EnemyStates[TEXT("Ironfeather")].CurrentPhase,2);
	TestTrue(TEXT("the existing forecast is replaced by the new phase deck"),State.CardRun.EnemyIntents.Num()==1&&State.CardRun.EnemyIntents[0].IntentDefinitionId!=OldIntent);
	FGameXXKCardEnemyIntent Resolved;bool Finished=false;
	TestTrue(FString::Printf(TEXT("first new-phase intent resolves: %s"),*Error),FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State,Resolved,Damage,Finished,&Error));
	return true;
}
#endif
