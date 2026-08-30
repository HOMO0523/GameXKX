#include "GameXXKCardBattleAdapter.h"
#include "GameXXKMVPRules.h"
#include "GameXXKPermanentPartyTestFixtures.h"

#include "GameXXKCardCatalog.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKRouteRewardEntryAcquisitionTest
{
	int32 GetRewardSourceNodeId(const FGameXXKRuntimeState& State)
	{
		return State.CardRun.ActiveBattleSourceNodeId >= 0
			? State.CardRun.ActiveBattleSourceNodeId
			: State.DungeonNodeIndex;
	}

	bool StartRewardReadyBossState(FGameXXKRuntimeState& OutState)
	{
		OutState = GameXXKPermanentPartyTestFixtures::MakeStartedState();
		if (!UGameXXKMVPRules::OpenWorldMap(OutState)
			|| !UGameXXKMVPRules::EnterWorldRegion(OutState, UGameXXKMVPRules::RegionQingshan())
			|| !UGameXXKMVPRules::AcceptTownQuest(OutState)
			|| !UGameXXKMVPRules::EnterDungeon(OutState))
		{
			return false;
		}

		OutState.bHasGeneratedRouteMap = false;
		OutState.RouteMapNodes.Reset();
		OutState.RouteMapEdges.Reset();
		OutState.ReachableRouteNodeIds.Reset();
		const int32 BossNodeIndex = UGameXXKMVPRules::GetFixedDungeonNodes(0).IndexOfByKey(EGameXXKNodeKind::Boss);
		if (BossNodeIndex == INDEX_NONE)
		{
			return false;
		}
		OutState.DungeonNodeIndex = BossNodeIndex;
		if (!UGameXXKMVPRules::AdvanceDungeonNode(OutState, EGameXXKNodeKind::Boss))
		{
			return false;
		}

		for (FGameXXKCardCombatUnit& Unit : OutState.CardRun.ActiveBattle.Units)
		{
			if (Unit.Side == EGameXXKCardTargetSide::Enemy)
			{
				Unit.HP = 0;
				Unit.bLiving = false;
			}
		}
		OutState.CardRun.ActiveBattle.Phase = EGameXXKCardBattlePhase::Victory;
		OutState.CardRun.PendingReward = FGameXXKPendingRouteCardReward();
		OutState.CardRun.bActiveBattleRewardResolved = false;
		return OutState.bDungeonActive
			&& OutState.CardRun.bLoadoutLockedForRoute
			&& OutState.CardRun.bHasActiveCardBattle
			&& GetRewardSourceNodeId(OutState) >= 0;
	}

	bool SetPendingBossReward(FGameXXKRuntimeState& State, FName& OutBossCardId, const int32 ChoiceSeed = 0x5A17)
	{
		OutBossCardId = NAME_None;
		FString Error;
		if (!FGameXXKCardBattleAdapter::CreateTieredBattleRewardOffer(
			State,
			EGameXXKNodeKind::Boss,
			GetRewardSourceNodeId(State),
			ChoiceSeed,
			&Error))
		{
			return false;
		}
		for (const FGameXXKBattleRewardOption& Option : State.CardRun.PendingReward.Options)
		{
			if (Option.Kind == EGameXXKBattleRewardKind::BossCard)
			{
				OutBossCardId = Option.CardId;
			}
		}
		State.CardRun.bActiveBattleRewardResolved = false;
		return !OutBossCardId.IsNone();
	}

	int32 CountHandCardsById(const FGameXXKRuntimeState& State, const FName CardId)
	{
		int32 Count = 0;
		for (const FGameXXKCardInstance& Instance : State.CardRun.ActiveBattle.Deck.Hand)
		{
			Count += Instance.CardId == CardId ? 1 : 0;
		}
		return Count;
	}

	TArray<FName> GetAllRouteOwnedCardIds()
	{
		TArray<FName> CardIds;
		for (const FGameXXKCardDefinition& Definition : FGameXXKCardCatalog::GetAllCardDefinitions())
		{
			if (Definition.Owner == EGameXXKCardOwner::Route)
			{
				CardIds.Add(Definition.Id);
			}
		}
		return CardIds;
	}

	FName PickRouteCardDifferentFrom(const TArray<FName>& CardIds, const FName ExcludedCardId)
	{
		for (const FName CardId : CardIds)
		{
			if (CardId != ExcludedCardId)
			{
				return CardId;
			}
		}
		return NAME_None;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteRewardBossPreviewTest,
	"GameXXK.Integration.CardRoute.BossCardSlots.Preview",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteRewardBossPreviewTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKRouteRewardEntryAcquisitionTest;
	FGameXXKRuntimeState State;
	if (!TestTrue(TEXT("preview fixture enters a canonical reward-ready boss route"), StartRewardReadyBossState(State)))
	{
		return false;
	}
	FName BossCardId;
	if (!TestTrue(TEXT("the boss tiered offer names its boss-card option"), SetPendingBossReward(State, BossCardId)))
	{
		return false;
	}

	FString Error;
	FGameXXKRouteCardAcquisitionPreview Preview;
	TestTrue(FString::Printf(TEXT("a free boss slot previews the saved boss card directly: %s"), *Error),
		FGameXXKCardBattleAdapter::PreviewPendingRouteReward(State, BossCardId, NAME_None, Preview, &Error));
	TestEqual(TEXT("a free boss slot previews an immediate commit"), Preview.Decision, EGameXXKRouteCardAcquisitionDecision::CanCommit);
	TestEqual(TEXT("the preview exposes the exact boss card"), Preview.CardId, BossCardId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteRewardBossCommitTest,
	"GameXXK.Integration.CardRoute.BossCardSlots.Commit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteRewardBossCommitTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKRouteRewardEntryAcquisitionTest;
	FGameXXKRuntimeState State;
	if (!TestTrue(TEXT("commit fixture enters a canonical reward-ready boss route"), StartRewardReadyBossState(State)))
	{
		return false;
	}
	FName BossCardId;
	if (!TestTrue(TEXT("commit fixture resolves the boss-card option"), SetPendingBossReward(State, BossCardId)))
	{
		return false;
	}

	const int32 HandCountBefore = State.CardRun.ActiveBattle.Deck.Hand.Num();
	const int32 BossInHandBefore = CountHandCardsById(State, BossCardId);
	FString Error;
	TestTrue(FString::Printf(TEXT("the boss card commits through the shared reward rules: %s"), *Error),
		FGameXXKCardBattleAdapter::CommitBossCardReward(State, BossCardId, &Error));
	TestEqual(TEXT("commit writes exactly one boss card slot"), State.CardRun.BossCardSlots.Num(), 1);
	TestEqual(TEXT("commit records the chosen boss card in the slot"), State.CardRun.BossCardSlots[0], BossCardId);
	TestEqual(TEXT("commit adds exactly one boss card instance to the hand"),
		CountHandCardsById(State, BossCardId),
		BossInHandBefore + 1);
	TestEqual(TEXT("commit grows the hand by exactly one card"), State.CardRun.ActiveBattle.Deck.Hand.Num(), HandCountBefore + 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteRewardDuplicateBossRejectedTest,
	"GameXXK.Integration.CardRoute.BossCardSlots.DuplicateRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteRewardDuplicateBossRejectedTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKRouteRewardEntryAcquisitionTest;
	FGameXXKRuntimeState State;
	if (!TestTrue(TEXT("duplicate fixture enters a canonical reward-ready boss route"), StartRewardReadyBossState(State)))
	{
		return false;
	}
	FName BossCardId;
	if (!TestTrue(TEXT("duplicate fixture resolves the boss-card option"), SetPendingBossReward(State, BossCardId)))
	{
		return false;
	}

	FString Error;
	TestTrue(TEXT("the boss card commits once"), FGameXXKCardBattleAdapter::CommitBossCardReward(State, BossCardId, &Error));
	Error.Reset();
	TestFalse(TEXT("committing the same boss card again is rejected"),
		FGameXXKCardBattleAdapter::CommitBossCardReward(State, BossCardId, &Error));
	TestTrue(TEXT("the duplicate commit reports a concrete error"), !Error.IsEmpty());
	TestEqual(TEXT("the duplicate commit leaves exactly one slot occupied"), State.CardRun.BossCardSlots.Num(), 1);
	TestEqual(TEXT("the duplicate commit adds no second hand instance"), CountHandCardsById(State, BossCardId), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteRewardFourthSlotRejectedTest,
	"GameXXK.Integration.CardRoute.BossCardSlots.FourthSlotRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteRewardFourthSlotRejectedTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKRouteRewardEntryAcquisitionTest;
	FGameXXKRuntimeState State;
	if (!TestTrue(TEXT("full-slot fixture enters a canonical reward-ready boss route"), StartRewardReadyBossState(State)))
	{
		return false;
	}

	const TArray<FName> RouteOwnedCardIds = GetAllRouteOwnedCardIds();
	if (!TestTrue(TEXT("the catalog supplies at least four distinct route cards for the full-slot fixture"), RouteOwnedCardIds.Num() >= 4))
	{
		return false;
	}
	State.CardRun.BossCardSlots = { RouteOwnedCardIds[0], RouteOwnedCardIds[1], RouteOwnedCardIds[2] };
	const FName FourthCardId = RouteOwnedCardIds[3];

	FGameXXKBattleRewardOption BossOption;
	BossOption.Kind = EGameXXKBattleRewardKind::BossCard;
	BossOption.CardId = FourthCardId;
	FGameXXKBattleRewardOption RelicOptionA;
	RelicOptionA.Kind = EGameXXKBattleRewardKind::Relic;
	RelicOptionA.RelicId = TEXT("Relic.Fixture.A");
	FGameXXKBattleRewardOption RelicOptionB;
	RelicOptionB.Kind = EGameXXKBattleRewardKind::Relic;
	RelicOptionB.RelicId = TEXT("Relic.Fixture.B");
	State.CardRun.PendingReward.SourceNodeId = GetRewardSourceNodeId(State);
	State.CardRun.PendingReward.ChoiceSeed = 0x5A17;
	State.CardRun.PendingReward.Options = { BossOption, RelicOptionA, RelicOptionB };

	FString Error;
	TestFalse(TEXT("a fourth boss card is rejected when every boss slot is full"),
		FGameXXKCardBattleAdapter::CommitBossCardReward(State, FourthCardId, &Error));
	TestTrue(TEXT("the full-slot rejection reports a concrete error"), !Error.IsEmpty());
	TestEqual(TEXT("the full-slot rejection leaves all three slots unchanged"), State.CardRun.BossCardSlots.Num(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteRewardResolveChoiceFinishTest,
	"GameXXK.Integration.CardRoute.BossCardSlots.ResolveChoiceFinish",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteRewardResolveChoiceFinishTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKRouteRewardEntryAcquisitionTest;
	FGameXXKRuntimeState State;
	if (!TestTrue(TEXT("resolve fixture enters a canonical reward-ready boss route"), StartRewardReadyBossState(State)))
	{
		return false;
	}
	FName BossCardId;
	if (!TestTrue(TEXT("resolve fixture resolves the boss-card option"), SetPendingBossReward(State, BossCardId)))
	{
		return false;
	}

	int32 BossOptionIndex = INDEX_NONE;
	for (int32 Index = 0; Index < State.CardRun.PendingReward.Options.Num(); ++Index)
	{
		if (State.CardRun.PendingReward.Options[Index].Kind == EGameXXKBattleRewardKind::BossCard)
		{
			BossOptionIndex = Index;
		}
	}
	if (!TestTrue(TEXT("the saved boss offer exposes a boss-card option index"), BossOptionIndex != INDEX_NONE))
	{
		return false;
	}

	FString Error;
	TestTrue(FString::Printf(TEXT("choosing the boss reward finishes the gated victory: %s"), *Error),
		UGameXXKMVPRules::ResolvePendingBattleRewardChoiceAndFinish(State, BossOptionIndex, NAME_None, &Error));
	TestTrue(TEXT("finishing the choice clears the saved tiered offer"), State.CardRun.PendingReward.Options.IsEmpty());
	TestTrue(TEXT("finishing the choice clears the saved pending card payload"), State.CardRun.PendingReward.CardIds.IsEmpty());
	TestEqual(TEXT("the finished boss reward occupies exactly one boss slot"), State.CardRun.BossCardSlots.Num(), 1);
	TestEqual(TEXT("the finished boss reward records the chosen card"), State.CardRun.BossCardSlots[0], BossCardId);
	TestFalse(TEXT("the finished victory settlement clears the reward-resolution gate"), State.CardRun.bActiveBattleRewardResolved);
	TestFalse(TEXT("the finished victory settlement clears the active card battle"), State.CardRun.bHasActiveCardBattle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteRewardPreviewNotInOfferTest,
	"GameXXK.Integration.CardRoute.BossCardSlots.PreviewNotInOffer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteRewardPreviewNotInOfferTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKRouteRewardEntryAcquisitionTest;
	FGameXXKRuntimeState State;
	if (!TestTrue(TEXT("absent-card fixture enters a canonical reward-ready boss route"), StartRewardReadyBossState(State)))
	{
		return false;
	}
	FName BossCardId;
	if (!TestTrue(TEXT("absent-card fixture resolves the boss-card option"), SetPendingBossReward(State, BossCardId)))
	{
		return false;
	}

	const FName UnofferedCardId = PickRouteCardDifferentFrom(GetAllRouteOwnedCardIds(), BossCardId);
	if (!TestTrue(TEXT("the catalog supplies a route card absent from the saved offer"), !UnofferedCardId.IsNone()))
	{
		return false;
	}

	FString Error;
	FGameXXKRouteCardAcquisitionPreview Preview;
	TestFalse(TEXT("preview rejects a boss card that is not part of the saved offer"),
		FGameXXKCardBattleAdapter::PreviewPendingRouteReward(State, UnofferedCardId, NAME_None, Preview, &Error));
	TestTrue(TEXT("the absent-card preview reports a concrete error"), !Error.IsEmpty());
	return true;
}

#endif
