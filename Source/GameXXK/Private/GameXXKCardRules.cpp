#include "GameXXKCardRules.h"

namespace
{
	// Five is the current gameplay limit; this ceiling leaves room for future effects while
	// bounding serialized input and keeping all hand-size arithmetic safely representable.
	constexpr int32 MaxSupportedHandLimit = 64;

	bool IsActiveChoice(const EGameXXKCardPendingChoiceKind Kind)
	{
		return Kind == EGameXXKCardPendingChoiceKind::ForcedDiscard
			|| Kind == EGameXXKCardPendingChoiceKind::InsightChooseToHand;
	}

	bool IsValidInstance(const FGameXXKCardInstance& Instance)
	{
		return !Instance.InstanceId.IsNone()
			&& !Instance.CardId.IsNone()
			&& !Instance.OwnerUnitId.IsNone()
			&& !Instance.SourceEntryId.IsNone()
			&& Instance.AcquisitionOrdinal != INDEX_NONE;
	}

	/** Pending-choice candidates are serialized UI views, so compare every stable field before trusting them. */
	bool IsSameInstance(const FGameXXKCardInstance& Left, const FGameXXKCardInstance& Right)
	{
		return Left.InstanceId == Right.InstanceId
			&& Left.CardId == Right.CardId
			&& Left.OwnerUnitId == Right.OwnerUnitId
			&& Left.SourceEntryId == Right.SourceEntryId
			&& Left.AcquisitionOrdinal == Right.AcquisitionOrdinal;
	}

	bool SetFailure(FString* OutError, const FString& Error)
	{
		if (OutError)
		{
			*OutError = Error;
		}
		return false;
	}

	void ClearPendingChoice(FGameXXKPendingCardChoice& PendingChoice)
	{
		PendingChoice = FGameXXKPendingCardChoice();
		PendingChoice.Kind = EGameXXKCardPendingChoiceKind::None;
		PendingChoice.bCancelPreservesDrawTop = true;
	}

	int32 NextRandomIndex(int32& InOutState, const int32 UpperExclusive)
	{
		check(UpperExclusive > 0);
		const uint32 PreviousState = static_cast<uint32>(InOutState);
		const uint32 NextState = PreviousState * 196314165u + 907633515u;
		InOutState = static_cast<int32>(NextState);
		return static_cast<int32>(NextState % static_cast<uint32>(UpperExclusive));
	}

	void ShufflePile(TArray<FGameXXKCardInstance>& InOutPile, int32& InOutRandomState)
	{
		for (int32 Index = InOutPile.Num() - 1; Index > 0; --Index)
		{
			const int32 OtherIndex = NextRandomIndex(InOutRandomState, Index + 1);
			InOutPile.Swap(Index, OtherIndex);
		}
	}

	bool EnsureDrawPileHasCard(FGameXXKBattleDeckState& InOutDeck)
	{
		if (!InOutDeck.DrawPile.IsEmpty())
		{
			return true;
		}

		if (InOutDeck.DiscardPile.IsEmpty())
		{
			return false;
		}

		InOutDeck.DrawPile = MoveTemp(InOutDeck.DiscardPile);
		ShufflePile(InOutDeck.DrawPile, InOutDeck.CurrentRandomState);
		return true;
	}

	bool ValidateCandidateCopiesMatchHand(const FGameXXKBattleDeckState& Deck, FString& OutError)
	{
		if (Deck.PendingChoice.Candidates.Num() != Deck.Hand.Num())
		{
			OutError = TEXT("Forced discard candidates do not mirror the hand.");
			return false;
		}

		for (int32 Index = 0; Index < Deck.Hand.Num(); ++Index)
		{
			if (!IsSameInstance(Deck.PendingChoice.Candidates[Index], Deck.Hand[Index]))
			{
				OutError = TEXT("Forced discard candidates do not exactly mirror the canonical hand instances.");
				return false;
			}
		}
		return true;
	}

