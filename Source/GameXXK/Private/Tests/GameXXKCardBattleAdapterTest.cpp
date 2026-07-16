#include "Misc/AutomationTest.h"

#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCardRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGameXXKBattleRuntimeUnit MakeLegacyBattleUnit(
		const TCHAR* Id,
		const TCHAR* DisplayName,
		const int32 Health,
		const int32 Mana,
		const int32 Attack,
		const int32 Defense,
		const bool bEnemy)
	{
		FGameXXKBattleRuntimeUnit Unit;
		Unit.Id = FName(Id);
		Unit.DisplayName = FText::FromString(DisplayName);
		Unit.HP = Health;
		Unit.MaxHP = Health;
		Unit.MP = Mana;
		Unit.MaxMP = Mana;
		Unit.Attack = Attack;
		Unit.Defense = Defense;
		Unit.Speed = bEnemy ? 8 : 10;
		Unit.Shield = 1;
		Unit.bEnemy = bEnemy;
		return Unit;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardBattleAdapterTest,
	"GameXXK.Integration.CardBattleAdapter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardBattleAdapterTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	FString Error;
	TestTrue(FString::Printf(TEXT("a migrated or new runtime receives the approved card-run defaults: %s"), *Error),
		FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error));
	TestEqual(TEXT("the fixed hero starts with the approved eight selected permanent cards"), State.CardRun.HeroSelectedCardIds.Num(), 8);
	TestEqual(TEXT("the fixed hero starts with the approved eight unlocked permanent cards"), State.CardRun.HeroUnlockedCardIds.Num(), 8);

	TestTrue(TEXT("the accepted route can attach a specific temporary task NPC with three fixed cards"),
		FGameXXKCardBattleAdapter::SetQuestNpcForCurrentRun(State, TEXT("Npc.TusiChief"), {}, &Error));
	TestEqual(TEXT("a named temporary task NPC persists exactly three configured cards"), State.CardRun.PartySelection.QuestNpc.SelectedCardIds.Num(), 3);
	TestEqual(TEXT("the configured temporary task NPC persists its stable identity"), State.CardRun.PartySelection.QuestNpc.NpcId, FName(TEXT("Npc.TusiChief")));

	State.ActiveBattleParty = {
		MakeLegacyBattleUnit(TEXT("Player"), TEXT("Hero"), 100, 30, 15, 8, false)};
	State.ActiveBattleEnemies = {
		MakeLegacyBattleUnit(TEXT("MoneyRat"), TEXT("钱鼠"), 60, 0, 9, 2, true)};
	State.bHasActiveBattle = true;
	State.ActiveBattleNodeId = 42;
	TestTrue(FString::Printf(TEXT("a route battle builds one serialized shared card runtime from the locked party: %s"), *Error),
		FGameXXKCardBattleAdapter::BeginCardBattle(State, EGameXXKNodeKind::Battle, EGameXXKCardTerrain::Plain, 991, &Error));
	TestTrue(TEXT("the active card battle is explicitly persisted inside runtime state"), State.CardRun.bHasActiveCardBattle);
	TestTrue(TEXT("the active card battle passes its independent persistence validation"), GameXXKCardRules::ValidateCardBattleRuntime(State.CardRun.ActiveBattle, &Error));
	TestEqual(TEXT("hero plus one temporary NPC and deterministic fillers still creates the exact eighteen-card opening deck"), State.CardRun.ActiveBattle.Deck.ActiveInstanceIds.Num(), 18);
	TestEqual(TEXT("the opening card runtime begins with five materialized hand cards"), State.CardRun.ActiveBattle.Deck.Hand.Num(), 5);
	TestEqual(TEXT("the legacy projection contains hero and one task NPC, not the old automatic follower"), State.ActiveBattleParty.Num(), 2);
	TestTrue(TEXT("the temporary task NPC has a stable card-runtime unit"), State.CardRun.ActiveBattle.Units.ContainsByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Npc.TusiChief") && Unit.Side == EGameXXKCardTargetSide::Party;
	}));

	FGameXXKCardCombatUnit* HeroUnit = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Player");
	});
	FGameXXKCardCombatUnit* EnemyUnit = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("MoneyRat");
	});
	TestNotNull(TEXT("the card-runtime projection retains the fixed hero"), HeroUnit);
	TestNotNull(TEXT("the card-runtime projection retains the route enemy by stable id"), EnemyUnit);
	if (HeroUnit && EnemyUnit)
	{
		HeroUnit->HP = 71;
		HeroUnit->Mana = 12;
		EnemyUnit->HP = 33;
		TestTrue(TEXT("projection sync only reflects serialized card state into legacy battle widgets"), FGameXXKCardBattleAdapter::SyncCardBattleToLegacyProjection(State, &Error));
		TestEqual(TEXT("legacy hero health projects from the card runtime"), State.ActiveBattleParty[0].HP, 71);
		TestEqual(TEXT("legacy hero mana projects from the card runtime"), State.ActiveBattleParty[0].MP, 12);
		TestEqual(TEXT("legacy enemy health projects from the card runtime"), State.ActiveBattleEnemies[0].HP, 33);
	}

	TArray<FName> RewardChoiceIds;
	TestTrue(FString::Printf(TEXT("a normal battle produces a deterministic three-card route reward offer: %s"), *Error),
		FGameXXKCardBattleAdapter::CreateRouteRewardOffer(State, EGameXXKNodeKind::Battle, 42, 2026, RewardChoiceIds, &Error));
	TestEqual(TEXT("normal battle reward exposes exactly three card choices"), RewardChoiceIds.Num(), 3);
	TSet<FName> UniqueRewardIds(RewardChoiceIds);
	TestEqual(TEXT("normal battle reward never duplicates a card within its visible three choices"), UniqueRewardIds.Num(), RewardChoiceIds.Num());
	if (!RewardChoiceIds.IsEmpty())
	{
		TestTrue(TEXT("the selected reward commits as a route-local card rather than a permanent hero card"),
			FGameXXKCardBattleAdapter::ChoosePendingRouteReward(State, RewardChoiceIds[0], NAME_None, &Error));
		TestTrue(TEXT("a chosen route reward remains in the route-local list"), State.CardRun.RouteCardIds.Contains(RewardChoiceIds[0]));
		TestEqual(TEXT("the reward offer clears only after an explicit pick"), State.CardRun.PendingReward.CardIds.Num(), 0);
	}

	FGameXXKCardBattleAdapter::ClearRouteLocalCardState(State);
	TestFalse(TEXT("ending the route clears an in-progress card battle"), State.CardRun.bHasActiveCardBattle);
	TestEqual(TEXT("ending the route removes temporary route reward cards"), State.CardRun.RouteCardIds.Num(), 0);
	TestEqual(TEXT("ending the route removes the temporary task NPC selection"), State.CardRun.PartySelection.QuestNpc.NpcId, NAME_None);
	TestEqual(TEXT("ending the route preserves the hero's permanent configured cards"), State.CardRun.HeroSelectedCardIds.Num(), 8);

	return true;
}

#endif
