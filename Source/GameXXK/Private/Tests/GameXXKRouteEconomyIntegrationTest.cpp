#include "Misc/AutomationTest.h"

#include "GameXXKCardBattleAdapter.h"
#include "GameXXKMVPRules.h"
#include "GameXXKRelicCatalog.h"
#include "GameXXKRelicRules.h"
#include "GameXXKRouteEconomyRules.h"
#include "GameXXKRouteEncounterCatalog.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr int32 DefaultRouteBalance = 60;

	TArray<uint8> SerializeRuntimeState(const FGameXXKRuntimeState& State)
	{
		TArray<uint8> Bytes;
		FMemoryWriter Writer(Bytes, true);
		FObjectAndNameAsStringProxyArchive Archive(Writer, false);
		FGameXXKRuntimeState Copy = State;
		FGameXXKRuntimeState::StaticStruct()->SerializeItem(Archive, &Copy, nullptr);
		return Bytes;
	}

	bool EnterRouteFixture(FGameXXKRuntimeState& OutState, const int32 PlayerGold = 137)
	{
		OutState = UGameXXKMVPRules::CreateNewGame();
		OutState.Screen = EGameXXKScreen::Town;
		OutState.CurrentRegion = UGameXXKMVPRules::RegionQingshan();
		OutState.CurrentMapId = UGameXXKMVPRules::RegionQingshan();
		OutState.QuestState = EGameXXKQuestState::Accepted;
		OutState.bFollowerJoined = true;
		OutState.PlayerGold = PlayerGold;
		if (!UGameXXKMVPRules::EnterDungeon(OutState))
		{
			return false;
		}

		// The pre-integration implementation does not initialize route economy.
		// Keep all later fixtures valid so their REDs isolate resolver behavior.
		if (!OutState.CardRun.bRouteEconomyInitialized)
		{
			FGameXXKRouteEconomyRules::ClearRouteEconomy(OutState.CardRun);
			if (!FGameXXKRouteEconomyRules::InitializeRoute(OutState.CardRun, DefaultRouteBalance))
			{
				return false;
			}
		}
		return true;
	}

	void SetSingleGeneratedNode(
		FGameXXKRuntimeState& State,
		const EGameXXKNodeKind NodeKind,
		const int32 NodeId,
		const EGameXXKScreen Screen,
		const bool bPending)
	{
		State.bDungeonActive = true;
		State.bHasGeneratedRouteMap = true;
		State.Screen = Screen;
		State.CurrentMapId = Screen == EGameXXKScreen::Battle ? FName(TEXT("Battle")) : FName(TEXT("HuangshanRoute"));
		State.CurrentRouteNodeId = NodeId;
		State.PendingRouteNodeId = bPending ? NodeId : INDEX_NONE;
		State.RouteMapNodes.Reset();
		State.RouteMapEdges.Reset();
		State.VisitedRouteNodeIds.Reset();
		State.ReachableRouteNodeIds.Reset();
		State.ReachableRouteNodeIds.Add(NodeId);
		State.RouteMapNodes.Add(FGameXXKRouteMapNode{
			NodeId,
			1,
			0,
			NodeKind,
			FVector2D(0.5f, 0.5f),
			TArray<int32>{}});
	}

	bool ForceActiveCardBattleVictory(FGameXXKRuntimeState& State)
	{
		if (!State.CardRun.bHasActiveCardBattle)
		{
			return false;
		}
		for (FGameXXKCardCombatUnit& Unit : State.CardRun.ActiveBattle.Units)
		{
			if (Unit.Side == EGameXXKCardTargetSide::Enemy)
			{
				Unit.HP = 0;
				Unit.bLiving = false;
			}
		}
		State.CardRun.ActiveBattle.Phase = EGameXXKCardBattlePhase::Victory;
		return true;
	}

	const FGameXXKRouteTravelMoneyReceipt* FindReceipt(
		const FGameXXKRuntimeState& State,
		const int32 Chapter,
		const int32 NodeId)
	{
		return State.CardRun.RewardedTravelMoneyNodes.FindByPredicate(
			[Chapter, NodeId](const FGameXXKRouteTravelMoneyReceipt& Receipt)
			{
				return Receipt.Chapter == Chapter && Receipt.NodeId == NodeId;
			});
	}

	int32 SumRouteAttributes(const FGameXXKRouteAttributeBonuses& Bonuses)
	{
		return Bonuses.MaxHealth + Bonuses.MaxMana + Bonuses.Attack + Bonuses.Defense + Bonuses.Speed;
	}

	bool ResolveCardBattleRewardGate(FGameXXKRuntimeState& State, const bool bBossBattle)
	{
		if (!State.CardRun.bHasActiveCardBattle)
		{
			if (State.Screen != EGameXXKScreen::DungeonMap
				|| !UGameXXKMVPRules::SelectRouteNodeById(State, State.CurrentRouteNodeId))
			{
				return false;
			}
		}
		if (!ForceActiveCardBattleVictory(State))
		{
			return false;
		}
		if (!UGameXXKMVPRules::ResolveBattleVictory(State, bBossBattle))
		{
			return false;
		}
		if (State.CardRun.PendingReward.CardIds.IsEmpty())
		{
			return true;
		}
		FString Error;
		return FGameXXKCardBattleAdapter::SkipPendingRouteReward(State, &Error);
	}

	bool ResolvePendingBattleReward(FGameXXKRuntimeState& State, const bool bBossBattle)
	{
		return ResolveCardBattleRewardGate(State, bBossBattle)
			&& UGameXXKMVPRules::ResolveBattleVictory(State, bBossBattle);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteEconomyIntegrationTest,
	"GameXXK.Route.Economy.Integration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteEconomyIntegrationTest::RunTest(const FString& Parameters)
{
	// A new-game or inactive-route state owns no initialized route economy.
	const FGameXXKRuntimeState NewGame = UGameXXKMVPRules::CreateNewGame();
	TestEqual(TEXT("a new game has zero route travel money"), NewGame.CardRun.RouteTravelMoney, 0);
	TestFalse(TEXT("a new game has no initialized route economy"), NewGame.CardRun.bRouteEconomyInitialized);
	TestTrue(TEXT("a new game has no route award receipts"), NewGame.CardRun.RewardedTravelMoneyNodes.IsEmpty());

	// A genuinely new route must discard stale economy before the idempotent initializer runs.
	FGameXXKRuntimeState EntryState = UGameXXKMVPRules::CreateNewGame();
	EntryState.Screen = EGameXXKScreen::Town;
	EntryState.CurrentRegion = UGameXXKMVPRules::RegionQingshan();
	EntryState.CurrentMapId = UGameXXKMVPRules::RegionQingshan();
	EntryState.QuestState = EGameXXKQuestState::Accepted;
	EntryState.bFollowerJoined = true;
	EntryState.PlayerGold = 913;
	EntryState.CardRun.bRouteEconomyInitialized = true;
	EntryState.CardRun.RouteTravelMoney = 777;
	EntryState.CardRun.RewardedTravelMoneyNodes.Add(FGameXXKRouteTravelMoneyReceipt{2, 44, 9});
	TestTrue(TEXT("entering a new route succeeds"), UGameXXKMVPRules::EnterDungeon(EntryState));
	TestEqual(TEXT("a new route starts with exactly 60 travel money"), EntryState.CardRun.RouteTravelMoney, DefaultRouteBalance);
	TestTrue(TEXT("a new route marks its economy initialized"), EntryState.CardRun.bRouteEconomyInitialized);
	TestTrue(TEXT("a new route clears every prior node receipt"), EntryState.CardRun.RewardedTravelMoneyNodes.IsEmpty());
	TestEqual(TEXT("route entry never changes permanent gold"), EntryState.PlayerGold, 913);

	FGameXXKRuntimeState MissingBattleGateState;
	if (!EnterRouteFixture(MissingBattleGateState))
	{
		AddError(TEXT("missing battle-gate fixture could not enter a valid route"));
		return false;
	}
	SetSingleGeneratedNode(MissingBattleGateState, EGameXXKNodeKind::Battle, 6, EGameXXKScreen::Battle, true);
	const TArray<uint8> MissingBattleGateBefore = SerializeRuntimeState(MissingBattleGateState);
	TestFalse(
		TEXT("a battle node cannot settle without an active resolved card-reward gate"),
		UGameXXKMVPRules::ResolveBattleVictory(MissingBattleGateState, false));
	TestEqual(
		TEXT("rejecting a missing card-battle snapshot preserves the complete runtime"),
		SerializeRuntimeState(MissingBattleGateState),
		MissingBattleGateBefore);

	// The first victory call opens only the three-card gate; settlement happens after it resolves.
	FGameXXKRuntimeState NormalState;
	if (!EnterRouteFixture(NormalState))
	{
		AddError(TEXT("normal battle route fixture could not enter a valid route"));
		return false;
	}
	FString RelicError;
	TestTrue(
		TEXT("the non-currency route-node relic fixture is acquired"),
		FGameXXKRelicRules::AcquireRelic(NormalState, TEXT("Relic.PaperCrane"), &RelicError));
	SetSingleGeneratedNode(NormalState, EGameXXKNodeKind::Battle, 7, EGameXXKScreen::DungeonMap, false);
	TestTrue(TEXT("selecting the normal battle begins card combat"), UGameXXKMVPRules::SelectRouteNodeById(NormalState, 7));
	TestTrue(TEXT("the card battle can be forced into victory"), ForceActiveCardBattleVictory(NormalState));
	const int32 NormalMoneyBeforeGate = NormalState.CardRun.RouteTravelMoney;
	const int32 NormalGoldBeforeGate = NormalState.PlayerGold;
	const int32 NormalXPBeforeGate = NormalState.PlayerXP;
	const int32 NormalPowderBeforeGate = UGameXXKMVPRules::GetItemCount(NormalState, UGameXXKMVPRules::ItemHealingPowder());
	const int32 NormalAttributesBeforeGate = SumRouteAttributes(NormalState.CardRun.RouteAttributeBonuses);
	const int32 NormalReceiptsBeforeGate = NormalState.CardRun.RewardedTravelMoneyNodes.Num();
	TestTrue(TEXT("the first normal victory call creates its reward offer"), UGameXXKMVPRules::ResolveBattleVictory(NormalState, false));
	TestEqual(TEXT("the reward gate offers exactly three cards"), NormalState.CardRun.PendingReward.CardIds.Num(), 3);
	TestEqual(TEXT("the reward gate does not award travel money"), NormalState.CardRun.RouteTravelMoney, NormalMoneyBeforeGate);
	TestEqual(TEXT("the reward gate does not write a node receipt"), NormalState.CardRun.RewardedTravelMoneyNodes.Num(), NormalReceiptsBeforeGate);
	TestEqual(TEXT("the reward gate does not award XP"), NormalState.PlayerXP, NormalXPBeforeGate);
	TestEqual(
		TEXT("the reward gate does not award an item"),
		UGameXXKMVPRules::GetItemCount(NormalState, UGameXXKMVPRules::ItemHealingPowder()),
		NormalPowderBeforeGate);
	TestEqual(TEXT("the reward gate does not trigger route-node relics"), SumRouteAttributes(NormalState.CardRun.RouteAttributeBonuses), NormalAttributesBeforeGate);
	TestEqual(TEXT("the reward gate never changes permanent gold"), NormalState.PlayerGold, NormalGoldBeforeGate);
	FString RewardError;
	TestTrue(
		FString::Printf(TEXT("skipping the saved reward succeeds: %s"), *RewardError),
		FGameXXKCardBattleAdapter::SkipPendingRouteReward(NormalState, &RewardError));
	TestTrue(TEXT("the resolved normal reward settles its node"), UGameXXKMVPRules::ResolveBattleVictory(NormalState, false));
	TestEqual(TEXT("a normal node awards exactly 20 travel money"), NormalState.CardRun.RouteTravelMoney, DefaultRouteBalance + 20);
	TestEqual(TEXT("normal node settlement writes one receipt"), NormalState.CardRun.RewardedTravelMoneyNodes.Num(), 1);
	if (const FGameXXKRouteTravelMoneyReceipt* Receipt = FindReceipt(NormalState, 1, 7))
	{
		TestEqual(TEXT("the normal receipt records exactly 20"), Receipt->Amount, 20);
	}
	else
	{
		AddError(TEXT("the normal node receipt is missing its chapter-scoped key"));
	}
	TestEqual(TEXT("normal node settlement applies its non-money relic once"), SumRouteAttributes(NormalState.CardRun.RouteAttributeBonuses), NormalAttributesBeforeGate + 2);
	TestEqual(TEXT("normal node settlement never changes permanent gold"), NormalState.PlayerGold, NormalGoldBeforeGate);

	// Replaying a saved completion may finish structure but must not repeat any reward side effect.
	SetSingleGeneratedNode(NormalState, EGameXXKNodeKind::Battle, 7, EGameXXKScreen::DungeonMap, false);
	TestTrue(TEXT("the duplicate node reopens and resolves a real card-reward gate"), ResolveCardBattleRewardGate(NormalState, false));
	const int32 DuplicateMoneyBefore = NormalState.CardRun.RouteTravelMoney;
	const int32 DuplicateXPBefore = NormalState.PlayerXP;
	const int32 DuplicatePowderBefore = UGameXXKMVPRules::GetItemCount(NormalState, UGameXXKMVPRules::ItemHealingPowder());
	const int32 DuplicateAttributesBefore = SumRouteAttributes(NormalState.CardRun.RouteAttributeBonuses);
	TestTrue(TEXT("a duplicate normal settlement may finish structural advancement"), UGameXXKMVPRules::ResolveBattleVictory(NormalState, false));
	TestEqual(TEXT("a duplicate receipt awards no travel money"), NormalState.CardRun.RouteTravelMoney, DuplicateMoneyBefore);
	TestEqual(TEXT("a duplicate receipt awards no XP"), NormalState.PlayerXP, DuplicateXPBefore);
	TestEqual(
		TEXT("a duplicate receipt awards no item"),
		UGameXXKMVPRules::GetItemCount(NormalState, UGameXXKMVPRules::ItemHealingPowder()),
		DuplicatePowderBefore);
	TestEqual(TEXT("a duplicate receipt applies no non-money relic effect"), SumRouteAttributes(NormalState.CardRun.RouteAttributeBonuses), DuplicateAttributesBefore);
	TestEqual(TEXT("a duplicate receipt does not append another receipt"), NormalState.CardRun.RewardedTravelMoneyNodes.Num(), 1);

	FGameXXKRuntimeState EliteState;
	if (!EnterRouteFixture(EliteState))
	{
		AddError(TEXT("elite route fixture could not enter a valid route"));
		return false;
	}
	SetSingleGeneratedNode(EliteState, EGameXXKNodeKind::Elite, 8, EGameXXKScreen::DungeonMap, false);
	const int32 EliteGoldBefore = EliteState.PlayerGold;
	TestTrue(TEXT("an elite battle settles after its real card-reward gate"), ResolvePendingBattleReward(EliteState, false));
	TestEqual(TEXT("an elite node awards exactly 35 travel money"), EliteState.CardRun.RouteTravelMoney, DefaultRouteBalance + 35);
	if (const FGameXXKRouteTravelMoneyReceipt* Receipt = FindReceipt(EliteState, 1, 8))
	{
		TestEqual(TEXT("the elite receipt records exactly 35"), Receipt->Amount, 35);
	}
	else
	{
		AddError(TEXT("the elite node receipt is missing"));
	}
	TestEqual(TEXT("an elite award never changes permanent gold"), EliteState.PlayerGold, EliteGoldBefore);

	// Zero-value nodes still lock completion, and route-node relics run only after a new receipt.
	FGameXXKRuntimeState ZeroNodeState;
	if (!EnterRouteFixture(ZeroNodeState))
	{
		AddError(TEXT("zero-node route fixture could not enter a valid route"));
		return false;
	}
	SetSingleGeneratedNode(ZeroNodeState, EGameXXKNodeKind::Start, 3, EGameXXKScreen::DungeonMap, false);
	const int32 ZeroNodeGoldBefore = ZeroNodeState.PlayerGold;
	TestTrue(TEXT("a start node structurally completes"), UGameXXKMVPRules::SelectRouteNodeById(ZeroNodeState, 3));
	TestEqual(TEXT("a zero-value node keeps travel money unchanged"), ZeroNodeState.CardRun.RouteTravelMoney, DefaultRouteBalance);
	if (const FGameXXKRouteTravelMoneyReceipt* Receipt = FindReceipt(ZeroNodeState, 1, 3))
	{
		TestEqual(TEXT("the zero-value node receipt records zero"), Receipt->Amount, 0);
	}
	else
	{
		AddError(TEXT("the zero-value start node does not have a receipt"));
	}
	TestEqual(TEXT("a zero-value node never changes permanent gold"), ZeroNodeState.PlayerGold, ZeroNodeGoldBefore);

	FGameXXKRuntimeState StackedWineState;
	if (!EnterRouteFixture(StackedWineState))
	{
		AddError(TEXT("stacked WineCup route fixture could not enter a valid route"));
		return false;
	}
	TestTrue(TEXT("the first WineCup is acquired"), FGameXXKRelicRules::AcquireRelic(StackedWineState, TEXT("Relic.WineCup"), &RelicError));
	TestTrue(TEXT("the second WineCup becomes a stack"), FGameXXKRelicRules::AcquireRelic(StackedWineState, TEXT("Relic.WineCup"), &RelicError));
	SetSingleGeneratedNode(StackedWineState, EGameXXKNodeKind::Start, 4, EGameXXKScreen::DungeonMap, false);
	TestTrue(TEXT("the stacked-WineCup zero node completes"), UGameXXKMVPRules::SelectRouteNodeById(StackedWineState, 4));
	TestEqual(TEXT("two WineCup stacks add exactly six travel money"), StackedWineState.CardRun.RouteTravelMoney, DefaultRouteBalance + 6);
	if (const FGameXXKRouteTravelMoneyReceipt* Receipt = FindReceipt(StackedWineState, 1, 4))
	{
		TestEqual(TEXT("base and stacked WineCup share one six-money receipt"), Receipt->Amount, 6);
	}
	else
	{
		AddError(TEXT("the stacked WineCup node receipt is missing"));
	}
	const FGameXXKRelicDefinition* WineCupDefinition = FGameXXKRelicCatalog::FindDefinition(TEXT("Relic.WineCup"));
	TestNotNull(TEXT("WineCup remains in the relic catalog"), WineCupDefinition);
	if (WineCupDefinition)
	{
		TestEqual(TEXT("WineCup names route travel money explicitly"), WineCupDefinition->Description.ToString(), FString(TEXT("完成路线节点时获得3行旅钱。")));
	}

	// Treasure acquisition is staged before bonus calculation, so WineCup affects its own node.
	FGameXXKRuntimeState TreasureState;
	if (!EnterRouteFixture(TreasureState))
	{
		AddError(TEXT("treasure route fixture could not enter a valid route"));
		return false;
	}
	SetSingleGeneratedNode(TreasureState, EGameXXKNodeKind::Chest, 10, EGameXXKScreen::RouteEvent, true);
	TreasureState.CardRun.PendingEvent.SourceNodeId = 10;
	TreasureState.CardRun.PendingEvent.EncounterId = TEXT("Encounter.Chest.Bamboo");
	TreasureState.CardRun.PendingRelicOffer.SourceNodeId = 10;
	TreasureState.CardRun.PendingRelicOffer.ChoiceSeed = 1010;
	TreasureState.CardRun.PendingRelicOffer.RelicIds = {
		TEXT("Relic.WineCup"),
		TEXT("Relic.HerbBasket"),
		TEXT("Relic.PaperCrane")};
	TestTrue(TEXT("choosing WineCup resolves the treasure node"), UGameXXKMVPRules::ResolveRouteEncounterChoice(TreasureState, 0));
	TestTrue(TEXT("the treasure grants WineCup"), TreasureState.CardRun.Relics.ContainsByPredicate([](const FGameXXKRelicInstance& Instance)
	{
		return Instance.RelicId == FName(TEXT("Relic.WineCup"));
	}));
	TestEqual(TEXT("a newly chosen WineCup adds three on that same treasure"), TreasureState.CardRun.RouteTravelMoney, DefaultRouteBalance + 3);
	if (const FGameXXKRouteTravelMoneyReceipt* Receipt = FindReceipt(TreasureState, 1, 10))
	{
		TestEqual(TEXT("the treasure receipt atomically includes the WineCup bonus"), Receipt->Amount, 3);
	}
	else
	{
		AddError(TEXT("the treasure WineCup node receipt is missing"));
	}

	// Formal catalog events carry only their explicit attribute/NPC/relic reward.
	FGameXXKRuntimeState FormalEventState;
	if (!EnterRouteFixture(FormalEventState))
	{
		AddError(TEXT("formal event route fixture could not enter a valid route"));
		return false;
	}
	SetSingleGeneratedNode(FormalEventState, EGameXXKNodeKind::Event, 11, EGameXXKScreen::DungeonMap, false);
	TestTrue(TEXT("selecting a formal event creates its saved offer"), UGameXXKMVPRules::SelectRouteNodeById(FormalEventState, 11));
	const FGameXXKRouteEncounterDefinition* FormalEncounter =
		FGameXXKRouteEncounterCatalog::FindDefinition(FormalEventState.CardRun.PendingEvent.EncounterId);
	TestNotNull(TEXT("the formal event definition exists"), FormalEncounter);
	const int32 FormalAttributeChoice = FormalEncounter
		? FormalEncounter->Choices.IndexOfByPredicate([](const FGameXXKRouteEncounterChoiceDefinition& Choice)
		{
			return Choice.RewardKind == EGameXXKRouteEncounterRewardKind::RouteAttribute;
		})
		: INDEX_NONE;
	TestTrue(TEXT("the formal event exposes an attribute choice"), FormalAttributeChoice != INDEX_NONE);
	const int32 FormalAttributeBefore = SumRouteAttributes(FormalEventState.CardRun.RouteAttributeBonuses);
	const int32 FormalGoldBefore = FormalEventState.PlayerGold;
	FGameXXKRuntimeState FormalLegacyBypassState = FormalEventState;
	const TArray<uint8> FormalLegacyBypassBefore = SerializeRuntimeState(FormalLegacyBypassState);
	TestFalse(
		TEXT("a saved formal event cannot bypass its explicit choice through the legacy reward API"),
		UGameXXKMVPRules::ResolveEventReward(FormalLegacyBypassState, true));
	TestEqual(
		TEXT("rejecting the formal-event legacy bypass preserves the complete runtime"),
		SerializeRuntimeState(FormalLegacyBypassState),
		FormalLegacyBypassBefore);
	FGameXXKRuntimeState FormalLegacyItemBypassState = FormalEventState;
	const TArray<uint8> FormalLegacyItemBypassBefore = SerializeRuntimeState(FormalLegacyItemBypassState);
	TestFalse(
		TEXT("a saved formal event cannot bypass its explicit choice through the legacy item API"),
		UGameXXKMVPRules::ResolveEventReward(FormalLegacyItemBypassState, false));
	TestEqual(
		TEXT("rejecting the formal-event item bypass preserves the complete runtime"),
		SerializeRuntimeState(FormalLegacyItemBypassState),
		FormalLegacyItemBypassBefore);
	if (FormalAttributeChoice != INDEX_NONE)
	{
		TestTrue(TEXT("the formal event choice resolves"), UGameXXKMVPRules::ResolveRouteEncounterChoice(FormalEventState, FormalAttributeChoice));
	}
	TestTrue(TEXT("the formal event applies its explicit attribute"), SumRouteAttributes(FormalEventState.CardRun.RouteAttributeBonuses) > FormalAttributeBefore);
	TestEqual(TEXT("a formal event adds no hidden base travel money"), FormalEventState.CardRun.RouteTravelMoney, DefaultRouteBalance);
	if (const FGameXXKRouteTravelMoneyReceipt* Receipt = FindReceipt(FormalEventState, 1, 11))
	{
		TestEqual(TEXT("the formal event records a zero-value receipt"), Receipt->Amount, 0);
	}
	else
	{
		AddError(TEXT("the formal event receipt is missing"));
	}
	TestEqual(TEXT("a formal event never changes permanent gold"), FormalEventState.PlayerGold, FormalGoldBefore);

	FGameXXKRuntimeState GeneratedMapLegacyBypass;
	if (!EnterRouteFixture(GeneratedMapLegacyBypass))
	{
		AddError(TEXT("generated-map legacy bypass fixture could not enter a valid route"));
		return false;
	}
	SetSingleGeneratedNode(GeneratedMapLegacyBypass, EGameXXKNodeKind::Event, 12, EGameXXKScreen::DungeonMap, false);
	const TArray<uint8> GeneratedMapLegacyBypassBefore = SerializeRuntimeState(GeneratedMapLegacyBypass);
	TestFalse(
		TEXT("the legacy event API cannot complete a generated event directly from the route map"),
		UGameXXKMVPRules::ResolveEventReward(GeneratedMapLegacyBypass, true));
	TestEqual(
		TEXT("rejecting the generated-map legacy bypass preserves the complete runtime"),
		SerializeRuntimeState(GeneratedMapLegacyBypass),
		GeneratedMapLegacyBypassBefore);

	FGameXXKRuntimeState LegacyMoneyEvent;
	if (!EnterRouteFixture(LegacyMoneyEvent))
	{
		AddError(TEXT("legacy money event fixture could not enter a valid route"));
		return false;
	}
	SetSingleGeneratedNode(LegacyMoneyEvent, EGameXXKNodeKind::Event, 17, EGameXXKScreen::RouteEvent, true);
	const int32 LegacyMoneyGoldBefore = LegacyMoneyEvent.PlayerGold;
	TestTrue(TEXT("the legacy take-money event resolves"), UGameXXKMVPRules::ResolveEventReward(LegacyMoneyEvent, true));
	TestEqual(TEXT("the legacy take-money event awards exactly 20"), LegacyMoneyEvent.CardRun.RouteTravelMoney, DefaultRouteBalance + 20);
	if (const FGameXXKRouteTravelMoneyReceipt* Receipt = FindReceipt(LegacyMoneyEvent, 1, 17))
	{
		TestEqual(TEXT("the legacy money event receipt records 20"), Receipt->Amount, 20);
	}
	else
	{
		AddError(TEXT("the legacy take-money event receipt is missing"));
	}
	TestEqual(TEXT("the legacy take-money event never changes permanent gold"), LegacyMoneyEvent.PlayerGold, LegacyMoneyGoldBefore);

	FGameXXKRuntimeState LegacyItemEvent;
	if (!EnterRouteFixture(LegacyItemEvent))
	{
		AddError(TEXT("legacy item event fixture could not enter a valid route"));
		return false;
	}
	SetSingleGeneratedNode(LegacyItemEvent, EGameXXKNodeKind::Event, 13, EGameXXKScreen::RouteEvent, true);
	const int32 LegacyItemGoldBefore = LegacyItemEvent.PlayerGold;
	TestTrue(TEXT("the legacy non-money event resolves"), UGameXXKMVPRules::ResolveEventReward(LegacyItemEvent, false));
	TestEqual(TEXT("the legacy non-money event adds zero travel money"), LegacyItemEvent.CardRun.RouteTravelMoney, DefaultRouteBalance);
	if (const FGameXXKRouteTravelMoneyReceipt* Receipt = FindReceipt(LegacyItemEvent, 1, 13))
	{
		TestEqual(TEXT("the legacy non-money event receipt records zero"), Receipt->Amount, 0);
	}
	else
	{
		AddError(TEXT("the legacy non-money event receipt is missing"));
	}
	TestEqual(TEXT("the legacy non-money event never changes permanent gold"), LegacyItemEvent.PlayerGold, LegacyItemGoldBefore);

	// Every economy error must roll back the complete runtime, not only the balance.
	FGameXXKRuntimeState OverflowState;
	if (!EnterRouteFixture(OverflowState))
	{
		AddError(TEXT("overflow fixture could not enter a valid route"));
		return false;
	}
	SetSingleGeneratedNode(OverflowState, EGameXXKNodeKind::Battle, 14, EGameXXKScreen::DungeonMap, false);
	TestTrue(TEXT("overflow fixture resolves its card-reward gate before corruption"), ResolveCardBattleRewardGate(OverflowState, false));
	OverflowState.CardRun.RouteTravelMoney = MAX_int32;
	const FGameXXKRuntimeState OverflowBefore = OverflowState;
	TestFalse(TEXT("an overflowing normal award is rejected"), UGameXXKMVPRules::ResolveBattleVictory(OverflowState, false));
	TestTrue(TEXT("overflow rejection preserves every reflected runtime property"),
		FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(&OverflowState, &OverflowBefore, PPF_None));

	FGameXXKRuntimeState UninitializedState;
	if (!EnterRouteFixture(UninitializedState))
	{
		AddError(TEXT("uninitialized economy fixture could not enter a valid route"));
		return false;
	}
	SetSingleGeneratedNode(UninitializedState, EGameXXKNodeKind::Battle, 15, EGameXXKScreen::DungeonMap, false);
	TestTrue(TEXT("uninitialized fixture resolves its card-reward gate before corruption"), ResolveCardBattleRewardGate(UninitializedState, false));
	UninitializedState.CardRun.bRouteEconomyInitialized = false;
	const FGameXXKRuntimeState UninitializedBefore = UninitializedState;
	TestFalse(TEXT("a node cannot resolve against uninitialized route economy"), UGameXXKMVPRules::ResolveBattleVictory(UninitializedState, false));
	TestTrue(TEXT("uninitialized-economy rejection preserves every reflected runtime property"),
		FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(&UninitializedState, &UninitializedBefore, PPF_None));

	FGameXXKRuntimeState NegativeMerchantState;
	if (!EnterRouteFixture(NegativeMerchantState))
	{
		AddError(TEXT("negative merchant fixture could not enter a valid route"));
		return false;
	}
	SetSingleGeneratedNode(NegativeMerchantState, EGameXXKNodeKind::Merchant, 16, EGameXXKScreen::RouteMerchant, true);
	NegativeMerchantState.CardRun.RouteTravelMoney = -1;
	const TArray<uint8> NegativeMerchantBefore = SerializeRuntimeState(NegativeMerchantState);
	TestFalse(TEXT("a merchant cannot clamp and resolve corrupted negative economy"), UGameXXKMVPRules::ResolveMerchantRouteNode(NegativeMerchantState));
	TestEqual(TEXT("negative merchant rejection preserves the complete runtime"), SerializeRuntimeState(NegativeMerchantState), NegativeMerchantBefore);

	// Boss awards settle before chapter advancement. Same node IDs remain distinct by chapter.
	FGameXXKRuntimeState BossState;
	if (!EnterRouteFixture(BossState))
	{
		AddError(TEXT("boss route fixture could not enter a valid route"));
		return false;
	}
	const int32 BossGoldBefore = BossState.PlayerGold;
	SetSingleGeneratedNode(BossState, EGameXXKNodeKind::Boss, 21, EGameXXKScreen::DungeonMap, false);
	TestTrue(TEXT("chapter-one Boss victory resolves"), ResolvePendingBattleReward(BossState, true));
	TestEqual(TEXT("chapter-one Boss awards exactly 50 before advancing"), BossState.CardRun.RouteTravelMoney, DefaultRouteBalance + 50);
	TestEqual(TEXT("chapter-one Boss advances to chapter two"), BossState.CardRun.RouteProgress.CurrentChapter, 2);
	TestTrue(TEXT("chapter-one Boss preserves initialized economy"), BossState.CardRun.bRouteEconomyInitialized);
	TestNotNull(TEXT("chapter-one Boss keeps its chapter-scoped receipt"), FindReceipt(BossState, 1, 21));
	TestEqual(TEXT("chapter-one Boss does not change permanent gold"), BossState.PlayerGold, BossGoldBefore);

	SetSingleGeneratedNode(BossState, EGameXXKNodeKind::Boss, 21, EGameXXKScreen::DungeonMap, false);
	TestTrue(TEXT("chapter-two Boss victory resolves"), ResolvePendingBattleReward(BossState, true));
	TestEqual(TEXT("chapter-two same-node ID receives its own 50"), BossState.CardRun.RouteTravelMoney, DefaultRouteBalance + 100);
	TestEqual(TEXT("chapter-two Boss advances to chapter three"), BossState.CardRun.RouteProgress.CurrentChapter, 3);
	TestTrue(TEXT("chapter-two Boss preserves initialized economy"), BossState.CardRun.bRouteEconomyInitialized);
	TestEqual(TEXT("chapter one and two preserve both receipts"), BossState.CardRun.RewardedTravelMoneyNodes.Num(), 2);
	TestNotNull(TEXT("chapter two uses a distinct same-node-ID receipt"), FindReceipt(BossState, 2, 21));

	const int32 GoldBeforeTerminalBoss = BossState.PlayerGold;
	SetSingleGeneratedNode(BossState, EGameXXKNodeKind::Boss, 21, EGameXXKScreen::DungeonMap, false);
	TestTrue(TEXT("chapter-three Boss victory resolves the route"), ResolvePendingBattleReward(BossState, true));
	TestEqual(TEXT("chapter-three clear converts post-Boss travel money ten to one"), BossState.PlayerGold, GoldBeforeTerminalBoss + (DefaultRouteBalance + 150) / 10);
	TestFalse(TEXT("chapter-three clear ends the active route"), BossState.bDungeonActive);
	TestEqual(TEXT("chapter-three clear zeros travel money"), BossState.CardRun.RouteTravelMoney, 0);
	TestFalse(TEXT("chapter-three clear removes the economy initialization"), BossState.CardRun.bRouteEconomyInitialized);
	TestTrue(TEXT("chapter-three clear removes all node receipts"), BossState.CardRun.RewardedTravelMoneyNodes.IsEmpty());
	TestTrue(TEXT("chapter-three clear preserves the applied settlement ID"), BossState.CardRun.LastAppliedRouteSettlementId.IsValid());

	FGameXXKRuntimeState FailureState;
	if (!EnterRouteFixture(FailureState))
	{
		AddError(TEXT("failure settlement fixture could not enter a valid route"));
		return false;
	}
	bool bSeedAwarded = false;
	TestTrue(
		TEXT("failure fixture records one seven-money node"),
		FGameXXKRouteEconomyRules::AwardNodeOnce(FailureState.CardRun, 1, 31, 7, bSeedAwarded));
	TestTrue(TEXT("failure fixture node is newly awarded"), bSeedAwarded);
	FGameXXKRuntimeState AbandonState = FailureState;
	const int32 FailureGoldBefore = FailureState.PlayerGold;
	TestTrue(TEXT("route defeat settles"), UGameXXKMVPRules::FailDungeonToTown(FailureState));
	TestEqual(TEXT("route defeat converts travel money twenty to one"), FailureState.PlayerGold, FailureGoldBefore + 67 / 20);
	TestEqual(TEXT("route defeat clears travel money"), FailureState.CardRun.RouteTravelMoney, 0);
	TestFalse(TEXT("route defeat clears economy initialization"), FailureState.CardRun.bRouteEconomyInitialized);
	TestTrue(TEXT("route defeat clears receipts"), FailureState.CardRun.RewardedTravelMoneyNodes.IsEmpty());
	TestTrue(TEXT("route defeat preserves LastAppliedRouteSettlementId"), FailureState.CardRun.LastAppliedRouteSettlementId.IsValid());

	const int32 AbandonGoldBefore = AbandonState.PlayerGold;
	TestTrue(TEXT("route abandon settles"), UGameXXKMVPRules::AbandonDungeonToTown(AbandonState));
	TestEqual(TEXT("route abandon converts travel money twenty to one"), AbandonState.PlayerGold, AbandonGoldBefore + 67 / 20);
	TestEqual(TEXT("route abandon clears travel money"), AbandonState.CardRun.RouteTravelMoney, 0);
	TestFalse(TEXT("route abandon clears economy initialization"), AbandonState.CardRun.bRouteEconomyInitialized);
	TestTrue(TEXT("route abandon clears receipts"), AbandonState.CardRun.RewardedTravelMoneyNodes.IsEmpty());
	TestTrue(TEXT("route abandon preserves LastAppliedRouteSettlementId"), AbandonState.CardRun.LastAppliedRouteSettlementId.IsValid());

	return true;
}

#endif
