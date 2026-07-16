#include "GameXXKCardRules.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardRulesTest,
	"GameXXK.Data.CardRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	TArray<FGameXXKCardInstance> MakeInstances(const int32 Count = 18)
	{
		TArray<FGameXXKCardInstance> Result;
		Result.Reserve(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			FGameXXKCardInstance Instance;
			Instance.InstanceId = FName(*FString::Printf(TEXT("Instance.%02d"), Index));
			Instance.CardId = FName(*FString::Printf(TEXT("Card.%02d"), Index % 3));
			Instance.OwnerUnitId = TEXT("Player");
			Instance.SourceEntryId = FName(*FString::Printf(TEXT("Entry.%02d"), Index));
			Instance.AcquisitionOrdinal = Index;
			Result.Add(Instance);
		}
		return Result;
	}

	FString InstanceIds(const TArray<FGameXXKCardInstance>& Instances)
	{
		FString Result;
		for (const FGameXXKCardInstance& Instance : Instances)
		{
			if (!Result.IsEmpty())
			{
				Result += TEXT(",");
			}
			Result += Instance.InstanceId.ToString();
		}
		return Result;
	}

	FString InstanceSnapshot(const FGameXXKCardInstance& Instance)
	{
		return FString::Printf(
			TEXT("{%s|%s|%s|%s|%d}"),
			*Instance.InstanceId.ToString(),
			*Instance.CardId.ToString(),
			*Instance.OwnerUnitId.ToString(),
			*Instance.SourceEntryId.ToString(),
			Instance.AcquisitionOrdinal);
	}

	FString InstanceSnapshots(const TArray<FGameXXKCardInstance>& Instances)
	{
		FString Result;
		for (const FGameXXKCardInstance& Instance : Instances)
		{
			Result += InstanceSnapshot(Instance);
		}
		return Result;
	}

	FString NameIds(const TArray<FName>& InstanceIdsToJoin)
	{
		FString Result;
		for (const FName InstanceId : InstanceIdsToJoin)
		{
			if (!Result.IsEmpty())
			{
				Result += TEXT(",");
			}
			Result += InstanceId.ToString();
		}
		return Result;
	}

	FString DeckSnapshot(const FGameXXKBattleDeckState& Deck)
	{
		return FString::Printf(
			TEXT("seed=%d;state=%d;energy=%d;limit=%d;active=%s;draw=%s;hand=%s;discard=%s;pending={kind=%d;required=%d;discard=%d;pick=%d;candidates=%s;insight=%s;picked=%s;reordered=%s;cancel=%d;preserve=%d}"),
			Deck.InitialRandomSeed,
			Deck.CurrentRandomState,
			Deck.SharedEnergy,
			Deck.HandLimit,
			*NameIds(Deck.ActiveInstanceIds),
			*InstanceSnapshots(Deck.DrawPile),
			*InstanceSnapshots(Deck.Hand),
			*InstanceSnapshots(Deck.DiscardPile),
			static_cast<int32>(Deck.PendingChoice.Kind),
			Deck.PendingChoice.RequiredCount,
			Deck.PendingChoice.RequiredDiscardCount,
			Deck.PendingChoice.RequiredHandPickCount,
			*InstanceSnapshots(Deck.PendingChoice.Candidates),
			*NameIds(Deck.PendingChoice.InsightTopOrder),
			*Deck.PendingChoice.InsightPickedInstanceId.ToString(),
			*NameIds(Deck.PendingChoice.InsightReorderedInstanceIds),
			static_cast<int32>(Deck.PendingChoice.bCanCancel),
			static_cast<int32>(Deck.PendingChoice.bCancelPreservesDrawTop));
	}

	bool IsInAnyZone(const FGameXXKBattleDeckState& Deck, const FName InstanceId)
	{
		return Deck.DrawPile.ContainsByPredicate([InstanceId](const FGameXXKCardInstance& Instance) { return Instance.InstanceId == InstanceId; })
			|| Deck.Hand.ContainsByPredicate([InstanceId](const FGameXXKCardInstance& Instance) { return Instance.InstanceId == InstanceId; })
			|| Deck.DiscardPile.ContainsByPredicate([InstanceId](const FGameXXKCardInstance& Instance) { return Instance.InstanceId == InstanceId; });
	}

	TArray<FName> TopIds(const FGameXXKBattleDeckState& Deck, const int32 Count)
	{
		TArray<FName> Result;
		for (int32 Index = 0; Index < Count && Index < Deck.DrawPile.Num(); ++Index)
		{
			Result.Add(Deck.DrawPile[Deck.DrawPile.Num() - 1 - Index].InstanceId);
		}
		return Result;
	}
}

