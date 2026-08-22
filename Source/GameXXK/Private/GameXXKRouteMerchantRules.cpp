#include "GameXXKRouteMerchantRules.h"

#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKCardQualityRules.h"
#include "GameXXKMVPRules.h"

namespace
{
	const FName HeroMemberId(TEXT("Player"));

	bool SetError(FString* OutError, const FString& Error)
	{
		if (OutError) { *OutError = Error; }
		return false;
	}

	bool IsConcreteQuality(const EGameXXKCardQuality Quality)
	{
		return Quality == EGameXXKCardQuality::Common
			|| Quality == EGameXXKCardQuality::Rare
			|| Quality == EGameXXKCardQuality::Epic;
	}

	uint32 Mix32(uint32 Value)
	{
		Value ^= Value >> 16;
		Value *= 0x7FEB352DU;
		Value ^= Value >> 15;
		Value *= 0x846CA68BU;
		Value ^= Value >> 16;
		return Value;
	}

	uint32 DeriveStockRandomSeed(const int32 RootSeed, const int32 SourceNodeId, const int32 RefreshCount)
	{
		uint32 Value = Mix32(static_cast<uint32>(RootSeed) ^ 0x6D657263U);
		Value = Mix32(Value ^ static_cast<uint32>(SourceNodeId) ^ 0x9E3779B9U);
		Value = Mix32(Value ^ static_cast<uint32>(RefreshCount) ^ 0x85EBCA6BU);
		return Value == 0 ? 0xA341316CU : Value;
	}

	int32 DerivePersistedStockSeed(const int32 RootSeed, const int32 SourceNodeId, const int32 RefreshCount)
	{
		const int32 Candidate = static_cast<int32>(
			DeriveStockRandomSeed(RootSeed, SourceNodeId, RefreshCount) & 0x7FFFFFFFU);
		return Candidate == 0 ? 1 : Candidate;
	}

	uint32 NextRandom(uint32& State)
	{
		if (State == 0) { State = 0xA341316CU; }
		State ^= State << 13;
		State ^= State >> 17;
		State ^= State << 5;
		return State;
	}

	FName MakeOfferId(const int32 RootSeed, const int32 SourceNodeId, const int32 RefreshCount, const int32 SlotIndex)
	{
		return FName(*FString::Printf(
			TEXT("Merchant.%08X.%08X.%08X.C.%d"),
			static_cast<uint32>(RootSeed),
			static_cast<uint32>(SourceNodeId),
			static_cast<uint32>(RefreshCount),
			SlotIndex));
	}

	bool IsPendingPurchaseEmpty(const FGameXXKPendingRouteMerchantPurchase& Pending)
	{
		return !Pending.bActive && Pending.OfferId.IsNone() && Pending.CardId.IsNone() && Pending.Price == 0;
	}

	bool IsMerchantEmpty(const FGameXXKRouteMerchantState& Merchant)
	{
		return Merchant.SourceNodeId == INDEX_NONE
			&& Merchant.OfferSeed == 0
			&& Merchant.RefreshCount == 0
			&& Merchant.Offers.IsEmpty()
			&& IsPendingPurchaseEmpty(Merchant.PendingPurchase);
	}

	bool IsLegacyRelicSnapshot(const FGameXXKRouteMerchantState& Merchant)
	{
		return Merchant.Offers.Num() == 4
			&& !Merchant.Offers.ContainsByPredicate([](const FGameXXKRouteMerchantOffer& Offer)
			{
				return Offer.Kind != EGameXXKRouteMerchantOfferKind::Relic;
			});
	}

	bool ValidateRouteContext(
		const FGameXXKRuntimeState& State,
		const FGameXXKRouteMapNode*& OutMerchantNode,
		FString* OutError)
	{
		OutMerchantNode = nullptr;
		if (!State.bDungeonActive || !State.bHasGeneratedRouteMap
			|| !State.CardRun.bLoadoutLockedForRoute || !State.CardRun.bRouteEconomyInitialized
			|| State.Screen != EGameXXKScreen::RouteMerchant)
		{
			return SetError(OutError, TEXT("Route merchant requires an active locked generated route and the merchant screen."));
		}
		if (State.PlayerGold < 0 || State.CardRun.RouteTravelMoney < 0)
		{
			return SetError(OutError, TEXT("Route merchant cannot operate with a negative currency balance."));
		}
		if (State.PendingRouteNodeId == INDEX_NONE)
		{
			return SetError(OutError, TEXT("Route merchant requires a pending route node."));
		}
		OutMerchantNode = State.RouteMapNodes.FindByPredicate([&State](const FGameXXKRouteMapNode& Node)
		{
			return Node.NodeId == State.PendingRouteNodeId;
		});
		if (!OutMerchantNode || OutMerchantNode->NodeKind != EGameXXKNodeKind::Merchant)
		{
			OutMerchantNode = nullptr;
			return SetError(OutError, TEXT("The pending route node is not a generated merchant node."));
		}
		return true;
	}

