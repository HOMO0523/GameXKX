#include "GameXXKCardRules.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKCardQualityRules.h"
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
			|| Kind == EGameXXKCardPendingChoiceKind::InsightChooseToHand;
	}

	bool IsConcreteCardQuality(const EGameXXKCardQuality Quality)
	{
		return Quality == EGameXXKCardQuality::Common
			|| Quality == EGameXXKCardQuality::Rare
			|| Quality == EGameXXKCardQuality::Epic;
	}

	bool IsValidInstance(const FGameXXKCardInstance& Instance)
	{
		return !Instance.InstanceId.IsNone()
			&& !Instance.CardId.IsNone()
			&& IsConcreteCardQuality(Instance.CurrentQuality)
			&& !Instance.OwnerUnitId.IsNone()
			&& !Instance.SourceEntryId.IsNone()
			&& Instance.AcquisitionOrdinal != INDEX_NONE;
	}

	/** Pending-choice candidates are serialized UI views, so compare every stable field before trusting them. */
	bool IsSameInstance(const FGameXXKCardInstance& Left, const FGameXXKCardInstance& Right)
	{
		return Left.InstanceId == Right.InstanceId
			&& Left.CardId == Right.CardId
			&& Left.CurrentQuality == Right.CurrentQuality
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
			|| !ValidateZone(Deck.DiscardPile, TEXT("DiscardPile"))
			|| !ValidateZone(Deck.ExhaustPile, TEXT("ExhaustPile")))
		{
			return false;
		}
		if (ZoneIds.Num() != LedgerIds.Num())
		{
			OutError = TEXT("A ledger instance is not present in any logical card zone.");
			return false;
		}
		if (Deck.Hand.Num() > BattleHandCapacity)
		{
			OutError = TEXT("Hand exceeds the twenty-card battle capacity.");
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
		for (const FGameXXKCardCombatUnit& Unit : InOutRuntime.Units)
		{
			bHasLivingParty |= Unit.bLiving && Unit.Side == EGameXXKCardTargetSide::Party;
			bHasLivingEnemy |= Unit.bLiving && Unit.Side == EGameXXKCardTargetSide::Enemy;
		}
		if (!bHasLivingParty || !bHasLivingEnemy)
		{
			InOutRuntime.Phase = !bHasLivingEnemy
				? EGameXXKCardBattlePhase::Victory
				: EGameXXKCardBattlePhase::Defeat;
			// A next-player-hand effect has no legal recipient after a terminal transition.
			InOutRuntime.PendingNextPlayerHandEnergySurcharge = 0;
			InOutRuntime.PendingNextPlayerHandEnergySurchargeSourceUnitId = NAME_None;
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
		if (!ValidateDeckStateInternal(NewDeck, OutError))
		{
			return false;
		}
		InOutDeck = MoveTemp(NewDeck);
		return true;
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
			const int32 PoisonStacksBefore = TargetBeforeDot
				? GameXXKCardRules::GetCombatStatusStacks(*TargetBeforeDot, EGameXXKCardStatus::Poison)
				: 0;
			const int32 RotStacksBefore = TargetBeforeDot
				? GameXXKCardRules::GetCombatStatusStacks(*TargetBeforeDot, EGameXXKCardStatus::DamageOverTime)
				: 0;
			int32 HealthDamage = 0;
			if (!GameXXKCardRules::ApplyCombatEndPhaseDot(InOutRuntime.Units, InOutRuntime.GuardLinks, UnitId, HealthDamage, &OutError))
			{
				return false;
			}
			if (HealthDamage > 0)
			{
				FGameXXKCardDamageResult& Result = OutResults.AddDefaulted_GetRef();
				Result.OriginalTargetUnitId = UnitId;
				Result.ResolvedTargetUnitId = UnitId;
				Result.Cause = EGameXXKCardDamageCause::Poison;
				Result.StatusStacksBefore = PoisonStacksBefore;
				Result.RotDamageBonus = RotStacksBefore;
				Result.StatusStacksConsumed = PoisonStacksBefore > 0 ? 1 : 0;
				Result.RequestedDamage = static_cast<int32>(FMath::Min<int64>(
					MAX_int32,
					static_cast<int64>(PoisonStacksBefore) + RotStacksBefore));
				Result.DamageAfterDefense = Result.RequestedDamage;
				Result.DamageAfterVulnerability = Result.RequestedDamage;
				Result.HealthDamage = HealthDamage;
				Result.TargetHealthBefore = TargetHealthBefore;
				Result.TargetHealthAfter = FMath::Max(0, TargetHealthBefore - HealthDamage);
				if (const FGameXXKCardCombatUnit* TargetAfterDot = FindCombatUnitById(InOutRuntime.Units, UnitId))
				{
					Result.TargetHealthAfter = TargetAfterDot->HP;
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
		for (FGameXXKCardCombatUnit& Unit : InOutRuntime.Units)
		{
			if (!Unit.bLiving || Unit.Side != Side)
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
		if (NewDeck.DrawPile.IsEmpty() && !NewDeck.DiscardPile.IsEmpty())
		{
			NewDeck.DrawPile = MoveTemp(NewDeck.DiscardPile);
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
	constexpr int32 MaxCombatArmor = 99;
	constexpr int32 WhiteApeStatusGuardArmor = 8;
	constexpr uint32 CombatRandomMultiplier = 196314165u;
	constexpr uint32 CombatRandomIncrement = 907633515u;
	constexpr uint32 CombatRandomSalt = 0xA341316Cu;

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

	bool IsConcreteCombatStatus(const EGameXXKCardStatus Status)
	{
		return Status != EGameXXKCardStatus::Invalid
			&& Status != EGameXXKCardStatus::None
			&& Status != EGameXXKCardStatus::Guard;
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
			return MAX_int32;
		case EGameXXKCardStatus::Vulnerability:
		case EGameXXKCardStatus::Mark:
			return 5;
		case EGameXXKCardStatus::Medicine:
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
		case EGameXXKCardStatus::Charge:
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
		for (const FGameXXKReactionRuntime& Reaction : Runtime.Reactions)
		{
			const FGameXXKCardCombatUnit* Recipient = FindCombatUnitById(Runtime.Units, Reaction.RecipientUnitId);
			const FGameXXKCardCombatUnit* Grantor = FindCombatUnitById(Runtime.Units, Reaction.GrantedByUnitId);
			if (Reaction.ReactionId.IsNone() || ReactionIds.Contains(Reaction.ReactionId)
				|| !IsPartyReactionStatus(Reaction.Status)
				|| !Recipient || Recipient->Side != EGameXXKCardTargetSide::Party
				|| !Grantor || Grantor->Side != EGameXXKCardTargetSide::Party
				|| Reaction.SourceCardInstanceId.IsNone()
				|| Reaction.RemainingTriggers != 1
				|| Reaction.ExpireBeforePlayerRound <= Runtime.RoundNumber)
			{
				OutError = TEXT("Card battle runtime contains an invalid independently consumable party reaction.");
				return false;
			}
			ReactionIds.Add(Reaction.ReactionId);
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

		for (int32 UseIndex = 0; UseIndex < Uses; ++UseIndex)
		{
			const int32 Ordinal = InOutRuntime.NextReactionOrdinal++;
			FGameXXKReactionRuntime& Reaction = InOutRuntime.Reactions.AddDefaulted_GetRef();
			Reaction.ReactionId = FName(*FString::Printf(TEXT("Reaction.%d"), Ordinal));
			Reaction.Status = Status;
			Reaction.RecipientUnitId = RecipientUnitId;
			Reaction.GrantedByUnitId = Instance.OwnerUnitId;
			Reaction.SourceCardInstanceId = Instance.InstanceId;
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
				|| Unit.Armor < 0 || Unit.Armor > MaxCombatArmor
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
				if (Unit.EnemyDefinitionId.IsNone() || Unit.CombatLevel < 1 || Unit.CombatLevel > 20)
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
					|| Unit.CombatLevel > 20))
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

		if (Context.Kind == EGameXXKCardDamageKind::SelfHealthLoss)
		{
			const FGameXXKCardCombatUnit* SourceUnit = FindCombatUnitById(Units, Context.SourceUnitId);
			if (!SourceUnit || !SourceUnit->bLiving || SourceUnit->UnitId != OriginalTarget.UnitId
				|| Context.IgnoredDefense != 0 || Context.MomentumStacksOverride != INDEX_NONE
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
				|| Context.MomentumStacksOverride != INDEX_NONE || !Context.OnHitStatuses.IsEmpty())
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

bool GameXXKCardRules::ResolveWhiteApeStatusGuardAfterStatusApplied(
	FGameXXKCardBattleRuntime& InOutRuntime,
	FGameXXKCardCombatUnit& InOutStatusTarget,
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
		GameXXKCardRules::AddCombatArmor(InOutStatusTarget, WhiteApeStatusGuardArmor);
		NewEnemyState.bFirstStatusPassiveAvailable = false;
	}
	InOutRuntime.EnemyStates.Add(InOutStatusTarget.UnitId, MoveTemp(NewEnemyState));
	return true;
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
	const int32 OriginalArmor = FMath::Clamp(InOutUnit.Armor, 0, MaxCombatArmor);
	const int64 RequestedArmor = static_cast<int64>(OriginalArmor) + static_cast<int64>(Amount);
	InOutUnit.Armor = static_cast<int32>(FMath::Min<int64>(MaxCombatArmor, RequestedArmor));
	return InOutUnit.Armor - OriginalArmor;
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
	bool ApplyStatusHealthLoss(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FName TargetUnitId,
		const EGameXXKCardDamageCause Cause,
		const int32 BaseStacks,
		const bool bApplyRot,
		FGameXXKCardDamageResult& OutResult,
		FString& OutError)
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
		NewResult.Cause = Cause;
		NewResult.StatusStacksBefore = BaseStacks;
		NewResult.RotDamageBonus = bApplyRot
			? GetCombatStatusStacksInternal(*Target, EGameXXKCardStatus::DamageOverTime)
			: 0;
		NewResult.RequestedDamage = static_cast<int32>(FMath::Min<int64>(
			MAX_int32,
			static_cast<int64>(BaseStacks) + NewResult.RotDamageBonus));
		NewResult.DamageAfterDefense = NewResult.RequestedDamage;
		NewResult.DamageAfterVulnerability = NewResult.RequestedDamage;
		NewResult.TargetHealthBefore = Target->HP;
		NewResult.HealthDamage = FMath::Min(Target->HP, NewResult.RequestedDamage);
		Target->HP -= NewResult.HealthDamage;
		Target->bLiving = Target->HP > 0;
		NewResult.TargetHealthAfter = Target->HP;
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
	const int32 RotStacks = GetCombatStatusStacksInternal(*Target, EGameXXKCardStatus::DamageOverTime);
	const int64 RawDamage = PoisonStacks > 0
		? static_cast<int64>(PoisonStacks) + RotStacks
		: 0;
	const int32 NewHealthDamage = static_cast<int32>(FMath::Min<int64>(Target->HP, RawDamage));
	Target->HP -= NewHealthDamage;
	Target->bLiving = Target->HP > 0;
	if (PoisonStacks > 0)
	{
		GameXXKCardRules::ConsumeCombatStatus(*Target, EGameXXKCardStatus::Poison, 1);
	}
	GameXXKCardRules::ConsumeCombatStatus(*Target, EGameXXKCardStatus::Burn, 1);
	GameXXKCardRules::ConsumeCombatStatus(*Target, EGameXXKCardStatus::DamageOverTime, 1);
	GameXXKCardRules::ConsumeCombatStatus(*Target, EGameXXKCardStatus::Weak, 1);
	RemoveLinksForDefeatedUnits(NewGuardLinks, NewUnits);

	InOutUnits = MoveTemp(NewUnits);
	InOutGuardLinks = MoveTemp(NewGuardLinks);
	OutHealthDamage = NewHealthDamage;
	return true;
}

bool GameXXKCardRules::ResolveToxicExplosion(
	FGameXXKCardBattleRuntime& InOutRuntime,
	const FName SourceUnitId,
	const FName TargetUnitId,
	const bool bPreserveDamageOverTimeStacks,
	TArray<FGameXXKCardDamageResult>& OutResults,
	FString* OutError)
{
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
		int32 ResultIndex = INDEX_NONE;
	};
	TArray<FExplosionPacketSpec> PacketSpecs = {
		{EGameXXKCardStatus::Bleed, EGameXXKCardDamageCause::ToxicExplosionBleed},
		{EGameXXKCardStatus::Poison, EGameXXKCardDamageCause::ToxicExplosionPoison},
		{EGameXXKCardStatus::Burn, EGameXXKCardDamageCause::ToxicExplosionBurn}};
	for (FExplosionPacketSpec& PacketSpec : PacketSpecs)
	{
		PacketSpec.StacksBefore = GetCombatStatusStacksInternal(*Target, PacketSpec.Status);
	}

	TArray<FGameXXKCardDamageResult> NewResults;
	NewResults.Reserve(3);
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
			true,
			PacketResult,
			ValidationError))
		{
			return SetFailure(OutError, ValidationError);
		}
		PacketResult.SourceUnitId = SourceUnitId;
		PacketSpec.ResultIndex = NewResults.Add(MoveTemp(PacketResult));
	}

	Target = FindCombatUnitById(NewRuntime.Units, TargetUnitId);
	if (!Target)
	{
		return SetFailure(OutError, TEXT("Toxic explosion target disappeared before status consumption."));
	}
	if (!bPreserveDamageOverTimeStacks)
	{
		for (const FExplosionPacketSpec& PacketSpec : PacketSpecs)
		{
			if (PacketSpec.ResultIndex == INDEX_NONE)
			{
				continue;
			}
			NewResults[PacketSpec.ResultIndex].StatusStacksConsumed =
				GameXXKCardRules::ConsumeCombatStatus(*Target, PacketSpec.Status, 1);
		}
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

	TArray<FGameXXKCardCombatUnit> NewUnits = InOutUnits;
	TArray<FGameXXKCardGuardLinkRuntime> NewGuardLinks = InOutGuardLinks;
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
	NewResult.ResolutionOrigin = Context.ResolutionOrigin;
	NewResult.Cause = IsDirectAttackDamageKind(Context.Kind)
		? EGameXXKCardDamageCause::DirectAttack
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

	const bool bDirectAttack = IsDirectAttackDamageKind(Context.Kind);
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
		const FGameXXKCardCombatUnit* SourceUnit = bDirectAttack
			? FindCombatUnitById(NewUnits, Context.SourceUnitId)
			: nullptr;
		const int32 MomentumStacks = Context.MomentumStacksOverride != INDEX_NONE
			? Context.MomentumStacksOverride
			: (SourceUnit ? GetCombatStatusStacksInternal(*SourceUnit, EGameXXKCardStatus::Momentum) : 0);
		const int32 DamageWithMomentum = bDirectAttack
			? static_cast<int32>(FMath::Min<int64>(
				MAX_int32,
				static_cast<int64>(RequestedDamage) + MomentumStacks))
			: RequestedDamage;
		const int32 DamageAfterWeak = SourceUnit
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
		const int32 VulnerabilityStacks = bDirectAttack
			? GetCombatStatusStacksInternal(*ResolvedTarget, EGameXXKCardStatus::Vulnerability)
			: 0;
		const int32 MarkStacks = bDirectAttack
			? GetCombatStatusStacksInternal(*ResolvedTarget, EGameXXKCardStatus::Mark)
			: 0;
		const int32 MarkBonusPercent = MarkStacks > 0
			? GameXXKCardRules::MarkDirectDamageBonusPercent
			: 0;
		if (bDirectAttack)
		{
			NewResult.MarkStacksBeforeHit = MarkStacks;
			NewResult.MarkDamageBonusPercent = MarkBonusPercent;
		}
		const int64 AmplifiedDamage = static_cast<int64>(NewResult.DamageAfterDefense)
			* static_cast<int64>(100 + 10 * VulnerabilityStacks + MarkBonusPercent)
			/ 100;
		NewResult.DamageAfterVulnerability = static_cast<int32>(FMath::Min<int64>(MAX_int32, AmplifiedDamage));
		if (VulnerabilityStacks > 0)
		{
			GameXXKCardRules::ConsumeCombatStatus(*ResolvedTarget, EGameXXKCardStatus::Vulnerability, MAX_int32);
		}
		if (MarkStacks > 0)
		{
			NewResult.MarkStacksConsumed = GameXXKCardRules::ConsumeCombatStatus(
				*ResolvedTarget,
				EGameXXKCardStatus::Mark,
				1);
		}
		NewResult.ArmorAbsorbed = IsDirectAttackDamageKind(Context.Kind)
			? FMath::Min(ResolvedTarget->Armor, NewResult.DamageAfterVulnerability)
			: 0;
		ResolvedTarget->Armor -= NewResult.ArmorAbsorbed;
		NewResult.HealthDamage = NewResult.DamageAfterVulnerability - NewResult.ArmorAbsorbed;
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

			if (EnemyDefinition->PassiveId == EGameXXKEnemyPassiveId::IronfeatherFirstHit
				&& EnemyState.bFirstHitPassiveAvailable)
			{
				const int32 PassiveReducedHealthDamage = NewResult.HealthDamage / 2;
				NewResult.HealthDamage = PassiveReducedHealthDamage;
				if (PassiveReducedHealthDamage > 0)
				{
					EnemyState.bFirstHitPassiveAvailable = false;
				}
			}
			else if (EnemyDefinition->PassiveId == EGameXXKEnemyPassiveId::BlackBearThickHide)
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
		ResolvedTarget->HP -= NewResult.HealthDamage;
		ResolvedTarget->bLiving = ResolvedTarget->HP > 0;
		if (ResolvedTarget->bLiving && IsDirectAttackDamageKind(Context.Kind))
		{
			for (const FGameXXKCardStatusStack& OnHitStatus : Context.OnHitStatuses)
			{
				if (GameXXKCardRules::AddCombatStatus(*ResolvedTarget, OnHitStatus.Status, OnHitStatus.Stacks) > 0
					&& PlayerCardRuntime
					&& !GameXXKCardRules::ResolveWhiteApeStatusGuardAfterStatusApplied(*PlayerCardRuntime, *ResolvedTarget, OutError))
				{
					return false;
				}
			}
		}
	}
	NewResult.TargetHealthAfter = ResolvedTarget->HP;

	RemoveLinksForDefeatedUnits(NewGuardLinks, NewUnits);
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
		if (Snapshot.CardId.IsNone()
			|| !FGameXXKCardCatalog::FindCardDefinition(Snapshot.CardId)
			|| !IsConcreteCardQuality(Snapshot.Quality)
			|| Snapshot.OwnerUnitId.IsNone()
			|| !FindCombatUnitById(Units, Snapshot.OwnerUnitId))
		{
			OutError = TEXT("An automatic card snapshot has no catalog card, concrete quality, or stable owner record.");
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

	bool ValidateAutomaticResolutionQueue(
		const FGameXXKCardBattleRuntime& Runtime,
		FString& OutError)
	{
		const FGameXXKAutomaticResolutionQueue& Queue = Runtime.AutomaticResolutionQueue;
		if (!Queue.bActive)
		{
			if (Queue.Origin != EGameXXKCardResolutionOrigin::Invalid
				|| !Queue.PendingCards.IsEmpty()
				|| Queue.NextCardIndex != 0
				|| Queue.PendingReward != EGameXXKHeroSpellTaskReward::None
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
			|| (Queue.PendingCards.IsEmpty() && Queue.PendingReward == EGameXXKHeroSpellTaskReward::None)
			|| ((Queue.PendingReward == EGameXXKHeroSpellTaskReward::None) != Queue.RewardOwnerUnitId.IsNone()))
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

	bool ValidateCardBattleRuntimeInternal(const FGameXXKCardBattleRuntime& Runtime, FString& OutError)
	{
		OutError.Reset();
		if (!IsSupportedCardBattlePhase(Runtime.Phase) || !IsConcreteTerrain(Runtime.Terrain) || Runtime.RoundNumber < 1
			|| Runtime.ActiveCardsPlayedThisRound < 0 || Runtime.NextReactionOrdinal < 0 || Runtime.NextModifierOrdinal < 0
			|| Runtime.RevealedEnemyIntentCount < 0 || Runtime.RevealedEnemyIntentCount > MaxCardBattleEnergy
			|| Runtime.PendingNextRoundEnergyBonus < 0 || Runtime.PendingNextRoundEnergyBonus > MaxCardBattleEnergy
			|| Runtime.PendingNextPlayerHandEnergySurcharge < 0 || Runtime.PendingNextPlayerHandEnergySurcharge > 1
			|| (Runtime.PendingNextPlayerHandEnergySurcharge == 0) != Runtime.PendingNextPlayerHandEnergySurchargeSourceUnitId.IsNone())
		{
			OutError = TEXT("Card battle runtime has an invalid phase, terrain, round, modifier counter, or deferred card state.");
			return false;
		}
		if (!ValidateDeckStateInternal(Runtime.Deck, OutError))
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
		for (const FGameXXKCardCombatUnit& Unit : Runtime.Units)
		{
			bHasParty |= Unit.Side == EGameXXKCardTargetSide::Party;
			bHasEnemy |= Unit.Side == EGameXXKCardTargetSide::Enemy;
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
			if (!Modifier.RequiredPlayedCardInstanceId.IsNone()
				&& (Modifier.SourceCardInstanceId != Modifier.RequiredPlayedCardInstanceId
					|| !IsCurrentHandInstance(Runtime.Deck, Modifier.RequiredPlayedCardInstanceId)
					|| ModifierSource->Side != EGameXXKCardTargetSide::Enemy
					|| (Runtime.Phase != EGameXXKCardBattlePhase::Player && Runtime.Phase != EGameXXKCardBattlePhase::Victory)
					|| Modifier.Definition.Trigger != EGameXXKCardBattleModifierTrigger::OnCardPlayed
					|| Modifier.Definition.EffectType != EGameXXKCardEffectType::ModifyEnergyCost
					|| Modifier.Definition.Target != EGameXXKCardEffectTarget::PlayedCard
					|| Modifier.Definition.RecipientScope != EGameXXKCardModifierRecipientScope::SharedDeck
					|| Modifier.Definition.RecipientTarget != EGameXXKCardEffectTarget::PlayedCard
					|| Modifier.Definition.RequiredTriggeredRole != EGameXXKCharacterRole::Invalid
					|| !Modifier.Definition.RequiredTriggeredOwnerId.IsNone()
					|| Modifier.Definition.Expiry != EGameXXKCardModifierExpiry::AfterTriggerCount
					|| Modifier.Definition.TriggeredAttackTargetScope != EGameXXKCardTriggeredAttackTargetScope::Invalid
					|| Modifier.Definition.Status != EGameXXKCardStatus::None
					|| Modifier.Definition.Magnitude != 1
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
					|| ModifierCondition.bNegate))
			{
				OutError = TEXT("A hand-bound energy surcharge modifier is malformed or no longer bound to its exact current hand instance.");
				return false;
			}
			if (!Modifier.RequiredPlayedCardInstanceId.IsNone() && ++HandBoundEnergySurchargeCount > 1)
			{
				OutError = TEXT("Card battle runtime cannot stack multiple hand-bound energy surcharge modifiers.");
				return false;
			}
			ModifierIds.Add(Modifier.ModifierId);
		}
		if (!ValidatePartyReactions(Runtime, OutError))
		{
			return false;
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
			EquipmentEffectKeys.Add(Key);
		}
		if (!bHasParty || !bHasEnemy)
		{
			OutError = TEXT("Card battle runtime must retain at least one party and one enemy record.");
			return false;
		}
		if (!ValidateAutomaticResolutionQueue(Runtime, OutError))
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

	bool ConsumeOnCardPlayedModifiers(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const TArray<FName>& ModifierIds,
		FString& OutError);

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
		const FGameXXKCardDefinition EffectiveDefinition = FGameXXKCardQualityRules::BuildEffectiveDefinition(
			*BaseDefinition,
			Instance->CurrentQuality);
		const FGameXXKCardCombatUnit* Owner = FindCombatUnitById(Runtime.Units, Instance->OwnerUnitId);
		if (!Owner || !Owner->bLiving)
		{
			OutError = TEXT("The card owner is absent or defeated.");
			return false;
		}
		if (EffectiveDefinition.EnergyCost < 0 || EffectiveDefinition.ManaCost < 0)
		{
			OutError = TEXT("The requested hand card has invalid resource costs.");
			return false;
		}
		int32 EffectiveEnergyCost = EffectiveDefinition.EnergyCost;
		if (!BuildEffectiveCardEnergyCost(Runtime, EffectiveDefinition, *Instance, *Owner, EffectiveEnergyCost, nullptr, nullptr, nullptr, OutError))
		{
			return false;
		}
		if (Runtime.Deck.SharedEnergy < EffectiveEnergyCost || Owner->Mana < EffectiveDefinition.ManaCost)
		{
			OutError = TEXT("The card owner does not have enough shared energy or mana.");
			return false;
		}

		FGameXXKCardPlayPreview NewPreview;
		NewPreview.CardInstanceId = Instance->InstanceId;
		NewPreview.CardId = Instance->CardId;
		NewPreview.OwnerUnitId = Instance->OwnerUnitId;
		NewPreview.EffectiveEnergyCost = EffectiveEnergyCost;
		NewPreview.EffectiveManaCost = EffectiveDefinition.ManaCost;
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
		default:
			OutError = TEXT("Card effect has an invalid condition type.");
			return false;
		}
		OutSatisfied = Condition.bNegate ? !bConditionValue : bConditionValue;
		return true;
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
		if ((ModifierDefinition.Trigger != EGameXXKCardBattleModifierTrigger::OnCardPlayed
			&& ModifierDefinition.Trigger != EGameXXKCardBattleModifierTrigger::BeforeFirstActiveCardNextPlayerRound)
			|| ModifierDefinition.EffectType != EGameXXKCardEffectType::ModifyEnergyCost)
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
		int64 EffectiveCost = Definition.EnergyCost;
		for (const FGameXXKCardBattleModifierRuntime& Modifier : Runtime.Modifiers)
		{
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
		OutEnergyCost = !TerrainFreeUnitIds.IsEmpty() || EffectiveCost <= 0
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
			if ((ModifierDefinition.Trigger != EGameXXKCardBattleModifierTrigger::OnCardPlayed
				&& ModifierDefinition.Trigger != EGameXXKCardBattleModifierTrigger::BeforeFirstActiveCardNextPlayerRound)
				|| ModifierDefinition.EffectType != EGameXXKCardEffectType::ModifyEnergyCost)
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
		case EGameXXKCardEffectType::Cleanse:
		case EGameXXKCardEffectType::TriggerHighestDamageOverTime:
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
			|| ModifierDefinition.EffectType != EGameXXKCardEffectType::BonusDamagePercent)
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
			int32 IgnoredConsumed = 0;
			if (!TryApplyEffectConditionAndConsumption(Modifier.Definition.Condition, InOutRuntime, InOutOwner, ConditionTarget, nullptr, bConditionSatisfied, IgnoredConsumed, OutError))
			{
				return false;
			}
			if (!bConditionSatisfied)
			{
				continue;
			}
			if ((Modifier.Definition.Magnitude > 0 && OutBonusPercent > MAX_int32 - Modifier.Definition.Magnitude)
				|| (Modifier.Definition.Magnitude < 0 && OutBonusPercent < MIN_int32 - Modifier.Definition.Magnitude))
			{
				OutError = TEXT("Next-attack modifier bonuses exceed the supported range.");
				return false;
			}
			OutBonusPercent += Modifier.Definition.Magnitude;
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
				|| ModifierDefinition.EffectType != EGameXXKCardEffectType::BonusDamagePercent)
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
		FString& OutError)
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
				if (!GameXXKCardRules::ApplyCombatDirectDamage(InOutRuntime.Units, InOutRuntime.GuardLinks, CounterContext, AttackerUnitId, static_cast<int32>(RequestedDamage), CounterResult, &OutError))
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
				if (GameXXKCardRules::AddCombatStatus(*Attacker, ModifierDefinition.Status, ModifierDefinition.Magnitude) > 0
					&& !GameXXKCardRules::ResolveWhiteApeStatusGuardAfterStatusApplied(InOutRuntime, *Attacker, &OutError))
				{
					return false;
				}
			}
			TriggeredModifierIds.Add(Modifier.ModifierId);
		}
		return ConsumeFirstDirectDamageModifiers(InOutRuntime, TriggeredModifierIds, OutError);
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
		TArray<int32> AttachmentIndices;
		for (int32 Index = AttackIndex + 1; Index < Definition.Effects.Num() && Definition.Effects[Index].Type != EGameXXKCardEffectType::DamagePercentAttack; ++Index)
		{
			if (IsAttackPacketAttachment(Definition.Effects[Index], Attack))
			{
				AttachmentIndices.Add(Index);
				OutAttachedEffectIndices.Add(Index);
			}
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
		const FGameXXKCardCombatUnit* PacketOwner = FindCombatUnitById(InOutRuntime.Units, Instance.OwnerUnitId);
		const int32 MomentumAtPacketStart = PacketOwner
			? GameXXKCardRules::GetCombatStatusStacks(*PacketOwner, EGameXXKCardStatus::Momentum)
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
					IgnoredDefense += Attachment.Magnitude;
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
			Target = FindCombatUnitById(InOutRuntime.Units, TargetId);
			if (!Owner || !Owner->bLiving || !Target || !Target->bLiving)
			{
				continue;
			}
			if (!bPreparedTriggeredAttack)
			{
				if (Origin == EGameXXKCardResolutionOrigin::ActivePlay)
				{
					TArray<FName> TriggeredModifierIds;
					if (!CollectOnNextAttackBonuses(InOutRuntime, Definition, Instance, *Owner, ConditionTarget, TriggeredAttackBonusPercent, TriggeredModifierIds, OutError))
					{
						return false;
					}
					if (!ConsumeOnNextAttackModifiers(InOutRuntime, TriggeredModifierIds, OutError))
					{
						return false;
					}
					bApplyNextAttackVulnerability = GameXXKCardRules::GetCombatStatusStacks(*Owner, EGameXXKCardStatus::NextAttackAppliesVulnerability) > 0;
					bApplyNextAttackMark = GameXXKCardRules::GetCombatStatusStacks(*Owner, EGameXXKCardStatus::NextAttackBonus) > 0;
					if (bApplyNextAttackVulnerability)
					{
						GameXXKCardRules::ConsumeCombatStatus(*Owner, EGameXXKCardStatus::NextAttackAppliesVulnerability, 1);
					}
					if (bApplyNextAttackMark)
					{
						GameXXKCardRules::ConsumeCombatStatus(*Owner, EGameXXKCardStatus::NextAttackBonus, 1);
					}
				}
				bPreparedTriggeredAttack = true;
			}
			Percent += TriggeredAttackBonusPercent;
			const int64 RawDamage = static_cast<int64>(Owner->Attack) * Percent / 100 + FlatDamage;
			if (RawDamage <= 0 || RawDamage > MAX_int32 || IgnoredDefense < 0 || IgnoredDefense > MAX_int32)
			{
				OutError = TEXT("Attack packet produced unsupported damage or defense-ignore values.");
				return false;
			}
			for (int32 HitIndex = 0; HitIndex < Attack.HitCount; ++HitIndex)
			{
				const FGameXXKCardCombatUnit* HitOwner = FindCombatUnitById(InOutRuntime.Units, Instance.OwnerUnitId);
				if (!HitOwner || !HitOwner->bLiving)
				{
					break;
				}
				FGameXXKCardDamageContext Context;
				Context.SourceUnitId = HitOwner->UnitId;
				Context.ResolutionOrigin = Origin;
				Context.Kind = Attack.Target == EGameXXKCardEffectTarget::AllEnemies
					? EGameXXKCardDamageKind::GroupAttack
					: EGameXXKCardDamageKind::SingleTargetAttack;
				Context.IgnoredDefense = IgnoredDefense;
				Context.MomentumStacksOverride = MomentumAtPacketStart;
				for (const FGameXXKCardEffect* OnHitEffect : OnHitStatusEffects)
				{
					if (OnHitEffect->HitCount == Attack.HitCount || HitIndex < OnHitEffect->HitCount)
					{
						FGameXXKCardStatusStack& Status = Context.OnHitStatuses.AddDefaulted_GetRef();
						Status.Status = OnHitEffect->Status;
						Status.Stacks = OnHitEffect->Magnitude;
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
				if (!GameXXKCardRules::ApplyPlayerCardDirectDamage(InOutRuntime, Context, TargetId, static_cast<int32>(RawDamage), DamageResult, &OutError))
				{
					return false;
				}
				InOutResult.DamageResults.Add(DamageResult);
				if (!ResolveFirstDirectDamageReactiveModifiers(InOutRuntime, Context, DamageResult, &InOutResult.DamageResults, OutError))
				{
					return false;
				}
				const FGameXXKCardCombatUnit* CurrentTarget = FindCombatUnitById(InOutRuntime.Units, TargetId);
				if (!CurrentTarget || !CurrentTarget->bLiving)
				{
					break;
				}
			}
		}
		return true;
	}

	bool EffectRequiresOriginalSelectedTarget(const FGameXXKCardEffect& Effect)
	{
		const bool bTargetCondition = Effect.Condition.Type == EGameXXKCardEffectConditionType::TargetHasStatus
			|| Effect.Condition.Type == EGameXXKCardEffectConditionType::TargetHasAnyDamageOverTime
			|| Effect.Condition.Type == EGameXXKCardEffectConditionType::TargetHealthBelowPercent;
		return Effect.Target == EGameXXKCardEffectTarget::SelectedTarget
			|| Effect.Type == EGameXXKCardEffectType::EachLivingAllyAttackSelectedTarget
			|| bTargetCondition
			|| (Effect.Type == EGameXXKCardEffectType::ApplyBattleModifier
				&& Effect.Modifier.RecipientTarget == EGameXXKCardEffectTarget::SelectedTarget)
			|| (Effect.Type == EGameXXKCardEffectType::ApplyGuardLink
				&& Effect.GuardLink.Guardian == EGameXXKCardEffectTarget::SelectedTarget);
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
		TSet<int32> AttachedEffectIndices;
		TMap<FName, int32> ConsumptionResults;
		bool bPreparedHealingAction = false;
		int32 HealingBonusPercent = 0;
		int32 HealingFlatBonus = 0;
		for (int32 EffectIndex = 0; EffectIndex < Definition.Effects.Num(); ++EffectIndex)
		{
			if (AttachedEffectIndices.Contains(EffectIndex))
			{
				continue;
			}
			const FGameXXKCardEffect& Effect = Definition.Effects[EffectIndex];
			if (bSkipMissingSelectedTargetEffects
				&& Effect.Type != EGameXXKCardEffectType::DamagePercentAttack
				&& EffectRequiresOriginalSelectedTarget(Effect))
			{
				continue;
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
			TArray<FName> EffectTargetIds;
			if (!ResolveEffectTargetIds(InOutRuntime, Instance.OwnerUnitId, CardTargetIds, Effect.Target, EffectTargetIds, OutError))
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

			if (Effect.Type == EGameXXKCardEffectType::DrawCards
				|| Effect.Type == EGameXXKCardEffectType::Insight
				|| Effect.Type == EGameXXKCardEffectType::DiscoverCards
				|| Effect.Type == EGameXXKCardEffectType::ReorderCards
				|| Effect.Type == EGameXXKCardEffectType::DiscardCards
				|| Effect.Type == EGameXXKCardEffectType::GainEnergy
				|| Effect.Type == EGameXXKCardEffectType::RevealEnemyIntent
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
					if (!GameXXKCardRules::DrawCards(InOutRuntime.Deck, Effect.Magnitude, DeclaredDiscardCount, &OutError))
					{
						return false;
					}
					InOutResult.bOpenedPendingChoice |= InOutRuntime.Deck.PendingChoice.Kind == EGameXXKCardPendingChoiceKind::ForcedDiscard;
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
					if (InOutRuntime.Deck.PendingChoice.Kind == EGameXXKCardPendingChoiceKind::ForcedDiscard)
					{
						if (InOutRuntime.Deck.PendingChoice.RequiredDiscardCount != Effect.Magnitude)
						{
							OutError = TEXT("Draw-then-discard choice does not match the card's declared discard count.");
							return false;
						}
						InOutResult.bOpenedPendingChoice = true;
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
				FGameXXKCardCombatUnit* Target = FindCombatUnitById(InOutRuntime.Units, EffectTargetId);
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
					if (!GameXXKCardRules::ApplyCombatDirectDamage(InOutRuntime.Units, InOutRuntime.GuardLinks, Context, Target->UnitId, Effect.Magnitude, DamageResult, &OutError))
					{
						return false;
					}
					InOutResult.DamageResults.Add(MoveTemp(DamageResult));
					break;
				}
				case EGameXXKCardEffectType::Heal:
				{
					int64 BaseHealing = FMath::Max(0, Effect.Magnitude);
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
					GameXXKCardRules::HealCombatUnit(*Target, static_cast<int32>(FMath::Clamp<int64>(FinalHealing, 0, MAX_int32)));
					break;
				}
				case EGameXXKCardEffectType::AddArmor:
					GameXXKCardRules::AddCombatArmor(*Target, Effect.Magnitude);
					break;
				case EGameXXKCardEffectType::GainMana:
					Target->Mana = static_cast<int32>(FMath::Min<int64>(Target->MaxMana, static_cast<int64>(Target->Mana) + FMath::Max(0, Effect.Magnitude)));
					break;
				case EGameXXKCardEffectType::GainManaPerConsumedStatus:
				{
					const int64 ManaGain = static_cast<int64>(FMath::Max(0, Effect.Magnitude)) * Consumed;
					Target->Mana = static_cast<int32>(FMath::Min<int64>(Target->MaxMana, static_cast<int64>(Target->Mana) + ManaGain));
					break;
				}
				case EGameXXKCardEffectType::ApplyStatus:
					if (GameXXKCardRules::AddCombatStatus(*Target, Effect.Status, Effect.Magnitude) > 0
						&& !GameXXKCardRules::ResolveWhiteApeStatusGuardAfterStatusApplied(InOutRuntime, *Target, &OutError))
					{
						return false;
					}
					break;
				case EGameXXKCardEffectType::RemoveStatus:
					GameXXKCardRules::ConsumeCombatStatus(*Target, Effect.Status, Effect.Magnitude);
					break;
				case EGameXXKCardEffectType::RemoveAnyDamageOverTime:
					RemoveAnyDamageOverTime(*Target, Effect.Magnitude);
					break;
				case EGameXXKCardEffectType::Cleanse:
					GameXXKCardRules::ConsumeCombatStatus(*Target, EGameXXKCardStatus::Bleed, MAX_int32);
					GameXXKCardRules::ConsumeCombatStatus(*Target, EGameXXKCardStatus::Poison, MAX_int32);
					GameXXKCardRules::ConsumeCombatStatus(*Target, EGameXXKCardStatus::Burn, MAX_int32);
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
						true,
						TriggerResult,
						OutError))
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
					TriggerResult.StatusStacksConsumed = GameXXKCardRules::ConsumeCombatStatus(*Target, TriggeredStatus, 1);
					InOutResult.DamageResults.Add(MoveTemp(TriggerResult));
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
						if (!ResolveFirstDirectDamageReactiveModifiers(InOutRuntime, Context, DamageResult, &InOutResult.DamageResults, OutError))
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
							OutError = TEXT("A guard link cannot protect its own guardian.");
							return false;
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
		return true;
	}

	bool ResolveCardEffectsFromSnapshot(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FGameXXKResolvedCardSnapshot& Snapshot,
		const EGameXXKCardResolutionOrigin Origin,
		FGameXXKCardPlayResult& InOutResult,
		FString& OutError)
	{
		if (Origin == EGameXXKCardResolutionOrigin::Invalid)
		{
			OutError = TEXT("Card effect resolution requires an explicit origin.");
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

		const FGameXXKCardDefinition QualityEffectiveDefinition = FGameXXKCardQualityRules::BuildEffectiveDefinition(
			*BaseDefinition,
			Snapshot.Quality);
		if (!ValidateCurrentEffectPlan(QualityEffectiveDefinition, OutError))
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
			|| Queue.PendingReward != EGameXXKHeroSpellTaskReward::None)
		{
			OutError = TEXT("A Blade replay cannot join a different automatic-resolution operation.");
			return false;
		}
		Queue.PendingCards.Add(Snapshot);
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
		FGameXXKCardPlayResult& InOutResult,
		FString& OutError)
	{
		const FGameXXKCardBattleModifier& Definition = Modifier.Definition;
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
					GameXXKCardRules::AddCombatStatus(*Recipient, Definition.Status, Definition.Magnitude);
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
			default:
				OutError = TEXT("Triggered status damage supports Bleed, Poison, or Burn only.");
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
			if (!ApplyStatusHealthLoss(InOutRuntime, ConditionTargetUnitId, Cause, StacksBefore, true, DamageResult, OutError))
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
			DamageResult.StatusStacksConsumed = GameXXKCardRules::ConsumeCombatStatus(*Target, Definition.Status, 1);
			if (DamageResult.StatusStacksConsumed != 1)
			{
				OutError = TEXT("Triggered status damage failed to consume its declared layer.");
				return false;
			}
			if (Definition.bPreserveTriggeredStatus
				&& GameXXKCardRules::AddCombatStatus(*Target, Definition.Status, DamageResult.StatusStacksConsumed) != DamageResult.StatusStacksConsumed)
			{
				OutError = TEXT("A preserving status trigger failed to restore its consumed layer.");
				return false;
			}
			InOutResult.DamageResults.Add(MoveTemp(DamageResult));
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
			if (!ResolveTriggeredModifierAction(InOutRuntime, ModifierCopy, PlayedSnapshot, PlayedInstance, ConditionTargetUnitId, InOutResult, OutError)
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

	void ExpireUntriggeredNextPlayerRoundModifiers(FGameXXKCardBattleRuntime& InOutRuntime)
	{
		InOutRuntime.Modifiers.RemoveAll([](const FGameXXKCardBattleModifierRuntime& Modifier)
		{
			switch (Modifier.Definition.Trigger)
			{
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
				FGameXXKCardPlayResult IgnoredResult;
				if (!ResolveTriggeredModifierAction(InOutRuntime, ModifierCopy, ModifierCopy.SourceCardSnapshot, SourceInstance, ConditionTargetUnitId, IgnoredResult, OutError))
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

bool GameXXKCardRules::InitializeCardBattleRuntime(
	FGameXXKCardBattleRuntime& InOutRuntime,
	const TArray<FGameXXKCardInstance>& Instances,
	const TArray<FGameXXKCardCombatUnit>& Units,
	const EGameXXKCardTerrain Terrain,
	const int32 InitialRandomSeed,
	FString* OutError)
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

bool GameXXKCardRules::ApplyPlayerCardDirectDamage(
	FGameXXKCardBattleRuntime& InOutRuntime,
	const FGameXXKCardDamageContext& Context,
	const FName TargetUnitId,
	const int32 RequestedDamage,
	FGameXXKCardDamageResult& OutResult,
	FString* OutError)
{
	if (OutError)
	{
		OutError->Reset();
	}
	if (!IsDirectAttackDamageKind(Context.Kind) || Context.SourceUnitId.IsNone())
	{
		return SetFailure(OutError, TEXT("Player card direct damage requires a concrete direct-attack context and source."));
	}
	FString ValidationError;
	if (!ValidateCardBattleRuntimeInternal(InOutRuntime, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	const FGameXXKCardCombatUnit* Source = FindCombatUnitById(InOutRuntime.Units, Context.SourceUnitId);
	if (!Source || !Source->bLiving || Source->Side != EGameXXKCardTargetSide::Party)
	{
		return SetFailure(OutError, TEXT("Player card direct damage requires one living party source."));
	}

	FGameXXKCardBattleRuntime NewRuntime = InOutRuntime;
	FGameXXKCardDamageContext ResolvedContext = Context;
	ResolvedContext.AgilityRollPercent = AdvanceCombatRandomRoll(NewRuntime);
	FGameXXKCardDamageResult NewResult;
	if (!ApplyCombatDirectDamageInternal(
		NewRuntime.Units,
		NewRuntime.GuardLinks,
		ResolvedContext,
		TargetUnitId,
		RequestedDamage,
		NewResult,
		&NewRuntime,
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

	FGameXXKAutomaticResolutionQueue& Queue = NewRuntime.AutomaticResolutionQueue;
	while (Queue.bActive && Queue.NextCardIndex < Queue.PendingCards.Num())
	{
		const FGameXXKResolvedCardSnapshot Snapshot = Queue.PendingCards[Queue.NextCardIndex++];
		FGameXXKCardPlayResult Result;
		if (!ResolveCardEffectsFromSnapshot(NewRuntime, Snapshot, Queue.Origin, Result, ValidationError))
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

	if (Queue.bActive && Queue.PendingReward != EGameXXKHeroSpellTaskReward::None)
	{
		return SetFailure(OutError, TEXT("The queued spell-task reward is not implemented by the current foundation resolver."));
	}
	if (Queue.bActive)
	{
		Queue = FGameXXKAutomaticResolutionQueue();
	}
	GameXXKCardRules::RefreshCombatTerminalPhase(NewRuntime);
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
	InOutRuntime = MoveTemp(NewRuntime);
	if (OutResumedResults)
	{
		*OutResumedResults = MoveTemp(ResumedResults);
	}
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
	TArray<FName> AppliedCostModifierIds;
	TArray<FName> TerrainFreeStatusUnitIds;
	TArray<FName> TerrainReductionStatusUnitIds;
	int32 FreshEffectiveEnergyCost = QualityEffectiveDefinition.EnergyCost;
	if (!BuildEffectiveCardEnergyCost(NewRuntime, QualityEffectiveDefinition, *Instance, *Owner, FreshEffectiveEnergyCost, &AppliedCostModifierIds, &TerrainFreeStatusUnitIds, &TerrainReductionStatusUnitIds, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	if (FreshEffectiveEnergyCost != Preview.EffectiveEnergyCost)
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
	if (!ConsumeOnCardPlayedModifiers(NewRuntime, AppliedCostModifierIds, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
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
	if (!ResolveCardEffectsFromSnapshot(
		NewRuntime,
		ActiveSnapshot,
		EGameXXKCardResolutionOrigin::ActivePlay,
		NewResult,
		ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	for (const EGameXXKCardBattleModifierTrigger Trigger : {
		EGameXXKCardBattleModifierTrigger::OnNextAttack,
		EGameXXKCardBattleModifierTrigger::FirstActiveAttackAgainstStatusNextPlayerRound,
		EGameXXKCardBattleModifierTrigger::AfterNextActiveCard})
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
	if (NewRuntime.ActiveCardsPlayedThisRound == MAX_int32)
	{
		return SetFailure(OutError, TEXT("The active-card counter has exhausted the supported range."));
	}
	++NewRuntime.ActiveCardsPlayedThisRound;
	NewRuntime.LastActiveCard = ActiveSnapshot;
	if (NewRuntime.AutomaticResolutionQueue.bActive)
	{
		TArray<FGameXXKCardPlayResult> AutomaticResults;
		if (!GameXXKCardRules::ResumeAutomaticResolutionQueue(NewRuntime, AutomaticResults, &ValidationError))
		{
			return SetFailure(OutError, ValidationError);
		}
		for (FGameXXKCardPlayResult& AutomaticResult : AutomaticResults)
		{
			NewResult.DamageResults.Append(MoveTemp(AutomaticResult.DamageResults));
			NewResult.bOpenedPendingChoice |= AutomaticResult.bOpenedPendingChoice;
		}
	}
	if (!IsActiveChoice(NewRuntime.Deck.PendingChoice.Kind)
		&& !NewRuntime.AutomaticResolutionQueue.bActive)
	{
		GameXXKCardRules::RefreshCombatTerminalPhase(NewRuntime);
		if (!EvaluateBossPhaseTransitions(NewRuntime, ValidationError))
		{
			return SetFailure(OutError, ValidationError);
		}
	}
	if (!ValidateCardBattleRuntimeInternal(NewRuntime, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	InOutRuntime = MoveTemp(NewRuntime);
	OutResult = MoveTemp(NewResult);
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
	ExpireUntriggeredNextPlayerRoundModifiers(NewRuntime);
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
	}
	NewRuntime.LastActiveCard = FGameXXKResolvedCardSnapshot();
	if (!DiscardRemainingHand(NewRuntime.Deck, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	RemoveHandBoundEnergySurchargesOutsideCurrentHand(NewRuntime);
	if (!ApplyEndPhaseDotForSide(NewRuntime, EGameXXKCardTargetSide::Party, NewEndPhaseDamageResults, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	GameXXKCardRules::RefreshCombatTerminalPhase(NewRuntime);
	if (!EvaluateBossPhaseTransitions(NewRuntime, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
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
	FGameXXKCardDamageContext ResolvedContext = Context;
	ResolvedContext.AgilityRollPercent = AdvanceCombatRandomRoll(NewRuntime);
	FName AppliedTargetUnitId = SelectedPartyTargetUnitId;
	bool bRedirectedByCard = false;
	if (!ApplySingleTargetEnemyRedirect(NewRuntime, ResolvedContext, SelectedPartyTargetUnitId, AppliedTargetUnitId, bRedirectedByCard, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	FGameXXKCardDamageResult NewResult;
	if (!ApplyCombatDirectDamage(NewRuntime.Units, NewRuntime.GuardLinks, ResolvedContext, AppliedTargetUnitId, RequestedDamage, NewResult, &ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	NewResult.OriginalTargetUnitId = SelectedPartyTargetUnitId;
	NewResult.bRedirected |= bRedirectedByCard;
	UE_LOG(LogTemp, Warning, TEXT("[EnemyAtk] source=%s requested=%d target=%s redirected=%d before=%d dmg=%d after=%d"),
		*ResolvedContext.SourceUnitId.ToString(),
		RequestedDamage,
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
	if (!bDeferTerminalPhase)
	{
		GameXXKCardRules::RefreshCombatTerminalPhase(NewRuntime);
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
		for (const FGameXXKReactionRuntime& Reaction : NewRuntime.Reactions)
		{
			if (Reaction.RecipientUnitId == FinalRecipientUnitId && Reaction.RemainingTriggers > 0)
			{
				TriggeredReactions.Add(Reaction);
			}
		}
		NewRuntime.Reactions.RemoveAll([FinalRecipientUnitId](const FGameXXKReactionRuntime& Reaction)
		{
			return Reaction.RecipientUnitId == FinalRecipientUnitId && Reaction.RemainingTriggers > 0;
		});
		SyncPartyReactionStatuses(NewRuntime);

		for (const FGameXXKReactionRuntime& Reaction : TriggeredReactions)
		{
			const FGameXXKCardCombatUnit* ReactionSource = FindCombatUnitById(NewRuntime.Units, Reaction.RecipientUnitId);
			const FGameXXKCardCombatUnit* Enemy = FindCombatUnitById(NewRuntime.Units, EnemySourceUnitId);
			if (!ReactionSource || ReactionSource->Side != EGameXXKCardTargetSide::Party
				|| !Enemy || !Enemy->bLiving || Enemy->Side != EGameXXKCardTargetSide::Enemy)
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

			FGameXXKCardDamageContext ReactionContext;
			ReactionContext.SourceUnitId = ReactionSource->UnitId;
			ReactionContext.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
			ReactionContext.ResolutionOrigin = EGameXXKCardResolutionOrigin::Reaction;
			FGameXXKCardDamageResult ReactionResult;
			if (!ApplyCombatDirectDamageInternal(
				NewRuntime.Units,
				NewRuntime.GuardLinks,
				ReactionContext,
				EnemySourceUnitId,
				static_cast<int32>(RequestedDamage),
				ReactionResult,
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
	TArray<FGameXXKCardDamageResult> NewEndPhaseDamageResults;
	if (!ApplyEndPhaseDotForSide(NewRuntime, EGameXXKCardTargetSide::Enemy, NewEndPhaseDamageResults, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	GameXXKCardRules::RefreshCombatTerminalPhase(NewRuntime);
	if (!EvaluateBossPhaseTransitions(NewRuntime, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	if (NewRuntime.Phase == EGameXXKCardBattlePhase::Enemy)
	{
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
		if (!ResolveNextPlayerRoundStartModifiers(NewRuntime, ValidationError))
		{
			return SetFailure(OutError, ValidationError);
		}
		NewRuntime.ActiveCardsPlayedThisRound = 0;
		NewRuntime.LastActiveCard = FGameXXKResolvedCardSnapshot();
		NewRuntime.Deck.SharedEnergy = FMath::Min(MaxCardBattleEnergy, 3 + NewRuntime.PendingNextRoundEnergyBonus);
		NewRuntime.PendingNextRoundEnergyBonus = 0;
		const int32 DrawCount = FMath::Max(0, NewRuntime.Deck.HandLimit - NewRuntime.Deck.Hand.Num());
		// A party unit defeated last round must not contribute cards to this round's hand.
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
	}
	if (!ValidateCardBattleRuntimeInternal(NewRuntime, ValidationError))
	{
		return SetFailure(OutError, ValidationError);
	}
	InOutRuntime = MoveTemp(NewRuntime);
	OutEndPhaseDamageResults = MoveTemp(NewEndPhaseDamageResults);
	return true;
}
