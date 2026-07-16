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

	FGameXXKCardTargetUnit MakeTargetUnit(
		const TCHAR* UnitId,
		const EGameXXKCardTargetSide Side,
		const bool bLiving,
		const int32 HP,
		const int32 MaxHP,
		const int32 StableSortOrder)
	{
		FGameXXKCardTargetUnit Unit;
		Unit.UnitId = FName(UnitId);
		Unit.Side = Side;
		Unit.bLiving = bLiving;
		Unit.HP = HP;
		Unit.MaxHP = MaxHP;
		Unit.StableSortOrder = StableSortOrder;
		return Unit;
	}

	void AddTargetStatus(FGameXXKCardTargetUnit& Unit, const EGameXXKCardStatus Status, const int32 Stacks)
	{
		FGameXXKCardStatusStack& Stack = Unit.Statuses.AddDefaulted_GetRef();
		Stack.Status = Status;
		Stack.Stacks = Stacks;
	}

	TArray<FGameXXKCardTargetUnit> MakeTargetUnits()
	{
		TArray<FGameXXKCardTargetUnit> Units;
		Units.Add(MakeTargetUnit(TEXT("Player"), EGameXXKCardTargetSide::Party, true, 90, 100, 1));
		AddTargetStatus(Units.Last(), EGameXXKCardStatus::Momentum, 2);
		Units.Add(MakeTargetUnit(TEXT("AllyHigh"), EGameXXKCardTargetSide::Party, true, 50, 100, 2));
		Units.Add(MakeTargetUnit(TEXT("ZetaLow"), EGameXXKCardTargetSide::Party, true, 10, 100, 3));
		Units.Add(MakeTargetUnit(TEXT("AlphaLow"), EGameXXKCardTargetSide::Party, true, 10, 100, 4));
		Units.Add(MakeTargetUnit(TEXT("AllyDefeated"), EGameXXKCardTargetSide::Party, false, 0, 100, 5));
		Units.Add(MakeTargetUnit(TEXT("EnemyMarked"), EGameXXKCardTargetSide::Enemy, true, 50, 100, 10));
		AddTargetStatus(Units.Last(), EGameXXKCardStatus::Burn, 2);
		Units.Add(MakeTargetUnit(TEXT("EnemyOther"), EGameXXKCardTargetSide::Enemy, true, 35, 100, 11));
		Units.Add(MakeTargetUnit(TEXT("EnemyDefeated"), EGameXXKCardTargetSide::Enemy, false, 0, 100, 12));
		return Units;
	}

	FGameXXKCardInstance MakeTargetCard()
	{
		FGameXXKCardInstance Card;
		Card.InstanceId = TEXT("Instance.Target");
		Card.CardId = TEXT("Card.Target");
		Card.OwnerUnitId = TEXT("Player");
		Card.SourceEntryId = TEXT("Entry.Target");
		Card.AcquisitionOrdinal = 77;
		return Card;
	}

	FGameXXKCardDefinition MakeTargetDefinition(const EGameXXKCardTargetMode Mode)
	{
		FGameXXKCardDefinition Definition;
		Definition.Id = TEXT("Card.Target");
		Definition.OwnerId = TEXT("Hero");
		Definition.TargetSpec.Mode = Mode;
		Definition.TargetSpec.RequiredUnitState = Mode == EGameXXKCardTargetMode::None
			? EGameXXKCardUnitState::Any
			: EGameXXKCardUnitState::Living;
		Definition.TargetSpec.bRequireDifferentFromOwner = Mode == EGameXXKCardTargetMode::OtherAlly
			|| Mode == EGameXXKCardTargetMode::AllOtherAllies
			|| Mode == EGameXXKCardTargetMode::LowestHealthOtherAlly;
		return Definition;
	}

	const FGameXXKCardTargetCandidateView* FindTargetCandidate(const FGameXXKCardTargetRequest& Request, const FName UnitId)
	{
		return Request.CandidateViews.FindByPredicate([UnitId](const FGameXXKCardTargetCandidateView& Candidate)
		{
			return Candidate.UnitId == UnitId;
		});
	}

	FString TargetUnitSnapshot(const TArray<FGameXXKCardTargetUnit>& Units)
	{
		FString Result;
		for (const FGameXXKCardTargetUnit& Unit : Units)
		{
			Result += FString::Printf(TEXT("{%s|%d|%d|%d|%d|%d"), *Unit.UnitId.ToString(), static_cast<int32>(Unit.Side), static_cast<int32>(Unit.bLiving), Unit.HP, Unit.MaxHP, Unit.StableSortOrder);
			for (const FGameXXKCardStatusStack& Status : Unit.Statuses)
			{
				Result += FString::Printf(TEXT("|%d:%d"), static_cast<int32>(Status.Status), Status.Stacks);
			}
			Result += TEXT("}");
		}
		return Result;
	}

	FString TargetRequestSnapshot(const FGameXXKCardTargetRequest& Request)
	{
		FString Result = FString::Printf(
			TEXT("{%s|%s|%d|%d|%d|%d|%s"),
			*Request.CardInstanceId.ToString(),
			*Request.SourceUnitId.ToString(),
			static_cast<int32>(Request.EffectiveMode),
			static_cast<int32>(Request.Presentation),
			static_cast<int32>(Request.bRequiresManualSelection),
			static_cast<int32>(Request.bRequiresRandomResolution),
			*Request.FailureReason);
		for (const FGameXXKCardTargetCandidateView& Candidate : Request.CandidateViews)
		{
			Result += FString::Printf(TEXT("|%s:%d:%d:%d:%d"), *Candidate.UnitId.ToString(), static_cast<int32>(Candidate.Side), static_cast<int32>(Candidate.bCanSelect), static_cast<int32>(Candidate.DisabledReason), static_cast<int32>(Candidate.bAutoLocked));
		}
		Result += TEXT("|auto=");
		for (const FName UnitId : Request.AutomaticTargetUnitIds)
		{
			Result += UnitId.ToString() + TEXT(",");
		}
		return Result + TEXT("}");
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

	// Target request rules are intentionally pure: this is the complete input view a future UI may highlight.
	const TArray<FGameXXKCardTargetUnit> TargetUnits = MakeTargetUnits();
	const FGameXXKCardInstance TargetCard = MakeTargetCard();
	const FString TargetUnitsBeforeBuild = TargetUnitSnapshot(TargetUnits);
	const auto BuildModeRequest = [&TargetCard, &TargetUnits](const EGameXXKCardTargetMode Mode, FGameXXKCardTargetRequest& OutRequest)
	{
		return GameXXKCardRules::BuildTargetRequest(MakeTargetDefinition(Mode), TargetCard, EGameXXKCardTerrain::Plain, TargetUnits, OutRequest, nullptr);
	};

	FGameXXKCardTargetRequest SingleEnemyRequest;
	TestTrue(TEXT("single enemy builds a target request"), BuildModeRequest(EGameXXKCardTargetMode::SingleEnemy, SingleEnemyRequest));
	TestEqual(TEXT("target request does not mutate its unit input"), TargetUnitSnapshot(TargetUnits), TargetUnitsBeforeBuild);
	TestTrue(TEXT("single enemy is a manual arrow mode"), SingleEnemyRequest.bRequiresManualSelection);
	TestFalse(TEXT("single enemy is not a random resolution mode"), SingleEnemyRequest.bRequiresRandomResolution);
	TestEqual(TEXT("single enemy uses player-select presentation"), SingleEnemyRequest.Presentation, EGameXXKCardTargetPresentation::PlayerSelectsUnit);
	TestTrue(TEXT("living enemy is manually legal"), GameXXKCardRules::IsManualTargetLegal(SingleEnemyRequest, TEXT("EnemyMarked")));
	TestFalse(TEXT("party target is not legal for single enemy"), GameXXKCardRules::IsManualTargetLegal(SingleEnemyRequest, TEXT("Player")));
	TestFalse(TEXT("empty target is not legal for single enemy"), GameXXKCardRules::IsManualTargetLegal(SingleEnemyRequest, NAME_None));
	const FGameXXKCardTargetCandidateView* PartyForEnemy = FindTargetCandidate(SingleEnemyRequest, TEXT("Player"));
	TestNotNull(TEXT("single enemy keeps party candidate as a disabled view"), PartyForEnemy);
	if (PartyForEnemy)
	{
		TestFalse(TEXT("party candidate cannot be selected as an enemy"), PartyForEnemy->bCanSelect);
		TestEqual(TEXT("party candidate exposes wrong-side reason"), PartyForEnemy->DisabledReason, EGameXXKCardTargetDisabledReason::WrongSide);
	}
	const FGameXXKCardTargetCandidateView* DeadEnemyForEnemy = FindTargetCandidate(SingleEnemyRequest, TEXT("EnemyDefeated"));
	TestNotNull(TEXT("single enemy keeps defeated enemy as a disabled view"), DeadEnemyForEnemy);
	if (DeadEnemyForEnemy)
	{
		TestEqual(TEXT("defeated candidate exposes living-state reason"), DeadEnemyForEnemy->DisabledReason, EGameXXKCardTargetDisabledReason::NotLiving);
	}

	FGameXXKCardTargetRequest SingleAllyRequest;
	TestTrue(TEXT("single ally builds a target request"), BuildModeRequest(EGameXXKCardTargetMode::SingleAlly, SingleAllyRequest));
	TestTrue(TEXT("single ally allows its owner when the spec allows it"), GameXXKCardRules::IsManualTargetLegal(SingleAllyRequest, TEXT("Player")));
	TestTrue(TEXT("single ally allows a living party ally"), GameXXKCardRules::IsManualTargetLegal(SingleAllyRequest, TEXT("AllyHigh")));

	FGameXXKCardTargetRequest OtherAllyRequest;
	TestTrue(TEXT("other ally builds a target request"), BuildModeRequest(EGameXXKCardTargetMode::OtherAlly, OtherAllyRequest));
	TestTrue(TEXT("other ally remains a manual arrow mode"), OtherAllyRequest.bRequiresManualSelection);
	TestFalse(TEXT("other ally excludes its owner"), GameXXKCardRules::IsManualTargetLegal(OtherAllyRequest, TEXT("Player")));
	TestTrue(TEXT("other ally allows a different living party member"), GameXXKCardRules::IsManualTargetLegal(OtherAllyRequest, TEXT("AllyHigh")));
	const FGameXXKCardTargetCandidateView* OwnerForOtherAlly = FindTargetCandidate(OtherAllyRequest, TEXT("Player"));
	TestNotNull(TEXT("other ally retains owner as a disabled view"), OwnerForOtherAlly);
	if (OwnerForOtherAlly)
	{
		TestEqual(TEXT("other ally exposes owner-excluded reason"), OwnerForOtherAlly->DisabledReason, EGameXXKCardTargetDisabledReason::OwnerExcluded);
	}

	FGameXXKCardTargetRequest AnyLivingRequest;
	TestTrue(TEXT("any living unit builds a target request"), BuildModeRequest(EGameXXKCardTargetMode::AnyLivingUnit, AnyLivingRequest));
	TestTrue(TEXT("any living unit remains a manual arrow mode"), AnyLivingRequest.bRequiresManualSelection);
	TestTrue(TEXT("any living unit accepts a party candidate"), GameXXKCardRules::IsManualTargetLegal(AnyLivingRequest, TEXT("AllyHigh")));
	TestTrue(TEXT("any living unit accepts an enemy candidate"), GameXXKCardRules::IsManualTargetLegal(AnyLivingRequest, TEXT("EnemyOther")));
	TestFalse(TEXT("any living unit rejects a defeated candidate"), GameXXKCardRules::IsManualTargetLegal(AnyLivingRequest, TEXT("AllyDefeated")));

	FGameXXKCardTargetRequest NoneRequest;
	TestTrue(TEXT("no-target mode builds a request"), BuildModeRequest(EGameXXKCardTargetMode::None, NoneRequest));
	TestFalse(TEXT("no-target mode never requests a manual arrow"), NoneRequest.bRequiresManualSelection);
	TestFalse(TEXT("no-target mode never requests random resolution"), NoneRequest.bRequiresRandomResolution);
	TestEqual(TEXT("no-target mode has no automatic unit IDs"), NoneRequest.AutomaticTargetUnitIds.Num(), 0);

	FGameXXKCardTargetRequest SelfRequest;
	TestTrue(TEXT("self mode builds a request"), BuildModeRequest(EGameXXKCardTargetMode::Self, SelfRequest));
	TestFalse(TEXT("self mode never requests a manual arrow"), SelfRequest.bRequiresManualSelection);
	TestEqual(TEXT("self mode automatically locks the stable source unit"), SelfRequest.AutomaticTargetUnitIds, TArray<FName>{TEXT("Player")});
	const FGameXXKCardTargetCandidateView* SelfCandidate = FindTargetCandidate(SelfRequest, TEXT("Player"));
	TestNotNull(TEXT("self mode includes its source candidate"), SelfCandidate);
	if (SelfCandidate)
	{
		TestTrue(TEXT("self source is automatically locked"), SelfCandidate->bAutoLocked);
	}

	FGameXXKCardTargetRequest AllEnemiesRequest;
	TestTrue(TEXT("all enemies builds a target request"), BuildModeRequest(EGameXXKCardTargetMode::AllEnemies, AllEnemiesRequest));
	TestFalse(TEXT("all enemies never requests a manual arrow"), AllEnemiesRequest.bRequiresManualSelection);
	TestEqual(TEXT("all enemies locks living enemies in stable order"), AllEnemiesRequest.AutomaticTargetUnitIds, TArray<FName>{TEXT("EnemyMarked"), TEXT("EnemyOther")});

	FGameXXKCardTargetRequest AllAlliesRequest;
	TestTrue(TEXT("all allies builds a target request"), BuildModeRequest(EGameXXKCardTargetMode::AllAllies, AllAlliesRequest));
	TestFalse(TEXT("all allies never requests a manual arrow"), AllAlliesRequest.bRequiresManualSelection);
	TestEqual(TEXT("all allies locks living party units in stable order"), AllAlliesRequest.AutomaticTargetUnitIds, TArray<FName>{TEXT("Player"), TEXT("AllyHigh"), TEXT("ZetaLow"), TEXT("AlphaLow")});

	FGameXXKCardTargetRequest AllOtherAlliesRequest;
	TestTrue(TEXT("all other allies builds a target request"), BuildModeRequest(EGameXXKCardTargetMode::AllOtherAllies, AllOtherAlliesRequest));
	TestFalse(TEXT("all other allies never requests a manual arrow"), AllOtherAlliesRequest.bRequiresManualSelection);
	TestEqual(TEXT("all other allies excludes the source and keeps stable order"), AllOtherAlliesRequest.AutomaticTargetUnitIds, TArray<FName>{TEXT("AllyHigh"), TEXT("ZetaLow"), TEXT("AlphaLow")});

	FGameXXKCardTargetRequest RandomEnemyRequest;
	TestTrue(TEXT("random enemy builds a target request"), BuildModeRequest(EGameXXKCardTargetMode::RandomEnemy, RandomEnemyRequest));
	TestFalse(TEXT("random enemy never requests a manual arrow"), RandomEnemyRequest.bRequiresManualSelection);
	TestTrue(TEXT("random enemy explicitly requests later random resolution"), RandomEnemyRequest.bRequiresRandomResolution);
	TestEqual(TEXT("random enemy does not pre-lock a target during request build"), RandomEnemyRequest.AutomaticTargetUnitIds.Num(), 0);
	const FString RandomRequestSnapshot = TargetRequestSnapshot(RandomEnemyRequest);
	int32 RandomStateA = 90210;
	int32 RandomStateB = 90210;
	TArray<FName> RandomTargetsA = { TEXT("DoNotKeep") };
	TArray<FName> RandomTargetsB = { TEXT("DoNotKeep") };
	TestTrue(TEXT("first random enemy resolution succeeds"), GameXXKCardRules::ResolveAutomaticTargetIds(RandomEnemyRequest, TargetUnits, RandomStateA, RandomTargetsA, nullptr));
	TestTrue(TEXT("second random enemy resolution succeeds"), GameXXKCardRules::ResolveAutomaticTargetIds(RandomEnemyRequest, TargetUnits, RandomStateB, RandomTargetsB, nullptr));
	TestEqual(TEXT("same random state resolves the same random enemy"), RandomTargetsA, RandomTargetsB);
	TestEqual(TEXT("same random state advances identically once"), RandomStateA, RandomStateB);
	TestEqual(TEXT("preview request does not consume or mutate random data"), TargetRequestSnapshot(RandomEnemyRequest), RandomRequestSnapshot);

	FGameXXKCardTargetRequest LowestAllyRequest;
	TestTrue(TEXT("lowest-health ally builds a target request"), BuildModeRequest(EGameXXKCardTargetMode::LowestHealthAlly, LowestAllyRequest));
	TestFalse(TEXT("lowest-health ally never requests a manual arrow"), LowestAllyRequest.bRequiresManualSelection);
	TestEqual(TEXT("lowest-health ally resolves percentage ties by stable sort order before name"), LowestAllyRequest.AutomaticTargetUnitIds, TArray<FName>{TEXT("ZetaLow")});

	FGameXXKCardTargetRequest LowestOtherAllyRequest;
	TestTrue(TEXT("lowest-health other ally builds a target request"), BuildModeRequest(EGameXXKCardTargetMode::LowestHealthOtherAlly, LowestOtherAllyRequest));
	TestEqual(TEXT("lowest-health other ally excludes source"), LowestOtherAllyRequest.AutomaticTargetUnitIds, TArray<FName>{TEXT("ZetaLow")});
	TArray<FGameXXKCardTargetUnit> SameSlotTieUnits = TargetUnits;
	SameSlotTieUnits.FindByPredicate([](const FGameXXKCardTargetUnit& Unit) { return Unit.UnitId == TEXT("ZetaLow"); })->StableSortOrder = 4;
	FGameXXKCardTargetRequest SameSlotTieRequest;
	TestTrue(TEXT("same-slot lowest-health tie builds a request"), GameXXKCardRules::BuildTargetRequest(MakeTargetDefinition(EGameXXKCardTargetMode::LowestHealthAlly), TargetCard, EGameXXKCardTerrain::Plain, SameSlotTieUnits, SameSlotTieRequest, nullptr));
	TestEqual(TEXT("same stable slot falls back to lexical stable UnitId"), SameSlotTieRequest.AutomaticTargetUnitIds, TArray<FName>{TEXT("AlphaLow")});

	FGameXXKCardDefinition TerrainOverrideDefinition = MakeTargetDefinition(EGameXXKCardTargetMode::SingleAlly);
	FGameXXKCardTargetModeOverride& TerrainOverride = TerrainOverrideDefinition.TargetSpec.ModeOverrides.AddDefaulted_GetRef();
	TerrainOverride.ConditionType = EGameXXKCardTargetModeOverrideConditionType::TerrainIsAny;
	TerrainOverride.Terrain = EGameXXKCardTerrain::Forest;
	TerrainOverride.Mode = EGameXXKCardTargetMode::AllAllies;
	TerrainOverride.Presentation = EGameXXKCardTargetPresentation::Group;
	FGameXXKCardTargetRequest PlainTerrainRequest;
	FGameXXKCardTargetRequest ForestTerrainRequest;
	TestTrue(TEXT("plain terrain leaves the base single-ally mode active"), GameXXKCardRules::BuildTargetRequest(TerrainOverrideDefinition, TargetCard, EGameXXKCardTerrain::Plain, TargetUnits, PlainTerrainRequest, nullptr));
	TestEqual(TEXT("plain terrain remains single ally"), PlainTerrainRequest.EffectiveMode, EGameXXKCardTargetMode::SingleAlly);
	TestTrue(TEXT("plain terrain still uses manual selection"), PlainTerrainRequest.bRequiresManualSelection);
	TestTrue(TEXT("forest terrain applies the catalog terrain target override"), GameXXKCardRules::BuildTargetRequest(TerrainOverrideDefinition, TargetCard, EGameXXKCardTerrain::Forest, TargetUnits, ForestTerrainRequest, nullptr));
	TestEqual(TEXT("forest terrain switches to all allies"), ForestTerrainRequest.EffectiveMode, EGameXXKCardTargetMode::AllAllies);
	TestFalse(TEXT("forest all-allies override removes the manual arrow"), ForestTerrainRequest.bRequiresManualSelection);
	TestEqual(TEXT("forest all-allies override locks all living allies"), ForestTerrainRequest.AutomaticTargetUnitIds, TArray<FName>{TEXT("Player"), TEXT("AllyHigh"), TEXT("ZetaLow"), TEXT("AlphaLow")});

	FGameXXKCardDefinition OwnerStatusOverrideDefinition = MakeTargetDefinition(EGameXXKCardTargetMode::SingleEnemy);
	FGameXXKCardTargetModeOverride& OwnerStatusOverride = OwnerStatusOverrideDefinition.TargetSpec.ModeOverrides.AddDefaulted_GetRef();
	OwnerStatusOverride.ConditionType = EGameXXKCardTargetModeOverrideConditionType::OwnerHasStatus;
	OwnerStatusOverride.Status = EGameXXKCardStatus::Momentum;
	OwnerStatusOverride.MinimumStatusStacks = 2;
	OwnerStatusOverride.Mode = EGameXXKCardTargetMode::AllEnemies;
	OwnerStatusOverride.Presentation = EGameXXKCardTargetPresentation::Group;
	FGameXXKCardTargetRequest OwnerStatusRequest;
	TestTrue(TEXT("owner-status override builds from the source status view"), GameXXKCardRules::BuildTargetRequest(OwnerStatusOverrideDefinition, TargetCard, EGameXXKCardTerrain::Plain, TargetUnits, OwnerStatusRequest, nullptr));
	TestEqual(TEXT("owner momentum override switches single enemy to all enemies"), OwnerStatusRequest.EffectiveMode, EGameXXKCardTargetMode::AllEnemies);

	FGameXXKCardDefinition HardFilterDefinition = MakeTargetDefinition(EGameXXKCardTargetMode::SingleEnemy);
	HardFilterDefinition.TargetSpec.RequiredStatus = EGameXXKCardStatus::Burn;
	HardFilterDefinition.TargetSpec.RequiredStatusMinimumStacks = 2;
	HardFilterDefinition.TargetSpec.ForbiddenStatus = EGameXXKCardStatus::Guard;
	HardFilterDefinition.TargetSpec.MinimumHealthPercent = 20.0f;
	HardFilterDefinition.TargetSpec.MaximumHealthPercent = 60.0f;
	HardFilterDefinition.TargetSpec.RequiredTerrain = EGameXXKCardTerrain::Forest;
	FGameXXKCardTargetRequest HardFilterRequest;
	TestTrue(TEXT("hard target filters build on a matching terrain"), GameXXKCardRules::BuildTargetRequest(HardFilterDefinition, TargetCard, EGameXXKCardTerrain::Forest, TargetUnits, HardFilterRequest, nullptr));
	TestTrue(TEXT("status and health-filtered enemy remains manually legal"), GameXXKCardRules::IsManualTargetLegal(HardFilterRequest, TEXT("EnemyMarked")));
	const FGameXXKCardTargetCandidateView* FilteredEnemy = FindTargetCandidate(HardFilterRequest, TEXT("EnemyOther"));
	TestNotNull(TEXT("hard filter keeps nonmatching enemy visible as disabled"), FilteredEnemy);
	if (FilteredEnemy)
	{
		TestEqual(TEXT("hard filter exposes missing-status reason"), FilteredEnemy->DisabledReason, EGameXXKCardTargetDisabledReason::RequiredStatusMissing);
	}
	FGameXXKCardTargetRequest WrongTerrainRequest;
	TestFalse(TEXT("no legal target on a wrong required terrain fails the request"), GameXXKCardRules::BuildTargetRequest(HardFilterDefinition, TargetCard, EGameXXKCardTerrain::Plain, TargetUnits, WrongTerrainRequest, nullptr));
	TestFalse(TEXT("wrong terrain failure has a clear reason"), WrongTerrainRequest.FailureReason.IsEmpty());

	FGameXXKCardDefinition TargetStatusOverrideDefinition = MakeTargetDefinition(EGameXXKCardTargetMode::SingleEnemy);
	FGameXXKCardTargetModeOverride& TargetStatusOverride = TargetStatusOverrideDefinition.TargetSpec.ModeOverrides.AddDefaulted_GetRef();
	TargetStatusOverride.ConditionType = EGameXXKCardTargetModeOverrideConditionType::TargetHasStatus;
	TargetStatusOverride.Status = EGameXXKCardStatus::Burn;
	TargetStatusOverride.MinimumStatusStacks = 1;
	TargetStatusOverride.Mode = EGameXXKCardTargetMode::AllEnemies;
	TargetStatusOverride.Presentation = EGameXXKCardTargetPresentation::Group;
	FGameXXKCardTargetRequest UnchangedInvalidRequest = SelfRequest;
	const FString TargetStatusOverrideSnapshot = TargetRequestSnapshot(UnchangedInvalidRequest);
	TestFalse(TEXT("target-status override is explicitly rejected before a target exists"), GameXXKCardRules::BuildTargetRequest(TargetStatusOverrideDefinition, TargetCard, EGameXXKCardTerrain::Plain, TargetUnits, UnchangedInvalidRequest, nullptr));
	TestEqual(TEXT("rejected target-status override leaves output request unchanged"), TargetRequestSnapshot(UnchangedInvalidRequest), TargetStatusOverrideSnapshot);

	FGameXXKCardDefinition DefeatedTargetDefinition = MakeTargetDefinition(EGameXXKCardTargetMode::SingleAlly);
	DefeatedTargetDefinition.TargetSpec.RequiredUnitState = EGameXXKCardUnitState::Defeated;
	TestFalse(TEXT("first-release target rules reject defeated-unit metadata instead of faking an arrow candidate"), GameXXKCardRules::BuildTargetRequest(DefeatedTargetDefinition, TargetCard, EGameXXKCardTerrain::Plain, TargetUnits, UnchangedInvalidRequest, nullptr));
	TestEqual(TEXT("unsupported defeated-target metadata leaves output request unchanged"), TargetRequestSnapshot(UnchangedInvalidRequest), TargetStatusOverrideSnapshot);
	FGameXXKCardDefinition AnyTargetDefinition = MakeTargetDefinition(EGameXXKCardTargetMode::SingleAlly);
	AnyTargetDefinition.TargetSpec.RequiredUnitState = EGameXXKCardUnitState::Any;
	TestFalse(TEXT("first-release target rules reject any-unit-state metadata instead of silently narrowing it to living"), GameXXKCardRules::BuildTargetRequest(AnyTargetDefinition, TargetCard, EGameXXKCardTerrain::Plain, TargetUnits, UnchangedInvalidRequest, nullptr));
	TestEqual(TEXT("unsupported any-unit-state metadata leaves output request unchanged"), TargetRequestSnapshot(UnchangedInvalidRequest), TargetStatusOverrideSnapshot);
	FGameXXKCardDefinition InvalidForbiddenStatusDefinition = MakeTargetDefinition(EGameXXKCardTargetMode::SingleEnemy);
	InvalidForbiddenStatusDefinition.TargetSpec.ForbiddenStatus = EGameXXKCardStatus::Invalid;
	TestFalse(TEXT("invalid forbidden-status metadata is rejected instead of treated as no filter"), GameXXKCardRules::BuildTargetRequest(InvalidForbiddenStatusDefinition, TargetCard, EGameXXKCardTerrain::Plain, TargetUnits, UnchangedInvalidRequest, nullptr));
	TestEqual(TEXT("invalid forbidden-status metadata leaves output request unchanged"), TargetRequestSnapshot(UnchangedInvalidRequest), TargetStatusOverrideSnapshot);
	FGameXXKCardDefinition InvertedHealthDefinition = MakeTargetDefinition(EGameXXKCardTargetMode::SingleEnemy);
	InvertedHealthDefinition.TargetSpec.MinimumHealthPercent = 70.0f;
	InvertedHealthDefinition.TargetSpec.MaximumHealthPercent = 30.0f;
	TestFalse(TEXT("inverted health-range metadata is rejected before candidate construction"), GameXXKCardRules::BuildTargetRequest(InvertedHealthDefinition, TargetCard, EGameXXKCardTerrain::Plain, TargetUnits, UnchangedInvalidRequest, nullptr));
	TestEqual(TEXT("inverted health-range metadata leaves output request unchanged"), TargetRequestSnapshot(UnchangedInvalidRequest), TargetStatusOverrideSnapshot);
	FGameXXKCardDefinition InactiveMalformedOverrideDefinition = MakeTargetDefinition(EGameXXKCardTargetMode::SingleEnemy);
	FGameXXKCardTargetModeOverride& InactiveMalformedOverride = InactiveMalformedOverrideDefinition.TargetSpec.ModeOverrides.AddDefaulted_GetRef();
	InactiveMalformedOverride.ConditionType = EGameXXKCardTargetModeOverrideConditionType::TerrainIsAny;
	InactiveMalformedOverride.Terrain = EGameXXKCardTerrain::Forest;
	InactiveMalformedOverride.Mode = EGameXXKCardTargetMode::AllEnemies;
	InactiveMalformedOverride.Presentation = EGameXXKCardTargetPresentation::PlayerSelectsUnit;
	TestFalse(TEXT("inactive malformed target override is rejected before a later terrain change can activate it"), GameXXKCardRules::BuildTargetRequest(InactiveMalformedOverrideDefinition, TargetCard, EGameXXKCardTerrain::Plain, TargetUnits, UnchangedInvalidRequest, nullptr));
	TestEqual(TEXT("inactive malformed target override leaves output request unchanged"), TargetRequestSnapshot(UnchangedInvalidRequest), TargetStatusOverrideSnapshot);
	TArray<FGameXXKCardTargetUnit> SaturatedStatusUnits = TargetUnits;
	FGameXXKCardTargetUnit* SaturatedSource = SaturatedStatusUnits.FindByPredicate([](const FGameXXKCardTargetUnit& Unit) { return Unit.UnitId == TEXT("Player"); });
	TestNotNull(TEXT("saturated-status test locates the target source"), SaturatedSource);
	if (SaturatedSource)
	{
		AddTargetStatus(*SaturatedSource, EGameXXKCardStatus::Momentum, MAX_int32);
		AddTargetStatus(*SaturatedSource, EGameXXKCardStatus::Momentum, 1);
	}
	FGameXXKCardDefinition SaturatedStatusOverrideDefinition = MakeTargetDefinition(EGameXXKCardTargetMode::SingleEnemy);
	FGameXXKCardTargetModeOverride& SaturatedStatusOverride = SaturatedStatusOverrideDefinition.TargetSpec.ModeOverrides.AddDefaulted_GetRef();
	SaturatedStatusOverride.ConditionType = EGameXXKCardTargetModeOverrideConditionType::OwnerHasStatus;
	SaturatedStatusOverride.Status = EGameXXKCardStatus::Momentum;
	SaturatedStatusOverride.MinimumStatusStacks = MAX_int32;
	SaturatedStatusOverride.Mode = EGameXXKCardTargetMode::AllEnemies;
	SaturatedStatusOverride.Presentation = EGameXXKCardTargetPresentation::Group;
	FGameXXKCardTargetRequest SaturatedStatusRequest;
	TestTrue(TEXT("status totals saturate instead of overflowing when evaluating an owner override"), GameXXKCardRules::BuildTargetRequest(SaturatedStatusOverrideDefinition, TargetCard, EGameXXKCardTerrain::Plain, SaturatedStatusUnits, SaturatedStatusRequest, nullptr));
	TestEqual(TEXT("saturated owner status still activates the intended override"), SaturatedStatusRequest.EffectiveMode, EGameXXKCardTargetMode::AllEnemies);

	FGameXXKCardInstance WrongCardId = TargetCard;
	WrongCardId.CardId = TEXT("Card.Wrong");
	FGameXXKCardTargetRequest InvalidBuildRequest = SelfRequest;
	const FString InvalidBuildSnapshot = TargetRequestSnapshot(InvalidBuildRequest);
	TestFalse(TEXT("card definition mismatch is rejected"), GameXXKCardRules::BuildTargetRequest(MakeTargetDefinition(EGameXXKCardTargetMode::Self), WrongCardId, EGameXXKCardTerrain::Plain, TargetUnits, InvalidBuildRequest, nullptr));
	TestEqual(TEXT("card definition mismatch leaves output request unchanged"), TargetRequestSnapshot(InvalidBuildRequest), InvalidBuildSnapshot);
	FGameXXKCardInstance MissingOwnerCard = TargetCard;
	MissingOwnerCard.OwnerUnitId = TEXT("AbsentOwner");
	TestFalse(TEXT("missing source owner is rejected"), GameXXKCardRules::BuildTargetRequest(MakeTargetDefinition(EGameXXKCardTargetMode::Self), MissingOwnerCard, EGameXXKCardTerrain::Plain, TargetUnits, InvalidBuildRequest, nullptr));
	TestEqual(TEXT("missing source owner leaves output request unchanged"), TargetRequestSnapshot(InvalidBuildRequest), InvalidBuildSnapshot);
	TArray<FGameXXKCardTargetUnit> DuplicateUnitIds = TargetUnits;
	const FGameXXKCardTargetUnit DuplicateTargetUnit = DuplicateUnitIds[0];
	DuplicateUnitIds.Add(DuplicateTargetUnit);
	const FString DuplicateUnitInputSnapshot = TargetUnitSnapshot(DuplicateUnitIds);
	TestFalse(TEXT("duplicate stable target UnitId is rejected"), GameXXKCardRules::BuildTargetRequest(MakeTargetDefinition(EGameXXKCardTargetMode::Self), TargetCard, EGameXXKCardTerrain::Plain, DuplicateUnitIds, InvalidBuildRequest, nullptr));
	TestEqual(TEXT("duplicate-unit build failure does not mutate input units"), TargetUnitSnapshot(DuplicateUnitIds), DuplicateUnitInputSnapshot);
	TestEqual(TEXT("duplicate-unit build failure leaves output request unchanged"), TargetRequestSnapshot(InvalidBuildRequest), InvalidBuildSnapshot);

	int32 ManualResolveRandomState = 443;
	TArray<FName> ManualResolveOutput = { TEXT("Preserve") };
	const TArray<FName> ManualResolveOutputBefore = ManualResolveOutput;
	TestFalse(TEXT("automatic resolver rejects manual request modes"), GameXXKCardRules::ResolveAutomaticTargetIds(SingleEnemyRequest, TargetUnits, ManualResolveRandomState, ManualResolveOutput, nullptr));
	TestEqual(TEXT("manual request rejected by automatic resolver preserves random state"), ManualResolveRandomState, 443);
	TestEqual(TEXT("manual request rejected by automatic resolver preserves output"), ManualResolveOutput, ManualResolveOutputBefore);
	FGameXXKCardTargetRequest WrongRandomTypeRequest = RandomEnemyRequest;
	WrongRandomTypeRequest.bRequiresRandomResolution = false;
	int32 WrongRandomTypeState = 991;
	TArray<FName> WrongRandomTypeOutput = { TEXT("Preserve") };
	TestFalse(TEXT("automatic resolver rejects malformed random request metadata"), GameXXKCardRules::ResolveAutomaticTargetIds(WrongRandomTypeRequest, TargetUnits, WrongRandomTypeState, WrongRandomTypeOutput, nullptr));
	TestEqual(TEXT("malformed random request preserves random state"), WrongRandomTypeState, 991);
	TestEqual(TEXT("malformed random request preserves output"), WrongRandomTypeOutput, TArray<FName>{TEXT("Preserve")});
	int32 DuplicateResolveRandomState = 992;
	TArray<FName> DuplicateResolveOutput = { TEXT("Preserve") };
	TestFalse(TEXT("automatic resolver rejects duplicate input target IDs"), GameXXKCardRules::ResolveAutomaticTargetIds(RandomEnemyRequest, DuplicateUnitIds, DuplicateResolveRandomState, DuplicateResolveOutput, nullptr));
	TestEqual(TEXT("duplicate automatic resolution preserves random state"), DuplicateResolveRandomState, 992);
	TestEqual(TEXT("duplicate automatic resolution preserves output"), DuplicateResolveOutput, TArray<FName>{TEXT("Preserve")});
	FGameXXKCardTargetRequest DuplicateManualCandidateRequest = SingleEnemyRequest;
	DuplicateManualCandidateRequest.CandidateViews.Add(*FindTargetCandidate(SingleEnemyRequest, TEXT("EnemyMarked")));
	TestFalse(TEXT("manual validation rejects duplicate candidate IDs"), GameXXKCardRules::IsManualTargetLegal(DuplicateManualCandidateRequest, TEXT("EnemyMarked")));

	return true;
}

#endif