	FGameXXKRouteMerchantOffer MakeUnavailableOffer(
		const int32 RootSeed,
		const int32 SourceNodeId,
		const int32 RefreshCount,
		const int32 SlotIndex)
	{
		FGameXXKRouteMerchantOffer Offer;
		Offer.OfferId = MakeOfferId(RootSeed, SourceNodeId, RefreshCount, SlotIndex);
		Offer.Kind = EGameXXKRouteMerchantOfferKind::Card;
		Offer.bUnavailable = true;
		return Offer;
	}

	bool FindDeployedMemberCards(
		const FGameXXKRuntimeState& State,
		const FName OwnerMemberId,
		const TArray<FName>*& OutCardIds)
	{
		OutCardIds = nullptr;
		if (OwnerMemberId == HeroMemberId)
		{
			OutCardIds = &State.CardRun.HeroSelectedCardIds;
			return true;
		}
		const FName ActiveCompanionId = State.CardRun.PartySelection.ActivePermanentCompanionInstanceId;
		if (!ActiveCompanionId.IsNone() && OwnerMemberId == ActiveCompanionId)
		{
			const FGameXXKPermanentCompanion* Companion =
				State.CardRun.CompanionRoster.PermanentCompanions.FindByPredicate(
					[OwnerMemberId](const FGameXXKPermanentCompanion& Candidate)
					{
						return Candidate.InstanceId == OwnerMemberId && Candidate.bIsActive;
					});
			if (Companion)
			{
				OutCardIds = &Companion->SelectedCardIds;
				return true;
			}
		}
		if (!State.CardRun.ActiveTemporaryQuestNpcId.IsNone()
			&& State.CardRun.ActiveTemporaryQuestNpcId == OwnerMemberId
			&& State.CardRun.PartySelection.QuestNpc.NpcId == OwnerMemberId)
		{
			OutCardIds = &State.CardRun.PartySelection.QuestNpc.SelectedCardIds;
			return true;
		}
		return false;
	}

