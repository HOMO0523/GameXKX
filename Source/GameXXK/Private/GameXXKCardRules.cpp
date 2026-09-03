#include "GameXXKCardRules.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKCardQualityRules.h"
#include "GameXXKCharacterStatRules.h"
#include "GameXXKCombatScalingRules.h"
#include "GameXXKEnemyCatalog.h"
#include "GameXXKEquipmentRules.h"

namespace
{
	// HandLimit is the normal round-refill target. Card effects may grow the hand to this separate
	// hard capacity; attempted draws beyond it keep every card in battle and reshuffle the remainder.
	constexpr int32 BattleHandCapacity = 20;
	constexpr int32 MaxSupportedHandLimit = BattleHandCapacity;

	struct FGameXXKCardPlayConditionSnapshot
	{
		TMap<FName, int32> MarkStacksByUnitId;
		TMap<FName, int32> MomentumStacksByUnitId;
	};

	FGameXXKCardPlayConditionSnapshot CaptureCardPlayConditionSnapshot(const FGameXXKCardBattleRuntime& Runtime)
	{
		FGameXXKCardPlayConditionSnapshot Snapshot;
		for (const FGameXXKCardCombatUnit& Unit : Runtime.Units)
		{
			Snapshot.MarkStacksByUnitId.Add(
				Unit.UnitId,
				GameXXKCardRules::GetCombatStatusStacks(Unit, EGameXXKCardStatus::Mark));
			Snapshot.MomentumStacksByUnitId.Add(
				Unit.UnitId,
				GameXXKCardRules::GetCombatStatusStacks(Unit, EGameXXKCardStatus::Momentum));
		}
		return Snapshot;
	}

	int32 GetConditionStatusStacks(
		const FGameXXKCardCombatUnit* Target,
		const EGameXXKCardStatus Status,
		const FGameXXKCardPlayConditionSnapshot* Snapshot)
	{
		if (!Target)
		{
			return 0;
		}
		if (Status == EGameXXKCardStatus::Mark && Snapshot)
		{
			return Snapshot->MarkStacksByUnitId.FindRef(Target->UnitId);
		}
		return GameXXKCardRules::GetCombatStatusStacks(*Target, Status);
	}

	bool IsActiveChoice(const EGameXXKCardPendingChoiceKind Kind)
	{
		return Kind == EGameXXKCardPendingChoiceKind::ForcedDiscard
			|| Kind == EGameXXKCardPendingChoiceKind::InsightChooseToHand
			|| Kind == EGameXXKCardPendingChoiceKind::HeroTaskSearchChooseToHand;
	}

	bool IsConcreteCardQuality(const EGameXXKCardQuality Quality)
	{
		return Quality == EGameXXKCardQuality::Common
			|| Quality == EGameXXKCardQuality::Rare
			|| Quality == EGameXXKCardQuality::Epic;
	}

	bool IsValidInstance(const FGameXXKCardInstance& Instance)
	{
		const bool bValidTemporaryState = Instance.bTemporary
			? Instance.EnergyCostOverride >= 0
				&& Instance.ManaCostOverride >= 0
				&& Instance.ExpireAfterPlayerRound > 0
			: Instance.EnergyCostOverride == INDEX_NONE
				&& Instance.ManaCostOverride == INDEX_NONE
				&& Instance.ExpireAfterPlayerRound == 0;
		return !Instance.InstanceId.IsNone()
			&& !Instance.CardId.IsNone()
			&& IsConcreteCardQuality(Instance.CurrentQuality)
			&& !Instance.OwnerUnitId.IsNone()
			&& !Instance.SourceEntryId.IsNone()
			&& Instance.AcquisitionOrdinal != INDEX_NONE
			&& bValidTemporaryState;
	}

	/** Pending-choice candidates are serialized UI views, so compare every stable field before trusting them. */
	bool IsSameInstance(const FGameXXKCardInstance& Left, const FGameXXKCardInstance& Right)
	{
		return Left.InstanceId == Right.InstanceId
			&& Left.CardId == Right.CardId
			&& Left.CurrentQuality == Right.CurrentQuality
			&& Left.OwnerUnitId == Right.OwnerUnitId
			&& Left.SourceEntryId == Right.SourceEntryId
			&& Left.AcquisitionOrdinal == Right.AcquisitionOrdinal
			&& Left.bTemporary == Right.bTemporary
			&& Left.EnergyCostOverride == Right.EnergyCostOverride
			&& Left.ManaCostOverride == Right.ManaCostOverride
			&& Left.ExpireAfterPlayerRound == Right.ExpireAfterPlayerRound;
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

	bool MoveRecyclableDiscardToDraw(FGameXXKBattleDeckState& InOutDeck)
	{
		TOptional<FGameXXKCardInstance> ResolvingCard;
		if (!InOutDeck.ResolvingCardInstanceId.IsNone())
		{
			const int32 ResolvingIndex = InOutDeck.DiscardPile.IndexOfByPredicate([&InOutDeck](const FGameXXKCardInstance& Instance)
			{
				return Instance.InstanceId == InOutDeck.ResolvingCardInstanceId;
			});
			if (ResolvingIndex != INDEX_NONE)
			{
				ResolvingCard.Emplace(MoveTemp(InOutDeck.DiscardPile[ResolvingIndex]));
				InOutDeck.DiscardPile.RemoveAt(ResolvingIndex, 1, EAllowShrinking::No);
			}
		}

		InOutDeck.DrawPile = MoveTemp(InOutDeck.DiscardPile);
		if (ResolvingCard.IsSet())
		{
			InOutDeck.DiscardPile.Add(MoveTemp(ResolvingCard.GetValue()));
		}
		return !InOutDeck.DrawPile.IsEmpty();
	}

	bool EnsureDrawPileHasCard(FGameXXKBattleDeckState& InOutDeck)
	{
		if (!InOutDeck.DrawPile.IsEmpty())
		{
			return true;
		}

		if (!MoveRecyclableDiscardToDraw(InOutDeck))
		{
			return false;
		}
		ShufflePile(InOutDeck.DrawPile, InOutDeck.CurrentRandomState);
		return true;
	}

	bool IsAutomaticHandOrderBefore(
		const FGameXXKCardInstance& Left,
		const FGameXXKCardInstance& Right)
	{
		return Left.AcquisitionOrdinal != Right.AcquisitionOrdinal
			? Left.AcquisitionOrdinal < Right.AcquisitionOrdinal
			: Left.InstanceId.LexicalLess(Right.InstanceId);
	}

	bool MaterializePendingAutomaticHandCards(
		FGameXXKBattleDeckState& InOutDeck,
		FString& OutError)
	{
		OutError.Reset();
		while (InOutDeck.Hand.Num() < BattleHandCapacity
			&& !InOutDeck.PendingAutomaticHandCards.IsEmpty())
		{
			InOutDeck.Hand.Add(MoveTemp(InOutDeck.PendingAutomaticHandCards[0]));
			InOutDeck.PendingAutomaticHandCards.RemoveAt(0, 1, EAllowShrinking::No);
		}
		return true;
	}

	bool QueueInstanceForAutomaticHand(
		FGameXXKBattleDeckState& InOutDeck,
		const FName InstanceId,
		FString& OutError)
	{
		OutError.Reset();
		const auto ContainsInstanceId = [InstanceId](const TArray<FGameXXKCardInstance>& Zone)
		{
			return Zone.ContainsByPredicate([InstanceId](const FGameXXKCardInstance& Instance)
			{
				return Instance.InstanceId == InstanceId;
			});
		};
		if (InstanceId.IsNone()
			|| ContainsInstanceId(InOutDeck.Hand)
			|| ContainsInstanceId(InOutDeck.PendingAutomaticHandCards))
		{
			OutError = TEXT("An automatic hand request must reference one unqueued draw or discard instance.");
			return false;
		}

		TArray<FGameXXKCardInstance>* SourceZone = nullptr;
		int32 SourceIndex = InOutDeck.DrawPile.IndexOfByPredicate([InstanceId](const FGameXXKCardInstance& Instance)
		{
			return Instance.InstanceId == InstanceId;
		});
		if (SourceIndex != INDEX_NONE)
		{
			SourceZone = &InOutDeck.DrawPile;
		}
		else
		{
			SourceIndex = InOutDeck.DiscardPile.IndexOfByPredicate([InstanceId](const FGameXXKCardInstance& Instance)
			{
				return Instance.InstanceId == InstanceId;
			});
			if (SourceIndex != INDEX_NONE)
			{
				SourceZone = &InOutDeck.DiscardPile;
			}
		}
		if (!SourceZone)
		{
			OutError = TEXT("An automatic hand request no longer exists in draw or discard.");
			return false;
		}

		InOutDeck.PendingAutomaticHandCards.Add(MoveTemp((*SourceZone)[SourceIndex]));
		SourceZone->RemoveAt(SourceIndex, 1, EAllowShrinking::No);
		InOutDeck.PendingAutomaticHandCards.Sort([](const FGameXXKCardInstance& Left, const FGameXXKCardInstance& Right)
		{
			return IsAutomaticHandOrderBefore(Left, Right);
		});
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
			|| !ValidateZone(Deck.DiscardPile, TEXT("DiscardPile"))
			|| !ValidateZone(Deck.ExhaustPile, TEXT("ExhaustPile"))
			|| !ValidateZone(Deck.PendingAutomaticHandCards, TEXT("PendingAutomaticHand")))
		{
			return false;
		}
		if (ZoneIds.Num() != LedgerIds.Num())
		{
			OutError = TEXT("A ledger instance is not present in any logical card zone.");
			return false;
		}
		if (!Deck.ResolvingCardInstanceId.IsNone()
			&& !LedgerIds.Contains(Deck.ResolvingCardInstanceId))
		{
			OutError = TEXT("The synchronously resolving card is absent from the active instance ledger.");
			return false;
		}
		if (Deck.Hand.Num() > BattleHandCapacity)
		{
			OutError = TEXT("Hand exceeds the twenty-card battle capacity.");
			return false;
		}
		if (!Deck.PendingAutomaticHandCards.IsEmpty() && Deck.Hand.Num() < BattleHandCapacity)
		{
			OutError = TEXT("Automatic hand overflow remained queued despite an available hand slot.");
			return false;
		}
		for (int32 Index = 1; Index < Deck.PendingAutomaticHandCards.Num(); ++Index)
		{
			if (IsAutomaticHandOrderBefore(
				Deck.PendingAutomaticHandCards[Index],
				Deck.PendingAutomaticHandCards[Index - 1]))
			{
				OutError = TEXT("Automatic hand overflow is not in stable acquisition order.");
				return false;
			}
		}

		if (Deck.PendingChoice.bLegacyHeroTaskSearch
			&& Deck.PendingChoice.Kind != EGameXXKCardPendingChoiceKind::HeroTaskSearchChooseToHand)
		{
			OutError = TEXT("A legacy Hero search marker belongs to an unrelated or inactive choice.");
			return false;
		}
		switch (Deck.PendingChoice.Kind)
		{
		case EGameXXKCardPendingChoiceKind::None:
			if (!Deck.PendingChoice.Candidates.IsEmpty()
				|| Deck.PendingChoice.RequiredCount != 0
				|| Deck.PendingChoice.RequiredDiscardCount != 0
				|| Deck.PendingChoice.RequiredHandPickCount != 0
				|| !Deck.PendingChoice.InsightTopOrder.IsEmpty()
				|| !Deck.PendingChoice.InsightPickedInstanceId.IsNone()
				|| !Deck.PendingChoice.InsightReorderedInstanceIds.IsEmpty()
				|| Deck.PendingChoice.bCanCancel
				|| !Deck.PendingChoice.bCancelPreservesDrawTop)
			{
				OutError = TEXT("No-pending-choice state contains stale choice data.");
				return false;
			}
			return true;

		case EGameXXKCardPendingChoiceKind::ForcedDiscard:
			if (Deck.PendingChoice.RequiredCount <= 0
				|| Deck.PendingChoice.RequiredCount > Deck.Hand.Num()
				|| Deck.PendingChoice.RequiredDiscardCount != Deck.PendingChoice.RequiredCount
				|| Deck.PendingChoice.RequiredHandPickCount != 0
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
				|| Deck.Hand.Num() >= BattleHandCapacity
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

		case EGameXXKCardPendingChoiceKind::HeroTaskSearchChooseToHand:
			if (Deck.PendingChoice.RequiredCount != 1
				|| Deck.PendingChoice.RequiredHandPickCount != 1
				|| Deck.PendingChoice.RequiredDiscardCount != 0
				|| Deck.PendingChoice.bCanCancel
				|| Deck.Hand.Num() >= BattleHandCapacity
				|| Deck.PendingChoice.Candidates.IsEmpty()
				|| !Deck.PendingChoice.InsightTopOrder.IsEmpty()
				|| !Deck.PendingChoice.InsightPickedInstanceId.IsNone()
				|| !Deck.PendingChoice.InsightReorderedInstanceIds.IsEmpty())
			{
				OutError = TEXT("Hero spell-task search choice is malformed.");
				return false;
			}
			{
				TSet<FName> CandidateIds;
				int32 PreviousAcquisitionOrdinal = MIN_int32;
				FName PreviousInstanceId = NAME_None;
				for (const FGameXXKCardInstance& Candidate : Deck.PendingChoice.Candidates)
				{
					const FGameXXKCardInstance* Canonical = Deck.DrawPile.FindByPredicate([&Candidate](const FGameXXKCardInstance& Instance)
					{
						return Instance.InstanceId == Candidate.InstanceId;
					});
					if (!Canonical)
					{
						Canonical = Deck.DiscardPile.FindByPredicate([&Candidate](const FGameXXKCardInstance& Instance)
						{
							return Instance.InstanceId == Candidate.InstanceId;
						});
					}
					const bool bOutOfOrder = Candidate.AcquisitionOrdinal < PreviousAcquisitionOrdinal
						|| (Candidate.AcquisitionOrdinal == PreviousAcquisitionOrdinal
							&& !PreviousInstanceId.IsNone()
							&& Candidate.InstanceId.LexicalLess(PreviousInstanceId));
					if (Candidate.InstanceId.IsNone()
						|| CandidateIds.Contains(Candidate.InstanceId)
						|| !Canonical
						|| !IsSameInstance(Candidate, *Canonical)
						|| bOutOfOrder)
					{
						OutError = TEXT("Hero spell-task search candidates are stale, duplicated, or out of order.");
						return false;
					}
					CandidateIds.Add(Candidate.InstanceId);
					PreviousAcquisitionOrdinal = Candidate.AcquisitionOrdinal;
					PreviousInstanceId = Candidate.InstanceId;
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

	bool MoveResolvedHandCard(
		FGameXXKBattleDeckState& InOutDeck,
		const FName InstanceId,
		const bool bExhaust,
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
		TArray<FGameXXKCardInstance>& Destination = bExhaust ? NewDeck.ExhaustPile : NewDeck.DiscardPile;
		Destination.Add(MoveTemp(NewDeck.Hand[HandIndex]));
		NewDeck.Hand.RemoveAt(HandIndex, 1, EAllowShrinking::No);
		if (!MaterializePendingAutomaticHandCards(NewDeck, ValidationError))
		{
			return SetFailure(OutError, ValidationError);
		}
		if (!ValidateDeckStateInternal(NewDeck, ValidationError))
		{
			return SetFailure(OutError, ValidationError);
		}
		InOutDeck = MoveTemp(NewDeck);
		return true;
	}

	bool IsConcreteTargetSide(const EGameXXKCardTargetSide Side)
	{
		return Side == EGameXXKCardTargetSide::Party || Side == EGameXXKCardTargetSide::Enemy;
	}

	bool IsManualTargetMode(const EGameXXKCardTargetMode Mode)
	{
		return Mode == EGameXXKCardTargetMode::SingleEnemy
			|| Mode == EGameXXKCardTargetMode::SingleAlly
			|| Mode == EGameXXKCardTargetMode::OtherAlly
			|| Mode == EGameXXKCardTargetMode::AnyLivingUnit;
	}

	bool IsAutomaticTargetMode(const EGameXXKCardTargetMode Mode)
	{
		return Mode == EGameXXKCardTargetMode::None
			|| Mode == EGameXXKCardTargetMode::Self
			|| Mode == EGameXXKCardTargetMode::AllEnemies
			|| Mode == EGameXXKCardTargetMode::AllAllies
			|| Mode == EGameXXKCardTargetMode::AllOtherAllies
			|| Mode == EGameXXKCardTargetMode::RandomEnemy
			|| Mode == EGameXXKCardTargetMode::LowestHealthAlly
			|| Mode == EGameXXKCardTargetMode::LowestHealthOtherAlly;
	}

	bool IsSupportedTargetMode(const EGameXXKCardTargetMode Mode)
	{
		return IsManualTargetMode(Mode) || IsAutomaticTargetMode(Mode);
	}

	bool TargetModeRequiresDifferentFromOwner(const EGameXXKCardTargetMode Mode)
	{
		return Mode == EGameXXKCardTargetMode::OtherAlly
			|| Mode == EGameXXKCardTargetMode::AllOtherAllies
			|| Mode == EGameXXKCardTargetMode::LowestHealthOtherAlly;
	}

	EGameXXKCardTargetPresentation PresentationForTargetMode(const EGameXXKCardTargetMode Mode)
	{
		switch (Mode)
		{
		case EGameXXKCardTargetMode::None:
			return EGameXXKCardTargetPresentation::NoSelection;
		case EGameXXKCardTargetMode::Self:
			return EGameXXKCardTargetPresentation::Self;
		case EGameXXKCardTargetMode::SingleEnemy:
		case EGameXXKCardTargetMode::SingleAlly:
		case EGameXXKCardTargetMode::OtherAlly:
		case EGameXXKCardTargetMode::AnyLivingUnit:
			return EGameXXKCardTargetPresentation::PlayerSelectsUnit;
		case EGameXXKCardTargetMode::AllEnemies:
		case EGameXXKCardTargetMode::AllAllies:
		case EGameXXKCardTargetMode::AllOtherAllies:
			return EGameXXKCardTargetPresentation::Group;
		case EGameXXKCardTargetMode::RandomEnemy:
		case EGameXXKCardTargetMode::LowestHealthAlly:
		case EGameXXKCardTargetMode::LowestHealthOtherAlly:
			return EGameXXKCardTargetPresentation::AutomaticUnit;
		case EGameXXKCardTargetMode::Invalid:
		default:
			return EGameXXKCardTargetPresentation::Invalid;
		}
	}

	bool IsConcreteTerrain(const EGameXXKCardTerrain Terrain)
	{
		return Terrain == EGameXXKCardTerrain::Plain
			|| Terrain == EGameXXKCardTerrain::Cliff
			|| Terrain == EGameXXKCardTerrain::Forest
			|| Terrain == EGameXXKCardTerrain::WaterShore
			|| Terrain == EGameXXKCardTerrain::Ferry
			|| Terrain == EGameXXKCardTerrain::Village
			|| Terrain == EGameXXKCardTerrain::Cave;
	}

	bool IsValidTargetPresentation(const EGameXXKCardTargetMode Mode, const EGameXXKCardTargetPresentation Presentation)
	{
		return Presentation == EGameXXKCardTargetPresentation::Invalid
			|| Presentation == PresentationForTargetMode(Mode);
	}

	bool ValidateTargetSpec(const FGameXXKCardTargetSpec& TargetSpec, FString& OutError)
	{
		if (!IsSupportedTargetMode(TargetSpec.Mode))
		{
			OutError = TEXT("Card definition has an invalid or unsupported target mode.");
			return false;
		}
		if (!IsValidTargetPresentation(TargetSpec.Mode, TargetSpec.Presentation))
		{
			OutError = TEXT("Card definition target mode and presentation do not form a valid interaction contract.");
			return false;
		}
		const EGameXXKCardUnitState ExpectedUnitState = TargetSpec.Mode == EGameXXKCardTargetMode::None
			? EGameXXKCardUnitState::Any
			: EGameXXKCardUnitState::Living;
		// First release only supports living combatants. Defeated-target flows require their own mode and UI.
		if (TargetSpec.RequiredUnitState != ExpectedUnitState)
		{
			OutError = TEXT("Card definition requests an unsupported target unit state for its target mode.");
			return false;
		}
		if (TargetSpec.RequiredStatus == EGameXXKCardStatus::Invalid
			|| TargetSpec.ForbiddenStatus == EGameXXKCardStatus::Invalid
			|| (TargetSpec.RequiredStatus != EGameXXKCardStatus::None && TargetSpec.RequiredStatus == TargetSpec.ForbiddenStatus))
		{
			OutError = TEXT("Card definition contains an invalid target status filter.");
			return false;
		}
		if ((TargetSpec.RequiredStatus == EGameXXKCardStatus::None && TargetSpec.RequiredStatusMinimumStacks != 0)
			|| (TargetSpec.RequiredStatus != EGameXXKCardStatus::None && TargetSpec.RequiredStatusMinimumStacks <= 0))
		{
			OutError = TEXT("Card definition has inconsistent required-status stack metadata.");
			return false;
		}
		if (!FMath::IsFinite(TargetSpec.MinimumHealthPercent)
			|| !FMath::IsFinite(TargetSpec.MaximumHealthPercent)
			|| TargetSpec.MinimumHealthPercent < 0.0f
			|| TargetSpec.MaximumHealthPercent > 100.0f
			|| TargetSpec.MinimumHealthPercent > TargetSpec.MaximumHealthPercent)
		{
			OutError = TEXT("Card definition has an invalid target health-percent range.");
			return false;
		}
		if ((TargetSpec.RequiredTerrain != EGameXXKCardTerrain::Invalid && !IsConcreteTerrain(TargetSpec.RequiredTerrain))
			|| (TargetSpec.AlternateRequiredTerrain != EGameXXKCardTerrain::Invalid && !IsConcreteTerrain(TargetSpec.AlternateRequiredTerrain))
			|| (TargetSpec.RequiredTerrain == EGameXXKCardTerrain::Invalid && TargetSpec.AlternateRequiredTerrain != EGameXXKCardTerrain::Invalid)
			|| (TargetSpec.RequiredTerrain != EGameXXKCardTerrain::Invalid && TargetSpec.RequiredTerrain == TargetSpec.AlternateRequiredTerrain))
		{
			OutError = TEXT("Card definition has invalid target terrain metadata.");
			return false;
		}
		if (TargetSpec.bRequireDifferentFromOwner != TargetModeRequiresDifferentFromOwner(TargetSpec.Mode))
		{
			OutError = TEXT("Card definition owner-difference metadata does not match its target mode.");
			return false;
		}

		for (const FGameXXKCardTargetModeOverride& Override : TargetSpec.ModeOverrides)
		{
			if (!IsSupportedTargetMode(Override.Mode) || !IsValidTargetPresentation(Override.Mode, Override.Presentation))
			{
				OutError = TEXT("Card definition has an invalid target-mode override contract.");
				return false;
			}
			switch (Override.ConditionType)
			{
			case EGameXXKCardTargetModeOverrideConditionType::TerrainIsAny:
				if (!IsConcreteTerrain(Override.Terrain)
					|| (Override.AlternateTerrain != EGameXXKCardTerrain::Invalid && !IsConcreteTerrain(Override.AlternateTerrain))
					|| Override.Terrain == Override.AlternateTerrain
					|| Override.Status != EGameXXKCardStatus::None
					|| Override.MinimumStatusStacks != 0)
				{
					OutError = TEXT("Terrain target-mode override has invalid terrain metadata.");
					return false;
				}
				break;
			case EGameXXKCardTargetModeOverrideConditionType::OwnerHasStatus:
				if (Override.Status == EGameXXKCardStatus::Invalid
					|| Override.Status == EGameXXKCardStatus::None
					|| Override.MinimumStatusStacks <= 0
					|| Override.Terrain != EGameXXKCardTerrain::Invalid
					|| Override.AlternateTerrain != EGameXXKCardTerrain::Invalid)
				{
					OutError = TEXT("Owner-status target-mode override has invalid status metadata.");
					return false;
				}
				break;
			case EGameXXKCardTargetModeOverrideConditionType::TargetHasStatus:
				OutError = TEXT("Target-status target-mode overrides require a selected target and are unsupported before selection.");
				return false;
			case EGameXXKCardTargetModeOverrideConditionType::Invalid:
			default:
				OutError = TEXT("Card definition has an invalid target-mode override condition.");
				return false;
			}
		}
		return true;
	}

	bool DoesTerrainMatch(
		const EGameXXKCardTerrain CurrentTerrain,
		const EGameXXKCardTerrain RequiredTerrain,
		const EGameXXKCardTerrain AlternateTerrain)
	{
		return RequiredTerrain == EGameXXKCardTerrain::Invalid
			|| CurrentTerrain == RequiredTerrain
			|| (AlternateTerrain != EGameXXKCardTerrain::Invalid && CurrentTerrain == AlternateTerrain);
	}

	int32 GetTargetStatusStacks(const FGameXXKCardTargetUnit& Unit, const EGameXXKCardStatus Status)
	{
		int64 Total = 0;
		for (const FGameXXKCardStatusStack& Stack : Unit.Statuses)
		{
			if (Stack.Status == Status && Stack.Stacks > 0)
			{
				Total = FMath::Min<int64>(MAX_int32, Total + static_cast<int64>(Stack.Stacks));
			}
		}
		return static_cast<int32>(Total);
	}

	bool ValidateAndSortTargetUnits(
		const TArray<FGameXXKCardTargetUnit>& TargetUnits,
		TArray<const FGameXXKCardTargetUnit*>& OutSortedUnits,
		FString& OutError)
	{
		OutSortedUnits.Reset();
		TSet<FName> SeenIds;
		for (const FGameXXKCardTargetUnit& Unit : TargetUnits)
		{
			if (Unit.UnitId.IsNone() || SeenIds.Contains(Unit.UnitId))
			{
				OutError = TEXT("Target units must have unique, non-empty stable UnitIds.");
				return false;
			}
			if (!IsConcreteTargetSide(Unit.Side) || Unit.MaxHP <= 0 || Unit.HP < 0 || Unit.HP > Unit.MaxHP || Unit.StableSortOrder == INDEX_NONE || Unit.StableSortOrder < 0)
			{
				OutError = TEXT("Target unit has invalid side, health, or stable sort data.");
				return false;
			}
			if (Unit.bLiving != (Unit.HP > 0))
			{
				OutError = TEXT("Target unit living state must match its health.");
				return false;
			}
			for (const FGameXXKCardStatusStack& Stack : Unit.Statuses)
			{
				if (Stack.Status == EGameXXKCardStatus::Invalid || Stack.Stacks < 0)
				{
					OutError = TEXT("Target unit contains an invalid status stack.");
					return false;
				}
			}
			SeenIds.Add(Unit.UnitId);
			OutSortedUnits.Add(&Unit);
		}

		OutSortedUnits.Sort([](const FGameXXKCardTargetUnit& Left, const FGameXXKCardTargetUnit& Right)
		{
			if (Left.StableSortOrder != Right.StableSortOrder)
			{
				return Left.StableSortOrder < Right.StableSortOrder;
			}
			return Left.UnitId.LexicalLess(Right.UnitId);
		});
		return true;
	}

	const FGameXXKCardTargetUnit* FindTargetUnitById(
		const TArray<const FGameXXKCardTargetUnit*>& SortedUnits,
		const FName UnitId)
	{
		for (const FGameXXKCardTargetUnit* Unit : SortedUnits)
		{
			if (Unit && Unit->UnitId == UnitId)
			{
				return Unit;
			}
		}
		return nullptr;
	}

	bool ResolveEffectiveTargetMode(
		const FGameXXKCardDefinition& Definition,
		const FGameXXKCardTargetUnit& SourceUnit,
		const EGameXXKCardTerrain Terrain,
		EGameXXKCardTargetMode& OutMode,
		EGameXXKCardTargetPresentation& OutPresentation,
		FString& OutError)
	{
		OutMode = Definition.TargetSpec.Mode;
		OutPresentation = Definition.TargetSpec.Presentation == EGameXXKCardTargetPresentation::Invalid
			? PresentationForTargetMode(OutMode)
			: Definition.TargetSpec.Presentation;
		if (OutMode == EGameXXKCardTargetMode::Invalid || OutPresentation == EGameXXKCardTargetPresentation::Invalid)
		{
			OutError = TEXT("Card definition has an invalid target mode or presentation.");
			return false;
		}

		for (const FGameXXKCardTargetModeOverride& Override : Definition.TargetSpec.ModeOverrides)
		{
			if (Override.ConditionType == EGameXXKCardTargetModeOverrideConditionType::Invalid
				|| Override.Mode == EGameXXKCardTargetMode::Invalid)
			{
				OutError = TEXT("Card definition has an invalid target-mode override.");
				return false;
			}

			bool bMatches = false;
			switch (Override.ConditionType)
			{
			case EGameXXKCardTargetModeOverrideConditionType::TerrainIsAny:
				bMatches = DoesTerrainMatch(Terrain, Override.Terrain, Override.AlternateTerrain);
				break;
			case EGameXXKCardTargetModeOverrideConditionType::OwnerHasStatus:
				bMatches = Override.Status != EGameXXKCardStatus::Invalid
					&& Override.Status != EGameXXKCardStatus::None
					&& Override.MinimumStatusStacks > 0
					&& GetTargetStatusStacks(SourceUnit, Override.Status) >= Override.MinimumStatusStacks;
				break;
			case EGameXXKCardTargetModeOverrideConditionType::TargetHasStatus:
				OutError = TEXT("Target-status target-mode overrides require a selected target and cannot build a pre-selection request.");
				return false;
			default:
				OutError = TEXT("Card definition has an unsupported target-mode override.");
				return false;
			}

			if (bMatches)
			{
				OutMode = Override.Mode;
				OutPresentation = Override.Presentation == EGameXXKCardTargetPresentation::Invalid
					? PresentationForTargetMode(Override.Mode)
					: Override.Presentation;
				if (OutPresentation == EGameXXKCardTargetPresentation::Invalid)
				{
					OutError = TEXT("Target-mode override has an invalid presentation.");
					return false;
				}
				break;
			}
		}
		return true;
	}

	EGameXXKCardTargetDisabledReason EvaluateTargetCandidate(
		const FGameXXKCardTargetSpec& TargetSpec,
		const EGameXXKCardTargetMode Mode,
		const EGameXXKCardTerrain Terrain,
		const FName SourceUnitId,
		const FGameXXKCardTargetUnit& Candidate)
	{
		const bool bIsSource = Candidate.UnitId == SourceUnitId;
		switch (Mode)
		{
		case EGameXXKCardTargetMode::Self:
			if (!bIsSource)
			{
				return EGameXXKCardTargetDisabledReason::NotSource;
			}
			break;
		case EGameXXKCardTargetMode::SingleEnemy:
		case EGameXXKCardTargetMode::AllEnemies:
		case EGameXXKCardTargetMode::RandomEnemy:
			if (Candidate.Side != EGameXXKCardTargetSide::Enemy)
			{
				return EGameXXKCardTargetDisabledReason::WrongSide;
			}
			break;
		case EGameXXKCardTargetMode::SingleAlly:
		case EGameXXKCardTargetMode::OtherAlly:
		case EGameXXKCardTargetMode::AllAllies:
		case EGameXXKCardTargetMode::AllOtherAllies:
		case EGameXXKCardTargetMode::LowestHealthAlly:
		case EGameXXKCardTargetMode::LowestHealthOtherAlly:
			if (Candidate.Side != EGameXXKCardTargetSide::Party)
			{
				return EGameXXKCardTargetDisabledReason::WrongSide;
			}
			break;
		case EGameXXKCardTargetMode::AnyLivingUnit:
			break;
		case EGameXXKCardTargetMode::None:
		case EGameXXKCardTargetMode::Invalid:
		default:
			return EGameXXKCardTargetDisabledReason::InvalidHealth;
		}

		const bool bOtherAllyMode = Mode == EGameXXKCardTargetMode::OtherAlly
			|| Mode == EGameXXKCardTargetMode::AllOtherAllies
			|| Mode == EGameXXKCardTargetMode::LowestHealthOtherAlly;
		if ((bOtherAllyMode || TargetSpec.bRequireDifferentFromOwner) && bIsSource)
		{
			return EGameXXKCardTargetDisabledReason::OwnerExcluded;
		}

		if (!Candidate.bLiving)
		{
			return EGameXXKCardTargetDisabledReason::NotLiving;
		}
		if (!DoesTerrainMatch(Terrain, TargetSpec.RequiredTerrain, TargetSpec.AlternateRequiredTerrain))
		{
			return EGameXXKCardTargetDisabledReason::TerrainMismatch;
		}
		if (TargetSpec.RequiredStatus != EGameXXKCardStatus::None
			&& (TargetSpec.RequiredStatus == EGameXXKCardStatus::Invalid
				|| TargetSpec.RequiredStatusMinimumStacks <= 0
				|| GetTargetStatusStacks(Candidate, TargetSpec.RequiredStatus) < TargetSpec.RequiredStatusMinimumStacks))
		{
			return EGameXXKCardTargetDisabledReason::RequiredStatusMissing;
		}
		if (TargetSpec.ForbiddenStatus != EGameXXKCardStatus::None
			&& TargetSpec.ForbiddenStatus != EGameXXKCardStatus::Invalid
			&& GetTargetStatusStacks(Candidate, TargetSpec.ForbiddenStatus) > 0)
		{
			return EGameXXKCardTargetDisabledReason::ForbiddenStatusPresent;
		}
		if (Candidate.MaxHP <= 0)
		{
			return EGameXXKCardTargetDisabledReason::InvalidHealth;
		}
		const float HealthPercent = 100.0f * static_cast<float>(Candidate.HP) / static_cast<float>(Candidate.MaxHP);
		if (HealthPercent < TargetSpec.MinimumHealthPercent)
		{
			return EGameXXKCardTargetDisabledReason::HealthBelowMinimum;
		}
		if (HealthPercent > TargetSpec.MaximumHealthPercent)
		{
			return EGameXXKCardTargetDisabledReason::HealthAboveMaximum;
		}
		return EGameXXKCardTargetDisabledReason::None;
	}

	bool IsLowerHealthCandidate(const FGameXXKCardTargetUnit& Left, const FGameXXKCardTargetUnit& Right)
	{
		const int64 LeftScaledHealth = static_cast<int64>(Left.HP) * static_cast<int64>(Right.MaxHP);
		const int64 RightScaledHealth = static_cast<int64>(Right.HP) * static_cast<int64>(Left.MaxHP);
		if (LeftScaledHealth != RightScaledHealth)
		{
			return LeftScaledHealth < RightScaledHealth;
		}
		if (Left.StableSortOrder != Right.StableSortOrder)
		{
			return Left.StableSortOrder < Right.StableSortOrder;
		}
		return Left.UnitId.LexicalLess(Right.UnitId);
	}

	bool ValidateRequestCandidateViews(
		const FGameXXKCardTargetRequest& Request,
		const TArray<const FGameXXKCardTargetUnit*>& SortedUnits,
		FString& OutError)
	{
		if (Request.EffectiveMode == EGameXXKCardTargetMode::None)
		{
			if (!Request.CandidateViews.IsEmpty() || !Request.AutomaticTargetUnitIds.IsEmpty())
			{
				OutError = TEXT("No-target request contains unexpected unit candidates.");
				return false;
			}
			return true;
		}
		if (Request.CandidateViews.Num() != SortedUnits.Num())
		{
			OutError = TEXT("Target request candidate views no longer match the battle-unit set.");
			return false;
		}
		TSet<FName> SeenCandidateIds;
		for (const FGameXXKCardTargetCandidateView& Candidate : Request.CandidateViews)
		{
			const FGameXXKCardTargetUnit* Unit = FindTargetUnitById(SortedUnits, Candidate.UnitId);
			if (Candidate.UnitId.IsNone() || SeenCandidateIds.Contains(Candidate.UnitId) || !Unit || Candidate.Side != Unit->Side)
			{
				OutError = TEXT("Target request candidate views contain stale or duplicate unit IDs.");
				return false;
			}
			SeenCandidateIds.Add(Candidate.UnitId);
		}
		return true;
	}

	// Phase helpers live beside the deck invariants, while their combat-only primitives are defined
	// later in this translation unit. Keep the forward declarations narrow so UI callers can only use
	// the public phase API below.
	constexpr int32 MaxDeferredPhaseEnergy = 99;
	bool IsStableUnitOrderBefore(const FGameXXKCardCombatUnit& Left, const FGameXXKCardCombatUnit& Right);
	FGameXXKCardCombatUnit* FindCombatUnitById(TArray<FGameXXKCardCombatUnit>& Units, FName UnitId);
	const FGameXXKCardCombatUnit* FindCombatUnitById(const TArray<FGameXXKCardCombatUnit>& Units, FName UnitId);
	bool ValidateCardBattleRuntimeInternal(const FGameXXKCardBattleRuntime& Runtime, FString& OutError);
	bool ApplyCombatEndPhaseDotForRuntime(
		FGameXXKCardBattleRuntime& InOutRuntime,
		FName TargetUnitId,
		int32& OutHealthDamage,
		int32& OutPacketHealthAfter,
		FString* OutError);
	bool TryApplyEffectConditionAndConsumption(
		const FGameXXKCardEffectCondition& Condition,
		FGameXXKCardBattleRuntime& InOutRuntime,
		FGameXXKCardCombatUnit& InOutOwner,
		FGameXXKCardCombatUnit* Target,
		const FGameXXKCardPlayConditionSnapshot* Snapshot,
		bool& OutSatisfied,
		int32& OutConsumed,
		FString& OutError);

	void UpdateBattleTerminalPhase(FGameXXKCardBattleRuntime& InOutRuntime)
	{
		bool bHasLivingParty = false;
		bool bHasLivingEnemy = false;
		bool bHasHero = false;
		bool bHasLivingHero = false;
		for (const FGameXXKCardCombatUnit& Unit : InOutRuntime.Units)
		{
			bHasLivingParty |= Unit.bLiving && Unit.Side == EGameXXKCardTargetSide::Party;
			bHasLivingEnemy |= Unit.bLiving && Unit.Side == EGameXXKCardTargetSide::Enemy;
			if (Unit.Side == EGameXXKCardTargetSide::Party && Unit.Role == EGameXXKCharacterRole::Hero)
			{
				bHasHero = true;
				bHasLivingHero |= Unit.bLiving;
			}
		}
		if (!bHasLivingParty || !bHasLivingEnemy || (bHasHero && !bHasLivingHero))
		{
			// Enemy elimination wins even when the same resolution also defeats the hero.
			// Otherwise the battle contract ends immediately when its fixed hero falls;
			// surviving companions do not continue an ownerless battle.
			InOutRuntime.Phase = !bHasLivingEnemy
				? EGameXXKCardBattlePhase::Victory
				: EGameXXKCardBattlePhase::Defeat;
			// A next-player-hand effect has no legal recipient after a terminal transition.
			InOutRuntime.PendingNextPlayerHandEnergySurcharge = 0;
			InOutRuntime.PendingNextPlayerHandEnergySurchargeSourceUnitId = NAME_None;
			// A terminal battle has no later player/enemy boundary at which Blade or PoJun
			// payloads could resolve. Clear them at the single terminal transition point so
			// kills from reactions and adapter-level intent batches obey the same invariant
			// as kills produced directly by an active card.
			InOutRuntime.PendingBladeCharge = FGameXXKBladeChargeRuntime();
			InOutRuntime.PendingBladeDelayedCard = FGameXXKBladeDelayedCardRuntime();
			InOutRuntime.PendingBladeFinish = FGameXXKBladeFinishRuntime();
			InOutRuntime.PendingBladeNativeStyle = FGameXXKBladeStyleRuntime();
			InOutRuntime.PendingBladeResidualStyle = FGameXXKBladeStyleRuntime();
			InOutRuntime.BladeRetainedHandCardInstanceIds.Reset();
			for (FGameXXKEquipmentBattleEffectRuntime& EffectRuntime : InOutRuntime.EquipmentEffects)
			{
				EffectRuntime.PendingPoJunStyle = FGameXXKPoJunStoredStyleRuntime();
				EffectRuntime.PoJunChargeProgressRound = 0;
				EffectRuntime.bPoJunChargeConsumedThisRound = false;
				EffectRuntime.PendingPoJunReplayPlayerRound = 0;
			}
		}
	}

	/**
	 * Commits the one-way boss phase threshold after a complete combat packet has resolved.
	 * Callers operate on a transaction copy, so a malformed persisted enemy state cannot leak a
	 * partially entered phase into the live battle.
	 */
	bool EvaluateBossPhaseTransitions(FGameXXKCardBattleRuntime& InOutRuntime, FString& OutError)
	{
		FGameXXKCardBattleRuntime NewRuntime = InOutRuntime;
		for (FGameXXKCardCombatUnit& Unit : NewRuntime.Units)
		{
			if (!Unit.bLiving || Unit.Side != EGameXXKCardTargetSide::Enemy || Unit.EnemyDefinitionId.IsNone())
			{
				continue;
			}

			const FGameXXKEnemyDefinition* Definition = FGameXXKEnemyCatalog::Find(Unit.EnemyDefinitionId);
			if (!Definition || Definition->PhaseId == EGameXXKEnemyPhaseId::None)
			{
				continue;
			}
			if (Definition->PhaseThresholdPercent <= 0 || Definition->PhaseThresholdPercent > 100)
			{
				OutError = TEXT("A boss phase definition has an unsupported health threshold.");
				return false;
			}

			FGameXXKEnemyBattleState& EnemyState = NewRuntime.EnemyStates.FindOrAdd(Unit.UnitId);
			if (EnemyState.DefinitionId.IsNone())
			{
				EnemyState.DefinitionId = Definition->Id;
			}
			if (EnemyState.DefinitionId != Definition->Id)
			{
				OutError = TEXT("A boss phase transition found a mismatched persisted enemy definition.");
				return false;
			}

			const bool bAtOrBelowPhaseThreshold = static_cast<int64>(Unit.HP) * 100
				<= static_cast<int64>(Unit.MaxHP) * Definition->PhaseThresholdPercent;
			if (bAtOrBelowPhaseThreshold)
			{
				// This value is deliberately never cleared: healing above the threshold does not
				// replay, undo, or re-enter a boss phase.
				EnemyState.bPhaseTwo = true;
			}

			if (!EnemyState.bPhaseTwo || EnemyState.bPhaseStatModifiersApplied)
			{
				continue;
			}

			if (EnemyState.PhaseAttackModifier != 0 || EnemyState.PhaseDefenseModifier != 0)
			{
				OutError = TEXT("A boss phase state has a stat modifier without a committed phase entry.");
				return false;
			}

			const int64 BaselineAttack = FMath::Max<int64>(0,
				static_cast<int64>(Unit.Attack) - EnemyState.TemporaryAttackModifier);
			const int64 BaselineDefense = FMath::Max<int64>(0, Unit.Defense);
			const int32 PhaseAttack = static_cast<int32>(FMath::Clamp<int64>(
				BaselineAttack * Definition->PhaseTwoAttackPercent / 100,
				0,
				MAX_int32));
			const int32 PhaseDefense = static_cast<int32>(FMath::Clamp<int64>(
				BaselineDefense * Definition->PhaseTwoDefensePercent / 100,
				0,
				MAX_int32));
			EnemyState.PhaseAttackModifier = static_cast<int32>(FMath::Clamp<int64>(
				static_cast<int64>(PhaseAttack) - BaselineAttack,
				MIN_int32,
				MAX_int32));
			EnemyState.PhaseDefenseModifier = static_cast<int32>(FMath::Clamp<int64>(
				static_cast<int64>(PhaseDefense) - BaselineDefense,
				MIN_int32,
				MAX_int32));
			Unit.Attack = static_cast<int32>(FMath::Clamp<int64>(
				static_cast<int64>(Unit.Attack) + EnemyState.PhaseAttackModifier,
				0,
				MAX_int32));
			Unit.Defense = static_cast<int32>(FMath::Clamp<int64>(
				static_cast<int64>(Unit.Defense) + EnemyState.PhaseDefenseModifier,
				0,
				MAX_int32));
			EnemyState.bPhaseStatModifiersApplied = true;
		}

		InOutRuntime = MoveTemp(NewRuntime);
		return true;
	}

	TArray<FName> CollectLivingUnitIdsForSide(
		const FGameXXKCardBattleRuntime& Runtime,
		const EGameXXKCardTargetSide Side)
	{
		TArray<const FGameXXKCardCombatUnit*> OrderedUnits;
		for (const FGameXXKCardCombatUnit& Unit : Runtime.Units)
		{
			if (Unit.bLiving && Unit.Side == Side)
			{
				OrderedUnits.Add(&Unit);
			}
		}
		OrderedUnits.Sort([](const FGameXXKCardCombatUnit& Left, const FGameXXKCardCombatUnit& Right)
		{
			return IsStableUnitOrderBefore(Left, Right);
		});
		TArray<FName> UnitIds;
		UnitIds.Reserve(OrderedUnits.Num());
		for (const FGameXXKCardCombatUnit* Unit : OrderedUnits)
		{
			UnitIds.Add(Unit->UnitId);
		}
		return UnitIds;
	}

	bool DiscardRemainingHand(
		FGameXXKBattleDeckState& InOutDeck,
		FString& OutError)
	{
		OutError.Reset();
		if (!ValidateDeckStateInternal(InOutDeck, OutError) || !RequireNoPendingChoice(InOutDeck, &OutError))
		{
			return false;
		}
		FGameXXKBattleDeckState NewDeck = InOutDeck;
		for (FGameXXKCardInstance& Instance : NewDeck.Hand)
		{
			NewDeck.DiscardPile.Add(MoveTemp(Instance));
		}
		NewDeck.Hand.Reset();
		if (!MaterializePendingAutomaticHandCards(NewDeck, OutError))
		{
			return false;
		}
		if (!ValidateDeckStateInternal(NewDeck, OutError))
		{
			return false;
		}
		InOutDeck = MoveTemp(NewDeck);
		return true;
	}

	bool DiscardRemainingHandExcept(
		FGameXXKBattleDeckState& InOutDeck,
		const TArray<FName>& RetainedInstanceIds,
		FString& OutError)
	{
		OutError.Reset();
		if (!ValidateDeckStateInternal(InOutDeck, OutError) || !RequireNoPendingChoice(InOutDeck, &OutError))
		{
			return false;
		}
		TSet<FName> RetainedIds;
		RetainedIds.Reserve(RetainedInstanceIds.Num());
		for (const FName InstanceId : RetainedInstanceIds)
		{
			RetainedIds.Add(InstanceId);
		}
		FGameXXKBattleDeckState NewDeck = InOutDeck;
		TArray<FGameXXKCardInstance> RetainedHand;
		RetainedHand.Reserve(NewDeck.Hand.Num());
		for (FGameXXKCardInstance& Instance : NewDeck.Hand)
		{
			if (RetainedIds.Contains(Instance.InstanceId) && !Instance.bTemporary)
			{
				RetainedHand.Add(MoveTemp(Instance));
			}
			else
			{
				NewDeck.DiscardPile.Add(MoveTemp(Instance));
			}
		}
		NewDeck.Hand = MoveTemp(RetainedHand);
		if (!MaterializePendingAutomaticHandCards(NewDeck, OutError))
		{
			return false;
		}
		if (!ValidateDeckStateInternal(NewDeck, OutError))
		{
			return false;
		}
		InOutDeck = MoveTemp(NewDeck);
		return true;
	}

	void ExpireTemporaryCardsAtPlayerRoundEnd(
		FGameXXKBattleDeckState& InOutDeck,
		const int32 PlayerRound)
	{
		TSet<FName> ExpiredIds;
		const auto ExpireZone = [PlayerRound, &ExpiredIds](TArray<FGameXXKCardInstance>& Zone)
		{
			for (int32 Index = Zone.Num() - 1; Index >= 0; --Index)
			{
				if (!Zone[Index].bTemporary || Zone[Index].ExpireAfterPlayerRound > PlayerRound)
				{
					continue;
				}
				ExpiredIds.Add(Zone[Index].InstanceId);
				Zone.RemoveAt(Index, 1, EAllowShrinking::No);
			}
		};
		ExpireZone(InOutDeck.DrawPile);
		ExpireZone(InOutDeck.Hand);
		ExpireZone(InOutDeck.DiscardPile);
		ExpireZone(InOutDeck.ExhaustPile);
		ExpireZone(InOutDeck.PendingAutomaticHandCards);
		InOutDeck.ActiveInstanceIds.RemoveAll([&ExpiredIds](const FName InstanceId)
		{
			return ExpiredIds.Contains(InstanceId);
		});
		FString IgnoredError;
		MaterializePendingAutomaticHandCards(InOutDeck, IgnoredError);
	}

	bool ApplyEndPhaseDotForSide(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const EGameXXKCardTargetSide Side,
		TArray<FGameXXKCardDamageResult>& OutResults,
		FString& OutError)
	{
		OutError.Reset();
		for (const FName UnitId : CollectLivingUnitIdsForSide(InOutRuntime, Side))
		{
			const FGameXXKCardCombatUnit* TargetBeforeDot = FindCombatUnitById(InOutRuntime.Units, UnitId);
			const int32 TargetHealthBefore = TargetBeforeDot ? TargetBeforeDot->HP : 0;
			const int32 TargetArmorBefore = TargetBeforeDot ? TargetBeforeDot->Armor : 0;
			const int32 PoisonStacksBefore = TargetBeforeDot
				? GameXXKCardRules::GetCombatStatusStacks(*TargetBeforeDot, EGameXXKCardStatus::Poison)
				: 0;
			const bool bLifeSavingConsumptionPendingBefore = InOutRuntime.bLifeSavingTalismanConsumptionPending;
			int32 HealthDamage = 0;
			int32 PacketHealthAfter = TargetHealthBefore;
			if (!ApplyCombatEndPhaseDotForRuntime(
				InOutRuntime,
				UnitId,
				HealthDamage,
				PacketHealthAfter,
				&OutError))
			{
				return false;
			}
			const bool bTriggeredLifeSavingTalisman = !bLifeSavingConsumptionPendingBefore
				&& InOutRuntime.bLifeSavingTalismanConsumptionPending;
			if (HealthDamage > 0 || bTriggeredLifeSavingTalisman)
			{
				FGameXXKCardDamageResult& Result = OutResults.AddDefaulted_GetRef();
				Result.OriginalTargetUnitId = UnitId;
				Result.ResolvedTargetUnitId = UnitId;
				Result.Kind = EGameXXKCardDamageKind::DamageOverTime;
				Result.Cause = EGameXXKCardDamageCause::Poison;
				Result.StatusStacksBefore = PoisonStacksBefore;
				Result.RotDamageBonus = 0;
				Result.StatusStacksConsumed = 0;
				Result.RequestedDamage = PoisonStacksBefore;
				Result.DamageAfterDefense = Result.RequestedDamage;
				Result.DamageAfterVulnerability = Result.RequestedDamage;
				Result.DamageBeforeLevelDifference = Result.RequestedDamage;
				Result.DamageAfterLevelDifference = Result.RequestedDamage;
				Result.HealthDamage = HealthDamage;
				Result.TargetHealthBefore = TargetHealthBefore;
				Result.TargetArmorBefore = TargetArmorBefore;
				Result.TargetHealthAfter = PacketHealthAfter;
				Result.TargetArmorAfter = TargetArmorBefore;
				if (const FGameXXKCardCombatUnit* TargetAfterDot = FindCombatUnitById(InOutRuntime.Units, UnitId))
				{
					Result.TargetArmorAfter = TargetAfterDot->Armor;
				}
			}
		}
		return true;
	}

	bool ClearArmorAtSidePhaseStart(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const EGameXXKCardTargetSide Side,
		FString& OutError)
	{
		TSet<FName> RetainedPartyArmorUnitIds;
		if (Side == EGameXXKCardTargetSide::Party)
		{
			for (const FName UnitId : InOutRuntime.RetainArmorAtNextPartyPhaseUnitIds)
			{
				RetainedPartyArmorUnitIds.Add(UnitId);
			}
		}
		for (FGameXXKCardCombatUnit& Unit : InOutRuntime.Units)
		{
			if (!Unit.bLiving || Unit.Side != Side)
			{
				continue;
			}
			if (Side == EGameXXKCardTargetSide::Party && RetainedPartyArmorUnitIds.Contains(Unit.UnitId))
			{
				continue;
			}

			if (Side == EGameXXKCardTargetSide::Enemy && !Unit.EnemyDefinitionId.IsNone())
			{
				const FGameXXKEnemyDefinition* EnemyDefinition = FGameXXKEnemyCatalog::Find(Unit.EnemyDefinitionId);
				if (EnemyDefinition && EnemyDefinition->PassiveId == EGameXXKEnemyPassiveId::BluehornArmorRetention)
				{
					FGameXXKEnemyBattleState& EnemyState = InOutRuntime.EnemyStates.FindOrAdd(Unit.UnitId);
					if (EnemyState.DefinitionId.IsNone())
					{
						EnemyState.DefinitionId = EnemyDefinition->Id;
					}
					if (EnemyState.DefinitionId != EnemyDefinition->Id)
					{
						OutError = TEXT("Enemy armor retention found a mismatched persisted enemy definition.");
						return false;
					}
					Unit.Armor /= 2;
					continue;
				}
			}

			GameXXKCardRules::BeginCombatUnitPhase(Unit);
		}
		if (Side == EGameXXKCardTargetSide::Party)
		{
			InOutRuntime.RetainArmorAtNextPartyPhaseUnitIds.Reset();
		}
		return true;
	}

	bool ResolveEndOfRoundModifiers(
		FGameXXKCardBattleRuntime& InOutRuntime,
		FString& OutError)
	{
		OutError.Reset();
		for (int32 Index = InOutRuntime.Modifiers.Num() - 1; Index >= 0; --Index)
		{
			FGameXXKCardBattleModifierRuntime& Modifier = InOutRuntime.Modifiers[Index];
			FGameXXKCardBattleModifier& Definition = Modifier.Definition;
			if (Definition.Trigger != EGameXXKCardBattleModifierTrigger::EndOfRound)
			{
				continue;
			}
			if (Definition.EffectType != EGameXXKCardEffectType::GainEnergy
				|| Definition.Target != EGameXXKCardEffectTarget::CardOwner
				|| Definition.Magnitude <= 0)
			{
				OutError = TEXT("An end-of-round modifier must grant a positive amount of shared energy to its card owner.");
				return false;
			}
			FGameXXKCardCombatUnit* ConditionTarget = Modifier.OriginalSelectedTargetUnitId.IsNone()
				? nullptr
				: FindCombatUnitById(InOutRuntime.Units, Modifier.OriginalSelectedTargetUnitId);
			for (const FName RecipientUnitId : Modifier.RecipientUnitIds)
			{
				FGameXXKCardCombatUnit* Recipient = FindCombatUnitById(InOutRuntime.Units, RecipientUnitId);
				if (!Recipient || !Recipient->bLiving)
				{
					continue;
				}
				bool bConditionSatisfied = false;
				int32 IgnoredConsumed = 0;
				if (!TryApplyEffectConditionAndConsumption(Definition.Condition, InOutRuntime, *Recipient, ConditionTarget, nullptr, bConditionSatisfied, IgnoredConsumed, OutError))
				{
					return false;
				}
				if (bConditionSatisfied)
				{
					InOutRuntime.PendingNextRoundEnergyBonus = FMath::Min(
						MaxDeferredPhaseEnergy,
						InOutRuntime.PendingNextRoundEnergyBonus + Definition.Magnitude);
				}
			}

			if (Definition.Expiry == EGameXXKCardModifierExpiry::AfterTriggerCount)
			{
				if (Definition.RemainingTriggers <= 0)
				{
					OutError = TEXT("An end-of-round modifier has no remaining trigger count.");
					return false;
				}
				if (--Definition.RemainingTriggers > 0)
				{
					continue;
				}
			}
			InOutRuntime.Modifiers.RemoveAt(Index, 1, EAllowShrinking::No);
		}
		return true;
	}

	void ExpireRoundBoundState(FGameXXKCardBattleRuntime& InOutRuntime)
	{
		for (FGameXXKCardCombatUnit& Unit : InOutRuntime.Units)
		{
			GameXXKCardRules::ConsumeCombatStatus(Unit, EGameXXKCardStatus::TerrainBonusDoubleThisRound, MAX_int32);
		}
		InOutRuntime.Modifiers.RemoveAll([](const FGameXXKCardBattleModifierRuntime& Modifier)
		{
			return Modifier.Definition.Trigger == EGameXXKCardBattleModifierTrigger::FirstDirectDamageReceivedThisRound
				|| Modifier.Definition.Expiry == EGameXXKCardModifierExpiry::EndOfCurrentRound
				|| Modifier.Definition.Expiry == EGameXXKCardModifierExpiry::EndOfCurrentRoundOrTriggerCount;
		});
	}

	bool ApplySingleTargetEnemyRedirect(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FGameXXKCardDamageContext& Context,
		const FName SelectedPartyTargetUnitId,
		FName& OutAppliedTargetUnitId,
		bool& bOutRedirected,
		FString& OutError)
	{
		OutError.Reset();
		OutAppliedTargetUnitId = SelectedPartyTargetUnitId;
		bOutRedirected = false;
		if (Context.Kind != EGameXXKCardDamageKind::SingleTargetAttack)
		{
			return true;
		}
		for (const FName CandidateUnitId : CollectLivingUnitIdsForSide(InOutRuntime, EGameXXKCardTargetSide::Party))
		{
			if (CandidateUnitId == SelectedPartyTargetUnitId)
			{
				continue;
			}
			FGameXXKCardCombatUnit* Candidate = FindCombatUnitById(InOutRuntime.Units, CandidateUnitId);
			if (Candidate && GameXXKCardRules::GetCombatStatusStacks(*Candidate, EGameXXKCardStatus::RedirectSingleTargetEnemyAttack) > 0)
			{
				if (GameXXKCardRules::ConsumeCombatStatus(*Candidate, EGameXXKCardStatus::RedirectSingleTargetEnemyAttack, 1) != 1)
				{
					OutError = TEXT("A single-target enemy-attack redirect changed before it could commit.");
					return false;
				}
				OutAppliedTargetUnitId = CandidateUnitId;
				bOutRedirected = true;
				return true;
			}
		}
		return true;
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
	const int32 RequiredDiscardCount,
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
	if (RequiredDiscardCount < 0)
	{
		return SetFailure(OutError, TEXT("Required discard count cannot be negative."));
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
	int32 RemainingToDraw = Count;
	while (RemainingToDraw > 0
		&& NewDeck.Hand.Num() < BattleHandCapacity
		&& EnsureDrawPileHasCard(NewDeck))
	{
		NewDeck.Hand.Add(MoveTemp(NewDeck.DrawPile.Last()));
		NewDeck.DrawPile.Pop(EAllowShrinking::No);
		--RemainingToDraw;
	}

	if (RemainingToDraw > 0 && NewDeck.Hand.Num() >= BattleHandCapacity)
	{
		// No card is consumed by the overflow. If the draw pile was just exhausted, recycle the
		// discard pile once; then shuffle the complete remaining draw pile deterministically.
		if (NewDeck.DrawPile.IsEmpty())
		{
			MoveRecyclableDiscardToDraw(NewDeck);
		}
		ShufflePile(NewDeck.DrawPile, NewDeck.CurrentRandomState);
	}

	if (RequiredDiscardCount > 0)
	{
		if (RequiredDiscardCount > NewDeck.Hand.Num())
		{
			return SetFailure(OutError, TEXT("Required discard count exceeds the cards available in hand."));
		}
		ClearPendingChoice(NewDeck.PendingChoice);
		NewDeck.PendingChoice.Kind = EGameXXKCardPendingChoiceKind::ForcedDiscard;
		NewDeck.PendingChoice.Candidates = NewDeck.Hand;
		NewDeck.PendingChoice.RequiredCount = RequiredDiscardCount;
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

void GameXXKCardRules::RemoveDefeatedPartyOwnerCards(
	FGameXXKBattleDeckState& InOutDeck,
	const TArray<FGameXXKCardCombatUnit>& Units)
{
	TSet<FName> LivingPartyOwnerIds;
	for (const FGameXXKCardCombatUnit& Unit : Units)
	{
		if (Unit.bLiving && Unit.Side == EGameXXKCardTargetSide::Party)
		{
			LivingPartyOwnerIds.Add(Unit.UnitId);
		}
	}

	const auto IsDefeatedOwner = [&LivingPartyOwnerIds](const FGameXXKCardInstance& Instance)
	{
		return !LivingPartyOwnerIds.Contains(Instance.OwnerUnitId);
	};
	InOutDeck.DrawPile.RemoveAll(IsDefeatedOwner);
	InOutDeck.DiscardPile.RemoveAll(IsDefeatedOwner);
	InOutDeck.Hand.RemoveAll(IsDefeatedOwner);
	InOutDeck.ExhaustPile.RemoveAll(IsDefeatedOwner);
	InOutDeck.PendingAutomaticHandCards.RemoveAll(IsDefeatedOwner);
	FString IgnoredError;
	MaterializePendingAutomaticHandCards(InOutDeck, IgnoredError);

	InOutDeck.ActiveInstanceIds.Reset();
	const auto RebuildLedger = [&InOutDeck](const TArray<FGameXXKCardInstance>& Zone)
	{
		for (const FGameXXKCardInstance& Instance : Zone)
		{
			InOutDeck.ActiveInstanceIds.Add(Instance.InstanceId);
		}
	};
	RebuildLedger(InOutDeck.DrawPile);
	RebuildLedger(InOutDeck.DiscardPile);
	RebuildLedger(InOutDeck.Hand);
	RebuildLedger(InOutDeck.ExhaustPile);
	RebuildLedger(InOutDeck.PendingAutomaticHandCards);
}

bool GameXXKCardRules::MoveHandCardToDiscard(
	FGameXXKBattleDeckState& InOutDeck,
	const FName InstanceId,
	FString* OutError)
{
	return MoveResolvedHandCard(InOutDeck, InstanceId, false, OutError);
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
	if (!MaterializePendingAutomaticHandCards(NewDeck, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}

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
	if (InOutDeck.Hand.Num() >= BattleHandCapacity)
	{
		return SetFailure(OutError, TEXT("Insight requires an open slot below the twenty-card battle capacity."));
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
	if (const FGameXXKCardInstance* Instance = FindInZone(Deck.ExhaustPile, InstanceId))
	{
		OutZone = EGameXXKCardZone::ExhaustPile;
		return Instance;
	}
	if (const FGameXXKCardInstance* Instance = FindInZone(Deck.PendingAutomaticHandCards, InstanceId))
	{
		OutZone = EGameXXKCardZone::PendingAutomaticHand;
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

bool GameXXKCardRules::BuildTargetRequest(
	const FGameXXKCardDefinition& Definition,
	const FGameXXKCardInstance& CardInstance,
	const EGameXXKCardTerrain Terrain,
	const TArray<FGameXXKCardTargetUnit>& TargetUnits,
	FGameXXKCardTargetRequest& OutRequest,
	FString* OutError)
{
	if (OutError)
	{
		OutError->Reset();
	}
	if (!IsValidInstance(CardInstance) || Definition.Id.IsNone() || Definition.Id != CardInstance.CardId)
	{
		return SetFailure(OutError, TEXT("Card instance and immutable definition do not match."));
	}

	FString ValidationError;
	TArray<const FGameXXKCardTargetUnit*> SortedUnits;
	if (!ValidateAndSortTargetUnits(TargetUnits, SortedUnits, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	const FGameXXKCardTargetUnit* SourceUnit = FindTargetUnitById(SortedUnits, CardInstance.OwnerUnitId);
	if (!SourceUnit || !SourceUnit->bLiving)
	{
		return SetFailure(OutError, TEXT("Card owner is absent from the current living battle-unit set."));
	}
	if (!ValidateTargetSpec(Definition.TargetSpec, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}

	EGameXXKCardTargetMode EffectiveMode = EGameXXKCardTargetMode::Invalid;
	EGameXXKCardTargetPresentation EffectivePresentation = EGameXXKCardTargetPresentation::Invalid;
	if (!ResolveEffectiveTargetMode(Definition, *SourceUnit, Terrain, EffectiveMode, EffectivePresentation, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	if (EffectivePresentation != PresentationForTargetMode(EffectiveMode))
	{
		return SetFailure(OutError, TEXT("Target mode and presentation do not form a valid interaction contract."));
	}
	if (!IsManualTargetMode(EffectiveMode) && !IsAutomaticTargetMode(EffectiveMode))
	{
		return SetFailure(OutError, TEXT("Card definition resolves to an unsupported target mode."));
	}

	FGameXXKCardTargetRequest NewRequest;
	NewRequest.CardInstanceId = CardInstance.InstanceId;
	NewRequest.SourceUnitId = SourceUnit->UnitId;
	NewRequest.EffectiveMode = EffectiveMode;
	NewRequest.Presentation = EffectivePresentation;
	NewRequest.bRequiresManualSelection = IsManualTargetMode(EffectiveMode);
	NewRequest.bRequiresRandomResolution = EffectiveMode == EGameXXKCardTargetMode::RandomEnemy;

	if (EffectiveMode == EGameXXKCardTargetMode::None)
	{
		OutRequest = MoveTemp(NewRequest);
		return true;
	}

	TArray<const FGameXXKCardTargetUnit*> LegalUnits;
	for (const FGameXXKCardTargetUnit* Unit : SortedUnits)
	{
		check(Unit);
		FGameXXKCardTargetCandidateView Candidate;
		Candidate.UnitId = Unit->UnitId;
		Candidate.Side = Unit->Side;
		Candidate.DisabledReason = EvaluateTargetCandidate(Definition.TargetSpec, EffectiveMode, Terrain, SourceUnit->UnitId, *Unit);
		Candidate.bCanSelect = Candidate.DisabledReason == EGameXXKCardTargetDisabledReason::None;
		if (Candidate.bCanSelect)
		{
			LegalUnits.Add(Unit);
		}
		NewRequest.CandidateViews.Add(Candidate);
	}

	if (LegalUnits.IsEmpty())
	{
		NewRequest.FailureReason = TEXT("No legal target is available for this card.");
		OutRequest = MoveTemp(NewRequest);
		return SetFailure(OutError, OutRequest.FailureReason);
	}

	switch (EffectiveMode)
	{
	case EGameXXKCardTargetMode::Self:
		NewRequest.AutomaticTargetUnitIds.Add(SourceUnit->UnitId);
		break;
	case EGameXXKCardTargetMode::AllEnemies:
	case EGameXXKCardTargetMode::AllAllies:
	case EGameXXKCardTargetMode::AllOtherAllies:
		for (const FGameXXKCardTargetUnit* Unit : LegalUnits)
		{
			NewRequest.AutomaticTargetUnitIds.Add(Unit->UnitId);
		}
		break;
	case EGameXXKCardTargetMode::LowestHealthAlly:
	case EGameXXKCardTargetMode::LowestHealthOtherAlly:
	{
		const FGameXXKCardTargetUnit* BestUnit = LegalUnits[0];
		for (const FGameXXKCardTargetUnit* Unit : LegalUnits)
		{
			if (IsLowerHealthCandidate(*Unit, *BestUnit))
			{
				BestUnit = Unit;
			}
		}
		NewRequest.AutomaticTargetUnitIds.Add(BestUnit->UnitId);
		break;
	}
	case EGameXXKCardTargetMode::RandomEnemy:
		// The preview exposes candidates but cannot consume the battle PRNG.
		break;
	case EGameXXKCardTargetMode::SingleEnemy:
	case EGameXXKCardTargetMode::SingleAlly:
	case EGameXXKCardTargetMode::OtherAlly:
	case EGameXXKCardTargetMode::AnyLivingUnit:
		break;
	case EGameXXKCardTargetMode::None:
	case EGameXXKCardTargetMode::Invalid:
	default:
		return SetFailure(OutError, TEXT("Card definition resolves to an unsupported target mode."));
	}

	if (!NewRequest.bRequiresManualSelection && !NewRequest.bRequiresRandomResolution)
	{
		TSet<FName> LockedIds(NewRequest.AutomaticTargetUnitIds);
		for (FGameXXKCardTargetCandidateView& Candidate : NewRequest.CandidateViews)
		{
			Candidate.bAutoLocked = Candidate.bCanSelect && LockedIds.Contains(Candidate.UnitId);
		}
	}

	OutRequest = MoveTemp(NewRequest);
	return true;
}

bool GameXXKCardRules::ResolveAutomaticTargetIds(
	const FGameXXKCardTargetRequest& Request,
	const TArray<FGameXXKCardTargetUnit>& TargetUnits,
	int32& InOutRandomState,
	TArray<FName>& OutTargetIds,
	FString* OutError)
{
	if (OutError)
	{
		OutError->Reset();
	}
	if (Request.CardInstanceId.IsNone() || Request.SourceUnitId.IsNone() || !Request.FailureReason.IsEmpty())
	{
		return SetFailure(OutError, TEXT("Automatic target request is incomplete or already failed."));
	}
	if (!IsAutomaticTargetMode(Request.EffectiveMode) || IsManualTargetMode(Request.EffectiveMode)
		|| Request.bRequiresManualSelection
		|| Request.Presentation != PresentationForTargetMode(Request.EffectiveMode)
		|| Request.bRequiresRandomResolution != (Request.EffectiveMode == EGameXXKCardTargetMode::RandomEnemy))
	{
		return SetFailure(OutError, TEXT("Automatic target request has incompatible mode metadata."));
	}

	FString ValidationError;
	TArray<const FGameXXKCardTargetUnit*> SortedUnits;
	if (!ValidateAndSortTargetUnits(TargetUnits, SortedUnits, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	const FGameXXKCardTargetUnit* SourceUnit = FindTargetUnitById(SortedUnits, Request.SourceUnitId);
	if (!SourceUnit || !SourceUnit->bLiving)
	{
		return SetFailure(OutError, TEXT("Automatic target request source is absent or defeated."));
	}
	if (!ValidateRequestCandidateViews(Request, SortedUnits, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}

	if (Request.EffectiveMode == EGameXXKCardTargetMode::None)
	{
		TArray<FName> NewTargetIds;
		OutTargetIds = MoveTemp(NewTargetIds);
		return true;
	}

	TMap<FName, const FGameXXKCardTargetCandidateView*> CandidateById;
	for (const FGameXXKCardTargetCandidateView& Candidate : Request.CandidateViews)
	{
		CandidateById.Add(Candidate.UnitId, &Candidate);
	}
	TArray<const FGameXXKCardTargetUnit*> LegalUnits;
	for (const FGameXXKCardTargetUnit* Unit : SortedUnits)
	{
		const FGameXXKCardTargetCandidateView* Candidate = CandidateById.FindRef(Unit->UnitId);
		if (Candidate && Candidate->bCanSelect)
		{
			if (!Unit->bLiving)
			{
				return SetFailure(OutError, TEXT("Automatic target request became stale because a legal unit is no longer living."));
			}
			LegalUnits.Add(Unit);
		}
	}

	if (Request.EffectiveMode == EGameXXKCardTargetMode::RandomEnemy)
	{
		if (!Request.AutomaticTargetUnitIds.IsEmpty() || LegalUnits.IsEmpty())
		{
			return SetFailure(OutError, TEXT("Random target request contains invalid automatic target metadata."));
		}
		for (const FGameXXKCardTargetUnit* Unit : LegalUnits)
		{
			if (Unit->Side != EGameXXKCardTargetSide::Enemy)
			{
				return SetFailure(OutError, TEXT("Random enemy request contains a non-enemy legal candidate."));
			}
		}
		int32 NewRandomState = InOutRandomState;
		TArray<FName> NewTargetIds;
		NewTargetIds.Add(LegalUnits[NextRandomIndex(NewRandomState, LegalUnits.Num())]->UnitId);
		InOutRandomState = NewRandomState;
		OutTargetIds = MoveTemp(NewTargetIds);
		return true;
	}

	TSet<FName> SeenAutomaticIds;
	for (const FName UnitId : Request.AutomaticTargetUnitIds)
	{
		const FGameXXKCardTargetCandidateView* Candidate = CandidateById.FindRef(UnitId);
		if (UnitId.IsNone() || SeenAutomaticIds.Contains(UnitId) || !Candidate || !Candidate->bCanSelect || !Candidate->bAutoLocked)
		{
			return SetFailure(OutError, TEXT("Automatic target request contains an invalid locked target ID."));
		}
		SeenAutomaticIds.Add(UnitId);
	}

	TArray<FName> ExpectedIds;
	switch (Request.EffectiveMode)
	{
	case EGameXXKCardTargetMode::Self:
		ExpectedIds.Add(Request.SourceUnitId);
		break;
	case EGameXXKCardTargetMode::AllEnemies:
	case EGameXXKCardTargetMode::AllAllies:
	case EGameXXKCardTargetMode::AllOtherAllies:
		for (const FGameXXKCardTargetUnit* Unit : LegalUnits)
		{
			ExpectedIds.Add(Unit->UnitId);
		}
		break;
	case EGameXXKCardTargetMode::LowestHealthAlly:
	case EGameXXKCardTargetMode::LowestHealthOtherAlly:
		if (LegalUnits.IsEmpty())
		{
			return SetFailure(OutError, TEXT("Automatic lowest-health target request has no legal candidates."));
		}
		{
			const FGameXXKCardTargetUnit* BestUnit = LegalUnits[0];
			for (const FGameXXKCardTargetUnit* Unit : LegalUnits)
			{
				if (IsLowerHealthCandidate(*Unit, *BestUnit))
				{
					BestUnit = Unit;
				}
			}
			ExpectedIds.Add(BestUnit->UnitId);
		}
		break;
	case EGameXXKCardTargetMode::None:
	case EGameXXKCardTargetMode::RandomEnemy:
	case EGameXXKCardTargetMode::Invalid:
	case EGameXXKCardTargetMode::SingleEnemy:
	case EGameXXKCardTargetMode::SingleAlly:
	case EGameXXKCardTargetMode::OtherAlly:
	case EGameXXKCardTargetMode::AnyLivingUnit:
	default:
		return SetFailure(OutError, TEXT("Automatic target request has an unsupported target mode."));
	}
	if (Request.AutomaticTargetUnitIds != ExpectedIds)
	{
		return SetFailure(OutError, TEXT("Automatic target request no longer matches the stable legal-target order."));
	}

	TArray<FName> NewTargetIds = ExpectedIds;
	OutTargetIds = MoveTemp(NewTargetIds);
	return true;
}

bool GameXXKCardRules::IsManualTargetLegal(const FGameXXKCardTargetRequest& Request, const FName UnitId)
{
	if (!Request.bRequiresManualSelection || UnitId.IsNone())
	{
		return false;
	}
	bool bFound = false;
	for (const FGameXXKCardTargetCandidateView& Candidate : Request.CandidateViews)
	{
		if (Candidate.UnitId == UnitId)
		{
			if (bFound)
			{
				return false;
			}
			bFound = true;
			if (!Candidate.bCanSelect || Candidate.bAutoLocked)
			{
				return false;
			}
		}
	}
	return bFound;
}

namespace
{
	constexpr int32 WhiteApeStatusGuardArmor = 8;
	constexpr uint32 CombatRandomMultiplier = 196314165u;
	constexpr uint32 CombatRandomIncrement = 907633515u;
	constexpr uint32 CombatRandomSalt = 0xA341316Cu;

	int32 ApplyAndRecordHealing(
		FGameXXKCardPlayResult& Result,
		const FName SourceUnitId,
		FGameXXKCardCombatUnit& Target,
		const int32 RequestedHealing)
	{
		FGameXXKCardHealingResult& HealingResult = Result.HealingResults.AddDefaulted_GetRef();
		HealingResult.SourceUnitId = SourceUnitId;
		HealingResult.TargetUnitId = Target.UnitId;
		HealingResult.RequestedHealing = FMath::Max(0, RequestedHealing);
		HealingResult.EffectiveHealing = GameXXKCardRules::HealCombatUnit(Target, HealingResult.RequestedHealing);
		return HealingResult.EffectiveHealing;
	}

	int32 ApplyAndRecordArmor(
		FGameXXKCardPlayResult& Result,
		const FName SourceUnitId,
		FGameXXKCardCombatUnit& Target,
		const int32 RequestedArmor)
	{
		FGameXXKCardArmorResult& ArmorResult = Result.ArmorResults.AddDefaulted_GetRef();
		ArmorResult.SourceUnitId = SourceUnitId;
		ArmorResult.TargetUnitId = Target.UnitId;
		ArmorResult.RequestedArmor = FMath::Max(0, RequestedArmor);
		ArmorResult.EffectiveArmor = GameXXKCardRules::AddCombatArmor(Target, ArmorResult.RequestedArmor);
		return ArmorResult.EffectiveArmor;
	}

	const FGameXXKEnemyDefinition* FindWhiteApeStatusGuardDefinition(const FGameXXKCardCombatUnit& Unit)
	{
		if (!Unit.bLiving || Unit.Side != EGameXXKCardTargetSide::Enemy || Unit.EnemyDefinitionId.IsNone())
		{
			return nullptr;
		}
		const FGameXXKEnemyDefinition* EnemyDefinition = FGameXXKEnemyCatalog::Find(Unit.EnemyDefinitionId);
		return EnemyDefinition && EnemyDefinition->PassiveId == EGameXXKEnemyPassiveId::WhiteApeStatusGuard
			? EnemyDefinition
			: nullptr;
	}

	bool ResolveWhiteApeStatusGuardAfterStatusAppliedInternal(
		FGameXXKCardBattleRuntime& InOutRuntime,
		FGameXXKCardCombatUnit& InOutStatusTarget,
		const FName SourceUnitId,
		FGameXXKCardPlayResult* InOutResult,
		FString* OutError)
	{
		const FGameXXKEnemyDefinition* EnemyDefinition = FindWhiteApeStatusGuardDefinition(InOutStatusTarget);
		if (!EnemyDefinition)
		{
			return true;
		}

		FGameXXKEnemyBattleState NewEnemyState;
		if (const FGameXXKEnemyBattleState* ExistingEnemyState = InOutRuntime.EnemyStates.Find(InOutStatusTarget.UnitId))
		{
			NewEnemyState = *ExistingEnemyState;
		}
		if (NewEnemyState.DefinitionId.IsNone())
		{
			NewEnemyState.DefinitionId = EnemyDefinition->Id;
		}
		if (NewEnemyState.DefinitionId != EnemyDefinition->Id)
		{
			return SetFailure(OutError, TEXT("White Ape status guard found a mismatched persisted enemy definition."));
		}
		if (NewEnemyState.bFirstStatusPassiveAvailable)
		{
			if (InOutResult && !SourceUnitId.IsNone())
			{
				ApplyAndRecordArmor(*InOutResult, SourceUnitId, InOutStatusTarget, WhiteApeStatusGuardArmor);
			}
			else
			{
				GameXXKCardRules::AddCombatArmor(InOutStatusTarget, WhiteApeStatusGuardArmor);
			}
			NewEnemyState.bFirstStatusPassiveAvailable = false;
		}
		InOutRuntime.EnemyStates.Add(InOutStatusTarget.UnitId, MoveTemp(NewEnemyState));
		return true;
	}

	bool IsConcreteCombatStatus(const EGameXXKCardStatus Status)
	{
		return Status != EGameXXKCardStatus::Invalid
			&& Status != EGameXXKCardStatus::None
			&& Status != EGameXXKCardStatus::Guard;
	}

	bool IsDotReservoirStatus(const EGameXXKCardStatus Status)
	{
		return Status == EGameXXKCardStatus::Bleed
			|| Status == EGameXXKCardStatus::Poison
			|| Status == EGameXXKCardStatus::Burn
			|| Status == EGameXXKCardStatus::DamageOverTime;
	}

	int32 GetCombatStatusCap(const EGameXXKCardStatus Status)
	{
		switch (Status)
		{
		case EGameXXKCardStatus::Momentum:
		case EGameXXKCardStatus::Bleed:
		case EGameXXKCardStatus::Poison:
		case EGameXXKCardStatus::Burn:
		case EGameXXKCardStatus::DamageOverTime:
		case EGameXXKCardStatus::Agility:
		case EGameXXKCardStatus::Medicine:
		case EGameXXKCardStatus::Charge:
			return MAX_int32;
		case EGameXXKCardStatus::Vulnerability:
		case EGameXXKCardStatus::Mark:
			return 5;
		case EGameXXKCardStatus::Wealth:
		case EGameXXKCardStatus::Counter:
		case EGameXXKCardStatus::Block:
			return 8;
		case EGameXXKCardStatus::Rage:
			return 5;
		case EGameXXKCardStatus::Weak:
			return 5;
		case EGameXXKCardStatus::CannotReceiveVulnerability:
		case EGameXXKCardStatus::NextAttackBonus:
		case EGameXXKCardStatus::NextAttackAppliesVulnerability:
		case EGameXXKCardStatus::TerrainBonusDouble:
		case EGameXXKCardStatus::TerrainBonusDoubleThisRound:
		case EGameXXKCardStatus::NextTerrainCardFree:
		case EGameXXKCardStatus::NextTerrainCardEnergyReduction:
		case EGameXXKCardStatus::Prey:
			return 1;
		case EGameXXKCardStatus::RedirectSingleTargetEnemyAttack:
			return 8;
		case EGameXXKCardStatus::NextHealingBonus:
			return 99;
		case EGameXXKCardStatus::Invalid:
		case EGameXXKCardStatus::None:
		case EGameXXKCardStatus::Guard:
		default:
			return 0;
		}
	}

	int32 GetCombatStatusStacksInternal(const FGameXXKCardCombatUnit& Unit, const EGameXXKCardStatus Status)
	{
		int64 Total = 0;
		for (const FGameXXKCardStatusStack& Stack : Unit.Statuses)
		{
			if (Stack.Status == Status && Stack.Stacks > 0)
			{
				Total = FMath::Min<int64>(MAX_int32, Total + static_cast<int64>(Stack.Stacks));
			}
		}
		return static_cast<int32>(Total);
	}

	void RemoveEmptyCombatStatusEntries(FGameXXKCardCombatUnit& InOutUnit)
	{
		for (int32 Index = InOutUnit.Statuses.Num() - 1; Index >= 0; --Index)
		{
			if (InOutUnit.Statuses[Index].Stacks <= 0)
			{
				InOutUnit.Statuses.RemoveAt(Index, 1, EAllowShrinking::No);
			}
		}
	}

	FGameXXKCardCombatUnit* FindCombatUnitById(TArray<FGameXXKCardCombatUnit>& Units, const FName UnitId)
	{
		return Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == UnitId;
		});
	}

	const FGameXXKCardCombatUnit* FindCombatUnitById(const TArray<FGameXXKCardCombatUnit>& Units, const FName UnitId)
	{
		return Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == UnitId;
		});
	}

	bool IsPartyReactionStatus(const EGameXXKCardStatus Status)
	{
		return Status == EGameXXKCardStatus::Counter || Status == EGameXXKCardStatus::Block;
	}

	FString BuildPartyReactionTriggerGroupKey(const FGameXXKReactionRuntime& Reaction)
	{
		if (Reaction.RegistrationBatchOrdinal != INDEX_NONE)
		{
			return FString::Printf(TEXT("Batch|%d"), Reaction.RegistrationBatchOrdinal);
		}
		// Additive save compatibility: reaction records saved before registration batches
		// existed are grouped by their complete logical source instead of being rejected.
		return FString::Printf(
			TEXT("Legacy|%s|%d|%s|%s"),
			*Reaction.RecipientUnitId.ToString(),
			static_cast<int32>(Reaction.Status),
			*Reaction.GrantedByUnitId.ToString(),
			*Reaction.SourceCardInstanceId.ToString());
	}

	int32 AdvanceCombatRandomRoll(FGameXXKCardBattleRuntime& InOutRuntime)
	{
		const uint32 NextState = static_cast<uint32>(InOutRuntime.CombatRandomState)
			* CombatRandomMultiplier + CombatRandomIncrement;
		InOutRuntime.CombatRandomState = static_cast<int32>(NextState);
		return static_cast<int32>(NextState % 100u);
	}

	int32 CountReactionUses(
		const FGameXXKCardBattleRuntime& Runtime,
		const FName RecipientUnitId,
		const EGameXXKCardStatus Status)
	{
		int64 Total = 0;
		for (const FGameXXKReactionRuntime& Reaction : Runtime.Reactions)
		{
			if (Reaction.RecipientUnitId == RecipientUnitId && Reaction.Status == Status && Reaction.RemainingTriggers > 0)
			{
				Total = FMath::Min<int64>(MAX_int32, Total + Reaction.RemainingTriggers);
			}
		}
		return static_cast<int32>(Total);
	}

	void SetCombatStatusStacksInternal(
		FGameXXKCardCombatUnit& InOutUnit,
		const EGameXXKCardStatus Status,
		const int32 Stacks)
	{
		GameXXKCardRules::ConsumeCombatStatus(InOutUnit, Status, MAX_int32);
		if (Stacks > 0)
		{
			FGameXXKCardStatusStack& NewStack = InOutUnit.Statuses.AddDefaulted_GetRef();
			NewStack.Status = Status;
			NewStack.Stacks = Stacks;
		}
	}

	void SyncPartyReactionStatuses(FGameXXKCardBattleRuntime& InOutRuntime)
	{
		for (FGameXXKCardCombatUnit& Unit : InOutRuntime.Units)
		{
			if (Unit.Side != EGameXXKCardTargetSide::Party)
			{
				continue;
			}
			SetCombatStatusStacksInternal(
				Unit,
				EGameXXKCardStatus::Counter,
				CountReactionUses(InOutRuntime, Unit.UnitId, EGameXXKCardStatus::Counter));
			SetCombatStatusStacksInternal(
				Unit,
				EGameXXKCardStatus::Block,
				CountReactionUses(InOutRuntime, Unit.UnitId, EGameXXKCardStatus::Block));
		}
	}

	void RemoveDefeatedPartyReactions(FGameXXKCardBattleRuntime& InOutRuntime)
	{
		InOutRuntime.Reactions.RemoveAll([&InOutRuntime](const FGameXXKReactionRuntime& Reaction)
		{
			const FGameXXKCardCombatUnit* Recipient = FindCombatUnitById(InOutRuntime.Units, Reaction.RecipientUnitId);
			return !Recipient || Recipient->Side != EGameXXKCardTargetSide::Party || !Recipient->bLiving;
		});
		SyncPartyReactionStatuses(InOutRuntime);
	}

	void ExpirePartyReactionsForPlayerRound(FGameXXKCardBattleRuntime& InOutRuntime)
	{
		InOutRuntime.Reactions.RemoveAll([&InOutRuntime](const FGameXXKReactionRuntime& Reaction)
		{
			return Reaction.RemainingTriggers <= 0
				|| Reaction.ExpireBeforePlayerRound <= InOutRuntime.RoundNumber;
		});
		RemoveDefeatedPartyReactions(InOutRuntime);
	}

	bool ValidatePartyReactions(const FGameXXKCardBattleRuntime& Runtime, FString& OutError)
	{
		TSet<FName> ReactionIds;
		TMap<FString, int32> ExpectedStatusStacks;
		TMap<int32, FString> RegistrationBatchSignatures;
		for (const FGameXXKReactionRuntime& Reaction : Runtime.Reactions)
		{
			const FGameXXKCardCombatUnit* Recipient = FindCombatUnitById(Runtime.Units, Reaction.RecipientUnitId);
			const FGameXXKCardCombatUnit* Grantor = FindCombatUnitById(Runtime.Units, Reaction.GrantedByUnitId);
			if (Reaction.ReactionId.IsNone() || ReactionIds.Contains(Reaction.ReactionId)
				|| !IsPartyReactionStatus(Reaction.Status)
				|| !Recipient || Recipient->Side != EGameXXKCardTargetSide::Party
				|| !Grantor || Grantor->Side != EGameXXKCardTargetSide::Party
				|| Reaction.SourceCardInstanceId.IsNone()
				|| Reaction.RegistrationBatchOrdinal < INDEX_NONE
				|| Reaction.RegistrationBatchOrdinal >= Runtime.NextReactionOrdinal
				|| Reaction.RemainingTriggers != 1
				|| Reaction.ExpireBeforePlayerRound <= Runtime.RoundNumber)
			{
				OutError = TEXT("Card battle runtime contains an invalid independently consumable party reaction.");
				return false;
			}
			ReactionIds.Add(Reaction.ReactionId);
			if (Reaction.RegistrationBatchOrdinal != INDEX_NONE)
			{
				const FString BatchSignature = FString::Printf(
					TEXT("%s|%d|%s|%s|%d"),
					*Reaction.RecipientUnitId.ToString(),
					static_cast<int32>(Reaction.Status),
					*Reaction.GrantedByUnitId.ToString(),
					*Reaction.SourceCardInstanceId.ToString(),
					Reaction.ExpireBeforePlayerRound);
				const FString* ExistingSignature = RegistrationBatchSignatures.Find(Reaction.RegistrationBatchOrdinal);
				if (ExistingSignature && *ExistingSignature != BatchSignature)
				{
					OutError = TEXT("One party-reaction registration batch contains mismatched source records.");
					return false;
				}
				RegistrationBatchSignatures.FindOrAdd(Reaction.RegistrationBatchOrdinal) = BatchSignature;
			}
			const FString Key = Reaction.RecipientUnitId.ToString() + TEXT("|") + FString::FromInt(static_cast<int32>(Reaction.Status));
			const int32 NewCount = ExpectedStatusStacks.FindRef(Key) + 1;
			if (NewCount > GetCombatStatusCap(Reaction.Status))
			{
				OutError = TEXT("Party reaction records exceed the visible combat-status capacity.");
				return false;
			}
			ExpectedStatusStacks.Add(Key, NewCount);
		}

		for (const FGameXXKCardCombatUnit& Unit : Runtime.Units)
		{
			if (Unit.Side != EGameXXKCardTargetSide::Party)
			{
				continue;
			}
			for (const EGameXXKCardStatus Status : {EGameXXKCardStatus::Counter, EGameXXKCardStatus::Block})
			{
				const FString Key = Unit.UnitId.ToString() + TEXT("|") + FString::FromInt(static_cast<int32>(Status));
				if (GetCombatStatusStacksInternal(Unit, Status) != ExpectedStatusStacks.FindRef(Key))
				{
					OutError = TEXT("Visible party Counter and Block stacks must exactly mirror reaction records.");
					return false;
				}
			}
		}
		return true;
	}

	bool RegisterPartyReactionUses(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FGameXXKCardInstance& Instance,
		const FName RecipientUnitId,
		const EGameXXKCardStatus Status,
		const int32 Uses,
		FString& OutError)
	{
		FGameXXKCardCombatUnit* Recipient = FindCombatUnitById(InOutRuntime.Units, RecipientUnitId);
		const FGameXXKCardCombatUnit* Grantor = FindCombatUnitById(InOutRuntime.Units, Instance.OwnerUnitId);
		if (!Recipient || !Recipient->bLiving || Recipient->Side != EGameXXKCardTargetSide::Party
			|| !Grantor || !Grantor->bLiving || Grantor->Side != EGameXXKCardTargetSide::Party
			|| Instance.InstanceId.IsNone() || !IsPartyReactionStatus(Status) || Uses <= 0
			|| Uses > GetCombatStatusCap(Status) - CountReactionUses(InOutRuntime, RecipientUnitId, Status))
		{
			OutError = TEXT("A party reaction requires living party records, a stable source card, and available reaction capacity.");
			return false;
		}
		if (InOutRuntime.RoundNumber == MAX_int32 || InOutRuntime.NextReactionOrdinal > MAX_int32 - Uses)
		{
			OutError = TEXT("Party reaction identity or expiry counters have exhausted their supported range.");
			return false;
		}

		const int32 RegistrationBatchOrdinal = InOutRuntime.NextReactionOrdinal;
		for (int32 UseIndex = 0; UseIndex < Uses; ++UseIndex)
		{
			const int32 Ordinal = InOutRuntime.NextReactionOrdinal++;
			FGameXXKReactionRuntime& Reaction = InOutRuntime.Reactions.AddDefaulted_GetRef();
			Reaction.ReactionId = FName(*FString::Printf(TEXT("Reaction.%d"), Ordinal));
			Reaction.Status = Status;
			Reaction.RecipientUnitId = RecipientUnitId;
			Reaction.GrantedByUnitId = Instance.OwnerUnitId;
			Reaction.SourceCardInstanceId = Instance.InstanceId;
			Reaction.RegistrationBatchOrdinal = RegistrationBatchOrdinal;
			Reaction.RemainingTriggers = 1;
			Reaction.ExpireBeforePlayerRound = InOutRuntime.RoundNumber + 1;
		}
		SyncPartyReactionStatuses(InOutRuntime);
		return true;
	}

	bool ValidateCombatUnits(const TArray<FGameXXKCardCombatUnit>& Units, FString& OutError)
	{
		TSet<FName> SeenUnitIds;
		TSet<int32> SeenExplicitEnemySlots;
		for (const FGameXXKCardCombatUnit& Unit : Units)
		{
			if (Unit.UnitId.IsNone() || SeenUnitIds.Contains(Unit.UnitId)
				|| !IsConcreteTargetSide(Unit.Side)
				|| Unit.MaxHP <= 0 || Unit.HP < 0 || Unit.HP > Unit.MaxHP
				|| Unit.MaxMana < 0 || Unit.Mana < 0 || Unit.Mana > Unit.MaxMana
				|| Unit.Attack < 0 || Unit.Defense < 0 || Unit.Speed < 1
				|| Unit.Armor < 0
				|| Unit.StableSortOrder == INDEX_NONE || Unit.StableSortOrder < 0
				|| Unit.bLiving != (Unit.HP > 0))
			{
				OutError = TEXT("Combat units must have unique stable IDs and internally consistent combat values.");
				return false;
			}
			if (Unit.Side == EGameXXKCardTargetSide::Enemy && Unit.BattleSlotNumber != INDEX_NONE)
			{
				if (Unit.BattleSlotNumber < 1 || Unit.BattleSlotNumber > 3 || SeenExplicitEnemySlots.Contains(Unit.BattleSlotNumber))
				{
					OutError = TEXT("Enemy combat units must use unique explicit 1P, 2P, or 3P presentation slots when a slot is saved.");
					return false;
				}
				if (Unit.EnemyDefinitionId.IsNone() || Unit.CombatLevel < 1 || Unit.CombatLevel > FGameXXKCharacterStatRules::MaxCharacterLevel)
				{
					OutError = TEXT("An explicitly slotted enemy combat unit requires a catalog identity and a valid route combat level.");
					return false;
				}
				SeenExplicitEnemySlots.Add(Unit.BattleSlotNumber);
			}
			else if (Unit.Side != EGameXXKCardTargetSide::Enemy
				&& (Unit.BattleSlotNumber != INDEX_NONE
					|| !Unit.EnemyDefinitionId.IsNone()
					|| Unit.CombatLevel < 0
					|| Unit.CombatLevel > FGameXXKCharacterStatRules::MaxCharacterLevel))
			{
				OutError = TEXT("Party combat units may persist a valid character level, but never an enemy definition or presentation slot.");
				return false;
			}
			for (const FGameXXKCardStatusStack& Stack : Unit.Statuses)
			{
				if (!IsConcreteCombatStatus(Stack.Status) || Stack.Stacks <= 0 || Stack.Stacks > GetCombatStatusCap(Stack.Status)
					|| GetCombatStatusStacksInternal(Unit, Stack.Status) > GetCombatStatusCap(Stack.Status))
				{
					OutError = TEXT("Combat unit contains an invalid or unbound status stack.");
					return false;
				}
			}
			SeenUnitIds.Add(Unit.UnitId);
		}
		return true;
	}

	bool ValidateGuardLinks(const TArray<FGameXXKCardCombatUnit>& Units, const TArray<FGameXXKCardGuardLinkRuntime>& GuardLinks, FString& OutError)
	{
		for (const FGameXXKCardGuardLinkRuntime& Link : GuardLinks)
		{
			const FGameXXKCardCombatUnit* Guardian = FindCombatUnitById(Units, Link.GuardianUnitId);
			const FGameXXKCardCombatUnit* Protected = FindCombatUnitById(Units, Link.ProtectedUnitId);
			if (!Guardian || !Protected || Link.GuardianUnitId == Link.ProtectedUnitId || Link.Stacks <= 0
				|| Link.RedirectPolicy != EGameXXKCardGuardRedirectPolicy::RedirectNextSingleTargetDirectAttackToGuardian
				|| Guardian->Side != Protected->Side)
			{
				OutError = TEXT("Combat guard bindings must reference two allied stable units with a valid redirect policy.");
				return false;
			}
		}
		return true;
	}

	bool IsPreferredGuardian(const FGameXXKCardCombatUnit& Candidate, const FGameXXKCardCombatUnit& Current)
	{
		if (Candidate.StableSortOrder != Current.StableSortOrder)
		{
			return Candidate.StableSortOrder < Current.StableSortOrder;
		}
		return Candidate.UnitId.LexicalLess(Current.UnitId);
	}

	void RemoveLinksForDefeatedUnits(TArray<FGameXXKCardGuardLinkRuntime>& InOutGuardLinks, const TArray<FGameXXKCardCombatUnit>& Units)
	{
		InOutGuardLinks.RemoveAll([&Units](const FGameXXKCardGuardLinkRuntime& Link)
		{
			const FGameXXKCardCombatUnit* Guardian = FindCombatUnitById(Units, Link.GuardianUnitId);
			const FGameXXKCardCombatUnit* Protected = FindCombatUnitById(Units, Link.ProtectedUnitId);
			return Link.Stacks <= 0 || !Guardian || !Protected || !Guardian->bLiving || !Protected->bLiving;
		});
	}

	bool IsDirectAttackDamageKind(const EGameXXKCardDamageKind Kind)
	{
		return Kind == EGameXXKCardDamageKind::SingleTargetAttack || Kind == EGameXXKCardDamageKind::GroupAttack;
	}

	bool IsFixedDamageKind(const EGameXXKCardDamageKind Kind)
	{
		return Kind == EGameXXKCardDamageKind::FixedDamage;
	}

	bool IsDirectOrFixedDamageKind(const EGameXXKCardDamageKind Kind)
	{
		return IsDirectAttackDamageKind(Kind) || IsFixedDamageKind(Kind);
	}

	bool ValidateOnHitStatuses(const TArray<FGameXXKCardStatusStack>& OnHitStatuses, FString& OutError)
	{
		TMap<EGameXXKCardStatus, int64> TotalByStatus;
		for (const FGameXXKCardStatusStack& Stack : OnHitStatuses)
		{
			if (!IsConcreteCombatStatus(Stack.Status) || Stack.Stacks <= 0 || Stack.Stacks > GetCombatStatusCap(Stack.Status))
			{
				OutError = TEXT("On-hit statuses must be bounded, concrete combat statuses.");
				return false;
			}
			const int64 NewTotal = TotalByStatus.FindRef(Stack.Status) + static_cast<int64>(Stack.Stacks);
			if (NewTotal > GetCombatStatusCap(Stack.Status))
			{
				OutError = TEXT("On-hit statuses cannot exceed their approved aggregate cap.");
				return false;
			}
			TotalByStatus.Add(Stack.Status, NewTotal);
		}
		return true;
	}

	bool ValidateDamageContext(
		const FGameXXKCardDamageContext& Context,
		const TArray<FGameXXKCardCombatUnit>& Units,
		const FGameXXKCardCombatUnit& OriginalTarget,
		const bool bAllowDefeatedDirectSource,
		FString& OutError)
	{
		if (Context.IgnoredDefense < 0)
		{
			OutError = TEXT("Damage context cannot ignore a negative amount of defense.");
			return false;
		}
		if (Context.MomentumStacksOverride < INDEX_NONE)
		{
			OutError = TEXT("Damage context has an invalid Momentum snapshot.");
			return false;
		}
		if (Context.VulnerabilityStacksToConsumeOverride < INDEX_NONE)
		{
			OutError = TEXT("Damage context has an invalid Vulnerability consumption override.");
			return false;
		}

		if (IsDirectAttackDamageKind(Context.Kind))
		{
			const FGameXXKCardCombatUnit* SourceUnit = FindCombatUnitById(Units, Context.SourceUnitId);
			if (!SourceUnit || (!SourceUnit->bLiving && !bAllowDefeatedDirectSource)
				|| SourceUnit->Side == OriginalTarget.Side
				|| Context.AgilityRollPercent < 0 || Context.AgilityRollPercent > 99)
			{
				OutError = TEXT("A direct attack requires an eligible opposing attacker and a deterministic 0..99 Agility roll.");
				return false;
			}
			return ValidateOnHitStatuses(Context.OnHitStatuses, OutError);
		}

		if (IsFixedDamageKind(Context.Kind))
		{
			const FGameXXKCardCombatUnit* SourceUnit = FindCombatUnitById(Units, Context.SourceUnitId);
			if (!SourceUnit || !SourceUnit->bLiving || SourceUnit->Side == OriginalTarget.Side
				|| Context.IgnoredDefense != 0
				|| Context.MomentumStacksOverride != INDEX_NONE
				|| Context.VulnerabilityStacksToConsumeOverride != INDEX_NONE
				|| !Context.OnHitStatuses.IsEmpty())
			{
				OutError = TEXT("Fixed damage requires a living opposing source and cannot carry attack-only modifiers.");
				return false;
			}
			return true;
		}

		if (Context.Kind == EGameXXKCardDamageKind::SelfHealthLoss)
		{
			const FGameXXKCardCombatUnit* SourceUnit = FindCombatUnitById(Units, Context.SourceUnitId);
			if (!SourceUnit || !SourceUnit->bLiving || SourceUnit->UnitId != OriginalTarget.UnitId
				|| Context.IgnoredDefense != 0 || Context.MomentumStacksOverride != INDEX_NONE
				|| Context.VulnerabilityStacksToConsumeOverride != INDEX_NONE
				|| !Context.OnHitStatuses.IsEmpty())
			{
				OutError = TEXT("Self health loss must target its own living stable source and cannot carry defense bypass or hit statuses.");
				return false;
			}
			return true;
		}

		if (Context.Kind == EGameXXKCardDamageKind::EnvironmentalHealthLoss)
		{
			if (!Context.SourceUnitId.IsNone() || Context.IgnoredDefense != 0
				|| Context.MomentumStacksOverride != INDEX_NONE
				|| Context.VulnerabilityStacksToConsumeOverride != INDEX_NONE
				|| !Context.OnHitStatuses.IsEmpty())
			{
				OutError = TEXT("Environmental health loss cannot carry a source unit, defense bypass, or hit statuses.");
				return false;
			}
			return true;
		}

		OutError = TEXT("Direct damage requires a supported explicit damage kind.");
		return false;
	}

	int32 ComputeDamageAfterDefense(const int32 RequestedDamage, const FGameXXKCardCombatUnit& Target, const int32 IgnoredDefense)
	{
		const int32 EffectiveDefense = FMath::Max(0, Target.Defense - IgnoredDefense);
		const int64 MitigatedDamage = static_cast<int64>(RequestedDamage) - EffectiveDefense;
		return static_cast<int32>(FMath::Clamp<int64>(MitigatedDamage, 1, MAX_int32));
	}
}

void GameXXKCardRules::RefreshCombatTerminalPhase(FGameXXKCardBattleRuntime& InOutRuntime)
{
	UpdateBattleTerminalPhase(InOutRuntime);
}

int32 GameXXKCardRules::GetCombatStatusStacks(const FGameXXKCardCombatUnit& Unit, const EGameXXKCardStatus Status)
{
	return IsConcreteCombatStatus(Status) ? GetCombatStatusStacksInternal(Unit, Status) : 0;
}

int32 GameXXKCardRules::AddCombatStatus(FGameXXKCardCombatUnit& InOutUnit, const EGameXXKCardStatus Status, const int32 Amount)
{
	if (!InOutUnit.bLiving || !IsConcreteCombatStatus(Status) || Amount <= 0)
	{
		return 0;
	}
	if (Status == EGameXXKCardStatus::Vulnerability
		&& GetCombatStatusStacksInternal(InOutUnit, EGameXXKCardStatus::CannotReceiveVulnerability) > 0)
	{
		return 0;
	}
	const int32 CurrentStacks = GetCombatStatusStacksInternal(InOutUnit, Status);
	const int64 AvailableStacks = FMath::Max<int64>(
		0,
		static_cast<int64>(GetCombatStatusCap(Status)) - CurrentStacks);
	const int32 AppliedStacks = static_cast<int32>(FMath::Min<int64>(AvailableStacks, Amount));
	if (AppliedStacks <= 0)
	{
		return 0;
	}
	FGameXXKCardStatusStack* ExistingStack = InOutUnit.Statuses.FindByPredicate([Status](const FGameXXKCardStatusStack& Stack)
	{
		return Stack.Status == Status;
	});
	if (ExistingStack)
	{
		ExistingStack->Stacks += AppliedStacks;
	}
	else
	{
		FGameXXKCardStatusStack& NewStack = InOutUnit.Statuses.AddDefaulted_GetRef();
		NewStack.Status = Status;
		NewStack.Stacks = AppliedStacks;
	}
	return AppliedStacks;
}

int32 GameXXKCardRules::AddDotFromCoefficient(
	FGameXXKCardBattleRuntime& InOutRuntime,
	const FName TargetUnitId,
	const EGameXXKCardStatus Status,
	const int32 BaseCoefficient,
	const EGameXXKCardQuality Quality)
{
	if (TargetUnitId.IsNone()
		|| !IsDotReservoirStatus(Status)
		|| BaseCoefficient <= 0
		|| InOutRuntime.TeamMaxLevelSnapshot < 1
		|| InOutRuntime.TeamMaxLevelSnapshot > 135
		|| FGameXXKCombatScalingRules::GetQualityPercent(Quality) <= 0)
	{
		return 0;
	}
	FGameXXKCardCombatUnit* Target = FindCombatUnitById(InOutRuntime.Units, TargetUnitId);
	if (!Target || !Target->bLiving)
	{
		return 0;
	}
	const int32 Cap = FGameXXKCombatScalingRules::ResolveDotCap(InOutRuntime.TeamMaxLevelSnapshot);
	const int32 Current = GameXXKCardRules::GetCombatStatusStacks(*Target, Status);
	const int32 Available = FMath::Max(0, Cap - FMath::Min(Current, Cap));
	const int32 ResolvedAddition = FGameXXKCombatScalingRules::ResolveDotAddition(
		BaseCoefficient,
		Quality,
		InOutRuntime.TeamMaxLevelSnapshot);
	return GameXXKCardRules::AddCombatStatus(*Target, Status, FMath::Min(Available, ResolvedAddition));
}

int32 GameXXKCardRules::ClearDotReservoir(
	FGameXXKCardCombatUnit& InOutUnit,
	const EGameXXKCardStatus Status)
{
	return IsDotReservoirStatus(Status)
		? GameXXKCardRules::ConsumeCombatStatus(InOutUnit, Status, MAX_int32)
		: 0;
}

int32 GameXXKCardRules::ClearAllDotReservoirs(FGameXXKCardCombatUnit& InOutUnit)
{
	int64 RemovedTotal = 0;
	for (const EGameXXKCardStatus Status : {
		EGameXXKCardStatus::Bleed,
		EGameXXKCardStatus::Poison,
		EGameXXKCardStatus::Burn,
		EGameXXKCardStatus::DamageOverTime})
	{
		RemovedTotal += GameXXKCardRules::ClearDotReservoir(InOutUnit, Status);
	}
	return static_cast<int32>(FMath::Min<int64>(MAX_int32, RemovedTotal));
}

bool GameXXKCardRules::ResolveWhiteApeStatusGuardAfterStatusApplied(
	FGameXXKCardBattleRuntime& InOutRuntime,
	FGameXXKCardCombatUnit& InOutStatusTarget,
	FString* OutError)
{
	return ResolveWhiteApeStatusGuardAfterStatusAppliedInternal(
		InOutRuntime,
		InOutStatusTarget,
		NAME_None,
		nullptr,
		OutError);
}

bool GameXXKCardRules::ResetWhiteApeStatusGuardsForPlayerRound(
	FGameXXKCardBattleRuntime& InOutRuntime,
	FString* OutError)
{
	TMap<FName, FGameXXKEnemyBattleState> NewEnemyStates = InOutRuntime.EnemyStates;
	for (const FGameXXKCardCombatUnit& Unit : InOutRuntime.Units)
	{
		const FGameXXKEnemyDefinition* EnemyDefinition = FindWhiteApeStatusGuardDefinition(Unit);
		if (!EnemyDefinition)
		{
			continue;
		}

		FGameXXKEnemyBattleState& EnemyState = NewEnemyStates.FindOrAdd(Unit.UnitId);
		if (EnemyState.DefinitionId.IsNone())
		{
			EnemyState.DefinitionId = EnemyDefinition->Id;
		}
		if (EnemyState.DefinitionId != EnemyDefinition->Id)
		{
			return SetFailure(OutError, TEXT("White Ape status guard reset found a mismatched persisted enemy definition."));
		}
		EnemyState.bFirstStatusPassiveAvailable = true;
	}
	InOutRuntime.EnemyStates = MoveTemp(NewEnemyStates);
	return true;
}

int32 GameXXKCardRules::ConsumeCombatStatus(FGameXXKCardCombatUnit& InOutUnit, const EGameXXKCardStatus Status, const int32 Maximum)
{
	if (!IsConcreteCombatStatus(Status) || Maximum < 0)
	{
		return 0;
	}
	const int32 EffectiveMaximum = Maximum == 0 ? MAX_int32 : Maximum;
	int32 Remaining = EffectiveMaximum;
	for (FGameXXKCardStatusStack& Stack : InOutUnit.Statuses)
	{
		if (Stack.Status == Status && Stack.Stacks > 0 && Remaining > 0)
		{
			const int32 ConsumedHere = FMath::Min(Stack.Stacks, Remaining);
			Stack.Stacks -= ConsumedHere;
			Remaining -= ConsumedHere;
		}
	}
	RemoveEmptyCombatStatusEntries(InOutUnit);
	return EffectiveMaximum - Remaining;
}

int32 GameXXKCardRules::AddCombatArmor(FGameXXKCardCombatUnit& InOutUnit, const int32 Amount)
{
	if (!InOutUnit.bLiving || Amount <= 0)
	{
		return 0;
	}
	const int32 OriginalArmor = FMath::Max(0, InOutUnit.Armor);
	const int64 RequestedArmor = static_cast<int64>(OriginalArmor) + static_cast<int64>(Amount);
	InOutUnit.Armor = static_cast<int32>(FMath::Min<int64>(MAX_int32, RequestedArmor));
	return InOutUnit.Armor - OriginalArmor;
}

int32 GameXXKCardRules::ResolvePrintedCostArmor(
	const FGameXXKCardCombatUnit& CardOwner,
	const int32 PrintedEnergyCost,
	const EGameXXKCardQuality Quality)
{
	if (!CardOwner.bLiving)
	{
		return 0;
	}
	return FGameXXKCombatScalingRules::ResolvePrintedCostArmor(
		CardOwner.Defense,
		PrintedEnergyCost,
		Quality);
}

int32 GameXXKCardRules::HealCombatUnit(FGameXXKCardCombatUnit& InOutUnit, const int32 Amount)
{
	if (!InOutUnit.bLiving || Amount <= 0)
	{
		return 0;
	}
	const int32 MaximumHealth = FMath::Max(1, InOutUnit.MaxHP);
	const int32 OriginalHealth = FMath::Clamp(InOutUnit.HP, 0, MaximumHealth);
	const int64 RequestedHealth = static_cast<int64>(OriginalHealth) + static_cast<int64>(Amount);
	InOutUnit.HP = static_cast<int32>(FMath::Min<int64>(MaximumHealth, RequestedHealth));
	return InOutUnit.HP - OriginalHealth;
}

int32 GameXXKCardRules::ConsumeSharedCombatEnergy(FGameXXKCardBattleRuntime& InOutRuntime, const int32 Amount)
{
	if (Amount <= 0)
	{
		return 0;
	}
	const int32 OriginalEnergy = FMath::Max(0, InOutRuntime.Deck.SharedEnergy);
	InOutRuntime.Deck.SharedEnergy = FMath::Max(0, OriginalEnergy - Amount);
	return OriginalEnergy - InOutRuntime.Deck.SharedEnergy;
}

void GameXXKCardRules::BeginCombatUnitPhase(FGameXXKCardCombatUnit& InOutUnit)
{
	if (InOutUnit.bLiving)
	{
		InOutUnit.Armor = 0;
	}
}

namespace
{
	bool ApplyHealthLossWithLifeSavingTalisman(
		TArray<FGameXXKCardCombatUnit>& InOutUnits,
		bool* bLifeSavingTalismanArmed,
		bool* bLifeSavingTalismanConsumptionPending,
		const int32 ProjectedHealingPercent,
		FGameXXKCardCombatUnit& Target,
		const int32 RequestedHealthDamage,
		FGameXXKCardPlayResult* InOutPlayResult,
		int32& OutHealthDamage,
		int32& OutPacketHealthAfter,
		FString& OutError)
	{
		OutHealthDamage = 0;
		OutPacketHealthAfter = Target.HP;
		const bool bHasLifeSavingProjection = bLifeSavingTalismanArmed != nullptr
			&& bLifeSavingTalismanConsumptionPending != nullptr;
		const bool bProjectionActive = bHasLifeSavingProjection
			&& (*bLifeSavingTalismanArmed || *bLifeSavingTalismanConsumptionPending);
		if (RequestedHealthDamage < 0
			|| Target.HP < 0
			|| Target.MaxHP <= 0
			|| Target.HP > Target.MaxHP
			|| ((bLifeSavingTalismanArmed == nullptr)
				!= (bLifeSavingTalismanConsumptionPending == nullptr))
			|| (bHasLifeSavingProjection
				&& *bLifeSavingTalismanArmed
				&& *bLifeSavingTalismanConsumptionPending)
			|| (!bHasLifeSavingProjection && ProjectedHealingPercent != 0)
			|| (bProjectionActive
				&& (ProjectedHealingPercent < 1 || ProjectedHealingPercent > 100))
			|| (bHasLifeSavingProjection && !bProjectionActive && ProjectedHealingPercent != 0))
		{
			OutError = TEXT("Health loss requires valid target health and a complete catalog-authored life-saving projection.");
			return false;
		}

		const int32 HealthBefore = Target.HP;
		OutHealthDamage = FMath::Min(HealthBefore, RequestedHealthDamage);
		OutPacketHealthAfter = HealthBefore - OutHealthDamage;
		const bool bTriggersLifeSavingTalisman = bLifeSavingTalismanArmed
			&& *bLifeSavingTalismanArmed
			&& !*bLifeSavingTalismanConsumptionPending
			&& Target.bLiving
			&& Target.Side == EGameXXKCardTargetSide::Party
			&& OutHealthDamage > 0
			&& static_cast<int64>(OutPacketHealthAfter) * 100
				< static_cast<int64>(Target.MaxHP) * 50;
		if (bTriggersLifeSavingTalisman)
		{
			OutPacketHealthAfter = FMath::Max(1, OutPacketHealthAfter);
			OutHealthDamage = HealthBefore - OutPacketHealthAfter;
		}

		Target.HP = OutPacketHealthAfter;
		Target.bLiving = Target.HP > 0;
		if (!bTriggersLifeSavingTalisman)
		{
			return true;
		}

		*bLifeSavingTalismanArmed = false;
		*bLifeSavingTalismanConsumptionPending = true;
		for (FGameXXKCardCombatUnit& Unit : InOutUnits)
		{
			if (!Unit.bLiving || Unit.Side != EGameXXKCardTargetSide::Party)
			{
				continue;
			}
			const int32 RequestedHealing = static_cast<int32>(
				(static_cast<int64>(Unit.MaxHP) * ProjectedHealingPercent + 99) / 100);
			if (InOutPlayResult)
			{
				FGameXXKCardHealingResult& Healing = InOutPlayResult->HealingResults.AddDefaulted_GetRef();
				Healing.SourceUnitId = NAME_None;
				Healing.TargetUnitId = Unit.UnitId;
				Healing.RequestedHealing = RequestedHealing;
				Healing.EffectiveHealing = GameXXKCardRules::HealCombatUnit(Unit, RequestedHealing);
			}
			else
			{
				GameXXKCardRules::HealCombatUnit(Unit, RequestedHealing);
			}
		}
		return true;
	}

	bool ApplyStatusHealthLoss(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FName TargetUnitId,
		const EGameXXKCardDamageCause Cause,
		const int32 BaseStacks,
		FGameXXKCardDamageResult& OutResult,
		FString& OutError,
		FGameXXKCardPlayResult* InOutPlayResult = nullptr)
	{
		if (TargetUnitId.IsNone() || Cause == EGameXXKCardDamageCause::Invalid || BaseStacks <= 0)
		{
			OutError = TEXT("Status health loss requires a stable target, explicit cause, and positive stack snapshot.");
			return false;
		}
		FGameXXKCardCombatUnit* Target = FindCombatUnitById(InOutRuntime.Units, TargetUnitId);
		if (!Target)
		{
			OutError = TEXT("Status health loss target is absent from the battle runtime.");
			return false;
		}

		FGameXXKCardDamageResult NewResult;
		NewResult.OriginalTargetUnitId = TargetUnitId;
		NewResult.ResolvedTargetUnitId = TargetUnitId;
		NewResult.Kind = EGameXXKCardDamageKind::DamageOverTime;
		NewResult.Cause = Cause;
		NewResult.StatusStacksBefore = BaseStacks;
		NewResult.RotDamageBonus = 0;
		NewResult.RequestedDamage = static_cast<int32>(FMath::Min<int64>(
			MAX_int32,
			static_cast<int64>(BaseStacks) + NewResult.RotDamageBonus));
		NewResult.DamageAfterDefense = NewResult.RequestedDamage;
		NewResult.DamageAfterVulnerability = NewResult.RequestedDamage;
		NewResult.DamageBeforeLevelDifference = NewResult.RequestedDamage;
		NewResult.DamageAfterLevelDifference = NewResult.RequestedDamage;
		NewResult.TargetHealthBefore = Target->HP;
		NewResult.TargetArmorBefore = Target->Armor;
		if (!ApplyHealthLossWithLifeSavingTalisman(
			InOutRuntime.Units,
			&InOutRuntime.bLifeSavingTalismanArmed,
			&InOutRuntime.bLifeSavingTalismanConsumptionPending,
			InOutRuntime.LifeSavingTalismanHealingPercent,
			*Target,
			NewResult.RequestedDamage,
			InOutPlayResult,
			NewResult.HealthDamage,
			NewResult.TargetHealthAfter,
			OutError))
		{
			return false;
		}
		NewResult.TargetArmorAfter = Target->Armor;
		RemoveLinksForDefeatedUnits(InOutRuntime.GuardLinks, InOutRuntime.Units);
		OutResult = MoveTemp(NewResult);
		return true;
	}
}

bool GameXXKCardRules::ApplyCombatEndPhaseDot(
	TArray<FGameXXKCardCombatUnit>& InOutUnits,
	TArray<FGameXXKCardGuardLinkRuntime>& InOutGuardLinks,
	const FName TargetUnitId,
	int32& OutHealthDamage,
	FString* OutError)
{
	if (OutError)
	{
		OutError->Reset();
	}
	if (TargetUnitId.IsNone())
	{
		return SetFailure(OutError, TEXT("End-phase DoT requires a living stable target ID."));
	}

	TArray<FGameXXKCardCombatUnit> NewUnits = InOutUnits;
	TArray<FGameXXKCardGuardLinkRuntime> NewGuardLinks = InOutGuardLinks;
	FString ValidationError;
	if (!ValidateCombatUnits(NewUnits, ValidationError) || !ValidateGuardLinks(NewUnits, NewGuardLinks, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	FGameXXKCardCombatUnit* Target = FindCombatUnitById(NewUnits, TargetUnitId);
	if (!Target || !Target->bLiving)
	{
		return SetFailure(OutError, TEXT("End-phase DoT target is absent or defeated."));
	}

	const int32 PoisonStacks = GetCombatStatusStacksInternal(*Target, EGameXXKCardStatus::Poison);
	const int64 RawDamage = PoisonStacks;
	const int32 NewHealthDamage = static_cast<int32>(FMath::Min<int64>(Target->HP, RawDamage));
	Target->HP -= NewHealthDamage;
	Target->bLiving = Target->HP > 0;
	GameXXKCardRules::ConsumeCombatStatus(*Target, EGameXXKCardStatus::Weak, 1);
	RemoveLinksForDefeatedUnits(NewGuardLinks, NewUnits);

	InOutUnits = MoveTemp(NewUnits);
	InOutGuardLinks = MoveTemp(NewGuardLinks);
	OutHealthDamage = NewHealthDamage;
	return true;
}

namespace
{
	bool ApplyCombatEndPhaseDotForRuntime(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FName TargetUnitId,
		int32& OutHealthDamage,
		int32& OutPacketHealthAfter,
		FString* OutError)
	{
		OutHealthDamage = 0;
		OutPacketHealthAfter = 0;
		if (OutError)
		{
			OutError->Reset();
		}
		if (TargetUnitId.IsNone())
		{
			return SetFailure(OutError, TEXT("End-phase DoT requires a living stable target ID."));
		}

		FGameXXKCardBattleRuntime NewRuntime = InOutRuntime;
		FString ValidationError;
		if (!ValidateCardBattleRuntimeInternal(NewRuntime, ValidationError))
		{
			return SetFailure(OutError, ValidationError);
		}
		FGameXXKCardCombatUnit* Target = FindCombatUnitById(NewRuntime.Units, TargetUnitId);
		if (!Target || !Target->bLiving)
		{
			return SetFailure(OutError, TEXT("End-phase DoT target is absent or defeated."));
		}

		const int32 PoisonStacks = GetCombatStatusStacksInternal(*Target, EGameXXKCardStatus::Poison);
		const int64 RawDamage = PoisonStacks;
		if (!ApplyHealthLossWithLifeSavingTalisman(
			NewRuntime.Units,
			&NewRuntime.bLifeSavingTalismanArmed,
			&NewRuntime.bLifeSavingTalismanConsumptionPending,
			NewRuntime.LifeSavingTalismanHealingPercent,
			*Target,
			static_cast<int32>(FMath::Min<int64>(MAX_int32, RawDamage)),
			nullptr,
			OutHealthDamage,
			OutPacketHealthAfter,
			ValidationError))
		{
			return SetFailure(OutError, ValidationError);
		}
		Target = FindCombatUnitById(NewRuntime.Units, TargetUnitId);
		if (!Target)
		{
			return SetFailure(OutError, TEXT("End-phase DoT target disappeared after health loss."));
		}
		GameXXKCardRules::ConsumeCombatStatus(*Target, EGameXXKCardStatus::Weak, 1);
		RemoveLinksForDefeatedUnits(NewRuntime.GuardLinks, NewRuntime.Units);
		if (!ValidateCardBattleRuntimeInternal(NewRuntime, ValidationError))
		{
			return SetFailure(OutError, ValidationError);
		}
		InOutRuntime = MoveTemp(NewRuntime);
		return true;
	}
}

bool GameXXKCardRules::ResolveToxicExplosion(
	FGameXXKCardBattleRuntime& InOutRuntime,
	const FName SourceUnitId,
	const FName TargetUnitId,
	const bool bPreserveDamageOverTimeStacks,
	TArray<FGameXXKCardDamageResult>& OutResults,
	FString* OutError)
{
	(void)bPreserveDamageOverTimeStacks;
	if (OutError)
	{
		OutError->Reset();
	}
	if (SourceUnitId.IsNone() || TargetUnitId.IsNone())
	{
		return SetFailure(OutError, TEXT("Toxic explosion requires stable source and target IDs."));
	}

	FGameXXKCardBattleRuntime NewRuntime = InOutRuntime;
	FString ValidationError;
	if (!ValidateCombatUnits(NewRuntime.Units, ValidationError)
		|| !ValidateGuardLinks(NewRuntime.Units, NewRuntime.GuardLinks, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	const FGameXXKCardCombatUnit* Source = FindCombatUnitById(NewRuntime.Units, SourceUnitId);
	FGameXXKCardCombatUnit* Target = FindCombatUnitById(NewRuntime.Units, TargetUnitId);
	if (!Source || !Source->bLiving || !Target || !Target->bLiving || Source->Side == Target->Side)
	{
		return SetFailure(OutError, TEXT("Toxic explosion requires a living source and opposing living target."));
	}

	struct FExplosionPacketSpec
	{
		EGameXXKCardStatus Status;
		EGameXXKCardDamageCause Cause;
		int32 StacksBefore = 0;
	};
	TArray<FExplosionPacketSpec> PacketSpecs = {
		{EGameXXKCardStatus::Bleed, EGameXXKCardDamageCause::ToxicExplosionBleed},
		{EGameXXKCardStatus::Poison, EGameXXKCardDamageCause::ToxicExplosionPoison},
		{EGameXXKCardStatus::Burn, EGameXXKCardDamageCause::ToxicExplosionBurn},
		{EGameXXKCardStatus::DamageOverTime, EGameXXKCardDamageCause::ToxicExplosionRot}};
	for (FExplosionPacketSpec& PacketSpec : PacketSpecs)
	{
		PacketSpec.StacksBefore = GetCombatStatusStacksInternal(*Target, PacketSpec.Status);
	}

	TArray<FGameXXKCardDamageResult> NewResults;
	NewResults.Reserve(4);
	for (FExplosionPacketSpec& PacketSpec : PacketSpecs)
	{
		if (PacketSpec.StacksBefore <= 0)
		{
			continue;
		}
		FGameXXKCardDamageResult PacketResult;
		if (!ApplyStatusHealthLoss(
			NewRuntime,
			TargetUnitId,
			PacketSpec.Cause,
			PacketSpec.StacksBefore,
			PacketResult,
			ValidationError))
		{
			return SetFailure(OutError, ValidationError);
		}
		PacketResult.SourceUnitId = SourceUnitId;
		NewResults.Add(MoveTemp(PacketResult));
	}
	UpdateBattleTerminalPhase(NewRuntime);
	InOutRuntime = MoveTemp(NewRuntime);
	OutResults = MoveTemp(NewResults);
	return true;
}

namespace
{
	bool ApplyCombatDirectDamageInternal(
	TArray<FGameXXKCardCombatUnit>& InOutUnits,
	TArray<FGameXXKCardGuardLinkRuntime>& InOutGuardLinks,
	const FGameXXKCardDamageContext& Context,
	const FName TargetUnitId,
	const int32 RequestedDamage,
	FGameXXKCardDamageResult& OutResult,
	FGameXXKCardBattleRuntime* PlayerCardRuntime,
	FGameXXKCardBattleRuntime* BattleProjection,
	FGameXXKCardPlayResult* InOutPlayResult,
	const bool bAllowDefeatedDirectSource,
	FString* OutError)
{
	if (OutError)
	{
		OutError->Reset();
	}
	if (TargetUnitId.IsNone() || RequestedDamage <= 0)
	{
		return SetFailure(OutError, TEXT("Direct damage requires a living stable target ID and a positive amount."));
	}
	if (BattleProjection && &BattleProjection->Units != &InOutUnits)
	{
		return SetFailure(OutError, TEXT("Direct damage received a mismatched life-saving battle projection."));
	}

	TArray<FGameXXKCardCombatUnit> NewUnits = InOutUnits;
	TArray<FGameXXKCardGuardLinkRuntime> NewGuardLinks = InOutGuardLinks;
	bool bLifeSavingTalismanArmed = BattleProjection && BattleProjection->bLifeSavingTalismanArmed;
	bool bLifeSavingTalismanConsumptionPending = BattleProjection
		&& BattleProjection->bLifeSavingTalismanConsumptionPending;
	const int32 LifeSavingTalismanHealingPercent = BattleProjection
		? BattleProjection->LifeSavingTalismanHealingPercent
		: 0;
	FString ValidationError;
	if (!ValidateCombatUnits(NewUnits, ValidationError) || !ValidateGuardLinks(NewUnits, NewGuardLinks, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	FGameXXKCardCombatUnit* OriginalTarget = FindCombatUnitById(NewUnits, TargetUnitId);
	if (!OriginalTarget || !OriginalTarget->bLiving)
	{
		return SetFailure(OutError, TEXT("Direct damage target is absent or defeated."));
	}
	if (!ValidateDamageContext(Context, NewUnits, *OriginalTarget, bAllowDefeatedDirectSource, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}

	FGameXXKCardDamageResult NewResult;
	NewResult.SourceUnitId = Context.SourceUnitId;
	NewResult.Kind = Context.Kind;
	NewResult.ResolutionOrigin = Context.ResolutionOrigin;
	NewResult.Cause = IsDirectAttackDamageKind(Context.Kind)
		? EGameXXKCardDamageCause::DirectAttack
		: IsFixedDamageKind(Context.Kind)
			? EGameXXKCardDamageCause::FixedDamage
			: Context.Kind == EGameXXKCardDamageKind::SelfHealthLoss
			? EGameXXKCardDamageCause::SelfLoss
			: EGameXXKCardDamageCause::Environment;
	NewResult.OriginalTargetUnitId = TargetUnitId;
	NewResult.ResolvedTargetUnitId = TargetUnitId;
	NewResult.RequestedDamage = RequestedDamage;
	FGameXXKCardCombatUnit* ResolvedTarget = OriginalTarget;
	int32 SelectedGuardLinkIndex = INDEX_NONE;
	for (int32 LinkIndex = 0; Context.Kind == EGameXXKCardDamageKind::SingleTargetAttack && LinkIndex < NewGuardLinks.Num(); ++LinkIndex)
	{
		const FGameXXKCardGuardLinkRuntime& Link = NewGuardLinks[LinkIndex];
		if (Link.ProtectedUnitId != TargetUnitId)
		{
			continue;
		}
		FGameXXKCardCombatUnit* Guardian = FindCombatUnitById(NewUnits, Link.GuardianUnitId);
		if (!Guardian || !Guardian->bLiving)
		{
			continue;
		}
		if (SelectedGuardLinkIndex == INDEX_NONE || IsPreferredGuardian(*Guardian, *ResolvedTarget))
		{
			SelectedGuardLinkIndex = LinkIndex;
			ResolvedTarget = Guardian;
		}
	}
	if (SelectedGuardLinkIndex != INDEX_NONE)
	{
		FGameXXKCardGuardLinkRuntime& SelectedLink = NewGuardLinks[SelectedGuardLinkIndex];
		--SelectedLink.Stacks;
		NewResult.bRedirected = true;
		NewResult.ResolvedTargetUnitId = ResolvedTarget->UnitId;
	}
	NewResult.TargetHealthBefore = ResolvedTarget->HP;
	NewResult.TargetArmorBefore = ResolvedTarget->Armor;
	int32 PacketHealthAfter = ResolvedTarget->HP;

	const bool bDirectAttack = IsDirectAttackDamageKind(Context.Kind);
	const bool bDirectDamage = IsDirectOrFixedDamageKind(Context.Kind);
	if (bDirectAttack)
	{
		NewResult.AgilityRollPercent = Context.AgilityRollPercent;
	}
	const int32 AgilityStacks = bDirectAttack
		? GetCombatStatusStacksInternal(*ResolvedTarget, EGameXXKCardStatus::Agility)
		: 0;
	if (AgilityStacks > 0 && Context.AgilityRollPercent < 25)
	{
		NewResult.AgilityStacksConsumed = GameXXKCardRules::ConsumeCombatStatus(
			*ResolvedTarget,
			EGameXXKCardStatus::Agility,
			1);
		NewResult.bAvoidedByAgility = true;
		NewResult.bPerfectAgilityDodge = true;
	}
	else if (AgilityStacks >= 2)
	{
		NewResult.AgilityStacksConsumed = GameXXKCardRules::ConsumeCombatStatus(
			*ResolvedTarget,
			EGameXXKCardStatus::Agility,
			2);
		NewResult.bAvoidedByAgility = true;
	}
	else
	{
		const FGameXXKCardCombatUnit* SourceUnit = bDirectDamage
			? FindCombatUnitById(NewUnits, Context.SourceUnitId)
			: nullptr;
		int32 TalentScaledDamage = RequestedDamage;
		if (bDirectAttack
			&& SourceUnit
			&& SourceUnit->Side == EGameXXKCardTargetSide::Party
			&& BattleProjection)
		{
			TalentScaledDamage = static_cast<int32>(FMath::Clamp<int64>(
				(static_cast<int64>(TalentScaledDamage)
					* (100 + FMath::Clamp(BattleProjection->TalentFinalDamagePercent, 0, 100)) + 50) / 100,
				1,
				MAX_int32));
			const int32 CriticalChance = FMath::Clamp(
				BattleProjection->TalentCriticalChancePercent,
				0,
				20);
			if (CriticalChance > 0
				&& AdvanceCombatRandomRoll(*BattleProjection) < CriticalChance)
			{
				NewResult.bTalentCriticalHit = true;
				const int32 CriticalMultiplier = 150
					+ FMath::Clamp(BattleProjection->TalentCriticalDamagePercent, 0, 50);
				TalentScaledDamage = static_cast<int32>(FMath::Clamp<int64>(
					(static_cast<int64>(TalentScaledDamage) * CriticalMultiplier + 50) / 100,
					1,
					MAX_int32));
			}
		}
		const int32 MomentumStacks = Context.MomentumStacksOverride != INDEX_NONE
			? Context.MomentumStacksOverride
			: (SourceUnit ? GetCombatStatusStacksInternal(*SourceUnit, EGameXXKCardStatus::Momentum) : 0);
		const int32 DamageWithMomentum = bDirectAttack
			? static_cast<int32>(FMath::Min<int64>(
				MAX_int32,
				static_cast<int64>(TalentScaledDamage) + MomentumStacks))
			: TalentScaledDamage;
		const int32 DamageAfterWeak = bDirectAttack
			&& SourceUnit
			&& GetCombatStatusStacksInternal(*SourceUnit, EGameXXKCardStatus::Weak) > 0
			? FMath::Max(1, DamageWithMomentum / 2)
			: DamageWithMomentum;
		if (bDirectAttack)
		{
			NewResult.BaseRequestedDamage = RequestedDamage;
			NewResult.MomentumDamageBonus = MomentumStacks;
			NewResult.DamageAfterWeak = DamageAfterWeak;
			NewResult.WeakDamageReduction = DamageWithMomentum - DamageAfterWeak;
		}
		NewResult.RequestedDamage = DamageWithMomentum;
		NewResult.DamageAfterDefense = bDirectAttack
			? ComputeDamageAfterDefense(DamageAfterWeak, *ResolvedTarget, Context.IgnoredDefense)
			: DamageAfterWeak;
		const int32 VulnerabilityStacks = bDirectDamage
			? GetCombatStatusStacksInternal(*ResolvedTarget, EGameXXKCardStatus::Vulnerability)
			: 0;
		const int32 MarkStacks = bDirectDamage
			? GetCombatStatusStacksInternal(*ResolvedTarget, EGameXXKCardStatus::Mark)
			: 0;
		const int32 MarkBonusPercent = MarkStacks > 0
			? GameXXKCardRules::MarkDirectDamageBonusPercent
			: 0;
		if (bDirectDamage)
		{
			NewResult.VulnerabilityStacksBeforeHit = VulnerabilityStacks;
			NewResult.MarkStacksBeforeHit = MarkStacks;
			NewResult.MarkDamageBonusPercent = MarkBonusPercent;
		}
		const int64 AmplifiedDamage = static_cast<int64>(NewResult.DamageAfterDefense)
			* static_cast<int64>(100 + 10 * VulnerabilityStacks + MarkBonusPercent)
			/ 100;
		NewResult.DamageAfterVulnerability = static_cast<int32>(FMath::Min<int64>(MAX_int32, AmplifiedDamage));
		NewResult.DamageBeforeLevelDifference = NewResult.DamageAfterVulnerability;
		NewResult.DamageAfterLevelDifference = NewResult.DamageBeforeLevelDifference;
		if (bDirectDamage && SourceUnit && SourceUnit->CombatLevel > 0 && ResolvedTarget->CombatLevel > 0)
		{
			NewResult.DamageAfterLevelDifference = FGameXXKCombatScalingRules::ApplyLevelDifferenceCeil(
				NewResult.DamageBeforeLevelDifference,
				SourceUnit->CombatLevel,
				ResolvedTarget->CombatLevel);
		}
		if (VulnerabilityStacks > 0)
		{
			const int32 VulnerabilityConsumptionLimit = Context.VulnerabilityStacksToConsumeOverride == INDEX_NONE
				? MAX_int32
				: Context.VulnerabilityStacksToConsumeOverride;
			if (VulnerabilityConsumptionLimit > 0)
			{
				NewResult.VulnerabilityStacksConsumed = GameXXKCardRules::ConsumeCombatStatus(
					*ResolvedTarget,
					EGameXXKCardStatus::Vulnerability,
					VulnerabilityConsumptionLimit);
			}
		}
		if (MarkStacks > 0)
		{
			NewResult.MarkStacksConsumed = GameXXKCardRules::ConsumeCombatStatus(
				*ResolvedTarget,
				EGameXXKCardStatus::Mark,
				1);
		}
		NewResult.ArmorAbsorbed = bDirectDamage
			? FMath::Min(ResolvedTarget->Armor, NewResult.DamageAfterLevelDifference)
			: 0;
		ResolvedTarget->Armor -= NewResult.ArmorAbsorbed;
		NewResult.HealthDamage = NewResult.DamageAfterLevelDifference - NewResult.ArmorAbsorbed;
		EGameXXKEnemyPassiveId ResolvedPlayerCardEnemyPassive = EGameXXKEnemyPassiveId::None;
		if (PlayerCardRuntime
			&& ResolvedTarget->Side == EGameXXKCardTargetSide::Enemy
			&& !ResolvedTarget->EnemyDefinitionId.IsNone())
		{
			const FGameXXKEnemyDefinition* EnemyDefinition = FGameXXKEnemyCatalog::Find(ResolvedTarget->EnemyDefinitionId);
			if (!EnemyDefinition)
			{
				return SetFailure(OutError, TEXT("A player-card enemy target must resolve to a catalog definition."));
			}

			FGameXXKEnemyBattleState& EnemyState = PlayerCardRuntime->EnemyStates.FindOrAdd(ResolvedTarget->UnitId);
			if (EnemyState.DefinitionId.IsNone())
			{
				EnemyState.DefinitionId = EnemyDefinition->Id;
			}
			if (EnemyState.DefinitionId != EnemyDefinition->Id)
			{
				return SetFailure(OutError, TEXT("Player-card enemy damage found a mismatched persisted enemy definition."));
			}
			ResolvedPlayerCardEnemyPassive = EnemyDefinition->PassiveId;

			if (bDirectAttack
				&& EnemyDefinition->PassiveId == EGameXXKEnemyPassiveId::IronfeatherFirstHit
				&& EnemyState.bFirstHitPassiveAvailable)
			{
				const int32 PassiveReducedHealthDamage = NewResult.HealthDamage / 2;
				NewResult.HealthDamage = PassiveReducedHealthDamage;
				if (PassiveReducedHealthDamage > 0)
				{
					EnemyState.bFirstHitPassiveAvailable = false;
				}
			}
			else if (bDirectAttack && EnemyDefinition->PassiveId == EGameXXKEnemyPassiveId::BlackBearThickHide)
			{
				const int64 ThickHideReducedHealthDamage = static_cast<int64>(NewResult.HealthDamage) * 85 / 100;
				NewResult.HealthDamage = static_cast<int32>(FMath::Min<int64>(MAX_int32, ThickHideReducedHealthDamage));
			}
		}
		NewResult.HealthDamage = FMath::Min(ResolvedTarget->HP, NewResult.HealthDamage);
		if (ResolvedPlayerCardEnemyPassive == EGameXXKEnemyPassiveId::RedtuskRage && NewResult.HealthDamage > 0)
		{
			const int32 CurrentRage = GetCombatStatusStacksInternal(*ResolvedTarget, EGameXXKCardStatus::Rage);
			if (CurrentRage < 5)
			{
				GameXXKCardRules::AddCombatStatus(*ResolvedTarget, EGameXXKCardStatus::Rage, 1);
			}
		}
		if (!ApplyHealthLossWithLifeSavingTalisman(
			NewUnits,
			BattleProjection ? &bLifeSavingTalismanArmed : nullptr,
			BattleProjection ? &bLifeSavingTalismanConsumptionPending : nullptr,
			LifeSavingTalismanHealingPercent,
			*ResolvedTarget,
			NewResult.HealthDamage,
			InOutPlayResult,
			NewResult.HealthDamage,
			PacketHealthAfter,
			ValidationError))
		{
			return SetFailure(OutError, ValidationError);
		}
		if (ResolvedTarget->bLiving && IsDirectAttackDamageKind(Context.Kind))
		{
			for (const FGameXXKCardStatusStack& OnHitStatus : Context.OnHitStatuses)
			{
				if (GameXXKCardRules::AddCombatStatus(*ResolvedTarget, OnHitStatus.Status, OnHitStatus.Stacks) > 0
					&& PlayerCardRuntime
					&& !ResolveWhiteApeStatusGuardAfterStatusAppliedInternal(
						*PlayerCardRuntime,
						*ResolvedTarget,
						Context.SourceUnitId,
						InOutPlayResult,
						OutError))
				{
					return false;
				}
			}
		}
	}
	NewResult.TargetHealthAfter = PacketHealthAfter;
	NewResult.TargetArmorAfter = ResolvedTarget->Armor;

	RemoveLinksForDefeatedUnits(NewGuardLinks, NewUnits);
	if (BattleProjection)
	{
		BattleProjection->bLifeSavingTalismanArmed = bLifeSavingTalismanArmed;
		BattleProjection->bLifeSavingTalismanConsumptionPending = bLifeSavingTalismanConsumptionPending;
	}
	InOutUnits = MoveTemp(NewUnits);
	InOutGuardLinks = MoveTemp(NewGuardLinks);
	OutResult = MoveTemp(NewResult);
	return true;
}
}

bool GameXXKCardRules::ApplyCombatDirectDamage(
	TArray<FGameXXKCardCombatUnit>& InOutUnits,
	TArray<FGameXXKCardGuardLinkRuntime>& InOutGuardLinks,
	const FGameXXKCardDamageContext& Context,
	const FName TargetUnitId,
	const int32 RequestedDamage,
	FGameXXKCardDamageResult& OutResult,
	FString* OutError)
{
	return ApplyCombatDirectDamageInternal(
		InOutUnits,
		InOutGuardLinks,
		Context,
		TargetUnitId,
		RequestedDamage,
		OutResult,
		nullptr,
		nullptr,
		nullptr,
		false,
		OutError);
}

namespace
{
	constexpr int32 MaxCardBattleEnergy = 99;

	bool IsSupportedCardBattlePhase(const EGameXXKCardBattlePhase Phase)
	{
		return Phase == EGameXXKCardBattlePhase::Player
			|| Phase == EGameXXKCardBattlePhase::Enemy
			|| Phase == EGameXXKCardBattlePhase::Victory
			|| Phase == EGameXXKCardBattlePhase::Defeat;
	}

	bool IsStableUnitOrderBefore(const FGameXXKCardCombatUnit& Left, const FGameXXKCardCombatUnit& Right)
	{
		if (Left.StableSortOrder != Right.StableSortOrder)
		{
			return Left.StableSortOrder < Right.StableSortOrder;
		}
		return Left.UnitId.LexicalLess(Right.UnitId);
	}

	TArray<FGameXXKCardTargetUnit> BuildTargetUnitView(const TArray<FGameXXKCardCombatUnit>& Units)
	{
		TArray<FGameXXKCardTargetUnit> Result;
		Result.Reserve(Units.Num());
		for (const FGameXXKCardCombatUnit& Unit : Units)
		{
			FGameXXKCardTargetUnit& TargetUnit = Result.AddDefaulted_GetRef();
			TargetUnit.UnitId = Unit.UnitId;
			TargetUnit.Side = Unit.Side;
			TargetUnit.bLiving = Unit.bLiving;
			TargetUnit.HP = Unit.HP;
			TargetUnit.MaxHP = Unit.MaxHP;
			TargetUnit.StableSortOrder = Unit.StableSortOrder;
			TargetUnit.Statuses = Unit.Statuses;
		}
		return Result;
	}

	bool IsCurrentHandInstance(const FGameXXKBattleDeckState& Deck, const FName InstanceId)
	{
		return !InstanceId.IsNone() && Deck.Hand.ContainsByPredicate([InstanceId](const FGameXXKCardInstance& Instance)
		{
			return Instance.InstanceId == InstanceId;
		});
	}

	bool IsAutomaticResolutionOrigin(const EGameXXKCardResolutionOrigin Origin)
	{
		return Origin == EGameXXKCardResolutionOrigin::AutomaticReplay
			|| Origin == EGameXXKCardResolutionOrigin::MageTaskReplay
			|| Origin == EGameXXKCardResolutionOrigin::TaskNpcTaskReplay
			|| Origin == EGameXXKCardResolutionOrigin::PartnerSorcererTaskReplay
			|| Origin == EGameXXKCardResolutionOrigin::HeavyArrow
			|| Origin == EGameXXKCardResolutionOrigin::Reaction
			|| Origin == EGameXXKCardResolutionOrigin::TerrainListener
			|| Origin == EGameXXKCardResolutionOrigin::TaskReward;
	}

	bool ValidateResolvedCardSnapshot(
		const FGameXXKResolvedCardSnapshot& Snapshot,
		const TArray<FGameXXKCardCombatUnit>& Units,
		FString& OutError)
	{
		const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(Snapshot.CardId);
		if (Snapshot.CardId.IsNone()
			|| !Definition
			|| !IsConcreteCardQuality(Snapshot.Quality)
			|| Snapshot.OwnerUnitId.IsNone()
			|| !FindCombatUnitById(Units, Snapshot.OwnerUnitId)
			|| Snapshot.PaidManaCost < 0
			|| Snapshot.SorcererSequencePosition < 0
			|| Snapshot.SorcererSequencePosition > 5)
		{
			OutError = TEXT("An automatic card snapshot has no catalog card, concrete quality, stable owner, or supported Sorcerer audit values.");
			return false;
		}
		if (Snapshot.SorcererSequencePosition == 0)
		{
			if (Snapshot.PaidManaCost != 0
				|| Snapshot.PreviousSorcererFamily != EGameXXKSorcererCardFamily::None
				|| Snapshot.SorcererTaskBranch != EGameXXKSorcererTaskBranch::None)
			{
				OutError = TEXT("A non-Sorcerer-task snapshot contains stale Sorcerer sequence audit data.");
				return false;
			}
		}
		else if (Definition->Owner != EGameXXKCardOwner::Profession
			|| Definition->Role != EGameXXKCharacterRole::Sorcerer
			|| Definition->SorcererRule.Family == EGameXXKSorcererCardFamily::None
			|| Definition->SorcererRule.SequenceRule == EGameXXKSorcererSequenceRule::None
			|| Definition->SorcererRule.RewardRule == EGameXXKSorcererRewardRule::None)
		{
			OutError = TEXT("Sorcerer sequence audit data belongs to a non-Sorcerer card snapshot.");
			return false;
		}
		TSet<FName> SeenTargetIds;
		for (const FName TargetUnitId : Snapshot.OriginalTargetUnitIds)
		{
			if (TargetUnitId.IsNone()
				|| SeenTargetIds.Contains(TargetUnitId)
				|| !FindCombatUnitById(Units, TargetUnitId))
			{
				OutError = TEXT("An automatic card snapshot contains an invalid, duplicate, or unknown original target.");
				return false;
			}
			SeenTargetIds.Add(TargetUnitId);
		}
		return true;
	}

	bool IsImplementedSheathedStyleRule(const EGameXXKBladeChargeRule Rule)
	{
		return Rule == EGameXXKBladeChargeRule::LightLoad
			|| Rule == EGameXXKBladeChargeRule::DrawTwoAfterNextActive
			|| Rule == EGameXXKBladeChargeRule::DrawSameOwnerAfterNextActive
			|| Rule == EGameXXKBladeChargeRule::DrawOtherOwnerAfterNextActive;
	}

	bool IsImplementedPartnerBladeChargeRule(const EGameXXKBladeChargeRule Rule)
	{
		switch (Rule)
		{
		case EGameXXKBladeChargeRule::ReplayNextActiveBase:
		case EGameXXKBladeChargeRule::CopyNextActiveToHand:
		case EGameXXKBladeChargeRule::ReturnNextActiveToHandOnce:
		case EGameXXKBladeChargeRule::ReplayNextActiveNextRound:
		case EGameXXKBladeChargeRule::RestoreNextActiveOwnerState:
		case EGameXXKBladeChargeRule::DuplicateNextSingleTargetOrDraw:
		case EGameXXKBladeChargeRule::MakeNextActiveEnergyFree:
		case EGameXXKBladeChargeRule::MakeNextActiveManaFree:
		case EGameXXKBladeChargeRule::RefundNextActiveCosts:
		case EGameXXKBladeChargeRule::CountNextActiveTwice:
		case EGameXXKBladeChargeRule::CopyNextActiveNextRound:
		case EGameXXKBladeChargeRule::RetainNextActiveNextRound:
		case EGameXXKBladeChargeRule::PreserveFinishCandidate:
		case EGameXXKBladeChargeRule::RetainRemainingHand:
		case EGameXXKBladeChargeRule::LightLoad:
		case EGameXXKBladeChargeRule::DrawTwoAfterNextActive:
		case EGameXXKBladeChargeRule::DrawSameOwnerAfterNextActive:
		case EGameXXKBladeChargeRule::DrawOtherOwnerAfterNextActive:
			return true;
		case EGameXXKBladeChargeRule::None:
		default:
			return false;
		}
	}

	bool ValidateBladeChargeRuntime(const FGameXXKCardBattleRuntime& Runtime, FString& OutError)
	{
		const FGameXXKBladeChargeRuntime& Charge = Runtime.PendingBladeCharge;
		if (Charge.Rule == EGameXXKBladeChargeRule::None)
		{
			if (!Charge.SourceCardId.IsNone()
				|| !Charge.SourceOwnerUnitId.IsNone()
				|| Charge.CreatedRound != 0)
			{
				OutError = TEXT("An inactive Blade Charge contains stale source metadata.");
				return false;
			}
			return true;
		}
		if (!IsImplementedPartnerBladeChargeRule(Charge.Rule)
			|| Runtime.Phase != EGameXXKCardBattlePhase::Player
			|| Charge.CreatedRound != Runtime.RoundNumber
			|| Charge.SourceCardId.IsNone()
			|| Charge.SourceOwnerUnitId.IsNone()
			|| !IsConcreteCardQuality(Charge.SourceQuality))
		{
			OutError = TEXT("An active Blade Charge has an unsupported rule, phase, round, or source identity.");
			return false;
		}
		const FGameXXKCardDefinition* SourceDefinition = FGameXXKCardCatalog::FindCardDefinition(Charge.SourceCardId);
		const FGameXXKCardCombatUnit* SourceOwner = FindCombatUnitById(Runtime.Units, Charge.SourceOwnerUnitId);
		if (!SourceDefinition
			|| SourceDefinition->Owner != EGameXXKCardOwner::Profession
			|| SourceDefinition->Role != EGameXXKCharacterRole::Blade
			|| SourceDefinition->BladeSequence.ChargeRule != Charge.Rule
			|| !SourceOwner
			|| SourceOwner->Side != EGameXXKCardTargetSide::Party)
		{
			OutError = TEXT("An active Blade Charge no longer matches its declarative card or party owner.");
			return false;
		}
		return true;
	}

	bool ValidateBladeStyleRuntime(
		const FGameXXKCardBattleRuntime& Runtime,
		const FGameXXKBladeStyleRuntime& Style,
		const bool bExpectedResidual,
		FString& OutError)
	{
		if (Style.Rule == EGameXXKBladeChargeRule::None)
		{
			if (!Style.SourceCardId.IsNone()
				|| !Style.SourceOwnerUnitId.IsNone()
				|| Style.TriggerPlayerRound != 0
				|| Style.bResidual)
			{
				OutError = TEXT("An inactive Blade style contains stale source or residual metadata.");
				return false;
			}
			return true;
		}
		const bool bExpectedRound = bExpectedResidual
			? Runtime.Phase == EGameXXKCardBattlePhase::Player
				&& Style.TriggerPlayerRound == Runtime.RoundNumber
			: (Runtime.Phase == EGameXXKCardBattlePhase::Enemy
					&& Style.TriggerPlayerRound == Runtime.RoundNumber + 1)
				|| (Runtime.Phase == EGameXXKCardBattlePhase::Player
					&& Style.TriggerPlayerRound == Runtime.RoundNumber);
		const bool bSupportedStyleRule = bExpectedResidual
			? IsImplementedPartnerBladeChargeRule(Style.Rule)
			: IsImplementedSheathedStyleRule(Style.Rule);
		if (!bSupportedStyleRule
			|| Style.bResidual != bExpectedResidual
			|| !bExpectedRound
			|| Style.SourceCardId.IsNone()
			|| Style.SourceOwnerUnitId.IsNone()
			|| !IsConcreteCardQuality(Style.SourceQuality))
		{
			OutError = TEXT("An active Blade style has an unsupported rule, phase, round, or source identity.");
			return false;
		}
		const FGameXXKCardDefinition* SourceDefinition = FGameXXKCardCatalog::FindCardDefinition(Style.SourceCardId);
		const FGameXXKCardCombatUnit* SourceOwner = FindCombatUnitById(Runtime.Units, Style.SourceOwnerUnitId);
		if (!SourceDefinition
			|| SourceDefinition->Owner != EGameXXKCardOwner::Profession
			|| SourceDefinition->Role != EGameXXKCharacterRole::Blade
			|| SourceDefinition->BladeSequence.ChargeRule != Style.Rule
			|| (!bExpectedResidual
				&& SourceDefinition->BladeSequence.FinishRule != EGameXXKBladeFinishRule::StoreChargeAsNativeStyle)
			|| !SourceOwner
			|| SourceOwner->Side != EGameXXKCardTargetSide::Party)
		{
			OutError = TEXT("An active Blade style no longer matches its declarative Sheathed source or party owner.");
			return false;
		}
		return true;
	}

	bool ValidatePoJunEquipmentRuntime(
		const FGameXXKCardBattleRuntime& Runtime,
		const FGameXXKEquipmentBattleEffectRuntime& EffectRuntime,
		FString& OutError)
	{
		const FGameXXKEquipmentActiveEffect& Effect = EffectRuntime.ActiveEffect;
		const bool bPoJunTwoPiece = Effect.Set == EGameXXKEquipmentSet::PoJun
			&& Effect.Hook == EGameXXKEquipmentSetBonusHook::PoJunChargeConsumed;
		const bool bPoJunFourPiece = Effect.Set == EGameXXKEquipmentSet::PoJun
			&& Effect.Hook == EGameXXKEquipmentSetBonusHook::PoJunBladeFinish;
		const bool bPoJunSixPiece = Effect.Set == EGameXXKEquipmentSet::PoJun
			&& Effect.Hook == EGameXXKEquipmentSetBonusHook::PoJunFirstActiveNextRound;
		const FGameXXKPoJunStoredStyleRuntime& Style = EffectRuntime.PendingPoJunStyle;
		const bool bInactiveStyle = Style.Rule == EGameXXKBladeChargeRule::None
			&& Style.SourceCardId.IsNone()
			&& Style.SourceOwnerUnitId.IsNone()
			&& Style.TriggerPlayerRound == 0;
		if (Effect.Set != EGameXXKEquipmentSet::PoJun)
		{
			if (!bInactiveStyle
				|| EffectRuntime.PoJunChargeProgressRound != 0
				|| EffectRuntime.bPoJunChargeConsumedThisRound
				|| EffectRuntime.PendingPoJunReplayPlayerRound != 0)
			{
				OutError = TEXT("A non-PoJun equipment descriptor contains PoJun-only runtime state.");
				return false;
			}
			return true;
		}
		if (!bPoJunFourPiece && !bInactiveStyle)
		{
			OutError = TEXT("Only a PoJun four-piece descriptor may retain a stored Charge style.");
			return false;
		}
		if (bPoJunFourPiece && Style.Rule == EGameXXKBladeChargeRule::None && !bInactiveStyle)
		{
			OutError = TEXT("An inactive PoJun stored style contains stale source metadata.");
			return false;
		}
		if (bPoJunFourPiece && Style.Rule != EGameXXKBladeChargeRule::None)
		{
			const bool bExpectedRound = (Runtime.Phase == EGameXXKCardBattlePhase::Enemy
					&& Style.TriggerPlayerRound == Runtime.RoundNumber + 1)
				|| (Runtime.Phase == EGameXXKCardBattlePhase::Player
					&& Style.TriggerPlayerRound == Runtime.RoundNumber);
			const FGameXXKCardDefinition* SourceDefinition = FGameXXKCardCatalog::FindCardDefinition(Style.SourceCardId);
			if (!IsImplementedPartnerBladeChargeRule(Style.Rule)
				|| !bExpectedRound
				|| Style.SourceOwnerUnitId != EffectRuntime.SourceCharacterId
				|| !IsConcreteCardQuality(Style.SourceQuality)
				|| !SourceDefinition
				|| SourceDefinition->Owner != EGameXXKCardOwner::Profession
				|| SourceDefinition->Role != EGameXXKCharacterRole::Blade
				|| SourceDefinition->BladeSequence.ChargeRule != Style.Rule
				|| SourceDefinition->BladeSequence.FinishRule == EGameXXKBladeFinishRule::None)
			{
				OutError = TEXT("An active PoJun stored style has an invalid round, wearer, or Blade source.");
				return false;
			}
		}

		if (bPoJunTwoPiece)
		{
			if (EffectRuntime.CurrentRoundTriggerCount > Effect.MaxTriggersPerRound
				|| (EffectRuntime.CurrentRoundTriggerCount > 0 && EffectRuntime.LastTriggerRound == 0))
			{
				OutError = TEXT("PoJun two-piece draw progress exceeds its per-wearer round budget.");
				return false;
			}
		}
		else if (EffectRuntime.CurrentRoundTriggerCount != 0 || EffectRuntime.LastTriggerRound != 0)
		{
			OutError = TEXT("Only PoJun two-piece may use the generic equipment trigger counter.");
			return false;
		}

		if (!bPoJunSixPiece)
		{
			if (EffectRuntime.PoJunChargeProgressRound != 0
				|| EffectRuntime.bPoJunChargeConsumedThisRound
				|| EffectRuntime.PendingPoJunReplayPlayerRound != 0)
			{
				OutError = TEXT("Only PoJun six-piece may retain opening/finisher loop progress.");
				return false;
			}
			return true;
		}

		const bool bProgressInactive = EffectRuntime.PoJunChargeProgressRound == 0
			&& !EffectRuntime.bPoJunChargeConsumedThisRound;
		const bool bProgressActive = Runtime.Phase == EGameXXKCardBattlePhase::Player
			&& EffectRuntime.PoJunChargeProgressRound == Runtime.RoundNumber
			&& EffectRuntime.bPoJunChargeConsumedThisRound;
		if (!bProgressInactive && !bProgressActive)
		{
			OutError = TEXT("PoJun six-piece Charge progress is stale or outside its player round.");
			return false;
		}
		if (EffectRuntime.PendingPoJunReplayPlayerRound != 0)
		{
			const bool bExpectedReplayRound = (Runtime.Phase == EGameXXKCardBattlePhase::Enemy
					&& EffectRuntime.PendingPoJunReplayPlayerRound == Runtime.RoundNumber + 1)
				|| (Runtime.Phase == EGameXXKCardBattlePhase::Player
					&& EffectRuntime.PendingPoJunReplayPlayerRound == Runtime.RoundNumber);
			if (!bExpectedReplayRound || bProgressActive)
			{
				OutError = TEXT("PoJun six-piece replay is stale, conflicting, or outside its trigger round.");
				return false;
			}
		}
		return true;
	}

	int32 CountEligibleBladeStyleRules(
		const FGameXXKCardBattleRuntime& Runtime,
		const EGameXXKBladeChargeRule Rule)
	{
		int32 Count = 0;
		if (Runtime.PendingBladeNativeStyle.Rule == Rule
			&& Runtime.PendingBladeNativeStyle.TriggerPlayerRound == Runtime.RoundNumber
			&& Runtime.ActiveCardsPlayedThisRound == 0)
		{
			++Count;
		}
		if (Runtime.PendingBladeResidualStyle.Rule == Rule
			&& Runtime.PendingBladeResidualStyle.TriggerPlayerRound == Runtime.RoundNumber)
		{
			++Count;
		}
		if (Runtime.ActiveCardsPlayedThisRound == 0)
		{
			for (const FGameXXKEquipmentBattleEffectRuntime& EffectRuntime : Runtime.EquipmentEffects)
			{
				const FGameXXKCardCombatUnit* Wearer = FindCombatUnitById(Runtime.Units, EffectRuntime.SourceCharacterId);
				if (EffectRuntime.ActiveEffect.Set == EGameXXKEquipmentSet::PoJun
					&& EffectRuntime.ActiveEffect.Hook == EGameXXKEquipmentSetBonusHook::PoJunBladeFinish
					&& EffectRuntime.PendingPoJunStyle.Rule == Rule
					&& EffectRuntime.PendingPoJunStyle.TriggerPlayerRound == Runtime.RoundNumber
					&& Wearer
					&& Wearer->bLiving)
				{
					++Count;
				}
			}
		}
		return Count;
	}

	bool ValidateBladeDelayedCardRuntime(const FGameXXKCardBattleRuntime& Runtime, FString& OutError)
	{
		const FGameXXKBladeDelayedCardRuntime& Delayed = Runtime.PendingBladeDelayedCard;
		if (Delayed.Rule == EGameXXKBladeChargeRule::None)
		{
			if (!Delayed.SourceCardId.IsNone()
				|| !Delayed.SourceOwnerUnitId.IsNone()
				|| !Delayed.RecordedCard.CardId.IsNone()
				|| !Delayed.RecordedCard.OwnerUnitId.IsNone()
				|| !Delayed.RecordedCard.OriginalTargetUnitIds.IsEmpty()
				|| !IsSameInstance(Delayed.RecordedInstance, FGameXXKCardInstance())
				|| Delayed.TriggerPlayerRound != 0)
			{
				OutError = TEXT("An inactive delayed Blade card contains stale source or recorded-card metadata.");
				return false;
			}
			return true;
		}
		const bool bSupportedDelayedRule = Delayed.Rule == EGameXXKBladeChargeRule::ReplayNextActiveNextRound
			|| Delayed.Rule == EGameXXKBladeChargeRule::CopyNextActiveNextRound
			|| Delayed.Rule == EGameXXKBladeChargeRule::RetainNextActiveNextRound;
		if (!bSupportedDelayedRule
			|| (Runtime.Phase != EGameXXKCardBattlePhase::Player && Runtime.Phase != EGameXXKCardBattlePhase::Enemy)
			|| Delayed.TriggerPlayerRound != Runtime.RoundNumber + 1
			|| Delayed.SourceCardId.IsNone()
			|| Delayed.SourceOwnerUnitId.IsNone()
			|| !IsConcreteCardQuality(Delayed.SourceQuality)
			|| !ValidateResolvedCardSnapshot(Delayed.RecordedCard, Runtime.Units, OutError)
			|| !IsValidInstance(Delayed.RecordedInstance)
			|| Delayed.RecordedInstance.CardId != Delayed.RecordedCard.CardId
			|| Delayed.RecordedInstance.CurrentQuality != Delayed.RecordedCard.Quality
			|| Delayed.RecordedInstance.OwnerUnitId != Delayed.RecordedCard.OwnerUnitId)
		{
			if (OutError.IsEmpty())
			{
				OutError = TEXT("A delayed Blade card has an unsupported rule, phase, trigger round, source, or snapshot.");
			}
			return false;
		}
		const FGameXXKCardDefinition* SourceDefinition = FGameXXKCardCatalog::FindCardDefinition(Delayed.SourceCardId);
		const FGameXXKCardCombatUnit* SourceOwner = FindCombatUnitById(Runtime.Units, Delayed.SourceOwnerUnitId);
		if (!SourceDefinition
			|| SourceDefinition->Owner != EGameXXKCardOwner::Profession
			|| SourceDefinition->Role != EGameXXKCharacterRole::Blade
			|| SourceDefinition->BladeSequence.ChargeRule != Delayed.Rule
			|| !SourceOwner
			|| SourceOwner->Side != EGameXXKCardTargetSide::Party)
		{
			OutError = TEXT("A delayed Blade card no longer matches its declarative Charge source or party owner.");
			return false;
		}
		return true;
	}

	bool ValidateBladeFinishRuntime(const FGameXXKCardBattleRuntime& Runtime, FString& OutError)
	{
		const FGameXXKBladeFinishRuntime& Finish = Runtime.PendingBladeFinish;
		if (Finish.Rule == EGameXXKBladeFinishRule::None)
		{
			if (!Finish.SourceCardId.IsNone()
				|| !Finish.SourceOwnerUnitId.IsNone()
				|| Finish.TriggerPlayerRound != 0
				|| Finish.RemainingTriggers != 0
				|| !Finish.ProtectedTargetUnitId.IsNone()
				|| Finish.ProtectedStatusStacks != 0
				|| Finish.bTriggeredForCurrentEnemyCard)
			{
				OutError = TEXT("An inactive Blade Finish contains stale source metadata.");
				return false;
			}
			return true;
		}
		const bool bExpectedRound = (Runtime.Phase == EGameXXKCardBattlePhase::Enemy
			&& Finish.TriggerPlayerRound == Runtime.RoundNumber + 1)
			|| (Runtime.Phase == EGameXXKCardBattlePhase::Player
				&& Finish.TriggerPlayerRound == Runtime.RoundNumber);
		const bool bNoProtectedStatus = Finish.ProtectedTargetUnitId.IsNone()
			&& Finish.ProtectedStatusStacks == 0;
		const bool bReturnRule = Finish.Rule == EGameXXKBladeFinishRule::ReturnFirstActiveNextRound
			&& Finish.RemainingTriggers == 0
			&& bNoProtectedStatus
			&& !Finish.bTriggeredForCurrentEnemyCard;
		const bool bBleedingTargetReturnRule = Finish.Rule == EGameXXKBladeFinishRule::ReturnFirstActiveAgainstBleeding
			&& Finish.RemainingTriggers == 0
			&& bNoProtectedStatus
			&& !Finish.bTriggeredForCurrentEnemyCard;
		const bool bEnemyPreparationRule = Finish.Rule == EGameXXKBladeFinishRule::MarkAndPrepareTwoCounters
			&& Runtime.Phase == EGameXXKCardBattlePhase::Enemy
			&& Finish.RemainingTriggers >= 1
			&& Finish.RemainingTriggers <= 2
			&& bNoProtectedStatus;
		const bool bBleedPreservationRule = Finish.Rule == EGameXXKBladeFinishRule::PreserveFirstTwoBleedTriggers
			&& Finish.RemainingTriggers >= 1
			&& Finish.RemainingTriggers <= 2
			&& bNoProtectedStatus
			&& !Finish.bTriggeredForCurrentEnemyCard;
		const bool bBleedDrawRule = Finish.Rule == EGameXXKBladeFinishRule::DrawOnFirstThreeBleedTriggers
			&& Finish.RemainingTriggers >= 1
			&& Finish.RemainingTriggers <= 3
			&& bNoProtectedStatus
			&& !Finish.bTriggeredForCurrentEnemyCard;
		const bool bBleedHealingRule = Finish.Rule == EGameXXKBladeFinishRule::HealBladeBleedCapTwelve
			&& Finish.RemainingTriggers >= 1
			&& Finish.RemainingTriggers <= FGameXXKCombatScalingRules::ResolveDotAddition(
				20, Finish.SourceQuality, Runtime.TeamMaxLevelSnapshot)
			&& bNoProtectedStatus
			&& !Finish.bTriggeredForCurrentEnemyCard;
		const FGameXXKCardCombatUnit* ProtectedTarget = FindCombatUnitById(Runtime.Units, Finish.ProtectedTargetUnitId);
		const bool bVulnerabilityFreezeRule = Finish.Rule == EGameXXKBladeFinishRule::FreezeVulnerabilityAndReplay
			&& Finish.RemainingTriggers == 0
			&& !Finish.bTriggeredForCurrentEnemyCard
			&& ((bNoProtectedStatus)
				|| (!Finish.ProtectedTargetUnitId.IsNone()
					&& Finish.ProtectedStatusStacks > 0
					&& Finish.ProtectedStatusStacks <= GetCombatStatusCap(EGameXXKCardStatus::Vulnerability)
					&& ProtectedTarget
					&& ProtectedTarget->Side == EGameXXKCardTargetSide::Enemy));
		const bool bSingleActiveRule = (Finish.Rule == EGameXXKBladeFinishRule::CopyFirstStatusConsumer
				|| Finish.Rule == EGameXXKBladeFinishRule::RefundFirstHighCostAndDrawTwo
				|| Finish.Rule == EGameXXKBladeFinishRule::CopyFirstKill)
			&& Finish.RemainingTriggers == 0
			&& bNoProtectedStatus
			&& !Finish.bTriggeredForCurrentEnemyCard;
		const bool bCounterReregisterRule = Finish.Rule == EGameXXKBladeFinishRule::MarkAndReregisterCounterVolley
			&& Runtime.Phase == EGameXXKCardBattlePhase::Enemy
			&& Finish.RemainingTriggers == 1
			&& bNoProtectedStatus
			&& !Finish.bTriggeredForCurrentEnemyCard;
		const bool bFreeDodgeRule = Finish.Rule == EGameXXKBladeFinishRule::FirstTwoDodgesFree
			&& Runtime.Phase == EGameXXKCardBattlePhase::Enemy
			&& Finish.RemainingTriggers >= 1
			&& Finish.RemainingTriggers <= 2
			&& bNoProtectedStatus
			&& !Finish.bTriggeredForCurrentEnemyCard;
		const bool bMarkTransferRule = Finish.Rule == EGameXXKBladeFinishRule::TransferMarkBeforeCounter
			&& Runtime.Phase == EGameXXKCardBattlePhase::Enemy
			&& Finish.RemainingTriggers == 0
			&& bNoProtectedStatus
			&& !Finish.bTriggeredForCurrentEnemyCard;
		const bool bGroupCounterRule = Finish.Rule == EGameXXKBladeFinishRule::FirstCounterVolleyHitsAll
			&& Runtime.Phase == EGameXXKCardBattlePhase::Enemy
			&& Finish.RemainingTriggers == 1
			&& bNoProtectedStatus
			&& !Finish.bTriggeredForCurrentEnemyCard;
		if ((!bReturnRule && !bBleedingTargetReturnRule && !bEnemyPreparationRule && !bBleedPreservationRule && !bBleedDrawRule && !bBleedHealingRule
				&& !bVulnerabilityFreezeRule && !bSingleActiveRule && !bCounterReregisterRule && !bFreeDodgeRule
				&& !bMarkTransferRule && !bGroupCounterRule)
			|| !bExpectedRound
			|| Finish.SourceCardId.IsNone()
			|| Finish.SourceOwnerUnitId.IsNone()
			|| !IsConcreteCardQuality(Finish.SourceQuality))
		{
			OutError = FString::Printf(
				TEXT("An active Blade Finish has an unsupported rule, phase, round, or source identity. "
					"rule=%d phase=%d round=%d trigger_round=%d remaining=%d source_card=%s source_owner=%s quality=%d protected_target=%s protected_stacks=%d enemy_card_triggered=%d expected_round=%d"),
				static_cast<int32>(Finish.Rule),
				static_cast<int32>(Runtime.Phase),
				Runtime.RoundNumber,
				Finish.TriggerPlayerRound,
				Finish.RemainingTriggers,
				*Finish.SourceCardId.ToString(),
				*Finish.SourceOwnerUnitId.ToString(),
				static_cast<int32>(Finish.SourceQuality),
				*Finish.ProtectedTargetUnitId.ToString(),
				Finish.ProtectedStatusStacks,
				Finish.bTriggeredForCurrentEnemyCard ? 1 : 0,
				bExpectedRound ? 1 : 0);
			return false;
		}
		const FGameXXKCardDefinition* SourceDefinition = FGameXXKCardCatalog::FindCardDefinition(Finish.SourceCardId);
		const FGameXXKCardCombatUnit* SourceOwner = FindCombatUnitById(Runtime.Units, Finish.SourceOwnerUnitId);
		if (!SourceDefinition
			|| SourceDefinition->Owner != EGameXXKCardOwner::Profession
			|| SourceDefinition->Role != EGameXXKCharacterRole::Blade
			|| SourceDefinition->BladeSequence.FinishRule != Finish.Rule
			|| !SourceOwner
			|| SourceOwner->Side != EGameXXKCardTargetSide::Party)
		{
			OutError = TEXT("An active Blade Finish no longer matches its declarative card or party owner.");
			return false;
		}
		return true;
	}

	bool ValidateBladeRetainedHandRuntime(const FGameXXKCardBattleRuntime& Runtime, FString& OutError)
	{
		if (Runtime.BladeRetainedHandCardInstanceIds.IsEmpty())
		{
			return true;
		}
		if (Runtime.Phase != EGameXXKCardBattlePhase::Player)
		{
			OutError = TEXT("Blade retained-hand identities may only persist during their player phase.");
			return false;
		}
		TSet<FName> SeenIds;
		for (const FName InstanceId : Runtime.BladeRetainedHandCardInstanceIds)
		{
			if (InstanceId.IsNone() || SeenIds.Contains(InstanceId))
			{
				OutError = TEXT("Blade retained-hand identities must be stable and unique.");
				return false;
			}
			SeenIds.Add(InstanceId);
		}
		return true;
	}

	bool ValidateTemporaryCardRuntime(const FGameXXKCardBattleRuntime& Runtime, FString& OutError)
	{
		const auto ValidateZone = [&Runtime, &OutError](const TArray<FGameXXKCardInstance>& Zone)
		{
			for (const FGameXXKCardInstance& Card : Zone)
			{
				if (Card.bTemporary
					&& (Runtime.Phase == EGameXXKCardBattlePhase::Enemy
						|| Card.ExpireAfterPlayerRound != Runtime.RoundNumber))
				{
					OutError = TEXT("A temporary card survived beyond its owning player-round boundary.");
					return false;
				}
			}
			return true;
		};
		return ValidateZone(Runtime.Deck.DrawPile)
			&& ValidateZone(Runtime.Deck.Hand)
			&& ValidateZone(Runtime.Deck.DiscardPile)
			&& ValidateZone(Runtime.Deck.ExhaustPile)
			&& ValidateZone(Runtime.Deck.PendingAutomaticHandCards);
	}

	bool ValidateAutomaticResolutionQueue(
		const FGameXXKCardBattleRuntime& Runtime,
		FString& OutError)
	{
		const FGameXXKAutomaticResolutionQueue& Queue = Runtime.AutomaticResolutionQueue;
		const bool bHasHeroReward = Queue.PendingReward != EGameXXKHeroSpellTaskReward::None;
		const bool bHasSorcererReward = Queue.PendingSorcererReward != EGameXXKSorcererRewardRule::None;
		const bool bHasAnyReward = bHasHeroReward || bHasSorcererReward;
		if (!Queue.bActive)
		{
			if (Queue.Origin != EGameXXKCardResolutionOrigin::Invalid
				|| !Queue.PendingCards.IsEmpty()
				|| Queue.NextCardIndex != 0
				|| bHasAnyReward
				|| !Queue.RewardOwnerUnitId.IsNone())
			{
				OutError = TEXT("An inactive automatic card queue contains stale continuation data.");
				return false;
			}
			return true;
		}
		if (!IsAutomaticResolutionOrigin(Queue.Origin)
			|| Queue.NextCardIndex < 0
			|| Queue.NextCardIndex > Queue.PendingCards.Num()
			|| Queue.PendingCards.Num() > 64
			|| (Queue.PendingCards.IsEmpty() && !bHasAnyReward)
			|| (bHasHeroReward && bHasSorcererReward)
			|| (bHasAnyReward == Queue.RewardOwnerUnitId.IsNone())
			|| (Queue.Origin == EGameXXKCardResolutionOrigin::MageTaskReplay) != bHasHeroReward
			|| (Queue.Origin == EGameXXKCardResolutionOrigin::PartnerSorcererTaskReplay) != bHasSorcererReward)
		{
			OutError = TEXT("An active automatic card queue has an invalid origin, cursor, or pending reward.");
			return false;
		}
		if (!Queue.RewardOwnerUnitId.IsNone() && !FindCombatUnitById(Runtime.Units, Queue.RewardOwnerUnitId))
		{
			OutError = TEXT("An automatic card reward references an unknown owner record.");
			return false;
		}
		for (const FGameXXKResolvedCardSnapshot& Snapshot : Queue.PendingCards)
		{
			if (!ValidateResolvedCardSnapshot(Snapshot, Runtime.Units, OutError))
			{
				return false;
			}
		}
		return true;
	}

	bool ValidateEquippedHeroCardIds(const TArray<FName>& EquippedHeroCardIds, FString& OutError)
	{
		if (EquippedHeroCardIds.IsEmpty())
		{
			return true;
		}
		if (EquippedHeroCardIds.Num() != 8)
		{
			OutError = TEXT("A protagonist spell-task loadout must contain exactly eight Hero CardIds.");
			return false;
		}
		TSet<FName> SeenCardIds;
		for (const FName CardId : EquippedHeroCardIds)
		{
			const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(CardId);
			if (CardId.IsNone()
				|| SeenCardIds.Contains(CardId)
				|| !Definition
				|| Definition->Owner != EGameXXKCardOwner::Hero)
			{
				OutError = TEXT("Equipped protagonist spell-task IDs must be unique catalog Hero cards.");
				return false;
			}
			SeenCardIds.Add(CardId);
		}
		return true;
	}

	bool AreSameResolvedCardSnapshots(
		const FGameXXKResolvedCardSnapshot& Left,
		const FGameXXKResolvedCardSnapshot& Right)
	{
		return Left.CardId == Right.CardId
			&& Left.Quality == Right.Quality
			&& Left.OwnerUnitId == Right.OwnerUnitId
			&& Left.OriginalTargetUnitIds == Right.OriginalTargetUnitIds
			&& Left.PaidManaCost == Right.PaidManaCost
			&& Left.SorcererSequencePosition == Right.SorcererSequencePosition
			&& Left.PreviousSorcererFamily == Right.PreviousSorcererFamily
			&& Left.SorcererTaskBranch == Right.SorcererTaskBranch;
	}

	TArray<FName> CollectEquippedHeroSorcererCardIds(const FGameXXKCardBattleRuntime& Runtime)
	{
		TArray<FName> Result;
		for (const FName CardId : Runtime.EquippedHeroCardIds)
		{
			const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(CardId);
			if (Definition
				&& Definition->Owner == EGameXXKCardOwner::Hero
				&& Definition->LinkedRole == EGameXXKCharacterRole::Sorcerer
				&& Definition->SpellTaskReward != EGameXXKHeroSpellTaskReward::None)
			{
				Result.Add(CardId);
			}
		}
		return Result;
	}

	bool ValidateHeroSpellTaskRuntime(const FGameXXKCardBattleRuntime& Runtime, FString& OutError)
	{
		if (!ValidateEquippedHeroCardIds(Runtime.EquippedHeroCardIds, OutError))
		{
			return false;
		}
		if (Runtime.HeroSpellTaskLastCompletedRound < 0
			|| Runtime.HeroSpellTaskLastCompletedRound > Runtime.RoundNumber)
		{
			OutError = TEXT("The protagonist spell-task completion round is outside the battle timeline.");
			return false;
		}
		const FGameXXKHeroSpellTaskRuntime& Task = Runtime.HeroSpellTask;
		if (!Task.bActive)
		{
			if (!Task.LockedHeroCardIds.IsEmpty()
				|| Runtime.Deck.PendingChoice.bLegacyHeroTaskSearch
				|| !Task.CompletedHeroCardIds.IsEmpty()
				|| !Task.FirstPlayOrder.IsEmpty()
				|| Task.StarterReward != EGameXXKHeroSpellTaskReward::None
				|| !Task.StarterOwnerUnitId.IsNone())
			{
				OutError = TEXT("An inactive protagonist spell task contains stale progress.");
				return false;
			}
			if (Runtime.AutomaticResolutionQueue.bActive
				&& Runtime.AutomaticResolutionQueue.Origin == EGameXXKCardResolutionOrigin::MageTaskReplay)
			{
				OutError = TEXT("A Mage replay queue requires its active completed spell task.");
				return false;
			}
			return true;
		}

		const TArray<FName> EquippedSorcererCardIds = CollectEquippedHeroSorcererCardIds(Runtime);
		if (EquippedSorcererCardIds.Num() != 4
			|| Task.LockedHeroCardIds != EquippedSorcererCardIds
			|| Task.CompletedHeroCardIds.Num() != Task.FirstPlayOrder.Num()
			|| Task.CompletedHeroCardIds.Num() > Task.LockedHeroCardIds.Num()
			|| Task.StarterReward == EGameXXKHeroSpellTaskReward::None
			|| Task.StarterOwnerUnitId.IsNone()
			|| (Runtime.HeroSpellTaskLastCompletedRound > 0
				&& Runtime.HeroSpellTaskLastCompletedRound == Runtime.RoundNumber))
		{
			OutError = TEXT("An active protagonist spell task has invalid locked IDs, progress, or starter metadata.");
			return false;
		}
		const FGameXXKCardCombatUnit* StarterOwner = FindCombatUnitById(Runtime.Units, Task.StarterOwnerUnitId);
		if (!StarterOwner || StarterOwner->Side != EGameXXKCardTargetSide::Party)
		{
			OutError = TEXT("An active protagonist spell task has no stable party starter owner.");
			return false;
		}
		TSet<FName> SeenCompletedIds;
		for (int32 Index = 0; Index < Task.CompletedHeroCardIds.Num(); ++Index)
		{
			const FName CardId = Task.CompletedHeroCardIds[Index];
			const FGameXXKResolvedCardSnapshot& Snapshot = Task.FirstPlayOrder[Index];
			if (CardId.IsNone()
				|| SeenCompletedIds.Contains(CardId)
				|| !Task.LockedHeroCardIds.Contains(CardId)
				|| Snapshot.CardId != CardId
				|| !ValidateResolvedCardSnapshot(Snapshot, Runtime.Units, OutError))
			{
				if (OutError.IsEmpty())
				{
					OutError = TEXT("A protagonist spell task contains duplicate, unlocked, or mismatched first-play progress.");
				}
				return false;
			}
			SeenCompletedIds.Add(CardId);
		}
		if (!Task.FirstPlayOrder.IsEmpty())
		{
			const FGameXXKCardDefinition* StarterDefinition = FGameXXKCardCatalog::FindCardDefinition(Task.FirstPlayOrder[0].CardId);
			const bool bRecordedMageStarter = StarterDefinition
				&& StarterDefinition->Owner == EGameXXKCardOwner::Hero
				&& StarterDefinition->LinkedRole == EGameXXKCharacterRole::Sorcerer
				&& StarterDefinition->SpellTaskReward != EGameXXKHeroSpellTaskReward::None;
			if (!bRecordedMageStarter
				|| Task.FirstPlayOrder[0].OwnerUnitId != Task.StarterOwnerUnitId
				|| StarterDefinition->SpellTaskReward != Task.StarterReward)
			{
				OutError = TEXT("A protagonist spell task starter no longer matches its saved Mage reward.");
				return false;
			}
		}

		const FGameXXKAutomaticResolutionQueue& Queue = Runtime.AutomaticResolutionQueue;
		if (Runtime.Deck.PendingChoice.bLegacyHeroTaskSearch)
		{
			if (!Queue.bActive || Runtime.Deck.PendingChoice.Candidates.ContainsByPredicate([&Task, &Runtime](const FGameXXKCardInstance& Card)
			{
				const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(Card.CardId);
				return Card.OwnerUnitId != Task.StarterOwnerUnitId
					|| !Runtime.EquippedHeroCardIds.Contains(Card.CardId)
					|| !Definition || Definition->Owner != EGameXXKCardOwner::Hero;
			}))
			{
				OutError = TEXT("A legacy Hero search has no saved replay or contains an unrelated candidate.");
				return false;
			}
		}
		if (Queue.bActive && Queue.Origin == EGameXXKCardResolutionOrigin::MageTaskReplay)
		{
			if (Task.CompletedHeroCardIds.Num() != 4
				|| Queue.PendingCards.Num() != Task.FirstPlayOrder.Num()
				|| Queue.PendingReward != Task.StarterReward
				|| Queue.RewardOwnerUnitId != Task.StarterOwnerUnitId)
			{
				OutError = TEXT("A Mage replay queue does not match its completed spell task.");
				return false;
			}
			for (int32 Index = 0; Index < Queue.PendingCards.Num(); ++Index)
			{
				if (!AreSameResolvedCardSnapshots(Queue.PendingCards[Index], Task.FirstPlayOrder[Index]))
				{
					OutError = TEXT("A Mage replay queue changed its first-play snapshot order.");
					return false;
				}
			}
		}
		return true;
	}

	bool IsNamedTaskNpcSpellOwnerId(const FName OwnerId)
	{
		return OwnerId == FName(TEXT("Npc.SongJinBao"))
			|| OwnerId == FName(TEXT("Npc.YueBai"));
	}

	bool ValidateTaskNpcSpellTaskRuntime(const FGameXXKCardBattleRuntime& Runtime, FString& OutError)
	{
		TSet<FName> SeenOwnerUnitIds;
		for (const FGameXXKTaskNpcSpellTaskRuntime& Task : Runtime.TaskNpcSpellTasks)
		{
			const FGameXXKCardCombatUnit* Owner = FindCombatUnitById(Runtime.Units, Task.OwnerUnitId);
			if (!Task.bActive
				|| Task.OwnerUnitId.IsNone()
				|| SeenOwnerUnitIds.Contains(Task.OwnerUnitId)
				|| !Owner
				|| Owner->Side != EGameXXKCardTargetSide::Party
				|| Task.LockedCardIds.Num() != 3
				|| Task.CompletedCardIds.Num() != Task.FirstPlayOrder.Num()
				|| Task.CompletedCardIds.Num() > Task.LockedCardIds.Num())
			{
				OutError = TEXT("A named task-NPC spell task has invalid owner, locked-card, or progress state.");
				return false;
			}
			SeenOwnerUnitIds.Add(Task.OwnerUnitId);

			FName CatalogOwnerId = NAME_None;
			TSet<FName> SeenLockedCardIds;
			for (const FName CardId : Task.LockedCardIds)
			{
				const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(CardId);
				if (CardId.IsNone()
					|| SeenLockedCardIds.Contains(CardId)
					|| !Definition
					|| Definition->Owner != EGameXXKCardOwner::QuestNpc
					|| !IsNamedTaskNpcSpellOwnerId(Definition->OwnerId)
					|| (!CatalogOwnerId.IsNone() && Definition->OwnerId != CatalogOwnerId))
				{
					OutError = TEXT("A named task-NPC spell task must lock three unique cards from one supported NPC catalog owner.");
					return false;
				}
				CatalogOwnerId = Definition->OwnerId;
				SeenLockedCardIds.Add(CardId);
			}

			TSet<FName> SeenCompletedCardIds;
			for (int32 Index = 0; Index < Task.CompletedCardIds.Num(); ++Index)
			{
				const FName CardId = Task.CompletedCardIds[Index];
				const FGameXXKResolvedCardSnapshot& Snapshot = Task.FirstPlayOrder[Index];
				if (CardId.IsNone()
					|| SeenCompletedCardIds.Contains(CardId)
					|| !Task.LockedCardIds.Contains(CardId)
					|| Snapshot.CardId != CardId
					|| Snapshot.OwnerUnitId != Task.OwnerUnitId
					|| !ValidateResolvedCardSnapshot(Snapshot, Runtime.Units, OutError))
				{
					if (OutError.IsEmpty())
					{
						OutError = TEXT("A named task-NPC spell task contains duplicate, unlocked, or mismatched first-play progress.");
					}
					return false;
				}
				SeenCompletedCardIds.Add(CardId);
			}
		}
		return true;
	}

	EGameXXKSorcererTaskBranch SorcererBranchForFamily(const EGameXXKSorcererCardFamily Family)
	{
		switch (Family)
		{
		case EGameXXKSorcererCardFamily::Fire:
			return EGameXXKSorcererTaskBranch::Fire;
		case EGameXXKSorcererCardFamily::Ice:
			return EGameXXKSorcererTaskBranch::Ice;
		case EGameXXKSorcererCardFamily::Lightning:
			return EGameXXKSorcererTaskBranch::Lightning;
		case EGameXXKSorcererCardFamily::Core:
		case EGameXXKSorcererCardFamily::Universal:
			return EGameXXKSorcererTaskBranch::Normal;
		case EGameXXKSorcererCardFamily::None:
		default:
			return EGameXXKSorcererTaskBranch::None;
		}
	}

	bool ValidateSorcererPartnerTaskRuntimes(const FGameXXKCardBattleRuntime& Runtime, FString& OutError)
	{
		TSet<FName> SeenOwnerUnitIds;
		bool bMatchedCompletedReplayQueue = false;
		for (const FGameXXKSorcererPartnerTaskRuntime& Task : Runtime.SorcererPartnerTasks)
		{
			const FGameXXKCardCombatUnit* Owner = FindCombatUnitById(Runtime.Units, Task.OwnerUnitId);
			if (Task.OwnerUnitId.IsNone()
				|| SeenOwnerUnitIds.Contains(Task.OwnerUnitId)
				|| !Owner
				|| Owner->Side != EGameXXKCardTargetSide::Party
				|| Owner->Role != EGameXXKCharacterRole::Sorcerer)
			{
				OutError = TEXT("A Sorcerer partner task has an invalid, duplicate, or non-Sorcerer party owner.");
				return false;
			}
			SeenOwnerUnitIds.Add(Task.OwnerUnitId);

			const auto FindOwnedPermanentCard = [&Runtime, &Task](const FName CardId) -> const FGameXXKCardInstance*
			{
				for (const TArray<FGameXXKCardInstance>* Zone : {
					&Runtime.Deck.DrawPile,
					&Runtime.Deck.Hand,
					&Runtime.Deck.DiscardPile,
					&Runtime.Deck.ExhaustPile,
					&Runtime.Deck.PendingAutomaticHandCards})
				{
					if (const FGameXXKCardInstance* Instance = Zone->FindByPredicate([CardId, &Task](const FGameXXKCardInstance& Candidate)
					{
						return Candidate.CardId == CardId
							&& Candidate.OwnerUnitId == Task.OwnerUnitId
							&& !Candidate.bTemporary;
					}))
					{
						return Instance;
					}
				}
				return nullptr;
			};

			TSet<FName> SeenAutoHandedCardIds;
			for (const FName CardId : Task.AutoHandedUniversalCardIds)
			{
				const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(CardId);
				if (CardId.IsNone()
					|| SeenAutoHandedCardIds.Contains(CardId)
					|| !Definition
					|| Definition->Owner != EGameXXKCardOwner::Profession
					|| Definition->Role != EGameXXKCharacterRole::Sorcerer
					|| Definition->SorcererRule.Family != EGameXXKSorcererCardFamily::Universal
					|| !FindOwnedPermanentCard(CardId))
				{
					OutError = TEXT("A Sorcerer task has invalid or duplicate per-battle Universal auto-hand history.");
					return false;
				}
				SeenAutoHandedCardIds.Add(CardId);
			}

			if (!Task.bActive)
			{
				if (!Task.LockedCardIds.IsEmpty()
					|| !Task.CompletedCardIds.IsEmpty()
					|| !Task.FirstPlayOrder.IsEmpty()
					|| Task.StarterReward != EGameXXKSorcererRewardRule::None
					|| Task.LockedBranch != EGameXXKSorcererTaskBranch::None)
				{
					OutError = TEXT("An inactive Sorcerer partner task contains stale five-card progress.");
					return false;
				}
				continue;
			}

			if (Task.LockedCardIds.Num() != 5
				|| Task.CompletedCardIds.IsEmpty()
				|| Task.CompletedCardIds.Num() != Task.FirstPlayOrder.Num()
				|| Task.CompletedCardIds.Num() > Task.LockedCardIds.Num()
				|| Task.StarterReward == EGameXXKSorcererRewardRule::None)
			{
				OutError = TEXT("An active Sorcerer partner task has invalid five-card locks, progress, or starter metadata.");
				return false;
			}

			TSet<FName> SeenLockedCardIds;
			for (const FName CardId : Task.LockedCardIds)
			{
				const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(CardId);
				if (CardId.IsNone()
					|| SeenLockedCardIds.Contains(CardId)
					|| !Definition
					|| Definition->Owner != EGameXXKCardOwner::Profession
					|| Definition->Role != EGameXXKCharacterRole::Sorcerer
					|| Definition->OwnerId != FName(TEXT("Profession.Sorcerer"))
					|| Definition->SorcererRule.Family == EGameXXKSorcererCardFamily::None
					|| Definition->SorcererRule.SequenceRule == EGameXXKSorcererSequenceRule::None
					|| Definition->SorcererRule.RewardRule == EGameXXKSorcererRewardRule::None
					|| !FindOwnedPermanentCard(CardId))
				{
					OutError = TEXT("A Sorcerer partner task must lock five unique permanent cards owned by that Sorcerer.");
					return false;
				}
				SeenLockedCardIds.Add(CardId);
			}
			for (const FName CardId : Task.AutoHandedUniversalCardIds)
			{
				if (!Task.LockedCardIds.Contains(CardId))
				{
					OutError = TEXT("An active Sorcerer task contains Universal auto-hand history outside its locked loadout.");
					return false;
				}
			}

			TSet<FName> SeenCompletedCardIds;
			for (int32 Index = 0; Index < Task.CompletedCardIds.Num(); ++Index)
			{
				const FName CardId = Task.CompletedCardIds[Index];
				const FGameXXKResolvedCardSnapshot& Snapshot = Task.FirstPlayOrder[Index];
				const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(CardId);
				const EGameXXKSorcererCardFamily ExpectedPreviousFamily = Index == 0
					? EGameXXKSorcererCardFamily::None
					: FGameXXKCardCatalog::FindCardDefinition(Task.CompletedCardIds[Index - 1])->SorcererRule.Family;
				if (CardId.IsNone()
					|| SeenCompletedCardIds.Contains(CardId)
					|| !Task.LockedCardIds.Contains(CardId)
					|| !Definition
					|| Snapshot.CardId != CardId
					|| Snapshot.OwnerUnitId != Task.OwnerUnitId
					|| Snapshot.SorcererSequencePosition != Index + 1
					|| Snapshot.PreviousSorcererFamily != ExpectedPreviousFamily
					|| !ValidateResolvedCardSnapshot(Snapshot, Runtime.Units, OutError))
				{
					if (OutError.IsEmpty())
					{
						OutError = TEXT("A Sorcerer partner task contains duplicate, unlocked, or mismatched first-play progress.");
					}
					return false;
				}
				SeenCompletedCardIds.Add(CardId);
			}

			const FGameXXKCardDefinition* StarterDefinition = FGameXXKCardCatalog::FindCardDefinition(Task.FirstPlayOrder[0].CardId);
			if (!StarterDefinition || StarterDefinition->SorcererRule.RewardRule != Task.StarterReward)
			{
				OutError = TEXT("A Sorcerer partner task starter no longer matches its saved reward rule.");
				return false;
			}

			EGameXXKSorcererTaskBranch ExpectedBranch = EGameXXKSorcererTaskBranch::None;
			if (StarterDefinition->SorcererRule.Family == EGameXXKSorcererCardFamily::Universal)
			{
				if (Task.FirstPlayOrder.Num() >= 2)
				{
					const FGameXXKCardDefinition* SecondDefinition = FGameXXKCardCatalog::FindCardDefinition(Task.FirstPlayOrder[1].CardId);
					ExpectedBranch = SecondDefinition
						? SorcererBranchForFamily(SecondDefinition->SorcererRule.Family)
						: EGameXXKSorcererTaskBranch::None;
				}
			}
			else
			{
				ExpectedBranch = SorcererBranchForFamily(StarterDefinition->SorcererRule.Family);
			}
			if (ExpectedBranch != Task.LockedBranch)
			{
				OutError = TEXT("A Sorcerer partner task branch does not match its starter and second-card families.");
				return false;
			}
			for (const FGameXXKResolvedCardSnapshot& Snapshot : Task.FirstPlayOrder)
			{
				if (Snapshot.SorcererTaskBranch != Task.LockedBranch)
				{
					OutError = TEXT("A Sorcerer first-play snapshot does not carry the task's locked branch.");
					return false;
				}
			}

			if (Task.CompletedCardIds.Num() == 5)
			{
				const FGameXXKAutomaticResolutionQueue& Queue = Runtime.AutomaticResolutionQueue;
				if (!Queue.bActive
					|| Queue.Origin != EGameXXKCardResolutionOrigin::PartnerSorcererTaskReplay
					|| Queue.PendingCards.Num() != Task.FirstPlayOrder.Num()
					|| Queue.PendingSorcererReward != Task.StarterReward
					|| Queue.RewardOwnerUnitId != Task.OwnerUnitId)
				{
					OutError = TEXT("A completed Sorcerer partner task must be owned by its matching automatic replay queue.");
					return false;
				}
				for (int32 Index = 0; Index < Queue.PendingCards.Num(); ++Index)
				{
					if (!AreSameResolvedCardSnapshots(Queue.PendingCards[Index], Task.FirstPlayOrder[Index]))
					{
						OutError = TEXT("A Sorcerer partner replay queue changed its first-play snapshot order.");
						return false;
					}
				}
				bMatchedCompletedReplayQueue = true;
			}
		}
		if (Runtime.AutomaticResolutionQueue.bActive
			&& Runtime.AutomaticResolutionQueue.Origin == EGameXXKCardResolutionOrigin::PartnerSorcererTaskReplay
			&& !bMatchedCompletedReplayQueue)
		{
			OutError = TEXT("A Sorcerer partner replay queue has no matching completed owner task.");
			return false;
		}
		return true;
	}

	bool ValidateCardBattleRuntimeInternal(const FGameXXKCardBattleRuntime& Runtime, FString& OutError)
	{
		OutError.Reset();
		const bool bLifeSavingProjectionActive = Runtime.bLifeSavingTalismanArmed
			|| Runtime.bLifeSavingTalismanConsumptionPending;
		if (!IsSupportedCardBattlePhase(Runtime.Phase) || !IsConcreteTerrain(Runtime.Terrain) || Runtime.RoundNumber < 1
			|| Runtime.TeamMaxLevelSnapshot < 1 || Runtime.TeamMaxLevelSnapshot > 135
			|| (Runtime.EnemyDifficultyDamagePercent != 100
				&& Runtime.EnemyDifficultyDamagePercent != 125
				&& Runtime.EnemyDifficultyDamagePercent != 150)
			|| Runtime.PendingNextRoundEnergyPenalty < 0 || Runtime.PendingNextRoundEnergyPenalty > 99
			|| Runtime.ActiveCardsPlayedThisRound < 0 || Runtime.NextReactionOrdinal < 0
			|| Runtime.NextGeneratedCardOrdinal < 0 || Runtime.NextModifierOrdinal < 0
			|| Runtime.PendingTriggeredDrawCount < 0
			|| Runtime.RevealedEnemyIntentCount < 0 || Runtime.RevealedEnemyIntentCount > MaxCardBattleEnergy
			|| Runtime.PendingPreservedPartyReactionUses < 0 || Runtime.PendingPreservedPartyReactionUses > 1
			|| Runtime.PendingNextRoundEnergyBonus < 0 || Runtime.PendingNextRoundEnergyBonus > MaxCardBattleEnergy
			|| Runtime.PendingNextPlayerHandEnergySurcharge < 0 || Runtime.PendingNextPlayerHandEnergySurcharge > 1
			|| Runtime.TalentFinalDamagePercent < 0 || Runtime.TalentFinalDamagePercent > 100
			|| Runtime.TalentCriticalChancePercent < 0 || Runtime.TalentCriticalChancePercent > 20
			|| Runtime.TalentCriticalDamagePercent < 0 || Runtime.TalentCriticalDamagePercent > 50
			|| (Runtime.PendingNextPlayerHandEnergySurcharge == 0) != Runtime.PendingNextPlayerHandEnergySurchargeSourceUnitId.IsNone()
			|| (Runtime.bLifeSavingTalismanArmed && Runtime.bLifeSavingTalismanConsumptionPending)
			|| (bLifeSavingProjectionActive
				&& (Runtime.LifeSavingTalismanHealingPercent < 1
					|| Runtime.LifeSavingTalismanHealingPercent > 100))
			|| (!bLifeSavingProjectionActive && Runtime.LifeSavingTalismanHealingPercent != 0))
		{
			OutError = TEXT("Card battle runtime has an invalid phase, terrain, round, modifier counter, deferred card state, or life-saving projection.");
			return false;
		}
		if (!ValidateDeckStateInternal(Runtime.Deck, OutError)
			|| !ValidateBladeChargeRuntime(Runtime, OutError)
			|| !ValidateBladeDelayedCardRuntime(Runtime, OutError)
			|| !ValidateBladeFinishRuntime(Runtime, OutError)
			|| !ValidateBladeStyleRuntime(Runtime, Runtime.PendingBladeNativeStyle, false, OutError)
			|| !ValidateBladeStyleRuntime(Runtime, Runtime.PendingBladeResidualStyle, true, OutError)
			|| !ValidateBladeRetainedHandRuntime(Runtime, OutError)
			|| !ValidateTemporaryCardRuntime(Runtime, OutError))
		{
			return false;
		}
		if (Runtime.Deck.SharedEnergy < 0 || Runtime.Deck.SharedEnergy > MaxCardBattleEnergy)
		{
			OutError = TEXT("Card battle shared energy is outside the supported range.");
			return false;
		}
		if (Runtime.Units.IsEmpty() || !ValidateCombatUnits(Runtime.Units, OutError) || !ValidateGuardLinks(Runtime.Units, Runtime.GuardLinks, OutError))
		{
			if (OutError.IsEmpty())
			{
				OutError = TEXT("Card battle runtime must contain validated stable combat units.");
			}
			return false;
		}

		bool bHasParty = false;
		bool bHasEnemy = false;
		TSet<FName> ModifierIds;
		int32 HandBoundEnergySurchargeCount = 0;
		TSet<FName> HandBoundSorcererManaDiscountInstanceIds;
		for (const FGameXXKCardCombatUnit& Unit : Runtime.Units)
		{
			bHasParty |= Unit.Side == EGameXXKCardTargetSide::Party;
			bHasEnemy |= Unit.Side == EGameXXKCardTargetSide::Enemy;
		}
		for (const TMap<FName, int32>* PendingHunterMap : {
			&Runtime.PendingHunterHeavyArrowIgnoreDefense,
			&Runtime.PendingHunterPerfectDodgeCharge})
		{
			for (const TPair<FName, int32>& Pending : *PendingHunterMap)
			{
				const FGameXXKCardCombatUnit* PendingOwner = FindCombatUnitById(Runtime.Units, Pending.Key);
				if (Pending.Key.IsNone() || Pending.Value <= 0 || !PendingOwner
					|| PendingOwner->Side != EGameXXKCardTargetSide::Party)
				{
					OutError = TEXT("A pending Hunter one-shot payload has no valid party owner or positive magnitude.");
					return false;
				}
			}
		}
		TSet<FString> HealerFormulaKeys;
		for (const FGameXXKHealerFormulaRuntime& Formula : Runtime.HealerFormulas)
		{
			const FGameXXKCardCombatUnit* FormulaOwner = FindCombatUnitById(Runtime.Units, Formula.OwnerUnitId);
			const FGameXXKCardDefinition* FormulaCard = FGameXXKCardCatalog::FindCardDefinition(Formula.SourceCardId);
			const bool bValidFormulaSource = FormulaCard
				&& ((FormulaCard->Owner == EGameXXKCardOwner::Profession
						&& FormulaCard->Role == EGameXXKCharacterRole::Healer)
					|| (FormulaCard->Owner == EGameXXKCardOwner::Hero
						&& FormulaCard->LinkedRole == EGameXXKCharacterRole::Healer));
			const FString FormulaKey = Formula.OwnerUnitId.ToString() + TEXT("|") + Formula.SourceCardId.ToString();
			TSet<FName> FormulaTriggeredUnits;
			bool bTriggeredUnitsValid = true;
			for (const FName TriggeredUnitId : Formula.TriggeredUnitIdsThisRound)
			{
				const FGameXXKCardCombatUnit* TriggeredUnit = FindCombatUnitById(Runtime.Units, TriggeredUnitId);
				const EGameXXKCardTargetSide ExpectedTriggeredSide = Formula.Kind == EGameXXKHealerFormulaKind::HeroFirstPartyHealthLossMedicine
					? EGameXXKCardTargetSide::Party
					: EGameXXKCardTargetSide::Enemy;
				if (TriggeredUnitId.IsNone() || FormulaTriggeredUnits.Contains(TriggeredUnitId)
					|| !TriggeredUnit || TriggeredUnit->Side != ExpectedTriggeredSide)
				{
					bTriggeredUnitsValid = false;
					break;
				}
				FormulaTriggeredUnits.Add(TriggeredUnitId);
			}
			if (Formula.OwnerUnitId.IsNone()
				|| Formula.SourceCardId.IsNone()
				|| Formula.Kind == EGameXXKHealerFormulaKind::None
				|| !FormulaOwner
				|| FormulaOwner->Side != EGameXXKCardTargetSide::Party
				|| !bValidFormulaSource
				|| FormulaCard->HealerRule.FormulaKind != Formula.Kind
				|| (Formula.SourceQuality != EGameXXKCardQuality::Invalid
					&& FGameXXKCombatScalingRules::GetQualityPercent(Formula.SourceQuality) <= 0)
				|| Formula.Progress < 0
				|| Formula.PhaseProgress < 0
				|| Formula.LastTriggeredRound < 0
				|| Formula.SecondaryLastTriggeredRound < 0
				|| Formula.UnitBudgetRound < 0
				|| Formula.LastTriggeredRound > Runtime.RoundNumber
				|| Formula.SecondaryLastTriggeredRound > Runtime.RoundNumber
				|| Formula.UnitBudgetRound > Runtime.RoundNumber
				|| !bTriggeredUnitsValid
				|| (Formula.Kind == EGameXXKHealerFormulaKind::HeroFirstPartyHealthLossMedicine
					&& Formula.TriggeredUnitIdsThisRound.Num() > 3)
				|| ((Formula.Kind != EGameXXKHealerFormulaKind::BleedPoisonMark
						&& Formula.Kind != EGameXXKHealerFormulaKind::HeroFirstPartyHealthLossMedicine)
					&& !Formula.TriggeredUnitIdsThisRound.IsEmpty())
				|| HealerFormulaKeys.Contains(FormulaKey))
			{
				OutError = TEXT("A Healer formula has invalid owner, catalog identity, progress, or duplicate state.");
				return false;
			}
			HealerFormulaKeys.Add(FormulaKey);
		}
		for (const TPair<FName, int32>& MedicineProgress : Runtime.MedicineGainRemainderByOwner)
		{
			const FGameXXKCardCombatUnit* MedicineOwner = FindCombatUnitById(Runtime.Units, MedicineProgress.Key);
			if (MedicineProgress.Key.IsNone()
				|| MedicineProgress.Value < 0
				|| MedicineProgress.Value >= 6
				|| !MedicineOwner
				|| MedicineOwner->Side != EGameXXKCardTargetSide::Party)
			{
				OutError = TEXT("A Medicine gain remainder has no valid party owner or is outside zero through five.");
				return false;
			}
		}
		for (const FGameXXKCardBattleModifierRuntime& Modifier : Runtime.Modifiers)
		{
			const FGameXXKCardCombatUnit* ModifierSource = FindCombatUnitById(Runtime.Units, Modifier.SourceUnitId);
			const FGameXXKCardEffectCondition& ModifierCondition = Modifier.Definition.Condition;
			if (Modifier.ModifierId.IsNone() || ModifierIds.Contains(Modifier.ModifierId)
				|| Modifier.SourceCardInstanceId.IsNone() || Modifier.SourceUnitId.IsNone()
				|| !ModifierSource
				|| Modifier.Definition.Trigger == EGameXXKCardBattleModifierTrigger::Invalid
				|| Modifier.Definition.EffectType == EGameXXKCardEffectType::Invalid)
			{
				OutError = TEXT("Card battle runtime contains an invalid persistent modifier.");
				return false;
			}
			if (!Modifier.RequiredPlayedCardInstanceId.IsNone())
			{
				const bool bEnergySurcharge = ModifierSource->Side == EGameXXKCardTargetSide::Enemy
					&& Modifier.Definition.EffectType == EGameXXKCardEffectType::ModifyEnergyCost
					&& Modifier.Definition.Magnitude == 1;
				const bool bSorcererManaDiscount = ModifierSource->Side == EGameXXKCardTargetSide::Party
					&& ModifierSource->Role == EGameXXKCharacterRole::Sorcerer
					&& Modifier.Definition.EffectType == EGameXXKCardEffectType::ModifyManaCost
					&& Modifier.Definition.Magnitude == -3;
				const bool bMalformedCommonState = Modifier.SourceCardInstanceId != Modifier.RequiredPlayedCardInstanceId
					|| !IsCurrentHandInstance(Runtime.Deck, Modifier.RequiredPlayedCardInstanceId)
					|| (Runtime.Phase != EGameXXKCardBattlePhase::Player && Runtime.Phase != EGameXXKCardBattlePhase::Victory)
					|| Modifier.Definition.Trigger != EGameXXKCardBattleModifierTrigger::OnCardPlayed
					|| Modifier.Definition.Target != EGameXXKCardEffectTarget::PlayedCard
					|| Modifier.Definition.RecipientScope != EGameXXKCardModifierRecipientScope::SharedDeck
					|| Modifier.Definition.RecipientTarget != EGameXXKCardEffectTarget::PlayedCard
					|| Modifier.Definition.RequiredTriggeredRole != EGameXXKCharacterRole::Invalid
					|| !Modifier.Definition.RequiredTriggeredOwnerId.IsNone()
					|| Modifier.Definition.Expiry != EGameXXKCardModifierExpiry::AfterTriggerCount
					|| Modifier.Definition.TriggeredAttackTargetScope != EGameXXKCardTriggeredAttackTargetScope::Invalid
					|| Modifier.Definition.Status != EGameXXKCardStatus::None
					|| Modifier.Definition.RemainingTriggers != 1
					|| Modifier.Definition.MinimumResult != 0
					|| !Modifier.Definition.bPersistent
					|| !Modifier.OriginalSelectedTargetUnitId.IsNone()
					|| !Modifier.RecipientUnitIds.IsEmpty()
					|| ModifierCondition.Type != EGameXXKCardEffectConditionType::None
					|| ModifierCondition.Status != EGameXXKCardStatus::None
					|| ModifierCondition.MinimumStatusStacks != 0
					|| ModifierCondition.MinimumArmor != 0
					|| ModifierCondition.HealthPercentThreshold != 0.0f
					|| ModifierCondition.Terrain != EGameXXKCardTerrain::Invalid
					|| ModifierCondition.AlternateTerrain != EGameXXKCardTerrain::Invalid
					|| ModifierCondition.bConsumeStatus
					|| ModifierCondition.MaxConsumedStatusStacks != 0
					|| ModifierCondition.bScaleMagnitudeByConsumedStacks
					|| ModifierCondition.bConsumeOwnerArmor
					|| ModifierCondition.MaxConsumedArmor != 0
					|| ModifierCondition.bNegate;
				if (bMalformedCommonState || (!bEnergySurcharge && !bSorcererManaDiscount))
				{
					OutError = TEXT("A hand-bound card-cost modifier is malformed or no longer bound to its exact current hand instance.");
					return false;
				}
				if (bEnergySurcharge && ++HandBoundEnergySurchargeCount > 1)
				{
					OutError = TEXT("Card battle runtime cannot stack multiple hand-bound energy surcharge modifiers.");
					return false;
				}
				if (bSorcererManaDiscount
					&& HandBoundSorcererManaDiscountInstanceIds.Contains(Modifier.RequiredPlayedCardInstanceId))
				{
					OutError = TEXT("Card battle runtime cannot stack multiple Sorcerer Mana discounts on the same hand instance.");
					return false;
				}
				if (bSorcererManaDiscount)
				{
					HandBoundSorcererManaDiscountInstanceIds.Add(Modifier.RequiredPlayedCardInstanceId);
				}
			}
			ModifierIds.Add(Modifier.ModifierId);
		}
		if (!ValidatePartyReactions(Runtime, OutError))
		{
			return false;
		}
		TSet<FName> ArmorRetentionUnitIds;
		for (const FName UnitId : Runtime.RetainArmorAtNextPartyPhaseUnitIds)
		{
			const FGameXXKCardCombatUnit* Unit = FindCombatUnitById(Runtime.Units, UnitId);
			if (UnitId.IsNone()
				|| ArmorRetentionUnitIds.Contains(UnitId)
				|| !Unit
				|| Unit->Side != EGameXXKCardTargetSide::Party)
			{
				OutError = TEXT("Next-round armor retention contains an invalid, duplicate, or non-party unit.");
				return false;
			}
			ArmorRetentionUnitIds.Add(UnitId);
		}
		if (Runtime.PendingNextPlayerHandEnergySurcharge > 0)
		{
			if (Runtime.Phase != EGameXXKCardBattlePhase::Enemy)
			{
				OutError = TEXT("A pending next-hand energy surcharge can only persist during the enemy phase.");
				return false;
			}
			const FGameXXKCardCombatUnit* PendingSource = FindCombatUnitById(Runtime.Units, Runtime.PendingNextPlayerHandEnergySurchargeSourceUnitId);
			if (!PendingSource || PendingSource->Side != EGameXXKCardTargetSide::Enemy)
			{
				OutError = TEXT("A pending next-hand energy surcharge requires a stable enemy source.");
				return false;
			}
		}
		TSet<FString> EquipmentEffectKeys;
		for (const FGameXXKEquipmentBattleEffectRuntime& EffectRuntime : Runtime.EquipmentEffects)
		{
			const FGameXXKCardCombatUnit* Source = FindCombatUnitById(Runtime.Units, EffectRuntime.SourceCharacterId);
			const FGameXXKEquipmentActiveEffect& Effect = EffectRuntime.ActiveEffect;
			const FString Key = Effect.EffectId.ToString() + TEXT("|") + EffectRuntime.SourceCharacterId.ToString();
			if (EffectRuntime.SourceCharacterId.IsNone()
				|| Effect.EffectId.IsNone()
				|| Effect.SourceCharacterId != EffectRuntime.SourceCharacterId
				|| !Source
				|| Source->Side != EGameXXKCardTargetSide::Party
				|| Source->Role == EGameXXKCharacterRole::QuestNpc
				|| Effect.Set == EGameXXKEquipmentSet::Invalid
				|| Effect.Scope == EGameXXKEquipmentSetBonusScope::Invalid
				|| Effect.Hook == EGameXXKEquipmentSetBonusHook::Invalid
				|| Effect.ModifierKind == EGameXXKEquipmentModifierKind::Invalid
				|| Effect.Unit == EGameXXKEquipmentMagnitudeUnit::Invalid
				|| Effect.Magnitude <= 0
				|| !FGameXXKEquipmentRules::IsKnownActiveEffect(Effect)
				|| EffectRuntime.CurrentRoundTriggerCount < 0
				|| EffectRuntime.LastTriggerRound < 0
				|| EffectRuntime.LastTriggerRound > Runtime.RoundNumber
				|| EquipmentEffectKeys.Contains(Key))
			{
				OutError = TEXT("Card battle runtime contains an invalid equipment effect descriptor.");
				return false;
			}
			if (!ValidatePoJunEquipmentRuntime(Runtime, EffectRuntime, OutError))
			{
				return false;
			}
			EquipmentEffectKeys.Add(Key);
		}
		if (!bHasParty || !bHasEnemy)
		{
			OutError = TEXT("Card battle runtime must retain at least one party and one enemy record.");
			return false;
		}
		if (!ValidateHeroSpellTaskRuntime(Runtime, OutError)
			|| !ValidateSorcererPartnerTaskRuntimes(Runtime, OutError)
			|| !ValidateTaskNpcSpellTaskRuntime(Runtime, OutError)
			|| !ValidateAutomaticResolutionQueue(Runtime, OutError))
		{
			return false;
		}
		return true;
	}

	bool IsConditionSatisfied(
		const FGameXXKCardEffectCondition& Condition,
		const FGameXXKCardBattleRuntime& Runtime,
		const FGameXXKCardCombatUnit& Owner,
		const FGameXXKCardCombatUnit* Target,
		const FGameXXKCardPlayConditionSnapshot* Snapshot,
		bool& OutSatisfied,
		FString& OutError);

	bool BuildEffectiveCardEnergyCost(
		const FGameXXKCardBattleRuntime& Runtime,
		const FGameXXKCardDefinition& Definition,
		const FGameXXKCardInstance& Instance,
		const FGameXXKCardCombatUnit& Owner,
		int32& OutEnergyCost,
		TArray<FName>* OutAppliedModifierIds,
		TArray<FName>* OutTerrainFreeUnitIds,
		TArray<FName>* OutTerrainReductionUnitIds,
		FString& OutError);

	bool IsHealerFormulaOpen(
		const FGameXXKCardBattleRuntime& Runtime,
		FName OwnerUnitId,
		FName CardId);

	bool BuildEffectiveCardManaCost(
		const FGameXXKCardBattleRuntime& Runtime,
		const FGameXXKCardDefinition& Definition,
		const FGameXXKCardInstance& Instance,
		const FGameXXKCardCombatUnit& Owner,
		int32& OutManaCost,
		TArray<FName>* OutAppliedModifierIds,
		FString& OutError);

	bool ConsumeOnCardPlayedModifiers(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const TArray<FName>& ModifierIds,
		FString& OutError);

	bool BuildWidenedActiveCardDefinition(
		const FGameXXKCardBattleRuntime& Runtime,
		const FGameXXKCardDefinition& Definition,
		const FGameXXKCardInstance& Instance,
		const FGameXXKCardCombatUnit& Owner,
		FGameXXKCardDefinition& OutDefinition,
		TArray<FName>* OutAppliedModifierIds,
		FString& OutError)
	{
		OutError.Reset();
		OutDefinition = Definition;
		if (OutAppliedModifierIds)
		{
			OutAppliedModifierIds->Reset();
		}

		EGameXXKCardTargetMode WidenedMode = EGameXXKCardTargetMode::Invalid;
		EGameXXKCardEffectTarget WidenedEffectTarget = EGameXXKCardEffectTarget::Invalid;
		switch (Definition.TargetSpec.Mode)
		{
		case EGameXXKCardTargetMode::SingleEnemy:
			WidenedMode = EGameXXKCardTargetMode::AllEnemies;
			WidenedEffectTarget = EGameXXKCardEffectTarget::AllEnemies;
			break;
		case EGameXXKCardTargetMode::SingleAlly:
			WidenedMode = EGameXXKCardTargetMode::AllAllies;
			WidenedEffectTarget = EGameXXKCardEffectTarget::AllAllies;
			break;
		case EGameXXKCardTargetMode::OtherAlly:
			WidenedMode = EGameXXKCardTargetMode::AllOtherAllies;
			WidenedEffectTarget = EGameXXKCardEffectTarget::AllOtherAllies;
			break;
		default:
			return true;
		}

		TArray<FName> AppliedModifierIds;
		for (const FGameXXKCardBattleModifierRuntime& Modifier : Runtime.Modifiers)
		{
			const FGameXXKCardBattleModifier& ModifierDefinition = Modifier.Definition;
			if (ModifierDefinition.Trigger != EGameXXKCardBattleModifierTrigger::BeforeNextActiveCard
				|| ModifierDefinition.EffectType != EGameXXKCardEffectType::WidenNextActiveSingleTarget)
			{
				continue;
			}
			if (ModifierDefinition.Target != EGameXXKCardEffectTarget::PlayedCard
				|| ModifierDefinition.Magnitude != 1
				|| ModifierDefinition.RemainingTriggers <= 0
				|| ModifierDefinition.bExcludeSourceUnit && Modifier.SourceUnitId == Instance.OwnerUnitId
				|| ModifierDefinition.RecipientScope != EGameXXKCardModifierRecipientScope::SharedDeck
					&& !Modifier.RecipientUnitIds.Contains(Instance.OwnerUnitId)
				|| ModifierDefinition.RequiredTriggeredRole != EGameXXKCharacterRole::Invalid
					&& ModifierDefinition.RequiredTriggeredRole != Owner.Role
				|| !ModifierDefinition.RequiredTriggeredOwnerId.IsNone()
					&& ModifierDefinition.RequiredTriggeredOwnerId != Definition.OwnerId)
			{
				continue;
			}
			if (ModifierDefinition.Condition.bConsumeStatus || ModifierDefinition.Condition.bConsumeOwnerArmor)
			{
				OutError = TEXT("A target-widening modifier cannot consume combat state during preview.");
				return false;
			}
			bool bConditionSatisfied = false;
			if (!IsConditionSatisfied(ModifierDefinition.Condition, Runtime, Owner, nullptr, nullptr, bConditionSatisfied, OutError))
			{
				return false;
			}
			if (bConditionSatisfied)
			{
				AppliedModifierIds.Add(Modifier.ModifierId);
			}
		}
		if (AppliedModifierIds.IsEmpty())
		{
			return true;
		}

		OutDefinition.TargetSpec.Mode = WidenedMode;
		OutDefinition.TargetSpec.Presentation = PresentationForTargetMode(WidenedMode);
		OutDefinition.TargetSpec.bRequireDifferentFromOwner = TargetModeRequiresDifferentFromOwner(WidenedMode);
		OutDefinition.TargetSpec.ModeOverrides.Reset();
		for (FGameXXKCardEffect& Effect : OutDefinition.Effects)
		{
			if (Effect.Target == EGameXXKCardEffectTarget::SelectedTarget)
			{
				Effect.Target = WidenedEffectTarget;
			}
			if (Effect.Type == EGameXXKCardEffectType::ApplyBattleModifier
				&& Effect.Modifier.RecipientTarget == EGameXXKCardEffectTarget::SelectedTarget)
			{
				Effect.Modifier.RecipientTarget = WidenedEffectTarget;
			}
			if (Effect.Type == EGameXXKCardEffectType::ApplyGuardLink
				&& Effect.GuardLink.Guardian == EGameXXKCardEffectTarget::SelectedTarget)
			{
				Effect.GuardLink.Guardian = WidenedEffectTarget;
			}
		}
		if (OutAppliedModifierIds)
		{
			*OutAppliedModifierIds = MoveTemp(AppliedModifierIds);
		}
		return true;
	}

	bool BuildCardPlayPreviewInternal(
		const FGameXXKCardBattleRuntime& Runtime,
		const FName CardInstanceId,
		FGameXXKCardPlayPreview& OutPreview,
		FString& OutError)
	{
		OutError.Reset();
		if (!ValidateCardBattleRuntimeInternal(Runtime, OutError))
		{
			return false;
		}
		if (Runtime.Phase != EGameXXKCardBattlePhase::Player)
		{
			OutError = TEXT("Cards can only be played during the player phase.");
			return false;
		}
		if (IsActiveChoice(Runtime.Deck.PendingChoice.Kind) || Runtime.AutomaticResolutionQueue.bActive)
		{
			OutError = TEXT("A pending card choice or automatic card queue must resolve before another card can be played.");
			return false;
		}
		if (CardInstanceId.IsNone())
		{
			OutError = TEXT("Card play requires a stable hand instance ID.");
			return false;
		}

		EGameXXKCardZone Zone = EGameXXKCardZone::Invalid;
		const FGameXXKCardInstance* Instance = GameXXKCardRules::FindInstance(Runtime.Deck, CardInstanceId, Zone);
		if (!Instance || Zone != EGameXXKCardZone::Hand)
		{
			OutError = TEXT("The requested card instance is not in the current hand.");
			return false;
		}
		const FGameXXKCardDefinition* BaseDefinition = FGameXXKCardCatalog::FindCardDefinition(Instance->CardId);
		if (!BaseDefinition)
		{
			OutError = TEXT("The requested hand card has no catalog definition.");
			return false;
		}
		const FGameXXKCardDefinition QualityEffectiveDefinition = FGameXXKCardQualityRules::BuildEffectiveDefinition(
			*BaseDefinition,
			Instance->CurrentQuality);
		const FGameXXKCardCombatUnit* Owner = FindCombatUnitById(Runtime.Units, Instance->OwnerUnitId);
		if (!Owner || !Owner->bLiving)
		{
			OutError = TEXT("The card owner is absent or defeated.");
			return false;
		}
		FGameXXKCardDefinition EffectiveDefinition;
		if (!BuildWidenedActiveCardDefinition(Runtime, QualityEffectiveDefinition, *Instance, *Owner, EffectiveDefinition, nullptr, OutError))
		{
			return false;
		}
		if (EffectiveDefinition.EnergyCost < 0 || EffectiveDefinition.ManaCost < 0)
		{
			OutError = TEXT("The requested hand card has invalid resource costs.");
			return false;
		}
		int32 EffectiveManaCost = EffectiveDefinition.ManaCost;
		if (!BuildEffectiveCardManaCost(Runtime, EffectiveDefinition, *Instance, *Owner, EffectiveManaCost, nullptr, OutError))
		{
			return false;
		}
		int32 EffectiveEnergyCost = EffectiveDefinition.EnergyCost;
		if (!BuildEffectiveCardEnergyCost(Runtime, EffectiveDefinition, *Instance, *Owner, EffectiveEnergyCost, nullptr, nullptr, nullptr, OutError))
		{
			return false;
		}
		if (Runtime.Deck.SharedEnergy < EffectiveEnergyCost || Owner->Mana < EffectiveManaCost)
		{
			OutError = TEXT("The card owner does not have enough shared energy or mana.");
			return false;
		}

		FGameXXKCardPlayPreview NewPreview;
		NewPreview.CardInstanceId = Instance->InstanceId;
		NewPreview.CardId = Instance->CardId;
		NewPreview.OwnerUnitId = Instance->OwnerUnitId;
		NewPreview.EffectiveEnergyCost = EffectiveEnergyCost;
		NewPreview.EffectiveManaCost = EffectiveManaCost;
		if (!GameXXKCardRules::BuildTargetRequest(EffectiveDefinition, *Instance, Runtime.Terrain, BuildTargetUnitView(Runtime.Units), NewPreview.TargetRequest, &OutError))
		{
			return false;
		}
		NewPreview.bCanPlay = true;
		OutPreview = MoveTemp(NewPreview);
		return true;
	}

	bool IsDamageOverTimeStatus(const EGameXXKCardStatus Status)
	{
		return Status == EGameXXKCardStatus::Bleed
			|| Status == EGameXXKCardStatus::Poison
			|| Status == EGameXXKCardStatus::Burn
			|| Status == EGameXXKCardStatus::DamageOverTime;
	}

	bool IsConditionSatisfied(
		const FGameXXKCardEffectCondition& Condition,
		const FGameXXKCardBattleRuntime& Runtime,
		const FGameXXKCardCombatUnit& Owner,
		const FGameXXKCardCombatUnit* Target,
		const FGameXXKCardPlayConditionSnapshot* Snapshot,
		bool& OutSatisfied,
		FString& OutError)
	{
		OutError.Reset();
		bool bConditionValue = false;
		switch (Condition.Type)
		{
		case EGameXXKCardEffectConditionType::None:
			bConditionValue = true;
			break;
		case EGameXXKCardEffectConditionType::TargetHasStatus:
			bConditionValue = GetConditionStatusStacks(Target, Condition.Status, Snapshot) >= Condition.MinimumStatusStacks;
			break;
		case EGameXXKCardEffectConditionType::TargetHasAnyDamageOverTime:
			bConditionValue = Target && (GetCombatStatusStacksInternal(*Target, EGameXXKCardStatus::Bleed) > 0
				|| GetCombatStatusStacksInternal(*Target, EGameXXKCardStatus::Poison) > 0
				|| GetCombatStatusStacksInternal(*Target, EGameXXKCardStatus::Burn) > 0
				|| GetCombatStatusStacksInternal(*Target, EGameXXKCardStatus::DamageOverTime) > 0);
			break;
		case EGameXXKCardEffectConditionType::OwnerHasStatus:
			bConditionValue = GetCombatStatusStacksInternal(Owner, Condition.Status) >= Condition.MinimumStatusStacks;
			break;
		case EGameXXKCardEffectConditionType::OwnerArmorAtLeast:
			bConditionValue = Owner.Armor >= Condition.MinimumArmor;
			break;
		case EGameXXKCardEffectConditionType::OwnerHealthBelowPercent:
			bConditionValue = static_cast<int64>(Owner.HP) * 100 < static_cast<int64>(Owner.MaxHP) * Condition.HealthPercentThreshold;
			break;
		case EGameXXKCardEffectConditionType::TargetHealthBelowPercent:
			bConditionValue = Target && static_cast<int64>(Target->HP) * 100 < static_cast<int64>(Target->MaxHP) * Condition.HealthPercentThreshold;
			break;
		case EGameXXKCardEffectConditionType::TerrainIsAny:
			bConditionValue = Runtime.Terrain == Condition.Terrain || Runtime.Terrain == Condition.AlternateTerrain;
			break;
		case EGameXXKCardEffectConditionType::OwnerHasDamageOverTime:
			bConditionValue = GetCombatStatusStacksInternal(Owner, EGameXXKCardStatus::Bleed) > 0
				|| GetCombatStatusStacksInternal(Owner, EGameXXKCardStatus::Poison) > 0
				|| GetCombatStatusStacksInternal(Owner, EGameXXKCardStatus::Burn) > 0
				|| GetCombatStatusStacksInternal(Owner, EGameXXKCardStatus::DamageOverTime) > 0;
			break;
		case EGameXXKCardEffectConditionType::TargetIsAlly:
			bConditionValue = Target && Target->Side == Owner.Side;
			break;
		case EGameXXKCardEffectConditionType::TargetIsEnemy:
			bConditionValue = Target && Target->Side != Owner.Side;
			break;
		default:
			OutError = TEXT("Card effect has an invalid condition type.");
			return false;
		}
		OutSatisfied = Condition.bNegate ? !bConditionValue : bConditionValue;
		return true;
	}

	bool IsTerrainConditionDefinitelyFalse(
		const FGameXXKCardEffectCondition& Condition,
		const EGameXXKCardTerrain CurrentTerrain)
	{
		if (Condition.Type != EGameXXKCardEffectConditionType::TerrainIsAny)
		{
			return false;
		}
		const bool bMatches = CurrentTerrain == Condition.Terrain
			|| CurrentTerrain == Condition.AlternateTerrain;
		return Condition.bNegate ? bMatches : !bMatches;
	}

	bool DoesOnCardPlayedModifierApply(
		const FGameXXKCardBattleModifierRuntime& Modifier,
		const FGameXXKCardBattleRuntime& Runtime,
		const FGameXXKCardDefinition& PlayedDefinition,
		const FGameXXKCardInstance& PlayedInstance,
		const FGameXXKCardCombatUnit& PlayedOwner,
		bool& OutApplies,
		FString& OutError)
	{
		OutApplies = false;
		OutError.Reset();
		const FGameXXKCardBattleModifier& ModifierDefinition = Modifier.Definition;
		const bool bCostTrigger = ModifierDefinition.Trigger == EGameXXKCardBattleModifierTrigger::OnCardPlayed
			|| ModifierDefinition.Trigger == EGameXXKCardBattleModifierTrigger::BeforeNextActiveCard
			|| ModifierDefinition.Trigger == EGameXXKCardBattleModifierTrigger::BeforeFirstActiveCardNextPlayerRound;
		const bool bCostEffect = ModifierDefinition.EffectType == EGameXXKCardEffectType::ModifyEnergyCost
			|| ModifierDefinition.EffectType == EGameXXKCardEffectType::ModifyManaCost;
		if (!bCostTrigger || !bCostEffect)
		{
			return true;
		}
		if (ModifierDefinition.Target != EGameXXKCardEffectTarget::PlayedCard)
		{
			OutError = TEXT("A card-play cost modifier must explicitly target the played card.");
			return false;
		}
		if (!Modifier.RequiredPlayedCardInstanceId.IsNone()
			&& Modifier.RequiredPlayedCardInstanceId != PlayedInstance.InstanceId)
		{
			return true;
		}
		if (ModifierDefinition.bExcludeSourceUnit
			&& Modifier.SourceUnitId == PlayedInstance.OwnerUnitId)
		{
			return true;
		}
		if (ModifierDefinition.RecipientScope != EGameXXKCardModifierRecipientScope::SharedDeck
			&& !Modifier.RecipientUnitIds.Contains(PlayedInstance.OwnerUnitId))
		{
			return true;
		}
		if (ModifierDefinition.RequiredTriggeredRole != EGameXXKCharacterRole::Invalid
			&& ModifierDefinition.RequiredTriggeredRole != PlayedOwner.Role)
		{
			return true;
		}
		if (!ModifierDefinition.RequiredTriggeredOwnerId.IsNone()
			&& ModifierDefinition.RequiredTriggeredOwnerId != PlayedDefinition.OwnerId)
		{
			return true;
		}
		if (ModifierDefinition.Condition.bConsumeStatus || ModifierDefinition.Condition.bConsumeOwnerArmor)
		{
			OutError = TEXT("A card-play cost modifier cannot consume combat state during a non-mutating preview.");
			return false;
		}
		bool bConditionSatisfied = false;
		if (!IsConditionSatisfied(ModifierDefinition.Condition, Runtime, PlayedOwner, nullptr, nullptr, bConditionSatisfied, OutError))
		{
			return false;
		}
		OutApplies = bConditionSatisfied;
		return true;
	}

	bool IsRouteTerrainCard(const FGameXXKCardDefinition& Definition)
	{
		return Definition.Owner == EGameXXKCardOwner::Route
			&& Definition.AcquisitionKey == TEXT("Route.Terrain");
	}

	void CollectTerrainCardCostStatusOwners(
		const FGameXXKCardBattleRuntime& Runtime,
		const FGameXXKCardDefinition& Definition,
		const FGameXXKCardCombatUnit& Owner,
		TArray<FName>& OutFreeUnitIds,
		TArray<FName>& OutReductionUnitIds)
	{
		OutFreeUnitIds.Reset();
		OutReductionUnitIds.Reset();
		if (!IsRouteTerrainCard(Definition))
		{
			return;
		}
		TArray<const FGameXXKCardCombatUnit*> OrderedAllies;
		for (const FGameXXKCardCombatUnit& Candidate : Runtime.Units)
		{
			if (Candidate.bLiving && Candidate.Side == Owner.Side)
			{
				OrderedAllies.Add(&Candidate);
			}
		}
		OrderedAllies.Sort([](const FGameXXKCardCombatUnit& Left, const FGameXXKCardCombatUnit& Right)
		{
			return IsStableUnitOrderBefore(Left, Right);
		});
		for (const FGameXXKCardCombatUnit* Candidate : OrderedAllies)
		{
			if (GetCombatStatusStacksInternal(*Candidate, EGameXXKCardStatus::NextTerrainCardFree) > 0)
			{
				OutFreeUnitIds.Add(Candidate->UnitId);
				return;
			}
		}
		for (const FGameXXKCardCombatUnit* Candidate : OrderedAllies)
		{
			if (GetCombatStatusStacksInternal(*Candidate, EGameXXKCardStatus::NextTerrainCardEnergyReduction) > 0)
			{
				OutReductionUnitIds.Add(Candidate->UnitId);
			}
		}
	}

	bool ConsumeTerrainCardCostStatuses(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const TArray<FName>& FreeUnitIds,
		const TArray<FName>& ReductionUnitIds,
		FString& OutError)
	{
		OutError.Reset();
		TSet<FName> SeenFreeIds;
		for (const FName UnitId : FreeUnitIds)
		{
			FGameXXKCardCombatUnit* Unit = FindCombatUnitById(InOutRuntime.Units, UnitId);
			if (UnitId.IsNone() || SeenFreeIds.Contains(UnitId) || !Unit || !Unit->bLiving
				|| GameXXKCardRules::ConsumeCombatStatus(*Unit, EGameXXKCardStatus::NextTerrainCardFree, 1) != 1)
			{
				OutError = TEXT("A terrain-card free-cost status changed before the card could commit.");
				return false;
			}
			SeenFreeIds.Add(UnitId);
		}
		TSet<FName> SeenReductionIds;
		for (const FName UnitId : ReductionUnitIds)
		{
			FGameXXKCardCombatUnit* Unit = FindCombatUnitById(InOutRuntime.Units, UnitId);
			if (UnitId.IsNone() || SeenReductionIds.Contains(UnitId) || !Unit || !Unit->bLiving
				|| GameXXKCardRules::ConsumeCombatStatus(*Unit, EGameXXKCardStatus::NextTerrainCardEnergyReduction, 1) != 1)
			{
				OutError = TEXT("A terrain-card energy-reduction status changed before the card could commit.");
				return false;
			}
			SeenReductionIds.Add(UnitId);
		}
		return true;
	}

	bool BuildEffectiveCardEnergyCost(
		const FGameXXKCardBattleRuntime& Runtime,
		const FGameXXKCardDefinition& Definition,
		const FGameXXKCardInstance& Instance,
		const FGameXXKCardCombatUnit& Owner,
		int32& OutEnergyCost,
		TArray<FName>* OutAppliedModifierIds,
		TArray<FName>* OutTerrainFreeUnitIds,
		TArray<FName>* OutTerrainReductionUnitIds,
		FString& OutError)
	{
		OutError.Reset();
		if (Definition.EnergyCost < 0)
		{
			OutError = TEXT("Card energy cost cannot be negative.");
			return false;
		}
		if (OutAppliedModifierIds)
		{
			OutAppliedModifierIds->Reset();
		}
		TArray<FName> TerrainFreeUnitIds;
		TArray<FName> TerrainReductionUnitIds;
		CollectTerrainCardCostStatusOwners(Runtime, Definition, Owner, TerrainFreeUnitIds, TerrainReductionUnitIds);
		if (OutTerrainFreeUnitIds)
		{
			*OutTerrainFreeUnitIds = TerrainFreeUnitIds;
		}
		if (OutTerrainReductionUnitIds)
		{
			*OutTerrainReductionUnitIds = TerrainReductionUnitIds;
		}
		int64 EffectiveCost = Instance.EnergyCostOverride == INDEX_NONE
			? Definition.EnergyCost
			: Instance.EnergyCostOverride;
		if (Definition.HealerRule.FormulaKind != EGameXXKHealerFormulaKind::None
			&& !IsHealerFormulaOpen(Runtime, Owner.UnitId, Definition.Id))
		{
			EffectiveCost += Definition.HealerRule.UnopenedFormulaEnergySurcharge;
		}
		for (const FGameXXKCardBattleModifierRuntime& Modifier : Runtime.Modifiers)
		{
			if (Modifier.Definition.EffectType != EGameXXKCardEffectType::ModifyEnergyCost)
			{
				continue;
			}
			bool bApplies = false;
			if (!DoesOnCardPlayedModifierApply(Modifier, Runtime, Definition, Instance, Owner, bApplies, OutError))
			{
				return false;
			}
			if (!bApplies)
			{
				continue;
			}
			EffectiveCost += Modifier.Definition.Magnitude;
			if (OutAppliedModifierIds)
			{
				OutAppliedModifierIds->Add(Modifier.ModifierId);
			}
		}
		EffectiveCost -= TerrainReductionUnitIds.Num();
		EffectiveCost -= Runtime.PendingBladeCharge.Rule == EGameXXKBladeChargeRule::LightLoad ? 1 : 0;
		EffectiveCost -= CountEligibleBladeStyleRules(Runtime, EGameXXKBladeChargeRule::LightLoad);
		OutEnergyCost = Runtime.PendingBladeCharge.Rule == EGameXXKBladeChargeRule::MakeNextActiveEnergyFree
			|| CountEligibleBladeStyleRules(Runtime, EGameXXKBladeChargeRule::MakeNextActiveEnergyFree) > 0
			? 0
			: !TerrainFreeUnitIds.IsEmpty() || EffectiveCost <= 0
			? 0
			: EffectiveCost > MAX_int32
				? MAX_int32
				: static_cast<int32>(EffectiveCost);
		return true;
	}

	bool BuildEffectiveCardManaCost(
		const FGameXXKCardBattleRuntime& Runtime,
		const FGameXXKCardDefinition& Definition,
		const FGameXXKCardInstance& Instance,
		const FGameXXKCardCombatUnit& Owner,
		int32& OutManaCost,
		TArray<FName>* OutAppliedModifierIds,
		FString& OutError)
	{
		OutError.Reset();
		if (Definition.ManaCost < 0)
		{
			OutError = TEXT("Card mana cost cannot be negative.");
			return false;
		}
		if (OutAppliedModifierIds)
		{
			OutAppliedModifierIds->Reset();
		}
		int64 EffectiveCost = Instance.ManaCostOverride == INDEX_NONE
			? Definition.ManaCost
			: Instance.ManaCostOverride;
		for (const FGameXXKCardBattleModifierRuntime& Modifier : Runtime.Modifiers)
		{
			if (Modifier.Definition.EffectType != EGameXXKCardEffectType::ModifyManaCost)
			{
				continue;
			}
			bool bApplies = false;
			if (!DoesOnCardPlayedModifierApply(Modifier, Runtime, Definition, Instance, Owner, bApplies, OutError))
			{
				return false;
			}
			if (!bApplies)
			{
				continue;
			}
			EffectiveCost += Modifier.Definition.Magnitude;
			if (OutAppliedModifierIds)
			{
				OutAppliedModifierIds->Add(Modifier.ModifierId);
			}
		}
		OutManaCost = Runtime.PendingBladeCharge.Rule == EGameXXKBladeChargeRule::MakeNextActiveManaFree
			|| CountEligibleBladeStyleRules(Runtime, EGameXXKBladeChargeRule::MakeNextActiveManaFree) > 0
			? 0
			: EffectiveCost <= 0
				? 0
				: EffectiveCost > MAX_int32
					? MAX_int32
					: static_cast<int32>(EffectiveCost);
		return true;
	}

	bool ConsumeOnCardPlayedModifiers(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const TArray<FName>& ModifierIds,
		FString& OutError)
	{
		OutError.Reset();
		TSet<FName> RequestedIds;
		for (const FName ModifierId : ModifierIds)
		{
			if (ModifierId.IsNone() || RequestedIds.Contains(ModifierId))
			{
				OutError = TEXT("Applied card-play modifier IDs must be unique and non-empty.");
				return false;
			}
			RequestedIds.Add(ModifierId);
		}
		for (int32 Index = InOutRuntime.Modifiers.Num() - 1; Index >= 0; --Index)
		{
			FGameXXKCardBattleModifierRuntime& Modifier = InOutRuntime.Modifiers[Index];
			if (!RequestedIds.Contains(Modifier.ModifierId))
			{
				continue;
			}
			FGameXXKCardBattleModifier& ModifierDefinition = Modifier.Definition;
			const bool bCostTrigger = ModifierDefinition.Trigger == EGameXXKCardBattleModifierTrigger::OnCardPlayed
				|| ModifierDefinition.Trigger == EGameXXKCardBattleModifierTrigger::BeforeNextActiveCard
				|| ModifierDefinition.Trigger == EGameXXKCardBattleModifierTrigger::BeforeFirstActiveCardNextPlayerRound;
			const bool bCostEffect = ModifierDefinition.EffectType == EGameXXKCardEffectType::ModifyEnergyCost
				|| ModifierDefinition.EffectType == EGameXXKCardEffectType::ModifyManaCost;
			if (!bCostTrigger || !bCostEffect)
			{
				OutError = TEXT("A non-cost modifier was selected for card-play consumption.");
				return false;
			}
			switch (ModifierDefinition.Expiry)
			{
			case EGameXXKCardModifierExpiry::AfterTriggerCount:
				if (ModifierDefinition.RemainingTriggers <= 0)
				{
					OutError = TEXT("A trigger-count cost modifier has no remaining uses.");
					return false;
				}
				--ModifierDefinition.RemainingTriggers;
				if (ModifierDefinition.RemainingTriggers == 0)
				{
					InOutRuntime.Modifiers.RemoveAt(Index, 1, EAllowShrinking::No);
				}
				break;
			case EGameXXKCardModifierExpiry::EndOfCurrentRound:
				break;
			case EGameXXKCardModifierExpiry::EndOfCurrentRoundOrTriggerCount:
				if (ModifierDefinition.RemainingTriggers > 0 && --ModifierDefinition.RemainingTriggers == 0)
				{
					InOutRuntime.Modifiers.RemoveAt(Index, 1, EAllowShrinking::No);
				}
				break;
			case EGameXXKCardModifierExpiry::Invalid:
			default:
				OutError = TEXT("A card-play cost modifier has an invalid expiry policy.");
				return false;
			}
		}
		return true;
	}

	bool MaterializePendingNextPlayerHandEnergySurcharge(
		FGameXXKCardBattleRuntime& InOutRuntime,
		FString& OutError)
	{
		OutError.Reset();
		const int32 SurchargeAmount = InOutRuntime.PendingNextPlayerHandEnergySurcharge;
		if (SurchargeAmount == 0)
		{
			return true;
		}
		if (InOutRuntime.Phase != EGameXXKCardBattlePhase::Player
			|| SurchargeAmount != 1
			|| InOutRuntime.PendingNextPlayerHandEnergySurchargeSourceUnitId.IsNone())
		{
			OutError = TEXT("A pending next-hand energy surcharge cannot be materialized from this card-battle state.");
			return false;
		}

		FGameXXKCardBattleRuntime PreviewRuntime = InOutRuntime;
		PreviewRuntime.PendingNextPlayerHandEnergySurcharge = 0;
		PreviewRuntime.PendingNextPlayerHandEnergySurchargeSourceUnitId = NAME_None;
		const FGameXXKCardInstance* SelectedInstance = nullptr;
		int32 SelectedNormalEnergyCost = INDEX_NONE;
		for (const FGameXXKCardInstance& Candidate : InOutRuntime.Deck.Hand)
		{
			FGameXXKCardPlayPreview CandidatePreview;
			FString CandidateError;
			if (!BuildCardPlayPreviewInternal(PreviewRuntime, Candidate.InstanceId, CandidatePreview, CandidateError))
			{
				continue;
			}
			const bool bIsBetterCandidate = !SelectedInstance
				|| CandidatePreview.EffectiveEnergyCost > SelectedNormalEnergyCost
				|| (CandidatePreview.EffectiveEnergyCost == SelectedNormalEnergyCost
					&& (Candidate.AcquisitionOrdinal < SelectedInstance->AcquisitionOrdinal
						|| (Candidate.AcquisitionOrdinal == SelectedInstance->AcquisitionOrdinal
							&& Candidate.InstanceId.LexicalLess(SelectedInstance->InstanceId))));
			if (bIsBetterCandidate)
			{
				SelectedInstance = &Candidate;
				SelectedNormalEnergyCost = CandidatePreview.EffectiveEnergyCost;
			}
		}

		if (!SelectedInstance)
		{
			InOutRuntime.PendingNextPlayerHandEnergySurcharge = 0;
			InOutRuntime.PendingNextPlayerHandEnergySurchargeSourceUnitId = NAME_None;
			return true;
		}

		TSet<FName> ExistingModifierIds;
		for (const FGameXXKCardBattleModifierRuntime& ExistingModifier : InOutRuntime.Modifiers)
		{
			ExistingModifierIds.Add(ExistingModifier.ModifierId);
		}
		FName ModifierId = NAME_None;
		while (ModifierId.IsNone() || ExistingModifierIds.Contains(ModifierId))
		{
			if (InOutRuntime.NextModifierOrdinal == MAX_int32)
			{
				OutError = TEXT("Battle modifier ordinal has exhausted the supported range.");
				return false;
			}
			ModifierId = FName(*FString::Printf(TEXT("Modifier.%d"), InOutRuntime.NextModifierOrdinal++));
		}

		FGameXXKCardBattleModifierRuntime& NewModifier = InOutRuntime.Modifiers.AddDefaulted_GetRef();
		NewModifier.ModifierId = ModifierId;
		NewModifier.RequiredPlayedCardInstanceId = SelectedInstance->InstanceId;
		NewModifier.SourceCardInstanceId = SelectedInstance->InstanceId;
		NewModifier.SourceUnitId = InOutRuntime.PendingNextPlayerHandEnergySurchargeSourceUnitId;
		NewModifier.Definition.Trigger = EGameXXKCardBattleModifierTrigger::OnCardPlayed;
		NewModifier.Definition.EffectType = EGameXXKCardEffectType::ModifyEnergyCost;
		NewModifier.Definition.Target = EGameXXKCardEffectTarget::PlayedCard;
		NewModifier.Definition.RecipientScope = EGameXXKCardModifierRecipientScope::SharedDeck;
		NewModifier.Definition.RecipientTarget = EGameXXKCardEffectTarget::PlayedCard;
		NewModifier.Definition.Expiry = EGameXXKCardModifierExpiry::AfterTriggerCount;
		NewModifier.Definition.Magnitude = SurchargeAmount;
		NewModifier.Definition.RemainingTriggers = 1;
		NewModifier.Definition.bPersistent = true;
		InOutRuntime.PendingNextPlayerHandEnergySurcharge = 0;
		InOutRuntime.PendingNextPlayerHandEnergySurchargeSourceUnitId = NAME_None;
		return true;
	}

	void RemoveHandBoundEnergySurchargesOutsideCurrentHand(FGameXXKCardBattleRuntime& InOutRuntime)
	{
		InOutRuntime.Modifiers.RemoveAll([&InOutRuntime](const FGameXXKCardBattleModifierRuntime& Modifier)
		{
			return !Modifier.RequiredPlayedCardInstanceId.IsNone()
				&& !IsCurrentHandInstance(InOutRuntime.Deck, Modifier.RequiredPlayedCardInstanceId);
		});
	}

	bool ResolveEffectTargetIds(
		const FGameXXKCardBattleRuntime& Runtime,
		const FName OwnerUnitId,
		const TArray<FName>& CardTargetIds,
		const EGameXXKCardEffectTarget EffectTarget,
		TArray<FName>& OutTargetIds,
		FString& OutError)
	{
		OutError.Reset();
		const FGameXXKCardCombatUnit* Owner = FindCombatUnitById(Runtime.Units, OwnerUnitId);
		if (!Owner || !Owner->bLiving)
		{
			OutError = TEXT("Card effect owner is absent or defeated.");
			return false;
		}
		TArray<const FGameXXKCardCombatUnit*> Candidates;
		for (const FGameXXKCardCombatUnit& Unit : Runtime.Units)
		{
			if (Unit.bLiving)
			{
				Candidates.Add(&Unit);
			}
		}
		Candidates.Sort([](const FGameXXKCardCombatUnit& Left, const FGameXXKCardCombatUnit& Right)
		{
			return IsStableUnitOrderBefore(Left, Right);
		});

		TArray<FName> NewTargetIds;
		switch (EffectTarget)
		{
		case EGameXXKCardEffectTarget::CardOwner:
			NewTargetIds.Add(OwnerUnitId);
			break;
		case EGameXXKCardEffectTarget::SelectedTarget:
		{
			const FGameXXKCardCombatUnit* SelectedTarget = CardTargetIds.Num() == 1
				? FindCombatUnitById(Runtime.Units, CardTargetIds[0])
				: nullptr;
			if (!SelectedTarget || !SelectedTarget->bLiving)
			{
				OutError = TEXT("Selected-target effect has no current living stable target.");
				return false;
			}
			NewTargetIds.Add(CardTargetIds[0]);
			break;
		}
		case EGameXXKCardEffectTarget::SelectedTargetSide:
		{
			const FGameXXKCardCombatUnit* SelectedTarget = CardTargetIds.Num() == 1
				? FindCombatUnitById(Runtime.Units, CardTargetIds[0])
				: nullptr;
			if (!SelectedTarget)
			{
				OutError = TEXT("Selected-side effect has no stable target anchor.");
				return false;
			}
			for (const FGameXXKCardCombatUnit* Candidate : Candidates)
			{
				if (Candidate->Side == SelectedTarget->Side)
				{
					NewTargetIds.Add(Candidate->UnitId);
				}
			}
			break;
		}
		case EGameXXKCardEffectTarget::AllEnemies:
			for (const FGameXXKCardCombatUnit* Candidate : Candidates)
			{
				if (Candidate->Side != Owner->Side)
				{
					NewTargetIds.Add(Candidate->UnitId);
				}
			}
			break;
		case EGameXXKCardEffectTarget::AllAllies:
		case EGameXXKCardEffectTarget::EachLivingAlly:
			for (const FGameXXKCardCombatUnit* Candidate : Candidates)
			{
				if (Candidate->Side == Owner->Side)
				{
					NewTargetIds.Add(Candidate->UnitId);
				}
			}
			break;
		case EGameXXKCardEffectTarget::AllOtherAllies:
			for (const FGameXXKCardCombatUnit* Candidate : Candidates)
			{
				if (Candidate->Side == Owner->Side && Candidate->UnitId != OwnerUnitId)
				{
					NewTargetIds.Add(Candidate->UnitId);
				}
			}
			break;
		case EGameXXKCardEffectTarget::LowestHealthAlly:
		case EGameXXKCardEffectTarget::LowestHealthOtherAlly:
		{
			const FGameXXKCardCombatUnit* Lowest = nullptr;
			for (const FGameXXKCardCombatUnit* Candidate : Candidates)
			{
				if (Candidate->Side != Owner->Side || (EffectTarget == EGameXXKCardEffectTarget::LowestHealthOtherAlly && Candidate->UnitId == OwnerUnitId))
				{
					continue;
				}
				if (!Lowest || static_cast<int64>(Candidate->HP) * Lowest->MaxHP < static_cast<int64>(Lowest->HP) * Candidate->MaxHP
					|| (static_cast<int64>(Candidate->HP) * Lowest->MaxHP == static_cast<int64>(Lowest->HP) * Candidate->MaxHP && IsStableUnitOrderBefore(*Candidate, *Lowest)))
				{
					Lowest = Candidate;
				}
			}
			if (Lowest)
			{
				NewTargetIds.Add(Lowest->UnitId);
			}
			break;
		}
		case EGameXXKCardEffectTarget::HighestArmorAlly:
		{
			const FGameXXKCardCombatUnit* HighestArmor = nullptr;
			for (const FGameXXKCardCombatUnit* Candidate : Candidates)
			{
				if (Candidate->Side != Owner->Side)
				{
					continue;
				}
				if (!HighestArmor
					|| Candidate->Armor > HighestArmor->Armor
					|| (Candidate->Armor == HighestArmor->Armor && IsStableUnitOrderBefore(*Candidate, *HighestArmor)))
				{
					HighestArmor = Candidate;
				}
			}
			if (HighestArmor)
			{
				NewTargetIds.Add(HighestArmor->UnitId);
			}
			break;
		}
		case EGameXXKCardEffectTarget::HighestAttackAlly:
		{
			const FGameXXKCardCombatUnit* HighestAttack = nullptr;
			for (const FGameXXKCardCombatUnit* Candidate : Candidates)
			{
				if (Candidate->Side != Owner->Side)
				{
					continue;
				}
				if (!HighestAttack
					|| Candidate->Attack > HighestAttack->Attack
					|| (Candidate->Attack == HighestAttack->Attack && IsStableUnitOrderBefore(*Candidate, *HighestAttack)))
				{
					HighestAttack = Candidate;
				}
			}
			if (HighestAttack)
			{
				NewTargetIds.Add(HighestAttack->UnitId);
			}
			break;
		}
		case EGameXXKCardEffectTarget::PriorityEnemy:
		{
			const FGameXXKCardCombatUnit* PriorityEnemy = nullptr;
			for (const FGameXXKCardCombatUnit* Candidate : Candidates)
			{
				if (Candidate->Side == Owner->Side)
				{
					continue;
				}
				const int32 CandidateMark = GameXXKCardRules::GetCombatStatusStacks(*Candidate, EGameXXKCardStatus::Mark);
				const int32 PriorityMark = PriorityEnemy
					? GameXXKCardRules::GetCombatStatusStacks(*PriorityEnemy, EGameXXKCardStatus::Mark)
					: INDEX_NONE;
				const bool bLowerHealthRatio = PriorityEnemy
					&& static_cast<int64>(Candidate->HP) * PriorityEnemy->MaxHP
						< static_cast<int64>(PriorityEnemy->HP) * Candidate->MaxHP;
				const bool bSameHealthRatio = PriorityEnemy
					&& static_cast<int64>(Candidate->HP) * PriorityEnemy->MaxHP
						== static_cast<int64>(PriorityEnemy->HP) * Candidate->MaxHP;
				if (!PriorityEnemy
					|| CandidateMark > PriorityMark
					|| (CandidateMark == PriorityMark && bLowerHealthRatio)
					|| (CandidateMark == PriorityMark && bSameHealthRatio && IsStableUnitOrderBefore(*Candidate, *PriorityEnemy)))
				{
					PriorityEnemy = Candidate;
				}
			}
			if (PriorityEnemy)
			{
				NewTargetIds.Add(PriorityEnemy->UnitId);
			}
			break;
		}
		case EGameXXKCardEffectTarget::Attacker:
		case EGameXXKCardEffectTarget::PlayedCard:
		case EGameXXKCardEffectTarget::Invalid:
		default:
			OutError = TEXT("This effect target requires a later reactive card-effect planner.");
			return false;
		}

		OutTargetIds = MoveTemp(NewTargetIds);
		return true;
	}

	bool ResolveModifierRecipientIds(
		const FGameXXKCardBattleRuntime& Runtime,
		const FGameXXKCardInstance& Instance,
		const TArray<FName>& CardTargetIds,
		const FGameXXKCardBattleModifier& Modifier,
		TArray<FName>& OutRecipientUnitIds,
		FString& OutError)
	{
		OutError.Reset();
		OutRecipientUnitIds.Reset();
		if (Modifier.RecipientScope == EGameXXKCardModifierRecipientScope::SharedDeck)
		{
			if (Modifier.RecipientTarget != EGameXXKCardEffectTarget::PlayedCard)
			{
				OutError = TEXT("A shared-deck modifier must explicitly identify the played-card recipient scope.");
				return false;
			}
			return true;
		}
		if (Modifier.RecipientScope == EGameXXKCardModifierRecipientScope::Invalid)
		{
			OutError = TEXT("A persistent modifier has no recipient scope.");
			return false;
		}
		if (!ResolveEffectTargetIds(Runtime, Instance.OwnerUnitId, CardTargetIds, Modifier.RecipientTarget, OutRecipientUnitIds, OutError))
		{
			return false;
		}
		return true;
	}

	bool RegisterBattleModifier(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FGameXXKCardInstance& Instance,
		const TArray<FName>& CardTargetIds,
		const FGameXXKCardBattleModifier& ModifierDefinition,
		FString& OutError)
	{
		OutError.Reset();
		if (!ModifierDefinition.bPersistent
			|| ModifierDefinition.Trigger == EGameXXKCardBattleModifierTrigger::Invalid
			|| ModifierDefinition.EffectType == EGameXXKCardEffectType::Invalid
			|| ModifierDefinition.Expiry == EGameXXKCardModifierExpiry::Invalid)
		{
			OutError = TEXT("A battle modifier must have a persistent trigger, effect, and expiry policy.");
			return false;
		}
		TArray<FName> RecipientUnitIds;
		if (!ResolveModifierRecipientIds(InOutRuntime, Instance, CardTargetIds, ModifierDefinition, RecipientUnitIds, OutError))
		{
			return false;
		}
		TSet<FName> ExistingModifierIds;
		for (const FGameXXKCardBattleModifierRuntime& ExistingModifier : InOutRuntime.Modifiers)
		{
			ExistingModifierIds.Add(ExistingModifier.ModifierId);
		}
		FName NewModifierId = NAME_None;
		while (NewModifierId.IsNone() || ExistingModifierIds.Contains(NewModifierId))
		{
			if (InOutRuntime.NextModifierOrdinal == MAX_int32)
			{
				OutError = TEXT("Battle modifier ordinal has exhausted the supported range.");
				return false;
			}
			NewModifierId = FName(*FString::Printf(TEXT("Modifier.%d"), InOutRuntime.NextModifierOrdinal++));
		}
		FGameXXKCardBattleModifierRuntime& NewModifier = InOutRuntime.Modifiers.AddDefaulted_GetRef();
		NewModifier.ModifierId = NewModifierId;
		NewModifier.SourceCardInstanceId = Instance.InstanceId;
		NewModifier.SourceUnitId = Instance.OwnerUnitId;
		NewModifier.OriginalSelectedTargetUnitId = CardTargetIds.Num() == 1 ? CardTargetIds[0] : NAME_None;
		NewModifier.RecipientUnitIds = MoveTemp(RecipientUnitIds);
		NewModifier.Definition = ModifierDefinition;
		NewModifier.SourceCardSnapshot.CardId = Instance.CardId;
		NewModifier.SourceCardSnapshot.Quality = Instance.CurrentQuality;
		NewModifier.SourceCardSnapshot.OwnerUnitId = Instance.OwnerUnitId;
		NewModifier.SourceCardSnapshot.OriginalTargetUnitIds = CardTargetIds;
		return true;
	}

	bool ResolveEffectSourceUnitId(
		const FGameXXKCardBattleRuntime& Runtime,
		const FName OwnerUnitId,
		const TArray<FName>& CardTargetIds,
		const EGameXXKCardEffectSource Source,
		FName& OutSourceUnitId,
		FString& OutError)
	{
		OutSourceUnitId = NAME_None;
		switch (Source)
		{
		case EGameXXKCardEffectSource::CardOwner:
			OutSourceUnitId = OwnerUnitId;
			break;
		case EGameXXKCardEffectSource::SelectedTarget:
			if (CardTargetIds.Num() == 1)
			{
				OutSourceUnitId = CardTargetIds[0];
			}
			break;
		case EGameXXKCardEffectSource::HighestArmorAlly:
		{
			TArray<FName> SourceIds;
			if (!ResolveEffectTargetIds(
				Runtime,
				OwnerUnitId,
				CardTargetIds,
				EGameXXKCardEffectTarget::HighestArmorAlly,
				SourceIds,
				OutError))
			{
				return false;
			}
			if (SourceIds.Num() == 1)
			{
				OutSourceUnitId = SourceIds[0];
			}
			break;
		}
		case EGameXXKCardEffectSource::HighestAttackAlly:
		{
			TArray<FName> SourceIds;
			if (!ResolveEffectTargetIds(
				Runtime,
				OwnerUnitId,
				CardTargetIds,
				EGameXXKCardEffectTarget::HighestAttackAlly,
				SourceIds,
				OutError))
			{
				return false;
			}
			if (SourceIds.Num() == 1)
			{
				OutSourceUnitId = SourceIds[0];
			}
			break;
		}
		case EGameXXKCardEffectSource::Invalid:
		default:
			break;
		}

		const FGameXXKCardCombatUnit* Owner = FindCombatUnitById(Runtime.Units, OwnerUnitId);
		const FGameXXKCardCombatUnit* SourceUnit = FindCombatUnitById(Runtime.Units, OutSourceUnitId);
		if (!Owner || !SourceUnit || !SourceUnit->bLiving || SourceUnit->Side != Owner->Side)
		{
			OutError = TEXT("A sourced card effect requires one living ally of the card owner.");
			return false;
		}
		return true;
	}

	int32 ResolveDefensePercentArmorAmount(
		const FGameXXKCardCombatUnit& Caster,
		const int32 DefensePercent,
		const EGameXXKCardQuality Quality)
	{
		const int32 QualityPercent = FGameXXKCombatScalingRules::GetQualityPercent(Quality);
		if (!Caster.bLiving || Caster.Defense <= 0 || DefensePercent <= 0 || QualityPercent <= 0)
		{
			return 0;
		}
		const int64 Numerator = static_cast<int64>(Caster.Defense)
			* static_cast<int64>(DefensePercent)
			* static_cast<int64>(QualityPercent);
		return static_cast<int32>(FMath::Min<int64>(MAX_int32, (Numerator + 9999) / 10000));
	}

	int32 ResolveCardStatusApplicationAmount(
		const FGameXXKCardBattleRuntime& Runtime,
		const FGameXXKCardCombatUnit& Target,
		const FGameXXKCardEffect& Effect,
		const EGameXXKCardQuality Quality)
	{
		if (Effect.MagnitudePolicy != EGameXXKCardMagnitudePolicy::DotCoefficient)
		{
			return Effect.Magnitude;
		}
		const int32 Cap = FGameXXKCombatScalingRules::ResolveDotCap(Runtime.TeamMaxLevelSnapshot);
		const int32 Current = GameXXKCardRules::GetCombatStatusStacks(Target, Effect.Status);
		const int32 Available = FMath::Max(0, Cap - FMath::Min(Current, Cap));
		const int32 Resolved = FGameXXKCombatScalingRules::ResolveDotAddition(
			Effect.Magnitude,
			Quality,
			Runtime.TeamMaxLevelSnapshot);
		return FMath::Min(Available, Resolved);
	}

	bool GrantStatusFromCardEffect(
		FGameXXKCardBattleRuntime& InOutRuntime,
		FGameXXKCardCombatUnit& InOutTarget,
		const EGameXXKCardStatus Status,
		const int32 Magnitude,
		FString& OutError,
		FGameXXKCardPlayResult* InOutResult = nullptr,
		const FName SourceUnitId = NAME_None,
		const bool bAllowHealerFormulaProgress = true)
	{
		const int32 Applied = GameXXKCardRules::AddCombatStatus(InOutTarget, Status, Magnitude);
		if (Applied > 0
			&& !ResolveWhiteApeStatusGuardAfterStatusAppliedInternal(
				InOutRuntime,
				InOutTarget,
				SourceUnitId,
				InOutResult,
				&OutError))
		{
			return false;
		}
		if (Status == EGameXXKCardStatus::Medicine && Applied > 0)
		{
			int32& Remainder = InOutRuntime.MedicineGainRemainderByOwner.FindOrAdd(InOutTarget.UnitId);
			const int64 TotalProgress = static_cast<int64>(Remainder) + Applied;
			const int32 CompletedSixes = static_cast<int32>(TotalProgress / 6);
			Remainder = static_cast<int32>(TotalProgress % 6);
			if (CompletedSixes > 0)
			{
				GameXXKCardRules::AddCombatStatus(InOutTarget, EGameXXKCardStatus::Momentum, CompletedSixes);
			}

			if (!bAllowHealerFormulaProgress) return true;

			for (FGameXXKHealerFormulaRuntime& Formula : InOutRuntime.HealerFormulas)
			{
				if (Formula.OwnerUnitId != InOutTarget.UnitId
					|| Formula.Kind != EGameXXKHealerFormulaKind::HighEnergyAndSixMedicine)
				{
					continue;
				}
				const bool bEnemyPhase = InOutRuntime.Phase == EGameXXKCardBattlePhase::Enemy;
				if (Formula.bProgressFromEnemyPhase != bEnemyPhase)
				{
					Formula.bProgressFromEnemyPhase = bEnemyPhase;
					Formula.PhaseProgress = 0;
				}
				Formula.PhaseProgress = FMath::Min(MAX_int32, Formula.PhaseProgress + Applied);
				if (Formula.PhaseProgress < 6)
				{
					continue;
				}
				if (!bEnemyPhase && Formula.SecondaryLastTriggeredRound != InOutRuntime.RoundNumber)
				{
					Formula.SecondaryLastTriggeredRound = InOutRuntime.RoundNumber;
					InOutRuntime.Deck.SharedEnergy = FMath::Min(MaxCardBattleEnergy, InOutRuntime.Deck.SharedEnergy + 1);
				}
				else if (bEnemyPhase && Formula.UnitBudgetRound != InOutRuntime.RoundNumber)
				{
					Formula.UnitBudgetRound = InOutRuntime.RoundNumber;
					InOutRuntime.PendingNextRoundEnergyBonus = FMath::Min(MaxDeferredPhaseEnergy, InOutRuntime.PendingNextRoundEnergyBonus + 1);
				}
			}
		}
		return true;
	}

	bool GrantStatusFromHealerFormula(
		FGameXXKCardBattleRuntime& Runtime, FGameXXKCardCombatUnit& Target, EGameXXKCardStatus Status,
		int32 Magnitude, FString& OutError, FGameXXKCardPlayResult* Result = nullptr, FName SourceUnitId = NAME_None)
	{
		return GrantStatusFromCardEffect(Runtime, Target, Status, Magnitude, OutError, Result, SourceUnitId, false);
	}

	EGameXXKCardQuality ResolveHealerFormulaQuality(const FGameXXKHealerFormulaRuntime& Formula)
	{
		return Formula.SourceQuality == EGameXXKCardQuality::Invalid
			? FGameXXKCardQualityRules::GetCardBaseQuality(Formula.SourceCardId) : Formula.SourceQuality;
	}

	bool IsHealerFormulaOpen(
		const FGameXXKCardBattleRuntime& Runtime,
		const FName OwnerUnitId,
		const FName CardId)
	{
		return Runtime.HealerFormulas.ContainsByPredicate([OwnerUnitId, CardId](const FGameXXKHealerFormulaRuntime& Formula)
		{
			return Formula.OwnerUnitId == OwnerUnitId && Formula.SourceCardId == CardId;
		});
	}

	bool IsShiGuThreeDotStatus(const EGameXXKCardStatus Status)
	{
		return Status == EGameXXKCardStatus::Bleed
			|| Status == EGameXXKCardStatus::Poison
			|| Status == EGameXXKCardStatus::Burn;
	}

	int32 CountShiGuDotTypes(const FGameXXKCardCombatUnit& Unit)
	{
		int32 Count = 0;
		for (const EGameXXKCardStatus Status : {
			EGameXXKCardStatus::Bleed,
			EGameXXKCardStatus::Poison,
			EGameXXKCardStatus::Burn})
		{
			Count += GameXXKCardRules::GetCombatStatusStacks(Unit, Status) > 0 ? 1 : 0;
		}
		return Count;
	}

	bool ResolveShiGuAfterActiveDotApplication(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const TArray<FGameXXKCardCombatUnit>& UnitsBeforeActiveCard,
		const FName CardOwnerUnitId,
		FGameXXKCardPlayResult& InOutResult,
		FString& OutError)
	{
		int32 TwoPieceIndex = INDEX_NONE;
		int32 FourPieceIndex = INDEX_NONE;
		for (int32 EffectIndex = 0; EffectIndex < InOutRuntime.EquipmentEffects.Num(); ++EffectIndex)
		{
			const FGameXXKEquipmentBattleEffectRuntime& EffectRuntime = InOutRuntime.EquipmentEffects[EffectIndex];
			const FGameXXKEquipmentActiveEffect& Effect = EffectRuntime.ActiveEffect;
			if (Effect.Set != EGameXXKEquipmentSet::ShiGu
				|| Effect.Scope != EGameXXKEquipmentSetBonusScope::Owner
				|| EffectRuntime.SourceCharacterId != CardOwnerUnitId)
			{
				continue;
			}
			if (Effect.RequiredPieces == 2 && Effect.Hook == EGameXXKEquipmentSetBonusHook::ShiGuDotApplied)
			{
				TwoPieceIndex = EffectIndex;
			}
			else if (Effect.RequiredPieces == 4 && Effect.Hook == EGameXXKEquipmentSetBonusHook::ShiGuDualDotEstablished)
			{
				FourPieceIndex = EffectIndex;
			}
		}
		if (TwoPieceIndex == INDEX_NONE && FourPieceIndex == INDEX_NONE)
		{
			return true;
		}

		TSet<FName> CandidateTargetSet;
		for (const FGameXXKCardStatusChangeResult& Change : InOutResult.StatusChanges)
		{
			if (Change.AppliedStacks > 0 && IsShiGuThreeDotStatus(Change.Status))
			{
				CandidateTargetSet.Add(Change.TargetUnitId);
			}
		}
		for (const FGameXXKCardCombatUnit& Before : UnitsBeforeActiveCard)
		{
			const FGameXXKCardCombatUnit* After = FindCombatUnitById(InOutRuntime.Units, Before.UnitId);
			if (!After)
			{
				continue;
			}
			for (const EGameXXKCardStatus Status : {
				EGameXXKCardStatus::Bleed,
				EGameXXKCardStatus::Poison,
				EGameXXKCardStatus::Burn})
			{
				if (GameXXKCardRules::GetCombatStatusStacks(*After, Status)
					> GameXXKCardRules::GetCombatStatusStacks(Before, Status))
				{
					CandidateTargetSet.Add(Before.UnitId);
					break;
				}
			}
		}
		TArray<FName> CandidateTargetIds = CandidateTargetSet.Array();
		CandidateTargetIds.Sort([&InOutRuntime](const FName LeftId, const FName RightId)
		{
			const FGameXXKCardCombatUnit* Left = FindCombatUnitById(InOutRuntime.Units, LeftId);
			const FGameXXKCardCombatUnit* Right = FindCombatUnitById(InOutRuntime.Units, RightId);
			if (Left && Right && Left->StableSortOrder != Right->StableSortOrder)
			{
				return Left->StableSortOrder < Right->StableSortOrder;
			}
			return LeftId.LexicalLess(RightId);
		});

		if (TwoPieceIndex != INDEX_NONE)
		{
			const int32 RotStacks = InOutRuntime.EquipmentEffects[TwoPieceIndex].ActiveEffect.Magnitude;
			for (const FName TargetId : CandidateTargetIds)
			{
				FGameXXKCardCombatUnit* Target = FindCombatUnitById(InOutRuntime.Units, TargetId);
				const FGameXXKCardCombatUnit* Owner = FindCombatUnitById(InOutRuntime.Units, CardOwnerUnitId);
				if (!Target || !Target->bLiving || !Owner || Target->Side == Owner->Side)
				{
					continue;
				}
				const int32 Applied = GameXXKCardRules::AddCombatStatus(
					*Target,
					EGameXXKCardStatus::DamageOverTime,
					RotStacks);
				if (Applied > 0)
				{
					FGameXXKCardStatusChangeResult& Change = InOutResult.StatusChanges.AddDefaulted_GetRef();
					Change.TargetUnitId = TargetId;
					Change.Status = EGameXXKCardStatus::DamageOverTime;
					Change.AppliedStacks = Applied;
				}
			}
		}

		if (FourPieceIndex == INDEX_NONE)
		{
			return true;
		}
		const FGameXXKEquipmentBattleEffectRuntime& FourPieceBefore = InOutRuntime.EquipmentEffects[FourPieceIndex];
		const int32 TriggerCountThisRound = FourPieceBefore.LastTriggerRound == InOutRuntime.RoundNumber
			? FourPieceBefore.CurrentRoundTriggerCount
			: 0;
		if (TriggerCountThisRound >= FourPieceBefore.ActiveEffect.MaxTriggersPerRound)
		{
			return true;
		}
		for (const FName TargetId : CandidateTargetIds)
		{
			const FGameXXKCardCombatUnit* Target = FindCombatUnitById(InOutRuntime.Units, TargetId);
			if (!Target || !Target->bLiving || CountShiGuDotTypes(*Target) < 2)
			{
				continue;
			}
			TArray<FGameXXKCardDamageResult> ExplosionResults;
			if (!GameXXKCardRules::ResolveToxicExplosion(
				InOutRuntime,
				CardOwnerUnitId,
				TargetId,
				false,
				ExplosionResults,
				&OutError))
			{
				return false;
			}
			TSet<EGameXXKCardDamageCause> DistinctCauses;
			for (FGameXXKCardDamageResult& DamageResult : ExplosionResults)
			{
				DistinctCauses.Add(DamageResult.Cause);
				DamageResult.ResolutionOrigin = InOutResult.ResolutionOrigin;
				InOutResult.DamageResults.Add(MoveTemp(DamageResult));
			}
			InOutResult.ToxicExplosionDistinctDotTypeCounts.Add(DistinctCauses.Num());
			FGameXXKEquipmentBattleEffectRuntime& FourPieceAfter = InOutRuntime.EquipmentEffects[FourPieceIndex];
			FourPieceAfter.LastTriggerRound = InOutRuntime.RoundNumber;
			FourPieceAfter.CurrentRoundTriggerCount = 1;
			break;
		}
		return true;
	}

	bool DrawOrDeferTriggeredCards(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const int32 Count,
		FString& OutError)
	{
		if (Count <= 0)
		{
			OutError = TEXT("A triggered draw count must be positive.");
			return false;
		}
		if (IsActiveChoice(InOutRuntime.Deck.PendingChoice.Kind)
			|| InOutRuntime.AutomaticResolutionQueue.bActive)
		{
			if (InOutRuntime.PendingTriggeredDrawCount > MAX_int32 - Count)
			{
				OutError = TEXT("Deferred triggered draws exceed the supported range.");
				return false;
			}
			InOutRuntime.PendingTriggeredDrawCount += Count;
			return true;
		}
		GameXXKCardRules::RemoveDefeatedPartyOwnerCards(InOutRuntime.Deck, InOutRuntime.Units);
		return GameXXKCardRules::DrawCards(InOutRuntime.Deck, Count, 0, &OutError);
	}

	bool MaterializePendingTriggeredDraws(
		FGameXXKCardBattleRuntime& InOutRuntime,
		FString& OutError)
	{
		if (InOutRuntime.PendingTriggeredDrawCount <= 0
			|| IsActiveChoice(InOutRuntime.Deck.PendingChoice.Kind)
			|| InOutRuntime.AutomaticResolutionQueue.bActive)
		{
			return true;
		}
		const int32 Count = InOutRuntime.PendingTriggeredDrawCount;
		GameXXKCardRules::RemoveDefeatedPartyOwnerCards(InOutRuntime.Deck, InOutRuntime.Units);
		if (!GameXXKCardRules::DrawCards(InOutRuntime.Deck, Count, 0, &OutError))
		{
			return false;
		}
		InOutRuntime.PendingTriggeredDrawCount = 0;
		return true;
	}

	bool ResolveZhuiFengAfterActiveCard(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const int32 ActiveCardCountIncrement,
		FGameXXKCardPlayResult& InOutResult,
		FString& OutError)
	{
		if (ActiveCardCountIncrement <= 0)
		{
			OutError = TEXT("A ZhuiFeng active-card count increment must be positive.");
			return false;
		}
		FGameXXKEquipmentBattleEffectRuntime* ChosenEffect = nullptr;
		for (FGameXXKEquipmentBattleEffectRuntime& Candidate : InOutRuntime.EquipmentEffects)
		{
			const FGameXXKEquipmentActiveEffect& Effect = Candidate.ActiveEffect;
			if (Effect.Set != EGameXXKEquipmentSet::ZhuiFeng
				|| Effect.Hook != EGameXXKEquipmentSetBonusHook::ZhuiFengActiveCardCount
				|| Effect.Scope != EGameXXKEquipmentSetBonusScope::Team)
			{
				continue;
			}
			if (!ChosenEffect
				|| Effect.RequiredPieces > ChosenEffect->ActiveEffect.RequiredPieces
				|| (Effect.RequiredPieces == ChosenEffect->ActiveEffect.RequiredPieces
					&& Candidate.SourceCharacterId.ToString() < ChosenEffect->SourceCharacterId.ToString()))
			{
				ChosenEffect = &Candidate;
			}
		}
		if (!ChosenEffect)
		{
			return true;
		}

		if (ChosenEffect->LastTriggerRound != InOutRuntime.RoundNumber)
		{
			ChosenEffect->CurrentRoundTriggerCount = 0;
		}
		const int32 PreviousCount = ChosenEffect->CurrentRoundTriggerCount;
		if (PreviousCount > MAX_int32 - ActiveCardCountIncrement)
		{
			OutError = TEXT("The ZhuiFeng active-card counter has exhausted the supported range.");
			return false;
		}
		const int32 CurrentCount = PreviousCount + ActiveCardCountIncrement;
		ChosenEffect->CurrentRoundTriggerCount = CurrentCount;
		ChosenEffect->LastTriggerRound = InOutRuntime.RoundNumber;
		const FGameXXKEquipmentActiveEffect& Effect = ChosenEffect->ActiveEffect;
		const int32 PairRewards = CurrentCount / 2 - PreviousCount / 2;
		if (PairRewards > 0)
		{
			const int64 DrawCount = static_cast<int64>(PairRewards) * Effect.Magnitude;
			if (DrawCount > MAX_int32)
			{
				OutError = TEXT("The ZhuiFeng pair-draw reward exceeds the supported range.");
				return false;
			}
			if (!DrawOrDeferTriggeredCards(InOutRuntime, static_cast<int32>(DrawCount), OutError))
			{
				return false;
			}
		}
		if (Effect.RequiredPieces >= 4 && PreviousCount < 2 && CurrentCount >= 2)
		{
			InOutRuntime.Deck.SharedEnergy = FMath::Min(
				MaxCardBattleEnergy,
				InOutRuntime.Deck.SharedEnergy + Effect.Magnitude);
		}
		if (Effect.RequiredPieces >= 6 && PreviousCount < 4 && CurrentCount >= 4)
		{
			InOutRuntime.Deck.SharedEnergy = FMath::Min(
				MaxCardBattleEnergy,
				InOutRuntime.Deck.SharedEnergy + Effect.Magnitude);
			for (FGameXXKCardCombatUnit& Unit : InOutRuntime.Units)
			{
				if (!Unit.bLiving || Unit.Side != EGameXXKCardTargetSide::Party)
				{
					continue;
				}
				const int32 AppliedStacks = GameXXKCardRules::AddCombatStatus(
					Unit,
					EGameXXKCardStatus::Charge,
					Effect.Magnitude);
				if (AppliedStacks > 0)
				{
					FGameXXKCardStatusChangeResult& Change = InOutResult.StatusChanges.AddDefaulted_GetRef();
					Change.TargetUnitId = Unit.UnitId;
					Change.Status = EGameXXKCardStatus::Charge;
					Change.AppliedStacks = AppliedStacks;
				}
			}
			if (!DrawOrDeferTriggeredCards(InOutRuntime, Effect.Magnitude, OutError))
			{
				return false;
			}
		}
		return true;
	}

	bool ResolveQingNangAfterPaidHighCostActive(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const int32 PaidEnergyCost,
		FGameXXKCardPlayResult& InOutResult,
		FString& OutError)
	{
		if (PaidEnergyCost < 2)
		{
			return true;
		}
		FGameXXKEquipmentBattleEffectRuntime* ChosenEffect = nullptr;
		for (FGameXXKEquipmentBattleEffectRuntime& Candidate : InOutRuntime.EquipmentEffects)
		{
			const FGameXXKEquipmentActiveEffect& Effect = Candidate.ActiveEffect;
			if (Effect.Set != EGameXXKEquipmentSet::QingNang
				|| Effect.Hook != EGameXXKEquipmentSetBonusHook::QingNangHighCostActive
				|| Effect.Scope != EGameXXKEquipmentSetBonusScope::Team)
			{
				continue;
			}
			if (!ChosenEffect
				|| Effect.RequiredPieces > ChosenEffect->ActiveEffect.RequiredPieces
				|| (Effect.RequiredPieces == ChosenEffect->ActiveEffect.RequiredPieces
					&& Candidate.SourceCharacterId.ToString() < ChosenEffect->SourceCharacterId.ToString()))
			{
				ChosenEffect = &Candidate;
			}
		}
		if (!ChosenEffect)
		{
			return true;
		}
		const FGameXXKEquipmentActiveEffect& Effect = ChosenEffect->ActiveEffect;
		if (ChosenEffect->LastTriggerRound != InOutRuntime.RoundNumber)
		{
			ChosenEffect->CurrentRoundTriggerCount = 0;
		}
		if (ChosenEffect->CurrentRoundTriggerCount >= Effect.MaxTriggersPerRound)
		{
			return true;
		}

		if (!DrawOrDeferTriggeredCards(InOutRuntime, Effect.Magnitude, OutError))
		{
			return false;
		}
		if (Effect.RequiredPieces >= 4)
		{
			TArray<FName> AllyUnitIds;
			for (const FGameXXKCardCombatUnit& Ally : InOutRuntime.Units)
			{
				if (Ally.bLiving && Ally.Side == EGameXXKCardTargetSide::Party)
				{
					AllyUnitIds.Add(Ally.UnitId);
				}
			}
			for (const FName AllyUnitId : AllyUnitIds)
			{
				FGameXXKCardCombatUnit* Ally = FindCombatUnitById(InOutRuntime.Units, AllyUnitId);
				if (!Ally || !Ally->bLiving)
				{
					continue;
				}
				const int32 ActualLoss = FMath::Min(1, FMath::Max(0, Ally->HP - 1));
				if (ActualLoss > 0)
				{
					FGameXXKCardDamageContext Context;
					Context.SourceUnitId = AllyUnitId;
					Context.Kind = EGameXXKCardDamageKind::SelfHealthLoss;
					Context.ResolutionOrigin = EGameXXKCardResolutionOrigin::TaskReward;
					FGameXXKCardDamageResult DamageResult;
					if (!ApplyCombatDirectDamageInternal(
						InOutRuntime.Units,
						InOutRuntime.GuardLinks,
						Context,
						AllyUnitId,
						ActualLoss,
						DamageResult,
						nullptr,
						&InOutRuntime,
						&InOutResult,
						false,
						&OutError))
					{
						return false;
					}
					InOutResult.DamageResults.Add(MoveTemp(DamageResult));
				}
				FGameXXKCardCombatUnit* CurrentAlly = FindCombatUnitById(InOutRuntime.Units, AllyUnitId);
				if (CurrentAlly && CurrentAlly->bLiving)
				{
					ApplyAndRecordHealing(InOutResult, Effect.SourceCharacterId, *CurrentAlly, 2);
				}
			}
		}
		if (Effect.RequiredPieces >= 6)
		{
			InOutRuntime.Deck.SharedEnergy = FMath::Min(
				MaxCardBattleEnergy,
				InOutRuntime.Deck.SharedEnergy + Effect.Magnitude);
		}
		ChosenEffect->LastTriggerRound = InOutRuntime.RoundNumber;
		++ChosenEffect->CurrentRoundTriggerCount;
		return true;
	}

	bool IsMedicineReverseDamage(const FGameXXKCardDamageResult& Result, const FName OwnerUnitId)
	{
		return Result.SourceUnitId == OwnerUnitId
			&& Result.Cause == EGameXXKCardDamageCause::Medicine
			&& Result.HealthDamage > 0;
	}

	bool ResolveHeroFirstPartyHealthLossMedicine(
		FGameXXKCardBattleRuntime& InOutRuntime,
		FGameXXKHealerFormulaRuntime& InOutFormula,
		const TArray<FGameXXKCardDamageResult>& DamageResults,
		const int32 FirstDamageResultIndex,
		FGameXXKCardPlayResult* InOutResult,
		FString& OutError)
	{
		if (FirstDamageResultIndex < 0 || FirstDamageResultIndex > DamageResults.Num())
		{
			OutError = TEXT("A Hero health-loss formula received an invalid result range.");
			return false;
		}
		if (InOutFormula.UnitBudgetRound != InOutRuntime.RoundNumber)
		{
			InOutFormula.UnitBudgetRound = InOutRuntime.RoundNumber;
			InOutFormula.TriggeredUnitIdsThisRound.Reset();
		}
		int32 Granted = 0;
		for (int32 ResultIndex = FirstDamageResultIndex;
			ResultIndex < DamageResults.Num() && InOutFormula.TriggeredUnitIdsThisRound.Num() < 3;
			++ResultIndex)
		{
			const FGameXXKCardDamageResult& Damage = DamageResults[ResultIndex];
			const FGameXXKCardCombatUnit* Target = FindCombatUnitById(InOutRuntime.Units, Damage.ResolvedTargetUnitId);
			if (Damage.HealthDamage <= 0
				|| !Target
				|| Target->Side != EGameXXKCardTargetSide::Party
				|| InOutFormula.TriggeredUnitIdsThisRound.Contains(Damage.ResolvedTargetUnitId))
			{
				continue;
			}
			InOutFormula.TriggeredUnitIdsThisRound.Add(Damage.ResolvedTargetUnitId);
			++Granted;
		}
		FGameXXKCardCombatUnit* FormulaOwner = FindCombatUnitById(InOutRuntime.Units, InOutFormula.OwnerUnitId);
		return Granted <= 0
			|| (FormulaOwner
				&& FormulaOwner->bLiving
				&& GrantStatusFromHealerFormula(
					InOutRuntime,
					*FormulaOwner,
					EGameXXKCardStatus::Medicine,
					Granted,
					OutError,
					InOutResult,
					FormulaOwner->UnitId));
	}

	bool ResolveOpenedHealerFormulasAfterActiveCard(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const TArray<FGameXXKCardCombatUnit>& UnitsBeforeActiveCard,
		const int32 PreexistingFormulaCount,
		const int32 PaidEnergyCost,
		const FGameXXKResolvedCardSnapshot& PlayedSnapshot,
		FGameXXKCardPlayResult& InOutResult,
		FString& OutError)
	{
		// Every formula qualifies against the same completed action, before any formula output.
		const FGameXXKCardPlayResult TriggerResult = InOutResult;
		const TArray<FGameXXKCardCombatUnit> UnitsAfterActiveCard = InOutRuntime.Units;
		const auto FindAfterUnit = [&UnitsAfterActiveCard](FName Id) -> const FGameXXKCardCombatUnit*
		{
			return UnitsAfterActiveCard.FindByPredicate([Id](const FGameXXKCardCombatUnit& Unit) { return Unit.UnitId == Id; });
		};
		const int32 FormulaCount = FMath::Min(PreexistingFormulaCount, InOutRuntime.HealerFormulas.Num());
		const auto FindBeforeUnit = [&UnitsBeforeActiveCard](const FName UnitId) -> const FGameXXKCardCombatUnit*
		{
			return UnitsBeforeActiveCard.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Unit)
			{
				return Unit.UnitId == UnitId;
			});
		};
		const auto DrawFormulaCards = [&InOutRuntime, &OutError](const int32 Count) -> bool
		{
			if (Count <= 0)
			{
				return true;
			}
			GameXXKCardRules::RemoveDefeatedPartyOwnerCards(InOutRuntime.Deck, InOutRuntime.Units);
			return GameXXKCardRules::DrawCards(InOutRuntime.Deck, Count, 0, &OutError);
		};
		int32 HealthChangeCount = 0;
		TSet<FName> HealthChangedUnitIds;
		for (const FGameXXKCardDamageResult& DamageResult : TriggerResult.DamageResults)
		{
			if (DamageResult.HealthDamage > 0)
			{
				++HealthChangeCount;
				HealthChangedUnitIds.Add(DamageResult.ResolvedTargetUnitId);
			}
		}
		for (const FGameXXKCardHealingResult& HealingResult : TriggerResult.HealingResults)
		{
			if (HealingResult.EffectiveHealing > 0)
			{
				++HealthChangeCount;
				HealthChangedUnitIds.Add(HealingResult.TargetUnitId);
			}
		}
		int32 CleansedPartyDotTypes = 0;
		bool bPartyBleedRemoved = false;
		for (const FGameXXKCardCombatUnit& Before : UnitsBeforeActiveCard)
		{
			const FGameXXKCardCombatUnit* After = FindAfterUnit(Before.UnitId);
			if (!After || Before.Side != EGameXXKCardTargetSide::Party)
			{
				continue;
			}
			bPartyBleedRemoved |= GameXXKCardRules::GetCombatStatusStacks(Before, EGameXXKCardStatus::Bleed)
				> GameXXKCardRules::GetCombatStatusStacks(*After, EGameXXKCardStatus::Bleed);
			for (const EGameXXKCardStatus DotStatus : {
				EGameXXKCardStatus::Bleed,
				EGameXXKCardStatus::Poison,
				EGameXXKCardStatus::Burn, EGameXXKCardStatus::DamageOverTime})
			{
				CleansedPartyDotTypes += GameXXKCardRules::GetCombatStatusStacks(Before, DotStatus) > 0
					&& GameXXKCardRules::GetCombatStatusStacks(*After, DotStatus) == 0 ? 1 : 0;
			}
		}
		TSet<FName> PoisonedEnemyIds;
		TSet<FName> DebuffedEnemyIds;
		TSet<FName> VulnerabilityGainedEnemyIds;
		for (const FGameXXKCardStatusChangeResult& StatusChange : TriggerResult.StatusChanges)
		{
			const FGameXXKCardCombatUnit* Target = FindAfterUnit(StatusChange.TargetUnitId);
			if (!Target || Target->Side != EGameXXKCardTargetSide::Enemy || StatusChange.AppliedStacks <= 0)
			{
				continue;
			}
			DebuffedEnemyIds.Add(StatusChange.TargetUnitId);
			if (StatusChange.Status == EGameXXKCardStatus::Poison)
			{
				PoisonedEnemyIds.Add(StatusChange.TargetUnitId);
			}
			if (StatusChange.Status == EGameXXKCardStatus::Vulnerability)
			{
				VulnerabilityGainedEnemyIds.Add(StatusChange.TargetUnitId);
			}
		}
		// Attack-packet attachments are committed by the shared damage resolver and
		// therefore do not pass through the standalone ApplyStatus audit above.  A
		// formula reacts to the actual before/after debuff gain, regardless of which
		// supported effect path applied it.
		for (const FGameXXKCardCombatUnit& Before : UnitsBeforeActiveCard)
		{
			const FGameXXKCardCombatUnit* After = FindAfterUnit(Before.UnitId);
			if (!After || Before.Side != EGameXXKCardTargetSide::Enemy)
			{
				continue;
			}
			for (const EGameXXKCardStatus DebuffStatus : {
				EGameXXKCardStatus::Bleed,
				EGameXXKCardStatus::Poison,
				EGameXXKCardStatus::Burn,
				EGameXXKCardStatus::Vulnerability,
				EGameXXKCardStatus::Mark,
				EGameXXKCardStatus::Weak})
			{
				if (GameXXKCardRules::GetCombatStatusStacks(*After, DebuffStatus)
					> GameXXKCardRules::GetCombatStatusStacks(Before, DebuffStatus))
				{
					DebuffedEnemyIds.Add(Before.UnitId);
					break;
				}
			}
		}
		TSet<FName> DirectlyDamagedEnemyIds;
		int32 PoisonPacketCount = 0;
		int32 BleedPacketCount = 0;
		for (const FGameXXKCardDamageResult& DamageResult : TriggerResult.DamageResults)
		{
			const FGameXXKCardCombatUnit* Target = FindAfterUnit(DamageResult.ResolvedTargetUnitId);
			if (DamageResult.Cause == EGameXXKCardDamageCause::DirectAttack
				&& Target
				&& Target->Side == EGameXXKCardTargetSide::Enemy
				&& !DamageResult.bAvoidedByAgility
				&& (DamageResult.HealthDamage > 0 || DamageResult.ArmorAbsorbed > 0))
			{
				DirectlyDamagedEnemyIds.Add(DamageResult.ResolvedTargetUnitId);
			}
			PoisonPacketCount += DamageResult.HealthDamage > 0
				&& (DamageResult.Cause == EGameXXKCardDamageCause::Poison
					|| DamageResult.Cause == EGameXXKCardDamageCause::ToxicExplosionPoison) ? 1 : 0;
			BleedPacketCount += DamageResult.HealthDamage > 0
				&& (DamageResult.Cause == EGameXXKCardDamageCause::Bleed
					|| DamageResult.Cause == EGameXXKCardDamageCause::ToxicExplosionBleed) ? 1 : 0;
		}
		for (int32 FormulaIndex = 0; FormulaIndex < FormulaCount; ++FormulaIndex)
		{
			FGameXXKHealerFormulaRuntime& Formula = InOutRuntime.HealerFormulas[FormulaIndex];
			FGameXXKCardCombatUnit* FormulaOwner = FindCombatUnitById(InOutRuntime.Units, Formula.OwnerUnitId);
			if (!FormulaOwner || !FormulaOwner->bLiving)
			{
				continue;
			}
			const EGameXXKCardQuality FormulaQuality = ResolveHealerFormulaQuality(Formula);
			switch (Formula.Kind)
			{
			case EGameXXKHealerFormulaKind::AnyHealthChangeMedicine:
			{
				if (HealthChangeCount > 0
					&& !GrantStatusFromHealerFormula(
						InOutRuntime,
						*FormulaOwner,
						EGameXXKCardStatus::Medicine,
						HealthChangeCount,
						OutError,
						&InOutResult,
						FormulaOwner->UnitId))
				{
					return false;
				}
				break;
			}
			case EGameXXKHealerFormulaKind::HighEnergyAndSixMedicine:
				if (PaidEnergyCost >= 2 && Formula.LastTriggeredRound != InOutRuntime.RoundNumber)
				{
					const FName FormulaOwnerUnitId = FormulaOwner->UnitId;
					Formula.LastTriggeredRound = InOutRuntime.RoundNumber;
					TArray<FName> AllyUnitIds;
					for (const FGameXXKCardCombatUnit& Ally : InOutRuntime.Units)
					{
						if (Ally.bLiving && Ally.Side == FormulaOwner->Side)
						{
							AllyUnitIds.Add(Ally.UnitId);
						}
					}
					for (const FName AllyUnitId : AllyUnitIds)
					{
						FGameXXKCardCombatUnit* Ally = FindCombatUnitById(InOutRuntime.Units, AllyUnitId);
						if (!Ally || !Ally->bLiving)
						{
							continue;
						}
						const int32 ActualLoss = FMath::Min(1, FMath::Max(0, Ally->HP - 1));
						if (ActualLoss > 0)
						{
							FGameXXKCardDamageContext Context;
							Context.SourceUnitId = AllyUnitId;
							Context.Kind = EGameXXKCardDamageKind::SelfHealthLoss;
							Context.ResolutionOrigin = EGameXXKCardResolutionOrigin::TaskReward;
							FGameXXKCardDamageResult DamageResult;
							if (!ApplyCombatDirectDamageInternal(
								InOutRuntime.Units,
								InOutRuntime.GuardLinks,
								Context,
								AllyUnitId,
								ActualLoss,
								DamageResult,
								nullptr,
								&InOutRuntime,
								&InOutResult,
								false,
								&OutError))
							{
								return false;
							}
							InOutResult.DamageResults.Add(MoveTemp(DamageResult));
						}
						FGameXXKCardCombatUnit* CurrentAlly = FindCombatUnitById(InOutRuntime.Units, AllyUnitId);
						if (CurrentAlly && CurrentAlly->bLiving)
						{
							ApplyAndRecordHealing(InOutResult, FormulaOwnerUnitId, *CurrentAlly,
								FGameXXKCombatScalingRules::ResolveMedicineHealing(10, 0, FormulaQuality, InOutRuntime.TeamMaxLevelSnapshot));
						}
					}
				}
				break;
			case EGameXXKHealerFormulaKind::FirstHealingMedicine:
			{
				const bool bResolvedHealing = TriggerResult.HealingResults.ContainsByPredicate([](const FGameXXKCardHealingResult& Result)
				{
					return Result.EffectiveHealing > 0;
				}) || TriggerResult.DamageResults.ContainsByPredicate([&PlayedSnapshot](const FGameXXKCardDamageResult& Result)
				{
					return IsMedicineReverseDamage(Result, PlayedSnapshot.OwnerUnitId);
				});
				if (bResolvedHealing && Formula.LastTriggeredRound != InOutRuntime.RoundNumber)
				{
					Formula.LastTriggeredRound = InOutRuntime.RoundNumber;
					if (!GrantStatusFromHealerFormula(InOutRuntime, *FormulaOwner, EGameXXKCardStatus::Medicine, 2, OutError, &InOutResult, FormulaOwner->UnitId)) return false;
				}
				break;
			}
			case EGameXXKHealerFormulaKind::ThreeCleansedDotMedicine:
			{
				if (Formula.UnitBudgetRound != InOutRuntime.RoundNumber)
				{
					Formula.UnitBudgetRound = InOutRuntime.RoundNumber;
					Formula.Progress = 0;
				}
				const int32 Rewards = FMath::Min(CleansedPartyDotTypes, FMath::Max(0, 3 - Formula.Progress));
				Formula.Progress += Rewards;
				if (Rewards > 0 && !GrantStatusFromHealerFormula(InOutRuntime, *FormulaOwner, EGameXXKCardStatus::Medicine, Rewards, OutError, &InOutResult, FormulaOwner->UnitId)) return false;
				break;
			}
			case EGameXXKHealerFormulaKind::LowHealthCrossMedicine:
			case EGameXXKHealerFormulaKind::LowHealthCrossAgility:
			{
				const int32 Threshold = Formula.Kind == EGameXXKHealerFormulaKind::LowHealthCrossMedicine ? 35 : 30;
				FName CrossedUnitId = NAME_None;
				for (const FGameXXKCardCombatUnit& Before : UnitsBeforeActiveCard)
				{
					const FGameXXKCardCombatUnit* After = FindAfterUnit(Before.UnitId);
					if (Before.Side == EGameXXKCardTargetSide::Party
						&& After
						&& static_cast<int64>(Before.HP) * 100 >= static_cast<int64>(Before.MaxHP) * Threshold
						&& static_cast<int64>(After->HP) * 100 < static_cast<int64>(After->MaxHP) * Threshold)
					{
						CrossedUnitId = Before.UnitId;
						break;
					}
				}
				if (!CrossedUnitId.IsNone() && Formula.LastTriggeredRound != InOutRuntime.RoundNumber)
				{
					Formula.LastTriggeredRound = InOutRuntime.RoundNumber;
					if (Formula.Kind == EGameXXKHealerFormulaKind::LowHealthCrossMedicine)
					{
						if (!GrantStatusFromHealerFormula(InOutRuntime, *FormulaOwner, EGameXXKCardStatus::Medicine, 3, OutError, &InOutResult, FormulaOwner->UnitId)) return false;
					}
					else if (FGameXXKCardCombatUnit* CrossedUnit = FindCombatUnitById(InOutRuntime.Units, CrossedUnitId))
					{
						if (!GrantStatusFromHealerFormula(InOutRuntime, *CrossedUnit, EGameXXKCardStatus::Agility, 2, OutError, &InOutResult, FormulaOwner->UnitId)) return false;
					}
				}
				break;
			}
			case EGameXXKHealerFormulaKind::ThreeEffectiveHealsDraw:
			{
				if (Formula.UnitBudgetRound != InOutRuntime.RoundNumber)
				{
					Formula.UnitBudgetRound = InOutRuntime.RoundNumber;
					Formula.PhaseProgress = 0;
					Formula.Progress = 0;
				}
				int32 EffectivePartyHeals = 0;
				for (const FGameXXKCardHealingResult& Result : TriggerResult.HealingResults)
				{
					const FGameXXKCardCombatUnit* Target = FindAfterUnit(Result.TargetUnitId);
					EffectivePartyHeals += Result.EffectiveHealing > 0 && Target && Target->Side == EGameXXKCardTargetSide::Party ? 1 : 0;
				}
				Formula.PhaseProgress = FMath::Min(MAX_int32, Formula.PhaseProgress + EffectivePartyHeals);
				while (Formula.PhaseProgress >= 3 && Formula.Progress < 2)
				{
					Formula.PhaseProgress -= 3;
					++Formula.Progress;
					if (!DrawFormulaCards(1)) return false;
				}
				break;
			}
			case EGameXXKHealerFormulaKind::BleedRemovedPartyArmor:
				if (bPartyBleedRemoved && Formula.LastTriggeredRound != InOutRuntime.RoundNumber)
				{
					Formula.LastTriggeredRound = InOutRuntime.RoundNumber;
					for (FGameXXKCardCombatUnit& Ally : InOutRuntime.Units)
					{
						if (Ally.bLiving && Ally.Side == EGameXXKCardTargetSide::Party)
						{
							ApplyAndRecordArmor(InOutResult, FormulaOwner->UnitId, Ally, ResolveDefensePercentArmorAmount(*FormulaOwner, 20, FormulaQuality));
						}
					}
				}
				break;
			case EGameXXKHealerFormulaKind::LargeHealingArmorOrVulnerability:
			{
				const int32 HealingThreshold = FGameXXKCombatScalingRules::ResolveMedicineHealing(20, 0, FormulaQuality, InOutRuntime.TeamMaxLevelSnapshot);
				FName QualifiedTargetId = NAME_None;
				for (const FGameXXKCardHealingResult& Healing : TriggerResult.HealingResults)
				{
					if (Healing.RequestedHealing >= HealingThreshold && Healing.EffectiveHealing > 0) { QualifiedTargetId = Healing.TargetUnitId; break; }
				}
				if (QualifiedTargetId.IsNone())
				{
					for (const FGameXXKCardDamageResult& Damage : TriggerResult.DamageResults)
					{
						if (IsMedicineReverseDamage(Damage, PlayedSnapshot.OwnerUnitId) && Damage.RequestedDamage >= HealingThreshold) { QualifiedTargetId = Damage.ResolvedTargetUnitId; break; }
					}
				}
				if (!QualifiedTargetId.IsNone() && Formula.LastTriggeredRound != InOutRuntime.RoundNumber)
				{
					Formula.LastTriggeredRound = InOutRuntime.RoundNumber;
					if (FGameXXKCardCombatUnit* QualifiedTarget = FindCombatUnitById(InOutRuntime.Units, QualifiedTargetId))
					{
						if (QualifiedTarget->Side == EGameXXKCardTargetSide::Party)
						{
							ApplyAndRecordArmor(InOutResult, FormulaOwner->UnitId, *QualifiedTarget, ResolveDefensePercentArmorAmount(*FormulaOwner, 20, FormulaQuality));
						}
						else if (!GrantStatusFromHealerFormula(InOutRuntime, *QualifiedTarget, EGameXXKCardStatus::Vulnerability, 1, OutError, &InOutResult, FormulaOwner->UnitId)) return false;
					}
				}
				break;
			}
			case EGameXXKHealerFormulaKind::ThreeUnitHealthChangeDrawMana:
				if (HealthChangedUnitIds.Num() >= 3 && Formula.LastTriggeredRound != InOutRuntime.RoundNumber)
				{
					Formula.LastTriggeredRound = InOutRuntime.RoundNumber;
					if (!DrawFormulaCards(1)) return false;
					for (FGameXXKCardCombatUnit& Ally : InOutRuntime.Units)
					{
						if (Ally.bLiving && Ally.Side == EGameXXKCardTargetSide::Party) Ally.Mana = FMath::Min(Ally.MaxMana, Ally.Mana + 2);
					}
				}
				break;
			case EGameXXKHealerFormulaKind::PoisonDamageMedicine:
				if (PoisonPacketCount > 0 && !GrantStatusFromHealerFormula(InOutRuntime, *FormulaOwner, EGameXXKCardStatus::Medicine, PoisonPacketCount, OutError, &InOutResult, FormulaOwner->UnitId)) return false;
				break;
			case EGameXXKHealerFormulaKind::BleedPoisonMark:
			{
				if (Formula.UnitBudgetRound != InOutRuntime.RoundNumber)
				{
					Formula.UnitBudgetRound = InOutRuntime.RoundNumber;
					Formula.TriggeredUnitIdsThisRound.Reset();
				}
				for (const FName EnemyId : DebuffedEnemyIds)
				{
					FGameXXKCardCombatUnit* Enemy = FindCombatUnitById(InOutRuntime.Units, EnemyId);
					const FGameXXKCardCombatUnit* EnemyAtTrigger = FindAfterUnit(EnemyId);
					if (Enemy && EnemyAtTrigger && !Formula.TriggeredUnitIdsThisRound.Contains(EnemyId)
						&& GameXXKCardRules::GetCombatStatusStacks(*EnemyAtTrigger, EGameXXKCardStatus::Bleed) > 0
						&& GameXXKCardRules::GetCombatStatusStacks(*EnemyAtTrigger, EGameXXKCardStatus::Poison) > 0)
					{
						if (!GrantStatusFromHealerFormula(InOutRuntime, *Enemy, EGameXXKCardStatus::Mark, 1, OutError, &InOutResult, FormulaOwner->UnitId)) return false;
						Formula.TriggeredUnitIdsThisRound.Add(EnemyId);
					}
				}
				break;
			}
			case EGameXXKHealerFormulaKind::GroupPoisonMedicineDraw:
				if (PoisonedEnemyIds.Num() >= 2 && Formula.LastTriggeredRound != InOutRuntime.RoundNumber)
				{
					Formula.LastTriggeredRound = InOutRuntime.RoundNumber;
					if (!GrantStatusFromHealerFormula(InOutRuntime, *FormulaOwner, EGameXXKCardStatus::Medicine, 2, OutError, &InOutResult, FormulaOwner->UnitId) || !DrawFormulaCards(1)) return false;
				}
				break;
			case EGameXXKHealerFormulaKind::DualDotExplosionMedicine:
			{
				int32 QualifiedExplosions = 0;
				for (const int32 DistinctDotTypeCount : TriggerResult.ToxicExplosionDistinctDotTypeCounts)
				{
					QualifiedExplosions += DistinctDotTypeCount >= 2 ? 1 : 0;
				}
				if (QualifiedExplosions > 0 && !GrantStatusFromHealerFormula(InOutRuntime, *FormulaOwner, EGameXXKCardStatus::Medicine, QualifiedExplosions * 2, OutError, &InOutResult, FormulaOwner->UnitId)) return false;
				break;
			}
			case EGameXXKHealerFormulaKind::TwoBleedPacketsMedicine:
			{
				const int64 Total = static_cast<int64>(Formula.Progress) + BleedPacketCount;
				const int32 Rewards = static_cast<int32>(Total / 2);
				Formula.Progress = static_cast<int32>(Total % 2);
				if (Rewards > 0 && !GrantStatusFromHealerFormula(InOutRuntime, *FormulaOwner, EGameXXKCardStatus::Medicine, Rewards, OutError, &InOutResult, FormulaOwner->UnitId)) return false;
				break;
			}
			case EGameXXKHealerFormulaKind::GroupDirectDamageEnergy:
				if (DirectlyDamagedEnemyIds.Num() >= 2 && Formula.LastTriggeredRound != InOutRuntime.RoundNumber)
				{
					Formula.LastTriggeredRound = InOutRuntime.RoundNumber;
					InOutRuntime.Deck.SharedEnergy = FMath::Min(MaxCardBattleEnergy, InOutRuntime.Deck.SharedEnergy + 1);
				}
				break;
			case EGameXXKHealerFormulaKind::PoisonedVulnerabilityMedicineDraw:
			{
				bool bQualified = false;
				for (const FName EnemyId : VulnerabilityGainedEnemyIds)
				{
					const FGameXXKCardCombatUnit* Before = FindBeforeUnit(EnemyId);
					if (Before && GameXXKCardRules::GetCombatStatusStacks(*Before, EGameXXKCardStatus::Poison) > 0)
					{
						bQualified = true;
						break;
					}
				}
				if (bQualified && Formula.LastTriggeredRound != InOutRuntime.RoundNumber)
				{
					Formula.LastTriggeredRound = InOutRuntime.RoundNumber;
					if (!GrantStatusFromHealerFormula(InOutRuntime, *FormulaOwner, EGameXXKCardStatus::Medicine, 1, OutError, &InOutResult, FormulaOwner->UnitId) || !DrawFormulaCards(1)) return false;
				}
				break;
			}
			case EGameXXKHealerFormulaKind::TripleDotExplosionMomentumDraw:
				if (TriggerResult.ToxicExplosionDistinctDotTypeCounts.ContainsByPredicate([](const int32 Count) { return Count >= 3; })
					&& Formula.LastTriggeredRound != InOutRuntime.RoundNumber)
				{
					Formula.LastTriggeredRound = InOutRuntime.RoundNumber;
					if (!GrantStatusFromHealerFormula(InOutRuntime, *FormulaOwner, EGameXXKCardStatus::Momentum, 1, OutError, &InOutResult, FormulaOwner->UnitId) || !DrawFormulaCards(1)) return false;
				}
				break;
			case EGameXXKHealerFormulaKind::HeroFirstPartyHealthLossMedicine:
				if (!ResolveHeroFirstPartyHealthLossMedicine(
					InOutRuntime,
					Formula,
					TriggerResult.DamageResults,
					0,
					&InOutResult,
					OutError))
				{
					return false;
				}
				break;
			case EGameXXKHealerFormulaKind::HeroSixMedicineHealDraw:
			{
				const FGameXXKCardDefinition* PlayedDefinition = FGameXXKCardCatalog::FindCardDefinition(PlayedSnapshot.CardId);
				int32 MedicineConsumed = 0;
				for (const FGameXXKCardStatusChangeResult& Change : TriggerResult.StatusChanges)
				{
					if (Change.TargetUnitId == PlayedSnapshot.OwnerUnitId
						&& Change.Status == EGameXXKCardStatus::Medicine
						&& Change.RemovedStacks > 0)
					{
						MedicineConsumed = static_cast<int32>(FMath::Min<int64>(
							MAX_int32,
							static_cast<int64>(MedicineConsumed) + Change.RemovedStacks));
					}
				}
				const bool bHeroMedicineAction = PlayedDefinition
					&& PlayedDefinition->Owner == EGameXXKCardOwner::Hero
					&& PlayedSnapshot.OwnerUnitId == Formula.OwnerUnitId
					&& (TriggerResult.HealingResults.ContainsByPredicate([](const FGameXXKCardHealingResult& Result)
						{
							return Result.RequestedHealing > 0;
						})
						|| TriggerResult.DamageResults.ContainsByPredicate([&PlayedSnapshot](const FGameXXKCardDamageResult& Result)
						{
							return IsMedicineReverseDamage(Result, PlayedSnapshot.OwnerUnitId);
						}));
				if (bHeroMedicineAction
					&& MedicineConsumed >= 6
					&& Formula.LastTriggeredRound != InOutRuntime.RoundNumber)
				{
					Formula.LastTriggeredRound = InOutRuntime.RoundNumber;
					if (!DrawFormulaCards(1))
					{
						return false;
					}
				}
				break;
			}
			case EGameXXKHealerFormulaKind::HeroDualDotExplosionMedicine:
			{
				if (Formula.UnitBudgetRound != InOutRuntime.RoundNumber)
				{
					Formula.UnitBudgetRound = InOutRuntime.RoundNumber;
					Formula.Progress = 0;
				}
				int32 QualifiedExplosions = 0;
				for (const int32 DistinctDotTypeCount : TriggerResult.ToxicExplosionDistinctDotTypeCounts)
				{
					QualifiedExplosions += DistinctDotTypeCount >= 2 ? 1 : 0;
				}
				const int32 GrantedTriggers = FMath::Min(QualifiedExplosions, FMath::Max(0, 2 - Formula.Progress));
				Formula.Progress += GrantedTriggers;
				if (GrantedTriggers > 0
					&& !GrantStatusFromHealerFormula(
						InOutRuntime,
						*FormulaOwner,
						EGameXXKCardStatus::Medicine,
						GrantedTriggers * 2,
						OutError,
						&InOutResult,
						FormulaOwner->UnitId))
				{
					return false;
				}
				break;
			}
			case EGameXXKHealerFormulaKind::HeroGroupHealEnergy:
			{
				TSet<FName> EffectivelyHealedAllies;
				for (const FGameXXKCardHealingResult& Healing : TriggerResult.HealingResults)
				{
					const FGameXXKCardCombatUnit* HealedUnit = FindAfterUnit(Healing.TargetUnitId);
					if (Healing.EffectiveHealing > 0
						&& HealedUnit
						&& HealedUnit->Side == FormulaOwner->Side)
					{
						EffectivelyHealedAllies.Add(Healing.TargetUnitId);
					}
				}
				if (EffectivelyHealedAllies.Num() >= 2
					&& Formula.LastTriggeredRound != InOutRuntime.RoundNumber)
				{
					Formula.LastTriggeredRound = InOutRuntime.RoundNumber;
					InOutRuntime.Deck.SharedEnergy = FMath::Min(
						MaxCardBattleEnergy,
						InOutRuntime.Deck.SharedEnergy + 1);
				}
				break;
			}
			default:
				break;
			}
		}
		return true;
	}

	bool ResolveOpenedHealerFormulasAfterDamageEvents(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const TArray<FGameXXKCardCombatUnit>& UnitsBeforeDamage,
		const TArray<FGameXXKCardDamageResult>& DamageResults,
		const int32 FirstDamageResultIndex,
		FString& OutError)
	{
		if (FirstDamageResultIndex < 0 || FirstDamageResultIndex > DamageResults.Num())
		{
			OutError = TEXT("A Healer damage-event formula received an invalid result range.");
			return false;
		}
		int32 HealthChangeCount = 0;
		int32 PoisonPacketCount = 0;
		int32 BleedPacketCount = 0;
		for (int32 ResultIndex = FirstDamageResultIndex; ResultIndex < DamageResults.Num(); ++ResultIndex)
		{
			const FGameXXKCardDamageResult& DamageResult = DamageResults[ResultIndex];
			if (DamageResult.HealthDamage <= 0)
			{
				continue;
			}
			++HealthChangeCount;
			PoisonPacketCount += DamageResult.Cause == EGameXXKCardDamageCause::Poison
				|| DamageResult.Cause == EGameXXKCardDamageCause::ToxicExplosionPoison ? 1 : 0;
			BleedPacketCount += DamageResult.Cause == EGameXXKCardDamageCause::Bleed
				|| DamageResult.Cause == EGameXXKCardDamageCause::ToxicExplosionBleed ? 1 : 0;
		}

		for (FGameXXKHealerFormulaRuntime& Formula : InOutRuntime.HealerFormulas)
		{
			FGameXXKCardCombatUnit* FormulaOwner = FindCombatUnitById(InOutRuntime.Units, Formula.OwnerUnitId);
			if (!FormulaOwner || !FormulaOwner->bLiving)
			{
				continue;
			}
			switch (Formula.Kind)
			{
			case EGameXXKHealerFormulaKind::AnyHealthChangeMedicine:
				if (HealthChangeCount > 0
					&& !GrantStatusFromHealerFormula(InOutRuntime, *FormulaOwner, EGameXXKCardStatus::Medicine, HealthChangeCount, OutError))
				{
					return false;
				}
				break;
			case EGameXXKHealerFormulaKind::LowHealthCrossMedicine:
			case EGameXXKHealerFormulaKind::LowHealthCrossAgility:
			{
				const int32 Threshold = Formula.Kind == EGameXXKHealerFormulaKind::LowHealthCrossMedicine ? 35 : 30;
				FName CrossedUnitId = NAME_None;
				for (const FGameXXKCardCombatUnit& Before : UnitsBeforeDamage)
				{
					const FGameXXKCardCombatUnit* After = FindCombatUnitById(InOutRuntime.Units, Before.UnitId);
					if (Before.Side == EGameXXKCardTargetSide::Party
						&& After
						&& static_cast<int64>(Before.HP) * 100 >= static_cast<int64>(Before.MaxHP) * Threshold
						&& static_cast<int64>(After->HP) * 100 < static_cast<int64>(After->MaxHP) * Threshold)
					{
						CrossedUnitId = Before.UnitId;
						break;
					}
				}
				if (!CrossedUnitId.IsNone() && Formula.LastTriggeredRound != InOutRuntime.RoundNumber)
				{
					Formula.LastTriggeredRound = InOutRuntime.RoundNumber;
					if (Formula.Kind == EGameXXKHealerFormulaKind::LowHealthCrossMedicine)
					{
						if (!GrantStatusFromHealerFormula(InOutRuntime, *FormulaOwner, EGameXXKCardStatus::Medicine, 3, OutError))
						{
							return false;
						}
					}
					else
					{
						FGameXXKCardCombatUnit* CrossedUnit = FindCombatUnitById(InOutRuntime.Units, CrossedUnitId);
						if (CrossedUnit && CrossedUnit->bLiving
							&& !GrantStatusFromHealerFormula(InOutRuntime, *CrossedUnit, EGameXXKCardStatus::Agility, 2, OutError))
						{
							return false;
						}
					}
				}
				break;
			}
			case EGameXXKHealerFormulaKind::PoisonDamageMedicine:
				if (PoisonPacketCount > 0
					&& !GrantStatusFromHealerFormula(InOutRuntime, *FormulaOwner, EGameXXKCardStatus::Medicine, PoisonPacketCount, OutError))
				{
					return false;
				}
				break;
			case EGameXXKHealerFormulaKind::TwoBleedPacketsMedicine:
			{
				const int64 Total = static_cast<int64>(Formula.Progress) + BleedPacketCount;
				const int32 Rewards = static_cast<int32>(Total / 2);
				Formula.Progress = static_cast<int32>(Total % 2);
				if (Rewards > 0
					&& !GrantStatusFromHealerFormula(InOutRuntime, *FormulaOwner, EGameXXKCardStatus::Medicine, Rewards, OutError))
				{
					return false;
				}
				break;
			}
			case EGameXXKHealerFormulaKind::HeroFirstPartyHealthLossMedicine:
				if (!ResolveHeroFirstPartyHealthLossMedicine(
					InOutRuntime,
					Formula,
					DamageResults,
					FirstDamageResultIndex,
					nullptr,
					OutError))
				{
					return false;
				}
				break;
			default:
				break;
			}
		}
		return true;
	}

	bool InstallHealerFormulaAfterActivePlay(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FGameXXKCardDefinition& Definition,
		const FName OwnerUnitId,
		FString& OutError)
	{
		const FGameXXKHealerCardRule& Rule = Definition.HealerRule;
		if (Rule.FormulaKind == EGameXXKHealerFormulaKind::None
			|| IsHealerFormulaOpen(InOutRuntime, OwnerUnitId, Definition.Id))
		{
			return true;
		}
		const FGameXXKCardCombatUnit* Owner = FindCombatUnitById(InOutRuntime.Units, OwnerUnitId);
		const bool bValidFormulaSource = (Definition.Owner == EGameXXKCardOwner::Profession
				&& Definition.Role == EGameXXKCharacterRole::Healer)
			|| (Definition.Owner == EGameXXKCardOwner::Hero
				&& Definition.LinkedRole == EGameXXKCharacterRole::Healer);
		if (!Owner || !Owner->bLiving || Owner->Side != EGameXXKCardTargetSide::Party
			|| !bValidFormulaSource
			|| Rule.UnopenedFormulaEnergySurcharge <= 0)
		{
			OutError = TEXT("A permanent Healer formula could not be installed for its living party owner.");
			return false;
		}
		FGameXXKHealerFormulaRuntime& Formula = InOutRuntime.HealerFormulas.AddDefaulted_GetRef();
		Formula.OwnerUnitId = OwnerUnitId;
		Formula.SourceCardId = Definition.Id;
		Formula.Kind = Rule.FormulaKind;
		Formula.SourceQuality = Definition.BaseQuality;
		return true;
	}

	bool IsCurrentlySupportedEffect(const FGameXXKCardEffect& Effect, FString& OutError)
	{
		OutError.Reset();
		switch (Effect.Type)
		{
		case EGameXXKCardEffectType::DamagePercentAttack:
		case EGameXXKCardEffectType::DamageFlat:
		case EGameXXKCardEffectType::LoseHealth:
		case EGameXXKCardEffectType::Heal:
		case EGameXXKCardEffectType::AddArmor:
		case EGameXXKCardEffectType::GainMana:
		case EGameXXKCardEffectType::GainManaPerConsumedStatus:
		case EGameXXKCardEffectType::GainEnergy:
		case EGameXXKCardEffectType::DrawCards:
		case EGameXXKCardEffectType::ApplyStatus:
		case EGameXXKCardEffectType::RemoveStatus:
		case EGameXXKCardEffectType::RemoveAnyDamageOverTime:
		case EGameXXKCardEffectType::CleanseFriendlyDamageOverTime:
		case EGameXXKCardEffectType::Insight:
		case EGameXXKCardEffectType::DiscoverCards:
		case EGameXXKCardEffectType::ReorderCards:
		case EGameXXKCardEffectType::DiscardCards:
		case EGameXXKCardEffectType::IgnoreDefense:
		case EGameXXKCardEffectType::BonusDamagePercent:
		case EGameXXKCardEffectType::BonusDamagePercentPerConsumedStatus:
		case EGameXXKCardEffectType::BonusDamagePercentPerConsumedArmor:
		case EGameXXKCardEffectType::EachLivingAllyAttackSelectedTarget:
		case EGameXXKCardEffectType::ApplyGuardLink:
		case EGameXXKCardEffectType::ApplyBattleModifier:
		case EGameXXKCardEffectType::RevealEnemyIntent:
		case EGameXXKCardEffectType::DoubleTerrainBonus:
		case EGameXXKCardEffectType::RedirectSingleTargetEnemyAttacks:
		case EGameXXKCardEffectType::RegisterReaction:
		case EGameXXKCardEffectType::LoseHealthNonlethal:
		case EGameXXKCardEffectType::Cleanse:
		case EGameXXKCardEffectType::TriggerHighestDamageOverTime:
		case EGameXXKCardEffectType::ResolveToxicExplosion:
		case EGameXXKCardEffectType::HealOrReverseWithMedicine:
		case EGameXXKCardEffectType::HealOrReverseFlat:
		case EGameXXKCardEffectType::GainMedicineFromPartyHealthLoss:
		case EGameXXKCardEffectType::DamagePercentAttackPlusArmor:
		case EGameXXKCardEffectType::DamageAllPercentAttackPerConsumedArmor:
		case EGameXXKCardEffectType::TriggerTerrainBenefit:
		case EGameXXKCardEffectType::GainArmorFromCurrentManaPercent:
		case EGameXXKCardEffectType::GainManaOverflowToArmor:
		case EGameXXKCardEffectType::SearchUnfinishedHeroTaskCard:
		case EGameXXKCardEffectType::SearchUnfinishedTaskNpcCard:
		case EGameXXKCardEffectType::PreserveNextReactionUse:
		case EGameXXKCardEffectType::RetainArmorNextRound:
		case EGameXXKCardEffectType::TriggerStatus:
		case EGameXXKCardEffectType::LightningPerTargetStatusSnapshot:
		case EGameXXKCardEffectType::DamagePercentAttackPerTargetStatus:
		case EGameXXKCardEffectType::IncreaseMaxMana:
		case EGameXXKCardEffectType::ChangeTerrain:
			return true;
		default:
			OutError = TEXT("This card effect is not yet supported by the runtime effect planner.");
			return false;
		}
	}

	bool ValidateCurrentEffectPlan(const FGameXXKCardDefinition& Definition, FString& OutError)
	{
		for (const FGameXXKCardEffect& Effect : Definition.Effects)
		{
			if (!IsCurrentlySupportedEffect(Effect, OutError))
			{
				return false;
			}
		}
		return true;
	}

	int32 RemoveAnyDamageOverTime(FGameXXKCardCombatUnit& InOutUnit, const int32 Maximum)
	{
		int32 Remaining = Maximum;
		int32 Removed = 0;
		for (const EGameXXKCardStatus Status : { EGameXXKCardStatus::Bleed, EGameXXKCardStatus::Poison, EGameXXKCardStatus::Burn, EGameXXKCardStatus::DamageOverTime })
		{
			if (Remaining <= 0)
			{
				break;
			}
			const int32 RemovedHere = GameXXKCardRules::ConsumeCombatStatus(InOutUnit, Status, Remaining);
			Removed += RemovedHere;
			Remaining -= RemovedHere;
		}
		return Removed;
	}

	bool IsCardEffectTargetCompatibleWithAttack(const FGameXXKCardEffect& Candidate, const FGameXXKCardEffect& Attack)
	{
		return Candidate.Target == Attack.Target;
	}

	bool IsAttackPacketAttachment(const FGameXXKCardEffect& Candidate, const FGameXXKCardEffect& Attack)
	{
		if (!IsCardEffectTargetCompatibleWithAttack(Candidate, Attack))
		{
			return false;
		}
		return Candidate.Type == EGameXXKCardEffectType::DamageFlat
			|| Candidate.Type == EGameXXKCardEffectType::IgnoreDefense
			|| Candidate.Type == EGameXXKCardEffectType::BonusDamagePercent
			|| Candidate.Type == EGameXXKCardEffectType::BonusDamagePercentPerConsumedStatus
			|| Candidate.Type == EGameXXKCardEffectType::BonusDamagePercentPerConsumedArmor
			|| Candidate.Type == EGameXXKCardEffectType::ApplyStatus;
	}

	bool HasSuccessfulConsumptionReference(const FGameXXKCardEffect& Effect, const TMap<FName, int32>& ConsumptionResults)
	{
		return Effect.ConsumedStackResultRef.IsNone() || ConsumptionResults.FindRef(Effect.ConsumedStackResultRef) > 0;
	}

	bool TryApplyEffectConditionAndConsumption(
		const FGameXXKCardEffectCondition& Condition,
		FGameXXKCardBattleRuntime& InOutRuntime,
		FGameXXKCardCombatUnit& InOutOwner,
		FGameXXKCardCombatUnit* Target,
		const FGameXXKCardPlayConditionSnapshot* Snapshot,
		bool& OutSatisfied,
		int32& OutConsumed,
		FString& OutError)
	{
		OutSatisfied = false;
		OutConsumed = 0;
		const FGameXXKCardPlayConditionSnapshot* EvaluationSnapshot = Condition.bConsumeStatus ? nullptr : Snapshot;
		if (!IsConditionSatisfied(Condition, InOutRuntime, InOutOwner, Target, EvaluationSnapshot, OutSatisfied, OutError) || !OutSatisfied)
		{
			return OutError.IsEmpty();
		}
		if (Condition.bNegate && (Condition.bConsumeStatus || Condition.bConsumeOwnerArmor))
		{
			OutError = TEXT("A negated card condition cannot consume the state it negates.");
			return false;
		}
		if (Condition.bConsumeStatus)
		{
			FGameXXKCardCombatUnit* ConsumedUnit = nullptr;
			if (Condition.Type == EGameXXKCardEffectConditionType::TargetHasStatus || Condition.Type == EGameXXKCardEffectConditionType::TargetHasAnyDamageOverTime)
			{
				ConsumedUnit = Target;
			}
			else if (Condition.Type == EGameXXKCardEffectConditionType::OwnerHasStatus || Condition.Type == EGameXXKCardEffectConditionType::OwnerHasDamageOverTime)
			{
				ConsumedUnit = &InOutOwner;
			}
			if (!ConsumedUnit)
			{
				OutError = TEXT("The requested status consumption does not have a concrete source unit.");
				return false;
			}
			int32 Maximum = Condition.MaxConsumedStatusStacks;
			if (Maximum == 0 && Condition.Status == EGameXXKCardStatus::Momentum && Snapshot)
			{
				// "Consume all" for a card packet means all Momentum captured when that
				// card began resolving. Momentum created by an intervening reaction belongs
				// to the later state and must survive this consumption.
				Maximum = Snapshot->MomentumStacksByUnitId.FindRef(ConsumedUnit->UnitId);
				if (Maximum <= 0)
				{
					OutSatisfied = false;
					return true;
				}
			}
			if (Condition.Type == EGameXXKCardEffectConditionType::TargetHasAnyDamageOverTime || Condition.Type == EGameXXKCardEffectConditionType::OwnerHasDamageOverTime)
			{
				OutConsumed = RemoveAnyDamageOverTime(*ConsumedUnit, Maximum == 0 ? MAX_int32 : Maximum);
			}
			else
			{
				OutConsumed = GameXXKCardRules::ConsumeCombatStatus(*ConsumedUnit, Condition.Status, Maximum);
			}
			if (OutConsumed <= 0)
			{
				OutError = TEXT("A card condition was satisfied but failed to consume its declared status.");
				return false;
			}
		}
		if (Condition.bConsumeOwnerArmor)
		{
			const int32 Maximum = Condition.MaxConsumedArmor == 0 ? InOutOwner.Armor : Condition.MaxConsumedArmor;
			const int32 RemovedArmor = FMath::Min(FMath::Max(0, Maximum), InOutOwner.Armor);
			if (RemovedArmor <= 0)
			{
				OutError = TEXT("A card condition was satisfied but failed to consume its declared armor.");
				return false;
			}
			InOutOwner.Armor -= RemovedArmor;
			OutConsumed += RemovedArmor;
		}
		return true;
	}

	bool DoesOnNextAttackModifierApply(
		const FGameXXKCardBattleModifierRuntime& Modifier,
		const FGameXXKCardDefinition& PlayedDefinition,
		const FGameXXKCardInstance& PlayedInstance,
		const FGameXXKCardCombatUnit& PlayedOwner,
		bool& OutApplies,
		FString& OutError)
	{
		OutApplies = false;
		OutError.Reset();
		const FGameXXKCardBattleModifier& ModifierDefinition = Modifier.Definition;
		if (ModifierDefinition.Trigger != EGameXXKCardBattleModifierTrigger::OnNextAttack
			|| (ModifierDefinition.EffectType != EGameXXKCardEffectType::BonusDamagePercent
				&& ModifierDefinition.EffectType != EGameXXKCardEffectType::BonusDamagePercentPerConsumedStatus))
		{
			return true;
		}
		if (ModifierDefinition.Target != EGameXXKCardEffectTarget::PlayedCard)
		{
			OutError = TEXT("A next-attack modifier must explicitly target the played card.");
			return false;
		}
		if (ModifierDefinition.RecipientScope != EGameXXKCardModifierRecipientScope::SharedDeck
			&& !Modifier.RecipientUnitIds.Contains(PlayedInstance.OwnerUnitId))
		{
			return true;
		}
		if (ModifierDefinition.RequiredTriggeredRole != EGameXXKCharacterRole::Invalid
			&& ModifierDefinition.RequiredTriggeredRole != PlayedOwner.Role)
		{
			return true;
		}
		if (!ModifierDefinition.RequiredTriggeredOwnerId.IsNone()
			&& ModifierDefinition.RequiredTriggeredOwnerId != PlayedDefinition.OwnerId)
		{
			return true;
		}
		if (ModifierDefinition.Expiry == EGameXXKCardModifierExpiry::AfterTriggerCount && ModifierDefinition.RemainingTriggers <= 0)
		{
			OutError = TEXT("A next-attack modifier has no remaining trigger count.");
			return false;
		}
		OutApplies = true;
		return true;
	}

	bool CollectOnNextAttackBonuses(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FGameXXKCardDefinition& PlayedDefinition,
		const FGameXXKCardInstance& PlayedInstance,
		FGameXXKCardCombatUnit& InOutOwner,
		FGameXXKCardCombatUnit* ConditionTarget,
		int32& OutBonusPercent,
		TArray<FName>& OutModifierIds,
		FString& OutError)
	{
		OutBonusPercent = 0;
		OutModifierIds.Reset();
		OutError.Reset();
		for (const FGameXXKCardBattleModifierRuntime& Modifier : InOutRuntime.Modifiers)
		{
			bool bModifierApplies = false;
			if (!DoesOnNextAttackModifierApply(Modifier, PlayedDefinition, PlayedInstance, InOutOwner, bModifierApplies, OutError))
			{
				return false;
			}
			if (!bModifierApplies)
			{
				continue;
			}
			bool bConditionSatisfied = false;
			int32 Consumed = 0;
			if (!TryApplyEffectConditionAndConsumption(Modifier.Definition.Condition, InOutRuntime, InOutOwner, ConditionTarget, nullptr, bConditionSatisfied, Consumed, OutError))
			{
				return false;
			}
			if (!bConditionSatisfied)
			{
				continue;
			}
			const int64 AppliedMagnitude = Modifier.Definition.EffectType == EGameXXKCardEffectType::BonusDamagePercentPerConsumedStatus
				? static_cast<int64>(Modifier.Definition.Magnitude) * Consumed
				: Modifier.Definition.Magnitude;
			if (AppliedMagnitude > MAX_int32
				|| AppliedMagnitude < MIN_int32
				|| OutBonusPercent > MAX_int32 - AppliedMagnitude
				|| OutBonusPercent < MIN_int32 - AppliedMagnitude)
			{
				OutError = TEXT("Next-attack modifier bonuses exceed the supported range.");
				return false;
			}
			OutBonusPercent += static_cast<int32>(AppliedMagnitude);
			OutModifierIds.Add(Modifier.ModifierId);
		}
		return true;
	}

	bool ConsumeOnNextAttackModifiers(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const TArray<FName>& ModifierIds,
		FString& OutError)
	{
		OutError.Reset();
		TSet<FName> RequestedIds;
		for (const FName ModifierId : ModifierIds)
		{
			if (ModifierId.IsNone() || RequestedIds.Contains(ModifierId))
			{
				OutError = TEXT("Triggered next-attack modifier IDs must be unique and non-empty.");
				return false;
			}
			RequestedIds.Add(ModifierId);
		}
		for (int32 Index = InOutRuntime.Modifiers.Num() - 1; Index >= 0; --Index)
		{
			FGameXXKCardBattleModifierRuntime& Modifier = InOutRuntime.Modifiers[Index];
			if (!RequestedIds.Contains(Modifier.ModifierId))
			{
				continue;
			}
			FGameXXKCardBattleModifier& ModifierDefinition = Modifier.Definition;
			if (ModifierDefinition.Trigger != EGameXXKCardBattleModifierTrigger::OnNextAttack
				|| (ModifierDefinition.EffectType != EGameXXKCardEffectType::BonusDamagePercent
					&& ModifierDefinition.EffectType != EGameXXKCardEffectType::BonusDamagePercentPerConsumedStatus))
			{
				OutError = TEXT("A non-attack modifier was selected for next-attack consumption.");
				return false;
			}
			switch (ModifierDefinition.Expiry)
			{
			case EGameXXKCardModifierExpiry::AfterTriggerCount:
				if (ModifierDefinition.RemainingTriggers <= 0)
				{
					OutError = TEXT("A next-attack modifier has no remaining trigger count.");
					return false;
				}
				--ModifierDefinition.RemainingTriggers;
				if (ModifierDefinition.RemainingTriggers == 0)
				{
					InOutRuntime.Modifiers.RemoveAt(Index, 1, EAllowShrinking::No);
				}
				break;
			case EGameXXKCardModifierExpiry::EndOfCurrentRound:
				break;
			case EGameXXKCardModifierExpiry::EndOfCurrentRoundOrTriggerCount:
				if (ModifierDefinition.RemainingTriggers > 0 && --ModifierDefinition.RemainingTriggers == 0)
				{
					InOutRuntime.Modifiers.RemoveAt(Index, 1, EAllowShrinking::No);
				}
				break;
			case EGameXXKCardModifierExpiry::Invalid:
			default:
				OutError = TEXT("A next-attack modifier has an invalid expiry policy.");
				return false;
			}
		}
		return true;
	}

	bool DoesOnNextHealingModifierApply(
		const FGameXXKCardBattleModifierRuntime& Modifier,
		const FGameXXKCardDefinition& PlayedDefinition,
		const FGameXXKCardInstance& PlayedInstance,
		const FGameXXKCardCombatUnit& PlayedOwner,
		bool& OutApplies,
		FString& OutError)
	{
		OutApplies = false;
		OutError.Reset();
		const FGameXXKCardBattleModifier& ModifierDefinition = Modifier.Definition;
		if (ModifierDefinition.Trigger != EGameXXKCardBattleModifierTrigger::OnNextHealing
			|| ModifierDefinition.EffectType != EGameXXKCardEffectType::ModifyHealingPercent)
		{
			return true;
		}
		if (ModifierDefinition.Target != EGameXXKCardEffectTarget::PlayedCard)
		{
			OutError = TEXT("A healing modifier must explicitly target the played card.");
			return false;
		}
		if (ModifierDefinition.RecipientScope != EGameXXKCardModifierRecipientScope::SharedDeck
			&& !Modifier.RecipientUnitIds.Contains(PlayedInstance.OwnerUnitId))
		{
			return true;
		}
		if (ModifierDefinition.RequiredTriggeredRole != EGameXXKCharacterRole::Invalid
			&& ModifierDefinition.RequiredTriggeredRole != PlayedOwner.Role)
		{
			return true;
		}
		if (!ModifierDefinition.RequiredTriggeredOwnerId.IsNone()
			&& ModifierDefinition.RequiredTriggeredOwnerId != PlayedDefinition.OwnerId)
		{
			return true;
		}
		if (ModifierDefinition.Expiry == EGameXXKCardModifierExpiry::AfterTriggerCount && ModifierDefinition.RemainingTriggers <= 0)
		{
			OutError = TEXT("A next-healing modifier has no remaining trigger count.");
			return false;
		}
		OutApplies = true;
		return true;
	}

	bool CollectOnNextHealingBonuses(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FGameXXKCardDefinition& PlayedDefinition,
		const FGameXXKCardInstance& PlayedInstance,
		FGameXXKCardCombatUnit& InOutOwner,
		FGameXXKCardCombatUnit* ConditionTarget,
		int32& OutBonusPercent,
		TArray<FName>& OutModifierIds,
		FString& OutError)
	{
		OutBonusPercent = 0;
		OutModifierIds.Reset();
		OutError.Reset();
		for (const FGameXXKCardBattleModifierRuntime& Modifier : InOutRuntime.Modifiers)
		{
			bool bModifierApplies = false;
			if (!DoesOnNextHealingModifierApply(Modifier, PlayedDefinition, PlayedInstance, InOutOwner, bModifierApplies, OutError))
			{
				return false;
			}
			if (!bModifierApplies)
			{
				continue;
			}
			bool bConditionSatisfied = false;
			int32 IgnoredConsumed = 0;
			if (!TryApplyEffectConditionAndConsumption(Modifier.Definition.Condition, InOutRuntime, InOutOwner, ConditionTarget, nullptr, bConditionSatisfied, IgnoredConsumed, OutError))
			{
				return false;
			}
			if (!bConditionSatisfied)
			{
				continue;
			}
			const int64 NewBonus = static_cast<int64>(OutBonusPercent) + Modifier.Definition.Magnitude;
			if (NewBonus < MIN_int32 || NewBonus > MAX_int32)
			{
				OutError = TEXT("Next-healing modifier bonuses exceed the supported range.");
				return false;
			}
			OutBonusPercent = static_cast<int32>(NewBonus);
			OutModifierIds.Add(Modifier.ModifierId);
		}
		return true;
	}

	bool ConsumeOnNextHealingModifiers(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const TArray<FName>& ModifierIds,
		FString& OutError)
	{
		OutError.Reset();
		TSet<FName> RequestedIds;
		for (const FName ModifierId : ModifierIds)
		{
			if (ModifierId.IsNone() || RequestedIds.Contains(ModifierId))
			{
				OutError = TEXT("Triggered next-healing modifier IDs must be unique and non-empty.");
				return false;
			}
			RequestedIds.Add(ModifierId);
		}
		for (int32 Index = InOutRuntime.Modifiers.Num() - 1; Index >= 0; --Index)
		{
			FGameXXKCardBattleModifierRuntime& Modifier = InOutRuntime.Modifiers[Index];
			if (!RequestedIds.Contains(Modifier.ModifierId))
			{
				continue;
			}
			FGameXXKCardBattleModifier& ModifierDefinition = Modifier.Definition;
			if (ModifierDefinition.Trigger != EGameXXKCardBattleModifierTrigger::OnNextHealing
				|| ModifierDefinition.EffectType != EGameXXKCardEffectType::ModifyHealingPercent)
			{
				OutError = TEXT("A non-healing modifier was selected for next-healing consumption.");
				return false;
			}
			switch (ModifierDefinition.Expiry)
			{
			case EGameXXKCardModifierExpiry::AfterTriggerCount:
				if (ModifierDefinition.RemainingTriggers <= 0)
				{
					OutError = TEXT("A next-healing modifier has no remaining trigger count.");
					return false;
				}
				--ModifierDefinition.RemainingTriggers;
				if (ModifierDefinition.RemainingTriggers == 0)
				{
					InOutRuntime.Modifiers.RemoveAt(Index, 1, EAllowShrinking::No);
				}
				break;
			case EGameXXKCardModifierExpiry::EndOfCurrentRound:
				break;
			case EGameXXKCardModifierExpiry::EndOfCurrentRoundOrTriggerCount:
				if (ModifierDefinition.RemainingTriggers > 0 && --ModifierDefinition.RemainingTriggers == 0)
				{
					InOutRuntime.Modifiers.RemoveAt(Index, 1, EAllowShrinking::No);
				}
				break;
			case EGameXXKCardModifierExpiry::Invalid:
			default:
				OutError = TEXT("A next-healing modifier has an invalid expiry policy.");
				return false;
			}
		}
		return true;
	}

	bool ConsumeFirstDirectDamageModifiers(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const TArray<FName>& ModifierIds,
		FString& OutError)
	{
		OutError.Reset();
		TSet<FName> RequestedIds;
		for (const FName ModifierId : ModifierIds)
		{
			if (ModifierId.IsNone() || RequestedIds.Contains(ModifierId))
			{
				OutError = TEXT("Triggered first-direct-damage modifier IDs must be unique and non-empty.");
				return false;
			}
			RequestedIds.Add(ModifierId);
		}
		for (int32 Index = InOutRuntime.Modifiers.Num() - 1; Index >= 0; --Index)
		{
			FGameXXKCardBattleModifierRuntime& Modifier = InOutRuntime.Modifiers[Index];
			if (!RequestedIds.Contains(Modifier.ModifierId))
			{
				continue;
			}
			FGameXXKCardBattleModifier& Definition = Modifier.Definition;
			if (Definition.Trigger != EGameXXKCardBattleModifierTrigger::FirstDirectDamageReceivedThisRound)
			{
				OutError = TEXT("A non-reactive modifier was selected for first-direct-damage consumption.");
				return false;
			}
			switch (Definition.Expiry)
			{
			case EGameXXKCardModifierExpiry::AfterTriggerCount:
				if (Definition.RemainingTriggers <= 0)
				{
					OutError = TEXT("A first-direct-damage modifier has no remaining trigger count.");
					return false;
				}
				if (--Definition.RemainingTriggers == 0)
				{
					InOutRuntime.Modifiers.RemoveAt(Index, 1, EAllowShrinking::No);
				}
				break;
			case EGameXXKCardModifierExpiry::EndOfCurrentRound:
				break;
			case EGameXXKCardModifierExpiry::EndOfCurrentRoundOrTriggerCount:
				if (Definition.RemainingTriggers > 0 && --Definition.RemainingTriggers == 0)
				{
					InOutRuntime.Modifiers.RemoveAt(Index, 1, EAllowShrinking::No);
				}
				break;
			case EGameXXKCardModifierExpiry::Invalid:
			default:
				OutError = TEXT("A first-direct-damage modifier has an invalid expiry policy.");
				return false;
			}
		}
		return true;
	}

	bool ResolveFirstDirectDamageReactiveModifiers(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FGameXXKCardDamageContext& IncomingContext,
		const FGameXXKCardDamageResult& IncomingResult,
		TArray<FGameXXKCardDamageResult>* OutAdditionalDamageResults,
		FString& OutError,
		FGameXXKCardPlayResult* InOutPlayResult = nullptr)
	{
		OutError.Reset();
		if ((IncomingContext.Kind != EGameXXKCardDamageKind::SingleTargetAttack && IncomingContext.Kind != EGameXXKCardDamageKind::GroupAttack)
			|| IncomingResult.bAvoidedByAgility)
		{
			return true;
		}
		FGameXXKCardCombatUnit* Recipient = FindCombatUnitById(InOutRuntime.Units, IncomingResult.ResolvedTargetUnitId);
		FGameXXKCardCombatUnit* Attacker = FindCombatUnitById(InOutRuntime.Units, IncomingContext.SourceUnitId);
		if (!Recipient || !Recipient->bLiving || !Attacker || !Attacker->bLiving || Recipient->Side == Attacker->Side)
		{
			return true;
		}
		const FName RecipientUnitId = Recipient->UnitId;
		const FName AttackerUnitId = Attacker->UnitId;
		TArray<FName> TriggeredModifierIds;
		for (const FGameXXKCardBattleModifierRuntime& Modifier : InOutRuntime.Modifiers)
		{
			const FGameXXKCardBattleModifier& ModifierDefinition = Modifier.Definition;
			if (ModifierDefinition.Trigger != EGameXXKCardBattleModifierTrigger::FirstDirectDamageReceivedThisRound
				|| !Modifier.RecipientUnitIds.Contains(RecipientUnitId))
			{
				continue;
			}
			if (ModifierDefinition.Target != EGameXXKCardEffectTarget::Attacker
				|| (ModifierDefinition.EffectType != EGameXXKCardEffectType::DamagePercentAttack
					&& ModifierDefinition.EffectType != EGameXXKCardEffectType::ApplyStatus))
			{
				OutError = TEXT("A first-direct-damage modifier has an unsupported target or effect type.");
				return false;
			}
			Recipient = FindCombatUnitById(InOutRuntime.Units, RecipientUnitId);
			Attacker = FindCombatUnitById(InOutRuntime.Units, AttackerUnitId);
			if (!Recipient || !Recipient->bLiving || !Attacker || !Attacker->bLiving)
			{
				continue;
			}
			bool bConditionSatisfied = false;
			int32 IgnoredConsumed = 0;
			if (!TryApplyEffectConditionAndConsumption(ModifierDefinition.Condition, InOutRuntime, *Recipient, Attacker, nullptr, bConditionSatisfied, IgnoredConsumed, OutError))
			{
				return false;
			}
			if (!bConditionSatisfied)
			{
				continue;
			}
			if (ModifierDefinition.EffectType == EGameXXKCardEffectType::DamagePercentAttack)
			{
				const int64 RequestedDamage = static_cast<int64>(Recipient->Attack) * ModifierDefinition.Magnitude / 100;
				if (RequestedDamage <= 0 || RequestedDamage > MAX_int32)
				{
					OutError = TEXT("A first-direct-damage counterattack produced an unsupported damage amount.");
					return false;
				}
				FGameXXKCardDamageContext CounterContext;
				CounterContext.SourceUnitId = RecipientUnitId;
				CounterContext.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
				CounterContext.ResolutionOrigin = EGameXXKCardResolutionOrigin::Reaction;
				FGameXXKCardDamageResult CounterResult;
				if (!ApplyCombatDirectDamageInternal(
					InOutRuntime.Units,
					InOutRuntime.GuardLinks,
					CounterContext,
					AttackerUnitId,
					static_cast<int32>(RequestedDamage),
					CounterResult,
					nullptr,
					&InOutRuntime,
					InOutPlayResult,
					false,
					&OutError))
				{
					return false;
				}
				CounterResult.Cause = EGameXXKCardDamageCause::Counter;
				if (OutAdditionalDamageResults)
				{
					OutAdditionalDamageResults->Add(MoveTemp(CounterResult));
				}
			}
			else
			{
				if (!IsConcreteCombatStatus(ModifierDefinition.Status) || ModifierDefinition.Magnitude <= 0)
				{
					OutError = TEXT("A first-direct-damage status reaction is missing concrete positive status data.");
					return false;
				}
				if (!GrantStatusFromCardEffect(
					InOutRuntime,
					*Attacker,
					ModifierDefinition.Status,
					ModifierDefinition.Magnitude,
					OutError))
				{
					return false;
				}
			}
			TriggeredModifierIds.Add(Modifier.ModifierId);
		}
		return ConsumeFirstDirectDamageModifiers(InOutRuntime, TriggeredModifierIds, OutError);
	}

	bool ResolveTriggeredStatusLayerConsumption(
		FGameXXKCardBattleRuntime& InOutRuntime,
		FGameXXKCardCombatUnit& Target,
		const EGameXXKCardStatus Status,
		const int32 DefaultConsumption,
		const FName TriggerSourceUnitId,
		const int32 TriggeredHealthDamage,
		int32& OutConsumedStacks,
		FString& OutError,
		FGameXXKCardPlayResult* InOutResult = nullptr)
	{
		OutConsumedStacks = 0;
		if (DefaultConsumption < 0)
		{
			OutError = TEXT("Triggered status layer consumption cannot be negative.");
			return false;
		}
		FGameXXKBladeFinishRuntime& Finish = InOutRuntime.PendingBladeFinish;
		const bool bInsideFinishWindow = (InOutRuntime.Phase == EGameXXKCardBattlePhase::Enemy
				&& Finish.TriggerPlayerRound == InOutRuntime.RoundNumber + 1)
			|| (InOutRuntime.Phase == EGameXXKCardBattlePhase::Player
				&& Finish.TriggerPlayerRound == InOutRuntime.RoundNumber);
		if (Status == EGameXXKCardStatus::Bleed
			&& bInsideFinishWindow
			&& Finish.Rule == EGameXXKBladeFinishRule::PreserveFirstTwoBleedTriggers
			&& Finish.RemainingTriggers > 0)
		{
			if (--Finish.RemainingTriggers == 0)
			{
				Finish = FGameXXKBladeFinishRuntime();
			}
			return true;
		}
		if (Status == EGameXXKCardStatus::Bleed
			&& bInsideFinishWindow
			&& Finish.Rule == EGameXXKBladeFinishRule::DrawOnFirstThreeBleedTriggers
			&& Finish.RemainingTriggers > 0)
		{
			GameXXKCardRules::RemoveDefeatedPartyOwnerCards(InOutRuntime.Deck, InOutRuntime.Units);
			if (!GameXXKCardRules::DrawCards(InOutRuntime.Deck, 1, 0, &OutError))
			{
				return false;
			}
			if (--Finish.RemainingTriggers == 0)
			{
				Finish = FGameXXKBladeFinishRuntime();
			}
		}
		if (Status == EGameXXKCardStatus::Bleed
			&& bInsideFinishWindow
			&& Finish.Rule == EGameXXKBladeFinishRule::HealBladeBleedCapTwelve
			&& Finish.RemainingTriggers > 0
			&& TriggerSourceUnitId == Finish.SourceOwnerUnitId
			&& TriggeredHealthDamage > 0)
		{
			FGameXXKCardCombatUnit* Blade = FindCombatUnitById(InOutRuntime.Units, Finish.SourceOwnerUnitId);
			if (!Blade || !Blade->bLiving)
			{
				OutError = TEXT("A Bleed-healing Blade Finish lost its living owner.");
				return false;
			}
			const int32 RequestedHealing = FMath::Min(TriggeredHealthDamage, Finish.RemainingTriggers);
			const int32 AppliedHealing = InOutResult
				? ApplyAndRecordHealing(*InOutResult, Finish.SourceOwnerUnitId, *Blade, RequestedHealing)
				: GameXXKCardRules::HealCombatUnit(*Blade, RequestedHealing);
			Finish.RemainingTriggers -= AppliedHealing;
			if (Finish.RemainingTriggers == 0)
			{
				Finish = FGameXXKBladeFinishRuntime();
			}
		}
		if (DefaultConsumption > 0)
		{
			OutConsumedStacks = GameXXKCardRules::ConsumeCombatStatus(Target, Status, DefaultConsumption);
		}
		return true;
	}

	bool ResolveBleedAfterUnavoidedDirectHit(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FName TargetUnitId,
		const FName SourceUnitId,
		const EGameXXKCardResolutionOrigin Origin,
		const int32 DefaultConsumption,
		FGameXXKCardPlayResult& InOutResult,
		int32& OutTriggeredHealthDamage,
		FString& OutError)
	{
		OutTriggeredHealthDamage = 0;
		FGameXXKCardCombatUnit* Target = FindCombatUnitById(InOutRuntime.Units, TargetUnitId);
		const int32 TriggeredBleedStacks = Target && Target->bLiving
			? GameXXKCardRules::GetCombatStatusStacks(*Target, EGameXXKCardStatus::Bleed)
			: 0;
		if (TriggeredBleedStacks <= 0)
		{
			return true;
		}

		FGameXXKCardDamageResult BleedResult;
		if (!ApplyStatusHealthLoss(
			InOutRuntime,
			TargetUnitId,
			EGameXXKCardDamageCause::Bleed,
			TriggeredBleedStacks,
			BleedResult,
			OutError,
			&InOutResult))
		{
			return false;
		}
		Target = FindCombatUnitById(InOutRuntime.Units, TargetUnitId);
		if (!Target)
		{
			OutError = TEXT("A direct hit lost its target before Bleed decay.");
			return false;
		}
		BleedResult.SourceUnitId = SourceUnitId;
		BleedResult.ResolutionOrigin = Origin;
		if (!ResolveTriggeredStatusLayerConsumption(
			InOutRuntime,
			*Target,
			EGameXXKCardStatus::Bleed,
			DefaultConsumption,
			SourceUnitId,
			BleedResult.HealthDamage,
			BleedResult.StatusStacksConsumed,
			OutError,
			&InOutResult))
		{
			return false;
		}
		OutTriggeredHealthDamage = BleedResult.HealthDamage;
		InOutResult.DamageResults.Add(MoveTemp(BleedResult));
		return true;
	}

	bool ApplyPlayerCardDirectDamageInternal(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FGameXXKCardDamageContext& Context,
		const FName TargetUnitId,
		const int32 RequestedDamage,
		FGameXXKCardDamageResult& OutResult,
		FGameXXKCardPlayResult* InOutPlayResult,
		FString* OutError)
	{
		if (OutError)
		{
			OutError->Reset();
		}
		if (!IsDirectOrFixedDamageKind(Context.Kind) || Context.SourceUnitId.IsNone())
		{
			return SetFailure(OutError, TEXT("Player card damage requires a concrete direct-attack or fixed-damage context and source."));
		}
		FString ValidationError;
		if (!ValidateCardBattleRuntimeInternal(InOutRuntime, ValidationError))
		{
			return SetFailure(OutError, ValidationError);
		}
		const FGameXXKCardCombatUnit* Source = FindCombatUnitById(InOutRuntime.Units, Context.SourceUnitId);
		if (!Source || !Source->bLiving || Source->Side != EGameXXKCardTargetSide::Party)
		{
			return SetFailure(OutError, TEXT("Player card damage requires one living party source."));
		}

		FGameXXKCardBattleRuntime NewRuntime = InOutRuntime;
		FGameXXKCardDamageContext ResolvedContext = Context;
		if (IsDirectAttackDamageKind(ResolvedContext.Kind))
		{
			ResolvedContext.AgilityRollPercent = AdvanceCombatRandomRoll(NewRuntime);
		}
		FGameXXKCardDamageResult NewResult;
		FGameXXKCardPlayResult PendingAuditResult;
		if (!ApplyCombatDirectDamageInternal(
			NewRuntime.Units,
			NewRuntime.GuardLinks,
			ResolvedContext,
			TargetUnitId,
			RequestedDamage,
			NewResult,
			&NewRuntime,
			&NewRuntime,
			InOutPlayResult ? &PendingAuditResult : nullptr,
			false,
			&ValidationError))
		{
			return SetFailure(OutError, ValidationError);
		}
		if (!ValidateCardBattleRuntimeInternal(NewRuntime, ValidationError))
		{
			return SetFailure(OutError, ValidationError);
		}
		InOutRuntime = MoveTemp(NewRuntime);
		OutResult = MoveTemp(NewResult);
		if (InOutPlayResult)
		{
			InOutPlayResult->ArmorResults.Append(MoveTemp(PendingAuditResult.ArmorResults));
			InOutPlayResult->HealingResults.Append(MoveTemp(PendingAuditResult.HealingResults));
		}
		return true;
	}

	bool ResolveAttackPacket(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FGameXXKCardDefinition& Definition,
		const FGameXXKCardInstance& Instance,
		const TArray<FName>& CardTargetIds,
		const EGameXXKCardResolutionOrigin Origin,
		const bool bSkipMissingSelectedTargetEffects,
		const FGameXXKCardPlayConditionSnapshot& ConditionSnapshot,
		const int32 AttackIndex,
		TSet<int32>& OutAttachedEffectIndices,
		TMap<FName, int32>& InOutConsumptionResults,
		FGameXXKCardPlayResult& InOutResult,
		FString& OutError)
	{
		const FGameXXKCardEffect& Attack = Definition.Effects[AttackIndex];
		const bool bBloodEdgeAttack = Definition.Owner == EGameXXKCardOwner::Profession
			&& Definition.Role == EGameXXKCharacterRole::Blade
			&& Definition.ProfessionArchetypeIds.Contains(TEXT("Archetype.Blade.BloodEdge"));
		const bool bMomentumBreakAttack = Definition.Owner == EGameXXKCardOwner::Profession
			&& Definition.Role == EGameXXKCharacterRole::Blade
			&& Definition.ProfessionArchetypeIds.Contains(TEXT("Archetype.Blade.MomentumBreak"));
		const bool bPoJunVulnerabilityRule = Definition.BladeSequence.BaseRule
			== EGameXXKBladeBaseRule::ConsumeVulnerabilityForExtraAttacks;
		TArray<int32> AttachmentIndices;
		for (int32 Index = AttackIndex + 1; Index < Definition.Effects.Num() && Definition.Effects[Index].Type != EGameXXKCardEffectType::DamagePercentAttack; ++Index)
		{
			if (bBloodEdgeAttack
				&& Definition.Effects[Index].Type == EGameXXKCardEffectType::ApplyStatus
				&& Definition.Effects[Index].Status == EGameXXKCardStatus::Bleed)
			{
				continue;
			}
			if (IsAttackPacketAttachment(Definition.Effects[Index], Attack))
			{
				AttachmentIndices.Add(Index);
				OutAttachedEffectIndices.Add(Index);
			}
		}
		if (IsTerrainConditionDefinitelyFalse(Attack.Condition, InOutRuntime.Terrain))
		{
			return true;
		}
		if (bSkipMissingSelectedTargetEffects
			&& CardTargetIds.IsEmpty()
			&& Attack.Target == EGameXXKCardEffectTarget::SelectedTarget)
		{
			return true;
		}

		TArray<FName> TargetIds;
		if (!ResolveEffectTargetIds(InOutRuntime, Instance.OwnerUnitId, CardTargetIds, Attack.Target, TargetIds, OutError))
		{
			return false;
		}
		FName AttackSourceUnitId = NAME_None;
		if (!ResolveEffectSourceUnitId(
			InOutRuntime,
			Instance.OwnerUnitId,
			CardTargetIds,
			Attack.Source,
			AttackSourceUnitId,
			OutError))
		{
			return false;
		}
		const FGameXXKCardCombatUnit* PacketSource = FindCombatUnitById(InOutRuntime.Units, AttackSourceUnitId);
		const int32 MomentumAtPacketStart = PacketSource
			? GameXXKCardRules::GetCombatStatusStacks(*PacketSource, EGameXXKCardStatus::Momentum)
			: 0;
		bool bPreparedTriggeredAttack = false;
		int32 TriggeredAttackBonusPercent = 0;
		bool bApplyNextAttackVulnerability = false;
		bool bApplyNextAttackMark = false;
		for (const FName TargetId : TargetIds)
		{
			FGameXXKCardCombatUnit* Owner = FindCombatUnitById(InOutRuntime.Units, Instance.OwnerUnitId);
			FGameXXKCardCombatUnit* Target = FindCombatUnitById(InOutRuntime.Units, TargetId);
			if (!Owner || !Owner->bLiving)
			{
				return true;
			}
			if (!Target || !Target->bLiving)
			{
				continue;
			}
			const int32 LockedPoJunExtraHits = bPoJunVulnerabilityRule
				? FMath::Min(3, GameXXKCardRules::GetCombatStatusStacks(*Target, EGameXXKCardStatus::Vulnerability))
				: 0;
			int32 PoJunVulnerabilityConsumed = 0;
			FGameXXKCardCombatUnit* ConditionTarget = CardTargetIds.Num() == 1 ? FindCombatUnitById(InOutRuntime.Units, CardTargetIds[0]) : Target;
			bool bBaseSatisfied = false;
			int32 BaseConsumed = 0;
			if (!TryApplyEffectConditionAndConsumption(Attack.Condition, InOutRuntime, *Owner, ConditionTarget, &ConditionSnapshot, bBaseSatisfied, BaseConsumed, OutError))
			{
				return false;
			}
			if (!bBaseSatisfied)
			{
				continue;
			}
			if (!Attack.ConsumptionGroupId.IsNone())
			{
				InOutConsumptionResults.FindOrAdd(Attack.ConsumptionGroupId) += BaseConsumed;
			}

			int64 Percent = Attack.Magnitude;
			int64 FlatDamage = 0;
			int32 IgnoredDefense = 0;
			TArray<const FGameXXKCardEffect*> OnHitStatusEffects;
			for (const int32 AttachmentIndex : AttachmentIndices)
			{
				const FGameXXKCardEffect& Attachment = Definition.Effects[AttachmentIndex];
				if (!HasSuccessfulConsumptionReference(Attachment, InOutConsumptionResults))
				{
					continue;
				}
				Owner = FindCombatUnitById(InOutRuntime.Units, Instance.OwnerUnitId);
				Target = FindCombatUnitById(InOutRuntime.Units, TargetId);
				if (!Owner || !Owner->bLiving || !Target || !Target->bLiving)
				{
					break;
				}
				bool bAttachmentSatisfied = false;
				int32 AttachmentConsumed = 0;
				if (!TryApplyEffectConditionAndConsumption(Attachment.Condition, InOutRuntime, *Owner, ConditionTarget, &ConditionSnapshot, bAttachmentSatisfied, AttachmentConsumed, OutError))
				{
					return false;
				}
				if (!bAttachmentSatisfied)
				{
					continue;
				}
				if (!Attachment.ConsumptionGroupId.IsNone())
				{
					InOutConsumptionResults.FindOrAdd(Attachment.ConsumptionGroupId) += AttachmentConsumed;
				}
				switch (Attachment.Type)
				{
				case EGameXXKCardEffectType::DamageFlat:
					FlatDamage += Attachment.Magnitude;
					break;
				case EGameXXKCardEffectType::IgnoreDefense:
					IgnoredDefense += Attachment.MagnitudePolicy == EGameXXKCardMagnitudePolicy::DefenseIgnoreCoefficient
						? FGameXXKCombatScalingRules::ResolveDotAddition(Attachment.Magnitude, Instance.CurrentQuality, InOutRuntime.TeamMaxLevelSnapshot)
						: Attachment.Magnitude;
					break;
				case EGameXXKCardEffectType::BonusDamagePercent:
					if (Attachment.SecondaryMagnitude > 0)
					{
						if (!ConditionTarget || Attachment.Condition.Type != EGameXXKCardEffectConditionType::TargetHasStatus)
						{
							OutError = TEXT("A per-status attack bonus requires a concrete target-status condition.");
							return false;
						}
						const int32 StatusStacks = GetConditionStatusStacks(ConditionTarget, Attachment.Condition.Status, &ConditionSnapshot);
						Percent += static_cast<int64>(Attachment.Magnitude) * FMath::Min(StatusStacks, Attachment.SecondaryMagnitude);
					}
					else
					{
						Percent += Attachment.Magnitude;
					}
					break;
				case EGameXXKCardEffectType::BonusDamagePercentPerConsumedStatus:
				case EGameXXKCardEffectType::BonusDamagePercentPerConsumedArmor:
				{
					// A packet attachment can either consume state itself, or reference an
					// earlier producer in this card.  In the latter case the attachment has
					// no local consumption and must use the stable recorded producer result.
					const int32 ConsumedStackCount = Attachment.ConsumedStackResultRef.IsNone()
						? AttachmentConsumed
						: InOutConsumptionResults.FindRef(Attachment.ConsumedStackResultRef);
					Percent += static_cast<int64>(Attachment.Magnitude) * ConsumedStackCount;
					break;
				}
				case EGameXXKCardEffectType::ApplyStatus:
					OnHitStatusEffects.Add(&Attachment);
					break;
				default:
					OutError = TEXT("Attack packet contains an unsupported attached effect.");
					return false;
				}
			}

			Owner = FindCombatUnitById(InOutRuntime.Units, Instance.OwnerUnitId);
			FGameXXKCardCombatUnit* AttackSource = FindCombatUnitById(InOutRuntime.Units, AttackSourceUnitId);
			Target = FindCombatUnitById(InOutRuntime.Units, TargetId);
			if (!Owner || !Owner->bLiving || !AttackSource || !AttackSource->bLiving || !Target || !Target->bLiving)
			{
				continue;
			}
			if (!bPreparedTriggeredAttack)
			{
				if (Origin == EGameXXKCardResolutionOrigin::ActivePlay)
				{
					TArray<FName> TriggeredModifierIds;
					if (!CollectOnNextAttackBonuses(InOutRuntime, Definition, Instance, *AttackSource, ConditionTarget, TriggeredAttackBonusPercent, TriggeredModifierIds, OutError))
					{
						return false;
					}
					if (!ConsumeOnNextAttackModifiers(InOutRuntime, TriggeredModifierIds, OutError))
					{
						return false;
					}
					bApplyNextAttackVulnerability = GameXXKCardRules::GetCombatStatusStacks(*AttackSource, EGameXXKCardStatus::NextAttackAppliesVulnerability) > 0;
					bApplyNextAttackMark = GameXXKCardRules::GetCombatStatusStacks(*AttackSource, EGameXXKCardStatus::NextAttackBonus) > 0;
					if (bApplyNextAttackVulnerability)
					{
						GameXXKCardRules::ConsumeCombatStatus(*AttackSource, EGameXXKCardStatus::NextAttackAppliesVulnerability, 1);
					}
					if (bApplyNextAttackMark)
					{
						GameXXKCardRules::ConsumeCombatStatus(*AttackSource, EGameXXKCardStatus::NextAttackBonus, 1);
					}
				}
				bPreparedTriggeredAttack = true;
			}
			Percent += TriggeredAttackBonusPercent;
			for (int32 HitIndex = 0; HitIndex < Attack.HitCount; ++HitIndex)
			{
				const FGameXXKCardCombatUnit* HitOwner = FindCombatUnitById(InOutRuntime.Units, AttackSourceUnitId);
				const FGameXXKCardCombatUnit* HitTarget = FindCombatUnitById(InOutRuntime.Units, TargetId);
				if (!HitOwner || !HitOwner->bLiving || !HitTarget || !HitTarget->bLiving)
				{
					break;
				}
				const FName HitSourceUnitId = HitOwner->UnitId;
				const int32 LiveBleedStacks = bBloodEdgeAttack
					? GameXXKCardRules::GetCombatStatusStacks(*HitTarget, EGameXXKCardStatus::Bleed)
					: 0;
				const int64 HitPercent = FMath::Min<int64>(
					MAX_int32,
					Percent
						+ static_cast<int64>(LiveBleedStacks) * 2
						+ (bMomentumBreakAttack ? static_cast<int64>(MomentumAtPacketStart) * 10 : 0));
				const int64 RawDamage = static_cast<int64>(HitOwner->Attack) * HitPercent / 100 + FlatDamage;
				if (RawDamage <= 0 || RawDamage > MAX_int32 || IgnoredDefense < 0 || IgnoredDefense > MAX_int32)
				{
					OutError = TEXT("Attack packet produced unsupported damage or defense-ignore values.");
					return false;
				}
				FGameXXKCardDamageContext Context;
				Context.SourceUnitId = HitSourceUnitId;
				Context.ResolutionOrigin = Origin;
				Context.Kind = Attack.Target == EGameXXKCardEffectTarget::AllEnemies
					? EGameXXKCardDamageKind::GroupAttack
					: EGameXXKCardDamageKind::SingleTargetAttack;
				Context.IgnoredDefense = IgnoredDefense;
				Context.MomentumStacksOverride = MomentumAtPacketStart;
				if (bPoJunVulnerabilityRule)
				{
					Context.VulnerabilityStacksToConsumeOverride = HitIndex == 0 ? LockedPoJunExtraHits : 0;
				}
				for (const FGameXXKCardEffect* OnHitEffect : OnHitStatusEffects)
				{
					if (OnHitEffect->HitCount == Attack.HitCount || HitIndex < OnHitEffect->HitCount)
					{
						const int32 AppliedMagnitude = ResolveCardStatusApplicationAmount(
							InOutRuntime,
							*HitTarget,
							*OnHitEffect,
							Instance.CurrentQuality);
						if (AppliedMagnitude > 0)
						{
							FGameXXKCardStatusStack& Status = Context.OnHitStatuses.AddDefaulted_GetRef();
							Status.Status = OnHitEffect->Status;
							Status.Stacks = AppliedMagnitude;
						}
					}
				}
				if (HitIndex == 0 && bApplyNextAttackVulnerability)
				{
					FGameXXKCardStatusStack& Status = Context.OnHitStatuses.AddDefaulted_GetRef();
					Status.Status = EGameXXKCardStatus::Vulnerability;
					Status.Stacks = 1;
				}
				if (HitIndex == 0 && bApplyNextAttackMark)
				{
					FGameXXKCardStatusStack& Status = Context.OnHitStatuses.AddDefaulted_GetRef();
					Status.Status = EGameXXKCardStatus::Mark;
					Status.Stacks = 1;
				}
				FGameXXKCardDamageResult DamageResult;
				if (!ApplyPlayerCardDirectDamageInternal(
					InOutRuntime,
					Context,
					TargetId,
					static_cast<int32>(RawDamage),
					DamageResult,
					&InOutResult,
					&OutError))
				{
					return false;
				}
				InOutResult.DamageResults.Add(DamageResult);
				PoJunVulnerabilityConsumed += DamageResult.VulnerabilityStacksConsumed;
				if (!DamageResult.bAvoidedByAgility)
				{
					int32 TriggeredBleedDamage = 0;
					if (!ResolveBleedAfterUnavoidedDirectHit(
						InOutRuntime,
						TargetId,
						HitSourceUnitId,
						Origin,
						0,
						InOutResult,
						TriggeredBleedDamage,
						OutError))
					{
						return false;
					}
					if (Definition.BladeSequence.BaseRule == EGameXXKBladeBaseRule::HealFromTriggeredBleed
						&& TriggeredBleedDamage > 0)
					{
						if (FGameXXKCardCombatUnit* BleedHealer = FindCombatUnitById(InOutRuntime.Units, Instance.OwnerUnitId))
						{
							ApplyAndRecordHealing(InOutResult, Instance.OwnerUnitId, *BleedHealer, TriggeredBleedDamage);
						}
					}
				}
				if (!ResolveFirstDirectDamageReactiveModifiers(InOutRuntime, Context, DamageResult, &InOutResult.DamageResults, OutError, &InOutResult))
				{
					return false;
				}
				const FGameXXKCardCombatUnit* CurrentTarget = FindCombatUnitById(InOutRuntime.Units, TargetId);
				if (!CurrentTarget || !CurrentTarget->bLiving)
				{
					break;
				}
			}
			if (LockedPoJunExtraHits > PoJunVulnerabilityConsumed)
			{
				if (FGameXXKCardCombatUnit* VulnerabilityTarget = FindCombatUnitById(InOutRuntime.Units, TargetId))
				{
					PoJunVulnerabilityConsumed += GameXXKCardRules::ConsumeCombatStatus(
						*VulnerabilityTarget,
						EGameXXKCardStatus::Vulnerability,
						LockedPoJunExtraHits - PoJunVulnerabilityConsumed);
				}
			}
			for (int32 ExtraHitIndex = 0; ExtraHitIndex < LockedPoJunExtraHits; ++ExtraHitIndex)
			{
				const FGameXXKCardCombatUnit* ExtraHitOwner = FindCombatUnitById(InOutRuntime.Units, Instance.OwnerUnitId);
				const FGameXXKCardCombatUnit* ExtraHitTarget = FindCombatUnitById(InOutRuntime.Units, TargetId);
				if (!ExtraHitOwner || !ExtraHitOwner->bLiving || !ExtraHitTarget || !ExtraHitTarget->bLiving)
				{
					break;
				}
				const int64 ExtraHitPercent = static_cast<int64>(
					FGameXXKCombatScalingRules::ScaleContinuousCeil(50, Instance.CurrentQuality))
					+ (bMomentumBreakAttack ? static_cast<int64>(MomentumAtPacketStart) * 10 : 0)
					+ TriggeredAttackBonusPercent;
				const int64 ExtraRawDamage = static_cast<int64>(ExtraHitOwner->Attack) * ExtraHitPercent / 100 + FlatDamage;
				if (ExtraRawDamage <= 0 || ExtraRawDamage > MAX_int32)
				{
					OutError = TEXT("A Po Jun extra attack produced unsupported damage.");
					return false;
				}
				FGameXXKCardDamageContext ExtraContext;
				ExtraContext.SourceUnitId = ExtraHitOwner->UnitId;
				ExtraContext.ResolutionOrigin = Origin;
				ExtraContext.Kind = Attack.Target == EGameXXKCardEffectTarget::AllEnemies
					? EGameXXKCardDamageKind::GroupAttack
					: EGameXXKCardDamageKind::SingleTargetAttack;
				ExtraContext.IgnoredDefense = IgnoredDefense;
				ExtraContext.MomentumStacksOverride = MomentumAtPacketStart;
				ExtraContext.VulnerabilityStacksToConsumeOverride = 0;
				FGameXXKCardDamageResult ExtraResult;
				if (!GameXXKCardRules::ApplyPlayerCardDirectDamage(
					InOutRuntime,
					ExtraContext,
					TargetId,
					static_cast<int32>(ExtraRawDamage),
					ExtraResult,
					&OutError))
				{
					return false;
				}
				InOutResult.DamageResults.Add(ExtraResult);
				if (!ResolveFirstDirectDamageReactiveModifiers(
					InOutRuntime,
					ExtraContext,
					ExtraResult,
					&InOutResult.DamageResults,
					OutError,
					&InOutResult))
				{
					return false;
				}
			}
		}
		return true;
	}

	bool EffectReferencesOriginalSelectedTarget(const FGameXXKCardEffect& Effect)
	{
		const bool bTargetCondition = Effect.Condition.Type == EGameXXKCardEffectConditionType::TargetHasStatus
			|| Effect.Condition.Type == EGameXXKCardEffectConditionType::TargetHasAnyDamageOverTime
			|| Effect.Condition.Type == EGameXXKCardEffectConditionType::TargetHealthBelowPercent
			|| Effect.Condition.Type == EGameXXKCardEffectConditionType::TargetIsAlly
			|| Effect.Condition.Type == EGameXXKCardEffectConditionType::TargetIsEnemy;
		return Effect.Target == EGameXXKCardEffectTarget::SelectedTarget
			|| Effect.Target == EGameXXKCardEffectTarget::SelectedTargetSide
			|| Effect.Source == EGameXXKCardEffectSource::SelectedTarget
			|| Effect.Type == EGameXXKCardEffectType::EachLivingAllyAttackSelectedTarget
			|| bTargetCondition
			|| (Effect.Type == EGameXXKCardEffectType::ApplyBattleModifier
				&& Effect.Modifier.RecipientTarget == EGameXXKCardEffectTarget::SelectedTarget)
			|| (Effect.Type == EGameXXKCardEffectType::ApplyGuardLink
				&& Effect.GuardLink.Guardian == EGameXXKCardEffectTarget::SelectedTarget);
	}

	bool EffectRequiresLivingOriginalSelectedTarget(const FGameXXKCardEffect& Effect)
	{
		return Effect.Target == EGameXXKCardEffectTarget::SelectedTarget
			|| Effect.Source == EGameXXKCardEffectSource::SelectedTarget
			|| Effect.Type == EGameXXKCardEffectType::EachLivingAllyAttackSelectedTarget
			|| (Effect.Type == EGameXXKCardEffectType::ApplyBattleModifier
				&& Effect.Modifier.RecipientTarget == EGameXXKCardEffectTarget::SelectedTarget)
			|| (Effect.Type == EGameXXKCardEffectType::ApplyGuardLink
				&& (Effect.GuardLink.Guardian == EGameXXKCardEffectTarget::SelectedTarget
					|| Effect.GuardLink.ProtectedUnit == EGameXXKCardEffectTarget::SelectedTarget));
	}

	bool ResolveSnapshotTargetIds(
		const FGameXXKCardBattleRuntime& Runtime,
		const FGameXXKCardDefinition& Definition,
		const FGameXXKResolvedCardSnapshot& Snapshot,
		const FGameXXKCardInstance& SyntheticInstance,
		TArray<FName>& OutTargetIds,
		FString& OutError)
	{
		FGameXXKCardTargetRequest Request;
		FString TargetError;
		const bool bBuiltRequest = GameXXKCardRules::BuildTargetRequest(
			Definition,
			SyntheticInstance,
			Runtime.Terrain,
			BuildTargetUnitView(Runtime.Units),
			Request,
			&TargetError);
		if (!bBuiltRequest && Request.CardInstanceId.IsNone())
		{
			OutError = TargetError;
			return false;
		}
		if (Request.EffectiveMode == EGameXXKCardTargetMode::None)
		{
			OutTargetIds.Reset();
			return true;
		}

		TArray<FName> NewTargetIds;
		if (Snapshot.OriginalTargetUnitIds.IsEmpty())
		{
			if (!Request.AutomaticTargetUnitIds.IsEmpty())
			{
				NewTargetIds = Request.AutomaticTargetUnitIds;
			}
			else if (Request.EffectiveMode == EGameXXKCardTargetMode::RandomEnemy)
			{
				for (const FGameXXKCardTargetCandidateView& Candidate : Request.CandidateViews)
				{
					if (Candidate.bCanSelect)
					{
						NewTargetIds.Add(Candidate.UnitId);
						break;
					}
				}
			}
			OutTargetIds = MoveTemp(NewTargetIds);
			return true;
		}

		for (const FName OriginalTargetUnitId : Snapshot.OriginalTargetUnitIds)
		{
			const FGameXXKCardTargetCandidateView* OriginalCandidate = Request.CandidateViews.FindByPredicate(
				[OriginalTargetUnitId](const FGameXXKCardTargetCandidateView& Candidate)
				{
					return Candidate.UnitId == OriginalTargetUnitId;
				});
			if (OriginalCandidate && OriginalCandidate->bCanSelect && !NewTargetIds.Contains(OriginalTargetUnitId))
			{
				NewTargetIds.Add(OriginalTargetUnitId);
				continue;
			}

			const FGameXXKCardCombatUnit* OriginalUnit = FindCombatUnitById(Runtime.Units, OriginalTargetUnitId);
			const FGameXXKCardCombatUnit* Owner = FindCombatUnitById(Runtime.Units, Snapshot.OwnerUnitId);
			EGameXXKCardTargetSide RequiredSide = OriginalUnit
				? OriginalUnit->Side
				: EGameXXKCardTargetSide::Invalid;
			if (RequiredSide == EGameXXKCardTargetSide::Invalid && Owner)
			{
				switch (Request.EffectiveMode)
				{
				case EGameXXKCardTargetMode::SingleEnemy:
				case EGameXXKCardTargetMode::RandomEnemy:
				case EGameXXKCardTargetMode::AllEnemies:
					RequiredSide = Owner->Side == EGameXXKCardTargetSide::Party
						? EGameXXKCardTargetSide::Enemy
						: EGameXXKCardTargetSide::Party;
					break;
				case EGameXXKCardTargetMode::Self:
				case EGameXXKCardTargetMode::SingleAlly:
				case EGameXXKCardTargetMode::OtherAlly:
				case EGameXXKCardTargetMode::AllAllies:
				case EGameXXKCardTargetMode::AllOtherAllies:
				case EGameXXKCardTargetMode::LowestHealthAlly:
				case EGameXXKCardTargetMode::LowestHealthOtherAlly:
					RequiredSide = Owner->Side;
					break;
				default:
					break;
				}
			}

			for (const FGameXXKCardTargetCandidateView& Candidate : Request.CandidateViews)
			{
				const FGameXXKCardCombatUnit* CandidateUnit = FindCombatUnitById(Runtime.Units, Candidate.UnitId);
				if (Candidate.bCanSelect
					&& CandidateUnit
					&& (RequiredSide == EGameXXKCardTargetSide::Invalid || CandidateUnit->Side == RequiredSide)
					&& !NewTargetIds.Contains(Candidate.UnitId))
				{
					NewTargetIds.Add(Candidate.UnitId);
					break;
				}
			}
		}
		OutTargetIds = MoveTemp(NewTargetIds);
		return true;
	}

	bool BuildTerrainAmplifiedDefinition(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FGameXXKCardDefinition& Definition,
		const FGameXXKCardInstance& Instance,
		FGameXXKCardDefinition& OutDefinition,
		FString& OutError)
	{
		OutDefinition = Definition;
		if (!IsRouteTerrainCard(Definition))
		{
			return true;
		}
		const FGameXXKCardCombatUnit* Owner = FindCombatUnitById(InOutRuntime.Units, Instance.OwnerUnitId);
		if (!Owner || !Owner->bLiving)
		{
			OutError = TEXT("A terrain card requires a living stable owner before terrain bonuses can resolve.");
			return false;
		}
		TArray<FGameXXKCardCombatUnit*> OrderedAllies;
		for (FGameXXKCardCombatUnit& Candidate : InOutRuntime.Units)
		{
			if (Candidate.bLiving && Candidate.Side == Owner->Side)
			{
				OrderedAllies.Add(&Candidate);
			}
		}
		OrderedAllies.Sort([](const FGameXXKCardCombatUnit& Left, const FGameXXKCardCombatUnit& Right)
		{
			return IsStableUnitOrderBefore(Left, Right);
		});
		FGameXXKCardCombatUnit* DoubleSource = nullptr;
		EGameXXKCardStatus DoubleStatus = EGameXXKCardStatus::None;
		for (FGameXXKCardCombatUnit* Candidate : OrderedAllies)
		{
			if (GameXXKCardRules::GetCombatStatusStacks(*Candidate, EGameXXKCardStatus::TerrainBonusDoubleThisRound) > 0)
			{
				DoubleSource = Candidate;
				DoubleStatus = EGameXXKCardStatus::TerrainBonusDoubleThisRound;
				break;
			}
			if (GameXXKCardRules::GetCombatStatusStacks(*Candidate, EGameXXKCardStatus::TerrainBonusDouble) > 0)
			{
				DoubleSource = Candidate;
				DoubleStatus = EGameXXKCardStatus::TerrainBonusDouble;
				break;
			}
		}
		if (!DoubleSource)
		{
			return true;
		}
		if (GameXXKCardRules::ConsumeCombatStatus(*DoubleSource, DoubleStatus, 1) != 1)
		{
			OutError = TEXT("A terrain-bonus doubling status changed before the terrain card could commit.");
			return false;
		}

		OutDefinition.Effects.Reset();
		for (int32 EffectIndex = 0; EffectIndex < Definition.Effects.Num(); ++EffectIndex)
		{
			const FGameXXKCardEffect& Effect = Definition.Effects[EffectIndex];
			if (Effect.Type == EGameXXKCardEffectType::DamagePercentAttack
				&& Effect.Condition.Type == EGameXXKCardEffectConditionType::TerrainIsAny)
			{
				int32 GroupEndIndex = EffectIndex;
				while (GroupEndIndex + 1 < Definition.Effects.Num()
					&& Definition.Effects[GroupEndIndex + 1].Type != EGameXXKCardEffectType::DamagePercentAttack
					&& IsAttackPacketAttachment(Definition.Effects[GroupEndIndex + 1], Effect))
				{
					++GroupEndIndex;
				}
				for (int32 CopyIndex = EffectIndex; CopyIndex <= GroupEndIndex; ++CopyIndex)
				{
					OutDefinition.Effects.Add(Definition.Effects[CopyIndex]);
				}
				for (int32 CopyIndex = EffectIndex; CopyIndex <= GroupEndIndex; ++CopyIndex)
				{
					OutDefinition.Effects.Add(Definition.Effects[CopyIndex]);
				}
				EffectIndex = GroupEndIndex;
				continue;
			}
			OutDefinition.Effects.Add(Effect);
			if (Effect.Condition.Type == EGameXXKCardEffectConditionType::TerrainIsAny)
			{
				OutDefinition.Effects.Add(Effect);
			}
		}
		return true;
	}

	bool OpenHeroSpellTaskSearchChoice(
		FGameXXKCardBattleRuntime& InOutRuntime,
		FGameXXKCardPlayResult& InOutResult,
		FString& OutError)
	{
		if (!InOutRuntime.HeroSpellTask.bActive
			|| InOutRuntime.Deck.Hand.Num() >= BattleHandCapacity)
		{
			return true;
		}
		if (IsActiveChoice(InOutRuntime.Deck.PendingChoice.Kind))
		{
			OutError = TEXT("A Hero spell-task search cannot replace an active card choice.");
			return false;
		}

		const FGameXXKHeroSpellTaskRuntime& Task = InOutRuntime.HeroSpellTask;
		TArray<FGameXXKCardInstance> Candidates;
		const auto CollectZone = [&Task, &Candidates](const TArray<FGameXXKCardInstance>& Zone)
		{
			for (const FGameXXKCardInstance& Instance : Zone)
			{
				if (Instance.OwnerUnitId == Task.StarterOwnerUnitId
					&& Task.LockedHeroCardIds.Contains(Instance.CardId)
					&& !Task.CompletedHeroCardIds.Contains(Instance.CardId))
				{
					Candidates.Add(Instance);
				}
			}
		};
		CollectZone(InOutRuntime.Deck.DrawPile);
		CollectZone(InOutRuntime.Deck.DiscardPile);
		Candidates.Sort([](const FGameXXKCardInstance& Left, const FGameXXKCardInstance& Right)
		{
			return Left.AcquisitionOrdinal != Right.AcquisitionOrdinal
				? Left.AcquisitionOrdinal < Right.AcquisitionOrdinal
				: Left.InstanceId.LexicalLess(Right.InstanceId);
		});
		if (Candidates.IsEmpty())
		{
			return true;
		}

		ClearPendingChoice(InOutRuntime.Deck.PendingChoice);
		InOutRuntime.Deck.PendingChoice.Kind = EGameXXKCardPendingChoiceKind::HeroTaskSearchChooseToHand;
		InOutRuntime.Deck.PendingChoice.Candidates = MoveTemp(Candidates);
		InOutRuntime.Deck.PendingChoice.RequiredCount = 1;
		InOutRuntime.Deck.PendingChoice.RequiredHandPickCount = 1;
		InOutRuntime.Deck.PendingChoice.bCanCancel = false;
		InOutRuntime.Deck.PendingChoice.bCancelPreservesDrawTop = true;
		InOutResult.bOpenedPendingChoice = true;
		return true;
	}

	bool OpenTaskNpcSpellTaskSearchChoice(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FName OwnerUnitId,
		FGameXXKCardPlayResult& InOutResult,
		FString& OutError)
	{
		const FGameXXKTaskNpcSpellTaskRuntime* Task = InOutRuntime.TaskNpcSpellTasks.FindByPredicate(
			[OwnerUnitId](const FGameXXKTaskNpcSpellTaskRuntime& Candidate)
			{
				return Candidate.bActive && Candidate.OwnerUnitId == OwnerUnitId;
			});
		if (!Task || InOutRuntime.Deck.Hand.Num() >= BattleHandCapacity)
		{
			return true;
		}
		if (IsActiveChoice(InOutRuntime.Deck.PendingChoice.Kind))
		{
			OutError = TEXT("A task-NPC spell-task search cannot replace an active card choice.");
			return false;
		}

		TArray<FGameXXKCardInstance> Candidates;
		const auto CollectZone = [Task, &Candidates](const TArray<FGameXXKCardInstance>& Zone)
		{
			for (const FGameXXKCardInstance& Instance : Zone)
			{
				if (Instance.OwnerUnitId == Task->OwnerUnitId
					&& Task->LockedCardIds.Contains(Instance.CardId)
					&& !Task->CompletedCardIds.Contains(Instance.CardId))
				{
					Candidates.Add(Instance);
				}
			}
		};
		CollectZone(InOutRuntime.Deck.DrawPile);
		CollectZone(InOutRuntime.Deck.DiscardPile);
		Candidates.Sort([](const FGameXXKCardInstance& Left, const FGameXXKCardInstance& Right)
		{
			return Left.AcquisitionOrdinal != Right.AcquisitionOrdinal
				? Left.AcquisitionOrdinal < Right.AcquisitionOrdinal
				: Left.InstanceId.LexicalLess(Right.InstanceId);
		});
		if (Candidates.IsEmpty())
		{
			return true;
		}

		ClearPendingChoice(InOutRuntime.Deck.PendingChoice);
		InOutRuntime.Deck.PendingChoice.Kind = EGameXXKCardPendingChoiceKind::HeroTaskSearchChooseToHand;
		InOutRuntime.Deck.PendingChoice.Candidates = MoveTemp(Candidates);
		InOutRuntime.Deck.PendingChoice.RequiredCount = 1;
		InOutRuntime.Deck.PendingChoice.RequiredHandPickCount = 1;
		InOutRuntime.Deck.PendingChoice.bCanCancel = false;
		InOutRuntime.Deck.PendingChoice.bCancelPreservesDrawTop = true;
		InOutResult.bOpenedPendingChoice = true;
		return true;
	}

	TArray<FGameXXKCardInstance> CollectSorcererPartnerTaskSearchCandidates(
		const FGameXXKCardBattleRuntime& Runtime,
		const FName OwnerUnitId)
	{
		TArray<FGameXXKCardInstance> Candidates;
		const FGameXXKSorcererPartnerTaskRuntime* Task = Runtime.SorcererPartnerTasks.FindByPredicate(
			[OwnerUnitId](const FGameXXKSorcererPartnerTaskRuntime& Candidate)
			{
				return Candidate.bActive && Candidate.OwnerUnitId == OwnerUnitId;
			});
		if (!Task || Runtime.Deck.Hand.Num() >= BattleHandCapacity)
		{
			return Candidates;
		}

		const auto CollectZone = [Task, &Candidates](const TArray<FGameXXKCardInstance>& Zone)
		{
			for (const FGameXXKCardInstance& Instance : Zone)
			{
				if (!Instance.bTemporary
					&& Instance.OwnerUnitId == Task->OwnerUnitId
					&& Task->LockedCardIds.Contains(Instance.CardId)
					&& !Task->CompletedCardIds.Contains(Instance.CardId))
				{
					Candidates.Add(Instance);
				}
			}
		};
		CollectZone(Runtime.Deck.DrawPile);
		CollectZone(Runtime.Deck.DiscardPile);
		Candidates.Sort([](const FGameXXKCardInstance& Left, const FGameXXKCardInstance& Right)
		{
			return IsAutomaticHandOrderBefore(Left, Right);
		});
		return Candidates;
	}

	bool OpenSorcererPartnerTaskSearchChoice(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FName OwnerUnitId,
		FGameXXKCardPlayResult& InOutResult,
		FString& OutError)
	{
		const FGameXXKSorcererPartnerTaskRuntime* Task = InOutRuntime.SorcererPartnerTasks.FindByPredicate(
			[OwnerUnitId](const FGameXXKSorcererPartnerTaskRuntime& Candidate)
			{
				return Candidate.bActive && Candidate.OwnerUnitId == OwnerUnitId;
			});
		if (!Task || InOutRuntime.Deck.Hand.Num() >= BattleHandCapacity)
		{
			return true;
		}
		if (IsActiveChoice(InOutRuntime.Deck.PendingChoice.Kind))
		{
			OutError = TEXT("A Sorcerer partner task search cannot replace an active card choice.");
			return false;
		}

		TArray<FGameXXKCardInstance> Candidates = CollectSorcererPartnerTaskSearchCandidates(
			InOutRuntime,
			OwnerUnitId);
		if (Candidates.IsEmpty())
		{
			return true;
		}

		ClearPendingChoice(InOutRuntime.Deck.PendingChoice);
		InOutRuntime.Deck.PendingChoice.Kind = EGameXXKCardPendingChoiceKind::HeroTaskSearchChooseToHand;
		InOutRuntime.Deck.PendingChoice.Candidates = MoveTemp(Candidates);
		InOutRuntime.Deck.PendingChoice.RequiredCount = 1;
		InOutRuntime.Deck.PendingChoice.RequiredHandPickCount = 1;
		InOutRuntime.Deck.PendingChoice.bCanCancel = false;
		InOutRuntime.Deck.PendingChoice.bCancelPreservesDrawTop = true;
		InOutResult.bOpenedPendingChoice = true;
		return true;
	}

	bool SorcererDefinitionHasDirectDamage(const FGameXXKCardDefinition& Definition)
	{
		return Definition.Effects.ContainsByPredicate([](const FGameXXKCardEffect& Effect)
		{
			switch (Effect.Type)
			{
			case EGameXXKCardEffectType::DamagePercentAttack:
			case EGameXXKCardEffectType::DamagePercentAttackPerTargetStatus:
			case EGameXXKCardEffectType::DamagePercentAttackPlusArmor:
			case EGameXXKCardEffectType::DamageAllPercentAttackPerConsumedArmor:
			case EGameXXKCardEffectType::LightningPerTargetStatusSnapshot:
			case EGameXXKCardEffectType::EachLivingAllyAttackSelectedTarget:
				return true;
			default:
				return false;
			}
		});
	}

	const FGameXXKResolvedCardSnapshot* FindPreviousSorcererTaskSnapshot(
		const FGameXXKCardBattleRuntime& Runtime,
		const FGameXXKResolvedCardSnapshot& Snapshot)
	{
		if (Snapshot.SorcererSequencePosition <= 1)
		{
			return nullptr;
		}
		const FGameXXKSorcererPartnerTaskRuntime* Task = Runtime.SorcererPartnerTasks.FindByPredicate(
			[&Snapshot](const FGameXXKSorcererPartnerTaskRuntime& Candidate)
			{
				return Candidate.bActive && Candidate.OwnerUnitId == Snapshot.OwnerUnitId;
			});
		const int32 PreviousIndex = Snapshot.SorcererSequencePosition - 2;
		return Task && Task->FirstPlayOrder.IsValidIndex(PreviousIndex)
			? &Task->FirstPlayOrder[PreviousIndex]
			: nullptr;
	}

	bool ApplySorcererSequenceDefinition(
		const FGameXXKCardBattleRuntime& Runtime,
		const FGameXXKResolvedCardSnapshot& Snapshot,
		FGameXXKCardDefinition& InOutDefinition,
		FString& OutError)
	{
		OutError.Reset();
		if (InOutDefinition.Owner != EGameXXKCardOwner::Profession
			|| InOutDefinition.OwnerId != FName(TEXT("Profession.Sorcerer"))
			|| InOutDefinition.Role != EGameXXKCharacterRole::Sorcerer
			|| Snapshot.SorcererSequencePosition == 0)
		{
			return true;
		}
		if (InOutDefinition.SorcererRule.SequenceRule == EGameXXKSorcererSequenceRule::None)
		{
			OutError = TEXT("A sequenced Sorcerer card has no declarative sequence rule.");
			return false;
		}

		const auto FindEffect = [&InOutDefinition](const EGameXXKCardEffectType Type) -> FGameXXKCardEffect*
		{
			return InOutDefinition.Effects.FindByPredicate([Type](const FGameXXKCardEffect& Effect)
			{
				return Effect.Type == Type;
			});
		};
		const auto FindStatusEffect = [&InOutDefinition](
			const EGameXXKCardEffectType Type,
			const EGameXXKCardStatus Status) -> FGameXXKCardEffect*
		{
			return InOutDefinition.Effects.FindByPredicate([Type, Status](const FGameXXKCardEffect& Effect)
			{
				return Effect.Type == Type && Effect.Status == Status;
			});
		};
		const auto RequireEffect = [&OutError](FGameXXKCardEffect* Effect, const TCHAR* Context) -> bool
		{
			if (Effect)
			{
				return true;
			}
			OutError = FString::Printf(TEXT("A Sorcerer sequence rule lost its %s base effect."), Context);
			return false;
		};

		const int32 Position = Snapshot.SorcererSequencePosition;
		switch (InOutDefinition.SorcererRule.SequenceRule)
		{
		case EGameXXKSorcererSequenceRule::CoreSearch:
			return true;
		case EGameXXKSorcererSequenceRule::CoreManaEcho:
		{
			FGameXXKCardEffect* Mana = FindEffect(EGameXXKCardEffectType::GainMana);
			if (!RequireEffect(Mana, TEXT("Mana")))
			{
				return false;
			}
			int32 PreviousPaidMana = 0;
			if (Position > 1)
			{
				const FGameXXKResolvedCardSnapshot* Previous = FindPreviousSorcererTaskSnapshot(Runtime, Snapshot);
				if (!Previous || Previous->SorcererSequencePosition != Position - 1)
				{
					OutError = TEXT("A Sorcerer Mana echo cannot find its preceding first-play snapshot.");
					return false;
				}
				PreviousPaidMana = Previous->PaidManaCost;
			}
			Mana->Magnitude = 3 + PreviousPaidMana / 2;
			return true;
		}
		case EGameXXKSorcererSequenceRule::FireLamp:
		{
			FGameXXKCardEffect* Burn = FindStatusEffect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardStatus::Burn);
			if (!RequireEffect(Burn, TEXT("Burn")))
			{
				return false;
			}
			if (Position <= 2)
			{
				Burn->Magnitude = 4;
			}
			return true;
		}
		case EGameXXKSorcererSequenceRule::FireSpread:
		{
			FGameXXKCardEffect* Burn = FindStatusEffect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardStatus::Burn);
			if (!RequireEffect(Burn, TEXT("Burn")))
			{
				return false;
			}
			if (Snapshot.PreviousSorcererFamily == EGameXXKSorcererCardFamily::Fire)
			{
				Burn->Magnitude = 3;
			}
			return true;
		}
		case EGameXXKSorcererSequenceRule::FireBurst:
		{
			FGameXXKCardEffect* Attack = FindEffect(EGameXXKCardEffectType::DamagePercentAttack);
			if (!RequireEffect(Attack, TEXT("attack")))
			{
				return false;
			}
			if (Position >= 3)
			{
				Attack->Type = EGameXXKCardEffectType::DamagePercentAttackPerTargetStatus;
				Attack->Status = EGameXXKCardStatus::Burn;
				Attack->SecondaryMagnitude = 10;
			}
			return true;
		}
		case EGameXXKSorcererSequenceRule::FireSearch:
		{
			FGameXXKCardEffect* Attack = FindEffect(EGameXXKCardEffectType::DamagePercentAttack);
			if (!RequireEffect(Attack, TEXT("attack")))
			{
				return false;
			}
			if (Position >= 4)
			{
				Attack->Magnitude = 70;
			}
			return true;
		}
		case EGameXXKSorcererSequenceRule::IceCurrentManaRestore:
		{
			FGameXXKCardEffect* Mana = FindEffect(EGameXXKCardEffectType::GainMana);
			const FGameXXKCardCombatUnit* Owner = FindCombatUnitById(Runtime.Units, Snapshot.OwnerUnitId);
			if (!RequireEffect(Mana, TEXT("Mana")) || !Owner)
			{
				if (OutError.IsEmpty())
				{
					OutError = TEXT("An Ice Mana sequence has no living runtime owner.");
				}
				return false;
			}
			const int32 ManaGain = static_cast<int32>(static_cast<int64>(Owner->Mana) * 25 / 100);
			if (ManaGain > 0)
			{
				Mana->Type = EGameXXKCardEffectType::GainManaOverflowToArmor;
				Mana->Magnitude = 100;
				Mana->SecondaryMagnitude = ManaGain;
			}
			else
			{
				Mana->Magnitude = ManaGain;
			}
			return true;
		}
		case EGameXXKSorcererSequenceRule::IceMaxMana:
		{
			FGameXXKCardEffect* Armor = FindEffect(EGameXXKCardEffectType::AddArmor);
			if (!RequireEffect(Armor, TEXT("maximum-Mana")))
			{
				return false;
			}
			Armor->Type = EGameXXKCardEffectType::IncreaseMaxMana;
			Armor->Magnitude = 4;
			FGameXXKCardEffect BonusArmor;
			BonusArmor.Type = EGameXXKCardEffectType::AddArmor;
			BonusArmor.Target = Armor->Target;
			BonusArmor.Source = Armor->Source;
			BonusArmor.Magnitude = 4;
			InOutDefinition.Effects.Add(MoveTemp(BonusArmor));
			return true;
		}
		case EGameXXKSorcererSequenceRule::IceArmorDouble:
		{
			FGameXXKCardEffect* Armor = FindEffect(EGameXXKCardEffectType::AddArmor);
			const FGameXXKCardCombatUnit* Owner = FindCombatUnitById(Runtime.Units, Snapshot.OwnerUnitId);
			if (!RequireEffect(Armor, TEXT("armor")) || !Owner)
			{
				if (OutError.IsEmpty())
				{
					OutError = TEXT("An Ice armor sequence has no living runtime owner.");
				}
				return false;
			}
			Armor->Magnitude = Owner->Armor == 0 ? 4 : Owner->Armor;
			return true;
		}
		case EGameXXKSorcererSequenceRule::IceSearch:
			return true;
		case EGameXXKSorcererSequenceRule::LightningMark:
		case EGameXXKSorcererSequenceRule::LightningSearch:
		{
			FGameXXKCardEffect* Mark = FindStatusEffect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardStatus::Mark);
			if (!RequireEffect(Mark, TEXT("Mark")))
			{
				return false;
			}
			if (Position <= 2)
			{
				Mark->Magnitude = 3;
			}
			return true;
		}
		case EGameXXKSorcererSequenceRule::LightningMarkHits:
		case EGameXXKSorcererSequenceRule::LightningStorm:
		{
			FGameXXKCardEffect* Lightning = FindEffect(EGameXXKCardEffectType::LightningPerTargetStatusSnapshot);
			if (!RequireEffect(Lightning, TEXT("lightning")))
			{
				return false;
			}
			if (Position >= 4)
			{
				Lightning->Magnitude = InOutDefinition.SorcererRule.SequenceRule == EGameXXKSorcererSequenceRule::LightningMarkHits
					? 65
					: 45;
			}
			return true;
		}
		case EGameXXKSorcererSequenceRule::UniversalScalingAttack:
		{
			FGameXXKCardEffect* Attack = FindEffect(EGameXXKCardEffectType::DamagePercentAttack);
			if (!RequireEffect(Attack, TEXT("attack")))
			{
				return false;
			}
			Attack->Magnitude = 60 + 25 * (Position - 1);
			return true;
		}
		case EGameXXKSorcererSequenceRule::UniversalDraw:
		{
			if (Position >= 3)
			{
				FGameXXKCardEffect Mana;
				Mana.Type = EGameXXKCardEffectType::GainMana;
				Mana.Target = EGameXXKCardEffectTarget::CardOwner;
				Mana.Source = EGameXXKCardEffectSource::CardOwner;
				Mana.Magnitude = 5;
				InOutDefinition.Effects.Add(MoveTemp(Mana));
			}
			return true;
		}
		case EGameXXKSorcererSequenceRule::UniversalPartyArmor:
		{
			FGameXXKCardEffect* Armor = FindEffect(EGameXXKCardEffectType::AddArmor);
			if (!RequireEffect(Armor, TEXT("armor")))
			{
				return false;
			}
			if (Position > 1)
			{
				const FGameXXKResolvedCardSnapshot* Previous = FindPreviousSorcererTaskSnapshot(Runtime, Snapshot);
				const FGameXXKCardDefinition* PreviousDefinition = Previous
					? FGameXXKCardCatalog::FindCardDefinition(Previous->CardId)
					: nullptr;
				if (!PreviousDefinition)
				{
					OutError = TEXT("A Sorcerer party-armor sequence cannot find its preceding card definition.");
					return false;
				}
				Armor->Magnitude = SorcererDefinitionHasDirectDamage(*PreviousDefinition) ? 3 : 6;
			}
			return true;
		}
		case EGameXXKSorcererSequenceRule::UniversalSearch:
		{
			FGameXXKCardEffect* Attack = FindEffect(EGameXXKCardEffectType::DamagePercentAttack);
			if (!RequireEffect(Attack, TEXT("attack")))
			{
				return false;
			}
			if (Position >= 4)
			{
				Attack->Magnitude = 90;
			}
			return true;
		}
		case EGameXXKSorcererSequenceRule::None:
		default:
			OutError = TEXT("A Sorcerer card reached an unsupported sequence rule.");
			return false;
		}
	}

	void ApplySorcererIceBranchManaOverflow(
		const FGameXXKResolvedCardSnapshot& Snapshot,
		FGameXXKCardDefinition& InOutDefinition)
	{
		if (Snapshot.SorcererSequencePosition == 0
			|| Snapshot.SorcererTaskBranch != EGameXXKSorcererTaskBranch::Ice)
		{
			return;
		}
		for (FGameXXKCardEffect& Effect : InOutDefinition.Effects)
		{
			if (Effect.Type != EGameXXKCardEffectType::GainMana || Effect.Magnitude <= 0)
			{
				continue;
			}
			Effect.Type = EGameXXKCardEffectType::GainManaOverflowToArmor;
			Effect.SecondaryMagnitude = Effect.Magnitude;
			Effect.Magnitude = 100;
		}
	}

	bool ShouldApplySorcererCoreSearchDiscount(
		const FGameXXKCardBattleRuntime& Runtime,
		const FName OwnerUnitId,
		bool& OutShouldApply,
		FString& OutError)
	{
		OutShouldApply = false;
		OutError.Reset();
		const FGameXXKResolvedCardSnapshot* SearchSnapshot = nullptr;
		const FGameXXKAutomaticResolutionQueue& Queue = Runtime.AutomaticResolutionQueue;
		if (Queue.bActive
			&& Queue.Origin == EGameXXKCardResolutionOrigin::PartnerSorcererTaskReplay
			&& Queue.NextCardIndex > 0
			&& Queue.PendingCards.IsValidIndex(Queue.NextCardIndex - 1))
		{
			SearchSnapshot = &Queue.PendingCards[Queue.NextCardIndex - 1];
		}
		else
		{
			const FGameXXKSorcererPartnerTaskRuntime* Task = Runtime.SorcererPartnerTasks.FindByPredicate(
				[OwnerUnitId](const FGameXXKSorcererPartnerTaskRuntime& Candidate)
				{
					return Candidate.bActive && Candidate.OwnerUnitId == OwnerUnitId;
				});
			if (Task && !Task->FirstPlayOrder.IsEmpty())
			{
				SearchSnapshot = &Task->FirstPlayOrder.Last();
			}
		}
		if (!SearchSnapshot || SearchSnapshot->OwnerUnitId != OwnerUnitId)
		{
			OutError = TEXT("A Sorcerer task search choice lost the snapshot that opened it.");
			return false;
		}
		const FGameXXKCardDefinition* SearchDefinition = FGameXXKCardCatalog::FindCardDefinition(SearchSnapshot->CardId);
		if (!SearchDefinition)
		{
			OutError = TEXT("A Sorcerer task search choice lost its catalog source.");
			return false;
		}
		OutShouldApply = SearchDefinition->SorcererRule.SequenceRule == EGameXXKSorcererSequenceRule::CoreSearch
			&& SearchSnapshot->SorcererSequencePosition > 0
			&& SearchSnapshot->SorcererSequencePosition <= 2;
		return true;
	}

	bool AddHandBoundSorcererManaDiscount(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FGameXXKCardInstance& SelectedInstance,
		const FName SorcererUnitId,
		FString& OutError)
	{
		OutError.Reset();
		const FGameXXKCardCombatUnit* Sorcerer = FindCombatUnitById(InOutRuntime.Units, SorcererUnitId);
		if (!Sorcerer
			|| !Sorcerer->bLiving
			|| Sorcerer->Side != EGameXXKCardTargetSide::Party
			|| Sorcerer->Role != EGameXXKCharacterRole::Sorcerer
			|| SelectedInstance.InstanceId.IsNone()
			|| !IsCurrentHandInstance(InOutRuntime.Deck, SelectedInstance.InstanceId))
		{
			OutError = TEXT("A Sorcerer search discount requires its living Sorcerer and exact searched hand instance.");
			return false;
		}

		TSet<FName> ExistingModifierIds;
		for (const FGameXXKCardBattleModifierRuntime& ExistingModifier : InOutRuntime.Modifiers)
		{
			ExistingModifierIds.Add(ExistingModifier.ModifierId);
		}
		FName ModifierId = NAME_None;
		while (ModifierId.IsNone() || ExistingModifierIds.Contains(ModifierId))
		{
			if (InOutRuntime.NextModifierOrdinal == MAX_int32)
			{
				OutError = TEXT("Battle modifier ordinal has exhausted the supported range.");
				return false;
			}
			ModifierId = FName(*FString::Printf(TEXT("Modifier.%d"), InOutRuntime.NextModifierOrdinal++));
		}

		FGameXXKCardBattleModifierRuntime& NewModifier = InOutRuntime.Modifiers.AddDefaulted_GetRef();
		NewModifier.ModifierId = ModifierId;
		NewModifier.RequiredPlayedCardInstanceId = SelectedInstance.InstanceId;
		NewModifier.SourceCardInstanceId = SelectedInstance.InstanceId;
		NewModifier.SourceUnitId = SorcererUnitId;
		NewModifier.Definition.Trigger = EGameXXKCardBattleModifierTrigger::OnCardPlayed;
		NewModifier.Definition.EffectType = EGameXXKCardEffectType::ModifyManaCost;
		NewModifier.Definition.Target = EGameXXKCardEffectTarget::PlayedCard;
		NewModifier.Definition.RecipientScope = EGameXXKCardModifierRecipientScope::SharedDeck;
		NewModifier.Definition.RecipientTarget = EGameXXKCardEffectTarget::PlayedCard;
		NewModifier.Definition.Expiry = EGameXXKCardModifierExpiry::AfterTriggerCount;
		NewModifier.Definition.Magnitude = -3;
		NewModifier.Definition.RemainingTriggers = 1;
		NewModifier.Definition.bPersistent = true;
		return true;
	}

	bool AddSorcererSearchFallbackWhenUnavailable(
		const FGameXXKCardBattleRuntime& Runtime,
		const FName OwnerUnitId,
		FGameXXKCardDefinition& InOutDefinition,
		FString& OutError)
	{
		if (InOutDefinition.Owner != EGameXXKCardOwner::Profession
			|| InOutDefinition.OwnerId != FName(TEXT("Profession.Sorcerer"))
			|| InOutDefinition.Role != EGameXXKCharacterRole::Sorcerer
			|| !InOutDefinition.Effects.ContainsByPredicate([](const FGameXXKCardEffect& Effect)
			{
				return Effect.Type == EGameXXKCardEffectType::SearchUnfinishedHeroTaskCard;
			})
			|| !CollectSorcererPartnerTaskSearchCandidates(Runtime, OwnerUnitId).IsEmpty())
		{
			return true;
		}

		const int32 SearchIndex = InOutDefinition.Effects.IndexOfByPredicate([](const FGameXXKCardEffect& Effect)
		{
			return Effect.Type == EGameXXKCardEffectType::SearchUnfinishedHeroTaskCard;
		});
		const EGameXXKCardEffectType FallbackType = InOutDefinition.SorcererRule.SequenceRule == EGameXXKSorcererSequenceRule::IceSearch
			? EGameXXKCardEffectType::GainArmorFromCurrentManaPercent
			: EGameXXKCardEffectType::DamagePercentAttack;
		int32 FallbackIndex = INDEX_NONE;
		for (int32 Index = SearchIndex - 1; Index >= 0; --Index)
		{
			if (InOutDefinition.Effects[Index].Type == FallbackType)
			{
				FallbackIndex = Index;
				break;
			}
		}
		if (SearchIndex == INDEX_NONE || FallbackIndex == INDEX_NONE)
		{
			OutError = TEXT("A Sorcerer task search fallback requires its preceding repeatable base effect.");
			return false;
		}
		const FGameXXKCardEffect FallbackEffect = InOutDefinition.Effects[FallbackIndex];
		InOutDefinition.Effects.Insert(FallbackEffect, SearchIndex + 1);
		return true;
	}

	FGameXXKCardCombatUnit* FindLivingTerrainEnemyAnchor(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FName OwnerUnitId,
		const FName PreferredTargetUnitId)
	{
		const FGameXXKCardCombatUnit* Owner = FindCombatUnitById(InOutRuntime.Units, OwnerUnitId);
		if (!Owner || !Owner->bLiving)
		{
			return nullptr;
		}
		FGameXXKCardCombatUnit* Preferred = PreferredTargetUnitId.IsNone()
			? nullptr
			: FindCombatUnitById(InOutRuntime.Units, PreferredTargetUnitId);
		if (Preferred && Preferred->bLiving && Preferred->Side != Owner->Side)
		{
			return Preferred;
		}

		FGameXXKCardCombatUnit* Fallback = nullptr;
		for (FGameXXKCardCombatUnit& Candidate : InOutRuntime.Units)
		{
			if (!Candidate.bLiving || Candidate.Side == Owner->Side)
			{
				continue;
			}
			if (!Fallback || IsStableUnitOrderBefore(Candidate, *Fallback))
			{
				Fallback = &Candidate;
			}
		}
		return Fallback;
	}

	bool ConsumeTriggeredModifierUse(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FName ModifierId,
		FString& OutError);

	bool ResolveTerrainBenefit(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FGameXXKCardInstance& SourceInstance,
		const FName PreferredEnemyTargetUnitId,
		const EGameXXKCardTerrain Terrain,
		const int32 Repetitions,
		const EGameXXKCardResolutionOrigin Origin,
		FGameXXKCardPlayResult* InOutResult,
		FString& OutError)
	{
		if (!IsConcreteTerrain(Terrain) || Repetitions <= 0)
		{
			OutError = TEXT("A terrain benefit requires a concrete terrain and positive repetition count.");
			return false;
		}
		const FGameXXKCardCombatUnit* Owner = FindCombatUnitById(InOutRuntime.Units, SourceInstance.OwnerUnitId);
		if (!Owner || !Owner->bLiving)
		{
			return true;
		}

		int32 EffectiveRepetitions = Repetitions;
		TArray<FName> CountOverrideIds;
		const FGameXXKCardDefinition* SourceDefinition = FGameXXKCardCatalog::FindCardDefinition(SourceInstance.CardId);
		const FGameXXKCardCombatUnit* ConditionTarget = FindLivingTerrainEnemyAnchor(
			InOutRuntime, SourceInstance.OwnerUnitId, PreferredEnemyTargetUnitId);
		for (const FGameXXKCardBattleModifierRuntime& Modifier : InOutRuntime.Modifiers)
		{
			const FGameXXKCardBattleModifier& Definition = Modifier.Definition;
			if (Definition.Trigger != EGameXXKCardBattleModifierTrigger::BeforeNextTerrainBenefit)
			{
				continue;
			}
			if (Definition.EffectType != EGameXXKCardEffectType::TriggerTerrainBenefit || Definition.Magnitude <= 0
				|| Definition.Condition.bConsumeStatus || Definition.Condition.bConsumeOwnerArmor)
			{
				OutError = TEXT("A terrain count override requires a positive terrain count and a non-consuming condition.");
				return false;
			}
			const FGameXXKCardCombatUnit* ModifierOwner = FindCombatUnitById(InOutRuntime.Units, Modifier.SourceUnitId);
			if (!ModifierOwner || !ModifierOwner->bLiving || ModifierOwner->Side != Owner->Side
				|| (Definition.bActivePlayOnly && Origin != EGameXXKCardResolutionOrigin::ActivePlay)
				|| (Definition.bExcludeSourceUnit && Modifier.SourceUnitId == SourceInstance.OwnerUnitId)
				|| (Definition.RecipientScope != EGameXXKCardModifierRecipientScope::SharedDeck
					&& !Modifier.RecipientUnitIds.Contains(SourceInstance.OwnerUnitId))
				|| (!Modifier.RequiredPlayedCardInstanceId.IsNone()
					&& Modifier.RequiredPlayedCardInstanceId != SourceInstance.InstanceId)
				|| (Definition.RequiredTriggeredRole != EGameXXKCharacterRole::Invalid
					&& Definition.RequiredTriggeredRole != Owner->Role)
				|| (!Definition.RequiredTriggeredOwnerId.IsNone()
					&& (!SourceDefinition || Definition.RequiredTriggeredOwnerId != SourceDefinition->OwnerId)))
			{
				continue;
			}
			bool bSatisfied = false;
			if (!IsConditionSatisfied(Definition.Condition, InOutRuntime, *Owner, ConditionTarget, nullptr, bSatisfied, OutError))
			{
				return false;
			}
			if (bSatisfied)
			{
				// Assign the total count for this event, without emitting a second benefit.
				EffectiveRepetitions = Definition.Magnitude;
				CountOverrideIds.Add(Modifier.ModifierId);
			}
		}
		for (const FName ModifierId : CountOverrideIds)
		{
			if (!ConsumeTriggeredModifierUse(InOutRuntime, ModifierId, OutError))
			{
				return false;
			}
		}

		for (int32 Repetition = 0; Repetition < EffectiveRepetitions; ++Repetition)
		{
			Owner = FindCombatUnitById(InOutRuntime.Units, SourceInstance.OwnerUnitId);
			if (!Owner || !Owner->bLiving)
			{
				return true;
			}
			switch (Terrain)
			{
			case EGameXXKCardTerrain::Plain:
			{
				FGameXXKCardCombatUnit* Target = FindLivingTerrainEnemyAnchor(
					InOutRuntime,
					SourceInstance.OwnerUnitId,
					PreferredEnemyTargetUnitId);
				if (Target && !GrantStatusFromCardEffect(InOutRuntime, *Target, EGameXXKCardStatus::Burn, 2, OutError, InOutResult, SourceInstance.OwnerUnitId))
				{
					return false;
				}
				break;
			}
			case EGameXXKCardTerrain::Cliff:
			{
				FGameXXKCardCombatUnit* Target = FindLivingTerrainEnemyAnchor(
					InOutRuntime,
					SourceInstance.OwnerUnitId,
					PreferredEnemyTargetUnitId);
				if (Target
					&& (!GrantStatusFromCardEffect(InOutRuntime, *Target, EGameXXKCardStatus::Vulnerability, 2, OutError, InOutResult, SourceInstance.OwnerUnitId)
						|| !GrantStatusFromCardEffect(InOutRuntime, *Target, EGameXXKCardStatus::Mark, 1, OutError, InOutResult, SourceInstance.OwnerUnitId)))
				{
					return false;
				}
				break;
			}
			case EGameXXKCardTerrain::Forest:
				for (FGameXXKCardCombatUnit& Candidate : InOutRuntime.Units)
				{
					if (Candidate.bLiving && Candidate.Side == Owner->Side)
					{
						if (InOutResult)
						{
							ApplyAndRecordHealing(*InOutResult, SourceInstance.OwnerUnitId, Candidate, 4);
						}
						else
						{
							GameXXKCardRules::HealCombatUnit(Candidate, 4);
						}
					}
				}
				break;
			case EGameXXKCardTerrain::WaterShore:
			case EGameXXKCardTerrain::Ferry:
				for (FGameXXKCardCombatUnit& Candidate : InOutRuntime.Units)
				{
					if (Candidate.bLiving && Candidate.Side == Owner->Side)
					{
						Candidate.Mana = static_cast<int32>(FMath::Min<int64>(
							Candidate.MaxMana,
							static_cast<int64>(Candidate.Mana) + 3));
					}
				}
				break;
			case EGameXXKCardTerrain::Village:
				GameXXKCardRules::RemoveDefeatedPartyOwnerCards(InOutRuntime.Deck, InOutRuntime.Units);
				if (!GameXXKCardRules::DrawCards(InOutRuntime.Deck, 1, 0, &OutError))
				{
					return false;
				}
				for (FGameXXKCardCombatUnit& Candidate : InOutRuntime.Units)
				{
					if (Candidate.bLiving && Candidate.Side == Owner->Side)
					{
						if (InOutResult)
						{
							ApplyAndRecordArmor(*InOutResult, SourceInstance.OwnerUnitId, Candidate, 4);
						}
						else
						{
							GameXXKCardRules::AddCombatArmor(Candidate, 4);
						}
					}
				}
				break;
			case EGameXXKCardTerrain::Cave:
			{
				TArray<FName> AllyUnitIds;
				for (const FGameXXKCardCombatUnit& Candidate : InOutRuntime.Units)
				{
					if (Candidate.bLiving && Candidate.Side == Owner->Side)
					{
						AllyUnitIds.Add(Candidate.UnitId);
					}
				}
				AllyUnitIds.Sort([&InOutRuntime](const FName LeftId, const FName RightId)
				{
					const FGameXXKCardCombatUnit* Left = FindCombatUnitById(InOutRuntime.Units, LeftId);
					const FGameXXKCardCombatUnit* Right = FindCombatUnitById(InOutRuntime.Units, RightId);
					return Left && Right && IsStableUnitOrderBefore(*Left, *Right);
				});
				for (const FName AllyUnitId : AllyUnitIds)
				{
					FGameXXKCardCombatUnit* Ally = FindCombatUnitById(InOutRuntime.Units, AllyUnitId);
					if (!Ally || !Ally->bLiving)
					{
						continue;
					}
					if (InOutResult)
					{
						ApplyAndRecordArmor(*InOutResult, SourceInstance.OwnerUnitId, *Ally, 8);
					}
					else
					{
						GameXXKCardRules::AddCombatArmor(*Ally, 8);
					}
					if (!RegisterPartyReactionUses(
						InOutRuntime,
						SourceInstance,
						AllyUnitId,
						EGameXXKCardStatus::Block,
						1,
						OutError))
					{
						return false;
					}
				}
				break;
			}
			case EGameXXKCardTerrain::Invalid:
			default:
				OutError = TEXT("A terrain benefit reached an unsupported terrain.");
				return false;
			}
		}
		return true;
	}

	bool ResolveDefinitionEffects(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FGameXXKCardDefinition& Definition,
		const FGameXXKCardInstance& Instance,
		const TArray<FName>& CardTargetIds,
		const EGameXXKCardResolutionOrigin Origin,
		const bool bSkipMissingSelectedTargetEffects,
		FGameXXKCardPlayResult& InOutResult,
		FString& OutError)
	{
		const FGameXXKCardPlayConditionSnapshot ConditionSnapshot = CaptureCardPlayConditionSnapshot(InOutRuntime);
		const int32 InitialDamageResultCount = InOutResult.DamageResults.Num();
		TSet<int32> AttachedEffectIndices;
		TMap<FName, int32> ConsumptionResults;
		TMap<FName, int32> EffectResults;
		int32 DeferredForcedDiscardCount = 0;
		bool bPreparedHealingAction = false;
		int32 HealingBonusPercent = 0;
		int32 HealingFlatBonus = 0;
		bool bPreparedMedicineAction = false;
		int32 MedicineSnapshot = 0;
		FName LockedHighestArmorAllyId = NAME_None;
		for (int32 EffectIndex = 0; EffectIndex < Definition.Effects.Num(); ++EffectIndex)
		{
			if (AttachedEffectIndices.Contains(EffectIndex))
			{
				continue;
			}
			const FGameXXKCardEffect& Effect = Definition.Effects[EffectIndex];
			if (Effect.Type != EGameXXKCardEffectType::DamagePercentAttack
				&& IsTerrainConditionDefinitelyFalse(Effect.Condition, InOutRuntime.Terrain))
			{
				continue;
			}
			if (EffectReferencesOriginalSelectedTarget(Effect))
			{
				const FGameXXKCardCombatUnit* CurrentSelectedTarget = CardTargetIds.Num() == 1
					? FindCombatUnitById(InOutRuntime.Units, CardTargetIds[0])
					: nullptr;
				if (bSkipMissingSelectedTargetEffects
					|| !CurrentSelectedTarget
					|| (EffectRequiresLivingOriginalSelectedTarget(Effect) && !CurrentSelectedTarget->bLiving))
				{
					continue;
				}
			}
			if (Effect.Type == EGameXXKCardEffectType::DamagePercentAttack)
			{
				if (!ResolveAttackPacket(InOutRuntime, Definition, Instance, CardTargetIds, Origin, bSkipMissingSelectedTargetEffects, ConditionSnapshot, EffectIndex, AttachedEffectIndices, ConsumptionResults, InOutResult, OutError))
				{
					return false;
				}
				continue;
			}
			if (Effect.Type == EGameXXKCardEffectType::DamageFlat
				|| Effect.Type == EGameXXKCardEffectType::IgnoreDefense
				|| Effect.Type == EGameXXKCardEffectType::BonusDamagePercent
				|| Effect.Type == EGameXXKCardEffectType::BonusDamagePercentPerConsumedStatus
				|| Effect.Type == EGameXXKCardEffectType::BonusDamagePercentPerConsumedArmor)
			{
				OutError = TEXT("An attack-packet modifier was not attached to a preceding attack effect.");
				return false;
			}
			if (!HasSuccessfulConsumptionReference(Effect, ConsumptionResults))
			{
				continue;
			}
			if (!Effect.ResultRef.IsNone() && EffectResults.FindRef(Effect.ResultRef) <= 0)
			{
				continue;
			}
			if (Effect.Type == EGameXXKCardEffectType::ChangeTerrain)
			{
				if (!IsConcreteTerrain(Effect.TerrainOverride))
				{
					OutError = TEXT("A terrain-switch card requires a concrete destination terrain.");
					return false;
				}
				const bool bChanged = InOutRuntime.Terrain != Effect.TerrainOverride;
				if (bChanged)
				{
					InOutRuntime.Terrain = Effect.TerrainOverride;
					InOutRuntime.bTerrainChangedThisRound = true;
				}
				if (!Effect.ResultGroupId.IsNone())
				{
					EffectResults.FindOrAdd(Effect.ResultGroupId) = bChanged ? 1 : 0;
				}
				continue;
			}
			if (Effect.Type == EGameXXKCardEffectType::TriggerTerrainBenefit)
			{
				FGameXXKCardCombatUnit* TerrainOwner = FindCombatUnitById(InOutRuntime.Units, Instance.OwnerUnitId);
				if (!TerrainOwner || !TerrainOwner->bLiving)
				{
					return true;
				}
				FGameXXKCardCombatUnit* TerrainConditionTarget = CardTargetIds.Num() == 1
					? FindCombatUnitById(InOutRuntime.Units, CardTargetIds[0])
					: nullptr;
				bool bConditionSatisfied = false;
				int32 Consumed = 0;
				if (!TryApplyEffectConditionAndConsumption(
					Effect.Condition,
					InOutRuntime,
					*TerrainOwner,
					TerrainConditionTarget,
					&ConditionSnapshot,
					bConditionSatisfied,
					Consumed,
					OutError))
				{
					return false;
				}
				if (!bConditionSatisfied)
				{
					continue;
				}
				if (!Effect.ConsumptionGroupId.IsNone())
				{
					ConsumptionResults.FindOrAdd(Effect.ConsumptionGroupId) += Consumed;
				}
				const bool bUsesChangedTerrainBranch = InOutRuntime.bTerrainChangedThisRound
					&& Effect.SecondaryMagnitude > 0;
				const int32 Repetitions = bUsesChangedTerrainBranch
					? Effect.SecondaryMagnitude
					: Effect.Magnitude;
				const EGameXXKCardTerrain BenefitTerrain = Effect.TerrainOverride == EGameXXKCardTerrain::Invalid
					? InOutRuntime.Terrain
					: Effect.TerrainOverride;
				if (!ResolveTerrainBenefit(
					InOutRuntime,
					Instance,
					CardTargetIds.Num() == 1 ? CardTargetIds[0] : NAME_None,
					BenefitTerrain,
					Repetitions,
					Origin,
					&InOutResult,
					OutError))
				{
					return false;
				}
				if (!Effect.ResultGroupId.IsNone())
				{
					EffectResults.FindOrAdd(Effect.ResultGroupId) = bUsesChangedTerrainBranch ? 1 : 0;
				}
				continue;
			}
			if (Effect.Type == EGameXXKCardEffectType::ApplyBattleModifier)
			{
				FGameXXKCardCombatUnit* ModifierOwner = FindCombatUnitById(InOutRuntime.Units, Instance.OwnerUnitId);
				if (!ModifierOwner || !ModifierOwner->bLiving)
				{
					OutError = TEXT("Card owner was defeated before its persistent modifier could register.");
					return false;
				}
				FGameXXKCardCombatUnit* ModifierConditionTarget = CardTargetIds.Num() == 1
					? FindCombatUnitById(InOutRuntime.Units, CardTargetIds[0])
					: nullptr;
				bool bConditionSatisfied = false;
				int32 Consumed = 0;
				if (!TryApplyEffectConditionAndConsumption(Effect.Condition, InOutRuntime, *ModifierOwner, ModifierConditionTarget, &ConditionSnapshot, bConditionSatisfied, Consumed, OutError))
				{
					return false;
				}
				if (!bConditionSatisfied)
				{
					continue;
				}
				if (!Effect.ConsumptionGroupId.IsNone())
				{
					ConsumptionResults.FindOrAdd(Effect.ConsumptionGroupId) += Consumed;
				}
				if (!RegisterBattleModifier(InOutRuntime, Instance, CardTargetIds, Effect.Modifier, OutError))
				{
					return false;
				}
				continue;
			}
			if (Effect.Type == EGameXXKCardEffectType::DamagePercentAttackPlusArmor
				|| Effect.Type == EGameXXKCardEffectType::DamageAllPercentAttackPerConsumedArmor)
			{
				FGameXXKCardCombatUnit* GuardEffectOwner = FindCombatUnitById(InOutRuntime.Units, Instance.OwnerUnitId);
				if (!GuardEffectOwner || !GuardEffectOwner->bLiving)
				{
					return true;
				}
				FGameXXKCardCombatUnit* GuardConditionTarget = CardTargetIds.Num() == 1
					? FindCombatUnitById(InOutRuntime.Units, CardTargetIds[0])
					: nullptr;
				bool bConditionSatisfied = false;
				int32 Consumed = 0;
				if (!TryApplyEffectConditionAndConsumption(
					Effect.Condition,
					InOutRuntime,
					*GuardEffectOwner,
					GuardConditionTarget,
					&ConditionSnapshot,
					bConditionSatisfied,
					Consumed,
					OutError))
				{
					return false;
				}
				if (!bConditionSatisfied)
				{
					continue;
				}
				if (!Effect.ConsumptionGroupId.IsNone())
				{
					ConsumptionResults.FindOrAdd(Effect.ConsumptionGroupId) += Consumed;
				}

				TArray<FName> GuardTargetIds;
				if (!ResolveEffectTargetIds(
					InOutRuntime,
					Instance.OwnerUnitId,
					CardTargetIds,
					Effect.Target,
					GuardTargetIds,
					OutError))
				{
					return false;
				}
				FName SourceUnitId = NAME_None;
				if (!ResolveEffectSourceUnitId(
					InOutRuntime,
					Instance.OwnerUnitId,
					CardTargetIds,
					Effect.Source,
					SourceUnitId,
					OutError))
				{
					return false;
				}
				if (Effect.Source == EGameXXKCardEffectSource::HighestArmorAlly)
				{
					LockedHighestArmorAllyId = SourceUnitId;
				}
				FGameXXKCardCombatUnit* SourceUnit = FindCombatUnitById(InOutRuntime.Units, SourceUnitId);
				if (!SourceUnit || Effect.Magnitude <= 0 || Effect.SecondaryMagnitude < 0)
				{
					OutError = TEXT("An armor-conversion attack has invalid source or percentage data.");
					return false;
				}
				const int32 SourceAttack = SourceUnit->Attack;
				const int32 ArmorSnapshot = SourceUnit->Armor;
				int64 Percent = Effect.Magnitude;
				if (Effect.Type == EGameXXKCardEffectType::DamageAllPercentAttackPerConsumedArmor)
				{
					Percent += static_cast<int64>(Effect.SecondaryMagnitude) * ArmorSnapshot;
				}
				if (Percent <= 0 || (SourceAttack > 0 && Percent > MAX_int64 / SourceAttack))
				{
					OutError = TEXT("An armor-conversion attack percentage exceeds the supported range.");
					return false;
				}
				int64 RawDamage = static_cast<int64>(SourceAttack) * Percent / 100;
				if (Effect.Type == EGameXXKCardEffectType::DamagePercentAttackPlusArmor)
				{
					RawDamage += ArmorSnapshot;
				}
				else
				{
					SourceUnit->Armor = 0;
				}
				if (RawDamage <= 0 || RawDamage > MAX_int32)
				{
					OutError = TEXT("An armor-conversion attack produced an unsupported damage amount.");
					return false;
				}

				for (const FName GuardTargetId : GuardTargetIds)
				{
					const FGameXXKCardCombatUnit* CurrentSource = FindCombatUnitById(InOutRuntime.Units, SourceUnitId);
					const FGameXXKCardCombatUnit* CurrentTarget = FindCombatUnitById(InOutRuntime.Units, GuardTargetId);
					if (!CurrentSource || !CurrentSource->bLiving)
					{
						break;
					}
					if (!CurrentTarget || !CurrentTarget->bLiving)
					{
						continue;
					}
					FGameXXKCardDamageContext Context;
					Context.SourceUnitId = SourceUnitId;
					Context.Kind = Effect.Type == EGameXXKCardEffectType::DamageAllPercentAttackPerConsumedArmor
						? EGameXXKCardDamageKind::GroupAttack
						: EGameXXKCardDamageKind::SingleTargetAttack;
					Context.ResolutionOrigin = Origin;
					FGameXXKCardDamageResult DamageResult;
					if (!GameXXKCardRules::ApplyPlayerCardDirectDamage(
						InOutRuntime,
						Context,
						GuardTargetId,
						static_cast<int32>(RawDamage),
						DamageResult,
						&OutError))
					{
						return false;
					}
					InOutResult.DamageResults.Add(DamageResult);
					if (!ResolveFirstDirectDamageReactiveModifiers(
						InOutRuntime,
						Context,
						DamageResult,
						&InOutResult.DamageResults,
						OutError,
						&InOutResult))
					{
						return false;
					}
				}
				continue;
			}
			if (Effect.Type == EGameXXKCardEffectType::Cleanse
				&& Effect.Target == EGameXXKCardEffectTarget::SelectedTarget
				&& CardTargetIds.Num() == 1)
			{
				const FGameXXKCardCombatUnit* CleanseOwner = FindCombatUnitById(InOutRuntime.Units, Instance.OwnerUnitId);
				const FGameXXKCardCombatUnit* CleanseTarget = FindCombatUnitById(InOutRuntime.Units, CardTargetIds[0]);
				if (CleanseOwner && CleanseTarget && CleanseTarget->Side != CleanseOwner->Side)
				{
					continue;
				}
			}
			TArray<FName> EffectTargetIds;
			if (Effect.Target == EGameXXKCardEffectTarget::HighestArmorAlly
				&& !LockedHighestArmorAllyId.IsNone())
			{
				const FGameXXKCardCombatUnit* LockedAlly = FindCombatUnitById(InOutRuntime.Units, LockedHighestArmorAllyId);
				if (LockedAlly && LockedAlly->bLiving)
				{
					EffectTargetIds.Add(LockedHighestArmorAllyId);
				}
			}
			else if (!ResolveEffectTargetIds(InOutRuntime, Instance.OwnerUnitId, CardTargetIds, Effect.Target, EffectTargetIds, OutError))
			{
				return false;
			}
			FGameXXKCardCombatUnit* Owner = FindCombatUnitById(InOutRuntime.Units, Instance.OwnerUnitId);
			if (!Owner || !Owner->bLiving)
			{
				return true;
			}
			FGameXXKCardCombatUnit* ConditionTarget = nullptr;
			if (CardTargetIds.Num() == 1)
			{
				ConditionTarget = FindCombatUnitById(InOutRuntime.Units, CardTargetIds[0]);
			}
			TMap<FName, int32> LightningStatusSnapshots;
			if (Effect.Type == EGameXXKCardEffectType::LightningPerTargetStatusSnapshot)
			{
				for (const FName EffectTargetId : EffectTargetIds)
				{
					const FGameXXKCardCombatUnit* SnapshotTarget = FindCombatUnitById(InOutRuntime.Units, EffectTargetId);
					if (SnapshotTarget && SnapshotTarget->bLiving)
					{
						LightningStatusSnapshots.Add(
							EffectTargetId,
							GameXXKCardRules::GetCombatStatusStacks(*SnapshotTarget, Effect.Status));
					}
				}
			}

			if (Effect.Type == EGameXXKCardEffectType::DrawCards
				|| Effect.Type == EGameXXKCardEffectType::Insight
				|| Effect.Type == EGameXXKCardEffectType::DiscoverCards
				|| Effect.Type == EGameXXKCardEffectType::ReorderCards
				|| Effect.Type == EGameXXKCardEffectType::DiscardCards
				|| Effect.Type == EGameXXKCardEffectType::GainEnergy
				|| Effect.Type == EGameXXKCardEffectType::RevealEnemyIntent
				|| Effect.Type == EGameXXKCardEffectType::SearchUnfinishedHeroTaskCard
				|| Effect.Type == EGameXXKCardEffectType::SearchUnfinishedTaskNpcCard
				|| Effect.Type == EGameXXKCardEffectType::PreserveNextReactionUse
				|| Effect.Type == EGameXXKCardEffectType::DoubleTerrainBonus)
			{
				bool bConditionSatisfied = false;
				int32 Consumed = 0;
				if (!TryApplyEffectConditionAndConsumption(Effect.Condition, InOutRuntime, *Owner, ConditionTarget, &ConditionSnapshot, bConditionSatisfied, Consumed, OutError))
				{
					return false;
				}
				if (!bConditionSatisfied)
				{
					continue;
				}
				if (!Effect.ConsumptionGroupId.IsNone())
				{
					ConsumptionResults.FindOrAdd(Effect.ConsumptionGroupId) += Consumed;
				}
				if (Effect.Type == EGameXXKCardEffectType::DrawCards)
				{
					int32 DeclaredDiscardCount = 0;
					for (int32 LaterEffectIndex = EffectIndex + 1; LaterEffectIndex < Definition.Effects.Num(); ++LaterEffectIndex)
					{
						if (Definition.Effects[LaterEffectIndex].Type == EGameXXKCardEffectType::DiscardCards)
						{
							DeclaredDiscardCount = Definition.Effects[LaterEffectIndex].Magnitude;
							break;
						}
					}
					// A dead party character's cards never re-enter the hand through a draw effect.
					GameXXKCardRules::RemoveDefeatedPartyOwnerCards(InOutRuntime.Deck, InOutRuntime.Units);
					if (!GameXXKCardRules::DrawCards(InOutRuntime.Deck, Effect.Magnitude, 0, &OutError))
					{
						return false;
					}
					if (DeclaredDiscardCount > 0)
					{
						if (DeferredForcedDiscardCount > 0 && DeferredForcedDiscardCount != DeclaredDiscardCount)
						{
							OutError = TEXT("A card cannot defer conflicting forced-discard counts.");
							return false;
						}
						DeferredForcedDiscardCount = DeclaredDiscardCount;
					}
				}
				else if (Effect.Type == EGameXXKCardEffectType::Insight)
				{
					if (!GameXXKCardRules::BeginInsight(InOutRuntime.Deck, Effect.Magnitude, &OutError))
					{
						return false;
					}
					InOutResult.bOpenedPendingChoice = true;
				}
				else if (Effect.Type == EGameXXKCardEffectType::DiscoverCards)
				{
					if (Effect.Magnitude != 1 || InOutRuntime.Deck.PendingChoice.Kind != EGameXXKCardPendingChoiceKind::InsightChooseToHand)
					{
						OutError = TEXT("Discover effect requires the card's active one-card insight choice.");
						return false;
					}
					InOutResult.bOpenedPendingChoice = true;
				}
				else if (Effect.Type == EGameXXKCardEffectType::ReorderCards)
				{
					if (InOutRuntime.Deck.PendingChoice.Kind != EGameXXKCardPendingChoiceKind::InsightChooseToHand)
					{
						OutError = TEXT("Reorder effect requires an insight choice opened by the same card.");
						return false;
					}
				}
				else if (Effect.Type == EGameXXKCardEffectType::DiscardCards)
				{
					if (DeferredForcedDiscardCount > 0)
					{
						if (DeferredForcedDiscardCount != Effect.Magnitude)
						{
							OutError = TEXT("Draw-then-discard choice does not match the card's declared discard count.");
							return false;
						}
					}
				}
				else if (Effect.Type == EGameXXKCardEffectType::GainEnergy)
				{
					const int32 ReferencedConsumption = Effect.ConsumedStackResultRef.IsNone()
						? 0
						: ConsumptionResults.FindRef(Effect.ConsumedStackResultRef);
					if (Effect.SecondaryMagnitude <= 0 || ReferencedConsumption >= Effect.SecondaryMagnitude)
					{
						InOutRuntime.Deck.SharedEnergy = FMath::Min(MaxCardBattleEnergy, InOutRuntime.Deck.SharedEnergy + FMath::Max(0, Effect.Magnitude));
					}
				}
				else if (Effect.Type == EGameXXKCardEffectType::RevealEnemyIntent)
				{
					InOutRuntime.RevealedEnemyIntentCount = FMath::Min(MaxCardBattleEnergy, InOutRuntime.RevealedEnemyIntentCount + FMath::Max(0, Effect.Magnitude));
				}
				else if (Effect.Type == EGameXXKCardEffectType::SearchUnfinishedHeroTaskCard)
				{
					const bool bSorcererPartnerSearch = Definition.Owner == EGameXXKCardOwner::Profession
						&& Definition.OwnerId == FName(TEXT("Profession.Sorcerer"))
						&& Definition.Role == EGameXXKCharacterRole::Sorcerer;
					const bool bOpened = bSorcererPartnerSearch
						? OpenSorcererPartnerTaskSearchChoice(InOutRuntime, Instance.OwnerUnitId, InOutResult, OutError)
						: OpenHeroSpellTaskSearchChoice(InOutRuntime, InOutResult, OutError);
					if (Effect.Magnitude != 1 || !bOpened)
					{
						if (OutError.IsEmpty())
						{
							OutError = TEXT("A Hero spell-task search must request exactly one existing card.");
						}
						return false;
					}
				}
				else if (Effect.Type == EGameXXKCardEffectType::SearchUnfinishedTaskNpcCard)
				{
					if (Effect.Magnitude != 1
						|| !OpenTaskNpcSpellTaskSearchChoice(InOutRuntime, Instance.OwnerUnitId, InOutResult, OutError))
					{
						if (OutError.IsEmpty())
						{
							OutError = TEXT("A task-NPC spell-task search must request exactly one carried unfinished card.");
						}
						return false;
					}
				}
				else if (Effect.Type == EGameXXKCardEffectType::PreserveNextReactionUse)
				{
					if (Effect.Target != EGameXXKCardEffectTarget::AllAllies || Effect.Magnitude != 1)
					{
						OutError = TEXT("Reaction-use preservation requires one team-wide use.");
						return false;
					}
					InOutRuntime.PendingPreservedPartyReactionUses = 1;
				}
				else
				{
					const EGameXXKCardStatus DoubleStatus = Effect.Status == EGameXXKCardStatus::None
						? EGameXXKCardStatus::TerrainBonusDouble
						: Effect.Status;
					if ((DoubleStatus != EGameXXKCardStatus::TerrainBonusDouble
							&& DoubleStatus != EGameXXKCardStatus::TerrainBonusDoubleThisRound)
						|| Effect.Magnitude <= 0)
					{
						OutError = TEXT("Terrain-bonus doubling requires a positive supported status window.");
						return false;
					}
					GameXXKCardRules::AddCombatStatus(*Owner, DoubleStatus, 1);
				}
				continue;
			}

			for (const FName EffectTargetId : EffectTargetIds)
			{
				Owner = FindCombatUnitById(InOutRuntime.Units, Instance.OwnerUnitId);
				FGameXXKCardCombatUnit* Target = FindCombatUnitById(InOutRuntime.Units, EffectTargetId);
				if (!Owner || !Owner->bLiving)
				{
					return true;
				}
				if (!Target || !Target->bLiving)
				{
					continue;
				}
				FGameXXKCardCombatUnit* PerTargetCondition = ConditionTarget ? ConditionTarget : Target;
				bool bConditionSatisfied = false;
				int32 Consumed = 0;
				if (!TryApplyEffectConditionAndConsumption(Effect.Condition, InOutRuntime, *Owner, PerTargetCondition, &ConditionSnapshot, bConditionSatisfied, Consumed, OutError))
				{
					return false;
				}
				if (!bConditionSatisfied)
				{
					continue;
				}
				if (!Effect.ConsumptionGroupId.IsNone())
				{
					ConsumptionResults.FindOrAdd(Effect.ConsumptionGroupId) += Consumed;
				}

				switch (Effect.Type)
				{
				case EGameXXKCardEffectType::DamagePercentAttack:
				{
					const int64 RawDamage = static_cast<int64>(Owner->Attack) * Effect.Magnitude / 100;
					if (RawDamage <= 0 || RawDamage > MAX_int32)
					{
						OutError = TEXT("Attack effect produced an unsupported direct-damage amount.");
						return false;
					}
					for (int32 HitIndex = 0; HitIndex < Effect.HitCount; ++HitIndex)
					{
						FGameXXKCardDamageContext Context;
						Context.SourceUnitId = Owner->UnitId;
						Context.Kind = Effect.Target == EGameXXKCardEffectTarget::AllEnemies
							? EGameXXKCardDamageKind::GroupAttack
							: EGameXXKCardDamageKind::SingleTargetAttack;
						FGameXXKCardDamageResult DamageResult;
						if (!GameXXKCardRules::ApplyPlayerCardDirectDamage(InOutRuntime, Context, Target->UnitId, static_cast<int32>(RawDamage), DamageResult, &OutError))
						{
							return false;
						}
						InOutResult.DamageResults.Add(MoveTemp(DamageResult));
						if (!FindCombatUnitById(InOutRuntime.Units, Target->UnitId)->bLiving)
						{
							break;
						}
					}
					break;
				}
				case EGameXXKCardEffectType::DamagePercentAttackPerTargetStatus:
				{
					if (Effect.Status != EGameXXKCardStatus::Burn
						|| Effect.Magnitude <= 0
						|| Effect.SecondaryMagnitude < 0
						|| Effect.HitCount <= 0)
					{
						OutError = TEXT("Status-scaled Sorcerer damage requires Burn, a positive base percentage, and positive hit count.");
						return false;
					}
					const int32 StatusStacks = GameXXKCardRules::GetCombatStatusStacks(*Target, Effect.Status);
					const int64 Percent = static_cast<int64>(Effect.Magnitude)
						+ static_cast<int64>(Effect.SecondaryMagnitude) * StatusStacks;
					const int64 RawDamage = static_cast<int64>(Owner->Attack) * Percent / 100;
					if (RawDamage <= 0 || RawDamage > MAX_int32)
					{
						OutError = TEXT("Status-scaled Sorcerer damage exceeds the supported range.");
						return false;
					}
					const FName DamageOwnerId = Owner->UnitId;
					const FName DamageTargetId = Target->UnitId;
					for (int32 HitIndex = 0; HitIndex < Effect.HitCount; ++HitIndex)
					{
						Owner = FindCombatUnitById(InOutRuntime.Units, DamageOwnerId);
						Target = FindCombatUnitById(InOutRuntime.Units, DamageTargetId);
						if (!Owner || !Owner->bLiving || !Target || !Target->bLiving)
						{
							break;
						}
						FGameXXKCardDamageContext Context;
						Context.SourceUnitId = DamageOwnerId;
						Context.Kind = Effect.Target == EGameXXKCardEffectTarget::AllEnemies
							? EGameXXKCardDamageKind::GroupAttack
							: EGameXXKCardDamageKind::SingleTargetAttack;
						Context.ResolutionOrigin = Origin;
						FGameXXKCardDamageResult DamageResult;
						if (!GameXXKCardRules::ApplyPlayerCardDirectDamage(
							InOutRuntime,
							Context,
							DamageTargetId,
							static_cast<int32>(RawDamage),
							DamageResult,
							&OutError))
						{
							return false;
						}
						InOutResult.DamageResults.Add(DamageResult);
						if (!ResolveFirstDirectDamageReactiveModifiers(
							InOutRuntime,
							Context,
							DamageResult,
							&InOutResult.DamageResults,
							OutError,
							&InOutResult))
						{
							return false;
						}
					}
					break;
				}
				case EGameXXKCardEffectType::LoseHealth:
				{
					if (Target->UnitId != Owner->UnitId || Effect.Magnitude <= 0)
					{
						OutError = TEXT("Lose-health effect must target its living card owner with a positive amount.");
						return false;
					}
					FGameXXKCardDamageContext Context;
					Context.SourceUnitId = Owner->UnitId;
					Context.Kind = EGameXXKCardDamageKind::SelfHealthLoss;
					Context.ResolutionOrigin = Origin;
					FGameXXKCardDamageResult DamageResult;
					if (!ApplyCombatDirectDamageInternal(
						InOutRuntime.Units,
						InOutRuntime.GuardLinks,
						Context,
						Target->UnitId,
						Effect.Magnitude,
						DamageResult,
						nullptr,
						&InOutRuntime,
						&InOutResult,
						false,
						&OutError))
					{
						return false;
					}
					InOutResult.DamageResults.Add(MoveTemp(DamageResult));
					break;
				}
				case EGameXXKCardEffectType::LoseHealthNonlethal:
				{
					if (Target->Side != Owner->Side || Effect.Magnitude <= 0)
					{
						OutError = TEXT("Nonlethal health loss requires a living ally and a positive amount.");
						return false;
					}
					const int32 ActualLoss = FMath::Min(Effect.Magnitude, FMath::Max(0, Target->HP - 1));
					if (ActualLoss <= 0)
					{
						break;
					}
					const FName SelfUnitId = Target->UnitId;
					FGameXXKCardDamageContext Context;
					Context.SourceUnitId = SelfUnitId;
					Context.Kind = EGameXXKCardDamageKind::SelfHealthLoss;
					Context.ResolutionOrigin = Origin;
					FGameXXKCardDamageResult DamageResult;
					if (!ApplyCombatDirectDamageInternal(
						InOutRuntime.Units,
						InOutRuntime.GuardLinks,
						Context,
						SelfUnitId,
						ActualLoss,
						DamageResult,
						nullptr,
						&InOutRuntime,
						&InOutResult,
						false,
						&OutError))
					{
						return false;
					}
					InOutResult.DamageResults.Add(MoveTemp(DamageResult));
					break;
				}
				case EGameXXKCardEffectType::Heal:
				{
					int64 BaseHealing = FMath::Max(0, Effect.Magnitude);
					if (Effect.MagnitudePolicy == EGameXXKCardMagnitudePolicy::MedicineCoefficient)
					{
						if (!bPreparedMedicineAction)
						{
							MedicineSnapshot = GameXXKCardRules::GetCombatStatusStacks(*Owner, EGameXXKCardStatus::Medicine);
							if (MedicineSnapshot > 0)
							{
								const int32 ConsumedMedicine = GameXXKCardRules::ConsumeCombatStatus(*Owner, EGameXXKCardStatus::Medicine, MedicineSnapshot);
								FGameXXKCardStatusChangeResult& MedicineChange = InOutResult.StatusChanges.AddDefaulted_GetRef();
								MedicineChange.TargetUnitId = Owner->UnitId;
								MedicineChange.Status = EGameXXKCardStatus::Medicine;
								MedicineChange.RemovedStacks = ConsumedMedicine;
							}
							bPreparedMedicineAction = true;
						}
						BaseHealing = FGameXXKCombatScalingRules::ResolveMedicineHealing(
							Effect.Magnitude,
							MedicineSnapshot,
							Instance.CurrentQuality,
							InOutRuntime.TeamMaxLevelSnapshot);
					}
					if (Effect.SecondaryMagnitude > 0)
					{
						int64 DirectHealthDamage = 0;
						for (const FGameXXKCardDamageResult& DamageResult : InOutResult.DamageResults)
						{
							const FGameXXKCardCombatUnit* DamagedUnit = FindCombatUnitById(InOutRuntime.Units, DamageResult.ResolvedTargetUnitId);
							if (DamageResult.SourceUnitId == Owner->UnitId && DamagedUnit && DamagedUnit->Side != Owner->Side)
							{
								DirectHealthDamage = FMath::Min<int64>(MAX_int32, DirectHealthDamage + FMath::Max(0, DamageResult.HealthDamage));
							}
						}
						BaseHealing = FMath::Min<int64>(Effect.SecondaryMagnitude, DirectHealthDamage * BaseHealing / 100);
					}
					if (BaseHealing <= 0)
					{
						break;
					}
					if (!bPreparedHealingAction)
					{
						HealingFlatBonus = GameXXKCardRules::GetCombatStatusStacks(*Owner, EGameXXKCardStatus::NextHealingBonus);
						if (HealingFlatBonus > 0)
						{
							GameXXKCardRules::ConsumeCombatStatus(*Owner, EGameXXKCardStatus::NextHealingBonus, HealingFlatBonus);
						}
						if (Origin == EGameXXKCardResolutionOrigin::ActivePlay)
						{
							TArray<FName> TriggeredHealingModifierIds;
							if (!CollectOnNextHealingBonuses(InOutRuntime, Definition, Instance, *Owner, PerTargetCondition, HealingBonusPercent, TriggeredHealingModifierIds, OutError))
							{
								return false;
							}
							if (!ConsumeOnNextHealingModifiers(InOutRuntime, TriggeredHealingModifierIds, OutError))
							{
								return false;
							}
						}
						bPreparedHealingAction = true;
					}
					const int64 MultiplierPercent = FMath::Max<int64>(0, 100 + static_cast<int64>(HealingBonusPercent));
					const int64 FinalHealing = BaseHealing * MultiplierPercent / 100 + HealingFlatBonus;
					const int32 RequestedHealing = static_cast<int32>(FMath::Clamp<int64>(FinalHealing, 0, MAX_int32));
					if (RequestedHealing > 0)
					{
						ApplyAndRecordHealing(InOutResult, Owner->UnitId, *Target, RequestedHealing);
					}
					break;
				}
				case EGameXXKCardEffectType::HealOrReverseWithMedicine:
				{
					if (Effect.Magnitude <= 0)
					{
						OutError = TEXT("Medicine healing or reversal requires a positive base amount.");
						return false;
					}
					if (!bPreparedMedicineAction)
					{
						MedicineSnapshot = GameXXKCardRules::GetCombatStatusStacks(*Owner, EGameXXKCardStatus::Medicine);
						if (MedicineSnapshot > 0)
						{
							const int32 ConsumedMedicine = GameXXKCardRules::ConsumeCombatStatus(*Owner, EGameXXKCardStatus::Medicine, MedicineSnapshot);
							FGameXXKCardStatusChangeResult& MedicineChange = InOutResult.StatusChanges.AddDefaulted_GetRef();
							MedicineChange.TargetUnitId = Owner->UnitId;
							MedicineChange.Status = EGameXXKCardStatus::Medicine;
							MedicineChange.RemovedStacks = ConsumedMedicine;
						}
						bPreparedMedicineAction = true;
					}
					const int32 ResolvedAmount = FGameXXKCombatScalingRules::ResolveMedicineHealing(
						Effect.Magnitude,
						MedicineSnapshot,
						Instance.CurrentQuality,
						InOutRuntime.TeamMaxLevelSnapshot);
					if (ResolvedAmount <= 0)
					{
						OutError = TEXT("Medicine healing or reversal produced an unsupported amount.");
						return false;
					}
					if (Target->Side == Owner->Side)
					{
						ApplyAndRecordHealing(InOutResult, Owner->UnitId, *Target, ResolvedAmount);
					}
					else
					{
						const FName MedicineOwnerUnitId = Owner->UnitId;
						const FName ReverseTargetUnitId = Target->UnitId;
						FGameXXKCardDamageContext Context;
						Context.Kind = EGameXXKCardDamageKind::EnvironmentalHealthLoss;
						Context.ResolutionOrigin = Origin;
						FGameXXKCardDamageResult DamageResult;
						if (!GameXXKCardRules::ApplyCombatDirectDamage(
							InOutRuntime.Units,
							InOutRuntime.GuardLinks,
							Context,
							ReverseTargetUnitId,
							ResolvedAmount,
							DamageResult,
							&OutError))
						{
							return false;
						}
						DamageResult.SourceUnitId = MedicineOwnerUnitId;
						DamageResult.Cause = EGameXXKCardDamageCause::Medicine;
						InOutResult.DamageResults.Add(MoveTemp(DamageResult));
					}
					break;
				}
				case EGameXXKCardEffectType::HealOrReverseFlat:
				{
					if (Effect.Magnitude <= 0)
					{
						OutError = TEXT("Flat healing or reversal requires a positive amount.");
						return false;
					}
					const int32 ResolvedAmount = Effect.MagnitudePolicy == EGameXXKCardMagnitudePolicy::MedicineCoefficient
						? FGameXXKCombatScalingRules::ResolveMedicineHealing(Effect.Magnitude, 0, Instance.CurrentQuality,
							InOutRuntime.TeamMaxLevelSnapshot)
						: Effect.Magnitude;
					if (Target->Side == Owner->Side)
					{
						ApplyAndRecordHealing(InOutResult, Owner->UnitId, *Target, ResolvedAmount);
					}
					else
					{
						const FName EffectOwnerUnitId = Owner->UnitId;
						const FName ReverseTargetUnitId = Target->UnitId;
						FGameXXKCardDamageContext Context;
						Context.Kind = EGameXXKCardDamageKind::EnvironmentalHealthLoss;
						Context.ResolutionOrigin = Origin;
						FGameXXKCardDamageResult DamageResult;
						if (!GameXXKCardRules::ApplyCombatDirectDamage(
							InOutRuntime.Units,
							InOutRuntime.GuardLinks,
							Context,
							ReverseTargetUnitId,
							ResolvedAmount,
							DamageResult,
							&OutError))
						{
							return false;
						}
						DamageResult.SourceUnitId = EffectOwnerUnitId;
						DamageResult.Cause = EGameXXKCardDamageCause::Medicine;
						InOutResult.DamageResults.Add(MoveTemp(DamageResult));
					}
					break;
				}
				case EGameXXKCardEffectType::AddArmor:
				{
					int32 ArmorAmount = Effect.Magnitude;
					if (Effect.MagnitudePolicy == EGameXXKCardMagnitudePolicy::PrintedCostArmor)
					{
						ArmorAmount = GameXXKCardRules::ResolvePrintedCostArmor(
							*Owner,
							Definition.EnergyCost,
							Instance.CurrentQuality);
					}
					else if (Effect.MagnitudePolicy == EGameXXKCardMagnitudePolicy::DefensePercent)
					{
						ArmorAmount = ResolveDefensePercentArmorAmount(
							*Owner,
							Effect.Magnitude,
							Instance.CurrentQuality);
					}
					if (ArmorAmount > 0)
					{
						ApplyAndRecordArmor(InOutResult, Owner->UnitId, *Target, ArmorAmount);
					}
					break;
				}
				case EGameXXKCardEffectType::RetainArmorNextRound:
					if ((Effect.Target != EGameXXKCardEffectTarget::AllAllies
							&& Effect.Target != EGameXXKCardEffectTarget::CardOwner)
						|| Effect.Magnitude != 1
						|| Target->Side != Owner->Side)
					{
						OutError = TEXT("Next-round armor retention requires one living ally or the allied team.");
						return false;
					}
					InOutRuntime.RetainArmorAtNextPartyPhaseUnitIds.AddUnique(Target->UnitId);
					break;
				case EGameXXKCardEffectType::GainArmorFromCurrentManaPercent:
				{
					if (Effect.Magnitude <= 0)
					{
						OutError = TEXT("Armor from current Mana requires a positive percentage.");
						return false;
					}
					const int64 ArmorGain = static_cast<int64>(Target->Mana) * Effect.Magnitude / 100;
					if (ArmorGain > MAX_int32)
					{
						OutError = TEXT("Armor from current Mana exceeds the supported range.");
						return false;
					}
					if (ArmorGain > 0)
					{
						ApplyAndRecordArmor(InOutResult, Owner->UnitId, *Target, static_cast<int32>(ArmorGain));
					}
					break;
				}
				case EGameXXKCardEffectType::GainMana:
					Target->Mana = static_cast<int32>(FMath::Min<int64>(Target->MaxMana, static_cast<int64>(Target->Mana) + FMath::Max(0, Effect.Magnitude)));
					break;
				case EGameXXKCardEffectType::IncreaseMaxMana:
					if (Target->UnitId != Owner->UnitId
						|| Effect.Magnitude <= 0
						|| Target->MaxMana > MAX_int32 - Effect.Magnitude)
					{
						OutError = TEXT("Maximum-Mana growth requires the card owner and a positive supported amount.");
						return false;
					}
					Target->MaxMana += Effect.Magnitude;
					break;
				case EGameXXKCardEffectType::GainManaOverflowToArmor:
				{
					if (Effect.Magnitude <= 0 || Effect.SecondaryMagnitude <= 0)
					{
						OutError = TEXT("Mana overflow conversion requires a positive percentage and Mana grant.");
						return false;
					}
					const int64 RawMana = static_cast<int64>(Target->Mana) + Effect.SecondaryMagnitude;
					const int64 Overflow = FMath::Max<int64>(0, RawMana - Target->MaxMana);
					const int32 ArmorGain = FGameXXKCombatScalingRules::ResolveManaOverflowArmor(
						static_cast<int32>(Overflow), Effect.Magnitude,
						Instance.CurrentQuality, InOutRuntime.TeamMaxLevelSnapshot);
					Target->Mana = static_cast<int32>(FMath::Min<int64>(Target->MaxMana, RawMana));
					if (ArmorGain > 0)
					{
						ApplyAndRecordArmor(InOutResult, Owner->UnitId, *Target, static_cast<int32>(ArmorGain));
					}
					break;
				}
				case EGameXXKCardEffectType::GainManaPerConsumedStatus:
				{
					const int64 ManaGain = static_cast<int64>(FMath::Max(0, Effect.Magnitude)) * Consumed;
					Target->Mana = static_cast<int32>(FMath::Min<int64>(Target->MaxMana, static_cast<int64>(Target->Mana) + ManaGain));
					break;
				}
				case EGameXXKCardEffectType::GainMedicineFromPartyHealthLoss:
				{
					if (Target->UnitId != Owner->UnitId || Effect.Magnitude <= 0)
					{
						OutError = TEXT("Medicine from party health loss requires the card owner and a positive per-ally amount.");
						return false;
					}
					TSet<FName> AffectedAllies;
					for (int32 ResultIndex = InitialDamageResultCount; ResultIndex < InOutResult.DamageResults.Num(); ++ResultIndex)
					{
						const FGameXXKCardDamageResult& DamageResult = InOutResult.DamageResults[ResultIndex];
						const FGameXXKCardCombatUnit* DamagedUnit = FindCombatUnitById(
							InOutRuntime.Units,
							DamageResult.ResolvedTargetUnitId);
						if (DamageResult.Cause == EGameXXKCardDamageCause::SelfLoss
							&& DamageResult.HealthDamage > 0
							&& DamagedUnit
							&& DamagedUnit->Side == Owner->Side)
						{
							AffectedAllies.Add(DamageResult.ResolvedTargetUnitId);
						}
					}
					const int64 RawMedicine = static_cast<int64>(AffectedAllies.Num()) * Effect.Magnitude;
					if (RawMedicine > MAX_int32)
					{
						OutError = TEXT("Party health loss produced an unsupported Medicine award.");
						return false;
					}
					if (RawMedicine > 0
						&& !GrantStatusFromCardEffect(
							InOutRuntime,
							*Target,
							EGameXXKCardStatus::Medicine,
							static_cast<int32>(RawMedicine),
							OutError,
							&InOutResult,
							Owner->UnitId))
					{
						return false;
					}
					break;
				}
				case EGameXXKCardEffectType::ApplyStatus:
				{
					const int32 StatusBefore = GameXXKCardRules::GetCombatStatusStacks(*Target, Effect.Status);
					const int32 AppliedMagnitude = ResolveCardStatusApplicationAmount(
						InOutRuntime,
						*Target,
						Effect,
						Instance.CurrentQuality);
					if (!GrantStatusFromCardEffect(
						InOutRuntime,
						*Target,
						Effect.Status,
						AppliedMagnitude,
						OutError,
						&InOutResult,
						Owner->UnitId))
					{
						return false;
					}
					const int32 AppliedStacks = GameXXKCardRules::GetCombatStatusStacks(*Target, Effect.Status) - StatusBefore;
					if (AppliedStacks > 0)
					{
						FGameXXKCardStatusChangeResult& StatusChange = InOutResult.StatusChanges.AddDefaulted_GetRef();
						StatusChange.TargetUnitId = Target->UnitId;
						StatusChange.Status = Effect.Status;
						StatusChange.AppliedStacks = AppliedStacks;
					}
					break;
				}
				case EGameXXKCardEffectType::RemoveStatus:
				{
					const int32 RemovedStacks = GameXXKCardRules::ConsumeCombatStatus(*Target, Effect.Status, Effect.Magnitude);
					if (RemovedStacks > 0)
					{
						FGameXXKCardStatusChangeResult& StatusChange = InOutResult.StatusChanges.AddDefaulted_GetRef();
						StatusChange.TargetUnitId = Target->UnitId;
						StatusChange.Status = Effect.Status;
						StatusChange.RemovedStacks = RemovedStacks;
					}
					break;
				}
				case EGameXXKCardEffectType::RemoveAnyDamageOverTime:
					GameXXKCardRules::ClearAllDotReservoirs(*Target);
					break;
				case EGameXXKCardEffectType::CleanseFriendlyDamageOverTime:
					if (Effect.Magnitude <= 0)
					{
						OutError = TEXT("A friendly damage-over-time cleanse requires a positive operation count.");
						return false;
					}
					if (Target->Side == Owner->Side)
					{
						GameXXKCardRules::ClearAllDotReservoirs(*Target);
					}
					break;
				case EGameXXKCardEffectType::Cleanse:
					if (Target->Side == Owner->Side)
					{
						GameXXKCardRules::ConsumeCombatStatus(*Target, EGameXXKCardStatus::Bleed, MAX_int32);
						GameXXKCardRules::ConsumeCombatStatus(*Target, EGameXXKCardStatus::Poison, MAX_int32);
						GameXXKCardRules::ConsumeCombatStatus(*Target, EGameXXKCardStatus::Burn, MAX_int32);
					}
					break;
				case EGameXXKCardEffectType::TriggerHighestDamageOverTime:
				{
					EGameXXKCardStatus TriggeredStatus = EGameXXKCardStatus::None;
					EGameXXKCardDamageCause TriggeredCause = EGameXXKCardDamageCause::Invalid;
					int32 TriggeredStacks = 0;
					for (const TPair<EGameXXKCardStatus, EGameXXKCardDamageCause>& Candidate : {
						TPair<EGameXXKCardStatus, EGameXXKCardDamageCause>(EGameXXKCardStatus::Bleed, EGameXXKCardDamageCause::Bleed),
						TPair<EGameXXKCardStatus, EGameXXKCardDamageCause>(EGameXXKCardStatus::Poison, EGameXXKCardDamageCause::Poison),
						TPair<EGameXXKCardStatus, EGameXXKCardDamageCause>(EGameXXKCardStatus::Burn, EGameXXKCardDamageCause::Burn)})
					{
						const int32 CandidateStacks = GameXXKCardRules::GetCombatStatusStacks(*Target, Candidate.Key);
						if (CandidateStacks > TriggeredStacks)
						{
							TriggeredStatus = Candidate.Key;
							TriggeredCause = Candidate.Value;
							TriggeredStacks = CandidateStacks;
						}
					}
					if (TriggeredStacks <= 0)
					{
						break;
					}
					const FName TriggerSourceUnitId = Owner->UnitId;
					const FName TriggerTargetUnitId = Target->UnitId;
					FGameXXKCardDamageResult TriggerResult;
					if (!ApplyStatusHealthLoss(
						InOutRuntime,
						TriggerTargetUnitId,
						TriggeredCause,
						TriggeredStacks,
						TriggerResult,
						OutError,
						&InOutResult))
					{
						return false;
					}
					Target = FindCombatUnitById(InOutRuntime.Units, TriggerTargetUnitId);
					if (!Target)
					{
						OutError = TEXT("Triggered status target disappeared before its stack could decay.");
						return false;
					}
					TriggerResult.SourceUnitId = TriggerSourceUnitId;
					if (!ResolveTriggeredStatusLayerConsumption(
						InOutRuntime,
						*Target,
						TriggeredStatus,
						0,
						TriggerSourceUnitId,
						TriggerResult.HealthDamage,
						TriggerResult.StatusStacksConsumed,
						OutError,
						&InOutResult))
					{
						return false;
					}
					InOutResult.DamageResults.Add(MoveTemp(TriggerResult));
					break;
				}
				case EGameXXKCardEffectType::TriggerStatus:
				{
					EGameXXKCardDamageCause Cause = EGameXXKCardDamageCause::Invalid;
					switch (Effect.Status)
					{
					case EGameXXKCardStatus::Bleed:
						Cause = EGameXXKCardDamageCause::Bleed;
						break;
					case EGameXXKCardStatus::Poison:
						Cause = EGameXXKCardDamageCause::Poison;
						break;
					case EGameXXKCardStatus::Burn:
						Cause = EGameXXKCardDamageCause::Burn;
						break;
					case EGameXXKCardStatus::DamageOverTime:
						Cause = EGameXXKCardDamageCause::Rot;
						break;
					default:
						OutError = TEXT("Triggered status damage supports Bleed, Poison, Burn, or Rot only.");
						return false;
					}
					if (Effect.Magnitude <= 0)
					{
						OutError = TEXT("Triggered status damage requires a positive trigger count.");
						return false;
					}
					const FName TriggerOwnerId = Owner->UnitId;
					const FName TriggerTargetId = Target->UnitId;
					for (int32 TriggerIndex = 0; TriggerIndex < Effect.Magnitude; ++TriggerIndex)
					{
						Target = FindCombatUnitById(InOutRuntime.Units, TriggerTargetId);
						const int32 StacksBefore = Target
							? GameXXKCardRules::GetCombatStatusStacks(*Target, Effect.Status)
							: 0;
						if (!Target || !Target->bLiving || StacksBefore <= 0)
						{
							break;
						}
						FGameXXKCardDamageResult TriggerResult;
						if (!ApplyStatusHealthLoss(
							InOutRuntime,
							TriggerTargetId,
							Cause,
							StacksBefore,
							TriggerResult,
							OutError,
							&InOutResult))
						{
							return false;
						}
						Target = FindCombatUnitById(InOutRuntime.Units, TriggerTargetId);
						if (!Target)
						{
							OutError = TEXT("Triggered status target disappeared before its layer update.");
							return false;
						}
						TriggerResult.SourceUnitId = TriggerOwnerId;
						TriggerResult.ResolutionOrigin = Origin;
						if (!ResolveTriggeredStatusLayerConsumption(
							InOutRuntime,
							*Target,
							Effect.Status,
							0,
							TriggerOwnerId,
							TriggerResult.HealthDamage,
							TriggerResult.StatusStacksConsumed,
							OutError,
							&InOutResult))
						{
							return false;
						}
						InOutResult.DamageResults.Add(MoveTemp(TriggerResult));
					}
					break;
				}
				case EGameXXKCardEffectType::LightningPerTargetStatusSnapshot:
				{
					if (Effect.Status != EGameXXKCardStatus::Mark || Effect.Magnitude <= 0)
					{
						OutError = TEXT("Lightning requires a positive percentage and a Mark snapshot.");
						return false;
					}
					const int32 LockedStrikes = LightningStatusSnapshots.FindRef(EffectTargetId);
					const FName LightningOwnerId = Owner->UnitId;
					const FName LightningTargetId = Target->UnitId;
					for (int32 StrikeIndex = 0; StrikeIndex < LockedStrikes; ++StrikeIndex)
					{
						Owner = FindCombatUnitById(InOutRuntime.Units, LightningOwnerId);
						Target = FindCombatUnitById(InOutRuntime.Units, LightningTargetId);
						if (!Owner || !Owner->bLiving || !Target || !Target->bLiving)
						{
							break;
						}
						const int64 RawDamage = static_cast<int64>(Owner->Attack) * Effect.Magnitude / 100;
						if (RawDamage <= 0 || RawDamage > MAX_int32)
						{
							OutError = TEXT("Lightning produced an unsupported damage amount.");
							return false;
						}
						FGameXXKCardDamageContext Context;
						Context.SourceUnitId = LightningOwnerId;
						Context.Kind = Effect.Target == EGameXXKCardEffectTarget::AllEnemies
							? EGameXXKCardDamageKind::GroupAttack
							: EGameXXKCardDamageKind::SingleTargetAttack;
						Context.ResolutionOrigin = Origin;
						FGameXXKCardDamageResult DamageResult;
						if (!GameXXKCardRules::ApplyPlayerCardDirectDamage(
							InOutRuntime,
							Context,
							LightningTargetId,
							static_cast<int32>(RawDamage),
							DamageResult,
							&OutError))
						{
							return false;
						}
						InOutResult.DamageResults.Add(DamageResult);
						if (!ResolveFirstDirectDamageReactiveModifiers(
							InOutRuntime,
							Context,
							DamageResult,
							&InOutResult.DamageResults,
							OutError,
							&InOutResult))
						{
							return false;
						}
					}
					break;
				}
				case EGameXXKCardEffectType::ResolveToxicExplosion:
				{
					const FName ExplosionSourceUnitId = Owner->UnitId;
					const FName ExplosionTargetUnitId = Target->UnitId;
					TArray<FGameXXKCardDamageResult> ExplosionResults;
					if (!GameXXKCardRules::ResolveToxicExplosion(
						InOutRuntime,
						ExplosionSourceUnitId,
						ExplosionTargetUnitId,
						false,
						ExplosionResults,
						&OutError))
					{
						return false;
					}
					TSet<EGameXXKCardDamageCause> DistinctDotCauses;
					for (const FGameXXKCardDamageResult& ExplosionResult : ExplosionResults)
					{
						DistinctDotCauses.Add(ExplosionResult.Cause);
					}
					InOutResult.ToxicExplosionDistinctDotTypeCounts.Add(DistinctDotCauses.Num());
					for (FGameXXKCardDamageResult& ExplosionResult : ExplosionResults)
					{
						ExplosionResult.ResolutionOrigin = Origin;
						InOutResult.DamageResults.Add(MoveTemp(ExplosionResult));
					}
					break;
				}
				case EGameXXKCardEffectType::RegisterReaction:
					if ((Effect.Status != EGameXXKCardStatus::Counter && Effect.Status != EGameXXKCardStatus::Block)
						|| Effect.Magnitude <= 0)
					{
						OutError = TEXT("A registered reaction requires positive Counter or Block uses.");
						return false;
					}
					if (!RegisterPartyReactionUses(
						InOutRuntime,
						Instance,
						Target->UnitId,
						Effect.Status,
						Effect.Magnitude,
						OutError))
					{
						return false;
					}
					break;
				case EGameXXKCardEffectType::EachLivingAllyAttackSelectedTarget:
				{
					if (Effect.Magnitude <= 0 || CardTargetIds.Num() != 1 || EffectTargetId != CardTargetIds[0])
					{
						OutError = TEXT("Joint attack requires one selected living target and a positive attack percentage.");
						return false;
					}
					struct FJointAttackSource
					{
						FName UnitId = NAME_None;
						int32 Attack = 0;
						int32 StableSortOrder = INDEX_NONE;
					};
					TArray<FJointAttackSource> Sources;
					const EGameXXKCardTargetSide TeamSide = Owner->Side;
					for (const FGameXXKCardCombatUnit& Candidate : InOutRuntime.Units)
					{
						if (Candidate.bLiving && Candidate.Side == TeamSide)
						{
							FJointAttackSource& Source = Sources.AddDefaulted_GetRef();
							Source.UnitId = Candidate.UnitId;
							Source.Attack = Candidate.Attack;
							Source.StableSortOrder = Candidate.StableSortOrder;
						}
					}
					Sources.Sort([](const FJointAttackSource& Left, const FJointAttackSource& Right)
					{
						return Left.StableSortOrder != Right.StableSortOrder
							? Left.StableSortOrder < Right.StableSortOrder
							: Left.UnitId.LexicalLess(Right.UnitId);
					});
					for (const FJointAttackSource& Source : Sources)
					{
						const FGameXXKCardCombatUnit* CurrentTarget = FindCombatUnitById(InOutRuntime.Units, EffectTargetId);
						if (!CurrentTarget || !CurrentTarget->bLiving)
						{
							break;
						}
						const int64 RawDamage = static_cast<int64>(Source.Attack) * Effect.Magnitude / 100;
						if (RawDamage <= 0 || RawDamage > MAX_int32)
						{
							OutError = TEXT("Joint attack produced an unsupported direct-damage amount.");
							return false;
						}
						FGameXXKCardDamageContext Context;
						Context.SourceUnitId = Source.UnitId;
						Context.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
						Context.ResolutionOrigin = Origin;
						FGameXXKCardDamageResult DamageResult;
						if (!GameXXKCardRules::ApplyPlayerCardDirectDamage(InOutRuntime, Context, EffectTargetId, static_cast<int32>(RawDamage), DamageResult, &OutError))
						{
							return false;
						}
						InOutResult.DamageResults.Add(DamageResult);
						if (!ResolveFirstDirectDamageReactiveModifiers(InOutRuntime, Context, DamageResult, &InOutResult.DamageResults, OutError, &InOutResult))
						{
							return false;
						}
					}
					break;
				}
				case EGameXXKCardEffectType::ApplyGuardLink:
				{
					if (Effect.GuardLink.Stacks <= 0 || Effect.GuardLink.RedirectPolicy == EGameXXKCardGuardRedirectPolicy::Invalid)
					{
						OutError = TEXT("Guard-link effect has invalid stack or redirect data.");
						return false;
					}
					TArray<FName> GuardianUnitIds;
					if (!ResolveEffectTargetIds(InOutRuntime, Instance.OwnerUnitId, CardTargetIds, Effect.GuardLink.Guardian, GuardianUnitIds, OutError))
					{
						return false;
					}
					for (const FName GuardianUnitId : GuardianUnitIds)
					{
						if (GuardianUnitId == EffectTargetId)
						{
							continue;
						}
						FGameXXKCardGuardLinkRuntime* ExistingLink = InOutRuntime.GuardLinks.FindByPredicate([GuardianUnitId, EffectTargetId, &Effect](const FGameXXKCardGuardLinkRuntime& Link)
						{
							return Link.GuardianUnitId == GuardianUnitId
								&& Link.ProtectedUnitId == EffectTargetId
								&& Link.RedirectPolicy == Effect.GuardLink.RedirectPolicy;
						});
						if (ExistingLink)
						{
							if (ExistingLink->Stacks > MAX_int32 - Effect.GuardLink.Stacks)
							{
								OutError = TEXT("Guard-link stacks exceed supported range.");
								return false;
							}
							ExistingLink->Stacks += Effect.GuardLink.Stacks;
						}
						else
						{
							FGameXXKCardGuardLinkRuntime& NewLink = InOutRuntime.GuardLinks.AddDefaulted_GetRef();
							NewLink.GuardianUnitId = GuardianUnitId;
							NewLink.ProtectedUnitId = EffectTargetId;
							NewLink.Stacks = Effect.GuardLink.Stacks;
							NewLink.RedirectPolicy = Effect.GuardLink.RedirectPolicy;
						}
					}
					break;
				}
				case EGameXXKCardEffectType::RedirectSingleTargetEnemyAttacks:
					GameXXKCardRules::AddCombatStatus(*Target, EGameXXKCardStatus::RedirectSingleTargetEnemyAttack, Effect.Magnitude);
					break;
				default:
					OutError = TEXT("Card effect reached an unsupported resolution branch.");
					return false;
				}
			}
		}
		if (DeferredForcedDiscardCount > 0)
		{
			GameXXKCardRules::RemoveDefeatedPartyOwnerCards(InOutRuntime.Deck, InOutRuntime.Units);
			if (!GameXXKCardRules::DrawCards(InOutRuntime.Deck, 0, DeferredForcedDiscardCount, &OutError))
			{
				return false;
			}
			InOutResult.bOpenedPendingChoice = true;
		}
		return true;
	}

	bool ResolveCardEffectsFromSnapshot(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FGameXXKResolvedCardSnapshot& Snapshot,
		const EGameXXKCardResolutionOrigin Origin,
		FGameXXKCardPlayResult& InOutResult,
		FString& OutError,
		const int32 PrimaryAttackBonusPercent = 0,
		const FGameXXKCardDefinition* EffectiveDefinitionOverride = nullptr)
	{
		if (Origin == EGameXXKCardResolutionOrigin::Invalid)
		{
			OutError = TEXT("Card effect resolution requires an explicit origin.");
			return false;
		}
		if (PrimaryAttackBonusPercent < 0)
		{
			OutError = TEXT("A primary-attack bonus cannot be negative.");
			return false;
		}
		const FGameXXKCardDefinition* BaseDefinition = FGameXXKCardCatalog::FindCardDefinition(Snapshot.CardId);
		if (!BaseDefinition || !IsConcreteCardQuality(Snapshot.Quality) || Snapshot.OwnerUnitId.IsNone())
		{
			OutError = TEXT("Card effect snapshot has no catalog definition, concrete quality, or owner.");
			return false;
		}
		const FGameXXKCardCombatUnit* Owner = FindCombatUnitById(InOutRuntime.Units, Snapshot.OwnerUnitId);
		if (!Owner)
		{
			OutError = TEXT("Card effect snapshot owner is absent from the battle record.");
			return false;
		}

		InOutResult.CardId = Snapshot.CardId;
		InOutResult.OwnerUnitId = Snapshot.OwnerUnitId;
		InOutResult.ResolutionOrigin = Origin;
		if (!Owner->bLiving)
		{
			InOutResult.TargetUnitIds.Reset();
			return true;
		}

		FGameXXKCardDefinition QualityEffectiveDefinition = EffectiveDefinitionOverride
			? *EffectiveDefinitionOverride
			: FGameXXKCardQualityRules::BuildEffectiveDefinition(*BaseDefinition, Snapshot.Quality);
		if (QualityEffectiveDefinition.Id != Snapshot.CardId)
		{
			OutError = TEXT("An active-card definition override does not match its stable snapshot CardId.");
			return false;
		}
		if (!ApplySorcererSequenceDefinition(
			InOutRuntime,
			Snapshot,
			QualityEffectiveDefinition,
			OutError))
		{
			return false;
		}
		ApplySorcererIceBranchManaOverflow(Snapshot, QualityEffectiveDefinition);
		if (!AddSorcererSearchFallbackWhenUnavailable(
			InOutRuntime,
			Snapshot.OwnerUnitId,
			QualityEffectiveDefinition,
			OutError)
			|| !ValidateCurrentEffectPlan(QualityEffectiveDefinition, OutError))
		{
			return false;
		}
		FGameXXKCardInstance SyntheticInstance;
		SyntheticInstance.InstanceId = InOutResult.CardInstanceId.IsNone()
			? FName(*FString::Printf(TEXT("Automatic.%s.%s"), *Snapshot.OwnerUnitId.ToString(), *Snapshot.CardId.ToString()))
			: InOutResult.CardInstanceId;
		SyntheticInstance.CardId = Snapshot.CardId;
		SyntheticInstance.CurrentQuality = Snapshot.Quality;
		SyntheticInstance.OwnerUnitId = Snapshot.OwnerUnitId;
		SyntheticInstance.SourceEntryId = SyntheticInstance.InstanceId;
		SyntheticInstance.AcquisitionOrdinal = 0;

		TArray<FName> TargetIds;
		if (Origin == EGameXXKCardResolutionOrigin::ActivePlay)
		{
			TargetIds = Snapshot.OriginalTargetUnitIds;
		}
		else if (!ResolveSnapshotTargetIds(
			InOutRuntime,
			QualityEffectiveDefinition,
			Snapshot,
			SyntheticInstance,
			TargetIds,
			OutError))
		{
			return false;
		}
		InOutResult.TargetUnitIds = TargetIds;

		FGameXXKCardDefinition ResolutionDefinition = QualityEffectiveDefinition;
		if (Origin == EGameXXKCardResolutionOrigin::ActivePlay
			&& !BuildTerrainAmplifiedDefinition(
				InOutRuntime,
				QualityEffectiveDefinition,
				SyntheticInstance,
				ResolutionDefinition,
				OutError))
		{
			return false;
		}
		if (PrimaryAttackBonusPercent > 0)
		{
			FGameXXKCardEffect* PrimaryAttack = ResolutionDefinition.Effects.FindByPredicate([](const FGameXXKCardEffect& Effect)
			{
				return Effect.Type == EGameXXKCardEffectType::DamagePercentAttack;
			});
			if (!PrimaryAttack
				|| PrimaryAttack->Magnitude <= 0
				|| PrimaryAttackBonusPercent > MAX_int32 - PrimaryAttack->Magnitude)
			{
				OutError = TEXT("A Heavy Arrow primary bonus requires one supported positive attack packet.");
				return false;
			}
			PrimaryAttack->Magnitude += PrimaryAttackBonusPercent;
		}
		const bool bSkipMissingSelectedTargetEffects = Origin != EGameXXKCardResolutionOrigin::ActivePlay
			&& TargetIds.IsEmpty();
		return ResolveDefinitionEffects(
			InOutRuntime,
			ResolutionDefinition,
			SyntheticInstance,
			TargetIds,
			Origin,
			bSkipMissingSelectedTargetEffects,
			InOutResult,
			OutError);
	}

	bool LockHeavyArrowCharge(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FName OwnerUnitId,
		const EGameXXKHeavyArrowChargeSource ChargeSource,
		FGameXXKCardPlayResult& InOutResult,
		int32& OutLockedCharge,
		FName& OutChargeOwnerUnitId,
		FString& OutError)
	{
		OutLockedCharge = 0;
		OutChargeOwnerUnitId = NAME_None;
		switch (ChargeSource)
		{
		case EGameXXKHeavyArrowChargeSource::CardOwner:
			OutChargeOwnerUnitId = OwnerUnitId;
			break;
		case EGameXXKHeavyArrowChargeSource::HighestAttackAlly:
		{
			TArray<FName> ChargeOwnerIds;
			if (!ResolveEffectTargetIds(
				InOutRuntime,
				OwnerUnitId,
				{},
				EGameXXKCardEffectTarget::HighestAttackAlly,
				ChargeOwnerIds,
				OutError))
			{
				return false;
			}
			if (ChargeOwnerIds.Num() == 1)
			{
				OutChargeOwnerUnitId = ChargeOwnerIds[0];
			}
			break;
		}
		default:
			OutError = TEXT("Heavy Arrow has an unsupported Charge source.");
			return false;
		}
		FGameXXKCardCombatUnit* ChargeOwner = FindCombatUnitById(InOutRuntime.Units, OutChargeOwnerUnitId);
		if (!ChargeOwner || !ChargeOwner->bLiving)
		{
			return true;
		}
		OutLockedCharge = GameXXKCardRules::GetCombatStatusStacks(*ChargeOwner, EGameXXKCardStatus::Charge);
		if (OutLockedCharge <= 0)
		{
			return true;
		}
		const int32 ConsumedCharge = GameXXKCardRules::ConsumeCombatStatus(
			*ChargeOwner,
			EGameXXKCardStatus::Charge,
			OutLockedCharge);
		if (ConsumedCharge != OutLockedCharge)
		{
			OutError = TEXT("Heavy Arrow Charge changed before its locked action could commit.");
			return false;
		}
		InOutResult.HeavyArrowChargeConsumed = ConsumedCharge;
		return true;
	}

	bool ResolveHeavyArrowPostLockEffects(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FGameXXKHeavyArrowRule& Rule,
		const FName ChargeOwnerUnitId,
		const TArray<FName>& CardTargetIds,
		const int32 LockedCharge,
		FGameXXKCardPlayResult& InOutResult,
		FString& OutError)
	{
		if (LockedCharge <= 0 || Rule.Kind == EGameXXKHeavyArrowKind::None)
		{
			return true;
		}
		FGameXXKCardCombatUnit* ChargeOwner = FindCombatUnitById(InOutRuntime.Units, ChargeOwnerUnitId);
		if (!ChargeOwner || !ChargeOwner->bLiving || Rule.ManaPerCharge < 0)
		{
			OutError = TEXT("Heavy Arrow has no living Charge owner or has invalid Mana restoration.");
			return false;
		}
		const int64 ManaGain = static_cast<int64>(Rule.ManaPerCharge) * LockedCharge;
		ChargeOwner->Mana = static_cast<int32>(FMath::Min<int64>(
			ChargeOwner->MaxMana,
			static_cast<int64>(ChargeOwner->Mana) + ManaGain));
		const int64 DrawCount = static_cast<int64>(Rule.DrawPerCharge) * LockedCharge;
		if (DrawCount < 0 || DrawCount > MAX_int32)
		{
			OutError = TEXT("A Heavy Arrow draw count exceeds the supported range.");
			return false;
		}
		if (DrawCount > 0)
		{
			GameXXKCardRules::RemoveDefeatedPartyOwnerCards(InOutRuntime.Deck, InOutRuntime.Units);
			if (!GameXXKCardRules::DrawCards(InOutRuntime.Deck, static_cast<int32>(DrawCount), 0, &OutError))
			{
				return false;
			}
		}
		if (Rule.MinimumChargeForEnergy > 0 && LockedCharge >= Rule.MinimumChargeForEnergy)
		{
			InOutRuntime.Deck.SharedEnergy = FMath::Min(
				MaxCardBattleEnergy,
				InOutRuntime.Deck.SharedEnergy + Rule.EnergyGain);
		}

		switch (Rule.Kind)
		{
		case EGameXXKHeavyArrowKind::ExtraAttackPerCharge:
		{
			if (CardTargetIds.Num() != 1)
			{
				OutError = TEXT("A Heavy Arrow extra attack requires one stable original target.");
				return false;
			}
			for (int32 ChargeIndex = 0; ChargeIndex < LockedCharge; ++ChargeIndex)
			{
				const FGameXXKCardCombatUnit* Owner = FindCombatUnitById(InOutRuntime.Units, ChargeOwnerUnitId);
				const FGameXXKCardCombatUnit* Target = FindCombatUnitById(InOutRuntime.Units, CardTargetIds[0]);
				if (!Owner || !Owner->bLiving || !Target || !Target->bLiving)
				{
					break;
				}
				const int64 RequestedDamage = static_cast<int64>(Owner->Attack) * Rule.MagnitudePerCharge / 100;
				if (RequestedDamage <= 0 || RequestedDamage > MAX_int32)
				{
					OutError = TEXT("A Heavy Arrow extra attack produced an unsupported damage amount.");
					return false;
				}
				FGameXXKCardDamageContext Context;
				Context.SourceUnitId = ChargeOwnerUnitId;
				Context.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
				Context.ResolutionOrigin = EGameXXKCardResolutionOrigin::HeavyArrow;
				FGameXXKCardDamageResult DamageResult;
				if (!GameXXKCardRules::ApplyPlayerCardDirectDamage(
					InOutRuntime,
					Context,
					CardTargetIds[0],
					static_cast<int32>(RequestedDamage),
					DamageResult,
					&OutError))
				{
					return false;
				}
				InOutResult.DamageResults.Add(DamageResult);
				++InOutResult.HeavyArrowExtraAttackCount;
				if (!DamageResult.bAvoidedByAgility)
				{
					int32 TriggeredBleedDamage = 0;
					if (!ResolveBleedAfterUnavoidedDirectHit(
						InOutRuntime,
						CardTargetIds[0],
						ChargeOwnerUnitId,
						EGameXXKCardResolutionOrigin::HeavyArrow,
						0,
						InOutResult,
						TriggeredBleedDamage,
						OutError))
					{
						return false;
					}
				}
				if (!ResolveFirstDirectDamageReactiveModifiers(
					InOutRuntime,
					Context,
					DamageResult,
					&InOutResult.DamageResults,
					OutError,
					&InOutResult))
				{
					return false;
				}
			}
			break;
		}
		case EGameXXKHeavyArrowKind::ToxicExplosionPerCharge:
		{
			if (CardTargetIds.Num() != 1)
			{
				OutError = TEXT("A Heavy Arrow Toxic Explosion requires one stable original target.");
				return false;
			}
			for (int32 ChargeIndex = 0; ChargeIndex < LockedCharge; ++ChargeIndex)
			{
				const FGameXXKCardCombatUnit* Owner = FindCombatUnitById(InOutRuntime.Units, ChargeOwnerUnitId);
				const FGameXXKCardCombatUnit* Target = FindCombatUnitById(InOutRuntime.Units, CardTargetIds[0]);
				if (!Owner || !Owner->bLiving || !Target || !Target->bLiving)
				{
					break;
				}
				TArray<FGameXXKCardDamageResult> ExplosionResults;
				if (!GameXXKCardRules::ResolveToxicExplosion(
					InOutRuntime,
					ChargeOwnerUnitId,
					CardTargetIds[0],
					false,
					ExplosionResults,
					&OutError))
				{
					return false;
				}
				TSet<EGameXXKCardDamageCause> DistinctDotCauses;
				for (const FGameXXKCardDamageResult& ExplosionResult : ExplosionResults)
				{
					DistinctDotCauses.Add(ExplosionResult.Cause);
				}
				InOutResult.ToxicExplosionDistinctDotTypeCounts.Add(DistinctDotCauses.Num());
				for (FGameXXKCardDamageResult& ExplosionResult : ExplosionResults)
				{
					ExplosionResult.ResolutionOrigin = EGameXXKCardResolutionOrigin::HeavyArrow;
					InOutResult.DamageResults.Add(MoveTemp(ExplosionResult));
				}
				++InOutResult.HeavyArrowToxicExplosionCount;
			}
			break;
		}
		case EGameXXKHeavyArrowKind::AddPrimaryAttackPercentPerCharge:
			break;
		case EGameXXKHeavyArrowKind::None:
		default:
			OutError = TEXT("A Heavy Arrow action has an unsupported rule kind.");
			return false;
		}

		const int64 BleedTriggerCount = static_cast<int64>(Rule.TriggeredBleedResolutionsPerCharge) * LockedCharge;
		if (BleedTriggerCount > MAX_int32)
		{
			OutError = TEXT("A Heavy Arrow Bleed-trigger count exceeds the supported range.");
			return false;
		}
		if (BleedTriggerCount > 0)
		{
			if (CardTargetIds.Num() != 1)
			{
				OutError = TEXT("A Heavy Arrow Bleed trigger requires one stable original target.");
				return false;
			}
			for (int32 TriggerIndex = 0; TriggerIndex < static_cast<int32>(BleedTriggerCount); ++TriggerIndex)
			{
				int32 TriggeredBleedDamage = 0;
				if (!ResolveBleedAfterUnavoidedDirectHit(
					InOutRuntime,
					CardTargetIds[0],
					ChargeOwnerUnitId,
					EGameXXKCardResolutionOrigin::HeavyArrow,
					0,
					InOutResult,
					TriggeredBleedDamage,
					OutError))
				{
					return false;
				}
			}
		}

		int64 BonusStatusStacks = static_cast<int64>(Rule.BonusStatusStacksPerCharge)
			* (LockedCharge / FMath::Max(1, Rule.BonusStatusChargeInterval));
		if (Rule.MaxBonusStatusStacks > 0) BonusStatusStacks = FMath::Min<int64>(BonusStatusStacks, Rule.MaxBonusStatusStacks);
		if (BonusStatusStacks > MAX_int32)
		{
			OutError = TEXT("A Heavy Arrow status payload exceeds the supported range.");
			return false;
		}
		if (BonusStatusStacks > 0)
		{
			FName StatusTargetUnitId = NAME_None;
			switch (Rule.BonusStatusTarget)
			{
			case EGameXXKCardEffectTarget::CardOwner:
				StatusTargetUnitId = ChargeOwnerUnitId;
				break;
			case EGameXXKCardEffectTarget::SelectedTarget:
				if (CardTargetIds.Num() != 1)
				{
					OutError = TEXT("A Heavy Arrow target status requires one stable original target.");
					return false;
				}
				StatusTargetUnitId = CardTargetIds[0];
				break;
			default:
				OutError = TEXT("A Heavy Arrow status payload has an unsupported target.");
				return false;
			}
			FGameXXKCardCombatUnit* StatusTarget = FindCombatUnitById(InOutRuntime.Units, StatusTargetUnitId);
			if (StatusTarget && StatusTarget->bLiving
				&& GameXXKCardRules::AddCombatStatus(*StatusTarget, Rule.BonusStatus, static_cast<int32>(BonusStatusStacks)) > 0
				&& StatusTarget->Side == EGameXXKCardTargetSide::Enemy
				&& !GameXXKCardRules::ResolveWhiteApeStatusGuardAfterStatusApplied(InOutRuntime, *StatusTarget, &OutError))
			{
				return false;
			}
		}
		return true;
	}

	bool HasActiveAttackEffect(const FGameXXKCardDefinition& Definition)
	{
		return Definition.Effects.ContainsByPredicate([](const FGameXXKCardEffect& Effect)
		{
			return Effect.Type == EGameXXKCardEffectType::DamagePercentAttack;
		});
	}

	FGameXXKCardInstance MakeSnapshotInstance(
		const FGameXXKResolvedCardSnapshot& Snapshot,
		const FName InstanceId)
	{
		FGameXXKCardInstance Instance;
		Instance.InstanceId = InstanceId.IsNone()
			? FName(*FString::Printf(TEXT("Snapshot.%s.%s"), *Snapshot.OwnerUnitId.ToString(), *Snapshot.CardId.ToString()))
			: InstanceId;
		Instance.CardId = Snapshot.CardId;
		Instance.CurrentQuality = Snapshot.Quality;
		Instance.OwnerUnitId = Snapshot.OwnerUnitId;
		Instance.SourceEntryId = Instance.InstanceId;
		Instance.AcquisitionOrdinal = 0;
		return Instance;
	}

	bool ConsumeTriggeredModifierUse(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FName ModifierId,
		FString& OutError)
	{
		const int32 Index = InOutRuntime.Modifiers.IndexOfByPredicate([ModifierId](const FGameXXKCardBattleModifierRuntime& Modifier)
		{
			return Modifier.ModifierId == ModifierId;
		});
		if (Index == INDEX_NONE)
		{
			OutError = TEXT("A triggered Blade modifier disappeared before it could be consumed.");
			return false;
		}

		FGameXXKCardBattleModifier& Definition = InOutRuntime.Modifiers[Index].Definition;
		switch (Definition.Expiry)
		{
		case EGameXXKCardModifierExpiry::AfterTriggerCount:
			if (Definition.RemainingTriggers <= 0)
			{
				OutError = TEXT("A triggered Blade modifier has no remaining use.");
				return false;
			}
			if (--Definition.RemainingTriggers == 0)
			{
				InOutRuntime.Modifiers.RemoveAt(Index, 1, EAllowShrinking::No);
			}
			break;
		case EGameXXKCardModifierExpiry::EndOfCurrentRound:
			break;
		case EGameXXKCardModifierExpiry::EndOfCurrentRoundOrTriggerCount:
			if (Definition.RemainingTriggers > 0 && --Definition.RemainingTriggers == 0)
			{
				InOutRuntime.Modifiers.RemoveAt(Index, 1, EAllowShrinking::No);
			}
			break;
		case EGameXXKCardModifierExpiry::Invalid:
		default:
			OutError = TEXT("A triggered Blade modifier has an invalid expiry policy.");
			return false;
		}
		return true;
	}

	bool QueueAutomaticCardReplay(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FGameXXKResolvedCardSnapshot& Snapshot,
		FString& OutError)
	{
		if (Snapshot.CardId.IsNone() || Snapshot.OwnerUnitId.IsNone())
		{
			OutError = TEXT("A Blade replay requires a complete source or triggered-card snapshot.");
			return false;
		}
		FGameXXKAutomaticResolutionQueue& Queue = InOutRuntime.AutomaticResolutionQueue;
		if (!Queue.bActive)
		{
			Queue.bActive = true;
			Queue.Origin = EGameXXKCardResolutionOrigin::AutomaticReplay;
			Queue.NextCardIndex = 0;
		}
		else if (Queue.Origin != EGameXXKCardResolutionOrigin::AutomaticReplay
			|| Queue.PendingReward != EGameXXKHeroSpellTaskReward::None
			|| Queue.PendingSorcererReward != EGameXXKSorcererRewardRule::None)
		{
			OutError = TEXT("A Blade replay cannot join a different automatic-resolution operation.");
			return false;
		}
		Queue.PendingCards.Add(Snapshot);
		return true;
	}

	bool CreateZeroCostTemporaryCopy(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FGameXXKCardInstance& PlayedInstance,
		FString& OutError);

	/**
	 * A defeated party unit's cards leave every zone at death, including a retained Blade card
	 * isolated in the exhaust pile. Its pending delay dies with the owner: cancel it so the next
	 * round boundary neither reserves a phantom hand slot nor fails the isolated-instance lookup.
	 */
	void CancelBladeDelayIfOwnerDefeated(FGameXXKCardBattleRuntime& InOutRuntime)
	{
		FGameXXKBladeDelayedCardRuntime& Delayed = InOutRuntime.PendingBladeDelayedCard;
		if (Delayed.Rule == EGameXXKBladeChargeRule::None || Delayed.RecordedInstance.OwnerUnitId.IsNone())
		{
			return;
		}
		const FGameXXKCardCombatUnit* Owner = FindCombatUnitById(InOutRuntime.Units, Delayed.RecordedInstance.OwnerUnitId);
		if (!Owner || !Owner->bLiving || Owner->Side != EGameXXKCardTargetSide::Party)
		{
			Delayed = FGameXXKBladeDelayedCardRuntime();
		}
	}

	bool ResolveBladeDelayedCardAtPlayerRoundStart(
		FGameXXKCardBattleRuntime& InOutRuntime,
		TArray<FGameXXKCardDamageResult>& InOutBoundaryDamageResults,
		FString& OutError)
	{
		if (InOutRuntime.PendingBladeDelayedCard.Rule == EGameXXKBladeChargeRule::None)
		{
			return true;
		}
		CancelBladeDelayIfOwnerDefeated(InOutRuntime);
		if (InOutRuntime.PendingBladeDelayedCard.Rule == EGameXXKBladeChargeRule::None)
		{
			return true;
		}
		if (InOutRuntime.PendingBladeDelayedCard.TriggerPlayerRound != InOutRuntime.RoundNumber)
		{
			OutError = TEXT("A delayed Blade card reached the wrong player-round boundary.");
			return false;
		}
		const FGameXXKBladeDelayedCardRuntime Delayed = InOutRuntime.PendingBladeDelayedCard;
		InOutRuntime.PendingBladeDelayedCard = FGameXXKBladeDelayedCardRuntime();
		switch (Delayed.Rule)
		{
		case EGameXXKBladeChargeRule::ReplayNextActiveNextRound:
			if (!QueueAutomaticCardReplay(InOutRuntime, Delayed.RecordedCard, OutError))
			{
				return false;
			}
			break;
		case EGameXXKBladeChargeRule::CopyNextActiveNextRound:
			if (!CreateZeroCostTemporaryCopy(InOutRuntime, Delayed.RecordedInstance, OutError))
			{
				return false;
			}
			break;
		case EGameXXKBladeChargeRule::RetainNextActiveNextRound:
		{
			const int32 ExhaustIndex = InOutRuntime.Deck.ExhaustPile.IndexOfByPredicate([&Delayed](const FGameXXKCardInstance& Card)
			{
				return Card.InstanceId == Delayed.RecordedInstance.InstanceId;
			});
			if (ExhaustIndex == INDEX_NONE
				|| !IsSameInstance(InOutRuntime.Deck.ExhaustPile[ExhaustIndex], Delayed.RecordedInstance))
			{
				OutError = TEXT("A delayed retained Blade card lost its isolated exact instance.");
				return false;
			}
			if (InOutRuntime.Deck.Hand.Num() >= BattleHandCapacity)
			{
				OutError = TEXT("A delayed retained Blade card has no available hand slot.");
				return false;
			}
			InOutRuntime.Deck.Hand.Add(MoveTemp(InOutRuntime.Deck.ExhaustPile[ExhaustIndex]));
			InOutRuntime.Deck.ExhaustPile.RemoveAt(ExhaustIndex, 1, EAllowShrinking::No);
			break;
		}
		case EGameXXKBladeChargeRule::None:
		default:
			OutError = TEXT("A delayed Blade card reached an unsupported player-round action.");
			return false;
		}

		TArray<FGameXXKCardPlayResult> AutomaticResults;
		if (!GameXXKCardRules::ResumeAutomaticResolutionQueue(InOutRuntime, AutomaticResults, &OutError))
		{
			return false;
		}
		for (FGameXXKCardPlayResult& Result : AutomaticResults)
		{
			InOutBoundaryDamageResults.Append(MoveTemp(Result.DamageResults));
		}
		return true;
	}

	void ArmImplementedPartnerBladeCharge(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FGameXXKCardDefinition& Definition,
		const FGameXXKCardInstance& Instance)
	{
		if (Definition.Owner != EGameXXKCardOwner::Profession
			|| Definition.Role != EGameXXKCharacterRole::Blade
			|| !IsImplementedPartnerBladeChargeRule(Definition.BladeSequence.ChargeRule))
		{
			return;
		}
		FGameXXKBladeChargeRuntime& Charge = InOutRuntime.PendingBladeCharge;
		Charge.Rule = Definition.BladeSequence.ChargeRule;
		Charge.SourceCardId = Definition.Id;
		Charge.SourceQuality = Instance.CurrentQuality;
		Charge.SourceOwnerUnitId = Instance.OwnerUnitId;
		Charge.CreatedRound = InOutRuntime.RoundNumber;
	}

	bool RecordBladeCardForNextRound(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FGameXXKBladeChargeRuntime& Charge,
		const FGameXXKResolvedCardSnapshot& PlayedSnapshot,
		const FGameXXKCardInstance& PlayedInstance,
		FString& OutError)
	{
		if (InOutRuntime.PendingBladeDelayedCard.Rule != EGameXXKBladeChargeRule::None)
		{
			OutError = TEXT("A Blade Charge cannot replace an unresolved delayed card.");
			return false;
		}
		FGameXXKBladeDelayedCardRuntime& Delayed = InOutRuntime.PendingBladeDelayedCard;
		Delayed.Rule = Charge.Rule;
		Delayed.SourceCardId = Charge.SourceCardId;
		Delayed.SourceQuality = Charge.SourceQuality;
		Delayed.SourceOwnerUnitId = Charge.SourceOwnerUnitId;
		Delayed.RecordedCard = PlayedSnapshot;
		Delayed.RecordedInstance = PlayedInstance;
		Delayed.TriggerPlayerRound = InOutRuntime.RoundNumber + 1;
		return true;
	}

	bool IsolateBladeCardForNextRound(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FGameXXKBladeChargeRuntime& Charge,
		const FGameXXKResolvedCardSnapshot& PlayedSnapshot,
		const FGameXXKCardInstance& PlayedInstance,
		FString& OutError)
	{
		if (InOutRuntime.PendingBladeDelayedCard.Rule != EGameXXKBladeChargeRule::None)
		{
			OutError = TEXT("A Blade Charge cannot replace an unresolved delayed card.");
			return false;
		}
		int32 ExhaustIndex = InOutRuntime.Deck.ExhaustPile.IndexOfByPredicate([&PlayedInstance](const FGameXXKCardInstance& Card)
		{
			return Card.InstanceId == PlayedInstance.InstanceId;
		});
		if (ExhaustIndex == INDEX_NONE)
		{
			const int32 DiscardIndex = InOutRuntime.Deck.DiscardPile.IndexOfByPredicate([&PlayedInstance](const FGameXXKCardInstance& Card)
			{
				return Card.InstanceId == PlayedInstance.InstanceId;
			});
			if (DiscardIndex == INDEX_NONE)
			{
				OutError = TEXT("The active card selected for delayed retention is not in discard or exhaust after resolving.");
				return false;
			}
			InOutRuntime.Deck.ExhaustPile.Add(MoveTemp(InOutRuntime.Deck.DiscardPile[DiscardIndex]));
			InOutRuntime.Deck.DiscardPile.RemoveAt(DiscardIndex, 1, EAllowShrinking::No);
			ExhaustIndex = InOutRuntime.Deck.ExhaustPile.Num() - 1;
		}
		if (!IsSameInstance(InOutRuntime.Deck.ExhaustPile[ExhaustIndex], PlayedInstance))
		{
			OutError = TEXT("The active card selected for delayed retention changed identity after resolving.");
			return false;
		}
		return RecordBladeCardForNextRound(InOutRuntime, Charge, PlayedSnapshot, PlayedInstance, OutError);
	}

	bool CreateZeroCostTemporaryCopy(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FGameXXKCardInstance& PlayedInstance,
		FString& OutError)
	{
		if (PlayedInstance.bTemporary || InOutRuntime.Deck.Hand.Num() >= BattleHandCapacity)
		{
			return true;
		}
		if (InOutRuntime.NextGeneratedCardOrdinal == MAX_int32)
		{
			OutError = TEXT("Generated-card identities have exhausted the supported range.");
			return false;
		}
		const int32 GeneratedOrdinal = InOutRuntime.NextGeneratedCardOrdinal++;
		const FName GeneratedInstanceId(*FString::Printf(
			TEXT("Generated.Blade.%d.%d"),
			InOutRuntime.RoundNumber,
			GeneratedOrdinal));
		if (InOutRuntime.Deck.ActiveInstanceIds.Contains(GeneratedInstanceId))
		{
			OutError = TEXT("A generated Blade copy collided with an existing stable card identity.");
			return false;
		}

		FGameXXKCardInstance Copy = PlayedInstance;
		Copy.InstanceId = GeneratedInstanceId;
		Copy.SourceEntryId = GeneratedInstanceId;
		Copy.AcquisitionOrdinal = GeneratedOrdinal;
		Copy.bTemporary = true;
		Copy.EnergyCostOverride = 0;
		Copy.ManaCostOverride = 0;
		Copy.ExpireAfterPlayerRound = InOutRuntime.RoundNumber;
		InOutRuntime.Deck.ActiveInstanceIds.Add(GeneratedInstanceId);
		InOutRuntime.Deck.Hand.Add(MoveTemp(Copy));
		return true;
	}

	bool RestoreConsumedStatusesAndArmor(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const TArray<FGameXXKCardCombatUnit>* UnitsBeforeActiveCard,
		FString& OutError)
	{
		if (!UnitsBeforeActiveCard)
		{
			OutError = TEXT("A restore-consumption Blade Charge has no pre-resolution unit snapshot.");
			return false;
		}
		for (const FGameXXKCardCombatUnit& Before : *UnitsBeforeActiveCard)
		{
			FGameXXKCardCombatUnit* Current = FindCombatUnitById(InOutRuntime.Units, Before.UnitId);
			if (!Current || !Current->bLiving)
			{
				continue;
			}
			const int32 ArmorToRestore = FMath::Max(0, Before.Armor - Current->Armor);
			if (ArmorToRestore > 0
				&& GameXXKCardRules::AddCombatArmor(*Current, ArmorToRestore) != ArmorToRestore)
			{
				OutError = TEXT("A restore-consumption Blade Charge could not restore the consumed Armor amount.");
				return false;
			}
			for (const FGameXXKCardStatusStack& BeforeStatus : Before.Statuses)
			{
				const int32 CurrentStacks = GameXXKCardRules::GetCombatStatusStacks(*Current, BeforeStatus.Status);
				const int32 StacksToRestore = FMath::Max(0, BeforeStatus.Stacks - CurrentStacks);
				if (StacksToRestore > 0
					&& GameXXKCardRules::AddCombatStatus(*Current, BeforeStatus.Status, StacksToRestore) != StacksToRestore)
				{
					OutError = TEXT("A restore-consumption Blade Charge could not restore a consumed status amount.");
					return false;
				}
			}
		}
		return true;
	}

	bool IsSingleUnitTargetMode(const EGameXXKCardTargetMode Mode)
	{
		switch (Mode)
		{
		case EGameXXKCardTargetMode::Self:
		case EGameXXKCardTargetMode::SingleEnemy:
		case EGameXXKCardTargetMode::SingleAlly:
		case EGameXXKCardTargetMode::OtherAlly:
		case EGameXXKCardTargetMode::RandomEnemy:
		case EGameXXKCardTargetMode::LowestHealthAlly:
		case EGameXXKCardTargetMode::LowestHealthOtherAlly:
		case EGameXXKCardTargetMode::AnyLivingUnit:
			return true;
		default:
			return false;
		}
	}

	bool DuplicateSingleTargetOrDrawTwo(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FGameXXKResolvedCardSnapshot& PlayedSnapshot,
		const FGameXXKCardInstance& PlayedInstance,
		FString& OutError)
	{
		const FGameXXKCardDefinition* BaseDefinition = FGameXXKCardCatalog::FindCardDefinition(PlayedSnapshot.CardId);
		if (!BaseDefinition)
		{
			OutError = TEXT("A branch-by-target-mode Blade Charge lost the triggered card definition.");
			return false;
		}
		const FGameXXKCardDefinition Definition = FGameXXKCardQualityRules::BuildEffectiveDefinition(
			*BaseDefinition,
			PlayedSnapshot.Quality);
		FGameXXKCardTargetRequest Request;
		if (!GameXXKCardRules::BuildTargetRequest(
			Definition,
			PlayedInstance,
			InOutRuntime.Terrain,
			BuildTargetUnitView(InOutRuntime.Units),
			Request,
			&OutError))
		{
			return false;
		}
		if (!IsSingleUnitTargetMode(Request.EffectiveMode))
		{
			GameXXKCardRules::RemoveDefeatedPartyOwnerCards(InOutRuntime.Deck, InOutRuntime.Units);
			return GameXXKCardRules::DrawCards(InOutRuntime.Deck, 2, 0, &OutError);
		}
		if (PlayedSnapshot.OriginalTargetUnitIds.Num() != 1)
		{
			OutError = TEXT("A single-target Blade Charge requires one resolved original target.");
			return false;
		}
		const FName OriginalTargetId = PlayedSnapshot.OriginalTargetUnitIds[0];
		const FGameXXKCardCombatUnit* OriginalTarget = FindCombatUnitById(InOutRuntime.Units, OriginalTargetId);
		if (!OriginalTarget)
		{
			OutError = TEXT("A single-target Blade Charge lost its original target record.");
			return false;
		}
		for (const FGameXXKCardTargetCandidateView& Candidate : Request.CandidateViews)
		{
			const FGameXXKCardCombatUnit* CandidateUnit = FindCombatUnitById(InOutRuntime.Units, Candidate.UnitId);
			if (!Candidate.bCanSelect
				|| !CandidateUnit
				|| Candidate.UnitId == OriginalTargetId
				|| CandidateUnit->Side != OriginalTarget->Side)
			{
				continue;
			}
			FGameXXKResolvedCardSnapshot DuplicatedSnapshot = PlayedSnapshot;
			DuplicatedSnapshot.OriginalTargetUnitIds = {Candidate.UnitId};
			return QueueAutomaticCardReplay(InOutRuntime, DuplicatedSnapshot, OutError);
		}
		return true;
	}

	bool DrawBladeOwnerFilteredCard(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FGameXXKCardInstance& TriggeredInstance,
		const bool bSameOwner,
		FString& OutError)
	{
		GameXXKCardRules::RemoveDefeatedPartyOwnerCards(InOutRuntime.Deck, InOutRuntime.Units);
		if (InOutRuntime.Deck.Hand.Num() >= BattleHandCapacity
			|| !EnsureDrawPileHasCard(InOutRuntime.Deck))
		{
			return true;
		}
		TArray<int32> EligibleIndices;
		for (int32 Index = 0; Index < InOutRuntime.Deck.DrawPile.Num(); ++Index)
		{
			const bool bOwnerMatches = InOutRuntime.Deck.DrawPile[Index].OwnerUnitId == TriggeredInstance.OwnerUnitId;
			if (bOwnerMatches == bSameOwner)
			{
				EligibleIndices.Add(Index);
			}
		}
		if (EligibleIndices.IsEmpty())
		{
			return GameXXKCardRules::DrawCards(InOutRuntime.Deck, 1, 0, &OutError);
		}
		const int32 PickedDrawIndex = EligibleIndices[NextRandomIndex(
			InOutRuntime.Deck.CurrentRandomState,
			EligibleIndices.Num())];
		InOutRuntime.Deck.Hand.Add(MoveTemp(InOutRuntime.Deck.DrawPile[PickedDrawIndex]));
		InOutRuntime.Deck.DrawPile.RemoveAt(PickedDrawIndex, 1, EAllowShrinking::No);
		return true;
	}

	bool ResolveImplementedSheathedChargeAfterActiveCard(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const EGameXXKBladeChargeRule Rule,
		const FGameXXKCardInstance& PlayedInstance,
		FString& OutError)
	{
		switch (Rule)
		{
		case EGameXXKBladeChargeRule::LightLoad:
			GameXXKCardRules::RemoveDefeatedPartyOwnerCards(InOutRuntime.Deck, InOutRuntime.Units);
			return GameXXKCardRules::DrawCards(InOutRuntime.Deck, 1, 0, &OutError);
		case EGameXXKBladeChargeRule::DrawTwoAfterNextActive:
			GameXXKCardRules::RemoveDefeatedPartyOwnerCards(InOutRuntime.Deck, InOutRuntime.Units);
			return GameXXKCardRules::DrawCards(InOutRuntime.Deck, 2, 0, &OutError);
		case EGameXXKBladeChargeRule::DrawSameOwnerAfterNextActive:
			return DrawBladeOwnerFilteredCard(InOutRuntime, PlayedInstance, true, OutError);
		case EGameXXKBladeChargeRule::DrawOtherOwnerAfterNextActive:
			return DrawBladeOwnerFilteredCard(InOutRuntime, PlayedInstance, false, OutError);
		case EGameXXKBladeChargeRule::None:
		default:
			OutError = TEXT("A Sheathed Charge reached an unsupported after-active action.");
			return false;
		}
	}

	bool ConsumeImplementedPartnerBladeCharge(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FGameXXKResolvedCardSnapshot& PlayedSnapshot,
		const FGameXXKCardInstance& PlayedInstance,
		const TArray<FGameXXKCardCombatUnit>* UnitsBeforeActiveCard,
		const int32 PaidEnergy,
		const int32 PaidMana,
		int32& OutAdditionalActiveCardCount,
		bool& OutPreserveFinishCandidate,
		FString& OutError)
	{
		OutAdditionalActiveCardCount = 0;
		OutPreserveFinishCandidate = false;
		if (InOutRuntime.PendingBladeCharge.Rule == EGameXXKBladeChargeRule::None)
		{
			return true;
		}
		const FGameXXKBladeChargeRuntime Charge = InOutRuntime.PendingBladeCharge;
		const EGameXXKBladeChargeRule Rule = Charge.Rule;
		InOutRuntime.PendingBladeCharge = FGameXXKBladeChargeRuntime();
		if (PlayedInstance.bTemporary
			&& Rule != EGameXXKBladeChargeRule::MakeNextActiveEnergyFree
			&& Rule != EGameXXKBladeChargeRule::MakeNextActiveManaFree
			&& Rule != EGameXXKBladeChargeRule::RefundNextActiveCosts
			&& Rule != EGameXXKBladeChargeRule::CountNextActiveTwice
			&& Rule != EGameXXKBladeChargeRule::PreserveFinishCandidate
			&& Rule != EGameXXKBladeChargeRule::RetainRemainingHand
			&& !IsImplementedSheathedStyleRule(Rule))
		{
			return true;
		}
		switch (Rule)
		{
		case EGameXXKBladeChargeRule::ReplayNextActiveBase:
			return QueueAutomaticCardReplay(InOutRuntime, PlayedSnapshot, OutError);
		case EGameXXKBladeChargeRule::CopyNextActiveToHand:
			return CreateZeroCostTemporaryCopy(InOutRuntime, PlayedInstance, OutError);
		case EGameXXKBladeChargeRule::ReturnNextActiveToHandOnce:
		{
			const int32 DiscardIndex = InOutRuntime.Deck.DiscardPile.IndexOfByPredicate([&PlayedInstance](const FGameXXKCardInstance& Card)
			{
				return Card.InstanceId == PlayedInstance.InstanceId;
			});
			if (DiscardIndex == INDEX_NONE)
			{
				OutError = TEXT("The active card selected by Blade Charge is not in the discard pile after resolving.");
				return false;
			}
			if (InOutRuntime.Deck.Hand.Num() < BattleHandCapacity)
			{
				InOutRuntime.Deck.Hand.Add(MoveTemp(InOutRuntime.Deck.DiscardPile[DiscardIndex]));
				InOutRuntime.Deck.DiscardPile.RemoveAt(DiscardIndex, 1, EAllowShrinking::No);
			}
			return true;
		}
		case EGameXXKBladeChargeRule::ReplayNextActiveNextRound:
		case EGameXXKBladeChargeRule::CopyNextActiveNextRound:
			return RecordBladeCardForNextRound(InOutRuntime, Charge, PlayedSnapshot, PlayedInstance, OutError);
		case EGameXXKBladeChargeRule::RetainNextActiveNextRound:
			return IsolateBladeCardForNextRound(InOutRuntime, Charge, PlayedSnapshot, PlayedInstance, OutError);
		case EGameXXKBladeChargeRule::RestoreNextActiveOwnerState:
			return RestoreConsumedStatusesAndArmor(InOutRuntime, UnitsBeforeActiveCard, OutError);
		case EGameXXKBladeChargeRule::DuplicateNextSingleTargetOrDraw:
			return DuplicateSingleTargetOrDrawTwo(InOutRuntime, PlayedSnapshot, PlayedInstance, OutError);
		case EGameXXKBladeChargeRule::MakeNextActiveEnergyFree:
		case EGameXXKBladeChargeRule::MakeNextActiveManaFree:
			return true;
		case EGameXXKBladeChargeRule::RefundNextActiveCosts:
		{
			if (PaidEnergy < 0 || PaidMana < 0)
			{
				OutError = TEXT("Blade Charge cannot refund negative paid costs.");
				return false;
			}
			FGameXXKCardCombatUnit* Owner = FindCombatUnitById(InOutRuntime.Units, PlayedInstance.OwnerUnitId);
			if (!Owner)
			{
				OutError = TEXT("Blade Charge lost the next active card owner before refunding costs.");
				return false;
			}
			InOutRuntime.Deck.SharedEnergy = FMath::Min(
				MaxCardBattleEnergy,
				InOutRuntime.Deck.SharedEnergy + PaidEnergy);
			Owner->Mana = static_cast<int32>(FMath::Min<int64>(
				Owner->MaxMana,
				static_cast<int64>(Owner->Mana) + PaidMana));
			return true;
		}
		case EGameXXKBladeChargeRule::CountNextActiveTwice:
			OutAdditionalActiveCardCount = 1;
			return true;
		case EGameXXKBladeChargeRule::PreserveFinishCandidate:
			OutPreserveFinishCandidate = true;
			return true;
		case EGameXXKBladeChargeRule::RetainRemainingHand:
			InOutRuntime.BladeRetainedHandCardInstanceIds.Reset();
			for (const FGameXXKCardInstance& Card : InOutRuntime.Deck.Hand)
			{
				if (!Card.bTemporary)
				{
					InOutRuntime.BladeRetainedHandCardInstanceIds.Add(Card.InstanceId);
				}
			}
			return true;
		case EGameXXKBladeChargeRule::LightLoad:
		case EGameXXKBladeChargeRule::DrawTwoAfterNextActive:
		case EGameXXKBladeChargeRule::DrawSameOwnerAfterNextActive:
		case EGameXXKBladeChargeRule::DrawOtherOwnerAfterNextActive:
			return ResolveImplementedSheathedChargeAfterActiveCard(InOutRuntime, Rule, PlayedInstance, OutError);
		case EGameXXKBladeChargeRule::None:
		default:
			OutError = TEXT("A pending partner Blade Charge reached an unsupported runtime rule.");
			return false;
		}
	}

	bool ConsumeExplicitBladeChargePayload(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const EGameXXKBladeChargeRule Rule,
		const FName SourceCardId,
		const EGameXXKCardQuality SourceQuality,
		const FName SourceOwnerUnitId,
		const FGameXXKResolvedCardSnapshot& PlayedSnapshot,
		const FGameXXKCardInstance& PlayedInstance,
		const TArray<FGameXXKCardCombatUnit>* UnitsBeforeActiveCard,
		const int32 PaidEnergy,
		const int32 PaidMana,
		int32& OutAdditionalActiveCardCount,
		bool& OutPreserveFinishCandidate,
		FString& OutError)
	{
		if (!IsImplementedPartnerBladeChargeRule(Rule))
		{
			OutError = TEXT("An explicit Blade style reached an unsupported Charge rule.");
			return false;
		}
		const FGameXXKBladeChargeRuntime SavedOrdinaryCharge = InOutRuntime.PendingBladeCharge;
		FGameXXKBladeChargeRuntime& ExplicitCharge = InOutRuntime.PendingBladeCharge;
		ExplicitCharge.Rule = Rule;
		ExplicitCharge.SourceCardId = SourceCardId;
		ExplicitCharge.SourceQuality = SourceQuality;
		ExplicitCharge.SourceOwnerUnitId = SourceOwnerUnitId;
		ExplicitCharge.CreatedRound = InOutRuntime.RoundNumber;
		const bool bResolved = ConsumeImplementedPartnerBladeCharge(
			InOutRuntime,
			PlayedSnapshot,
			PlayedInstance,
			UnitsBeforeActiveCard,
			PaidEnergy,
			PaidMana,
			OutAdditionalActiveCardCount,
			OutPreserveFinishCandidate,
			OutError);
		if (InOutRuntime.PendingBladeCharge.Rule != EGameXXKBladeChargeRule::None)
		{
			OutError = TEXT("An explicit Blade style did not consume its temporary Charge slot.");
			InOutRuntime.PendingBladeCharge = SavedOrdinaryCharge;
			return false;
		}
		InOutRuntime.PendingBladeCharge = SavedOrdinaryCharge;
		return bResolved;
	}

	bool ResolvePoJunAfterOrdinaryChargeConsumed(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FGameXXKBladeChargeRuntime& ConsumedCharge,
		FString& OutError)
	{
		if (ConsumedCharge.Rule == EGameXXKBladeChargeRule::None)
		{
			return true;
		}
		for (FGameXXKEquipmentBattleEffectRuntime& EffectRuntime : InOutRuntime.EquipmentEffects)
		{
			const FGameXXKEquipmentActiveEffect& Effect = EffectRuntime.ActiveEffect;
			if (Effect.Set != EGameXXKEquipmentSet::PoJun
				|| EffectRuntime.SourceCharacterId != ConsumedCharge.SourceOwnerUnitId)
			{
				continue;
			}
			if (Effect.Hook == EGameXXKEquipmentSetBonusHook::PoJunChargeConsumed)
			{
				if (EffectRuntime.LastTriggerRound != InOutRuntime.RoundNumber)
				{
					EffectRuntime.CurrentRoundTriggerCount = 0;
				}
				if (EffectRuntime.CurrentRoundTriggerCount >= Effect.MaxTriggersPerRound)
				{
					continue;
				}
				GameXXKCardRules::RemoveDefeatedPartyOwnerCards(InOutRuntime.Deck, InOutRuntime.Units);
				if (!GameXXKCardRules::DrawCards(InOutRuntime.Deck, Effect.Magnitude, 0, &OutError))
				{
					return false;
				}
				EffectRuntime.LastTriggerRound = InOutRuntime.RoundNumber;
				++EffectRuntime.CurrentRoundTriggerCount;
			}
			else if (Effect.Hook == EGameXXKEquipmentSetBonusHook::PoJunFirstActiveNextRound)
			{
				EffectRuntime.PoJunChargeProgressRound = InOutRuntime.RoundNumber;
				EffectRuntime.bPoJunChargeConsumedThisRound = true;
			}
		}
		return true;
	}

	void ExpirePoJunNextRoundStateAtPlayerPhaseEnd(FGameXXKCardBattleRuntime& InOutRuntime)
	{
		for (FGameXXKEquipmentBattleEffectRuntime& EffectRuntime : InOutRuntime.EquipmentEffects)
		{
			if (EffectRuntime.PendingPoJunStyle.Rule != EGameXXKBladeChargeRule::None
				&& EffectRuntime.PendingPoJunStyle.TriggerPlayerRound <= InOutRuntime.RoundNumber)
			{
				EffectRuntime.PendingPoJunStyle = FGameXXKPoJunStoredStyleRuntime();
			}
			if (EffectRuntime.PendingPoJunReplayPlayerRound > 0
				&& EffectRuntime.PendingPoJunReplayPlayerRound <= InOutRuntime.RoundNumber)
			{
				EffectRuntime.PendingPoJunReplayPlayerRound = 0;
			}
		}
	}

	bool ResolvePoJunAfterSuccessfulBladeFinish(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FGameXXKCardDefinition& Definition,
		const FGameXXKResolvedCardSnapshot& SourceSnapshot,
		FString& OutError)
	{
		if (Definition.Owner != EGameXXKCardOwner::Profession
			|| Definition.Role != EGameXXKCharacterRole::Blade
			|| !IsImplementedPartnerBladeChargeRule(Definition.BladeSequence.ChargeRule)
			|| Definition.BladeSequence.FinishRule == EGameXXKBladeFinishRule::None)
		{
			return true;
		}
		for (FGameXXKEquipmentBattleEffectRuntime& EffectRuntime : InOutRuntime.EquipmentEffects)
		{
			const FGameXXKEquipmentActiveEffect& Effect = EffectRuntime.ActiveEffect;
			if (Effect.Set != EGameXXKEquipmentSet::PoJun
				|| EffectRuntime.SourceCharacterId != SourceSnapshot.OwnerUnitId)
			{
				continue;
			}
			if (Effect.Hook == EGameXXKEquipmentSetBonusHook::PoJunBladeFinish)
			{
				FGameXXKPoJunStoredStyleRuntime& Style = EffectRuntime.PendingPoJunStyle;
				Style.Rule = Definition.BladeSequence.ChargeRule;
				Style.SourceCardId = Definition.Id;
				Style.SourceQuality = SourceSnapshot.Quality;
				Style.SourceOwnerUnitId = SourceSnapshot.OwnerUnitId;
				Style.TriggerPlayerRound = InOutRuntime.RoundNumber + 1;
			}
			else if (Effect.Hook == EGameXXKEquipmentSetBonusHook::PoJunFirstActiveNextRound
				&& EffectRuntime.bPoJunChargeConsumedThisRound
				&& EffectRuntime.PoJunChargeProgressRound == InOutRuntime.RoundNumber)
			{
				if (InOutRuntime.RoundNumber == MAX_int32)
				{
					OutError = TEXT("PoJun six-piece cannot schedule a replay beyond the supported round range.");
					return false;
				}
				EffectRuntime.PendingPoJunReplayPlayerRound = InOutRuntime.RoundNumber + 1;
			}
		}
		return true;
	}

	void ResetPoJunCurrentRoundProgress(FGameXXKCardBattleRuntime& InOutRuntime)
	{
		for (FGameXXKEquipmentBattleEffectRuntime& EffectRuntime : InOutRuntime.EquipmentEffects)
		{
			EffectRuntime.PoJunChargeProgressRound = 0;
			EffectRuntime.bPoJunChargeConsumedThisRound = false;
		}
	}

	void ClearPoJunBattleState(FGameXXKCardBattleRuntime& InOutRuntime)
	{
		for (FGameXXKEquipmentBattleEffectRuntime& EffectRuntime : InOutRuntime.EquipmentEffects)
		{
			EffectRuntime.PendingPoJunStyle = FGameXXKPoJunStoredStyleRuntime();
			EffectRuntime.PoJunChargeProgressRound = 0;
			EffectRuntime.bPoJunChargeConsumedThisRound = false;
			EffectRuntime.PendingPoJunReplayPlayerRound = 0;
		}
	}

	bool QueuePoJunOpeningReplaysAfterActiveCard(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const bool bFirstActiveThisRound,
		const FGameXXKResolvedCardSnapshot& PlayedSnapshot,
		FString& OutError)
	{
		if (!bFirstActiveThisRound)
		{
			return true;
		}
		for (FGameXXKEquipmentBattleEffectRuntime& EffectRuntime : InOutRuntime.EquipmentEffects)
		{
			if (EffectRuntime.ActiveEffect.Set != EGameXXKEquipmentSet::PoJun
				|| EffectRuntime.ActiveEffect.Hook != EGameXXKEquipmentSetBonusHook::PoJunFirstActiveNextRound
				|| EffectRuntime.PendingPoJunReplayPlayerRound != InOutRuntime.RoundNumber)
			{
				continue;
			}
			EffectRuntime.PendingPoJunReplayPlayerRound = 0;
			const FGameXXKCardCombatUnit* Wearer = FindCombatUnitById(InOutRuntime.Units, EffectRuntime.SourceCharacterId);
			if (Wearer && Wearer->bLiving
				&& !QueueAutomaticCardReplay(InOutRuntime, PlayedSnapshot, OutError))
			{
				return false;
			}
		}
		return true;
	}

	bool ResolveImplementedPartnerBladeBaseAfterActiveCard(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FGameXXKCardDefinition& Definition,
		const FGameXXKCardInstance& Instance,
		const int32 PaidEnergy,
		const int32 PaidMana,
		const FGameXXKCardPlayResult& Result,
		FString& OutError)
	{
		if (Definition.BladeSequence.BaseRule != EGameXXKBladeBaseRule::RefundCostsAndDrawOnKill)
		{
			return true;
		}
		const bool bCompletedKill = Result.DamageResults.ContainsByPredicate([&Instance](const FGameXXKCardDamageResult& Damage)
		{
			return Damage.SourceUnitId == Instance.OwnerUnitId
				&& Damage.TargetHealthBefore > 0
				&& Damage.TargetHealthAfter == 0;
		});
		if (!bCompletedKill)
		{
			return true;
		}
		if (PaidEnergy < 0 || PaidMana < 0
			|| static_cast<int64>(InOutRuntime.Deck.SharedEnergy) + PaidEnergy > MAX_int32)
		{
			OutError = TEXT("A killing Blade card cannot refund unsupported paid costs.");
			return false;
		}
		FGameXXKCardCombatUnit* Owner = FindCombatUnitById(InOutRuntime.Units, Instance.OwnerUnitId);
		if (!Owner || !Owner->bLiving)
		{
			OutError = TEXT("A killing Blade card lost its living owner before refunding costs.");
			return false;
		}
		InOutRuntime.Deck.SharedEnergy += PaidEnergy;
		Owner->Mana = static_cast<int32>(FMath::Min<int64>(
			Owner->MaxMana,
			static_cast<int64>(Owner->Mana) + PaidMana));
		GameXXKCardRules::RemoveDefeatedPartyOwnerCards(InOutRuntime.Deck, InOutRuntime.Units);
		return GameXXKCardRules::DrawCards(InOutRuntime.Deck, 1, 0, &OutError);
	}

	void ArmImplementedPartnerBladeFinish(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FGameXXKCardDefinition& Definition,
		const FGameXXKResolvedCardSnapshot& SourceSnapshot)
	{
		if (Definition.Owner != EGameXXKCardOwner::Profession
			|| Definition.Role != EGameXXKCharacterRole::Blade
			|| (Definition.BladeSequence.FinishRule != EGameXXKBladeFinishRule::ReturnFirstActiveNextRound
				&& Definition.BladeSequence.FinishRule != EGameXXKBladeFinishRule::MarkAndPrepareTwoCounters
				&& Definition.BladeSequence.FinishRule != EGameXXKBladeFinishRule::PreserveFirstTwoBleedTriggers
				&& Definition.BladeSequence.FinishRule != EGameXXKBladeFinishRule::DrawOnFirstThreeBleedTriggers
				&& Definition.BladeSequence.FinishRule != EGameXXKBladeFinishRule::HealBladeBleedCapTwelve
				&& Definition.BladeSequence.FinishRule != EGameXXKBladeFinishRule::ReturnFirstActiveAgainstBleeding
				&& Definition.BladeSequence.FinishRule != EGameXXKBladeFinishRule::FreezeVulnerabilityAndReplay
				&& Definition.BladeSequence.FinishRule != EGameXXKBladeFinishRule::CopyFirstStatusConsumer
				&& Definition.BladeSequence.FinishRule != EGameXXKBladeFinishRule::RefundFirstHighCostAndDrawTwo
				&& Definition.BladeSequence.FinishRule != EGameXXKBladeFinishRule::CopyFirstKill
				&& Definition.BladeSequence.FinishRule != EGameXXKBladeFinishRule::MarkAndReregisterCounterVolley
				&& Definition.BladeSequence.FinishRule != EGameXXKBladeFinishRule::FirstTwoDodgesFree
				&& Definition.BladeSequence.FinishRule != EGameXXKBladeFinishRule::TransferMarkBeforeCounter
				&& Definition.BladeSequence.FinishRule != EGameXXKBladeFinishRule::FirstCounterVolleyHitsAll
				&& Definition.BladeSequence.FinishRule != EGameXXKBladeFinishRule::StoreChargeAsNativeStyle))
		{
			return;
		}
		if (Definition.BladeSequence.FinishRule == EGameXXKBladeFinishRule::StoreChargeAsNativeStyle)
		{
			if (!IsImplementedSheathedStyleRule(Definition.BladeSequence.ChargeRule))
			{
				return;
			}
			FGameXXKBladeStyleRuntime& Style = InOutRuntime.PendingBladeNativeStyle;
			Style.Rule = Definition.BladeSequence.ChargeRule;
			Style.SourceCardId = Definition.Id;
			Style.SourceQuality = SourceSnapshot.Quality;
			Style.SourceOwnerUnitId = SourceSnapshot.OwnerUnitId;
			Style.TriggerPlayerRound = InOutRuntime.RoundNumber + 1;
			Style.bResidual = false;
			return;
		}
		FGameXXKBladeFinishRuntime& Finish = InOutRuntime.PendingBladeFinish;
		Finish.Rule = Definition.BladeSequence.FinishRule;
		Finish.SourceCardId = Definition.Id;
		Finish.SourceQuality = SourceSnapshot.Quality;
		Finish.SourceOwnerUnitId = SourceSnapshot.OwnerUnitId;
		Finish.TriggerPlayerRound = InOutRuntime.RoundNumber + 1;
		if (Finish.Rule == EGameXXKBladeFinishRule::FreezeVulnerabilityAndReplay
			&& SourceSnapshot.OriginalTargetUnitIds.Num() == 1)
		{
			const FName ProtectedTargetUnitId = SourceSnapshot.OriginalTargetUnitIds[0];
			const FGameXXKCardCombatUnit* ProtectedTarget = FindCombatUnitById(InOutRuntime.Units, ProtectedTargetUnitId);
			const int32 ProtectedStacks = ProtectedTarget
				? GameXXKCardRules::GetCombatStatusStacks(*ProtectedTarget, EGameXXKCardStatus::Vulnerability)
				: 0;
			if (ProtectedTarget
				&& ProtectedTarget->Side == EGameXXKCardTargetSide::Enemy
				&& ProtectedStacks > 0)
			{
				Finish.ProtectedTargetUnitId = ProtectedTargetUnitId;
				Finish.ProtectedStatusStacks = ProtectedStacks;
			}
		}
		if (Finish.Rule == EGameXXKBladeFinishRule::MarkAndPrepareTwoCounters
			|| Finish.Rule == EGameXXKBladeFinishRule::PreserveFirstTwoBleedTriggers)
		{
			Finish.RemainingTriggers = 2;
			if (Finish.Rule == EGameXXKBladeFinishRule::MarkAndPrepareTwoCounters)
			{
				if (FGameXXKCardCombatUnit* SourceOwner = FindCombatUnitById(InOutRuntime.Units, SourceSnapshot.OwnerUnitId))
				{
					GameXXKCardRules::AddCombatStatus(*SourceOwner, EGameXXKCardStatus::Mark, 2);
				}
			}
		}
		else if (Finish.Rule == EGameXXKBladeFinishRule::DrawOnFirstThreeBleedTriggers)
		{
			Finish.RemainingTriggers = 3;
		}
		else if (Finish.Rule == EGameXXKBladeFinishRule::HealBladeBleedCapTwelve)
		{
			// Keep the serialized enum name; the remaining healing budget now uses coefficient twenty.
			Finish.RemainingTriggers = FGameXXKCombatScalingRules::ResolveDotAddition(
				20, SourceSnapshot.Quality, InOutRuntime.TeamMaxLevelSnapshot);
		}
		else if (Finish.Rule == EGameXXKBladeFinishRule::MarkAndReregisterCounterVolley)
		{
			Finish.RemainingTriggers = 1;
			if (FGameXXKCardCombatUnit* SourceOwner = FindCombatUnitById(InOutRuntime.Units, SourceSnapshot.OwnerUnitId))
			{
				GameXXKCardRules::AddCombatStatus(*SourceOwner, EGameXXKCardStatus::Mark, 2);
			}
		}
		else if (Finish.Rule == EGameXXKBladeFinishRule::FirstTwoDodgesFree)
		{
			Finish.RemainingTriggers = 2;
		}
		else if (Finish.Rule == EGameXXKBladeFinishRule::FirstCounterVolleyHitsAll)
		{
			Finish.RemainingTriggers = 1;
		}
	}

	bool RestoreDuanYueProtectedVulnerability(FGameXXKCardBattleRuntime& InOutRuntime, FString& OutError)
	{
		const FGameXXKBladeFinishRuntime& Finish = InOutRuntime.PendingBladeFinish;
		if (Finish.Rule != EGameXXKBladeFinishRule::FreezeVulnerabilityAndReplay
			|| Finish.TriggerPlayerRound != InOutRuntime.RoundNumber
			|| Finish.ProtectedStatusStacks <= 0)
		{
			return true;
		}
		FGameXXKCardCombatUnit* Target = FindCombatUnitById(InOutRuntime.Units, Finish.ProtectedTargetUnitId);
		if (!Target || !Target->bLiving || Target->Side != EGameXXKCardTargetSide::Enemy)
		{
			return true;
		}
		const int32 CurrentStacks = GameXXKCardRules::GetCombatStatusStacks(*Target, EGameXXKCardStatus::Vulnerability);
		if (CurrentStacks >= Finish.ProtectedStatusStacks)
		{
			return true;
		}
		FGameXXKCardStatusStack* ExistingStack = Target->Statuses.FindByPredicate([](const FGameXXKCardStatusStack& Stack)
		{
			return Stack.Status == EGameXXKCardStatus::Vulnerability;
		});
		if (ExistingStack)
		{
			ExistingStack->Stacks = Finish.ProtectedStatusStacks;
		}
		else
		{
			FGameXXKCardStatusStack& RestoredStack = Target->Statuses.AddDefaulted_GetRef();
			RestoredStack.Status = EGameXXKCardStatus::Vulnerability;
			RestoredStack.Stacks = Finish.ProtectedStatusStacks;
		}
		if (GameXXKCardRules::GetCombatStatusStacks(*Target, EGameXXKCardStatus::Vulnerability) != Finish.ProtectedStatusStacks)
		{
			OutError = TEXT("Duan Yue Finish could not restore its protected Vulnerability snapshot.");
			return false;
		}
		return true;
	}

	bool DidActiveCardConsumeEnemyStatus(
		const FGameXXKCardBattleRuntime& Runtime,
		const FGameXXKCardPlayResult& Result)
	{
		return Result.DamageResults.ContainsByPredicate([&Runtime](const FGameXXKCardDamageResult& Damage)
		{
			const FGameXXKCardCombatUnit* Target = FindCombatUnitById(Runtime.Units, Damage.ResolvedTargetUnitId);
			return Damage.ResolutionOrigin == EGameXXKCardResolutionOrigin::ActivePlay
				&& Target
				&& Target->Side == EGameXXKCardTargetSide::Enemy
				&& (Damage.StatusStacksConsumed > 0
					|| Damage.MarkStacksConsumed > 0
					|| Damage.VulnerabilityStacksConsumed > 0);
		});
	}

	bool DidActiveCardCompleteKill(const FGameXXKCardPlayResult& Result)
	{
		return Result.DamageResults.ContainsByPredicate([](const FGameXXKCardDamageResult& Damage)
		{
			return Damage.ResolutionOrigin == EGameXXKCardResolutionOrigin::ActivePlay
				&& Damage.TargetHealthBefore > 0
				&& Damage.TargetHealthAfter == 0;
		});
	}

	bool TryResolveImplementedPartnerBladeFinish(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FGameXXKCardDefinition& PlayedDefinition,
		const FGameXXKCardInstance& PlayedInstance,
		const FGameXXKResolvedCardSnapshot& PlayedSnapshot,
		const bool bTargetedBleedingEnemyAtPlay,
		const bool bTargetedVulnerableEnemyAtPlay,
		const int32 PaidEnergy,
		const FGameXXKCardPlayResult& Result,
		FString& OutError)
	{
		if (InOutRuntime.PendingBladeFinish.Rule == EGameXXKBladeFinishRule::None
			|| InOutRuntime.PendingBladeFinish.TriggerPlayerRound != InOutRuntime.RoundNumber)
		{
			return true;
		}
		const EGameXXKBladeFinishRule Rule = InOutRuntime.PendingBladeFinish.Rule;
		if ((Rule == EGameXXKBladeFinishRule::ReturnFirstActiveNextRound
				|| Rule == EGameXXKBladeFinishRule::ReturnFirstActiveAgainstBleeding)
			&& (PlayedDefinition.bExhaustOnPlay || PlayedInstance.bTemporary))
		{
			return true;
		}
		if (Rule == EGameXXKBladeFinishRule::ReturnFirstActiveAgainstBleeding
			&& !bTargetedBleedingEnemyAtPlay)
		{
			return true;
		}
		if (Rule == EGameXXKBladeFinishRule::FreezeVulnerabilityAndReplay)
		{
			if (!bTargetedVulnerableEnemyAtPlay)
			{
				return true;
			}
			InOutRuntime.PendingBladeFinish = FGameXXKBladeFinishRuntime();
			return PlayedInstance.bTemporary
				? true
				: QueueAutomaticCardReplay(InOutRuntime, PlayedSnapshot, OutError);
		}
		if (Rule == EGameXXKBladeFinishRule::CopyFirstStatusConsumer)
		{
			if (!DidActiveCardConsumeEnemyStatus(InOutRuntime, Result))
			{
				return true;
			}
			InOutRuntime.PendingBladeFinish = FGameXXKBladeFinishRuntime();
			return CreateZeroCostTemporaryCopy(InOutRuntime, PlayedInstance, OutError);
		}
		if (Rule == EGameXXKBladeFinishRule::RefundFirstHighCostAndDrawTwo)
		{
			if (PlayedDefinition.EnergyCost < 2)
			{
				return true;
			}
			if (PaidEnergy < 0)
			{
				OutError = TEXT("Zhan Yi Finish cannot refund a negative paid Energy cost.");
				return false;
			}
			InOutRuntime.Deck.SharedEnergy = FMath::Min(
				MaxCardBattleEnergy,
				InOutRuntime.Deck.SharedEnergy + PaidEnergy);
			GameXXKCardRules::RemoveDefeatedPartyOwnerCards(InOutRuntime.Deck, InOutRuntime.Units);
			if (!GameXXKCardRules::DrawCards(InOutRuntime.Deck, 2, 0, &OutError))
			{
				return false;
			}
			InOutRuntime.PendingBladeFinish = FGameXXKBladeFinishRuntime();
			return true;
		}
		if (Rule == EGameXXKBladeFinishRule::CopyFirstKill)
		{
			if (!DidActiveCardCompleteKill(Result))
			{
				return true;
			}
			InOutRuntime.PendingBladeFinish = FGameXXKBladeFinishRuntime();
			return CreateZeroCostTemporaryCopy(InOutRuntime, PlayedInstance, OutError);
		}
		if (Rule != EGameXXKBladeFinishRule::ReturnFirstActiveNextRound
			&& Rule != EGameXXKBladeFinishRule::ReturnFirstActiveAgainstBleeding)
		{
			// Passive Finish windows (mark/counter prep, preserved Bleed triggers,
			// Bleed draws/heals, free dodges and volley rules) are consumed by
			// their own trigger handlers later in the round. Playing another
			// active card inside such a window is legal and must not block or
			// report an unsupported rule.
			return true;
		}
		const int32 DiscardIndex = InOutRuntime.Deck.DiscardPile.IndexOfByPredicate([&PlayedInstance](const FGameXXKCardInstance& Card)
		{
			return Card.InstanceId == PlayedInstance.InstanceId;
		});
		if (DiscardIndex == INDEX_NONE)
		{
			OutError = TEXT("The card selected by Blade Finish is not in the discard pile after resolving.");
			return false;
		}
		if (InOutRuntime.Deck.Hand.Num() < BattleHandCapacity)
		{
			InOutRuntime.Deck.Hand.Add(MoveTemp(InOutRuntime.Deck.DiscardPile[DiscardIndex]));
			InOutRuntime.Deck.DiscardPile.RemoveAt(DiscardIndex, 1, EAllowShrinking::No);
		}
		InOutRuntime.PendingBladeFinish = FGameXXKBladeFinishRuntime();
		return true;
	}

	bool PrepareImplementedBladeFinishForEnemyCard(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FGameXXKCardDamageContext& Context,
		const FName AppliedTargetUnitId,
		FString& OutError)
	{
		FGameXXKBladeFinishRuntime& Finish = InOutRuntime.PendingBladeFinish;
		if (Finish.Rule != EGameXXKBladeFinishRule::MarkAndPrepareTwoCounters
			|| Finish.RemainingTriggers <= 0
			|| Finish.bTriggeredForCurrentEnemyCard
			|| Context.Kind != EGameXXKCardDamageKind::SingleTargetAttack
			|| AppliedTargetUnitId != Finish.SourceOwnerUnitId)
		{
			return true;
		}
		FGameXXKCardCombatUnit* Recipient = FindCombatUnitById(InOutRuntime.Units, AppliedTargetUnitId);
		if (!Recipient || !Recipient->bLiving || Recipient->Side != EGameXXKCardTargetSide::Party)
		{
			OutError = TEXT("A Blade enemy-card Finish lost its living party recipient.");
			return false;
		}
		GameXXKCardRules::AddCombatStatus(*Recipient, EGameXXKCardStatus::Agility, 2);

		FGameXXKResolvedCardSnapshot SourceSnapshot;
		SourceSnapshot.CardId = Finish.SourceCardId;
		SourceSnapshot.Quality = Finish.SourceQuality;
		SourceSnapshot.OwnerUnitId = Finish.SourceOwnerUnitId;
		const FName SourceInstanceId(*FString::Printf(
			TEXT("BladeFinish.%d.%s.%d"),
			InOutRuntime.RoundNumber,
			*Finish.SourceOwnerUnitId.ToString(),
			Finish.RemainingTriggers));
		const FGameXXKCardInstance SourceInstance = MakeSnapshotInstance(SourceSnapshot, SourceInstanceId);
		if (!RegisterPartyReactionUses(
			InOutRuntime,
			SourceInstance,
			AppliedTargetUnitId,
			EGameXXKCardStatus::Counter,
			1,
			OutError))
		{
			return false;
		}
		Finish.bTriggeredForCurrentEnemyCard = true;
		if (--Finish.RemainingTriggers == 0)
		{
			Finish = FGameXXKBladeFinishRuntime();
		}
		return true;
	}

	bool IsActiveCardEligibleForModifier(
		const FGameXXKCardBattleModifierRuntime& Modifier,
		const EGameXXKCardBattleModifierTrigger Trigger,
		const FGameXXKCardDefinition& PlayedDefinition,
		const FGameXXKCardInstance& PlayedInstance,
		const FGameXXKCardCombatUnit& PlayedOwner,
		bool& OutEligible,
		FString& OutError)
	{
		OutEligible = false;
		const FGameXXKCardBattleModifier& Definition = Modifier.Definition;
		if (Definition.Trigger != Trigger)
		{
			return true;
		}
		if (Definition.Expiry == EGameXXKCardModifierExpiry::AfterTriggerCount && Definition.RemainingTriggers <= 0)
		{
			OutError = TEXT("An active-card timing modifier has no remaining use.");
			return false;
		}
		if (Definition.bExcludeSourceUnit && Modifier.SourceUnitId == PlayedInstance.OwnerUnitId)
		{
			return true;
		}
		if (Definition.RequiredTriggeredRole != EGameXXKCharacterRole::Invalid
			&& Definition.RequiredTriggeredRole != PlayedOwner.Role)
		{
			return true;
		}
		if (!Definition.RequiredTriggeredOwnerId.IsNone()
			&& Definition.RequiredTriggeredOwnerId != PlayedDefinition.OwnerId)
		{
			return true;
		}

		const bool bTriggerUsesPlayedOwnerScope = Trigger == EGameXXKCardBattleModifierTrigger::BeforeNextActiveCard
			|| Trigger == EGameXXKCardBattleModifierTrigger::AfterNextActiveCard
			|| Trigger == EGameXXKCardBattleModifierTrigger::OnNextAttack;
		if (bTriggerUsesPlayedOwnerScope
			&& Definition.RecipientScope != EGameXXKCardModifierRecipientScope::SharedDeck
			&& !Modifier.RecipientUnitIds.Contains(PlayedInstance.OwnerUnitId))
		{
			return true;
		}
		OutEligible = true;
		return true;
	}

	bool FindSatisfiedModifierTarget(
		const FGameXXKCardBattleModifierRuntime& Modifier,
		const FGameXXKResolvedCardSnapshot& PlayedSnapshot,
		FGameXXKCardBattleRuntime& InOutRuntime,
		FGameXXKCardCombatUnit& PlayedOwner,
		bool& OutSatisfied,
		FName& OutTargetUnitId,
		FString& OutError)
	{
		OutSatisfied = false;
		OutTargetUnitId = NAME_None;
		const FGameXXKCardEffectCondition& Condition = Modifier.Definition.Condition;
		const bool bNeedsTarget = Condition.Type == EGameXXKCardEffectConditionType::TargetHasStatus
			|| Condition.Type == EGameXXKCardEffectConditionType::TargetHasAnyDamageOverTime
			|| Condition.Type == EGameXXKCardEffectConditionType::TargetHealthBelowPercent;
		if (bNeedsTarget)
		{
			for (const FName TargetUnitId : PlayedSnapshot.OriginalTargetUnitIds)
			{
				FGameXXKCardCombatUnit* Target = FindCombatUnitById(InOutRuntime.Units, TargetUnitId);
				if (!Target || !Target->bLiving)
				{
					continue;
				}
				bool bCandidateSatisfied = false;
				if (!IsConditionSatisfied(Condition, InOutRuntime, PlayedOwner, Target, nullptr, bCandidateSatisfied, OutError))
				{
					return false;
				}
				if (bCandidateSatisfied)
				{
					OutSatisfied = true;
					OutTargetUnitId = TargetUnitId;
					return true;
				}
			}
			return true;
		}

		FGameXXKCardCombatUnit* OptionalTarget = PlayedSnapshot.OriginalTargetUnitIds.IsEmpty()
			? nullptr
			: FindCombatUnitById(InOutRuntime.Units, PlayedSnapshot.OriginalTargetUnitIds[0]);
		if (!IsConditionSatisfied(Condition, InOutRuntime, PlayedOwner, OptionalTarget, nullptr, OutSatisfied, OutError))
		{
			return false;
		}
		if (OutSatisfied && OptionalTarget)
		{
			OutTargetUnitId = OptionalTarget->UnitId;
		}
		return true;
	}

	bool ResolveTriggeredModifierAction(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FGameXXKCardBattleModifierRuntime& Modifier,
		const FGameXXKResolvedCardSnapshot& PlayedSnapshot,
		const FGameXXKCardInstance& PlayedInstance,
		const FName ConditionTargetUnitId,
		FGameXXKCardPlayResult* InOutResult,
		FString& OutError)
	{
		const FGameXXKCardBattleModifier& Definition = Modifier.Definition;
		if (Definition.EffectType == EGameXXKCardEffectType::TriggerTerrainBenefit)
		{
			const FGameXXKCardInstance SourceInstance = MakeSnapshotInstance(
				Modifier.SourceCardSnapshot,
				Modifier.SourceCardInstanceId);
			return ResolveTerrainBenefit(
				InOutRuntime,
				SourceInstance,
				Modifier.OriginalSelectedTargetUnitId,
				InOutRuntime.Terrain,
				Definition.Magnitude,
				EGameXXKCardResolutionOrigin::Reaction,
				InOutResult,
				OutError);
		}
		TArray<FName> RecipientUnitIds;
		switch (Definition.Target)
		{
		case EGameXXKCardEffectTarget::PlayedCard:
			RecipientUnitIds.Add(PlayedSnapshot.OwnerUnitId);
			break;
		case EGameXXKCardEffectTarget::CardOwner:
			RecipientUnitIds = Modifier.RecipientUnitIds;
			if (RecipientUnitIds.IsEmpty())
			{
				RecipientUnitIds.Add(Modifier.SourceUnitId);
			}
			break;
		case EGameXXKCardEffectTarget::SelectedTarget:
			if (!ConditionTargetUnitId.IsNone())
			{
				RecipientUnitIds.Add(ConditionTargetUnitId);
			}
			break;
		default:
			break;
		}

		switch (Definition.EffectType)
		{
		case EGameXXKCardEffectType::ApplyStatus:
			if (Definition.Status == EGameXXKCardStatus::None || Definition.Magnitude <= 0 || RecipientUnitIds.IsEmpty())
			{
				OutError = TEXT("A triggered status grant has invalid status, magnitude, or recipient data.");
				return false;
			}
			for (const FName RecipientUnitId : RecipientUnitIds)
			{
				FGameXXKCardCombatUnit* Recipient = FindCombatUnitById(InOutRuntime.Units, RecipientUnitId);
				if (Recipient && Recipient->bLiving)
				{
					if (!GrantStatusFromCardEffect(
						InOutRuntime,
						*Recipient,
						Definition.Status,
						Definition.Magnitude,
						OutError,
						InOutResult,
						Modifier.SourceUnitId))
					{
						return false;
					}
				}
			}
			return true;
		case EGameXXKCardEffectType::RegisterReaction:
			if ((Definition.Status != EGameXXKCardStatus::Counter && Definition.Status != EGameXXKCardStatus::Block)
				|| Definition.Magnitude <= 0 || RecipientUnitIds.IsEmpty())
			{
				OutError = TEXT("A triggered reaction requires Counter or Block uses and a recipient.");
				return false;
			}
			for (const FName RecipientUnitId : RecipientUnitIds)
			{
				const FGameXXKCardInstance SourceInstance = MakeSnapshotInstance(Modifier.SourceCardSnapshot, Modifier.SourceCardInstanceId);
				if (!RegisterPartyReactionUses(InOutRuntime, SourceInstance, RecipientUnitId, Definition.Status, Definition.Magnitude, OutError))
				{
					return false;
				}
			}
			return true;
		case EGameXXKCardEffectType::DrawCards:
			if (Definition.Magnitude <= 0)
			{
				OutError = TEXT("A triggered draw requires a positive card count.");
				return false;
			}
			GameXXKCardRules::RemoveDefeatedPartyOwnerCards(InOutRuntime.Deck, InOutRuntime.Units);
			if (!GameXXKCardRules::DrawCards(InOutRuntime.Deck, Definition.Magnitude, 0, &OutError))
			{
				return false;
			}
			return true;
		case EGameXXKCardEffectType::GainEnergy:
			if (Definition.Magnitude <= 0)
			{
				OutError = TEXT("A triggered energy grant requires a positive magnitude.");
				return false;
			}
			InOutRuntime.Deck.SharedEnergy = FMath::Min(MaxCardBattleEnergy, InOutRuntime.Deck.SharedEnergy + Definition.Magnitude);
			return true;
		case EGameXXKCardEffectType::AddArmor:
			if (Definition.Magnitude <= 0 || RecipientUnitIds.IsEmpty())
			{
				OutError = TEXT("A triggered armor grant requires a positive magnitude and a recipient.");
				return false;
			}
			for (const FName RecipientUnitId : RecipientUnitIds)
			{
				FGameXXKCardCombatUnit* Recipient = FindCombatUnitById(InOutRuntime.Units, RecipientUnitId);
				if (!Recipient || !Recipient->bLiving)
				{
					continue;
				}
				if (!InOutResult)
				{
					OutError = TEXT("A triggered armor grant requires a result recorder.");
					return false;
				}
				ApplyAndRecordArmor(*InOutResult, Modifier.SourceUnitId, *Recipient, Definition.Magnitude);
			}
			return true;
		case EGameXXKCardEffectType::ReplayTriggeredCardBase:
			return QueueAutomaticCardReplay(InOutRuntime, PlayedSnapshot, OutError);
		case EGameXXKCardEffectType::ReplaySourceCardBase:
			return QueueAutomaticCardReplay(InOutRuntime, Modifier.SourceCardSnapshot, OutError);
		case EGameXXKCardEffectType::TriggerStatus:
		{
			if (ConditionTargetUnitId.IsNone())
			{
				OutError = TEXT("A triggered status damage effect requires a stable target.");
				return false;
			}
			EGameXXKCardDamageCause Cause = EGameXXKCardDamageCause::Invalid;
			switch (Definition.Status)
			{
			case EGameXXKCardStatus::Bleed:
				Cause = EGameXXKCardDamageCause::Bleed;
				break;
			case EGameXXKCardStatus::Poison:
				Cause = EGameXXKCardDamageCause::Poison;
				break;
			case EGameXXKCardStatus::Burn:
				Cause = EGameXXKCardDamageCause::Burn;
				break;
			case EGameXXKCardStatus::DamageOverTime:
				Cause = EGameXXKCardDamageCause::Rot;
				break;
			default:
				OutError = TEXT("Triggered status damage supports Bleed, Poison, Burn, or Rot only.");
				return false;
			}
			FGameXXKCardCombatUnit* Target = FindCombatUnitById(InOutRuntime.Units, ConditionTargetUnitId);
			const int32 StacksBefore = Target
				? GameXXKCardRules::GetCombatStatusStacks(*Target, Definition.Status)
				: 0;
			if (!Target || !Target->bLiving || StacksBefore <= 0)
			{
				OutError = TEXT("Triggered status damage lost its living status-bearing target.");
				return false;
			}
			FGameXXKCardDamageResult DamageResult;
			if (!ApplyStatusHealthLoss(
				InOutRuntime,
				ConditionTargetUnitId,
				Cause,
				StacksBefore,
				DamageResult,
				OutError,
				InOutResult))
			{
				return false;
			}
			DamageResult.SourceUnitId = Modifier.SourceUnitId;
			DamageResult.ResolutionOrigin = EGameXXKCardResolutionOrigin::Reaction;
			Target = FindCombatUnitById(InOutRuntime.Units, ConditionTargetUnitId);
			if (!Target)
			{
				OutError = TEXT("Triggered status target disappeared before its layer update.");
				return false;
			}
			if (!ResolveTriggeredStatusLayerConsumption(
				InOutRuntime,
				*Target,
				Definition.Status,
				0,
				Modifier.SourceUnitId,
				DamageResult.HealthDamage,
				DamageResult.StatusStacksConsumed,
				OutError,
				InOutResult))
			{
				return false;
			}
			if (DamageResult.StatusStacksConsumed != 0 && DamageResult.StatusStacksConsumed != 1)
			{
				OutError = TEXT("Triggered status damage produced an unsupported layer-consumption result.");
				return false;
			}
			if (Definition.bPreserveTriggeredStatus
				&& DamageResult.StatusStacksConsumed > 0
				&& GameXXKCardRules::AddCombatStatus(*Target, Definition.Status, DamageResult.StatusStacksConsumed) != DamageResult.StatusStacksConsumed)
			{
				OutError = TEXT("A preserving status trigger failed to restore its consumed layer.");
				return false;
			}
			if (InOutResult)
			{
				InOutResult->DamageResults.Add(MoveTemp(DamageResult));
			}
			return true;
		}
		default:
			OutError = TEXT("An active-card timing modifier reached an unsupported action.");
			return false;
		}
	}

	bool ResolveActiveCardTimingModifiers(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const TSet<FName>& PreexistingModifierIds,
		const EGameXXKCardBattleModifierTrigger Trigger,
		const FGameXXKCardDefinition& PlayedDefinition,
		const FGameXXKCardInstance& PlayedInstance,
		const FGameXXKResolvedCardSnapshot& PlayedSnapshot,
		FGameXXKCardPlayResult& InOutResult,
		FString& OutError)
	{
		const bool bAttackTrigger = Trigger == EGameXXKCardBattleModifierTrigger::OnNextAttack
			|| Trigger == EGameXXKCardBattleModifierTrigger::FirstActiveAttackAgainstStatusNextPlayerRound;
		if (bAttackTrigger && !HasActiveAttackEffect(PlayedDefinition))
		{
			return true;
		}
		FGameXXKCardCombatUnit* PlayedOwner = FindCombatUnitById(InOutRuntime.Units, PlayedInstance.OwnerUnitId);
		if (!PlayedOwner || !PlayedOwner->bLiving)
		{
			return true;
		}

		TArray<FGameXXKCardBattleModifierRuntime> Candidates;
		for (const FGameXXKCardBattleModifierRuntime& Modifier : InOutRuntime.Modifiers)
		{
			if (PreexistingModifierIds.Contains(Modifier.ModifierId)
				&& Modifier.Definition.Trigger == Trigger
				&& (Trigger != EGameXXKCardBattleModifierTrigger::OnNextAttack
					|| Modifier.Definition.EffectType == EGameXXKCardEffectType::TriggerStatus))
			{
				Candidates.Add(Modifier);
			}
		}
		for (const FGameXXKCardBattleModifierRuntime& Candidate : Candidates)
		{
			const FGameXXKCardBattleModifierRuntime* LiveModifier = InOutRuntime.Modifiers.FindByPredicate([&Candidate](const FGameXXKCardBattleModifierRuntime& Modifier)
			{
				return Modifier.ModifierId == Candidate.ModifierId;
			});
			if (!LiveModifier)
			{
				continue;
			}
			bool bEligible = false;
			if (!IsActiveCardEligibleForModifier(*LiveModifier, Trigger, PlayedDefinition, PlayedInstance, *PlayedOwner, bEligible, OutError))
			{
				return false;
			}
			if (!bEligible)
			{
				continue;
			}
			bool bConditionSatisfied = false;
			FName ConditionTargetUnitId = NAME_None;
			if (!FindSatisfiedModifierTarget(*LiveModifier, PlayedSnapshot, InOutRuntime, *PlayedOwner, bConditionSatisfied, ConditionTargetUnitId, OutError))
			{
				return false;
			}
			if (!bConditionSatisfied)
			{
				continue;
			}
			const FGameXXKCardBattleModifierRuntime ModifierCopy = *LiveModifier;
			if (!ResolveTriggeredModifierAction(InOutRuntime, ModifierCopy, PlayedSnapshot, PlayedInstance, ConditionTargetUnitId, &InOutResult, OutError)
				|| !ConsumeTriggeredModifierUse(InOutRuntime, ModifierCopy.ModifierId, OutError))
			{
				return false;
			}
		}
		return true;
	}

	bool ResolveBladeSupplementalEffects(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FGameXXKCardDefinition& EffectiveDefinition,
		const FGameXXKCardInstance& SourceInstance,
		const TArray<FName>& TargetUnitIds,
		const TArray<FGameXXKCardEffect>& SupplementalEffects,
		FGameXXKCardPlayResult& InOutResult,
		FString& OutError)
	{
		if (SupplementalEffects.IsEmpty())
		{
			return true;
		}
		FGameXXKCardDefinition SupplementalDefinition = EffectiveDefinition;
		SupplementalDefinition.Effects = SupplementalEffects;
		// WidenNextActiveSingleTarget rewrites the base definition at active-play time,
		// while Charge/Finish payloads can resolve later from the catalog definition.
		// Multiple resolved targets on an originally single-target card are the stable
		// snapshot of that rewrite, so carry the same target expansion into supplemental
		// clauses instead of trying to recover one nonexistent clicked target.
		EGameXXKCardEffectTarget WidenedEffectTarget = EGameXXKCardEffectTarget::Invalid;
		if (TargetUnitIds.Num() > 1)
		{
			switch (EffectiveDefinition.TargetSpec.Mode)
			{
			case EGameXXKCardTargetMode::SingleEnemy:
				WidenedEffectTarget = EGameXXKCardEffectTarget::AllEnemies;
				break;
			case EGameXXKCardTargetMode::SingleAlly:
				WidenedEffectTarget = EGameXXKCardEffectTarget::AllAllies;
				break;
			case EGameXXKCardTargetMode::OtherAlly:
				WidenedEffectTarget = EGameXXKCardEffectTarget::AllOtherAllies;
				break;
			default:
				break;
			}
		}
		if (WidenedEffectTarget != EGameXXKCardEffectTarget::Invalid)
		{
			for (FGameXXKCardEffect& Effect : SupplementalDefinition.Effects)
			{
				if (Effect.Target == EGameXXKCardEffectTarget::SelectedTarget)
				{
					Effect.Target = WidenedEffectTarget;
				}
				if (Effect.Type == EGameXXKCardEffectType::ApplyBattleModifier
					&& Effect.Modifier.RecipientTarget == EGameXXKCardEffectTarget::SelectedTarget)
				{
					Effect.Modifier.RecipientTarget = WidenedEffectTarget;
				}
				if (Effect.Type == EGameXXKCardEffectType::ApplyGuardLink
					&& Effect.GuardLink.Guardian == EGameXXKCardEffectTarget::SelectedTarget)
				{
					Effect.GuardLink.Guardian = WidenedEffectTarget;
				}
			}
		}
		SupplementalDefinition.ChargeEffects.Reset();
		SupplementalDefinition.FinishEffects.Reset();
		return ResolveDefinitionEffects(
			InOutRuntime,
			SupplementalDefinition,
			SourceInstance,
			TargetUnitIds,
			EGameXXKCardResolutionOrigin::ActivePlay,
			false,
			InOutResult,
			OutError);
	}

	bool ResolveImplementedBladeStylesAfterActiveCard(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FGameXXKCardDefinition& EffectiveDefinition,
		const FGameXXKCardInstance& PlayedInstance,
		const FGameXXKResolvedCardSnapshot& PlayedSnapshot,
		const TArray<FName>& TargetUnitIds,
		const TArray<FGameXXKCardCombatUnit>* UnitsBeforeActiveCard,
		const int32 PaidEnergy,
		const int32 PaidMana,
		FGameXXKCardPlayResult& InOutResult,
		int32& OutAdditionalActiveCardCount,
		bool& OutPreserveFinishCandidate,
		FString& OutError)
	{
		OutAdditionalActiveCardCount = 0;
		OutPreserveFinishCandidate = false;
		const bool bConsumeNative = InOutRuntime.PendingBladeNativeStyle.Rule != EGameXXKBladeChargeRule::None
			&& InOutRuntime.PendingBladeNativeStyle.TriggerPlayerRound == InOutRuntime.RoundNumber
			&& InOutRuntime.ActiveCardsPlayedThisRound == 0;
		const bool bConsumeResidual = InOutRuntime.PendingBladeResidualStyle.Rule != EGameXXKBladeChargeRule::None
			&& InOutRuntime.PendingBladeResidualStyle.TriggerPlayerRound == InOutRuntime.RoundNumber;
		const FGameXXKBladeStyleRuntime NativeStyle = bConsumeNative
			? InOutRuntime.PendingBladeNativeStyle
			: FGameXXKBladeStyleRuntime();
		const FGameXXKBladeStyleRuntime ResidualStyle = bConsumeResidual
			? InOutRuntime.PendingBladeResidualStyle
			: FGameXXKBladeStyleRuntime();
		TArray<FGameXXKPoJunStoredStyleRuntime> PoJunStyles;
		if (InOutRuntime.ActiveCardsPlayedThisRound == 0)
		{
			for (FGameXXKEquipmentBattleEffectRuntime& EffectRuntime : InOutRuntime.EquipmentEffects)
			{
				const FGameXXKCardCombatUnit* Wearer = FindCombatUnitById(InOutRuntime.Units, EffectRuntime.SourceCharacterId);
				if (EffectRuntime.ActiveEffect.Set != EGameXXKEquipmentSet::PoJun
					|| EffectRuntime.ActiveEffect.Hook != EGameXXKEquipmentSetBonusHook::PoJunBladeFinish
					|| EffectRuntime.PendingPoJunStyle.Rule == EGameXXKBladeChargeRule::None
					|| EffectRuntime.PendingPoJunStyle.TriggerPlayerRound != InOutRuntime.RoundNumber
					|| !Wearer
					|| !Wearer->bLiving)
				{
					continue;
				}
				PoJunStyles.Add(EffectRuntime.PendingPoJunStyle);
				EffectRuntime.PendingPoJunStyle = FGameXXKPoJunStoredStyleRuntime();
			}
		}
		if (!bConsumeNative && !bConsumeResidual && PoJunStyles.IsEmpty())
		{
			return true;
		}
		if (bConsumeNative)
		{
			InOutRuntime.PendingBladeNativeStyle = FGameXXKBladeStyleRuntime();
		}
		if (bConsumeResidual)
		{
			InOutRuntime.PendingBladeResidualStyle = FGameXXKBladeStyleRuntime();
		}

		if (EffectiveDefinition.BladeSequence.BaseRule == EGameXXKBladeBaseRule::OpenBladeExtraAttack)
		{
			FGameXXKCardEffect OpenAttack;
			OpenAttack.Type = EGameXXKCardEffectType::DamagePercentAttack;
			OpenAttack.Target = EGameXXKCardEffectTarget::SelectedTarget;
			OpenAttack.Source = EGameXXKCardEffectSource::CardOwner;
			OpenAttack.Magnitude = 90;
			OpenAttack.HitCount = 1;
			if (!ResolveBladeSupplementalEffects(
				InOutRuntime,
				EffectiveDefinition,
				PlayedInstance,
				TargetUnitIds,
				{OpenAttack},
				InOutResult,
				OutError))
			{
				return false;
			}
		}
		else if (EffectiveDefinition.BladeSequence.BaseRule == EGameXXKBladeBaseRule::OpenBladeResidualStyle
			&& (bConsumeNative || !PoJunStyles.IsEmpty()))
		{
			FGameXXKBladeStyleRuntime& NewResidual = InOutRuntime.PendingBladeResidualStyle;
			if (bConsumeNative)
			{
				NewResidual = NativeStyle;
			}
			else
			{
				const FGameXXKPoJunStoredStyleRuntime& PoJunStyle = PoJunStyles[0];
				NewResidual.Rule = PoJunStyle.Rule;
				NewResidual.SourceCardId = PoJunStyle.SourceCardId;
				NewResidual.SourceQuality = PoJunStyle.SourceQuality;
				NewResidual.SourceOwnerUnitId = PoJunStyle.SourceOwnerUnitId;
			}
			NewResidual.TriggerPlayerRound = InOutRuntime.RoundNumber;
			NewResidual.bResidual = true;
		}

		const auto ConsumeStyle = [&](
			const EGameXXKBladeChargeRule Rule,
			const FName SourceCardId,
			const EGameXXKCardQuality SourceQuality,
			const FName SourceOwnerUnitId)
		{
			int32 AdditionalActiveCardCount = 0;
			bool bPreserveFinishCandidate = false;
			if (!ConsumeExplicitBladeChargePayload(
				InOutRuntime,
				Rule,
				SourceCardId,
				SourceQuality,
				SourceOwnerUnitId,
				PlayedSnapshot,
				PlayedInstance,
				UnitsBeforeActiveCard,
				PaidEnergy,
				PaidMana,
				AdditionalActiveCardCount,
				bPreserveFinishCandidate,
				OutError))
			{
				return false;
			}
			if (AdditionalActiveCardCount < 0
				|| OutAdditionalActiveCardCount > MAX_int32 - AdditionalActiveCardCount)
			{
				OutError = TEXT("Blade style active-card counting exceeds the supported range.");
				return false;
			}
			OutAdditionalActiveCardCount += AdditionalActiveCardCount;
			OutPreserveFinishCandidate |= bPreserveFinishCandidate;
			return true;
		};
		if (bConsumeNative
			&& !ConsumeStyle(
				NativeStyle.Rule,
				NativeStyle.SourceCardId,
				NativeStyle.SourceQuality,
				NativeStyle.SourceOwnerUnitId))
		{
			return false;
		}
		if (bConsumeResidual
			&& !ConsumeStyle(
				ResidualStyle.Rule,
				ResidualStyle.SourceCardId,
				ResidualStyle.SourceQuality,
				ResidualStyle.SourceOwnerUnitId))
		{
			return false;
		}
		for (const FGameXXKPoJunStoredStyleRuntime& PoJunStyle : PoJunStyles)
		{
			if (!ConsumeStyle(
				PoJunStyle.Rule,
				PoJunStyle.SourceCardId,
				PoJunStyle.SourceQuality,
				PoJunStyle.SourceOwnerUnitId))
			{
				return false;
			}
		}
		return true;
	}

	bool RecordHeroSpellTaskActivePlay(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FGameXXKCardDefinition& Definition,
		const FGameXXKResolvedCardSnapshot& Snapshot,
		FString& OutError)
	{
		OutError.Reset();
		if (Definition.Owner != EGameXXKCardOwner::Hero
			|| !InOutRuntime.EquippedHeroCardIds.Contains(Definition.Id))
		{
			return true;
		}

		FGameXXKHeroSpellTaskRuntime& Task = InOutRuntime.HeroSpellTask;
		if (!Task.bActive)
		{
			if (Definition.LinkedRole != EGameXXKCharacterRole::Sorcerer
				|| Definition.SpellTaskReward == EGameXXKHeroSpellTaskReward::None
				|| InOutRuntime.EquippedHeroCardIds.IsEmpty()
				|| (InOutRuntime.HeroSpellTaskLastCompletedRound > 0
					&& InOutRuntime.HeroSpellTaskLastCompletedRound == InOutRuntime.RoundNumber))
			{
				return true;
			}
			const TArray<FName> EquippedSorcererCardIds = CollectEquippedHeroSorcererCardIds(InOutRuntime);
			if (EquippedSorcererCardIds.Num() != 4)
			{
				return true;
			}
			Task.bActive = true;
			Task.LockedHeroCardIds = EquippedSorcererCardIds;
			Task.StarterReward = Definition.SpellTaskReward;
			Task.StarterOwnerUnitId = Snapshot.OwnerUnitId;
		}

		if (!Task.LockedHeroCardIds.Contains(Definition.Id)
			|| Task.CompletedHeroCardIds.Contains(Definition.Id))
		{
			return true;
		}
		FGameXXKResolvedCardSnapshot RecordedSnapshot = Snapshot;
		if (Definition.TargetSpec.Presentation != EGameXXKCardTargetPresentation::PlayerSelectsUnit)
		{
			RecordedSnapshot.OriginalTargetUnitIds.Reset();
		}
		Task.CompletedHeroCardIds.Add(Definition.Id);
		Task.FirstPlayOrder.Add(MoveTemp(RecordedSnapshot));
		return true;
	}

	bool CollectNamedTaskNpcCarriedCardIds(
		const FGameXXKCardBattleRuntime& Runtime,
		const FName OwnerUnitId,
		const FName CatalogOwnerId,
		TArray<FName>& OutCardIds,
		FString& OutError)
	{
		TArray<FGameXXKCardInstance> CarriedInstances;
		const auto CollectZone = [&CarriedInstances, OwnerUnitId, CatalogOwnerId](const TArray<FGameXXKCardInstance>& Zone)
		{
			for (const FGameXXKCardInstance& Instance : Zone)
			{
				const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(Instance.CardId);
				if (!Instance.bTemporary
					&& Instance.OwnerUnitId == OwnerUnitId
					&& Definition
					&& Definition->Owner == EGameXXKCardOwner::QuestNpc
					&& Definition->OwnerId == CatalogOwnerId)
				{
					CarriedInstances.Add(Instance);
				}
			}
		};
		CollectZone(Runtime.Deck.DrawPile);
		CollectZone(Runtime.Deck.Hand);
		CollectZone(Runtime.Deck.DiscardPile);
		CollectZone(Runtime.Deck.ExhaustPile);
		CollectZone(Runtime.Deck.PendingAutomaticHandCards);
		CarriedInstances.Sort([](const FGameXXKCardInstance& Left, const FGameXXKCardInstance& Right)
		{
			return Left.AcquisitionOrdinal != Right.AcquisitionOrdinal
				? Left.AcquisitionOrdinal < Right.AcquisitionOrdinal
				: Left.InstanceId.LexicalLess(Right.InstanceId);
		});

		OutCardIds.Reset();
		for (const FGameXXKCardInstance& Instance : CarriedInstances)
		{
			if (!OutCardIds.Contains(Instance.CardId))
			{
				OutCardIds.Add(Instance.CardId);
			}
		}
		if (CarriedInstances.Num() != 3 || OutCardIds.Num() != 3)
		{
			OutError = TEXT("A named task-NPC spell task requires exactly three unique carried non-temporary cards.");
			return false;
		}
		return true;
	}

	bool RecordTaskNpcSpellTaskActivePlay(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FGameXXKCardDefinition& Definition,
		const FGameXXKResolvedCardSnapshot& Snapshot,
		FString& OutError)
	{
		OutError.Reset();
		if (Definition.Owner != EGameXXKCardOwner::QuestNpc
			|| !IsNamedTaskNpcSpellOwnerId(Definition.OwnerId))
		{
			return true;
		}

		FGameXXKTaskNpcSpellTaskRuntime* Task = InOutRuntime.TaskNpcSpellTasks.FindByPredicate(
			[&Snapshot](const FGameXXKTaskNpcSpellTaskRuntime& Candidate)
			{
				return Candidate.bActive && Candidate.OwnerUnitId == Snapshot.OwnerUnitId;
			});
		if (!Task)
		{
			TArray<FName> LockedCardIds;
			if (!CollectNamedTaskNpcCarriedCardIds(
				InOutRuntime,
				Snapshot.OwnerUnitId,
				Definition.OwnerId,
				LockedCardIds,
				OutError))
			{
				return false;
			}
			if (!LockedCardIds.Contains(Definition.Id))
			{
				OutError = TEXT("The active named task-NPC card is not one of that NPC's three carried cards.");
				return false;
			}
			FGameXXKTaskNpcSpellTaskRuntime& NewTask = InOutRuntime.TaskNpcSpellTasks.AddDefaulted_GetRef();
			NewTask.bActive = true;
			NewTask.OwnerUnitId = Snapshot.OwnerUnitId;
			NewTask.LockedCardIds = MoveTemp(LockedCardIds);
			Task = &NewTask;
		}

		if (!Task->LockedCardIds.Contains(Definition.Id)
			|| Task->CompletedCardIds.Contains(Definition.Id))
		{
			return true;
		}
		FGameXXKResolvedCardSnapshot RecordedSnapshot = Snapshot;
		if (Definition.TargetSpec.Presentation != EGameXXKCardTargetPresentation::PlayerSelectsUnit)
		{
			RecordedSnapshot.OriginalTargetUnitIds.Reset();
		}
		Task->CompletedCardIds.Add(Definition.Id);
		Task->FirstPlayOrder.Add(MoveTemp(RecordedSnapshot));
		return true;
	}

	bool CollectSorcererPartnerCarriedCardIds(
		const FGameXXKCardBattleRuntime& Runtime,
		const FName OwnerUnitId,
		TArray<FName>& OutCardIds,
		FString& OutError)
	{
		TArray<FGameXXKCardInstance> CarriedInstances;
		const auto CollectZone = [&CarriedInstances, OwnerUnitId](const TArray<FGameXXKCardInstance>& Zone)
		{
			for (const FGameXXKCardInstance& Instance : Zone)
			{
				const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(Instance.CardId);
				if (!Instance.bTemporary
					&& Instance.OwnerUnitId == OwnerUnitId
					&& Definition
					&& Definition->Owner == EGameXXKCardOwner::Profession
					&& Definition->OwnerId == FName(TEXT("Profession.Sorcerer"))
					&& Definition->Role == EGameXXKCharacterRole::Sorcerer
					&& Definition->SorcererRule.Family != EGameXXKSorcererCardFamily::None)
				{
					CarriedInstances.Add(Instance);
				}
			}
		};
		CollectZone(Runtime.Deck.DrawPile);
		CollectZone(Runtime.Deck.Hand);
		CollectZone(Runtime.Deck.DiscardPile);
		CollectZone(Runtime.Deck.ExhaustPile);
		CollectZone(Runtime.Deck.PendingAutomaticHandCards);
		CarriedInstances.Sort([](const FGameXXKCardInstance& Left, const FGameXXKCardInstance& Right)
		{
			return Left.AcquisitionOrdinal != Right.AcquisitionOrdinal
				? Left.AcquisitionOrdinal < Right.AcquisitionOrdinal
				: Left.InstanceId.LexicalLess(Right.InstanceId);
		});

		OutCardIds.Reset();
		for (const FGameXXKCardInstance& Instance : CarriedInstances)
		{
			if (!OutCardIds.Contains(Instance.CardId))
			{
				OutCardIds.Add(Instance.CardId);
			}
		}
		if (CarriedInstances.Num() != 5 || OutCardIds.Num() != 5)
		{
			OutError = TEXT("A permanent Sorcerer partner task requires exactly five unique carried non-temporary cards.");
			return false;
		}
		return true;
	}

	bool RecordSorcererPartnerTaskActivePlay(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FGameXXKCardDefinition& Definition,
		const FGameXXKCardInstance& PlayedInstance,
		FGameXXKResolvedCardSnapshot& InOutSnapshot,
		const int32 EffectivePaidMana,
		FString& OutError)
	{
		OutError.Reset();
		if (PlayedInstance.bTemporary
			|| Definition.Owner != EGameXXKCardOwner::Profession
			|| Definition.OwnerId != FName(TEXT("Profession.Sorcerer"))
			|| Definition.Role != EGameXXKCharacterRole::Sorcerer
			|| Definition.SorcererRule.Family == EGameXXKSorcererCardFamily::None
			|| Definition.SorcererRule.SequenceRule == EGameXXKSorcererSequenceRule::None
			|| Definition.SorcererRule.RewardRule == EGameXXKSorcererRewardRule::None)
		{
			return true;
		}
		if (EffectivePaidMana < 0
			|| PlayedInstance.CardId != Definition.Id
			|| PlayedInstance.OwnerUnitId != InOutSnapshot.OwnerUnitId)
		{
			OutError = TEXT("A Sorcerer partner active play has invalid owner, card, or paid-Mana audit data.");
			return false;
		}

		FGameXXKSorcererPartnerTaskRuntime* Task = InOutRuntime.SorcererPartnerTasks.FindByPredicate(
			[&InOutSnapshot](const FGameXXKSorcererPartnerTaskRuntime& Candidate)
			{
				return Candidate.OwnerUnitId == InOutSnapshot.OwnerUnitId;
			});
		if (!Task)
		{
			FGameXXKSorcererPartnerTaskRuntime& NewTask = InOutRuntime.SorcererPartnerTasks.AddDefaulted_GetRef();
			NewTask.OwnerUnitId = InOutSnapshot.OwnerUnitId;
			Task = &NewTask;
		}
		if (!Task->bActive)
		{
			TArray<FName> LockedCardIds;
			if (!CollectSorcererPartnerCarriedCardIds(
				InOutRuntime,
				InOutSnapshot.OwnerUnitId,
				LockedCardIds,
				OutError))
			{
				return false;
			}
			if (!LockedCardIds.Contains(Definition.Id))
			{
				OutError = TEXT("The active Sorcerer partner card is not one of that partner's five carried cards.");
				return false;
			}
			Task->bActive = true;
			Task->LockedCardIds = MoveTemp(LockedCardIds);
			Task->StarterReward = Definition.SorcererRule.RewardRule;
			Task->LockedBranch = Definition.SorcererRule.Family == EGameXXKSorcererCardFamily::Universal
				? EGameXXKSorcererTaskBranch::None
				: SorcererBranchForFamily(Definition.SorcererRule.Family);
		}

		if (!Task->LockedCardIds.Contains(Definition.Id))
		{
			OutError = TEXT("A Sorcerer partner active play falls outside its locked five-card task.");
			return false;
		}
		if (Task->CompletedCardIds.Contains(Definition.Id))
		{
			return true;
		}

		FGameXXKResolvedCardSnapshot RecordedSnapshot = InOutSnapshot;
		RecordedSnapshot.PaidManaCost = EffectivePaidMana;
		RecordedSnapshot.SorcererSequencePosition = Task->FirstPlayOrder.Num() + 1;
		RecordedSnapshot.PreviousSorcererFamily = EGameXXKSorcererCardFamily::None;
		if (!Task->FirstPlayOrder.IsEmpty())
		{
			const FGameXXKCardDefinition* PreviousDefinition = FGameXXKCardCatalog::FindCardDefinition(Task->FirstPlayOrder.Last().CardId);
			if (!PreviousDefinition || PreviousDefinition->SorcererRule.Family == EGameXXKSorcererCardFamily::None)
			{
				OutError = TEXT("A Sorcerer partner task lost the family of its previous recorded card.");
				return false;
			}
			RecordedSnapshot.PreviousSorcererFamily = PreviousDefinition->SorcererRule.Family;
		}

		if (Task->FirstPlayOrder.Num() == 1 && Task->LockedBranch == EGameXXKSorcererTaskBranch::None)
		{
			Task->LockedBranch = SorcererBranchForFamily(Definition.SorcererRule.Family);
			if (Task->LockedBranch == EGameXXKSorcererTaskBranch::None)
			{
				OutError = TEXT("A Universal-started Sorcerer task could not lock a branch from its second card.");
				return false;
			}
			for (FGameXXKResolvedCardSnapshot& ExistingSnapshot : Task->FirstPlayOrder)
			{
				ExistingSnapshot.SorcererTaskBranch = Task->LockedBranch;
			}
		}
		RecordedSnapshot.SorcererTaskBranch = Task->LockedBranch;
		Task->CompletedCardIds.Add(Definition.Id);
		Task->FirstPlayOrder.Add(RecordedSnapshot);
		InOutSnapshot = MoveTemp(RecordedSnapshot);
		return true;
	}

	bool ResolveSorcererPartnerStarterAutomaticHand(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FName OwnerUnitId,
		FString& OutError)
	{
		OutError.Reset();
		FGameXXKSorcererPartnerTaskRuntime* Task = InOutRuntime.SorcererPartnerTasks.FindByPredicate(
			[OwnerUnitId](const FGameXXKSorcererPartnerTaskRuntime& Candidate)
			{
				return Candidate.bActive && Candidate.OwnerUnitId == OwnerUnitId;
			});
		if (!Task || Task->FirstPlayOrder.IsEmpty() || IsActiveChoice(InOutRuntime.Deck.PendingChoice.Kind))
		{
			return true;
		}
		const FGameXXKResolvedCardSnapshot& StarterSnapshot = Task->FirstPlayOrder[0];
		const FGameXXKCardDefinition* StarterDefinition = FGameXXKCardCatalog::FindCardDefinition(StarterSnapshot.CardId);
		if (!StarterDefinition
			|| StarterSnapshot.OwnerUnitId != OwnerUnitId
			|| StarterSnapshot.SorcererSequencePosition != 1)
		{
			OutError = TEXT("A Sorcerer partner task cannot resolve automatic hand movement without its starter snapshot.");
			return false;
		}

		const auto FindMovableInstanceId = [&InOutRuntime, OwnerUnitId](const FName CardId) -> FName
		{
			for (const TArray<FGameXXKCardInstance>* Zone : {&InOutRuntime.Deck.DrawPile, &InOutRuntime.Deck.DiscardPile})
			{
				if (const FGameXXKCardInstance* Instance = Zone->FindByPredicate([OwnerUnitId, CardId](const FGameXXKCardInstance& Candidate)
				{
					return !Candidate.bTemporary
						&& Candidate.OwnerUnitId == OwnerUnitId
						&& Candidate.CardId == CardId;
				}))
				{
					return Instance->InstanceId;
				}
			}
			return NAME_None;
		};

		if (StarterDefinition->SorcererRule.Family == EGameXXKSorcererCardFamily::Universal)
		{
			if (Task->AutoHandedUniversalCardIds.Contains(StarterDefinition->Id))
			{
				return true;
			}
			for (const FName CardId : Task->LockedCardIds)
			{
				if (Task->CompletedCardIds.Contains(CardId))
				{
					continue;
				}
				const FName InstanceId = FindMovableInstanceId(CardId);
				if (!InstanceId.IsNone()
					&& !QueueInstanceForAutomaticHand(InOutRuntime.Deck, InstanceId, OutError))
				{
					return false;
				}
			}
			Task->AutoHandedUniversalCardIds.Add(StarterDefinition->Id);
		}
		else
		{
			for (const FName CardId : Task->LockedCardIds)
			{
				const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(CardId);
				if (!Definition
					|| Definition->SorcererRule.Family != EGameXXKSorcererCardFamily::Universal
					|| Task->CompletedCardIds.Contains(CardId)
					|| Task->AutoHandedUniversalCardIds.Contains(CardId))
				{
					continue;
				}
				const FName InstanceId = FindMovableInstanceId(CardId);
				if (!InstanceId.IsNone())
				{
					if (!QueueInstanceForAutomaticHand(InOutRuntime.Deck, InstanceId, OutError))
					{
						return false;
					}
					Task->AutoHandedUniversalCardIds.Add(CardId);
				}
			}
		}
		return MaterializePendingAutomaticHandCards(InOutRuntime.Deck, OutError);
	}

	bool TryStartCompletedSorcererPartnerTaskQueue(
		FGameXXKCardBattleRuntime& InOutRuntime,
		FString& OutError)
	{
		OutError.Reset();
		FGameXXKSorcererPartnerTaskRuntime* CompletedTask = InOutRuntime.SorcererPartnerTasks.FindByPredicate(
			[](const FGameXXKSorcererPartnerTaskRuntime& Task)
			{
				return Task.bActive
					&& Task.CompletedCardIds.Num() == 5
					&& Task.FirstPlayOrder.Num() == 5;
			});
		if (!CompletedTask)
		{
			return true;
		}
		if (InOutRuntime.AutomaticResolutionQueue.bActive)
		{
			const FGameXXKAutomaticResolutionQueue& ExistingQueue = InOutRuntime.AutomaticResolutionQueue;
			if (ExistingQueue.Origin == EGameXXKCardResolutionOrigin::PartnerSorcererTaskReplay
				&& ExistingQueue.RewardOwnerUnitId == CompletedTask->OwnerUnitId
				&& ExistingQueue.PendingSorcererReward == CompletedTask->StarterReward
				&& ExistingQueue.PendingCards.Num() == CompletedTask->FirstPlayOrder.Num())
			{
				return true;
			}
			OutError = TEXT("A completed Sorcerer partner task cannot replace another automatic operation.");
			return false;
		}
		if (IsActiveChoice(InOutRuntime.Deck.PendingChoice.Kind))
		{
			OutError = TEXT("A completed Sorcerer partner task cannot start while another card choice is active.");
			return false;
		}
		if (CompletedTask->LockedCardIds.Num() != 5
			|| CompletedTask->StarterReward == EGameXXKSorcererRewardRule::None
			|| CompletedTask->OwnerUnitId.IsNone())
		{
			OutError = TEXT("A completed Sorcerer partner task has invalid replay or reward metadata.");
			return false;
		}

		FGameXXKAutomaticResolutionQueue& Queue = InOutRuntime.AutomaticResolutionQueue;
		Queue.bActive = true;
		Queue.Origin = EGameXXKCardResolutionOrigin::PartnerSorcererTaskReplay;
		Queue.PendingCards = CompletedTask->FirstPlayOrder;
		Queue.NextCardIndex = 0;
		Queue.PendingSorcererReward = CompletedTask->StarterReward;
		Queue.RewardOwnerUnitId = CompletedTask->OwnerUnitId;
		return true;
	}

	bool TryStartCompletedHeroSpellTaskQueue(
		FGameXXKCardBattleRuntime& InOutRuntime,
		FString& OutError)
	{
		OutError.Reset();
		const FGameXXKHeroSpellTaskRuntime& Task = InOutRuntime.HeroSpellTask;
		if (!Task.bActive
			|| Task.CompletedHeroCardIds.Num() != 4
			|| Task.FirstPlayOrder.Num() != 4
			|| InOutRuntime.AutomaticResolutionQueue.bActive
			|| IsActiveChoice(InOutRuntime.Deck.PendingChoice.Kind))
		{
			return true;
		}
		if (Task.LockedHeroCardIds.Num() != 4
			|| Task.StarterReward == EGameXXKHeroSpellTaskReward::None
			|| Task.StarterOwnerUnitId.IsNone())
		{
			OutError = TEXT("A completed protagonist spell task has invalid replay or reward metadata.");
			return false;
		}

		FGameXXKAutomaticResolutionQueue& Queue = InOutRuntime.AutomaticResolutionQueue;
		Queue.bActive = true;
		Queue.Origin = EGameXXKCardResolutionOrigin::MageTaskReplay;
		Queue.PendingCards = Task.FirstPlayOrder;
		Queue.NextCardIndex = 0;
		Queue.PendingReward = Task.StarterReward;
		Queue.RewardOwnerUnitId = Task.StarterOwnerUnitId;
		return true;
	}

	FGameXXKCardEffect MakeTaskRewardEffect(
		const EGameXXKCardEffectType Type,
		const EGameXXKCardEffectTarget Target,
		const int32 Magnitude,
		const EGameXXKCardStatus Status = EGameXXKCardStatus::None,
		const int32 HitCount = 1)
	{
		FGameXXKCardEffect Effect;
		Effect.Type = Type;
		Effect.Target = Target;
		Effect.Source = EGameXXKCardEffectSource::CardOwner;
		Effect.Magnitude = Magnitude;
		Effect.Status = Status;
		Effect.HitCount = HitCount;
		return Effect;
	}

	bool ResolveHeroSpellTaskReward(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const EGameXXKHeroSpellTaskReward Reward,
		const FName OwnerUnitId,
		FGameXXKCardPlayResult& OutResult,
		FString& OutError)
	{
		OutError.Reset();
		OutResult = FGameXXKCardPlayResult();
		OutResult.OwnerUnitId = OwnerUnitId;
		OutResult.ResolutionOrigin = EGameXXKCardResolutionOrigin::TaskReward;
		OutResult.CardId = InOutRuntime.HeroSpellTask.FirstPlayOrder.IsEmpty()
			? NAME_None
			: InOutRuntime.HeroSpellTask.FirstPlayOrder[0].CardId;

		FGameXXKCardCombatUnit* Owner = FindCombatUnitById(InOutRuntime.Units, OwnerUnitId);
		if (!Owner)
		{
			OutError = TEXT("A protagonist spell-task reward references an absent owner.");
			return false;
		}
		if (!Owner->bLiving)
		{
			return true;
		}

		FGameXXKCardDefinition RewardDefinition;
		RewardDefinition.Id = OutResult.CardId;
		RewardDefinition.Owner = EGameXXKCardOwner::Hero;
		RewardDefinition.OwnerId = TEXT("Hero");
		RewardDefinition.Role = EGameXXKCharacterRole::Hero;
		RewardDefinition.LinkedRole = EGameXXKCharacterRole::Sorcerer;
		const EGameXXKCardQuality StarterQuality = InOutRuntime.HeroSpellTask.FirstPlayOrder.IsEmpty()
			? EGameXXKCardQuality::Common
			: InOutRuntime.HeroSpellTask.FirstPlayOrder[0].Quality;
		FGameXXKCardInstance RewardInstance;
		RewardInstance.InstanceId = FName(*FString::Printf(TEXT("TaskReward.%d.%s"), InOutRuntime.RoundNumber, *OwnerUnitId.ToString()));
		RewardInstance.CardId = OutResult.CardId;
		RewardInstance.CurrentQuality = StarterQuality;
		RewardInstance.OwnerUnitId = OwnerUnitId;
		RewardInstance.SourceEntryId = RewardInstance.InstanceId;
		RewardInstance.AcquisitionOrdinal = 0;

		switch (Reward)
		{
		case EGameXXKHeroSpellTaskReward::Fire:
			RewardDefinition.Effects = {
				MakeTaskRewardEffect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllEnemies, 6, EGameXXKCardStatus::Burn),
				MakeTaskRewardEffect(EGameXXKCardEffectType::TriggerStatus, EGameXXKCardEffectTarget::AllEnemies, 1, EGameXXKCardStatus::Burn)};
			RewardDefinition.Effects[0].MagnitudePolicy = EGameXXKCardMagnitudePolicy::DotCoefficient;
			break;
		case EGameXXKHeroSpellTaskReward::Ice:
			RewardDefinition.Effects = {
				MakeTaskRewardEffect(EGameXXKCardEffectType::DamageAllPercentAttackPerConsumedArmor, EGameXXKCardEffectTarget::AllEnemies, 100)};
			RewardDefinition.Effects[0].MagnitudePolicy = EGameXXKCardMagnitudePolicy::ContinuousQuality;
			RewardDefinition.Effects[0].SecondaryMagnitude = 1;
			break;
		case EGameXXKHeroSpellTaskReward::Lightning:
			RewardDefinition.Effects = {
				MakeTaskRewardEffect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllEnemies, 3, EGameXXKCardStatus::Mark),
				MakeTaskRewardEffect(EGameXXKCardEffectType::LightningPerTargetStatusSnapshot, EGameXXKCardEffectTarget::AllEnemies, 60, EGameXXKCardStatus::Mark)};
			RewardDefinition.Effects[1].MagnitudePolicy = EGameXXKCardMagnitudePolicy::ContinuousQuality;
			break;
		case EGameXXKHeroSpellTaskReward::Universal:
		{
			RewardDefinition.Effects = {
				MakeTaskRewardEffect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 2),
				MakeTaskRewardEffect(EGameXXKCardEffectType::GainEnergy, EGameXXKCardEffectTarget::CardOwner, 1)};
			FGameXXKCardEffect Discount = MakeTaskRewardEffect(
				EGameXXKCardEffectType::ApplyBattleModifier,
				EGameXXKCardEffectTarget::CardOwner,
				0);
			Discount.Modifier.Trigger = EGameXXKCardBattleModifierTrigger::OnCardPlayed;
			Discount.Modifier.EffectType = EGameXXKCardEffectType::ModifyEnergyCost;
			Discount.Modifier.Target = EGameXXKCardEffectTarget::PlayedCard;
			Discount.Modifier.RecipientScope = EGameXXKCardModifierRecipientScope::SharedDeck;
			Discount.Modifier.RecipientTarget = EGameXXKCardEffectTarget::PlayedCard;
			Discount.Modifier.RequiredTriggeredRole = EGameXXKCharacterRole::Hero;
			Discount.Modifier.RequiredTriggeredOwnerId = TEXT("Hero");
			Discount.Modifier.Expiry = EGameXXKCardModifierExpiry::AfterTriggerCount;
			Discount.Modifier.RemainingTriggers = 1;
			Discount.Modifier.Magnitude = -1;
			Discount.Modifier.bPersistent = true;
			RewardDefinition.Effects.Add(MoveTemp(Discount));
			break;
		}
		case EGameXXKHeroSpellTaskReward::None:
		default:
			OutError = TEXT("A protagonist spell-task queue has no supported starter reward.");
			return false;
		}

		const FGameXXKCardDefinition EffectiveRewardDefinition = FGameXXKCardQualityRules::BuildEffectiveDefinition(
			RewardDefinition,
			StarterQuality);
		return ResolveDefinitionEffects(
			InOutRuntime,
			EffectiveRewardDefinition,
			RewardInstance,
			{},
			EGameXXKCardResolutionOrigin::TaskReward,
			false,
			OutResult,
			OutError);
	}

	void AppendNestedSorcererRewardResult(
		FGameXXKCardPlayResult& InOutRewardResult,
		FGameXXKCardPlayResult&& NestedResult)
	{
		for (const FName TargetUnitId : NestedResult.TargetUnitIds)
		{
			InOutRewardResult.TargetUnitIds.AddUnique(TargetUnitId);
		}
		InOutRewardResult.DamageResults.Append(MoveTemp(NestedResult.DamageResults));
		InOutRewardResult.StatusChanges.Append(MoveTemp(NestedResult.StatusChanges));
		InOutRewardResult.HealingResults.Append(MoveTemp(NestedResult.HealingResults));
		InOutRewardResult.ArmorResults.Append(MoveTemp(NestedResult.ArmorResults));
		InOutRewardResult.ToxicExplosionDistinctDotTypeCounts.Append(MoveTemp(NestedResult.ToxicExplosionDistinctDotTypeCounts));
		InOutRewardResult.HeavyArrowChargeConsumed += NestedResult.HeavyArrowChargeConsumed;
		InOutRewardResult.HeavyArrowExtraAttackCount += NestedResult.HeavyArrowExtraAttackCount;
		InOutRewardResult.HeavyArrowToxicExplosionCount += NestedResult.HeavyArrowToxicExplosionCount;
		InOutRewardResult.HeavyArrowPrimaryBonusPercent += NestedResult.HeavyArrowPrimaryBonusPercent;
		InOutRewardResult.AutomaticResolutionCount += 1 + NestedResult.AutomaticResolutionCount;
		InOutRewardResult.MaximumAutomaticQueueDepth = FMath::Max(
			InOutRewardResult.MaximumAutomaticQueueDepth,
			NestedResult.MaximumAutomaticQueueDepth);
		InOutRewardResult.bOpenedPendingChoice |= NestedResult.bOpenedPendingChoice;
	}

	bool GrantSorcererRewardStatus(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FName TargetUnitId,
		const EGameXXKCardStatus Status,
		const int32 Stacks,
		FGameXXKCardPlayResult& InOutResult,
		FString& OutError)
	{
		if (Stacks <= 0)
		{
			return true;
		}
		FGameXXKCardCombatUnit* Target = FindCombatUnitById(InOutRuntime.Units, TargetUnitId);
		if (!Target || !Target->bLiving)
		{
			return true;
		}
		const int32 Before = GameXXKCardRules::GetCombatStatusStacks(*Target, Status);
		if (!GrantStatusFromCardEffect(
			InOutRuntime,
			*Target,
			Status,
			Stacks,
			OutError,
			&InOutResult,
			InOutResult.OwnerUnitId))
		{
			return false;
		}
		Target = FindCombatUnitById(InOutRuntime.Units, TargetUnitId);
		const int32 Applied = Target
			? GameXXKCardRules::GetCombatStatusStacks(*Target, Status) - Before
			: 0;
		if (Applied > 0)
		{
			FGameXXKCardStatusChangeResult& Change = InOutResult.StatusChanges.AddDefaulted_GetRef();
			Change.TargetUnitId = TargetUnitId;
			Change.Status = Status;
			Change.AppliedStacks = Applied;
		}
		return true;
	}

	bool TriggerSorcererRewardBurnWithoutDecay(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FName OwnerUnitId,
		const int32 TriggerCount,
		FGameXXKCardPlayResult& InOutResult,
		FString& OutError)
	{
		if (TriggerCount <= 0)
		{
			OutError = TEXT("A Sorcerer reward Burn trigger requires a positive count.");
			return false;
		}
		for (const FName EnemyUnitId : CollectLivingUnitIdsForSide(InOutRuntime, EGameXXKCardTargetSide::Enemy))
		{
			for (int32 TriggerIndex = 0; TriggerIndex < TriggerCount; ++TriggerIndex)
			{
				const FGameXXKCardCombatUnit* Enemy = FindCombatUnitById(InOutRuntime.Units, EnemyUnitId);
				const int32 BurnStacks = Enemy && Enemy->bLiving
					? GameXXKCardRules::GetCombatStatusStacks(*Enemy, EGameXXKCardStatus::Burn)
					: 0;
				if (BurnStacks <= 0)
				{
					break;
				}
				FGameXXKCardDamageResult TriggerResult;
				if (!ApplyStatusHealthLoss(
					InOutRuntime,
					EnemyUnitId,
					EGameXXKCardDamageCause::Burn,
					BurnStacks,
					TriggerResult,
					OutError))
				{
					return false;
				}
				TriggerResult.SourceUnitId = OwnerUnitId;
				TriggerResult.ResolutionOrigin = EGameXXKCardResolutionOrigin::TaskReward;
				TriggerResult.StatusStacksConsumed = 0;
				InOutResult.DamageResults.Add(MoveTemp(TriggerResult));
			}
		}
		return true;
	}

	bool ResolveSorcererRewardIceDamage(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FName OwnerUnitId,
		const int32 BasePercent,
		const int32 PercentPerArmor,
		FGameXXKCardPlayResult& InOutResult,
		int32& OutConsumedArmor,
		FString& OutError)
	{
		OutConsumedArmor = 0;
		FGameXXKCardCombatUnit* Owner = FindCombatUnitById(InOutRuntime.Units, OwnerUnitId);
		if (!Owner || !Owner->bLiving || BasePercent <= 0 || PercentPerArmor < 0)
		{
			OutError = TEXT("A Sorcerer Ice reward requires its living owner and a supported armor formula.");
			return false;
		}
		OutConsumedArmor = FMath::Max(0, Owner->Armor);
		const int32 OwnerAttack = Owner->Attack;
		Owner->Armor = 0;
		const int64 Percent = static_cast<int64>(BasePercent)
			+ static_cast<int64>(PercentPerArmor) * OutConsumedArmor;
		const int64 RawDamage = static_cast<int64>(OwnerAttack) * Percent / 100;
		if (RawDamage <= 0 || RawDamage > MAX_int32)
		{
			OutError = TEXT("A Sorcerer Ice reward produced unsupported group damage.");
			return false;
		}
		for (const FName EnemyUnitId : CollectLivingUnitIdsForSide(InOutRuntime, EGameXXKCardTargetSide::Enemy))
		{
			const FGameXXKCardCombatUnit* Enemy = FindCombatUnitById(InOutRuntime.Units, EnemyUnitId);
			if (!Enemy || !Enemy->bLiving)
			{
				continue;
			}
			FGameXXKCardDamageContext Context;
			Context.SourceUnitId = OwnerUnitId;
			Context.Kind = EGameXXKCardDamageKind::GroupAttack;
			Context.ResolutionOrigin = EGameXXKCardResolutionOrigin::TaskReward;
			FGameXXKCardDamageResult DamageResult;
			if (!GameXXKCardRules::ApplyPlayerCardDirectDamage(
				InOutRuntime,
				Context,
				EnemyUnitId,
				static_cast<int32>(RawDamage),
				DamageResult,
				&OutError))
			{
				return false;
			}
			InOutResult.DamageResults.Add(DamageResult);
			if (!ResolveFirstDirectDamageReactiveModifiers(
				InOutRuntime,
				Context,
				DamageResult,
				&InOutResult.DamageResults,
				OutError,
				&InOutResult))
			{
				return false;
			}
		}
		return true;
	}

	bool ResolveSorcererRewardReplay(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FGameXXKSorcererPartnerTaskRuntime& Task,
		const EGameXXKSorcererCardFamily RequestedFamily,
		FGameXXKCardPlayResult& OutReplayResult,
		FString& OutError)
	{
		const FGameXXKResolvedCardSnapshot* ReplaySnapshot = nullptr;
		if (RequestedFamily == EGameXXKSorcererCardFamily::None)
		{
			ReplaySnapshot = Task.FirstPlayOrder.IsEmpty() ? nullptr : &Task.FirstPlayOrder.Last();
		}
		else
		{
			for (int32 Index = Task.FirstPlayOrder.Num() - 1; Index >= 0; --Index)
			{
				const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(Task.FirstPlayOrder[Index].CardId);
				if (Definition && Definition->SorcererRule.Family == RequestedFamily)
				{
					ReplaySnapshot = &Task.FirstPlayOrder[Index];
					break;
				}
			}
		}
		if (!ReplaySnapshot)
		{
			OutError = TEXT("A Sorcerer Universal reward cannot find its requested recorded replay card.");
			return false;
		}
		if (!ResolveCardEffectsFromSnapshot(
			InOutRuntime,
			*ReplaySnapshot,
			EGameXXKCardResolutionOrigin::PartnerSorcererTaskReplay,
			OutReplayResult,
			OutError))
		{
			return false;
		}
		if (IsActiveChoice(InOutRuntime.Deck.PendingChoice.Kind))
		{
			OutError = TEXT("A completed Sorcerer reward replay unexpectedly opened an unfinished-card choice.");
			return false;
		}
		return true;
	}

	bool ResolveSorcererPartnerTaskReward(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const EGameXXKSorcererRewardRule Reward,
		const FName OwnerUnitId,
		FGameXXKCardPlayResult& OutResult,
		FString& OutError)
	{
		OutError.Reset();
		OutResult = FGameXXKCardPlayResult();
		const FGameXXKSorcererPartnerTaskRuntime* Task = InOutRuntime.SorcererPartnerTasks.FindByPredicate(
			[OwnerUnitId](const FGameXXKSorcererPartnerTaskRuntime& Candidate)
			{
				return Candidate.bActive && Candidate.OwnerUnitId == OwnerUnitId;
			});
		if (!Task
			|| Task->FirstPlayOrder.Num() != 5
			|| Task->StarterReward != Reward
			|| Reward == EGameXXKSorcererRewardRule::None)
		{
			OutError = TEXT("A Sorcerer partner task queue has no matching completed starter reward.");
			return false;
		}

		const FGameXXKSorcererPartnerTaskRuntime CompletedTask = *Task;
		OutResult.CardId = CompletedTask.FirstPlayOrder[0].CardId;
		OutResult.OwnerUnitId = OwnerUnitId;
		OutResult.ResolutionOrigin = EGameXXKCardResolutionOrigin::TaskReward;
		const FGameXXKCardCombatUnit* Owner = FindCombatUnitById(InOutRuntime.Units, OwnerUnitId);
		if (!Owner)
		{
			OutError = TEXT("A Sorcerer partner task reward references an absent owner.");
			return false;
		}
		if (!Owner->bLiving)
		{
			return true;
		}

		FGameXXKCardDefinition RewardDefinition;
		RewardDefinition.Id = OutResult.CardId;
		RewardDefinition.Owner = EGameXXKCardOwner::Profession;
		RewardDefinition.OwnerId = TEXT("Profession.Sorcerer");
		RewardDefinition.Role = EGameXXKCharacterRole::Sorcerer;
		FGameXXKCardInstance RewardInstance;
		RewardInstance.InstanceId = FName(*FString::Printf(TEXT("SorcererTaskReward.%d.%s"), InOutRuntime.RoundNumber, *OwnerUnitId.ToString()));
		RewardInstance.CardId = OutResult.CardId;
		RewardInstance.CurrentQuality = EGameXXKCardQuality::Common;
		RewardInstance.OwnerUnitId = OwnerUnitId;
		RewardInstance.SourceEntryId = RewardInstance.InstanceId;
		RewardInstance.AcquisitionOrdinal = 0;
		const auto ResolveEffects = [&](TArray<FGameXXKCardEffect> Effects) -> bool
		{
			RewardDefinition.Effects = MoveTemp(Effects);
			return ResolveDefinitionEffects(
				InOutRuntime,
				RewardDefinition,
				RewardInstance,
				{},
				EGameXXKCardResolutionOrigin::TaskReward,
				false,
				OutResult,
				OutError);
		};
		const auto ResolveLightning = [&](const int32 MarkStacks, const int32 Percent, const int32 Energy, const int32 Draw) -> bool
		{
			TArray<FGameXXKCardEffect> Effects = {
				MakeTaskRewardEffect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllEnemies, MarkStacks, EGameXXKCardStatus::Mark)};
			if (Percent > 0)
			{
				Effects.Add(MakeTaskRewardEffect(EGameXXKCardEffectType::LightningPerTargetStatusSnapshot, EGameXXKCardEffectTarget::AllEnemies, Percent, EGameXXKCardStatus::Mark));
			}
			if (Energy > 0)
			{
				Effects.Add(MakeTaskRewardEffect(EGameXXKCardEffectType::GainEnergy, EGameXXKCardEffectTarget::CardOwner, Energy));
			}
			if (Draw > 0)
			{
				Effects.Add(MakeTaskRewardEffect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, Draw));
			}
			return ResolveEffects(MoveTemp(Effects));
		};

		switch (Reward)
		{
		case EGameXXKSorcererRewardRule::CoreSearch:
			return ResolveEffects({
				MakeTaskRewardEffect(EGameXXKCardEffectType::GainEnergy, EGameXXKCardEffectTarget::CardOwner, 1),
				MakeTaskRewardEffect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::CardOwner, 8),
				MakeTaskRewardEffect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 2)});
		case EGameXXKSorcererRewardRule::CoreManaEcho:
			return ResolveEffects({
				MakeTaskRewardEffect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::AllAllies, 8),
				MakeTaskRewardEffect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 2)});
		case EGameXXKSorcererRewardRule::FireLamp:
		{
			for (const FName EnemyUnitId : CollectLivingUnitIdsForSide(InOutRuntime, EGameXXKCardTargetSide::Enemy))
			{
				const FGameXXKCardCombatUnit* Enemy = FindCombatUnitById(InOutRuntime.Units, EnemyUnitId);
				const int32 ExistingBurn = Enemy
					? GameXXKCardRules::GetCombatStatusStacks(*Enemy, EGameXXKCardStatus::Burn)
					: 0;
				if (!GrantSorcererRewardStatus(InOutRuntime, EnemyUnitId, EGameXXKCardStatus::Burn, ExistingBurn, OutResult, OutError))
				{
					return false;
				}
			}
			return true;
		}
		case EGameXXKSorcererRewardRule::FireSpread:
		{
			int32 HighestBurn = 0;
			for (const FName EnemyUnitId : CollectLivingUnitIdsForSide(InOutRuntime, EGameXXKCardTargetSide::Enemy))
			{
				const FGameXXKCardCombatUnit* Enemy = FindCombatUnitById(InOutRuntime.Units, EnemyUnitId);
				HighestBurn = FMath::Max(HighestBurn, Enemy
					? GameXXKCardRules::GetCombatStatusStacks(*Enemy, EGameXXKCardStatus::Burn)
					: 0);
			}
			for (const FName EnemyUnitId : CollectLivingUnitIdsForSide(InOutRuntime, EGameXXKCardTargetSide::Enemy))
			{
				const FGameXXKCardCombatUnit* Enemy = FindCombatUnitById(InOutRuntime.Units, EnemyUnitId);
				const int32 ExistingBurn = Enemy
					? GameXXKCardRules::GetCombatStatusStacks(*Enemy, EGameXXKCardStatus::Burn)
					: 0;
				if (!GrantSorcererRewardStatus(InOutRuntime, EnemyUnitId, EGameXXKCardStatus::Burn, HighestBurn - ExistingBurn + 3, OutResult, OutError))
				{
					return false;
				}
			}
			return true;
		}
		case EGameXXKSorcererRewardRule::FireBurst:
			return TriggerSorcererRewardBurnWithoutDecay(InOutRuntime, OwnerUnitId, 2, OutResult, OutError);
		case EGameXXKSorcererRewardRule::FireSearch:
			return ResolveEffects({
				MakeTaskRewardEffect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllEnemies, 6, EGameXXKCardStatus::Burn),
				MakeTaskRewardEffect(EGameXXKCardEffectType::GainEnergy, EGameXXKCardEffectTarget::CardOwner, 1),
				MakeTaskRewardEffect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 2)});
		case EGameXXKSorcererRewardRule::IceCurrentManaRestore:
		case EGameXXKSorcererRewardRule::IceMaxMana:
		case EGameXXKSorcererRewardRule::IceArmorDouble:
		case EGameXXKSorcererRewardRule::IceSearch:
		{
			int32 ConsumedArmor = 0;
			if (!ResolveSorcererRewardIceDamage(InOutRuntime, OwnerUnitId, 100, 20, OutResult, ConsumedArmor, OutError))
			{
				return false;
			}
			switch (Reward)
			{
			case EGameXXKSorcererRewardRule::IceCurrentManaRestore:
				return ResolveEffects({
					MakeTaskRewardEffect(EGameXXKCardEffectType::GainEnergy, EGameXXKCardEffectTarget::CardOwner, 1),
					MakeTaskRewardEffect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 1)});
			case EGameXXKSorcererRewardRule::IceMaxMana:
			{
				FGameXXKCardCombatUnit* CurrentOwner = FindCombatUnitById(InOutRuntime.Units, OwnerUnitId);
				if (!CurrentOwner || CurrentOwner->MaxMana > MAX_int32 - 8)
				{
					OutError = TEXT("The Ice maximum-Mana reward exceeds the supported range.");
					return false;
				}
				CurrentOwner->MaxMana += 8;
				CurrentOwner->Mana = CurrentOwner->MaxMana;
				return true;
			}
			case EGameXXKSorcererRewardRule::IceArmorDouble:
				return ResolveEffects({MakeTaskRewardEffect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 6)});
			case EGameXXKSorcererRewardRule::IceSearch:
				return ResolveEffects({MakeTaskRewardEffect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllEnemies, 2, EGameXXKCardStatus::Weak)});
			default:
				return false;
			}
		}
		case EGameXXKSorcererRewardRule::LightningMark:
			return ResolveLightning(5, 0, 1, 2);
		case EGameXXKSorcererRewardRule::LightningSearch:
			return ResolveLightning(3, 0, 1, 2);
		case EGameXXKSorcererRewardRule::LightningMarkHits:
			return ResolveLightning(5, 70, 0, 0);
		case EGameXXKSorcererRewardRule::LightningStorm:
			return ResolveLightning(3, 60, 0, 0);
		case EGameXXKSorcererRewardRule::UniversalScalingAttack:
		{
			switch (CompletedTask.LockedBranch)
			{
			case EGameXXKSorcererTaskBranch::Normal:
				return ResolveEffects({MakeTaskRewardEffect(EGameXXKCardEffectType::DamagePercentAttack, EGameXXKCardEffectTarget::AllEnemies, 300)});
			case EGameXXKSorcererTaskBranch::Fire:
				if (!ResolveEffects({MakeTaskRewardEffect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllEnemies, 3, EGameXXKCardStatus::Burn)}))
				{
					return false;
				}
				return TriggerSorcererRewardBurnWithoutDecay(InOutRuntime, OwnerUnitId, 1, OutResult, OutError);
			case EGameXXKSorcererTaskBranch::Ice:
			{
				int32 ConsumedArmor = 0;
				return ResolveSorcererRewardIceDamage(InOutRuntime, OwnerUnitId, 120, 25, OutResult, ConsumedArmor, OutError);
			}
			case EGameXXKSorcererTaskBranch::Lightning:
				return ResolveLightning(3, 60, 0, 0);
			case EGameXXKSorcererTaskBranch::None:
			default:
				OutError = TEXT("A Universal Sorcerer reward has no locked task branch.");
				return false;
			}
		}
		case EGameXXKSorcererRewardRule::UniversalDraw:
		{
			switch (CompletedTask.LockedBranch)
			{
			case EGameXXKSorcererTaskBranch::Normal:
				return ResolveEffects({
					MakeTaskRewardEffect(EGameXXKCardEffectType::GainEnergy, EGameXXKCardEffectTarget::CardOwner, 2),
					MakeTaskRewardEffect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 3),
					MakeTaskRewardEffect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::AllAllies, 6)});
			case EGameXXKSorcererTaskBranch::Fire:
				return ResolveEffects({
					MakeTaskRewardEffect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllEnemies, 4, EGameXXKCardStatus::Burn),
					MakeTaskRewardEffect(EGameXXKCardEffectType::GainEnergy, EGameXXKCardEffectTarget::CardOwner, 1),
					MakeTaskRewardEffect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 3)});
			case EGameXXKSorcererTaskBranch::Ice:
			{
				int32 ConsumedArmor = 0;
				if (!ResolveSorcererRewardIceDamage(InOutRuntime, OwnerUnitId, 100, 20, OutResult, ConsumedArmor, OutError))
				{
					return false;
				}
				FGameXXKCardCombatUnit* CurrentOwner = FindCombatUnitById(InOutRuntime.Units, OwnerUnitId);
				if (!CurrentOwner)
				{
					OutError = TEXT("The Universal Ice reward lost its owner before its armor refund.");
					return false;
				}
				const int32 RefundedArmor = ConsumedArmor / 4;
				if (RefundedArmor > 0)
				{
					ApplyAndRecordArmor(OutResult, OwnerUnitId, *CurrentOwner, RefundedArmor);
				}
				return ResolveEffects({
					MakeTaskRewardEffect(EGameXXKCardEffectType::GainEnergy, EGameXXKCardEffectTarget::CardOwner, 1),
					MakeTaskRewardEffect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 2)});
			}
			case EGameXXKSorcererTaskBranch::Lightning:
				return ResolveLightning(2, 40, 1, 2);
			case EGameXXKSorcererTaskBranch::None:
			default:
				OutError = TEXT("A Universal Sorcerer reward has no locked task branch.");
				return false;
			}
		}
		case EGameXXKSorcererRewardRule::UniversalPartyArmor:
		{
			switch (CompletedTask.LockedBranch)
			{
			case EGameXXKSorcererTaskBranch::Normal:
				return ResolveEffects({
					MakeTaskRewardEffect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 12),
					MakeTaskRewardEffect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllEnemies, 2, EGameXXKCardStatus::Weak)});
			case EGameXXKSorcererTaskBranch::Fire:
				return ResolveEffects({
					MakeTaskRewardEffect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 8),
					MakeTaskRewardEffect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllEnemies, 4, EGameXXKCardStatus::Burn),
					MakeTaskRewardEffect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllEnemies, 1, EGameXXKCardStatus::Weak)});
			case EGameXXKSorcererTaskBranch::Ice:
			{
				int32 ConsumedArmor = 0;
				if (!ResolveSorcererRewardIceDamage(InOutRuntime, OwnerUnitId, 100, 20, OutResult, ConsumedArmor, OutError))
				{
					return false;
				}
				return ResolveEffects({MakeTaskRewardEffect(
					EGameXXKCardEffectType::AddArmor,
					EGameXXKCardEffectTarget::AllAllies,
					6 + ConsumedArmor / 4)});
			}
			case EGameXXKSorcererTaskBranch::Lightning:
				if (!ResolveLightning(2, 30, 0, 0))
				{
					return false;
				}
				return ResolveEffects({MakeTaskRewardEffect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 6)});
			case EGameXXKSorcererTaskBranch::None:
			default:
				OutError = TEXT("A Universal Sorcerer reward has no locked task branch.");
				return false;
			}
		}
		case EGameXXKSorcererRewardRule::UniversalSearch:
		{
			FGameXXKCardPlayResult ReplayResult;
			switch (CompletedTask.LockedBranch)
			{
			case EGameXXKSorcererTaskBranch::Normal:
				if (!ResolveSorcererRewardReplay(InOutRuntime, CompletedTask, EGameXXKSorcererCardFamily::None, ReplayResult, OutError))
				{
					return false;
				}
				AppendNestedSorcererRewardResult(OutResult, MoveTemp(ReplayResult));
				return ResolveEffects({MakeTaskRewardEffect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 1)});
			case EGameXXKSorcererTaskBranch::Fire:
			{
				const TArray<FName> StableEnemyUnitIds = CollectLivingUnitIdsForSide(
					InOutRuntime,
					EGameXXKCardTargetSide::Enemy);
				TMap<FName, int32> BurnBefore;
				for (const FName EnemyUnitId : StableEnemyUnitIds)
				{
					const FGameXXKCardCombatUnit* Enemy = FindCombatUnitById(InOutRuntime.Units, EnemyUnitId);
					BurnBefore.Add(EnemyUnitId, Enemy
						? GameXXKCardRules::GetCombatStatusStacks(*Enemy, EGameXXKCardStatus::Burn)
						: 0);
				}
				if (!ResolveSorcererRewardReplay(InOutRuntime, CompletedTask, EGameXXKSorcererCardFamily::Fire, ReplayResult, OutError))
				{
					return false;
				}
				AppendNestedSorcererRewardResult(OutResult, MoveTemp(ReplayResult));
				for (const FName EnemyUnitId : StableEnemyUnitIds)
				{
					const FGameXXKCardCombatUnit* Enemy = FindCombatUnitById(InOutRuntime.Units, EnemyUnitId);
					const int32 AppliedByReplay = Enemy
						? FMath::Max(0, GameXXKCardRules::GetCombatStatusStacks(*Enemy, EGameXXKCardStatus::Burn) - BurnBefore.FindRef(EnemyUnitId))
						: 0;
					if (!GrantSorcererRewardStatus(InOutRuntime, EnemyUnitId, EGameXXKCardStatus::Burn, AppliedByReplay, OutResult, OutError))
					{
						return false;
					}
				}
				return ResolveEffects({MakeTaskRewardEffect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllEnemies, 2, EGameXXKCardStatus::Burn)});
			}
			case EGameXXKSorcererTaskBranch::Ice:
			{
				if (!ResolveSorcererRewardReplay(InOutRuntime, CompletedTask, EGameXXKSorcererCardFamily::Ice, ReplayResult, OutError))
				{
					return false;
				}
				AppendNestedSorcererRewardResult(OutResult, MoveTemp(ReplayResult));
				int32 ConsumedArmor = 0;
				if (!ResolveSorcererRewardIceDamage(InOutRuntime, OwnerUnitId, 100, 20, OutResult, ConsumedArmor, OutError))
				{
					return false;
				}
				return ResolveEffects({MakeTaskRewardEffect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 1)});
			}
			case EGameXXKSorcererTaskBranch::Lightning:
				if (!ResolveEffects({MakeTaskRewardEffect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllEnemies, 2, EGameXXKCardStatus::Mark)}))
				{
					return false;
				}
				if (!ResolveSorcererRewardReplay(InOutRuntime, CompletedTask, EGameXXKSorcererCardFamily::Lightning, ReplayResult, OutError))
				{
					return false;
				}
				AppendNestedSorcererRewardResult(OutResult, MoveTemp(ReplayResult));
				return ResolveEffects({
					MakeTaskRewardEffect(EGameXXKCardEffectType::LightningPerTargetStatusSnapshot, EGameXXKCardEffectTarget::AllEnemies, 40, EGameXXKCardStatus::Mark),
					MakeTaskRewardEffect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 1)});
			case EGameXXKSorcererTaskBranch::None:
			default:
				OutError = TEXT("A Universal Sorcerer reward has no locked task branch.");
				return false;
			}
		}
		case EGameXXKSorcererRewardRule::None:
		default:
			OutError = TEXT("A Sorcerer partner task queue has no supported starter reward.");
			return false;
		}
	}

	bool ResolveTaskNpcSpellTaskReward(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FGameXXKTaskNpcSpellTaskRuntime& Task,
		FGameXXKCardPlayResult& OutResult,
		FString& OutError)
	{
		OutError.Reset();
		OutResult = FGameXXKCardPlayResult();
		if (Task.FirstPlayOrder.Num() != 3)
		{
			OutError = TEXT("A completed named task-NPC spell task has no exact three-card replay order.");
			return false;
		}

		const FGameXXKResolvedCardSnapshot& StarterSnapshot = Task.FirstPlayOrder[0];
		const FGameXXKCardDefinition* BaseDefinition = FGameXXKCardCatalog::FindCardDefinition(StarterSnapshot.CardId);
		if (!BaseDefinition
			|| BaseDefinition->Owner != EGameXXKCardOwner::QuestNpc
			|| BaseDefinition->TaskNpcRewardEffects.IsEmpty()
			|| !IsConcreteCardQuality(StarterSnapshot.Quality))
		{
			OutError = TEXT("A completed named task-NPC spell task has no valid starter reward definition.");
			return false;
		}

		OutResult.CardId = StarterSnapshot.CardId;
		OutResult.OwnerUnitId = StarterSnapshot.OwnerUnitId;
		OutResult.ResolutionOrigin = EGameXXKCardResolutionOrigin::TaskReward;
		const FGameXXKCardCombatUnit* Owner = FindCombatUnitById(InOutRuntime.Units, StarterSnapshot.OwnerUnitId);
		if (!Owner)
		{
			OutError = TEXT("A named task-NPC spell-task reward references an absent owner.");
			return false;
		}
		if (!Owner->bLiving)
		{
			return true;
		}

		FGameXXKCardDefinition RewardDefinition = FGameXXKCardQualityRules::BuildEffectiveDefinition(
			*BaseDefinition,
			StarterSnapshot.Quality);
		RewardDefinition.Effects = RewardDefinition.TaskNpcRewardEffects;
		if (!ValidateCurrentEffectPlan(RewardDefinition, OutError))
		{
			return false;
		}

		FGameXXKCardInstance RewardInstance;
		RewardInstance.InstanceId = FName(*FString::Printf(
			TEXT("TaskNpcReward.%d.%s"),
			InOutRuntime.RoundNumber,
			*StarterSnapshot.OwnerUnitId.ToString()));
		RewardInstance.CardId = StarterSnapshot.CardId;
		RewardInstance.CurrentQuality = StarterSnapshot.Quality;
		RewardInstance.OwnerUnitId = StarterSnapshot.OwnerUnitId;
		RewardInstance.SourceEntryId = RewardInstance.InstanceId;
		RewardInstance.AcquisitionOrdinal = 0;

		TArray<FName> TargetIds;
		if (!ResolveSnapshotTargetIds(
			InOutRuntime,
			RewardDefinition,
			StarterSnapshot,
			RewardInstance,
			TargetIds,
			OutError))
		{
			return false;
		}
		OutResult.TargetUnitIds = TargetIds;
		return ResolveDefinitionEffects(
			InOutRuntime,
			RewardDefinition,
			RewardInstance,
			TargetIds,
			EGameXXKCardResolutionOrigin::TaskReward,
			TargetIds.IsEmpty(),
			OutResult,
			OutError);
	}

	bool TryResolveCompletedTaskNpcSpellTasks(
		FGameXXKCardBattleRuntime& InOutRuntime,
		FGameXXKCardPlayResult& InOutActiveResult,
		FString& OutError)
	{
		OutError.Reset();
		for (int32 TaskIndex = 0; TaskIndex < InOutRuntime.TaskNpcSpellTasks.Num();)
		{
			const FGameXXKTaskNpcSpellTaskRuntime Task = InOutRuntime.TaskNpcSpellTasks[TaskIndex];
			if (Task.CompletedCardIds.Num() != 3 || Task.FirstPlayOrder.Num() != 3)
			{
				++TaskIndex;
				continue;
			}
			if (IsActiveChoice(InOutRuntime.Deck.PendingChoice.Kind))
			{
				OutError = TEXT("A named task-NPC spell task cannot complete while another card choice is active.");
				return false;
			}
			if (InOutActiveResult.AutomaticResolutionCount > MAX_int32 - 4)
			{
				OutError = TEXT("Named task-NPC automatic resolution count exceeds the supported range.");
				return false;
			}

			InOutActiveResult.MaximumAutomaticQueueDepth = FMath::Max(
				InOutActiveResult.MaximumAutomaticQueueDepth,
				4);
			for (const FGameXXKResolvedCardSnapshot& Snapshot : Task.FirstPlayOrder)
			{
				FGameXXKCardPlayResult ReplayResult;
				if (!ResolveCardEffectsFromSnapshot(
					InOutRuntime,
					Snapshot,
					EGameXXKCardResolutionOrigin::TaskNpcTaskReplay,
					ReplayResult,
					OutError))
				{
					return false;
				}
				if (ReplayResult.bOpenedPendingChoice)
				{
					OutError = TEXT("A named task-NPC base replay unexpectedly opened an unresolved card choice.");
					return false;
				}
				InOutActiveResult.DamageResults.Append(MoveTemp(ReplayResult.DamageResults));
				InOutActiveResult.ArmorResults.Append(MoveTemp(ReplayResult.ArmorResults));
			}

			FGameXXKCardPlayResult RewardResult;
			if (!ResolveTaskNpcSpellTaskReward(InOutRuntime, Task, RewardResult, OutError))
			{
				return false;
			}
			if (RewardResult.bOpenedPendingChoice)
			{
				OutError = TEXT("A named task-NPC reward unexpectedly opened an unresolved card choice.");
				return false;
			}
			InOutActiveResult.DamageResults.Append(MoveTemp(RewardResult.DamageResults));
			InOutActiveResult.ArmorResults.Append(MoveTemp(RewardResult.ArmorResults));
			InOutActiveResult.AutomaticResolutionCount += 4;
			InOutRuntime.TaskNpcSpellTasks.RemoveAt(TaskIndex, 1, EAllowShrinking::No);
		}
		return true;
	}

	void ExpireUntriggeredNextPlayerRoundModifiers(FGameXXKCardBattleRuntime& InOutRuntime)
	{
		InOutRuntime.Modifiers.RemoveAll([](const FGameXXKCardBattleModifierRuntime& Modifier)
		{
			switch (Modifier.Definition.Trigger)
			{
			case EGameXXKCardBattleModifierTrigger::BeforeNextActiveCard:
			case EGameXXKCardBattleModifierTrigger::AfterNextActiveCard:
			case EGameXXKCardBattleModifierTrigger::NextPlayerRoundStart:
			case EGameXXKCardBattleModifierTrigger::BeforeFirstActiveCardNextPlayerRound:
			case EGameXXKCardBattleModifierTrigger::AfterFirstActiveCardNextPlayerRound:
			case EGameXXKCardBattleModifierTrigger::FirstActiveAttackAgainstStatusNextPlayerRound:
				return true;
			default:
				return false;
			}
		});
	}

	bool ResolveNextPlayerRoundStartModifiers(
		FGameXXKCardBattleRuntime& InOutRuntime,
		FString& OutError)
	{
		TArray<FGameXXKCardBattleModifierRuntime> Candidates;
		for (const FGameXXKCardBattleModifierRuntime& Modifier : InOutRuntime.Modifiers)
		{
			if (Modifier.Definition.Trigger == EGameXXKCardBattleModifierTrigger::NextPlayerRoundStart)
			{
				Candidates.Add(Modifier);
			}
		}
		for (const FGameXXKCardBattleModifierRuntime& Candidate : Candidates)
		{
			const FGameXXKCardBattleModifierRuntime* LiveModifier = InOutRuntime.Modifiers.FindByPredicate([&Candidate](const FGameXXKCardBattleModifierRuntime& Modifier)
			{
				return Modifier.ModifierId == Candidate.ModifierId;
			});
			if (!LiveModifier)
			{
				continue;
			}
			FGameXXKCardCombatUnit* SourceOwner = FindCombatUnitById(InOutRuntime.Units, LiveModifier->SourceUnitId);
			if (!SourceOwner || !SourceOwner->bLiving)
			{
				if (!ConsumeTriggeredModifierUse(InOutRuntime, LiveModifier->ModifierId, OutError))
				{
					return false;
				}
				continue;
			}
			bool bConditionSatisfied = false;
			FName ConditionTargetUnitId = NAME_None;
			if (!FindSatisfiedModifierTarget(*LiveModifier, LiveModifier->SourceCardSnapshot, InOutRuntime, *SourceOwner, bConditionSatisfied, ConditionTargetUnitId, OutError))
			{
				return false;
			}
			const FName ModifierId = LiveModifier->ModifierId;
			if (bConditionSatisfied)
			{
				const FGameXXKCardBattleModifierRuntime ModifierCopy = *LiveModifier;
				const FGameXXKCardInstance SourceInstance = MakeSnapshotInstance(ModifierCopy.SourceCardSnapshot, ModifierCopy.SourceCardInstanceId);
				if (!ResolveTriggeredModifierAction(InOutRuntime, ModifierCopy, ModifierCopy.SourceCardSnapshot, SourceInstance, ConditionTargetUnitId, nullptr, OutError))
				{
					return false;
				}
			}
			if (!ConsumeTriggeredModifierUse(InOutRuntime, ModifierId, OutError))
			{
				return false;
			}
		}
		return true;
	}
}

#if WITH_DEV_AUTOMATION_TESTS
namespace GameXXKCardRulesTestBridge
{
	bool IsMedicineReverseDamage(const FGameXXKCardDamageResult& Result, const FName OwnerUnitId)
	{
		return ::IsMedicineReverseDamage(Result, OwnerUnitId);
	}
}
#endif

bool GameXXKCardRules::InitializeCardBattleRuntime(
	FGameXXKCardBattleRuntime& InOutRuntime,
	const TArray<FGameXXKCardInstance>& Instances,
	const TArray<FGameXXKCardCombatUnit>& Units,
	const EGameXXKCardTerrain Terrain,
	const int32 InitialRandomSeed,
	FString* OutError,
	const int32 EnemyDifficultyDamagePercent)
{
	if (OutError)
	{
		OutError->Reset();
	}
	if (!IsConcreteTerrain(Terrain))
	{
		return SetFailure(OutError, TEXT("Card battle runtime requires a concrete terrain."));
	}
	FGameXXKCardBattleRuntime NewRuntime;
	NewRuntime.Phase = EGameXXKCardBattlePhase::Player;
	NewRuntime.Terrain = Terrain;
	NewRuntime.RoundNumber = 1;
	NewRuntime.Units = Units;
	NewRuntime.TeamMaxLevelSnapshot = 1;
	for (const FGameXXKCardCombatUnit& Unit : NewRuntime.Units)
	{
		if (Unit.bLiving && Unit.Side == EGameXXKCardTargetSide::Party && Unit.CombatLevel > 0)
		{
			NewRuntime.TeamMaxLevelSnapshot = FMath::Max(NewRuntime.TeamMaxLevelSnapshot, Unit.CombatLevel);
		}
	}
	NewRuntime.EnemyDifficultyDamagePercent = EnemyDifficultyDamagePercent;
	FString ValidationError;
	if (!ValidateCombatUnits(NewRuntime.Units, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	if (!InitializeBattleDeck(NewRuntime.Deck, Instances, InitialRandomSeed, OutError))
	{
		return false;
	}
	uint32 CombatSeed = static_cast<uint32>(NewRuntime.Deck.CurrentRandomState) ^ CombatRandomSalt;
	if (CombatSeed == 0)
	{
		CombatSeed = CombatRandomSalt;
	}
	NewRuntime.CombatRandomState = static_cast<int32>(CombatSeed);
	if (!ValidateCardBattleRuntimeInternal(NewRuntime, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	InOutRuntime = MoveTemp(NewRuntime);
	return true;
}

bool GameXXKCardRules::ValidateCardBattleRuntime(const FGameXXKCardBattleRuntime& Runtime, FString* OutError)
{
	FString ValidationError;
	const bool bValid = ValidateCardBattleRuntimeInternal(Runtime, ValidationError);
	if (OutError)
	{
		*OutError = ValidationError;
	}
	return bValid;
}

bool GameXXKCardRules::ValidateCardSnapshot(
	const FGameXXKResolvedCardSnapshot& Snapshot,
	const TArray<FGameXXKCardCombatUnit>& Units,
	FString* OutError)
{
	FString ValidationError;
	const bool bValid = ValidateResolvedCardSnapshot(Snapshot, Units, ValidationError);
	if (OutError)
	{
		*OutError = ValidationError;
	}
	return bValid;
}

bool GameXXKCardRules::ApplyPlayerCardDirectDamage(
	FGameXXKCardBattleRuntime& InOutRuntime,
	const FGameXXKCardDamageContext& Context,
	const FName TargetUnitId,
	const int32 RequestedDamage,
	FGameXXKCardDamageResult& OutResult,
	FString* OutError)
{
	return ApplyPlayerCardDirectDamageInternal(
		InOutRuntime,
		Context,
		TargetUnitId,
		RequestedDamage,
		OutResult,
		nullptr,
		OutError);
}

bool GameXXKCardRules::QueueNextPlayerRoundEnergyPenalty(
	FGameXXKCardBattleRuntime& InOutRuntime,
	const int32 Amount,
	FString* OutError)
{
	if (OutError)
	{
		OutError->Reset();
	}
	FString ValidationError;
	if (!ValidateCardBattleRuntimeInternal(InOutRuntime, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	if (InOutRuntime.Phase != EGameXXKCardBattlePhase::Enemy || Amount <= 0)
	{
		return SetFailure(OutError, TEXT("A next-player-round energy penalty requires a positive enemy-phase amount."));
	}

	FGameXXKCardBattleRuntime NewRuntime = InOutRuntime;
	NewRuntime.PendingNextRoundEnergyPenalty = static_cast<int32>(FMath::Min<int64>(
		99,
		static_cast<int64>(NewRuntime.PendingNextRoundEnergyPenalty) + Amount));
	if (!ValidateCardBattleRuntimeInternal(NewRuntime, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	InOutRuntime = MoveTemp(NewRuntime);
	return true;
}

bool GameXXKCardRules::QueueNextPlayerHandEnergySurcharge(
	FGameXXKCardBattleRuntime& InOutRuntime,
	const int32 SurchargeAmount,
	const FName SourceUnitId,
	FString* OutError)
{
	if (OutError)
	{
		OutError->Reset();
	}
	FString ValidationError;
	if (!ValidateCardBattleRuntimeInternal(InOutRuntime, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	if (InOutRuntime.Phase != EGameXXKCardBattlePhase::Enemy
		|| SurchargeAmount != 1
		|| SourceUnitId.IsNone())
	{
		return SetFailure(OutError, TEXT("A next-player-hand energy surcharge requires one positive point from a stable enemy during the enemy phase."));
	}
	const FGameXXKCardCombatUnit* Source = FindCombatUnitById(InOutRuntime.Units, SourceUnitId);
	if (!Source || !Source->bLiving || Source->Side != EGameXXKCardTargetSide::Enemy)
	{
		return SetFailure(OutError, TEXT("A next-player-hand energy surcharge requires its living enemy source to remain valid."));
	}
	if (InOutRuntime.PendingNextPlayerHandEnergySurcharge != 0)
	{
		// Multiple catalog packets in one enemy phase deliberately collapse to the first saved effect.
		return true;
	}

	FGameXXKCardBattleRuntime NewRuntime = InOutRuntime;
	NewRuntime.PendingNextPlayerHandEnergySurcharge = SurchargeAmount;
	NewRuntime.PendingNextPlayerHandEnergySurchargeSourceUnitId = SourceUnitId;
	if (!ValidateCardBattleRuntimeInternal(NewRuntime, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	InOutRuntime = MoveTemp(NewRuntime);
	return true;
}

bool GameXXKCardRules::ResumeAutomaticResolutionQueue(
	FGameXXKCardBattleRuntime& InOutRuntime,
	TArray<FGameXXKCardPlayResult>& OutResults,
	FString* OutError)
{
	if (OutError)
	{
		OutError->Reset();
	}
	FString ValidationError;
	if (!ValidateCardBattleRuntimeInternal(InOutRuntime, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}

	FGameXXKCardBattleRuntime NewRuntime = InOutRuntime;
	TArray<FGameXXKCardPlayResult> NewResults;
	if (IsActiveChoice(NewRuntime.Deck.PendingChoice.Kind))
	{
		OutResults = MoveTemp(NewResults);
		return true;
	}

	while (true)
	{
		if (!TryStartCompletedSorcererPartnerTaskQueue(NewRuntime, ValidationError)
			|| !TryStartCompletedHeroSpellTaskQueue(NewRuntime, ValidationError))
		{
			return SetFailure(OutError, ValidationError);
		}
		FGameXXKAutomaticResolutionQueue& Queue = NewRuntime.AutomaticResolutionQueue;
		if (!Queue.bActive)
		{
			break;
		}

		while (Queue.NextCardIndex < Queue.PendingCards.Num())
		{
			const FGameXXKResolvedCardSnapshot Snapshot = Queue.PendingCards[Queue.NextCardIndex++];
			const EGameXXKCardResolutionOrigin QueueOrigin = Queue.Origin;
			FGameXXKCardPlayResult Result;
			if (!ResolveCardEffectsFromSnapshot(NewRuntime, Snapshot, QueueOrigin, Result, ValidationError))
			{
				return SetFailure(OutError, ValidationError);
			}
			NewResults.Add(MoveTemp(Result));
			if (IsActiveChoice(NewRuntime.Deck.PendingChoice.Kind))
			{
				if (!ValidateCardBattleRuntimeInternal(NewRuntime, ValidationError))
				{
					return SetFailure(OutError, ValidationError);
				}
				InOutRuntime = MoveTemp(NewRuntime);
				OutResults = MoveTemp(NewResults);
				return true;
			}
		}

		if (Queue.PendingReward != EGameXXKHeroSpellTaskReward::None)
		{
			const EGameXXKHeroSpellTaskReward Reward = Queue.PendingReward;
			const FName RewardOwnerUnitId = Queue.RewardOwnerUnitId;
			FGameXXKCardPlayResult RewardResult;
			if (!ResolveHeroSpellTaskReward(NewRuntime, Reward, RewardOwnerUnitId, RewardResult, ValidationError))
			{
				return SetFailure(OutError, ValidationError);
			}
			NewResults.Add(MoveTemp(RewardResult));
		}
		if (Queue.PendingSorcererReward != EGameXXKSorcererRewardRule::None)
		{
			const EGameXXKSorcererRewardRule Reward = Queue.PendingSorcererReward;
			const FName RewardOwnerUnitId = Queue.RewardOwnerUnitId;
			FGameXXKCardPlayResult RewardResult;
			if (!ResolveSorcererPartnerTaskReward(NewRuntime, Reward, RewardOwnerUnitId, RewardResult, ValidationError))
			{
				return SetFailure(OutError, ValidationError);
			}
			NewResults.Add(MoveTemp(RewardResult));
		}

		const bool bCompletedMageTask = Queue.Origin == EGameXXKCardResolutionOrigin::MageTaskReplay;
		const bool bCompletedSorcererPartnerTask = Queue.Origin == EGameXXKCardResolutionOrigin::PartnerSorcererTaskReplay;
		const FName CompletedRewardOwnerUnitId = Queue.RewardOwnerUnitId;
		Queue = FGameXXKAutomaticResolutionQueue();
		if (bCompletedMageTask)
		{
			NewRuntime.HeroSpellTaskLastCompletedRound = NewRuntime.RoundNumber;
			NewRuntime.HeroSpellTask = FGameXXKHeroSpellTaskRuntime();
		}
		if (bCompletedSorcererPartnerTask)
		{
			FGameXXKSorcererPartnerTaskRuntime* CompletedTask = NewRuntime.SorcererPartnerTasks.FindByPredicate(
				[CompletedRewardOwnerUnitId](const FGameXXKSorcererPartnerTaskRuntime& Candidate)
				{
					return Candidate.OwnerUnitId == CompletedRewardOwnerUnitId;
				});
			if (!CompletedTask)
			{
				return SetFailure(OutError, TEXT("A completed Sorcerer partner queue lost its owner task before reset."));
			}
			CompletedTask->bActive = false;
			CompletedTask->LockedCardIds.Reset();
			CompletedTask->CompletedCardIds.Reset();
			CompletedTask->FirstPlayOrder.Reset();
			CompletedTask->StarterReward = EGameXXKSorcererRewardRule::None;
			CompletedTask->LockedBranch = EGameXXKSorcererTaskBranch::None;
		}
	}
	GameXXKCardRules::RefreshCombatTerminalPhase(NewRuntime);
	if (NewRuntime.Phase == EGameXXKCardBattlePhase::Victory
		|| NewRuntime.Phase == EGameXXKCardBattlePhase::Defeat)
	{
		NewRuntime.PendingBladeCharge = FGameXXKBladeChargeRuntime();
		NewRuntime.PendingBladeDelayedCard = FGameXXKBladeDelayedCardRuntime();
		NewRuntime.PendingBladeFinish = FGameXXKBladeFinishRuntime();
		NewRuntime.PendingBladeNativeStyle = FGameXXKBladeStyleRuntime();
		NewRuntime.PendingBladeResidualStyle = FGameXXKBladeStyleRuntime();
		ClearPoJunBattleState(NewRuntime);
	}
	if (!EvaluateBossPhaseTransitions(NewRuntime, ValidationError)
		|| !ValidateCardBattleRuntimeInternal(NewRuntime, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	InOutRuntime = MoveTemp(NewRuntime);
	OutResults = MoveTemp(NewResults);
	return true;
}

bool GameXXKCardRules::SubmitForcedDiscard(
	FGameXXKCardBattleRuntime& InOutRuntime,
	const TArray<FName>& DiscardedInstanceIds,
	FString* OutError,
	TArray<FGameXXKCardPlayResult>* OutResumedResults)
{
	if (OutError)
	{
		OutError->Reset();
	}
	FString ValidationError;
	if (!ValidateCardBattleRuntimeInternal(InOutRuntime, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	FGameXXKCardBattleRuntime NewRuntime = InOutRuntime;
	if (!GameXXKCardRules::SubmitForcedDiscard(NewRuntime.Deck, DiscardedInstanceIds, &ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	RemoveHandBoundEnergySurchargesOutsideCurrentHand(NewRuntime);
	TArray<FGameXXKCardPlayResult> ResumedResults;
	if (!GameXXKCardRules::ResumeAutomaticResolutionQueue(NewRuntime, ResumedResults, &ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	if (!MaterializePendingTriggeredDraws(NewRuntime, ValidationError)
		|| !ValidateCardBattleRuntimeInternal(NewRuntime, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	InOutRuntime = MoveTemp(NewRuntime);
	if (OutResumedResults)
	{
		*OutResumedResults = MoveTemp(ResumedResults);
	}
	return true;
}

bool GameXXKCardRules::SubmitInsightChoice(
	FGameXXKCardBattleRuntime& InOutRuntime,
	const FName PickedInstanceId,
	const TArray<FName>& ReorderedRemainingInstanceIds,
	FString* OutError,
	TArray<FGameXXKCardPlayResult>* OutResumedResults)
{
	if (OutError)
	{
		OutError->Reset();
	}
	FString ValidationError;
	if (!ValidateCardBattleRuntimeInternal(InOutRuntime, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	FGameXXKCardBattleRuntime NewRuntime = InOutRuntime;
	if (!GameXXKCardRules::SubmitInsightChoice(
		NewRuntime.Deck,
		PickedInstanceId,
		ReorderedRemainingInstanceIds,
		&ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	TArray<FGameXXKCardPlayResult> ResumedResults;
	if (!GameXXKCardRules::ResumeAutomaticResolutionQueue(NewRuntime, ResumedResults, &ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	if (!MaterializePendingTriggeredDraws(NewRuntime, ValidationError)
		|| !ValidateCardBattleRuntimeInternal(NewRuntime, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	InOutRuntime = MoveTemp(NewRuntime);
	if (OutResumedResults)
	{
		*OutResumedResults = MoveTemp(ResumedResults);
	}
	return true;
}

bool GameXXKCardRules::CancelInsight(
	FGameXXKCardBattleRuntime& InOutRuntime,
	FString* OutError,
	TArray<FGameXXKCardPlayResult>* OutResumedResults)
{
	if (OutError)
	{
		OutError->Reset();
	}
	FString ValidationError;
	if (!ValidateCardBattleRuntimeInternal(InOutRuntime, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	FGameXXKCardBattleRuntime NewRuntime = InOutRuntime;
	if (!GameXXKCardRules::CancelInsight(NewRuntime.Deck, &ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	TArray<FGameXXKCardPlayResult> ResumedResults;
	if (!GameXXKCardRules::ResumeAutomaticResolutionQueue(NewRuntime, ResumedResults, &ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	if (!MaterializePendingTriggeredDraws(NewRuntime, ValidationError)
		|| !ValidateCardBattleRuntimeInternal(NewRuntime, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	InOutRuntime = MoveTemp(NewRuntime);
	if (OutResumedResults)
	{
		*OutResumedResults = MoveTemp(ResumedResults);
	}
	return true;
}

bool GameXXKCardRules::SubmitHeroTaskSearchChoice(
	FGameXXKCardBattleRuntime& InOutRuntime,
	const FName PickedInstanceId,
	TArray<FGameXXKCardPlayResult>& OutResumedResults,
	FString* OutError)
{
	OutResumedResults.Reset();
	if (OutError)
	{
		OutError->Reset();
	}
	FString ValidationError;
	if (!ValidateCardBattleRuntimeInternal(InOutRuntime, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	if (InOutRuntime.Deck.PendingChoice.Kind != EGameXXKCardPendingChoiceKind::HeroTaskSearchChooseToHand
		|| PickedInstanceId.IsNone())
	{
		return SetFailure(OutError, TEXT("There is no valid spell-task search choice to submit."));
	}
	const FGameXXKCardInstance* Offered = InOutRuntime.Deck.PendingChoice.Candidates.FindByPredicate(
		[PickedInstanceId](const FGameXXKCardInstance& Candidate)
		{
			return Candidate.InstanceId == PickedInstanceId;
		});
	if (!Offered)
	{
		return SetFailure(OutError, TEXT("The selected spell-task card is not part of the current offer."));
	}
	const FGameXXKHeroSpellTaskRuntime& HeroTask = InOutRuntime.HeroSpellTask;
	const bool bValidHeroTaskCandidate = HeroTask.bActive
		&& Offered->OwnerUnitId == HeroTask.StarterOwnerUnitId
		&& HeroTask.LockedHeroCardIds.Contains(Offered->CardId)
		&& !HeroTask.CompletedHeroCardIds.Contains(Offered->CardId);
	const bool bValidLegacyHeroCandidate = HeroTask.bActive
		&& InOutRuntime.Deck.PendingChoice.bLegacyHeroTaskSearch
		&& InOutRuntime.AutomaticResolutionQueue.bActive
		&& Offered->OwnerUnitId == HeroTask.StarterOwnerUnitId
		&& InOutRuntime.EquippedHeroCardIds.Contains(Offered->CardId);
	const FGameXXKTaskNpcSpellTaskRuntime* TaskNpcTask = InOutRuntime.TaskNpcSpellTasks.FindByPredicate(
		[Offered](const FGameXXKTaskNpcSpellTaskRuntime& Candidate)
		{
			return Candidate.bActive
				&& Offered->OwnerUnitId == Candidate.OwnerUnitId
				&& Candidate.LockedCardIds.Contains(Offered->CardId)
				&& !Candidate.CompletedCardIds.Contains(Offered->CardId);
		});
	const FGameXXKSorcererPartnerTaskRuntime* SorcererTask = InOutRuntime.SorcererPartnerTasks.FindByPredicate(
		[Offered](const FGameXXKSorcererPartnerTaskRuntime& Candidate)
		{
			return Candidate.bActive
				&& !Offered->bTemporary
				&& Offered->OwnerUnitId == Candidate.OwnerUnitId
				&& Candidate.LockedCardIds.Contains(Offered->CardId)
				&& !Candidate.CompletedCardIds.Contains(Offered->CardId);
		});
	if (!bValidHeroTaskCandidate && !bValidLegacyHeroCandidate && !TaskNpcTask && !SorcererTask)
	{
		return SetFailure(OutError, TEXT("The selected card no longer belongs to an active unfinished spell task."));
	}
	bool bApplySorcererManaDiscount = false;
	const FName SorcererSearchOwnerUnitId = SorcererTask ? SorcererTask->OwnerUnitId : NAME_None;
	if (SorcererTask
		&& !ShouldApplySorcererCoreSearchDiscount(
			InOutRuntime,
			SorcererSearchOwnerUnitId,
			bApplySorcererManaDiscount,
			ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}

	FGameXXKCardBattleRuntime NewRuntime = InOutRuntime;
	TArray<FGameXXKCardInstance>* SourceZone = nullptr;
	int32 SourceIndex = NewRuntime.Deck.DrawPile.IndexOfByPredicate([PickedInstanceId](const FGameXXKCardInstance& Instance)
	{
		return Instance.InstanceId == PickedInstanceId;
	});
	if (SourceIndex != INDEX_NONE)
	{
		SourceZone = &NewRuntime.Deck.DrawPile;
	}
	else
	{
		SourceIndex = NewRuntime.Deck.DiscardPile.IndexOfByPredicate([PickedInstanceId](const FGameXXKCardInstance& Instance)
		{
			return Instance.InstanceId == PickedInstanceId;
		});
		if (SourceIndex != INDEX_NONE)
		{
			SourceZone = &NewRuntime.Deck.DiscardPile;
		}
	}
	if (!SourceZone
		|| NewRuntime.Deck.Hand.Num() >= BattleHandCapacity
		|| !IsSameInstance((*SourceZone)[SourceIndex], *Offered))
	{
		return SetFailure(OutError, TEXT("The selected spell-task card is stale or the hand is full."));
	}

	NewRuntime.Deck.Hand.Add(MoveTemp((*SourceZone)[SourceIndex]));
	SourceZone->RemoveAt(SourceIndex, 1, EAllowShrinking::No);
	if (bApplySorcererManaDiscount
		&& !AddHandBoundSorcererManaDiscount(
			NewRuntime,
			NewRuntime.Deck.Hand.Last(),
			SorcererSearchOwnerUnitId,
			ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	ClearPendingChoice(NewRuntime.Deck.PendingChoice);
	TArray<FGameXXKCardPlayResult> ResumedResults;
	if (!ResolveSorcererPartnerStarterAutomaticHand(
			NewRuntime,
			Offered->OwnerUnitId,
			ValidationError)
		|| !GameXXKCardRules::ResumeAutomaticResolutionQueue(NewRuntime, ResumedResults, &ValidationError)
		|| !MaterializePendingTriggeredDraws(NewRuntime, ValidationError)
		|| !ValidateCardBattleRuntimeInternal(NewRuntime, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	InOutRuntime = MoveTemp(NewRuntime);
	OutResumedResults = MoveTemp(ResumedResults);
	return true;
}

bool GameXXKCardRules::BuildCardPlayPreview(
	const FGameXXKCardBattleRuntime& Runtime,
	const FName CardInstanceId,
	FGameXXKCardPlayPreview& OutPreview,
	FString* OutError)
{
	if (OutError)
	{
		OutError->Reset();
	}
	FString ValidationError;
	FGameXXKCardPlayPreview NewPreview;
	if (!BuildCardPlayPreviewInternal(Runtime, CardInstanceId, NewPreview, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	EGameXXKCardZone Zone = EGameXXKCardZone::Invalid;
	const FGameXXKCardInstance* Instance = FindInstance(Runtime.Deck, CardInstanceId, Zone);
	const FGameXXKCardDefinition* BaseDefinition = Instance
		? FGameXXKCardCatalog::FindCardDefinition(Instance->CardId)
		: nullptr;
	if (!Instance || Zone != EGameXXKCardZone::Hand || !BaseDefinition)
	{
		return SetFailure(OutError, TEXT("The requested hand card has no catalog definition."));
	}
	const FGameXXKCardDefinition EffectiveDefinition = FGameXXKCardQualityRules::BuildEffectiveDefinition(
		*BaseDefinition,
		Instance->CurrentQuality);
	if (!ValidateCurrentEffectPlan(EffectiveDefinition, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	OutPreview = MoveTemp(NewPreview);
	return true;
}

bool GameXXKCardRules::ResolveCardPlay(
	FGameXXKCardBattleRuntime& InOutRuntime,
	const FName CardInstanceId,
	const FName SelectedTargetUnitId,
	FGameXXKCardPlayResult& OutResult,
	FString* OutError)
{
	if (OutError)
	{
		OutError->Reset();
	}
	FGameXXKCardPlayPreview Preview;
	FString ValidationError;
	if (!BuildCardPlayPreview(InOutRuntime, CardInstanceId, Preview, &ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}

	TArray<FName> TargetIds;
	FGameXXKCardBattleRuntime NewRuntime = InOutRuntime;
	if (Preview.TargetRequest.bRequiresManualSelection)
	{
		if (!IsManualTargetLegal(Preview.TargetRequest, SelectedTargetUnitId))
		{
			return SetFailure(OutError, TEXT("The submitted stable target is no longer a legal manual card target."));
		}
		TargetIds.Add(SelectedTargetUnitId);
	}
	else
	{
		if (!SelectedTargetUnitId.IsNone())
		{
			return SetFailure(OutError, TEXT("Automatic card targeting cannot accept a submitted stable target ID."));
		}
		if (!ResolveAutomaticTargetIds(Preview.TargetRequest, BuildTargetUnitView(NewRuntime.Units), NewRuntime.Deck.CurrentRandomState, TargetIds, &ValidationError))
		{
			return SetFailure(OutError, ValidationError);
		}
	}

	EGameXXKCardZone Zone = EGameXXKCardZone::Invalid;
	const FGameXXKCardInstance* Instance = FindInstance(NewRuntime.Deck, CardInstanceId, Zone);
	const FGameXXKCardDefinition* BaseDefinition = Instance ? FGameXXKCardCatalog::FindCardDefinition(Instance->CardId) : nullptr;
	FGameXXKCardCombatUnit* Owner = Instance ? FindCombatUnitById(NewRuntime.Units, Instance->OwnerUnitId) : nullptr;
	if (!Instance || Zone != EGameXXKCardZone::Hand || !BaseDefinition || !Owner || !Owner->bLiving)
	{
		return SetFailure(OutError, TEXT("The card, catalog definition, or living owner changed before card play could commit."));
	}
	const FGameXXKCardDefinition QualityEffectiveDefinition = FGameXXKCardQualityRules::BuildEffectiveDefinition(
		*BaseDefinition,
		Instance->CurrentQuality);
	FGameXXKCardDefinition ActiveEffectiveDefinition;
	TArray<FName> AppliedTargetWideningModifierIds;
	if (!BuildWidenedActiveCardDefinition(
		NewRuntime,
		QualityEffectiveDefinition,
		*Instance,
		*Owner,
		ActiveEffectiveDefinition,
		&AppliedTargetWideningModifierIds,
		ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	TArray<FName> AppliedEnergyCostModifierIds;
	TArray<FName> AppliedManaCostModifierIds;
	TArray<FName> TerrainFreeStatusUnitIds;
	TArray<FName> TerrainReductionStatusUnitIds;
	int32 FreshEffectiveEnergyCost = ActiveEffectiveDefinition.EnergyCost;
	if (!BuildEffectiveCardEnergyCost(NewRuntime, ActiveEffectiveDefinition, *Instance, *Owner, FreshEffectiveEnergyCost, &AppliedEnergyCostModifierIds, &TerrainFreeStatusUnitIds, &TerrainReductionStatusUnitIds, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	int32 FreshEffectiveManaCost = ActiveEffectiveDefinition.ManaCost;
	if (!BuildEffectiveCardManaCost(NewRuntime, ActiveEffectiveDefinition, *Instance, *Owner, FreshEffectiveManaCost, &AppliedManaCostModifierIds, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	if (FreshEffectiveEnergyCost != Preview.EffectiveEnergyCost
		|| FreshEffectiveManaCost != Preview.EffectiveManaCost)
	{
		return SetFailure(OutError, TEXT("A card-play modifier changed after the preview was built."));
	}
	if (NewRuntime.Deck.SharedEnergy < Preview.EffectiveEnergyCost || Owner->Mana < Preview.EffectiveManaCost)
	{
		return SetFailure(OutError, TEXT("Card resources changed before card play could commit."));
	}
	NewRuntime.Deck.SharedEnergy -= Preview.EffectiveEnergyCost;
	Owner->Mana -= Preview.EffectiveManaCost;
	const FGameXXKCardInstance CopiedInstance = *Instance;
	if (!MoveResolvedHandCard(NewRuntime.Deck, CardInstanceId, QualityEffectiveDefinition.bExhaustOnPlay, &ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	NewRuntime.Deck.ResolvingCardInstanceId = CopiedInstance.InstanceId;
	TArray<FName> AppliedCostModifierIds = MoveTemp(AppliedEnergyCostModifierIds);
	AppliedCostModifierIds.Append(MoveTemp(AppliedManaCostModifierIds));
	if (!ConsumeOnCardPlayedModifiers(NewRuntime, AppliedCostModifierIds, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	for (const FName ModifierId : AppliedTargetWideningModifierIds)
	{
		if (!ConsumeTriggeredModifierUse(NewRuntime, ModifierId, ValidationError))
		{
			return SetFailure(OutError, ValidationError);
		}
	}
	if (!ConsumeTerrainCardCostStatuses(NewRuntime, TerrainFreeStatusUnitIds, TerrainReductionStatusUnitIds, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}

	FGameXXKCardPlayResult NewResult;
	NewResult.CardInstanceId = CopiedInstance.InstanceId;
	FGameXXKResolvedCardSnapshot ActiveSnapshot;
	ActiveSnapshot.CardId = CopiedInstance.CardId;
	ActiveSnapshot.Quality = CopiedInstance.CurrentQuality;
	ActiveSnapshot.OwnerUnitId = CopiedInstance.OwnerUnitId;
	ActiveSnapshot.OriginalTargetUnitIds = TargetIds;
	const TArray<FGameXXKCardCombatUnit> HealerFormulaUnitsBeforeActiveCard = NewRuntime.Units;
	const int32 PreexistingHealerFormulaCount = NewRuntime.HealerFormulas.Num();
	const bool bTargetedBleedingEnemyAtPlay = TargetIds.ContainsByPredicate([&NewRuntime](const FName TargetId)
	{
		const FGameXXKCardCombatUnit* Target = FindCombatUnitById(NewRuntime.Units, TargetId);
		return Target
			&& Target->bLiving
			&& Target->Side == EGameXXKCardTargetSide::Enemy
			&& GameXXKCardRules::GetCombatStatusStacks(*Target, EGameXXKCardStatus::Bleed) > 0;
	});
	const bool bTargetedVulnerableEnemyAtPlay = TargetIds.ContainsByPredicate([&NewRuntime](const FName TargetId)
	{
		const FGameXXKCardCombatUnit* Target = FindCombatUnitById(NewRuntime.Units, TargetId);
		return Target
			&& Target->bLiving
			&& Target->Side == EGameXXKCardTargetSide::Enemy
			&& GameXXKCardRules::GetCombatStatusStacks(*Target, EGameXXKCardStatus::Vulnerability) > 0;
	});
	TArray<FGameXXKCardCombatUnit> BladeChargeUnitsBeforeActiveCard;
	if (NewRuntime.PendingBladeCharge.Rule == EGameXXKBladeChargeRule::RestoreNextActiveOwnerState
		|| CountEligibleBladeStyleRules(NewRuntime, EGameXXKBladeChargeRule::RestoreNextActiveOwnerState) > 0)
	{
		BladeChargeUnitsBeforeActiveCard = NewRuntime.Units;
	}
	if (!RecordHeroSpellTaskActivePlay(
		NewRuntime,
		QualityEffectiveDefinition,
		ActiveSnapshot,
		ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	if (!RecordTaskNpcSpellTaskActivePlay(
		NewRuntime,
		QualityEffectiveDefinition,
		ActiveSnapshot,
		ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	if (!RecordSorcererPartnerTaskActivePlay(
		NewRuntime,
		QualityEffectiveDefinition,
		CopiedInstance,
		ActiveSnapshot,
		Preview.EffectiveManaCost,
		ValidationError)
		|| !TryStartCompletedSorcererPartnerTaskQueue(NewRuntime, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	const bool bFirstActiveThisRound = NewRuntime.ActiveCardsPlayedThisRound == 0;
	TSet<FName> PreexistingModifierIds;
	for (const FGameXXKCardBattleModifierRuntime& Modifier : NewRuntime.Modifiers)
	{
		PreexistingModifierIds.Add(Modifier.ModifierId);
	}
	if (!ResolveActiveCardTimingModifiers(
		NewRuntime,
		PreexistingModifierIds,
		EGameXXKCardBattleModifierTrigger::BeforeNextActiveCard,
		QualityEffectiveDefinition,
		CopiedInstance,
		ActiveSnapshot,
		NewResult,
		ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	int32 LockedHeavyArrowCharge = 0;
	FName LockedHeavyArrowChargeOwnerUnitId = CopiedInstance.OwnerUnitId;
	int32 HeavyArrowPrimaryBonusPercent = 0;
	if (QualityEffectiveDefinition.HeavyArrow.Kind != EGameXXKHeavyArrowKind::None
		&& QualityEffectiveDefinition.HeavyArrow.LockTiming == EGameXXKHeavyArrowLockTiming::BeforeBaseEffects)
	{
		if (!LockHeavyArrowCharge(
			NewRuntime,
			CopiedInstance.OwnerUnitId,
			QualityEffectiveDefinition.HeavyArrow.ChargeSource,
			NewResult,
			LockedHeavyArrowCharge,
			LockedHeavyArrowChargeOwnerUnitId,
			ValidationError))
		{
			return SetFailure(OutError, ValidationError);
		}
	}
	const FGameXXKHeavyArrowRule& ActiveHeavyArrow = QualityEffectiveDefinition.HeavyArrow;
	const FGameXXKHunterCardRule& ActiveHunterRule = QualityEffectiveDefinition.HunterRule;
	int32 PendingHunterHeavyArrowIgnoreDefense = 0;
	if (ActiveHeavyArrow.Kind != EGameXXKHeavyArrowKind::None)
	{
		if (const int32* PendingIgnore = NewRuntime.PendingHunterHeavyArrowIgnoreDefense.Find(CopiedInstance.OwnerUnitId))
		{
			PendingHunterHeavyArrowIgnoreDefense = *PendingIgnore;
			NewRuntime.PendingHunterHeavyArrowIgnoreDefense.Remove(CopiedInstance.OwnerUnitId);
		}
	}
	const int64 HeavyArrowPrimaryPercentPerCharge =
		(ActiveHeavyArrow.Kind == EGameXXKHeavyArrowKind::AddPrimaryAttackPercentPerCharge
			? ActiveHeavyArrow.MagnitudePerCharge
			: 0)
		+ static_cast<int64>(ActiveHeavyArrow.AdditionalPrimaryAttackPercentPerCharge);
	const int64 PrimaryBonus = HeavyArrowPrimaryPercentPerCharge * LockedHeavyArrowCharge
		+ static_cast<int64>(ActiveHunterRule.PrimaryAttackPercentPerPriorActiveCard)
			* NewRuntime.ActiveCardsPlayedThisRound;
	if (PrimaryBonus > 0)
	{
		if (PrimaryBonus < 0 || PrimaryBonus > MAX_int32)
		{
			return SetFailure(OutError, TEXT("A Heavy Arrow primary bonus exceeds the supported range."));
		}
		HeavyArrowPrimaryBonusPercent = static_cast<int32>(PrimaryBonus);
		NewResult.HeavyArrowPrimaryBonusPercent = HeavyArrowPrimaryBonusPercent;
	}
	const int64 TotalHunterIgnoredDefense = static_cast<int64>(FGameXXKCombatScalingRules::ResolveDotAddition(
		ActiveHeavyArrow.IgnoreDefensePerCharge, CopiedInstance.CurrentQuality, NewRuntime.TeamMaxLevelSnapshot))
		* LockedHeavyArrowCharge + PendingHunterHeavyArrowIgnoreDefense;
	if (TotalHunterIgnoredDefense > 0)
	{
		FGameXXKCardEffect* PrimaryAttack = ActiveEffectiveDefinition.Effects.FindByPredicate([](const FGameXXKCardEffect& Effect)
		{
			return Effect.Type == EGameXXKCardEffectType::DamagePercentAttack;
		});
		if (!PrimaryAttack || TotalHunterIgnoredDefense > MAX_int32)
		{
			return SetFailure(OutError, TEXT("A Heavy Arrow Defense-ignore payload requires one supported primary attack."));
		}
		FGameXXKCardEffect DefenseIgnore;
		DefenseIgnore.Type = EGameXXKCardEffectType::IgnoreDefense;
		DefenseIgnore.Target = PrimaryAttack->Target;
		DefenseIgnore.Source = PrimaryAttack->Source;
		DefenseIgnore.Magnitude = static_cast<int32>(TotalHunterIgnoredDefense);
		DefenseIgnore.HitCount = PrimaryAttack->HitCount;
		ActiveEffectiveDefinition.Effects.Add(MoveTemp(DefenseIgnore));
	}
	if (LockedHeavyArrowCharge > 0 && ActiveHeavyArrow.HealthThresholdPointsPerCharge > 0)
	{
		FGameXXKCardEffect* ThresholdAttachment = ActiveEffectiveDefinition.Effects.FindByPredicate([](const FGameXXKCardEffect& Effect)
		{
			return Effect.Type == EGameXXKCardEffectType::BonusDamagePercent
				&& Effect.Condition.Type == EGameXXKCardEffectConditionType::TargetHealthBelowPercent;
		});
		if (!ThresholdAttachment)
		{
			return SetFailure(OutError, TEXT("A Heavy Arrow threshold payload requires one health-threshold attack attachment."));
		}
		const int64 WidenedThreshold = static_cast<int64>(ThresholdAttachment->Condition.HealthPercentThreshold)
			+ static_cast<int64>(ActiveHeavyArrow.HealthThresholdPointsPerCharge) * LockedHeavyArrowCharge;
		ThresholdAttachment->Condition.HealthPercentThreshold = static_cast<float>(FMath::Min<int64>(100, WidenedThreshold));
	}
	if (!ResolveCardEffectsFromSnapshot(
		NewRuntime,
		ActiveSnapshot,
		EGameXXKCardResolutionOrigin::ActivePlay,
		NewResult,
		ValidationError,
		HeavyArrowPrimaryBonusPercent,
		&ActiveEffectiveDefinition))
	{
		return SetFailure(OutError, ValidationError);
	}
	if (QualityEffectiveDefinition.HeavyArrow.Kind != EGameXXKHeavyArrowKind::None
		&& QualityEffectiveDefinition.HeavyArrow.LockTiming == EGameXXKHeavyArrowLockTiming::AfterBaseEffects
		&& !LockHeavyArrowCharge(
			NewRuntime,
			CopiedInstance.OwnerUnitId,
			QualityEffectiveDefinition.HeavyArrow.ChargeSource,
			NewResult,
			LockedHeavyArrowCharge,
			LockedHeavyArrowChargeOwnerUnitId,
			ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	if (!ResolveHeavyArrowPostLockEffects(
		NewRuntime,
		QualityEffectiveDefinition.HeavyArrow,
		LockedHeavyArrowChargeOwnerUnitId,
		TargetIds,
		LockedHeavyArrowCharge,
		NewResult,
		ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	if (!ResolveShiGuAfterActiveDotApplication(
		NewRuntime,
		HealerFormulaUnitsBeforeActiveCard,
		CopiedInstance.OwnerUnitId,
		NewResult,
		ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	if (!ResolveSorcererPartnerStarterAutomaticHand(
		NewRuntime,
		CopiedInstance.OwnerUnitId,
		ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	const auto AccumulateHunterOneShot = [&](TMap<FName, int32>& PendingByOwner, const int32 Added, const TCHAR* Context) -> bool
	{
		if (Added <= 0)
		{
			return true;
		}
		const int32 Existing = PendingByOwner.FindRef(CopiedInstance.OwnerUnitId);
		if (Existing > MAX_int32 - Added)
		{
			ValidationError = FString::Printf(TEXT("%s exceeds the supported range."), Context);
			return false;
		}
		PendingByOwner.Add(CopiedInstance.OwnerUnitId, Existing + Added);
		return true;
	};
	if (!AccumulateHunterOneShot(
			NewRuntime.PendingHunterHeavyArrowIgnoreDefense,
			FGameXXKCombatScalingRules::ResolveDotAddition(ActiveHunterRule.NextHeavyArrowIgnoreDefense, CopiedInstance.CurrentQuality, NewRuntime.TeamMaxLevelSnapshot),
			TEXT("A pending Hunter Heavy Arrow Defense-ignore payload"))
		|| !AccumulateHunterOneShot(
			NewRuntime.PendingHunterPerfectDodgeCharge,
			ActiveHunterRule.ChargeOnNextPerfectDodge,
			TEXT("A pending Hunter perfect-dodge Charge payload")))
	{
		return SetFailure(OutError, ValidationError);
	}
	if (ActiveHunterRule.PriorActiveCardInterval > 0)
	{
		const int32 CompletedIntervals = NewRuntime.ActiveCardsPlayedThisRound
			/ ActiveHunterRule.PriorActiveCardInterval;
		const int64 IntervalDrawCount = static_cast<int64>(CompletedIntervals)
			* ActiveHunterRule.DrawPerCompletedInterval;
		const int64 IntervalStatusStacks = static_cast<int64>(CompletedIntervals)
			* ActiveHunterRule.StatusStacksPerCompletedInterval;
		if (IntervalDrawCount > MAX_int32 || IntervalStatusStacks > MAX_int32)
		{
			return SetFailure(OutError, TEXT("A Hunter prior-card interval reward exceeds the supported range."));
		}
		if (IntervalDrawCount > 0)
		{
			GameXXKCardRules::RemoveDefeatedPartyOwnerCards(NewRuntime.Deck, NewRuntime.Units);
			if (!GameXXKCardRules::DrawCards(NewRuntime.Deck, static_cast<int32>(IntervalDrawCount), 0, &ValidationError))
			{
				return SetFailure(OutError, ValidationError);
			}
		}
		if (IntervalStatusStacks > 0)
		{
			FGameXXKCardCombatUnit* HunterOwner = FindCombatUnitById(NewRuntime.Units, CopiedInstance.OwnerUnitId);
			if (!HunterOwner || !HunterOwner->bLiving
				|| GameXXKCardRules::AddCombatStatus(
					*HunterOwner,
					ActiveHunterRule.StatusPerCompletedInterval,
					static_cast<int32>(IntervalStatusStacks)) != IntervalStatusStacks)
			{
				return SetFailure(OutError, TEXT("A Hunter prior-card interval status could not be granted exactly."));
			}
		}
	}
	for (const EGameXXKCardBattleModifierTrigger Trigger : {
		EGameXXKCardBattleModifierTrigger::OnNextAttack,
		EGameXXKCardBattleModifierTrigger::FirstActiveAttackAgainstStatusNextPlayerRound,
		EGameXXKCardBattleModifierTrigger::AfterNextActiveCard,
		EGameXXKCardBattleModifierTrigger::AfterEachActiveCard})
	{
		if (!ResolveActiveCardTimingModifiers(
			NewRuntime,
			PreexistingModifierIds,
			Trigger,
			QualityEffectiveDefinition,
			CopiedInstance,
			ActiveSnapshot,
			NewResult,
			ValidationError))
		{
			return SetFailure(OutError, ValidationError);
		}
	}
	if (!ResolveImplementedPartnerBladeBaseAfterActiveCard(
		NewRuntime,
		QualityEffectiveDefinition,
		CopiedInstance,
		Preview.EffectiveEnergyCost,
		Preview.EffectiveManaCost,
		NewResult,
		ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	int32 AdditionalActiveCardCount = 0;
	bool bPreserveFinishCandidate = false;
	int32 StyleAdditionalActiveCardCount = 0;
	bool bStylePreservesFinishCandidate = false;
	if (!ResolveImplementedBladeStylesAfterActiveCard(
		NewRuntime,
		QualityEffectiveDefinition,
		CopiedInstance,
		ActiveSnapshot,
		TargetIds,
		BladeChargeUnitsBeforeActiveCard.IsEmpty() ? nullptr : &BladeChargeUnitsBeforeActiveCard,
		Preview.EffectiveEnergyCost,
		Preview.EffectiveManaCost,
		NewResult,
		StyleAdditionalActiveCardCount,
		bStylePreservesFinishCandidate,
		ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	AdditionalActiveCardCount += StyleAdditionalActiveCardCount;
	bPreserveFinishCandidate |= bStylePreservesFinishCandidate;
	if (bFirstActiveThisRound
		&& !ResolveActiveCardTimingModifiers(
			NewRuntime,
			PreexistingModifierIds,
			EGameXXKCardBattleModifierTrigger::AfterFirstActiveCardNextPlayerRound,
			QualityEffectiveDefinition,
			CopiedInstance,
			ActiveSnapshot,
			NewResult,
			ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	if (bFirstActiveThisRound
		&& QualityEffectiveDefinition.LinkedRole == EGameXXKCharacterRole::Blade
		&& !ResolveBladeSupplementalEffects(
			NewRuntime,
			QualityEffectiveDefinition,
			CopiedInstance,
			TargetIds,
			QualityEffectiveDefinition.ChargeEffects,
			NewResult,
			ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	const FGameXXKBladeChargeRuntime OrdinaryChargeAtConsumption = NewRuntime.PendingBladeCharge;
	int32 OrdinaryAdditionalActiveCardCount = 0;
	bool bOrdinaryPreservesFinishCandidate = false;
	if (!ConsumeImplementedPartnerBladeCharge(
			NewRuntime,
			ActiveSnapshot,
			CopiedInstance,
			BladeChargeUnitsBeforeActiveCard.IsEmpty() ? nullptr : &BladeChargeUnitsBeforeActiveCard,
			Preview.EffectiveEnergyCost,
			Preview.EffectiveManaCost,
			OrdinaryAdditionalActiveCardCount,
			bOrdinaryPreservesFinishCandidate,
			ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	if (OrdinaryAdditionalActiveCardCount < 0
		|| AdditionalActiveCardCount > MAX_int32 - OrdinaryAdditionalActiveCardCount)
	{
		return SetFailure(OutError, TEXT("The combined Blade Charge count exceeds the supported range."));
	}
	AdditionalActiveCardCount += OrdinaryAdditionalActiveCardCount;
	bPreserveFinishCandidate |= bOrdinaryPreservesFinishCandidate;
	if (!ResolvePoJunAfterOrdinaryChargeConsumed(NewRuntime, OrdinaryChargeAtConsumption, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	if (bFirstActiveThisRound)
	{
		ArmImplementedPartnerBladeCharge(NewRuntime, QualityEffectiveDefinition, CopiedInstance);
	}
	const int32 ActiveCardCountIncrement = 1 + AdditionalActiveCardCount;
	if (AdditionalActiveCardCount < 0
		|| NewRuntime.ActiveCardsPlayedThisRound > MAX_int32 - ActiveCardCountIncrement)
	{
		return SetFailure(OutError, TEXT("The active-card counter has exhausted the supported range."));
	}
	NewRuntime.ActiveCardsPlayedThisRound += ActiveCardCountIncrement;
	if (!bPreserveFinishCandidate)
	{
		NewRuntime.LastActiveCard = ActiveSnapshot;
	}
	if (!TryResolveCompletedTaskNpcSpellTasks(NewRuntime, NewResult, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	if (!TryStartCompletedHeroSpellTaskQueue(NewRuntime, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	if (NewRuntime.AutomaticResolutionQueue.bActive)
	{
		NewResult.MaximumAutomaticQueueDepth = FMath::Max(
			NewResult.MaximumAutomaticQueueDepth,
			NewRuntime.AutomaticResolutionQueue.PendingCards.Num()
				- NewRuntime.AutomaticResolutionQueue.NextCardIndex
				+ (NewRuntime.AutomaticResolutionQueue.PendingReward != EGameXXKHeroSpellTaskReward::None ? 1 : 0)
				+ (NewRuntime.AutomaticResolutionQueue.PendingSorcererReward != EGameXXKSorcererRewardRule::None ? 1 : 0));
		TArray<FGameXXKCardPlayResult> AutomaticResults;
		if (!GameXXKCardRules::ResumeAutomaticResolutionQueue(NewRuntime, AutomaticResults, &ValidationError))
		{
			return SetFailure(OutError, ValidationError);
		}
		for (FGameXXKCardPlayResult& AutomaticResult : AutomaticResults)
		{
			NewResult.DamageResults.Append(MoveTemp(AutomaticResult.DamageResults));
			NewResult.StatusChanges.Append(MoveTemp(AutomaticResult.StatusChanges));
			NewResult.HealingResults.Append(MoveTemp(AutomaticResult.HealingResults));
			NewResult.ArmorResults.Append(MoveTemp(AutomaticResult.ArmorResults));
			NewResult.ToxicExplosionDistinctDotTypeCounts.Append(MoveTemp(AutomaticResult.ToxicExplosionDistinctDotTypeCounts));
			NewResult.bOpenedPendingChoice |= AutomaticResult.bOpenedPendingChoice;
			NewResult.AutomaticResolutionCount += AutomaticResult.AutomaticResolutionCount;
		}
		NewResult.AutomaticResolutionCount += AutomaticResults.Num();
	}
	if (!TryResolveImplementedPartnerBladeFinish(
		NewRuntime,
		QualityEffectiveDefinition,
		CopiedInstance,
		ActiveSnapshot,
		bTargetedBleedingEnemyAtPlay,
		bTargetedVulnerableEnemyAtPlay,
		Preview.EffectiveEnergyCost,
		NewResult,
		ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	if (NewRuntime.AutomaticResolutionQueue.bActive)
	{
		NewResult.MaximumAutomaticQueueDepth = FMath::Max(
			NewResult.MaximumAutomaticQueueDepth,
			NewRuntime.AutomaticResolutionQueue.PendingCards.Num()
				- NewRuntime.AutomaticResolutionQueue.NextCardIndex
				+ (NewRuntime.AutomaticResolutionQueue.PendingReward != EGameXXKHeroSpellTaskReward::None ? 1 : 0)
				+ (NewRuntime.AutomaticResolutionQueue.PendingSorcererReward != EGameXXKSorcererRewardRule::None ? 1 : 0));
		TArray<FGameXXKCardPlayResult> AutomaticResults;
		if (!GameXXKCardRules::ResumeAutomaticResolutionQueue(NewRuntime, AutomaticResults, &ValidationError))
		{
			return SetFailure(OutError, ValidationError);
		}
		for (FGameXXKCardPlayResult& AutomaticResult : AutomaticResults)
		{
			NewResult.DamageResults.Append(MoveTemp(AutomaticResult.DamageResults));
			NewResult.StatusChanges.Append(MoveTemp(AutomaticResult.StatusChanges));
			NewResult.HealingResults.Append(MoveTemp(AutomaticResult.HealingResults));
			NewResult.ArmorResults.Append(MoveTemp(AutomaticResult.ArmorResults));
			NewResult.ToxicExplosionDistinctDotTypeCounts.Append(MoveTemp(AutomaticResult.ToxicExplosionDistinctDotTypeCounts));
			NewResult.bOpenedPendingChoice |= AutomaticResult.bOpenedPendingChoice;
			NewResult.AutomaticResolutionCount += AutomaticResult.AutomaticResolutionCount;
		}
		NewResult.AutomaticResolutionCount += AutomaticResults.Num();
	}
	if (!QueuePoJunOpeningReplaysAfterActiveCard(
		NewRuntime,
		bFirstActiveThisRound,
		ActiveSnapshot,
		ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	if (NewRuntime.AutomaticResolutionQueue.bActive)
	{
		NewResult.MaximumAutomaticQueueDepth = FMath::Max(
			NewResult.MaximumAutomaticQueueDepth,
			NewRuntime.AutomaticResolutionQueue.PendingCards.Num()
				- NewRuntime.AutomaticResolutionQueue.NextCardIndex
				+ (NewRuntime.AutomaticResolutionQueue.PendingReward != EGameXXKHeroSpellTaskReward::None ? 1 : 0)
				+ (NewRuntime.AutomaticResolutionQueue.PendingSorcererReward != EGameXXKSorcererRewardRule::None ? 1 : 0));
		TArray<FGameXXKCardPlayResult> AutomaticResults;
		if (!GameXXKCardRules::ResumeAutomaticResolutionQueue(NewRuntime, AutomaticResults, &ValidationError))
		{
			return SetFailure(OutError, ValidationError);
		}
		for (FGameXXKCardPlayResult& AutomaticResult : AutomaticResults)
		{
			NewResult.DamageResults.Append(MoveTemp(AutomaticResult.DamageResults));
			NewResult.StatusChanges.Append(MoveTemp(AutomaticResult.StatusChanges));
			NewResult.HealingResults.Append(MoveTemp(AutomaticResult.HealingResults));
			NewResult.ArmorResults.Append(MoveTemp(AutomaticResult.ArmorResults));
			NewResult.ToxicExplosionDistinctDotTypeCounts.Append(MoveTemp(AutomaticResult.ToxicExplosionDistinctDotTypeCounts));
			NewResult.bOpenedPendingChoice |= AutomaticResult.bOpenedPendingChoice;
			NewResult.AutomaticResolutionCount += AutomaticResult.AutomaticResolutionCount;
		}
		NewResult.AutomaticResolutionCount += AutomaticResults.Num();
	}
	if (!ResolveZhuiFengAfterActiveCard(
			NewRuntime,
			ActiveCardCountIncrement,
			NewResult,
			ValidationError)
		|| !ResolveQingNangAfterPaidHighCostActive(
			NewRuntime,
			Preview.EffectiveEnergyCost,
			NewResult,
			ValidationError)
		|| !ResolveOpenedHealerFormulasAfterActiveCard(
			NewRuntime,
			HealerFormulaUnitsBeforeActiveCard,
			PreexistingHealerFormulaCount,
			Preview.EffectiveEnergyCost,
			ActiveSnapshot,
			NewResult,
			ValidationError)
		|| !InstallHealerFormulaAfterActivePlay(
			NewRuntime,
			QualityEffectiveDefinition,
			CopiedInstance.OwnerUnitId,
			ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	if (!IsActiveChoice(NewRuntime.Deck.PendingChoice.Kind)
		&& !NewRuntime.AutomaticResolutionQueue.bActive)
	{
		GameXXKCardRules::RefreshCombatTerminalPhase(NewRuntime);
		if (NewRuntime.Phase == EGameXXKCardBattlePhase::Victory
			|| NewRuntime.Phase == EGameXXKCardBattlePhase::Defeat)
		{
			NewRuntime.PendingBladeCharge = FGameXXKBladeChargeRuntime();
			NewRuntime.PendingBladeDelayedCard = FGameXXKBladeDelayedCardRuntime();
			NewRuntime.PendingBladeFinish = FGameXXKBladeFinishRuntime();
			NewRuntime.PendingBladeNativeStyle = FGameXXKBladeStyleRuntime();
			NewRuntime.PendingBladeResidualStyle = FGameXXKBladeStyleRuntime();
			NewRuntime.BladeRetainedHandCardInstanceIds.Reset();
			ClearPoJunBattleState(NewRuntime);
		}
		if (!EvaluateBossPhaseTransitions(NewRuntime, ValidationError))
		{
			return SetFailure(OutError, ValidationError);
		}
	}
	if (NewRuntime.Deck.ResolvingCardInstanceId != CopiedInstance.InstanceId)
	{
		return SetFailure(OutError, TEXT("The resolving-card guard changed before the active card transaction completed."));
	}
	NewRuntime.Deck.ResolvingCardInstanceId = NAME_None;
	if (!ValidateCardBattleRuntimeInternal(NewRuntime, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	InOutRuntime = MoveTemp(NewRuntime);
	OutResult = MoveTemp(NewResult);
	return true;
}

bool GameXXKCardRules::NotifyTerrainChanged(
	FGameXXKCardBattleRuntime& InOutRuntime,
	const EGameXXKCardTerrain NewTerrain,
	FString* OutError)
{
	if (OutError)
	{
		OutError->Reset();
	}
	FString ValidationError;
	if (!ValidateCardBattleRuntimeInternal(InOutRuntime, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	if (!IsConcreteTerrain(NewTerrain))
	{
		return SetFailure(OutError, TEXT("A terrain change requires a concrete destination terrain."));
	}
	if (InOutRuntime.Terrain == NewTerrain)
	{
		return SetFailure(OutError, TEXT("A terrain change must select a different terrain."));
	}

	FGameXXKCardBattleRuntime NewRuntime = InOutRuntime;
	NewRuntime.Terrain = NewTerrain;
	NewRuntime.bTerrainChangedThisRound = true;
	if (!ValidateCardBattleRuntimeInternal(NewRuntime, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	InOutRuntime = MoveTemp(NewRuntime);
	return true;
}

bool GameXXKCardRules::EndPlayerCardPhase(
	FGameXXKCardBattleRuntime& InOutRuntime,
	TArray<FGameXXKCardDamageResult>& OutEndPhaseDamageResults,
	FString* OutError)
{
	if (OutError)
	{
		OutError->Reset();
	}
	FString ValidationError;
	if (!ValidateCardBattleRuntimeInternal(InOutRuntime, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	if (InOutRuntime.Phase != EGameXXKCardBattlePhase::Player)
	{
		return SetFailure(OutError, TEXT("Only an active player card phase can be ended."));
	}
	if (!RequireNoPendingChoice(InOutRuntime.Deck, &ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}

	FGameXXKCardBattleRuntime NewRuntime = InOutRuntime;
	TArray<FGameXXKCardDamageResult> NewEndPhaseDamageResults;
	TOptional<FGameXXKCardDefinition> DeferredBladeFinishDefinition;
	FGameXXKResolvedCardSnapshot DeferredBladeFinishSource;
	ExpireUntriggeredNextPlayerRoundModifiers(NewRuntime);
	ExpirePoJunNextRoundStateAtPlayerPhaseEnd(NewRuntime);
	NewRuntime.PendingBladeCharge = FGameXXKBladeChargeRuntime();
	NewRuntime.PendingBladeFinish = FGameXXKBladeFinishRuntime();
	NewRuntime.PendingBladeNativeStyle = FGameXXKBladeStyleRuntime();
	NewRuntime.PendingBladeResidualStyle = FGameXXKBladeStyleRuntime();
	if (NewRuntime.ActiveCardsPlayedThisRound > 0 && !NewRuntime.LastActiveCard.CardId.IsNone())
	{
		const FGameXXKCardDefinition* BaseDefinition = FGameXXKCardCatalog::FindCardDefinition(NewRuntime.LastActiveCard.CardId);
		if (!BaseDefinition || !IsConcreteCardQuality(NewRuntime.LastActiveCard.Quality))
		{
			return SetFailure(OutError, TEXT("The last active card cannot resolve its Blade Finish definition."));
		}
		const FGameXXKCardDefinition EffectiveDefinition = FGameXXKCardQualityRules::BuildEffectiveDefinition(
			*BaseDefinition,
			NewRuntime.LastActiveCard.Quality);
		if (EffectiveDefinition.LinkedRole == EGameXXKCharacterRole::Blade)
		{
			const FName FinishInstanceId(*FString::Printf(
				TEXT("Finish.%d.%s.%s"),
				NewRuntime.RoundNumber,
				*NewRuntime.LastActiveCard.OwnerUnitId.ToString(),
				*NewRuntime.LastActiveCard.CardId.ToString()));
			const FGameXXKCardInstance FinishInstance = MakeSnapshotInstance(NewRuntime.LastActiveCard, FinishInstanceId);
			FGameXXKCardPlayResult FinishResult;
			if (!ResolveBladeSupplementalEffects(
				NewRuntime,
				EffectiveDefinition,
				FinishInstance,
				NewRuntime.LastActiveCard.OriginalTargetUnitIds,
				EffectiveDefinition.FinishEffects,
				FinishResult,
				ValidationError))
			{
				return SetFailure(OutError, ValidationError);
			}
			NewEndPhaseDamageResults.Append(MoveTemp(FinishResult.DamageResults));
		}
		// Finish/native-style/PoJun payloads target the next player round. Keep
		// their source until this transaction has completed player-side DoT and
		// formally entered Enemy phase; strict runtime validation intentionally
		// rejects a next-round payload while the phase is still Player.
		DeferredBladeFinishDefinition = EffectiveDefinition;
		DeferredBladeFinishSource = NewRuntime.LastActiveCard;
	}
	NewRuntime.LastActiveCard = FGameXXKResolvedCardSnapshot();
	ExpireTemporaryCardsAtPlayerRoundEnd(NewRuntime.Deck, NewRuntime.RoundNumber);
	const bool bHandDiscarded = NewRuntime.BladeRetainedHandCardInstanceIds.IsEmpty()
		? DiscardRemainingHand(NewRuntime.Deck, ValidationError)
		: DiscardRemainingHandExcept(NewRuntime.Deck, NewRuntime.BladeRetainedHandCardInstanceIds, ValidationError);
	if (!bHandDiscarded)
	{
		return SetFailure(OutError, ValidationError);
	}
	NewRuntime.BladeRetainedHandCardInstanceIds.Reset();
	RemoveHandBoundEnergySurchargesOutsideCurrentHand(NewRuntime);
	const TArray<FGameXXKCardCombatUnit> HealerFormulaUnitsBeforePlayerDot = NewRuntime.Units;
	const int32 FirstPlayerDotResultIndex = NewEndPhaseDamageResults.Num();
	if (!ApplyEndPhaseDotForSide(NewRuntime, EGameXXKCardTargetSide::Party, NewEndPhaseDamageResults, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	if (!ResolveOpenedHealerFormulasAfterDamageEvents(
		NewRuntime,
		HealerFormulaUnitsBeforePlayerDot,
		NewEndPhaseDamageResults,
		FirstPlayerDotResultIndex,
		ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	GameXXKCardRules::RefreshCombatTerminalPhase(NewRuntime);
	if (!EvaluateBossPhaseTransitions(NewRuntime, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	if (NewRuntime.Phase == EGameXXKCardBattlePhase::Victory
		|| NewRuntime.Phase == EGameXXKCardBattlePhase::Defeat)
	{
		NewRuntime.PendingBladeDelayedCard = FGameXXKBladeDelayedCardRuntime();
		NewRuntime.PendingBladeFinish = FGameXXKBladeFinishRuntime();
		NewRuntime.PendingBladeNativeStyle = FGameXXKBladeStyleRuntime();
		NewRuntime.PendingBladeResidualStyle = FGameXXKBladeStyleRuntime();
		ClearPoJunBattleState(NewRuntime);
	}
	if (NewRuntime.Phase == EGameXXKCardBattlePhase::Player)
	{
		// Enemies begin their own phase after player-side DoT resolves, so only their armor expires here.
		if (!ClearArmorAtSidePhaseStart(NewRuntime, EGameXXKCardTargetSide::Enemy, ValidationError))
		{
			return SetFailure(OutError, ValidationError);
		}
		NewRuntime.Phase = EGameXXKCardBattlePhase::Enemy;
	}
	if (NewRuntime.Phase == EGameXXKCardBattlePhase::Enemy
		&& DeferredBladeFinishDefinition.IsSet())
	{
		ArmImplementedPartnerBladeFinish(
			NewRuntime,
			DeferredBladeFinishDefinition.GetValue(),
			DeferredBladeFinishSource);
		if (!ResolvePoJunAfterSuccessfulBladeFinish(
			NewRuntime,
			DeferredBladeFinishDefinition.GetValue(),
			DeferredBladeFinishSource,
			ValidationError))
		{
			return SetFailure(OutError, ValidationError);
		}
	}
	ResetPoJunCurrentRoundProgress(NewRuntime);
	if (!ValidateCardBattleRuntimeInternal(NewRuntime, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	InOutRuntime = MoveTemp(NewRuntime);
	OutEndPhaseDamageResults = MoveTemp(NewEndPhaseDamageResults);
	return true;
}

bool GameXXKCardRules::ResolveEnemyDirectAttack(
	FGameXXKCardBattleRuntime& InOutRuntime,
	const FGameXXKCardDamageContext& Context,
	const FName SelectedPartyTargetUnitId,
	const int32 RequestedDamage,
	FGameXXKCardDamageResult& OutResult,
	TArray<FGameXXKCardDamageResult>* OutReactiveDamageResults,
	FString* OutError,
	const bool bDeferTerminalPhase)
{
	if (OutError)
	{
		OutError->Reset();
	}
	FString ValidationError;
	if (!ValidateCardBattleRuntimeInternal(InOutRuntime, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	if (InOutRuntime.Phase != EGameXXKCardBattlePhase::Enemy)
	{
		return SetFailure(OutError, TEXT("Enemy direct attacks can only resolve during the enemy phase."));
	}
	if ((Context.Kind != EGameXXKCardDamageKind::SingleTargetAttack && Context.Kind != EGameXXKCardDamageKind::GroupAttack)
		|| Context.SourceUnitId.IsNone() || SelectedPartyTargetUnitId.IsNone() || RequestedDamage <= 0)
	{
		return SetFailure(OutError, TEXT("Enemy direct attacks require a concrete direct-attack context, source, party target, and positive damage."));
	}
	const FGameXXKCardCombatUnit* Enemy = FindCombatUnitById(InOutRuntime.Units, Context.SourceUnitId);
	const FGameXXKCardCombatUnit* SelectedTarget = FindCombatUnitById(InOutRuntime.Units, SelectedPartyTargetUnitId);
	if (!Enemy || !Enemy->bLiving || Enemy->Side != EGameXXKCardTargetSide::Enemy
		|| !SelectedTarget || !SelectedTarget->bLiving || SelectedTarget->Side != EGameXXKCardTargetSide::Party)
	{
		return SetFailure(OutError, TEXT("Enemy direct attacks require one living enemy source and one living party target."));
	}

	FGameXXKCardBattleRuntime NewRuntime = InOutRuntime;
	const TArray<FGameXXKCardCombatUnit> HealerFormulaUnitsBeforeEnemyDamage = NewRuntime.Units;
	FGameXXKCardDamageContext ResolvedContext = Context;
	ResolvedContext.AgilityRollPercent = AdvanceCombatRandomRoll(NewRuntime);
	FName AppliedTargetUnitId = SelectedPartyTargetUnitId;
	bool bRedirectedByCard = false;
	if (!ApplySingleTargetEnemyRedirect(NewRuntime, ResolvedContext, SelectedPartyTargetUnitId, AppliedTargetUnitId, bRedirectedByCard, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	if (!PrepareImplementedBladeFinishForEnemyCard(NewRuntime, ResolvedContext, AppliedTargetUnitId, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	const int32 DifficultyScaledDamage = FGameXXKCombatScalingRules::ScaleByPercentCeil(
		RequestedDamage,
		NewRuntime.EnemyDifficultyDamagePercent);
	FGameXXKCardDamageResult NewResult;
	if (!ApplyCombatDirectDamageInternal(
		NewRuntime.Units,
		NewRuntime.GuardLinks,
		ResolvedContext,
		AppliedTargetUnitId,
		DifficultyScaledDamage,
		NewResult,
		nullptr,
		&NewRuntime,
		nullptr,
		false,
		&ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	FGameXXKBladeFinishRuntime& DodgeFinish = NewRuntime.PendingBladeFinish;
	if (DodgeFinish.Rule == EGameXXKBladeFinishRule::FirstTwoDodgesFree
		&& DodgeFinish.TriggerPlayerRound == NewRuntime.RoundNumber + 1
		&& DodgeFinish.RemainingTriggers > 0
		&& AppliedTargetUnitId == DodgeFinish.SourceOwnerUnitId
		&& NewResult.bAvoidedByAgility
		&& NewResult.AgilityStacksConsumed > 0)
	{
		FGameXXKCardCombatUnit* DodgeOwner = FindCombatUnitById(NewRuntime.Units, AppliedTargetUnitId);
		if (!DodgeOwner
			|| GameXXKCardRules::AddCombatStatus(
				*DodgeOwner,
				EGameXXKCardStatus::Agility,
				NewResult.AgilityStacksConsumed) != NewResult.AgilityStacksConsumed)
		{
			return SetFailure(OutError, TEXT("Zhu Ying Finish could not restore the Agility consumed by its protected dodge."));
		}
		NewResult.AgilityStacksConsumed = 0;
		if (--DodgeFinish.RemainingTriggers == 0)
		{
			DodgeFinish = FGameXXKBladeFinishRuntime();
		}
	}
	if (NewResult.bPerfectAgilityDodge)
	{
		if (const int32* PendingCharge = NewRuntime.PendingHunterPerfectDodgeCharge.Find(AppliedTargetUnitId))
		{
			FGameXXKCardCombatUnit* DodgeOwner = FindCombatUnitById(NewRuntime.Units, AppliedTargetUnitId);
			if (!DodgeOwner || !DodgeOwner->bLiving
				|| GameXXKCardRules::AddCombatStatus(*DodgeOwner, EGameXXKCardStatus::Charge, *PendingCharge) != *PendingCharge)
			{
				return SetFailure(OutError, TEXT("A Hunter perfect-dodge payload could not grant its exact Charge."));
			}
			NewRuntime.PendingHunterPerfectDodgeCharge.Remove(AppliedTargetUnitId);
		}
	}
	NewResult.OriginalTargetUnitId = SelectedPartyTargetUnitId;
	NewResult.bRedirected |= bRedirectedByCard;
	UE_LOG(LogTemp, Verbose, TEXT("[EnemyAtk] source=%s requested=%d scaled=%d target=%s redirected=%d before=%d dmg=%d after=%d"),
		*ResolvedContext.SourceUnitId.ToString(),
		RequestedDamage,
		DifficultyScaledDamage,
		*AppliedTargetUnitId.ToString(),
		bRedirectedByCard,
		NewResult.TargetHealthBefore,
		NewResult.HealthDamage,
		NewResult.TargetHealthAfter);
	TArray<FGameXXKCardDamageResult> NewReactiveDamageResults;
	if (!ResolveFirstDirectDamageReactiveModifiers(NewRuntime, ResolvedContext, NewResult, &NewReactiveDamageResults, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	TArray<FGameXXKCardDamageResult> HealerFormulaDamageResults;
	HealerFormulaDamageResults.Reserve(1 + NewReactiveDamageResults.Num());
	HealerFormulaDamageResults.Add(NewResult);
	HealerFormulaDamageResults.Append(NewReactiveDamageResults);
	if (!ResolveOpenedHealerFormulasAfterDamageEvents(
		NewRuntime,
		HealerFormulaUnitsBeforeEnemyDamage,
		HealerFormulaDamageResults,
		0,
		ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	if (!bDeferTerminalPhase)
	{
		GameXXKCardRules::RefreshCombatTerminalPhase(NewRuntime);
		if (NewRuntime.Phase == EGameXXKCardBattlePhase::Victory
			|| NewRuntime.Phase == EGameXXKCardBattlePhase::Defeat)
		{
			NewRuntime.PendingBladeCharge = FGameXXKBladeChargeRuntime();
			NewRuntime.PendingBladeDelayedCard = FGameXXKBladeDelayedCardRuntime();
			NewRuntime.PendingBladeFinish = FGameXXKBladeFinishRuntime();
			NewRuntime.PendingBladeNativeStyle = FGameXXKBladeStyleRuntime();
			NewRuntime.PendingBladeResidualStyle = FGameXXKBladeStyleRuntime();
			NewRuntime.BladeRetainedHandCardInstanceIds.Reset();
			ClearPoJunBattleState(NewRuntime);
		}
	}
	if (!EvaluateBossPhaseTransitions(NewRuntime, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	if (!ValidateCardBattleRuntimeInternal(NewRuntime, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	InOutRuntime = MoveTemp(NewRuntime);
	OutResult = MoveTemp(NewResult);
	if (OutReactiveDamageResults)
	{
		*OutReactiveDamageResults = MoveTemp(NewReactiveDamageResults);
	}
	return true;
}

bool GameXXKCardRules::ResolvePartyReactionsAfterEnemyCard(
	FGameXXKCardBattleRuntime& InOutRuntime,
	const FName EnemySourceUnitId,
	const EGameXXKCardDamageKind CompletedCardKind,
	const FName FinalRecipientUnitId,
	TArray<FGameXXKCardDamageResult>& OutReactionDamageResults,
	FString* OutError)
{
	if (OutError)
	{
		OutError->Reset();
	}
	OutReactionDamageResults.Reset();
	FString ValidationError;
	if (!ValidateCardBattleRuntimeInternal(InOutRuntime, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	if (InOutRuntime.Phase != EGameXXKCardBattlePhase::Enemy)
	{
		return SetFailure(OutError, TEXT("Party reactions can only resolve at an enemy-card boundary."));
	}

	FGameXXKCardBattleRuntime NewRuntime = InOutRuntime;
	TArray<FGameXXKCardDamageResult> NewResults;
	if (CompletedCardKind == EGameXXKCardDamageKind::SingleTargetAttack
		&& !EnemySourceUnitId.IsNone() && !FinalRecipientUnitId.IsNone())
	{
		const FGameXXKCardCombatUnit* Recipient = FindCombatUnitById(NewRuntime.Units, FinalRecipientUnitId);
		if (!Recipient || Recipient->Side != EGameXXKCardTargetSide::Party)
		{
			return SetFailure(OutError, TEXT("A single-target party reaction boundary requires its final party recipient."));
		}

		TArray<FGameXXKReactionRuntime> TriggeredReactions;
		TSet<FString> TriggeredRegistrationGroups;
		for (const FGameXXKReactionRuntime& Reaction : NewRuntime.Reactions)
		{
			if (Reaction.RecipientUnitId == FinalRecipientUnitId && Reaction.RemainingTriggers > 0)
			{
				const FString TriggerGroupKey = BuildPartyReactionTriggerGroupKey(Reaction);
				if (!TriggeredRegistrationGroups.Contains(TriggerGroupKey))
				{
					TriggeredRegistrationGroups.Add(TriggerGroupKey);
					TriggeredReactions.Add(Reaction);
				}
			}
		}
		TSet<FName> ConsumedReactionIds;
		const bool bPreserveFirstTriggeredReaction = NewRuntime.PendingPreservedPartyReactionUses > 0
			&& !TriggeredReactions.IsEmpty();
		for (int32 TriggeredIndex = bPreserveFirstTriggeredReaction ? 1 : 0;
			TriggeredIndex < TriggeredReactions.Num();
			++TriggeredIndex)
		{
			ConsumedReactionIds.Add(TriggeredReactions[TriggeredIndex].ReactionId);
		}
		NewRuntime.Reactions.RemoveAll([&ConsumedReactionIds](const FGameXXKReactionRuntime& Reaction)
		{
			return ConsumedReactionIds.Contains(Reaction.ReactionId);
		});
		if (bPreserveFirstTriggeredReaction)
		{
			--NewRuntime.PendingPreservedPartyReactionUses;
		}
		SyncPartyReactionStatuses(NewRuntime);

		const FGameXXKBladeFinishRuntime FinishAtVolleyStart = NewRuntime.PendingBladeFinish;
		int32 BladeCounterSourceCount = 0;
		for (const FGameXXKReactionRuntime& Reaction : TriggeredReactions)
		{
			if (Reaction.Status == EGameXXKCardStatus::Counter
				&& Reaction.RecipientUnitId == FinishAtVolleyStart.SourceOwnerUnitId)
			{
				++BladeCounterSourceCount;
			}
		}
		const bool bBladeCounterVolley = BladeCounterSourceCount > 0;
		const bool bGroupBladeCounterVolley = bBladeCounterVolley
			&& FinishAtVolleyStart.Rule == EGameXXKBladeFinishRule::FirstCounterVolleyHitsAll
			&& FinishAtVolleyStart.RemainingTriggers > 0;
		const TArray<FName> GroupCounterTargetIds = bGroupBladeCounterVolley
			? CollectLivingUnitIdsForSide(NewRuntime, EGameXXKCardTargetSide::Enemy)
			: TArray<FName>();

		if (bBladeCounterVolley
			&& FinishAtVolleyStart.Rule == EGameXXKBladeFinishRule::TransferMarkBeforeCounter)
		{
			FGameXXKCardCombatUnit* FinishOwner = FindCombatUnitById(NewRuntime.Units, FinishAtVolleyStart.SourceOwnerUnitId);
			FGameXXKCardCombatUnit* AttackingEnemy = FindCombatUnitById(NewRuntime.Units, EnemySourceUnitId);
			if (!FinishOwner || FinishOwner->Side != EGameXXKCardTargetSide::Party
				|| !AttackingEnemy || !AttackingEnemy->bLiving || AttackingEnemy->Side != EGameXXKCardTargetSide::Enemy)
			{
				return SetFailure(OutError, TEXT("Po Lang Finish lost its party owner or living enemy before transferring Mark."));
			}
			const int32 TransferLimit = FMath::Min(
				BladeCounterSourceCount,
				GameXXKCardRules::GetCombatStatusStacks(*FinishOwner, EGameXXKCardStatus::Mark));
			const int32 Transferred = GameXXKCardRules::AddCombatStatus(
				*AttackingEnemy,
				EGameXXKCardStatus::Mark,
				TransferLimit);
			if (GameXXKCardRules::ConsumeCombatStatus(*FinishOwner, EGameXXKCardStatus::Mark, Transferred) != Transferred)
			{
				return SetFailure(OutError, TEXT("Po Lang Finish could not remove the exact Mark amount transferred to the attacker."));
			}
		}

		for (const FGameXXKReactionRuntime& Reaction : TriggeredReactions)
		{
			const FGameXXKCardCombatUnit* ReactionSource = FindCombatUnitById(NewRuntime.Units, Reaction.RecipientUnitId);
			const bool bGroupThisCounter = bGroupBladeCounterVolley
				&& Reaction.Status == EGameXXKCardStatus::Counter
				&& Reaction.RecipientUnitId == FinishAtVolleyStart.SourceOwnerUnitId;
			const FGameXXKCardCombatUnit* Enemy = FindCombatUnitById(NewRuntime.Units, EnemySourceUnitId);
			if (!ReactionSource || ReactionSource->Side != EGameXXKCardTargetSide::Party
				|| (!bGroupThisCounter && (!Enemy || !Enemy->bLiving || Enemy->Side != EGameXXKCardTargetSide::Enemy)))
			{
				continue;
			}
			const int64 RequestedDamage = Reaction.Status == EGameXXKCardStatus::Block
				? static_cast<int64>(ReactionSource->Attack) + ReactionSource->Armor
				: static_cast<int64>(ReactionSource->Attack);
			if (RequestedDamage <= 0 || RequestedDamage > MAX_int32)
			{
				return SetFailure(OutError, TEXT("A queued Counter or Block source produced an unsupported damage amount."));
			}
			const FName ReactionSourceUnitId = ReactionSource->UnitId;

			const TArray<FName> ReactionTargetIds = bGroupThisCounter
				? GroupCounterTargetIds
				: TArray<FName>{EnemySourceUnitId};
			for (const FName ReactionTargetId : ReactionTargetIds)
			{
				const FGameXXKCardCombatUnit* CurrentTarget = FindCombatUnitById(NewRuntime.Units, ReactionTargetId);
				if (!CurrentTarget || !CurrentTarget->bLiving || CurrentTarget->Side != EGameXXKCardTargetSide::Enemy)
				{
					continue;
				}
				FGameXXKCardDamageContext ReactionContext;
				ReactionContext.SourceUnitId = ReactionSourceUnitId;
				ReactionContext.Kind = bGroupThisCounter
					? EGameXXKCardDamageKind::GroupAttack
					: EGameXXKCardDamageKind::SingleTargetAttack;
				ReactionContext.ResolutionOrigin = EGameXXKCardResolutionOrigin::Reaction;
				FGameXXKCardDamageResult ReactionResult;
				if (!ApplyCombatDirectDamageInternal(
					NewRuntime.Units,
					NewRuntime.GuardLinks,
					ReactionContext,
					ReactionTargetId,
					static_cast<int32>(RequestedDamage),
					ReactionResult,
					nullptr,
					nullptr,
					nullptr,
					true,
					&ValidationError))
				{
					return SetFailure(OutError, ValidationError);
				}
				ReactionResult.Cause = Reaction.Status == EGameXXKCardStatus::Block
					? EGameXXKCardDamageCause::Block
					: EGameXXKCardDamageCause::Counter;
				NewResults.Add(MoveTemp(ReactionResult));
			}
		}

		if (bBladeCounterVolley
			&& FinishAtVolleyStart.Rule == EGameXXKBladeFinishRule::MarkAndReregisterCounterVolley
			&& FinishAtVolleyStart.RemainingTriggers > 0)
		{
			for (const FGameXXKReactionRuntime& Reaction : TriggeredReactions)
			{
				if (Reaction.Status != EGameXXKCardStatus::Counter
					|| Reaction.RecipientUnitId != FinishAtVolleyStart.SourceOwnerUnitId)
				{
					continue;
				}
				const FGameXXKCardCombatUnit* Grantor = FindCombatUnitById(NewRuntime.Units, Reaction.GrantedByUnitId);
				if (!Grantor || !Grantor->bLiving || Grantor->Side != EGameXXKCardTargetSide::Party)
				{
					continue;
				}
				FGameXXKCardInstance SourceInstance;
				SourceInstance.InstanceId = Reaction.SourceCardInstanceId;
				SourceInstance.OwnerUnitId = Reaction.GrantedByUnitId;
				if (!RegisterPartyReactionUses(
					NewRuntime,
					SourceInstance,
					Reaction.RecipientUnitId,
					EGameXXKCardStatus::Counter,
					1,
					ValidationError))
				{
					return SetFailure(OutError, ValidationError);
				}
			}
			NewRuntime.PendingBladeFinish = FGameXXKBladeFinishRuntime();
		}
		else if (bGroupBladeCounterVolley)
		{
			NewRuntime.PendingBladeFinish = FGameXXKBladeFinishRuntime();
		}
	}
	if (NewRuntime.PendingBladeFinish.Rule == EGameXXKBladeFinishRule::MarkAndPrepareTwoCounters)
	{
		NewRuntime.PendingBladeFinish.bTriggeredForCurrentEnemyCard = false;
	}
	RemoveDefeatedPartyReactions(NewRuntime);
	if (!EvaluateBossPhaseTransitions(NewRuntime, ValidationError)
		|| !ValidateCardBattleRuntimeInternal(NewRuntime, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	InOutRuntime = MoveTemp(NewRuntime);
	OutReactionDamageResults = MoveTemp(NewResults);
	return true;
}

bool GameXXKCardRules::BeginNextPlayerCardRound(
	FGameXXKCardBattleRuntime& InOutRuntime,
	TArray<FGameXXKCardDamageResult>& OutEndPhaseDamageResults,
	FString* OutError)
{
	if (OutError)
	{
		OutError->Reset();
	}
	FString ValidationError;
	if (!ValidateCardBattleRuntimeInternal(InOutRuntime, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	if (InOutRuntime.Phase != EGameXXKCardBattlePhase::Enemy)
	{
		return SetFailure(OutError, TEXT("A new player card phase can only begin after the enemy phase."));
	}
	if (!RequireNoPendingChoice(InOutRuntime.Deck, &ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}

	FGameXXKCardBattleRuntime NewRuntime = InOutRuntime;
	const TArray<FGameXXKCardCombatUnit> HealerFormulaUnitsBeforeEnemyDot = NewRuntime.Units;
	TArray<FGameXXKCardDamageResult> NewEndPhaseDamageResults;
	if (!ApplyEndPhaseDotForSide(NewRuntime, EGameXXKCardTargetSide::Enemy, NewEndPhaseDamageResults, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	if (!ResolveOpenedHealerFormulasAfterDamageEvents(
		NewRuntime,
		HealerFormulaUnitsBeforeEnemyDot,
		NewEndPhaseDamageResults,
		0,
		ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	GameXXKCardRules::RefreshCombatTerminalPhase(NewRuntime);
	if (!EvaluateBossPhaseTransitions(NewRuntime, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	if (NewRuntime.Phase == EGameXXKCardBattlePhase::Victory
		|| NewRuntime.Phase == EGameXXKCardBattlePhase::Defeat)
	{
		NewRuntime.PendingBladeDelayedCard = FGameXXKBladeDelayedCardRuntime();
		NewRuntime.PendingBladeFinish = FGameXXKBladeFinishRuntime();
		NewRuntime.PendingBladeNativeStyle = FGameXXKBladeStyleRuntime();
		NewRuntime.PendingBladeResidualStyle = FGameXXKBladeStyleRuntime();
		ClearPoJunBattleState(NewRuntime);
	}
	if (NewRuntime.Phase == EGameXXKCardBattlePhase::Enemy)
	{
		if (NewRuntime.PendingBladeFinish.Rule == EGameXXKBladeFinishRule::MarkAndPrepareTwoCounters
			|| NewRuntime.PendingBladeFinish.Rule == EGameXXKBladeFinishRule::MarkAndReregisterCounterVolley
			|| NewRuntime.PendingBladeFinish.Rule == EGameXXKBladeFinishRule::FirstTwoDodgesFree
			|| NewRuntime.PendingBladeFinish.Rule == EGameXXKBladeFinishRule::TransferMarkBeforeCounter
			|| NewRuntime.PendingBladeFinish.Rule == EGameXXKBladeFinishRule::FirstCounterVolleyHitsAll)
		{
			NewRuntime.PendingBladeFinish = FGameXXKBladeFinishRuntime();
		}
		// A full round ends only after enemy-side DoT. Evaluate this before clearing party armor.
		if (!ResolveEndOfRoundModifiers(NewRuntime, ValidationError))
		{
			return SetFailure(OutError, ValidationError);
		}
		if (!ClearArmorAtSidePhaseStart(NewRuntime, EGameXXKCardTargetSide::Party, ValidationError))
		{
			return SetFailure(OutError, ValidationError);
		}
		// The party shares one player-card phase, so that boundary is each living
		// character's turn start. Restore two points independently and never carry
		// overflow across a character's own maximum-mana cap.
		for (FGameXXKCardCombatUnit& Unit : NewRuntime.Units)
		{
			if (!Unit.bLiving || Unit.Side != EGameXXKCardTargetSide::Party)
			{
				continue;
			}
			Unit.Mana += FMath::Min(2, FMath::Max(0, Unit.MaxMana - Unit.Mana));
		}
		ExpireRoundBoundState(NewRuntime);
		NewRuntime.RevealedEnemyIntentCount = FMath::Max(0, NewRuntime.RevealedEnemyIntentCount - 1);
		if (NewRuntime.RoundNumber == MAX_int32)
		{
			return SetFailure(OutError, TEXT("Card battle round counter has exhausted the supported range."));
		}
		++NewRuntime.RoundNumber;
		ExpirePartyReactionsForPlayerRound(NewRuntime);
		NewRuntime.bTerrainChangedThisRound = false;
		if (!ResolveNextPlayerRoundStartModifiers(NewRuntime, ValidationError))
		{
			return SetFailure(OutError, ValidationError);
		}
		if (!RestoreDuanYueProtectedVulnerability(NewRuntime, ValidationError))
		{
			return SetFailure(OutError, ValidationError);
		}
		NewRuntime.ActiveCardsPlayedThisRound = 0;
		NewRuntime.LastActiveCard = FGameXXKResolvedCardSnapshot();
		const int64 RequestedEnergyRefill = static_cast<int64>(3)
			+ NewRuntime.BonusSharedEnergyCap
			+ NewRuntime.PendingNextRoundEnergyBonus
			- NewRuntime.PendingNextRoundEnergyPenalty;
		NewRuntime.Deck.SharedEnergy = static_cast<int32>(FMath::Clamp<int64>(RequestedEnergyRefill, 0, MaxCardBattleEnergy));
		NewRuntime.PendingNextRoundEnergyBonus = 0;
		NewRuntime.PendingNextRoundEnergyPenalty = 0;
		CancelBladeDelayIfOwnerDefeated(NewRuntime);
		const int32 ReservedRetainedCardSlots = NewRuntime.PendingBladeDelayedCard.Rule == EGameXXKBladeChargeRule::RetainNextActiveNextRound ? 1 : 0;
		const int32 DrawCount = FMath::Max(0, NewRuntime.Deck.HandLimit + NewRuntime.BonusRoundDrawCount - ReservedRetainedCardSlots - NewRuntime.Deck.Hand.Num());
		// A party unit defeated last round must not contribute cards or an owner-bound
		// Sorcerer task to this round. The task's locked-card and per-battle auto-hand
		// history become unreachable at the same boundary that removes its five cards.
		NewRuntime.SorcererPartnerTasks.RemoveAll([&NewRuntime](const FGameXXKSorcererPartnerTaskRuntime& Task)
		{
			const FGameXXKCardCombatUnit* Owner = FindCombatUnitById(NewRuntime.Units, Task.OwnerUnitId);
			return !Owner || !Owner->bLiving || Owner->Side != EGameXXKCardTargetSide::Party;
		});
		RemoveDefeatedPartyOwnerCards(NewRuntime.Deck, NewRuntime.Units);
		if (!DrawCards(NewRuntime.Deck, DrawCount, 0, &ValidationError))
		{
			return SetFailure(OutError, ValidationError);
		}
		NewRuntime.Phase = EGameXXKCardBattlePhase::Player;
		if (!MaterializePendingNextPlayerHandEnergySurcharge(NewRuntime, ValidationError))
		{
			return SetFailure(OutError, ValidationError);
		}
		if (!ResolveBladeDelayedCardAtPlayerRoundStart(NewRuntime, NewEndPhaseDamageResults, ValidationError))
		{
			return SetFailure(OutError, ValidationError);
		}
	}
	if (!ValidateCardBattleRuntimeInternal(NewRuntime, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	InOutRuntime = MoveTemp(NewRuntime);
	OutEndPhaseDamageResults = MoveTemp(NewEndPhaseDamageResults);
	return true;
}