	bool ValidateDeckStateInternal(const FGameXXKBattleDeckState& Deck, FString& OutError)
	{
		OutError.Reset();
		if (Deck.HandLimit <= 0 || Deck.HandLimit > MaxSupportedHandLimit)
		{
			OutError = TEXT("Hand limit is outside the supported serialized range.");
			return false;
		}
		if (Deck.ActiveInstanceIds.IsEmpty())
		{
			OutError = TEXT("Deck has no initialized instance ledger.");
			return false;
		}

		TSet<FName> LedgerIds;
		for (const FName InstanceId : Deck.ActiveInstanceIds)
		{
			if (InstanceId.IsNone() || LedgerIds.Contains(InstanceId))
			{
				OutError = TEXT("Deck instance ledger contains an invalid or duplicate ID.");
				return false;
			}
			LedgerIds.Add(InstanceId);
		}

		TSet<FName> ZoneIds;
		const auto ValidateZone = [&LedgerIds, &ZoneIds, &OutError](const TArray<FGameXXKCardInstance>& Zone, const TCHAR* ZoneName)
		{
			for (const FGameXXKCardInstance& Instance : Zone)
			{
				if (!IsValidInstance(Instance) || !LedgerIds.Contains(Instance.InstanceId) || ZoneIds.Contains(Instance.InstanceId))
				{
					OutError = FString::Printf(TEXT("Deck instance is invalid, absent from the ledger, or duplicated in %s."), ZoneName);
					return false;
				}
				ZoneIds.Add(Instance.InstanceId);
			}
			return true;
		};

		if (!ValidateZone(Deck.DrawPile, TEXT("DrawPile"))
			|| !ValidateZone(Deck.Hand, TEXT("Hand"))
			|| !ValidateZone(Deck.DiscardPile, TEXT("DiscardPile")))
		{
			return false;
		}
		if (ZoneIds.Num() != LedgerIds.Num())
		{
			OutError = TEXT("A ledger instance is not present in any logical card zone.");
			return false;
		}

		switch (Deck.PendingChoice.Kind)
		{
		case EGameXXKCardPendingChoiceKind::None:
			if (Deck.Hand.Num() > Deck.HandLimit
				|| !Deck.PendingChoice.Candidates.IsEmpty()
				|| Deck.PendingChoice.RequiredCount != 0
				|| Deck.PendingChoice.RequiredDiscardCount != 0
				|| Deck.PendingChoice.RequiredHandPickCount != 0
				|| !Deck.PendingChoice.InsightTopOrder.IsEmpty()
				|| !Deck.PendingChoice.InsightPickedInstanceId.IsNone()
				|| !Deck.PendingChoice.InsightReorderedInstanceIds.IsEmpty()
				|| Deck.PendingChoice.bCanCancel
				|| !Deck.PendingChoice.bCancelPreservesDrawTop)
			{
				OutError = TEXT("No-pending-choice state contains a hand overflow or stale choice data.");
				return false;
			}
			return true;

		case EGameXXKCardPendingChoiceKind::ForcedDiscard:
			if (Deck.PendingChoice.RequiredCount <= 0
				|| Deck.PendingChoice.RequiredCount > Deck.Hand.Num()
				|| Deck.PendingChoice.RequiredDiscardCount != Deck.PendingChoice.RequiredCount
				|| Deck.PendingChoice.RequiredHandPickCount != 0
				|| Deck.Hand.Num() <= Deck.HandLimit
				|| Deck.Hand.Num() - Deck.HandLimit != Deck.PendingChoice.RequiredCount
				|| Deck.PendingChoice.bCanCancel
				|| !Deck.PendingChoice.InsightTopOrder.IsEmpty())
			{
				OutError = TEXT("Forced discard choice is malformed.");
				return false;
			}
			return ValidateCandidateCopiesMatchHand(Deck, OutError);

		case EGameXXKCardPendingChoiceKind::InsightChooseToHand:
			if (Deck.PendingChoice.RequiredCount != 1
				|| Deck.PendingChoice.RequiredHandPickCount != 1
				|| Deck.PendingChoice.RequiredDiscardCount != 0
				|| !Deck.PendingChoice.bCanCancel
				|| Deck.Hand.Num() >= Deck.HandLimit
				|| Deck.PendingChoice.Candidates.IsEmpty()
				|| Deck.PendingChoice.Candidates.Num() != Deck.PendingChoice.InsightTopOrder.Num()
				|| Deck.DrawPile.Num() < Deck.PendingChoice.Candidates.Num())
			{
				OutError = TEXT("Insight choice is malformed.");
				return false;
			}
			for (int32 Index = 0; Index < Deck.PendingChoice.Candidates.Num(); ++Index)
			{
				const FGameXXKCardInstance& Candidate = Deck.PendingChoice.Candidates[Index];
				const FGameXXKCardInstance& CanonicalTop = Deck.DrawPile[Deck.DrawPile.Num() - 1 - Index];
				if (Candidate.InstanceId.IsNone()
					|| Candidate.InstanceId != Deck.PendingChoice.InsightTopOrder[Index]
					|| !IsSameInstance(Candidate, CanonicalTop))
				{
					OutError = TEXT("Insight candidates no longer exactly match the canonical draw-pile top.");
					return false;
				}
			}
			return true;

		case EGameXXKCardPendingChoiceKind::Invalid:
		default:
			OutError = TEXT("Pending choice kind is invalid.");
			return false;
		}
	}