	bool GenerateStock(
		const FGameXXKRuntimeState& State,
		const int32 SourceNodeId,
		const int32 RefreshCount,
		const FGameXXKRouteMerchantState* PreviousStock,
		const bool bRequireRerollCandidate,
		FGameXXKRouteMerchantState& OutMerchant,
		FString* OutError)
	{
		if (SourceNodeId < 0 || RefreshCount < 0)
		{
			return SetError(OutError, TEXT("Merchant stock requires a valid source node and non-negative refresh count."));
		}
		TArray<FGameXXKRouteMerchantRules::FDeployedCardCandidate> Pool;
		if (!FGameXXKRouteMerchantRules::BuildEffectiveDeployedCardPool(State, Pool, OutError))
		{
			return false;
		}
		Pool.Sort([](const auto& Left, const auto& Right)
		{
			return Left.CardId == Right.CardId
				? Left.OwnerMemberId.ToString() < Right.OwnerMemberId.ToString()
				: Left.CardId.ToString() < Right.CardId.ToString();
		});

		TSet<FName> PreservedSoldCardIds;
		int32 UnsoldSlotCount = FGameXXKRouteMerchantRules::CardSlotCount;
		if (PreviousStock)
		{
			UnsoldSlotCount = 0;
			for (const FGameXXKRouteMerchantOffer& Offer : PreviousStock->Offers)
			{
				if (Offer.bSold && !Offer.bUnavailable && !Offer.ContentId.IsNone())
				{
					PreservedSoldCardIds.Add(Offer.ContentId);
				}
				else
				{
					++UnsoldSlotCount;
				}
			}
			Pool.RemoveAll([&PreservedSoldCardIds](const auto& Candidate)
			{
				return PreservedSoldCardIds.Contains(Candidate.CardId);
			});
		}
		if (bRequireRerollCandidate && (UnsoldSlotCount <= 0 || Pool.IsEmpty()))
		{
			return SetError(OutError, TEXT("No unsold carried-card upgrade target is available to refresh."));
		}

		const int32 RootSeed = State.CardRun.RouteProgress.RootSeed;
		uint32 RandomState = DeriveStockRandomSeed(RootSeed, SourceNodeId, RefreshCount);
		FGameXXKRouteMerchantState Candidate;
		Candidate.SourceNodeId = SourceNodeId;
		Candidate.OfferSeed = DerivePersistedStockSeed(RootSeed, SourceNodeId, RefreshCount);
		Candidate.RefreshCount = RefreshCount;
		Candidate.Offers.Reserve(FGameXXKRouteMerchantRules::CardSlotCount);
		for (int32 SlotIndex = 0; SlotIndex < FGameXXKRouteMerchantRules::CardSlotCount; ++SlotIndex)
		{
			if (PreviousStock && PreviousStock->Offers.IsValidIndex(SlotIndex)
				&& PreviousStock->Offers[SlotIndex].bSold)
			{
				FGameXXKRouteMerchantOffer Sold = PreviousStock->Offers[SlotIndex];
				Sold.OfferId = MakeOfferId(RootSeed, SourceNodeId, RefreshCount, SlotIndex);
				Candidate.Offers.Add(MoveTemp(Sold));
				continue;
			}
			FGameXXKRouteMerchantOffer Offer = MakeUnavailableOffer(
				RootSeed, SourceNodeId, RefreshCount, SlotIndex);
			if (!Pool.IsEmpty())
			{
				const int32 PickIndex = static_cast<int32>(
					NextRandom(RandomState) % static_cast<uint32>(Pool.Num()));
				const FGameXXKRouteMerchantRules::FDeployedCardCandidate Picked = Pool[PickIndex];
				Pool.RemoveAt(PickIndex);
				Offer.bUnavailable = false;
				Offer.ContentId = Picked.CardId;
				Offer.OwnerMemberId = Picked.OwnerMemberId;
				Offer.Quality = Picked.CurrentQuality;
				Offer.NextQuality = FGameXXKCardBattleAdapter::GetNextCardQuality(Picked.CurrentQuality);
				Offer.Price = FGameXXKCardQualityRules::GetCardPrice(Offer.NextQuality);
			}
			Candidate.Offers.Add(MoveTemp(Offer));
		}
		if (Candidate.Offers.Num() != FGameXXKRouteMerchantRules::CardSlotCount)
		{
			return SetError(OutError, TEXT("Merchant stock generation did not produce all four card slots."));
		}
		OutMerchant = MoveTemp(Candidate);
		return true;
	}

	bool ValidateSavedStockCore(
		const FGameXXKRuntimeState& State,
		const FGameXXKRouteMerchantState& Merchant,
		FString* OutError)
	{
		if (IsMerchantEmpty(Merchant))
		{
			return true;
		}
		if (IsLegacyRelicSnapshot(Merchant))
		{
			if (Merchant.SourceNodeId < 0
				|| Merchant.RefreshCount < 0
				|| Merchant.OfferSeed != DerivePersistedStockSeed(
					State.CardRun.RouteProgress.RootSeed,
					Merchant.SourceNodeId,
					Merchant.RefreshCount)
				|| !IsPendingPurchaseEmpty(Merchant.PendingPurchase))
			{
				return SetError(OutError, TEXT("The legacy relic merchant snapshot has invalid persisted metadata."));
			}
			return true;
		}
		if (Merchant.SourceNodeId < 0 || Merchant.RefreshCount < 0
			|| Merchant.OfferSeed != DerivePersistedStockSeed(
				State.CardRun.RouteProgress.RootSeed, Merchant.SourceNodeId, Merchant.RefreshCount)
			|| Merchant.Offers.Num() != FGameXXKRouteMerchantRules::CardSlotCount)
		{
			return SetError(OutError, TEXT("The saved merchant stock metadata is incomplete or does not match its derived identity."));
		}
		TSet<FName> SeenOfferIds;
		TSet<FName> SeenCardIds;
		for (int32 Index = 0; Index < Merchant.Offers.Num(); ++Index)
		{
			const FGameXXKRouteMerchantOffer& Offer = Merchant.Offers[Index];
			if (Offer.Kind != EGameXXKRouteMerchantOfferKind::Card
				|| Offer.OfferId != MakeOfferId(
					State.CardRun.RouteProgress.RootSeed, Merchant.SourceNodeId, Merchant.RefreshCount, Index)
				|| Offer.OfferId.IsNone() || SeenOfferIds.Contains(Offer.OfferId))
			{
				return SetError(OutError, TEXT("The saved merchant stock contains an invalid or duplicate slot identity."));
			}
			SeenOfferIds.Add(Offer.OfferId);
			if (Offer.bUnavailable)
			{
				if (!Offer.ContentId.IsNone() || !Offer.OwnerMemberId.IsNone()
					|| Offer.Quality != EGameXXKCardQuality::Invalid
					|| Offer.NextQuality != EGameXXKCardQuality::Invalid
					|| Offer.Price != 0 || Offer.bSold)
				{
					return SetError(OutError, TEXT("A saved unavailable merchant slot has mutable payload or sold state."));
				}
				continue;
			}
			const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(Offer.ContentId);
			if (Offer.ContentId.IsNone() || Offer.OwnerMemberId.IsNone() || !Definition
				|| !IsConcreteQuality(Offer.Quality) || Offer.Quality >= EGameXXKCardQuality::Epic
				|| Offer.NextQuality != FGameXXKCardBattleAdapter::GetNextCardQuality(Offer.Quality)
				|| FGameXXKCardQualityRules::GetCardPrice(Offer.NextQuality) != Offer.Price
				|| SeenCardIds.Contains(Offer.ContentId))
			{
				return SetError(OutError, TEXT("A saved merchant card offer violates definition, owner, quality, price, or uniqueness rules."));
			}
			SeenCardIds.Add(Offer.ContentId);
		}
		if (!IsPendingPurchaseEmpty(Merchant.PendingPurchase))
		{
			return SetError(OutError, TEXT("Carried-card upgrades never persist a replacement transaction."));
		}
		return true;
	}