bool FGameXXKCardRulesTest::RunTest(const FString& Parameters)
{
	// Effect and turn resolution intentionally belong to the following card-runtime task.
	TestEqual(TEXT("pending choice invalid value remains stable"), static_cast<uint8>(EGameXXKCardPendingChoiceKind::Invalid), static_cast<uint8>(0));
	TestEqual(TEXT("pending choice none value remains stable"), static_cast<uint8>(EGameXXKCardPendingChoiceKind::None), static_cast<uint8>(1));
	TestEqual(TEXT("pending choice forced discard value remains stable"), static_cast<uint8>(EGameXXKCardPendingChoiceKind::ForcedDiscard), static_cast<uint8>(2));
	TestEqual(TEXT("pending choice insight value remains stable"), static_cast<uint8>(EGameXXKCardPendingChoiceKind::InsightChooseToHand), static_cast<uint8>(3));

	const TArray<FGameXXKCardInstance> Instances = MakeInstances();
	FGameXXKBattleDeckState Deck;
	TestTrue(TEXT("18 unique instances initialize"), GameXXKCardRules::InitializeBattleDeck(Deck, Instances, 1207));
	TestEqual(TEXT("18 instances start as exactly five in hand"), Deck.Hand.Num(), 5);
	TestEqual(TEXT("18 instances leave thirteen in draw pile"), Deck.DrawPile.Num(), 13);
	TestEqual(TEXT("battle starts with no discarded cards"), Deck.DiscardPile.Num(), 0);
	TestEqual(TEXT("battle starts at three shared energy"), Deck.SharedEnergy, 3);
	TestEqual(TEXT("battle starts with the normal hand limit"), Deck.HandLimit, 5);
	TestEqual(TEXT("initial pending choice is none"), Deck.PendingChoice.Kind, EGameXXKCardPendingChoiceKind::None);
	TestTrue(TEXT("initial zones pass the uniqueness invariant"), GameXXKCardRules::ValidateDeckState(Deck));

	FGameXXKBattleDeckState SameSeedDeck;
	TestTrue(TEXT("same seed deck initializes"), GameXXKCardRules::InitializeBattleDeck(SameSeedDeck, Instances, 1207));
	TestEqual(TEXT("same seed produces exactly the same logical deck order"), DeckSnapshot(SameSeedDeck), DeckSnapshot(Deck));
	TArray<FGameXXKCardInstance> DuplicateInputInstances = Instances;
	DuplicateInputInstances[1].InstanceId = DuplicateInputInstances[0].InstanceId;
	const FString InvalidInitializeSnapshot = DeckSnapshot(Deck);
	TestFalse(TEXT("initialization rejects duplicate instance IDs"), GameXXKCardRules::InitializeBattleDeck(Deck, DuplicateInputInstances, 1207));
	TestEqual(TEXT("invalid initialization preserves every persisted deck field"), DeckSnapshot(Deck), InvalidInitializeSnapshot);
	for (const FGameXXKCardInstance& Instance : Instances)
	{
		TestTrue(FString::Printf(TEXT("instance %s occurs in one logical zone"), *Instance.InstanceId.ToString()), IsInAnyZone(Deck, Instance.InstanceId));
	}
	FGameXXKBattleDeckState DuplicateZoneDeck = Deck;
	DuplicateZoneDeck.DrawPile.Add(DuplicateZoneDeck.Hand[0]);
	TestFalse(TEXT("the same instance cannot occupy hand and draw pile simultaneously"), GameXXKCardRules::ValidateDeckState(DuplicateZoneDeck));
	FGameXXKBattleDeckState OversizedHandLimitDeck = Deck;
	OversizedHandLimitDeck.HandLimit = MAX_int32;
	const FString OversizedHandLimitSnapshot = DeckSnapshot(OversizedHandLimitDeck);
	TestFalse(TEXT("serialized hand limits cannot overflow temporary-overdraw arithmetic"), GameXXKCardRules::DrawCards(OversizedHandLimitDeck, 1, true));
	TestEqual(TEXT("oversized hand limit rejection preserves every persisted deck field"), DeckSnapshot(OversizedHandLimitDeck), OversizedHandLimitSnapshot);
	FGameXXKBattleDeckState StaleNoneChoiceDeck = Deck;
	StaleNoneChoiceDeck.PendingChoice.Candidates.Add(StaleNoneChoiceDeck.Hand[0]);
	const FString StaleNoneChoiceSnapshot = DeckSnapshot(StaleNoneChoiceDeck);
	TestFalse(TEXT("no-pending-choice state rejects stale candidate UI data"), GameXXKCardRules::DrawCards(StaleNoneChoiceDeck, 1, false));
	TestEqual(TEXT("stale no-choice data rejection preserves every persisted deck field"), DeckSnapshot(StaleNoneChoiceDeck), StaleNoneChoiceSnapshot);

	const FString FullHandSnapshot = DeckSnapshot(Deck);
	TestTrue(TEXT("normal draw at the hand cap succeeds without burning cards"), GameXXKCardRules::DrawCards(Deck, 2, false));
	TestEqual(TEXT("normal draw never exceeds the five-card hand cap"), Deck.Hand.Num(), 5);
	TestEqual(TEXT("normal capped draw leaves state unchanged"), DeckSnapshot(Deck), FullHandSnapshot);
	const FString NegativeDrawSnapshot = DeckSnapshot(Deck);
	TestFalse(TEXT("negative draw count is rejected"), GameXXKCardRules::DrawCards(Deck, -1, false));
	TestEqual(TEXT("negative draw preserves every persisted deck field"), DeckSnapshot(Deck), NegativeDrawSnapshot);
	const FString InvalidBeginInsightSnapshot = DeckSnapshot(Deck);
	TestFalse(TEXT("insight is rejected without a free hand slot"), GameXXKCardRules::BeginInsight(Deck, 3));
	TestEqual(TEXT("invalid insight begin preserves every persisted deck field"), DeckSnapshot(Deck), InvalidBeginInsightSnapshot);
	const FString InvalidCancelInsightSnapshot = DeckSnapshot(Deck);
	TestFalse(TEXT("cancel insight is rejected without an active insight choice"), GameXXKCardRules::CancelInsight(Deck));
	TestEqual(TEXT("invalid insight cancel preserves every persisted deck field"), DeckSnapshot(Deck), InvalidCancelInsightSnapshot);

	const FName MissingInstanceId(TEXT("Instance.DoesNotExist"));
	const FString MissingMoveSnapshot = DeckSnapshot(Deck);
	TestFalse(TEXT("moving an unknown hand instance fails"), GameXXKCardRules::MoveHandCardToDiscard(Deck, MissingInstanceId));
	TestEqual(TEXT("unknown hand instance does not mutate any zone"), DeckSnapshot(Deck), MissingMoveSnapshot);

	FGameXXKBattleDeckState DrawDiscardDeck;
	TestTrue(TEXT("temporary-overdraw deck initializes"), GameXXKCardRules::InitializeBattleDeck(DrawDiscardDeck, Instances, 1209));
	const FName PlayedInstanceId = DrawDiscardDeck.Hand.Last().InstanceId;
	TestTrue(TEXT("a played card can move from hand to discard"), GameXXKCardRules::MoveHandCardToDiscard(DrawDiscardDeck, PlayedInstanceId));
	TestEqual(TEXT("played card frees a hand slot"), DrawDiscardDeck.Hand.Num(), 4);
	TestTrue(TEXT("draw two may temporarily make a six-card hand"), GameXXKCardRules::DrawCards(DrawDiscardDeck, 2, true));
	TestEqual(TEXT("temporary overdraw reaches six cards"), DrawDiscardDeck.Hand.Num(), 6);
	TestEqual(TEXT("temporary overdraw opens forced discard"), DrawDiscardDeck.PendingChoice.Kind, EGameXXKCardPendingChoiceKind::ForcedDiscard);
	TestEqual(TEXT("temporary overdraw requires exactly one discard"), DrawDiscardDeck.PendingChoice.RequiredCount, 1);
	TestEqual(TEXT("forced discard exposes every current hand card as a candidate"), DrawDiscardDeck.PendingChoice.Candidates.Num(), 6);
	FGameXXKBattleDeckState OverflowedForcedDiscardDeck = DrawDiscardDeck;
	OverflowedForcedDiscardDeck.PendingChoice.RequiredCount = MAX_int32;
	const FString OverflowedForcedDiscardSnapshot = DeckSnapshot(OverflowedForcedDiscardDeck);
	TestFalse(TEXT("serialized forced-discard count cannot overflow hand-limit validation"), GameXXKCardRules::SubmitForcedDiscard(OverflowedForcedDiscardDeck, { OverflowedForcedDiscardDeck.Hand.Last().InstanceId }));
	TestEqual(TEXT("overflowed forced-discard rejection preserves every persisted deck field"), DeckSnapshot(OverflowedForcedDiscardDeck), OverflowedForcedDiscardSnapshot);
	const FString PendingDrawSnapshot = DeckSnapshot(DrawDiscardDeck);
	TestFalse(TEXT("pending forced discard blocks an additional draw"), GameXXKCardRules::DrawCards(DrawDiscardDeck, 1, false));
	TestEqual(TEXT("pending-blocked draw preserves every persisted deck field"), DeckSnapshot(DrawDiscardDeck), PendingDrawSnapshot);

	const FString WrongDiscardSnapshot = DeckSnapshot(DrawDiscardDeck);
	TestFalse(TEXT("forced discard rejects an instance outside the hand"), GameXXKCardRules::SubmitForcedDiscard(DrawDiscardDeck, { MissingInstanceId }));
	TestEqual(TEXT("wrong forced discard does not mutate deck state"), DeckSnapshot(DrawDiscardDeck), WrongDiscardSnapshot);
	const FName DiscardChoice = DrawDiscardDeck.Hand.Last().InstanceId;
	TestTrue(TEXT("valid forced discard resolves"), GameXXKCardRules::SubmitForcedDiscard(DrawDiscardDeck, { DiscardChoice }));
	TestEqual(TEXT("forced discard returns the hand to five"), DrawDiscardDeck.Hand.Num(), 5);
	TestEqual(TEXT("forced discard clears the pending choice"), DrawDiscardDeck.PendingChoice.Kind, EGameXXKCardPendingChoiceKind::None);
	TestTrue(TEXT("forced discard leaves every instance in exactly one zone"), GameXXKCardRules::ValidateDeckState(DrawDiscardDeck));

	FGameXXKBattleDeckState ReshuffleDeck;
	TestTrue(TEXT("reshuffle deck initializes"), GameXXKCardRules::InitializeBattleDeck(ReshuffleDeck, Instances, 1211));
	ReshuffleDeck.DiscardPile.Append(ReshuffleDeck.DrawPile);
	ReshuffleDeck.DrawPile.Reset();
	TestTrue(TEXT("test setup preserves zone invariant before the just-played move"), GameXXKCardRules::ValidateDeckState(ReshuffleDeck));
	const FName JustMovedInstanceId = ReshuffleDeck.Hand.Last().InstanceId;
	TestTrue(TEXT("just-played card moves to discard before the empty draw"), GameXXKCardRules::MoveHandCardToDiscard(ReshuffleDeck, JustMovedInstanceId));
	TestTrue(TEXT("draw from an empty pile deterministically reshuffles discard"), GameXXKCardRules::DrawCards(ReshuffleDeck, 1, false));
	TestEqual(TEXT("reshuffle consumes the entire discard pile"), ReshuffleDeck.DiscardPile.Num(), 0);
	TestTrue(TEXT("reshuffle includes the just-moved instance"), IsInAnyZone(ReshuffleDeck, JustMovedInstanceId));
	TestFalse(TEXT("just-moved instance is not left outside the reshuffled draw/hand zones"), ReshuffleDeck.DiscardPile.ContainsByPredicate([JustMovedInstanceId](const FGameXXKCardInstance& Instance) { return Instance.InstanceId == JustMovedInstanceId; }));
	TestTrue(TEXT("reshuffled zones retain one instance per id"), GameXXKCardRules::ValidateDeckState(ReshuffleDeck));

	FGameXXKBattleDeckState SameReshuffleDeckA;
	FGameXXKBattleDeckState SameReshuffleDeckB;
	TestTrue(TEXT("first deterministic reshuffle deck initializes"), GameXXKCardRules::InitializeBattleDeck(SameReshuffleDeckA, Instances, 1213));
	TestTrue(TEXT("second deterministic reshuffle deck initializes"), GameXXKCardRules::InitializeBattleDeck(SameReshuffleDeckB, Instances, 1213));
	SameReshuffleDeckA.DiscardPile.Append(SameReshuffleDeckA.DrawPile);
	SameReshuffleDeckA.DrawPile.Reset();
	SameReshuffleDeckB.DiscardPile.Append(SameReshuffleDeckB.DrawPile);
	SameReshuffleDeckB.DrawPile.Reset();
	TestTrue(TEXT("first deterministic reshuffle setup moves its played card"), GameXXKCardRules::MoveHandCardToDiscard(SameReshuffleDeckA, SameReshuffleDeckA.Hand.Last().InstanceId));
	TestTrue(TEXT("second deterministic reshuffle setup moves its played card"), GameXXKCardRules::MoveHandCardToDiscard(SameReshuffleDeckB, SameReshuffleDeckB.Hand.Last().InstanceId));
	TestEqual(TEXT("identical pre-reshuffle states are byte-for-byte equivalent"), DeckSnapshot(SameReshuffleDeckA), DeckSnapshot(SameReshuffleDeckB));
	TestTrue(TEXT("first deterministic reshuffle draws"), GameXXKCardRules::DrawCards(SameReshuffleDeckA, 1, false));
	TestTrue(TEXT("second deterministic reshuffle draws"), GameXXKCardRules::DrawCards(SameReshuffleDeckB, 1, false));
	TestEqual(TEXT("identical reshuffles produce the same complete state"), DeckSnapshot(SameReshuffleDeckA), DeckSnapshot(SameReshuffleDeckB));
	TestEqual(TEXT("identical reshuffles persist the same random state"), SameReshuffleDeckA.CurrentRandomState, SameReshuffleDeckB.CurrentRandomState);

	FGameXXKBattleDeckState InsightDeck;
	TestTrue(TEXT("insight deck initializes"), GameXXKCardRules::InitializeBattleDeck(InsightDeck, Instances, 1208));
	const FName InsightCardAlreadyPlayed = InsightDeck.Hand.Last().InstanceId;
	TestTrue(TEXT("insight card has already left hand before the choice opens"), GameXXKCardRules::MoveHandCardToDiscard(InsightDeck, InsightCardAlreadyPlayed));
	const TArray<FName> InsightOriginalTop = TopIds(InsightDeck, 3);
	TestTrue(TEXT("insight exposes the top three cards without moving them"), GameXXKCardRules::BeginInsight(InsightDeck, 3));
	TestEqual(TEXT("insight opens the expected pending choice"), InsightDeck.PendingChoice.Kind, EGameXXKCardPendingChoiceKind::InsightChooseToHand);
	TestEqual(TEXT("insight exposes three candidate cards"), InsightDeck.PendingChoice.Candidates.Num(), 3);
	TestEqual(TEXT("insight keeps a top-to-bottom ID order"), NameIds(InsightDeck.PendingChoice.InsightTopOrder), NameIds(InsightOriginalTop));
	TestEqual(TEXT("insight requires exactly one hand pick"), InsightDeck.PendingChoice.RequiredCount, 1);
	const FName InsightPickedId = InsightDeck.PendingChoice.Candidates[1].InstanceId;
	const TArray<FName> InsightReorderedIds = { InsightDeck.PendingChoice.Candidates[2].InstanceId, InsightDeck.PendingChoice.Candidates[0].InstanceId };
	const FString InvalidInsightSubmitSnapshot = DeckSnapshot(InsightDeck);
	TestFalse(TEXT("insight rejects an instance outside its offered top cards"), GameXXKCardRules::SubmitInsightChoice(InsightDeck, MissingInstanceId, InsightReorderedIds));
	TestEqual(TEXT("invalid insight submit preserves every persisted deck field"), DeckSnapshot(InsightDeck), InvalidInsightSubmitSnapshot);
	TestTrue(TEXT("insight moves the chosen top card into hand and reorders the rest"), GameXXKCardRules::SubmitInsightChoice(InsightDeck, InsightPickedId, InsightReorderedIds));
	TestEqual(TEXT("insight fills the slot freed by its played card"), InsightDeck.Hand.Num(), 5);
	TestTrue(TEXT("insight picked card is in hand"), InsightDeck.Hand.ContainsByPredicate([InsightPickedId](const FGameXXKCardInstance& Instance) { return Instance.InstanceId == InsightPickedId; }));
	TestEqual(TEXT("first explicit insight reorder ID becomes the logical draw top"), InsightDeck.DrawPile.Last().InstanceId, InsightReorderedIds[0]);
	TestEqual(TEXT("second explicit insight reorder ID becomes next below the logical top"), InsightDeck.DrawPile[InsightDeck.DrawPile.Num() - 2].InstanceId, InsightReorderedIds[1]);
	TestTrue(TEXT("insight confirmation keeps zone invariant"), GameXXKCardRules::ValidateDeckState(InsightDeck));

	FGameXXKBattleDeckState CancelInsightDeck;
	TestTrue(TEXT("cancel insight deck initializes"), GameXXKCardRules::InitializeBattleDeck(CancelInsightDeck, Instances, 1212));
	const FName CancelledInsightCardId = CancelInsightDeck.Hand.Last().InstanceId;
	TestTrue(TEXT("cancel insight setup moves the played card to discard"), GameXXKCardRules::MoveHandCardToDiscard(CancelInsightDeck, CancelledInsightCardId));
	const TArray<FName> TopBeforeCancel = TopIds(CancelInsightDeck, 3);
	TestTrue(TEXT("cancel insight setup opens a pending choice"), GameXXKCardRules::BeginInsight(CancelInsightDeck, 3));
	TestTrue(TEXT("canceling insight succeeds"), GameXXKCardRules::CancelInsight(CancelInsightDeck));
	TestEqual(TEXT("canceling insight preserves the exact logical top order"), NameIds(TopIds(CancelInsightDeck, 3)), NameIds(TopBeforeCancel));
	TestTrue(TEXT("canceling insight does not revive the already-played insight card"), CancelInsightDeck.DiscardPile.ContainsByPredicate([CancelledInsightCardId](const FGameXXKCardInstance& Instance) { return Instance.InstanceId == CancelledInsightCardId; }));
	TestEqual(TEXT("canceling insight clears only the pending choice"), CancelInsightDeck.PendingChoice.Kind, EGameXXKCardPendingChoiceKind::None);
	TestTrue(TEXT("cancel insight retains the zone invariant"), GameXXKCardRules::ValidateDeckState(CancelInsightDeck));

	FGameXXKBattleDeckState TamperedCandidateDeck;
	TestTrue(TEXT("tampered candidate deck initializes"), GameXXKCardRules::InitializeBattleDeck(TamperedCandidateDeck, Instances, 1214));
	TestTrue(TEXT("tampered candidate setup moves the played insight card"), GameXXKCardRules::MoveHandCardToDiscard(TamperedCandidateDeck, TamperedCandidateDeck.Hand.Last().InstanceId));
	TestTrue(TEXT("tampered candidate setup opens insight"), GameXXKCardRules::BeginInsight(TamperedCandidateDeck, 3));
	const FName TamperedPickedId = TamperedCandidateDeck.PendingChoice.Candidates[1].InstanceId;
	const TArray<FName> TamperedReorderedIds = { TamperedCandidateDeck.PendingChoice.Candidates[2].InstanceId, TamperedCandidateDeck.PendingChoice.Candidates[0].InstanceId };
	TamperedCandidateDeck.PendingChoice.Candidates[1].CardId = TEXT("Card.CorruptedCandidate");
	const FString TamperedCandidateSnapshot = DeckSnapshot(TamperedCandidateDeck);
	TestFalse(TEXT("insight candidate view must exactly match the canonical draw instance"), GameXXKCardRules::ValidateDeckState(TamperedCandidateDeck));
	TestFalse(TEXT("insight submission rejects a tampered candidate view"), GameXXKCardRules::SubmitInsightChoice(TamperedCandidateDeck, TamperedPickedId, TamperedReorderedIds));
	TestEqual(TEXT("tampered insight submit preserves every persisted deck field"), DeckSnapshot(TamperedCandidateDeck), TamperedCandidateSnapshot);

	return true;
}

#endif