	bool RequireNoPendingChoice(const FGameXXKBattleDeckState& Deck, FString* OutError)
	{
		if (IsActiveChoice(Deck.PendingChoice.Kind))
		{
			return SetFailure(OutError, TEXT("A pending card choice must resolve before this operation."));
		}
		return true;
	}

	const FGameXXKCardInstance* FindInZone(const TArray<FGameXXKCardInstance>& Zone, const FName InstanceId)
	{
		return Zone.FindByPredicate([InstanceId](const FGameXXKCardInstance& Instance)
		{
			return Instance.InstanceId == InstanceId;
		});
	}
}

bool GameXXKCardRules::InitializeBattleDeck(
	FGameXXKBattleDeckState& InOutDeck,
	const TArray<FGameXXKCardInstance>& Instances,
	const int32 InitialRandomSeed,
	FString* OutError)
{
	if (OutError)
	{
		OutError->Reset();
	}
	if (Instances.IsEmpty())
	{
		return SetFailure(OutError, TEXT("Cannot initialize an empty battle deck."));
	}

	FGameXXKBattleDeckState NewDeck;
	NewDeck.InitialRandomSeed = InitialRandomSeed;
	NewDeck.CurrentRandomState = InitialRandomSeed;
	NewDeck.SharedEnergy = 3;
	NewDeck.HandLimit = 5;
	NewDeck.DrawPile = Instances;
	NewDeck.ActiveInstanceIds.Reserve(Instances.Num());

	TSet<FName> SeenInstanceIds;
	for (const FGameXXKCardInstance& Instance : Instances)
	{
		if (!IsValidInstance(Instance) || SeenInstanceIds.Contains(Instance.InstanceId))
		{
			return SetFailure(OutError, TEXT("Battle deck instances must have unique, complete stable identities."));
		}
		SeenInstanceIds.Add(Instance.InstanceId);
		NewDeck.ActiveInstanceIds.Add(Instance.InstanceId);
	}

	ClearPendingChoice(NewDeck.PendingChoice);
	ShufflePile(NewDeck.DrawPile, NewDeck.CurrentRandomState);
	while (NewDeck.Hand.Num() < NewDeck.HandLimit && !NewDeck.DrawPile.IsEmpty())
	{
		NewDeck.Hand.Add(MoveTemp(NewDeck.DrawPile.Last()));
		NewDeck.DrawPile.Pop(EAllowShrinking::No);
	}

	FString ValidationError;
	if (!ValidateDeckStateInternal(NewDeck, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	InOutDeck = MoveTemp(NewDeck);
	return true;
}

bool GameXXKCardRules::DrawCards(
	FGameXXKBattleDeckState& InOutDeck,
	const int32 Count,
	const bool bAllowTemporaryOverdraw,
	FString* OutError)
{
	if (OutError)
	{
		OutError->Reset();
	}
	if (Count < 0)
	{
		return SetFailure(OutError, TEXT("Draw count cannot be negative."));
	}

	FString ValidationError;
	if (!ValidateDeckStateInternal(InOutDeck, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	if (!RequireNoPendingChoice(InOutDeck, OutError))
	{
		return false;
	}

	FGameXXKBattleDeckState NewDeck = InOutDeck;
	const int32 MaximumHandSize = NewDeck.HandLimit + (bAllowTemporaryOverdraw ? 1 : 0);
	int32 RemainingToDraw = FMath::Min(Count, FMath::Max(0, MaximumHandSize - NewDeck.Hand.Num()));
	while (RemainingToDraw > 0 && EnsureDrawPileHasCard(NewDeck))
	{
		NewDeck.Hand.Add(MoveTemp(NewDeck.DrawPile.Last()));
		NewDeck.DrawPile.Pop(EAllowShrinking::No);
		--RemainingToDraw;
	}

	if (bAllowTemporaryOverdraw && NewDeck.Hand.Num() > NewDeck.HandLimit)
	{
		ClearPendingChoice(NewDeck.PendingChoice);
		NewDeck.PendingChoice.Kind = EGameXXKCardPendingChoiceKind::ForcedDiscard;
		NewDeck.PendingChoice.Candidates = NewDeck.Hand;
		NewDeck.PendingChoice.RequiredCount = NewDeck.Hand.Num() - NewDeck.HandLimit;
		NewDeck.PendingChoice.RequiredDiscardCount = NewDeck.PendingChoice.RequiredCount;
		NewDeck.PendingChoice.RequiredHandPickCount = 0;
		NewDeck.PendingChoice.bCanCancel = false;
		NewDeck.PendingChoice.bCancelPreservesDrawTop = true;
	}

	if (!ValidateDeckStateInternal(NewDeck, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	InOutDeck = MoveTemp(NewDeck);
	return true;
}

bool GameXXKCardRules::MoveHandCardToDiscard(
	FGameXXKBattleDeckState& InOutDeck,
	const FName InstanceId,
	FString* OutError)
{
	if (OutError)
	{
		OutError->Reset();
	}
	if (InstanceId.IsNone())
	{
		return SetFailure(OutError, TEXT("Cannot move an empty instance ID."));
	}

	FString ValidationError;
	if (!ValidateDeckStateInternal(InOutDeck, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	if (!RequireNoPendingChoice(InOutDeck, OutError))
	{
		return false;
	}

	const int32 HandIndex = InOutDeck.Hand.IndexOfByPredicate([InstanceId](const FGameXXKCardInstance& Instance)
	{
		return Instance.InstanceId == InstanceId;
	});
	if (HandIndex == INDEX_NONE)
	{
		return SetFailure(OutError, TEXT("Requested instance is not in the current hand."));
	}

	FGameXXKBattleDeckState NewDeck = InOutDeck;
	NewDeck.DiscardPile.Add(MoveTemp(NewDeck.Hand[HandIndex]));
	NewDeck.Hand.RemoveAt(HandIndex, 1, EAllowShrinking::No);
	if (!ValidateDeckStateInternal(NewDeck, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	InOutDeck = MoveTemp(NewDeck);
	return true;
}

bool GameXXKCardRules::SubmitForcedDiscard(
	FGameXXKBattleDeckState& InOutDeck,
	const TArray<FName>& DiscardedInstanceIds,
	FString* OutError)
{
	if (OutError)
	{
		OutError->Reset();
	}

	FString ValidationError;
	if (!ValidateDeckStateInternal(InOutDeck, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	if (InOutDeck.PendingChoice.Kind != EGameXXKCardPendingChoiceKind::ForcedDiscard)
	{
		return SetFailure(OutError, TEXT("There is no forced discard choice to submit."));
	}
	if (DiscardedInstanceIds.Num() != InOutDeck.PendingChoice.RequiredCount)
	{
		return SetFailure(OutError, TEXT("Forced discard selection has the wrong number of instances."));
	}

	TSet<FName> SubmittedIds;
	for (const FName InstanceId : DiscardedInstanceIds)
	{
		if (InstanceId.IsNone() || SubmittedIds.Contains(InstanceId) || !FindInZone(InOutDeck.Hand, InstanceId))
		{
			return SetFailure(OutError, TEXT("Forced discard selection contains an invalid or non-hand instance."));
		}
		SubmittedIds.Add(InstanceId);
	}

	FGameXXKBattleDeckState NewDeck = InOutDeck;
	for (const FName InstanceId : DiscardedInstanceIds)
	{
		const int32 HandIndex = NewDeck.Hand.IndexOfByPredicate([InstanceId](const FGameXXKCardInstance& Instance)
		{
			return Instance.InstanceId == InstanceId;
		});
		check(HandIndex != INDEX_NONE);
		NewDeck.DiscardPile.Add(MoveTemp(NewDeck.Hand[HandIndex]));
		NewDeck.Hand.RemoveAt(HandIndex, 1, EAllowShrinking::No);
	}
	ClearPendingChoice(NewDeck.PendingChoice);

	if (!ValidateDeckStateInternal(NewDeck, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	InOutDeck = MoveTemp(NewDeck);
	return true;
}

bool GameXXKCardRules::BeginInsight(
	FGameXXKBattleDeckState& InOutDeck,
	const int32 LookCount,
	FString* OutError)
{
	if (OutError)
	{
		OutError->Reset();
	}
	if (LookCount <= 0)
	{
		return SetFailure(OutError, TEXT("Insight look count must be positive."));
	}

	FString ValidationError;
	if (!ValidateDeckStateInternal(InOutDeck, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	if (!RequireNoPendingChoice(InOutDeck, OutError))
	{
		return false;
	}
	if (InOutDeck.Hand.Num() >= InOutDeck.HandLimit)
	{
		return SetFailure(OutError, TEXT("Insight requires an open hand slot."));
	}
	if (InOutDeck.DrawPile.IsEmpty())
	{
		return SetFailure(OutError, TEXT("Insight requires a non-empty draw pile."));
	}

	FGameXXKBattleDeckState NewDeck = InOutDeck;
	ClearPendingChoice(NewDeck.PendingChoice);
	NewDeck.PendingChoice.Kind = EGameXXKCardPendingChoiceKind::InsightChooseToHand;
	NewDeck.PendingChoice.RequiredCount = 1;
	NewDeck.PendingChoice.RequiredDiscardCount = 0;
	NewDeck.PendingChoice.RequiredHandPickCount = 1;
	NewDeck.PendingChoice.bCanCancel = true;
	NewDeck.PendingChoice.bCancelPreservesDrawTop = true;
	const int32 ActualLookCount = FMath::Min(LookCount, NewDeck.DrawPile.Num());
	NewDeck.PendingChoice.Candidates.Reserve(ActualLookCount);
	NewDeck.PendingChoice.InsightTopOrder.Reserve(ActualLookCount);
	for (int32 Index = 0; Index < ActualLookCount; ++Index)
	{
		const FGameXXKCardInstance& Candidate = NewDeck.DrawPile[NewDeck.DrawPile.Num() - 1 - Index];
		NewDeck.PendingChoice.Candidates.Add(Candidate);
		NewDeck.PendingChoice.InsightTopOrder.Add(Candidate.InstanceId);
	}

	if (!ValidateDeckStateInternal(NewDeck, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	InOutDeck = MoveTemp(NewDeck);
	return true;
}

bool GameXXKCardRules::SubmitInsightChoice(
	FGameXXKBattleDeckState& InOutDeck,
	const FName PickedInstanceId,
	const TArray<FName>& ReorderedRemainingInstanceIds,
	FString* OutError)
{
	if (OutError)
	{
		OutError->Reset();
	}

	FString ValidationError;
	if (!ValidateDeckStateInternal(InOutDeck, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	if (InOutDeck.PendingChoice.Kind != EGameXXKCardPendingChoiceKind::InsightChooseToHand)
	{
		return SetFailure(OutError, TEXT("There is no insight choice to submit."));
	}
	if (PickedInstanceId.IsNone() || ReorderedRemainingInstanceIds.Num() != InOutDeck.PendingChoice.Candidates.Num() - 1)
	{
		return SetFailure(OutError, TEXT("Insight selection is incomplete."));
	}

	TSet<FName> OfferedIds;
	TMap<FName, FGameXXKCardInstance> OfferedInstances;
	for (int32 Index = 0; Index < InOutDeck.PendingChoice.Candidates.Num(); ++Index)
	{
		const FGameXXKCardInstance& Candidate = InOutDeck.PendingChoice.Candidates[Index];
		// ValidateDeckStateInternal above already establishes that this is the same
		// instance. Re-read from DrawPile here so a serialized candidate can never
		// overwrite a canonical card field during the confirmation transaction.
		const FGameXXKCardInstance& CanonicalTop = InOutDeck.DrawPile[InOutDeck.DrawPile.Num() - 1 - Index];
		if (Candidate.InstanceId.IsNone() || OfferedIds.Contains(Candidate.InstanceId))
		{
			return SetFailure(OutError, TEXT("Insight offer contains duplicate or invalid instances."));
		}
		OfferedIds.Add(Candidate.InstanceId);
		OfferedInstances.Add(CanonicalTop.InstanceId, CanonicalTop);
	}
	if (!OfferedIds.Contains(PickedInstanceId))
	{
		return SetFailure(OutError, TEXT("Insight pick is not part of the offered top cards."));
	}

	TSet<FName> ReorderedIds;
	for (const FName InstanceId : ReorderedRemainingInstanceIds)
	{
		if (InstanceId.IsNone()
			|| InstanceId == PickedInstanceId
			|| !OfferedIds.Contains(InstanceId)
			|| ReorderedIds.Contains(InstanceId))
		{
			return SetFailure(OutError, TEXT("Insight reorder is not a complete permutation of the remaining offer."));
		}
		ReorderedIds.Add(InstanceId);
	}
	if (ReorderedIds.Num() != OfferedIds.Num() - 1)
	{
		return SetFailure(OutError, TEXT("Insight reorder is not a complete permutation of the remaining offer."));
	}

	FGameXXKBattleDeckState NewDeck = InOutDeck;
	NewDeck.PendingChoice.InsightPickedInstanceId = PickedInstanceId;
	NewDeck.PendingChoice.InsightReorderedInstanceIds = ReorderedRemainingInstanceIds;
	const int32 LowerDrawCount = NewDeck.DrawPile.Num() - NewDeck.PendingChoice.Candidates.Num();
	TArray<FGameXXKCardInstance> RebuiltDrawPile;
	RebuiltDrawPile.Reserve(NewDeck.DrawPile.Num() - 1);
	for (int32 Index = 0; Index < LowerDrawCount; ++Index)
	{
		RebuiltDrawPile.Add(MoveTemp(NewDeck.DrawPile[Index]));
	}
	for (int32 Index = ReorderedRemainingInstanceIds.Num() - 1; Index >= 0; --Index)
	{
		const FGameXXKCardInstance* ReorderedInstance = OfferedInstances.Find(ReorderedRemainingInstanceIds[Index]);
		check(ReorderedInstance);
		RebuiltDrawPile.Add(*ReorderedInstance);
	}
	const FGameXXKCardInstance* PickedInstance = OfferedInstances.Find(PickedInstanceId);
	check(PickedInstance);
	NewDeck.Hand.Add(*PickedInstance);
	NewDeck.DrawPile = MoveTemp(RebuiltDrawPile);
	ClearPendingChoice(NewDeck.PendingChoice);

	if (!ValidateDeckStateInternal(NewDeck, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	InOutDeck = MoveTemp(NewDeck);
	return true;
}

bool GameXXKCardRules::CancelInsight(FGameXXKBattleDeckState& InOutDeck, FString* OutError)
{
	if (OutError)
	{
		OutError->Reset();
	}

	FString ValidationError;
	if (!ValidateDeckStateInternal(InOutDeck, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	if (InOutDeck.PendingChoice.Kind != EGameXXKCardPendingChoiceKind::InsightChooseToHand)
	{
		return SetFailure(OutError, TEXT("There is no insight choice to cancel."));
	}

	FGameXXKBattleDeckState NewDeck = InOutDeck;
	ClearPendingChoice(NewDeck.PendingChoice);
	if (!ValidateDeckStateInternal(NewDeck, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	InOutDeck = MoveTemp(NewDeck);
	return true;
}

bool GameXXKCardRules::ValidateDeckState(const FGameXXKBattleDeckState& Deck, FString* OutError)
{
	FString ValidationError;
	const bool bIsValid = ValidateDeckStateInternal(Deck, ValidationError);
	if (OutError)
	{
		*OutError = ValidationError;
	}
	return bIsValid;
}

const FGameXXKCardInstance* GameXXKCardRules::FindInstance(
	const FGameXXKBattleDeckState& Deck,
	const FName InstanceId,
	EGameXXKCardZone& OutZone)
{
	OutZone = EGameXXKCardZone::Invalid;
	if (const FGameXXKCardInstance* Instance = FindInZone(Deck.DrawPile, InstanceId))
	{
		OutZone = EGameXXKCardZone::DrawPile;
		return Instance;
	}
	if (const FGameXXKCardInstance* Instance = FindInZone(Deck.Hand, InstanceId))
	{
		OutZone = EGameXXKCardZone::Hand;
		return Instance;
	}
	if (const FGameXXKCardInstance* Instance = FindInZone(Deck.DiscardPile, InstanceId))
	{
		OutZone = EGameXXKCardZone::DiscardPile;
		return Instance;
	}
	return nullptr;
}

const FGameXXKCardInstance* GameXXKCardRules::GetDrawPileTop(const FGameXXKBattleDeckState& Deck)
{
	return Deck.DrawPile.IsEmpty() ? nullptr : &Deck.DrawPile.Last();
}

bool GameXXKCardRules::HasPendingChoice(const FGameXXKBattleDeckState& Deck)
{
	return IsActiveChoice(Deck.PendingChoice.Kind);
}