	bool ValidateStoredStock(
		const FGameXXKRuntimeState& State,
		const FGameXXKRouteMerchantState& Merchant,
		FString* OutError)
	{
		if (Merchant.SourceNodeId != State.PendingRouteNodeId)
		{
			return SetError(OutError, TEXT("The saved merchant stock does not belong to the active pending merchant."));
		}
		return ValidateSavedStockCore(State, Merchant, OutError);
	}

	bool SetPurchaseFailure(
		FGameXXKRouteMerchantPurchasePreview& OutPreview,
		const EGameXXKRouteMerchantPurchaseFailure Failure,
		const FString& Reason,
		FString* OutError)
	{
		OutPreview.bCanPurchase = false;
		OutPreview.Failure = Failure;
		OutPreview.FailureReason = Reason;
		if (OutError) { *OutError = Reason; }
		return false;
	}

	bool BuildPurchasePreview(
		const FGameXXKRuntimeState& State,
		const FName OfferId,
		const FName ReplacementEntryId,
		FGameXXKRouteMerchantPurchasePreview& OutPreview,
		FString* OutError)
	{
		OutPreview = FGameXXKRouteMerchantPurchasePreview();
		if (OutError) { OutError->Reset(); }
		const FGameXXKRouteMapNode* MerchantNode = nullptr;
		FString ValidationError;
		if (!ValidateRouteContext(State, MerchantNode, &ValidationError))
		{
			return SetPurchaseFailure(OutPreview, EGameXXKRouteMerchantPurchaseFailure::InvalidRouteContext, ValidationError, OutError);
		}
		if (!ValidateStoredStock(State, State.CardRun.RouteMerchant, &ValidationError)
			|| IsLegacyRelicSnapshot(State.CardRun.RouteMerchant))
		{
			return SetPurchaseFailure(OutPreview, EGameXXKRouteMerchantPurchaseFailure::InvalidMerchantStock,
				ValidationError.IsEmpty() ? TEXT("Legacy merchant stock must normalize before purchase.") : ValidationError, OutError);
		}
		const FGameXXKRouteMerchantOffer* Offer = State.CardRun.RouteMerchant.Offers.FindByPredicate(
			[OfferId](const FGameXXKRouteMerchantOffer& Candidate) { return Candidate.OfferId == OfferId; });
		if (!Offer)
		{
			return SetPurchaseFailure(OutPreview, EGameXXKRouteMerchantPurchaseFailure::StaleOfferId,
				TEXT("The merchant offer id is stale or unknown."), OutError);
		}
		OutPreview.Offer = *Offer;
		OutPreview.BalanceBefore = State.PlayerGold;
		OutPreview.BalanceAfter = State.PlayerGold;
		OutPreview.Price = Offer->Price;
		if (Offer->bUnavailable)
		{
			return SetPurchaseFailure(OutPreview, EGameXXKRouteMerchantPurchaseFailure::OfferUnavailable,
				TEXT("No upgradable carried card is available for this slot."), OutError);
		}
		if (Offer->bSold)
		{
			return SetPurchaseFailure(OutPreview, EGameXXKRouteMerchantPurchaseFailure::OfferAlreadySold,
				TEXT("This merchant offer was already sold."), OutError);
		}
		if (Offer->Kind != EGameXXKRouteMerchantOfferKind::Card)
		{
			return SetPurchaseFailure(OutPreview, EGameXXKRouteMerchantPurchaseFailure::InvalidMerchantStock,
				TEXT("Route merchants only stock carried-card upgrades."), OutError);
		}
		if (!ReplacementEntryId.IsNone())
		{
			return SetPurchaseFailure(OutPreview, EGameXXKRouteMerchantPurchaseFailure::InvalidReplacementEntryId,
				TEXT("Carried-card upgrades never accept a replacement EntryId."), OutError);
		}
		if (State.PlayerGold < Offer->Price)
		{
			return SetPurchaseFailure(OutPreview, EGameXXKRouteMerchantPurchaseFailure::InsufficientOrdinaryGold,
				FString::Printf(TEXT("Ordinary gold is short by %d."), Offer->Price - State.PlayerGold), OutError);
		}
		const TArray<FName>* OwnerCards = nullptr;
		if (!FindDeployedMemberCards(State, Offer->OwnerMemberId, OwnerCards))
		{
			return SetPurchaseFailure(OutPreview, EGameXXKRouteMerchantPurchaseFailure::OwnerNoLongerDeployed,
				TEXT("The card owner is no longer deployed."), OutError);
		}
		if (!OwnerCards || !OwnerCards->Contains(Offer->ContentId))
		{
			return SetPurchaseFailure(OutPreview, EGameXXKRouteMerchantPurchaseFailure::CardNoLongerCarried,
				TEXT("The deployed owner no longer carries this card."), OutError);
		}
		const EGameXXKCardQuality CurrentQuality =
			FGameXXKCardBattleAdapter::GetConfiguredCardQuality(State.CardRun, Offer->ContentId);
		if (CurrentQuality >= EGameXXKCardQuality::Epic)
		{
			return SetPurchaseFailure(OutPreview, EGameXXKRouteMerchantPurchaseFailure::CardAlreadyMaxQuality,
				TEXT("The carried card already reached Epic quality."), OutError);
		}
		if (CurrentQuality != Offer->Quality)
		{
			return SetPurchaseFailure(OutPreview, EGameXXKRouteMerchantPurchaseFailure::StaleCardQuality,
				TEXT("The carried card quality changed after stock generation."), OutError);
		}
		OutPreview.BalanceAfter = State.PlayerGold - Offer->Price;
		OutPreview.FinalQuality = Offer->NextQuality;
		OutPreview.bCanPurchase = true;
		return true;
	}

