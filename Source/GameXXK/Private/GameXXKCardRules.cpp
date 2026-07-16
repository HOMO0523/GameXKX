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
