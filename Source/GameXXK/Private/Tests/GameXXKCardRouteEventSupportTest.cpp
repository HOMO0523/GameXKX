#include "GameXXKMVPRules.h"
#include "GameXXKRouteEconomyRules.h"
#include "MVP/GameXXKBattleScenePresenter.h"
#include "MVP/GameXXKBattleSceneUnitActor.h"

#include "Misc/AutomationTest.h"
#include "PaperFlipbook.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardRouteEventSupportTest,
	"GameXXK.Integration.CardRoute.EventSupport",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardRouteEventSupportTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	State.bDungeonActive = true;
	State.bHasGeneratedRouteMap = true;
	State.Screen = EGameXXKScreen::RouteEvent;
	State.RouteSeed = 24681357;
	State.RouteMapNodes = { FGameXXKRouteMapNode(23, 2, 0, EGameXXKNodeKind::Event, FVector2D(0.5f, 0.4f), {}) };
	State.PendingRouteNodeId = 23;
	State.CardRun.RouteProgress.CurrentChapter = 1;
	TestTrue(TEXT("event support fixture initializes its route economy"), FGameXXKRouteEconomyRules::InitializeRoute(State.CardRun));
	State.CardRun.PendingEvent.SourceNodeId = 23;
	State.CardRun.PendingEvent.ChoiceSeed = 7654321;
	State.CardRun.PendingEvent.EventNpcId = TEXT("Npc.YueBai");

	TestTrue(TEXT("accepting a named route event NPC adds only the temporary task-NPC slot and completes that event node"),
		UGameXXKMVPRules::AcceptRouteEventNpcSupport(State));
	TestEqual(TEXT("the accepted named NPC is available to the later route battles"), State.CardRun.ActiveTemporaryQuestNpcId, FName(TEXT("Npc.YueBai")));
	TestEqual(TEXT("the accepted named NPC uses its fixed three-card route selection"), State.CardRun.PartySelection.QuestNpc.SelectedCardIds.Num(), 3);
	TestEqual(TEXT("accepting temporary support returns to the route map"), State.Screen, EGameXXKScreen::DungeonMap);
	TestTrue(TEXT("the accepted event node completes exactly once"), State.VisitedRouteNodeIds.Contains(23));

	// Reuse the stable linear battle entry to prove that the accepted event identity is
	// projected into the next battle and then given a concrete scene placement.
	State.bHasGeneratedRouteMap = false;
	State.RouteMapNodes.Reset();
	State.RouteMapEdges.Reset();
	State.ReachableRouteNodeIds.Reset();
	State.DungeonNodeIndex = 1;
	TestTrue(TEXT("the battle after accepting temporary support opens normally"),
		UGameXXKMVPRules::AdvanceDungeonNode(State, EGameXXKNodeKind::Battle));
	TestTrue(TEXT("the accepted event NPC joins the next battle party"),
		State.ActiveBattleParty.ContainsByPredicate([](const FGameXXKBattleRuntimeUnit& Unit)
		{
			return Unit.Id == TEXT("Npc.YueBai");
		}));
	TestTrue(TEXT("the accepted event NPC is represented as a task-NPC combat unit"),
		State.CardRun.ActiveBattle.Units.ContainsByPredicate([](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == TEXT("Npc.YueBai") && Unit.Role == EGameXXKCharacterRole::QuestNpc;
		}));
	const FGameXXKBattleSceneUnitPlacement* QuestNpcPlacement = AGameXXKBattleScenePresenter::BuildUnitPlacementsForState(State).FindByPredicate([](const FGameXXKBattleSceneUnitPlacement& Placement)
		{
			return !Placement.bEnemy && Placement.UnitId == TEXT("Npc.YueBai");
		});
	TestNotNull(TEXT("the battle scene presenter creates a party placement for the accepted event NPC"), QuestNpcPlacement);
	if (QuestNpcPlacement)
	{
		TestEqual(TEXT("the task NPC retains fixed 我 3P after accepting its route event"), QuestNpcPlacement->SlotNumber, 3);
		const FGameXXKBattleRuntimeUnit* QuestNpcUnit = State.ActiveBattleParty.FindByPredicate([](const FGameXXKBattleRuntimeUnit& Unit)
		{
			return Unit.Id == TEXT("Npc.YueBai");
		});
		TestNotNull(TEXT("the accepted task NPC has a projected legacy scene unit"), QuestNpcUnit);
		if (QuestNpcUnit)
		{
			AGameXXKBattleSceneUnitActor* QuestNpcActor = NewObject<AGameXXKBattleSceneUnitActor>();
			QuestNpcActor->ConfigureFromRuntimeUnit(false, QuestNpcPlacement->UnitIndex, *QuestNpcUnit, QuestNpcPlacement->SlotNumber);
			UPaperFlipbook* QuestNpcFlipbook = QuestNpcActor->GetCurrentBattleFlipbook();
			TestNotNull(TEXT("the accepted named task NPC resolves its named battle flipbook"), QuestNpcFlipbook);
			if (QuestNpcFlipbook)
			{
				TestTrue(TEXT("the accepted Yue Bai NPC uses its dedicated battle flipbook"), QuestNpcFlipbook->GetPathName().Contains(TEXT("FB_PartyDeckNPC_YueBai_Idle_South")));
			}
		}
	}
	const TArray<FGameXXKBattleSceneUnitPlacement> AcceptedNpcPlacements = AGameXXKBattleScenePresenter::BuildUnitPlacementsForState(State);
	const bool bHasPersistentPartner = State.CardRun.ActiveBattle.Units.ContainsByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.Side == EGameXXKCardTargetSide::Party
			&& Unit.Role != EGameXXKCharacterRole::Hero
			&& Unit.Role != EGameXXKCharacterRole::QuestNpc;
	});
	if (!bHasPersistentPartner)
	{
		TestFalse(TEXT("event NPC support never invents a permanent-partner 1P placement"),
			AcceptedNpcPlacements.ContainsByPredicate([](const FGameXXKBattleSceneUnitPlacement& Placement)
			{
				return !Placement.bEnemy && Placement.UnitId != TEXT("Player") && Placement.SlotNumber == 1;
			}));
	}

	State.Screen = EGameXXKScreen::RouteEvent;
	State.RouteMapNodes.Add(FGameXXKRouteMapNode(24, 3, 0, EGameXXKNodeKind::Event, FVector2D(0.5f, 0.55f), {}));
	State.PendingRouteNodeId = 24;
	State.CardRun.PendingEvent.SourceNodeId = 24;
	State.CardRun.PendingEvent.ChoiceSeed = 7654322;
	State.CardRun.PendingEvent.EventNpcId = TEXT("Npc.TusiChief");
	TestFalse(TEXT("a second task NPC cannot silently replace the route's existing temporary support"),
		UGameXXKMVPRules::AcceptRouteEventNpcSupport(State));
	TestEqual(TEXT("rejecting a second support offer preserves the original temporary NPC"),
		State.CardRun.PartySelection.QuestNpc.NpcId, FName(TEXT("Npc.YueBai")));
	TestEqual(TEXT("rejecting a second support offer leaves the event choice available"),
		State.CardRun.PendingEvent.EventNpcId, FName(TEXT("Npc.TusiChief")));
	return true;
}

#endif