	void CopyPreviewToResult(
		const FGameXXKRouteMerchantPurchasePreview& Preview,
		const FName ReplacementEntryId,
		FGameXXKRouteMerchantPurchaseResult& OutResult)
	{
		OutResult = FGameXXKRouteMerchantPurchaseResult();
		OutResult.bRequiresReplacement = false;
		OutResult.Offer = Preview.Offer;
		OutResult.OfferId = Preview.Offer.OfferId;
		OutResult.CardId = Preview.Offer.ContentId;
		OutResult.BalanceBefore = Preview.BalanceBefore;
		OutResult.BalanceAfter = Preview.BalanceAfter;
		OutResult.Price = Preview.Price;
		OutResult.FinalQuality = Preview.FinalQuality;
		OutResult.ReplacementEntryId = ReplacementEntryId;
		OutResult.Failure = Preview.Failure;
		OutResult.FailureReason = Preview.FailureReason;
	}
}

bool FGameXXKRouteMerchantRules::BuildEffectiveDeployedCardPool(
	const FGameXXKRuntimeState& State,
	TArray<FDeployedCardCandidate>& OutCandidates,
	FString* OutError)
{
	OutCandidates.Reset();
	if (OutError) { OutError->Reset(); }
	TSet<FName> SeenCardIds;
	auto AppendConfiguredCards = [&](const FName OwnerMemberId, const TArray<FName>& CardIds)
	{
		if (OwnerMemberId.IsNone()) { return; }
		for (const FName CardId : CardIds)
		{
			if (CardId.IsNone() || SeenCardIds.Contains(CardId)
				|| !FGameXXKCardCatalog::FindCardDefinition(CardId))
			{
				continue;
			}
			const EGameXXKCardQuality CurrentQuality =
				FGameXXKCardBattleAdapter::GetConfiguredCardQuality(State.CardRun, CardId);
			if (!IsConcreteQuality(CurrentQuality) || CurrentQuality >= EGameXXKCardQuality::Epic)
			{
				continue;
			}
			SeenCardIds.Add(CardId);
			FDeployedCardCandidate Candidate;
			Candidate.OwnerMemberId = OwnerMemberId;
			Candidate.CardId = CardId;
			Candidate.CurrentQuality = CurrentQuality;
			OutCandidates.Add(MoveTemp(Candidate));
		}
	};
	AppendConfiguredCards(HeroMemberId, State.CardRun.HeroSelectedCardIds);
	const FName ActiveCompanionId = State.CardRun.PartySelection.ActivePermanentCompanionInstanceId;
	if (!ActiveCompanionId.IsNone())
	{
		if (const FGameXXKPermanentCompanion* Companion =
			State.CardRun.CompanionRoster.PermanentCompanions.FindByPredicate(
				[ActiveCompanionId](const FGameXXKPermanentCompanion& Candidate)
				{
					return Candidate.InstanceId == ActiveCompanionId && Candidate.bIsActive;
				}))
		{
			AppendConfiguredCards(Companion->InstanceId, Companion->SelectedCardIds);
		}
	}
	if (!State.CardRun.ActiveTemporaryQuestNpcId.IsNone()
		&& State.CardRun.ActiveTemporaryQuestNpcId == State.CardRun.PartySelection.QuestNpc.NpcId)
	{
		AppendConfiguredCards(
			State.CardRun.ActiveTemporaryQuestNpcId,
			State.CardRun.PartySelection.QuestNpc.SelectedCardIds);
	}
	return true;
}

