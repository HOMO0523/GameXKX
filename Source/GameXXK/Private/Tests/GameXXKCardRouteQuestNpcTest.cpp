#include "GameXXKCardBattleAdapter.h"
#include "GameXXKMVPRules.h"
#include "GameXXKPartyFormationRules.h"
#include "MVP/GameXXKMVPSubsystem.h"

#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardRouteQuestNpcTest,
	"GameXXK.Integration.CardRoute.QuestNpc",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardRouteQuestNpcTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("card-route NPC fixture starts a complete game"),
		Subsystem && Subsystem->StartGame()))
	{
		return false;
	}
	FGameXXKRuntimeState State = Subsystem->GetRuntimeStateCopy();
	TestTrue(TEXT("Yue Bai becomes the persistent selected NPC"),
		FGameXXKPartyFormationRules::SetQuestNpc(State, TEXT("Npc.YueBai")));
	State.CardRun.bLoadoutLockedForRoute = true;
	State.bDungeonActive = true;
	State.bHasActiveBattle = true;
	State.ActiveBattleNodeId = 42;
	FGameXXKBattleRuntimeUnit Enemy;
	Enemy.Id = TEXT("Enemy.Test.P1");
	Enemy.EnemyDefinitionId = TEXT("Enemy.Ch1.Rooster");
	Enemy.BattleSlotNumber = 1;
	Enemy.CombatLevel = 1;
	Enemy.HP = 30;
	Enemy.MaxHP = 30;
	Enemy.Attack = 1;
	Enemy.Defense = 0;
	Enemy.Speed = 1;
	Enemy.bEnemy = true;
	State.ActiveBattleEnemies = {Enemy};

	FString Error;
	TestTrue(*FString::Printf(TEXT("battle builds from the frozen permanent party: %s"), *Error),
		FGameXXKCardBattleAdapter::BeginCardBattle(
			State,
			EGameXXKNodeKind::Battle,
			EGameXXKCardTerrain::Plain,
			4242,
			&Error));

	TestTrue(TEXT("Yue Bai has a stable card-runtime unit"),
		State.CardRun.ActiveBattle.Units.ContainsByPredicate(
			[](const FGameXXKCardCombatUnit& Unit)
			{
				return Unit.UnitId == TEXT("Npc.YueBai")
					&& Unit.Side == EGameXXKCardTargetSide::Party;
			}));
	int32 YueBaiCardCount = 0;
	const auto CountYueBaiCards = [&YueBaiCardCount](const TArray<FGameXXKCardInstance>& Zone)
	{
		for (const FGameXXKCardInstance& Instance : Zone)
		{
			if (Instance.OwnerUnitId == TEXT("Npc.YueBai"))
			{
				++YueBaiCardCount;
			}
		}
	};
	CountYueBaiCards(State.CardRun.ActiveBattle.Deck.DrawPile);
	CountYueBaiCards(State.CardRun.ActiveBattle.Deck.Hand);
	CountYueBaiCards(State.CardRun.ActiveBattle.Deck.DiscardPile);
	CountYueBaiCards(State.CardRun.ActiveBattle.Deck.ExhaustPile);
	CountYueBaiCards(State.CardRun.ActiveBattle.Deck.PendingAutomaticHandCards);
	TestEqual(TEXT("Yue Bai contributes the persisted three-card loadout"), YueBaiCardCount, 3);
	TestTrue(TEXT("battle construction never revives temporary provenance"),
		State.CardRun.ActiveTemporaryQuestNpcId.IsNone());
	return true;
}

#endif