int32 FGameXXKRouteMerchantRules::GetRefreshCost(const int32 RefreshCount)
{
	if (RefreshCount < 0) { return 0; }
	if (RefreshCount == 0) return 20;
	if (RefreshCount == 1) return 30;
	if (RefreshCount == 2) return 40;
	return 50;
}

bool FGameXXKRouteMerchantRules::ValidateSavedStock(const FGameXXKRuntimeState& State, FString* OutError)
{
	if (OutError) { OutError->Reset(); }
	return ValidateSavedStockCore(State, State.CardRun.RouteMerchant, OutError);
}

bool FGameXXKRouteMerchantRules::EnsureStock(FGameXXKRuntimeState& InOutState, FString* OutError)
{
	if (OutError) { OutError->Reset(); }
	const FGameXXKRouteMapNode* MerchantNode = nullptr;
	if (!ValidateRouteContext(InOutState, MerchantNode, OutError)) { return false; }
	const FGameXXKRouteMerchantState& Existing = InOutState.CardRun.RouteMerchant;
	if (IsLegacyRelicSnapshot(Existing)
		&& !ValidateSavedStockCore(InOutState, Existing, OutError))
	{
		return false;
	}
	if (Existing.SourceNodeId == MerchantNode->NodeId && !IsLegacyRelicSnapshot(Existing))
	{
		return ValidateStoredStock(InOutState, Existing, OutError);
	}
	if (Existing.SourceNodeId == INDEX_NONE && !IsMerchantEmpty(Existing))
	{
		return SetError(OutError, TEXT("The empty merchant snapshot contains partial persisted metadata."));
	}
	if (Existing.PendingPurchase.bActive && !IsLegacyRelicSnapshot(Existing))
	{
		return SetError(OutError, TEXT("A pending merchant purchase prevents opening a different merchant node."));
	}
	const int32 RefreshCount = Existing.SourceNodeId == MerchantNode->NodeId
		? FMath::Max(0, Existing.RefreshCount) : 0;
	FGameXXKRouteMerchantState Generated;
	if (!GenerateStock(InOutState, MerchantNode->NodeId, RefreshCount, nullptr, false, Generated, OutError))
	{
		return false;
	}
	FGameXXKRuntimeState Candidate = InOutState;
	Candidate.CardRun.RouteMerchant = MoveTemp(Generated);
	InOutState = MoveTemp(Candidate);
	return true;
}

bool FGameXXKRouteMerchantRules::GetView(
	FGameXXKRuntimeState& InOutState,
	FGameXXKRouteMerchantView& OutView,
	FString* OutError)
{
	if (!EnsureStock(InOutState, OutError))
	{
		OutView = FGameXXKRouteMerchantView();
		return false;
	}
	return GetView(static_cast<const FGameXXKRuntimeState&>(InOutState), OutView, OutError);
}

bool FGameXXKRouteMerchantRules::GetView(
	const FGameXXKRuntimeState& State,
	FGameXXKRouteMerchantView& OutView,
	FString* OutError)
{
	OutView = FGameXXKRouteMerchantView();
	if (OutError) { OutError->Reset(); }
	const FGameXXKRouteMapNode* MerchantNode = nullptr;
	if (!ValidateRouteContext(State, MerchantNode, OutError)
		|| !ValidateStoredStock(State, State.CardRun.RouteMerchant, OutError)
		|| IsLegacyRelicSnapshot(State.CardRun.RouteMerchant))
	{
		return false;
	}
	const FGameXXKRouteMerchantState& Merchant = State.CardRun.RouteMerchant;
	OutView.PlayerGold = State.PlayerGold;
	OutView.RouteTravelMoney = State.PlayerGold;
	OutView.RefreshCost = GetRefreshCost(Merchant.RefreshCount);
	OutView.bRefreshAffordable = OutView.RefreshCost > 0 && State.PlayerGold >= OutView.RefreshCost;
	OutView.bRefreshEnabled = OutView.bRefreshAffordable
		&& !Merchant.PendingPurchase.bActive
		&& Merchant.RefreshCount < MAX_int32
		&& Merchant.Offers.ContainsByPredicate([](const FGameXXKRouteMerchantOffer& Offer)
		{
			return !Offer.bSold;
		});
	if (Merchant.PendingPurchase.bActive)
	{
		OutView.RefreshDisabledReason = TEXT("请先完成或取消当前卡牌替换");
	}
	else if (Merchant.RefreshCount == MAX_int32)
	{
		OutView.RefreshDisabledReason = TEXT("刷新次数已达上限");
	}
	else if (!OutView.bRefreshAffordable)
	{
		OutView.RefreshDisabledReason = FString::Printf(
			TEXT("金币不足，还差%d"), FMath::Max(0, OutView.RefreshCost - State.PlayerGold));
	}
	else if (!OutView.bRefreshEnabled)
	{
		OutView.RefreshDisabledReason = TEXT("没有可刷新的卡牌");
	}
	OutView.bHasPendingReplacement = false;
	OutView.bCanLeave = true;
	for (const FGameXXKRouteMerchantOffer& Offer : Merchant.Offers)
	{
		FGameXXKRouteMerchantOfferView OfferView;
		OfferView.SavedOffer = Offer;
		OfferView.bAffordable = !Offer.bUnavailable && Offer.Price > 0 && State.PlayerGold >= Offer.Price;
		OfferView.bPurchaseEnabled = OfferView.bAffordable && !Offer.bSold;
		if (Offer.bUnavailable) { OfferView.DisabledReason = TEXT("没有可强化卡牌"); }
		else if (Offer.bSold) { OfferView.DisabledReason = TEXT("已售"); }
		else if (!OfferView.bAffordable)
		{
			OfferView.DisabledReason = FString::Printf(
				TEXT("金币不足，还差%d"), FMath::Max(0, Offer.Price - State.PlayerGold));
		}
		OutView.CardOffers.Add(MoveTemp(OfferView));
	}
	return OutView.CardOffers.Num() == CardSlotCount && OutView.RelicOffers.Num() == RelicSlotCount;
}

bool FGameXXKRouteMerchantRules::Refresh(FGameXXKRuntimeState& InOutState, FString* OutError)
{
	if (OutError) { OutError->Reset(); }
	FGameXXKRuntimeState Candidate = InOutState;
	const FGameXXKRouteMapNode* MerchantNode = nullptr;
	if (!ValidateRouteContext(Candidate, MerchantNode, OutError))
	{
		return false;
	}
	if (Candidate.CardRun.RouteMerchant.SourceNodeId == MerchantNode->NodeId
		&& Candidate.CardRun.RouteMerchant.RefreshCount == MAX_int32)
	{
		return SetError(OutError, TEXT("Merchant refresh count cannot be safely incremented."));
	}
	if (!EnsureStock(Candidate, OutError)) { return false; }
	const FGameXXKRouteMerchantState Existing = Candidate.CardRun.RouteMerchant;
	if (Existing.PendingPurchase.bActive)
	{
		return SetError(OutError, TEXT("Merchant stock cannot refresh while a card replacement is pending."));
	}
	if (Existing.RefreshCount == MAX_int32)
	{
		return SetError(OutError, TEXT("Merchant refresh count cannot be safely incremented."));
	}
	const int32 Cost = GetRefreshCost(Existing.RefreshCount);
	if (Cost <= 0 || Candidate.PlayerGold < Cost)
	{
		return SetError(OutError, TEXT("There is not enough ordinary gold to refresh the merchant."));
	}
	FGameXXKRouteMerchantState Refreshed;
	if (!GenerateStock(Candidate, MerchantNode->NodeId, Existing.RefreshCount + 1,
		&Existing, true, Refreshed, OutError))
	{
		return false;
	}
	Candidate.CardRun.RouteMerchant = MoveTemp(Refreshed);
	Candidate.PlayerGold -= Cost;
	InOutState = MoveTemp(Candidate);
	return true;
}

bool FGameXXKRouteMerchantRules::PreviewPurchase(
	const FGameXXKRuntimeState& State,
	const FName OfferId,
	const FName ReplacementEntryId,
	FGameXXKRouteMerchantPurchasePreview& OutPreview,
	FString* OutError)
{
	return BuildPurchasePreview(State, OfferId, ReplacementEntryId, OutPreview, OutError);
}

bool FGameXXKRouteMerchantRules::Purchase(
	FGameXXKRuntimeState& InOutState,
	const FName OfferId,
	const FName ReplacementEntryId,
	FGameXXKRouteMerchantPurchaseResult& OutResult)
{
	FGameXXKRouteMerchantPurchasePreview Preview;
	FString PreviewError;
	if (!BuildPurchasePreview(InOutState, OfferId, ReplacementEntryId, Preview, &PreviewError))
	{
		CopyPreviewToResult(Preview, ReplacementEntryId, OutResult);
		return false;
	}
	CopyPreviewToResult(Preview, ReplacementEntryId, OutResult);
	FGameXXKRuntimeState Candidate = InOutState;
	FGameXXKRouteMerchantPurchasePreview CommitPreview;
	FString CommitError;
	if (!BuildPurchasePreview(Candidate, OfferId, ReplacementEntryId, CommitPreview, &CommitError))
	{
		CopyPreviewToResult(CommitPreview, ReplacementEntryId, OutResult);
		return false;
	}
	FGameXXKRouteMerchantOffer* CandidateOffer = Candidate.CardRun.RouteMerchant.Offers.FindByPredicate(
		[OfferId](const FGameXXKRouteMerchantOffer& Offer) { return Offer.OfferId == OfferId; });
	if (!CandidateOffer || CandidateOffer->bSold || CandidateOffer->bUnavailable
		|| Candidate.PlayerGold < CandidateOffer->Price)
	{
		OutResult.Failure = EGameXXKRouteMerchantPurchaseFailure::InvalidMerchantStock;
		OutResult.FailureReason = TEXT("The candidate merchant offer changed before commit.");
		return false;
	}
	Candidate.PlayerGold -= CandidateOffer->Price;
	Candidate.CardRun.UpgradedCardQualities.Add(CandidateOffer->ContentId, CandidateOffer->NextQuality);
	CandidateOffer->bSold = true;
	Candidate.CardRun.RouteMerchant.PendingPurchase = FGameXXKPendingRouteMerchantPurchase();
	OutResult.bPurchased = true;
	OutResult.BalanceAfter = Candidate.PlayerGold;
	OutResult.FinalQuality = CandidateOffer->NextQuality;
	OutResult.Failure = EGameXXKRouteMerchantPurchaseFailure::None;
	OutResult.FailureReason.Reset();
	InOutState = MoveTemp(Candidate);
	return true;
}

bool FGameXXKRouteMerchantRules::CancelPendingPurchase(FGameXXKRuntimeState& InOutState, FString* OutError)
{
	if (OutError) { OutError->Reset(); }
	const FGameXXKRouteMapNode* MerchantNode = nullptr;
	if (!ValidateRouteContext(InOutState, MerchantNode, OutError)
		|| !ValidateStoredStock(InOutState, InOutState.CardRun.RouteMerchant, OutError))
	{
		return false;
	}
	FGameXXKRuntimeState Candidate = InOutState;
	Candidate.CardRun.RouteMerchant.PendingPurchase = FGameXXKPendingRouteMerchantPurchase();
	InOutState = MoveTemp(Candidate);
	return true;
}
