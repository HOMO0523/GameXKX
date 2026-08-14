#include "GameXXKRouteMerchantRules.h"

#include "GameXXKCardQualityRules.h"
#include "GameXXKMVPRules.h"
#include "GameXXKRelicCatalog.h"
#include "GameXXKRelicRules.h"

namespace
{
	bool SetError(FString* OutError, const FString& Error)
	{
		if (OutError)
		{
			*OutError = Error;
		}
		return false;
	}

	bool IsConcreteQuality(const EGameXXKCardQuality Quality)
	{
		return Quality == EGameXXKCardQuality::Common
			|| Quality == EGameXXKCardQuality::Rare
			|| Quality == EGameXXKCardQuality::Epic;
	}

	bool NameLess(const FName Left, const FName Right)
	{
		return Left.ToString() < Right.ToString();
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
		const int32 Candidate = static_cast<int32>(DeriveStockRandomSeed(RootSeed, SourceNodeId, RefreshCount) & 0x7FFFFFFFU);
		return Candidate == 0 ? 1 : Candidate;
	}

	uint32 NextRandom(uint32& State)
	{
		if (State == 0)
		{
			State = 0xA341316CU;
		}
		State ^= State << 13;
		State ^= State >> 17;
		State ^= State << 5;
		return State;
	}

	FName MakeOfferId(
		const int32 RootSeed,
		const int32 SourceNodeId,
		const int32 RefreshCount,
		const EGameXXKRouteMerchantOfferKind Kind,
		const int32 SlotIndex)
	{
		const TCHAR KindCode = Kind == EGameXXKRouteMerchantOfferKind::Card ? TEXT('C') : TEXT('R');
		return FName(*FString::Printf(
			TEXT("Merchant.%08X.%08X.%08X.%c.%d"),
			static_cast<uint32>(RootSeed),
			static_cast<uint32>(SourceNodeId),
			static_cast<uint32>(RefreshCount),
			KindCode,
			SlotIndex));
	}

	bool IsPendingPurchaseEmpty(const FGameXXKPendingRouteMerchantPurchase& Pending)
	{
		return !Pending.bActive
			&& Pending.OfferId.IsNone()
			&& Pending.CardId.IsNone()
			&& Pending.Price == 0;
	}

	bool IsMerchantEmpty(const FGameXXKRouteMerchantState& Merchant)
	{
		return Merchant.SourceNodeId == INDEX_NONE
			&& Merchant.OfferSeed == 0
			&& Merchant.RefreshCount == 0
			&& Merchant.Offers.IsEmpty()
			&& IsPendingPurchaseEmpty(Merchant.PendingPurchase);
	}

	bool ValidateRouteContext(const FGameXXKRuntimeState& State, const FGameXXKRouteMapNode*& OutMerchantNode, FString* OutError)
	{
		OutMerchantNode = nullptr;
		if (!State.bDungeonActive
			|| !State.bHasGeneratedRouteMap
			|| !State.CardRun.bLoadoutLockedForRoute
			|| !State.CardRun.bRouteEconomyInitialized
			|| State.Screen != EGameXXKScreen::RouteMerchant)
		{
			return SetError(OutError, TEXT("Route merchant requires an active locked generated route and the merchant screen."));
		}
		if (State.CardRun.RouteTravelMoney < 0)
		{
			return SetError(OutError, TEXT("Route merchant cannot operate with a negative route-travel-money balance."));
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

	bool PickUniqueId(
		const TArray<FName>& Pool,
		const TSet<FName>& AlreadySelected,
		uint32& InOutRandomState,
		FName& OutId)
	{
		TArray<FName> Legal;
		Legal.Reserve(Pool.Num());
		for (const FName Id : Pool)
		{
			if (!AlreadySelected.Contains(Id))
			{
				Legal.Add(Id);
			}
		}
		if (Legal.IsEmpty())
		{
			OutId = NAME_None;
			return false;
		}
		const int32 PickIndex = static_cast<int32>(NextRandom(InOutRandomState) % static_cast<uint32>(Legal.Num()));
		OutId = Legal[PickIndex];
		return true;
	}

	FGameXXKRouteMerchantOffer MakeUnavailableOffer(
		const int32 RootSeed,
		const int32 SourceNodeId,
		const int32 RefreshCount,
		const EGameXXKRouteMerchantOfferKind Kind,
		const int32 SlotIndex)
	{
		FGameXXKRouteMerchantOffer Offer;
		Offer.OfferId = MakeOfferId(RootSeed, SourceNodeId, RefreshCount, Kind, SlotIndex);
		Offer.Kind = Kind;
		Offer.Quality = EGameXXKCardQuality::Invalid;
		Offer.bUnavailable = true;
		return Offer;
	}

	bool BuildRelicOffer(
		const int32 RootSeed,
		const int32 SourceNodeId,
		const int32 RefreshCount,
		const int32 SlotIndex,
		const FName RelicId,
		FGameXXKRouteMerchantOffer& OutOffer,
		FString* OutError)
	{
		if (RelicId.IsNone())
		{
			OutOffer = MakeUnavailableOffer(
				RootSeed,
				SourceNodeId,
				RefreshCount,
				EGameXXKRouteMerchantOfferKind::Relic,
				SlotIndex);
			return true;
		}
		const FGameXXKRelicDefinition* Definition = FGameXXKRelicCatalog::FindDefinition(RelicId);
		if (!Definition || !IsConcreteQuality(Definition->BaseQuality))
		{
			return SetError(OutError, TEXT("Merchant relic generation resolved an invalid catalog definition."));
		}
		const int32 Price = FGameXXKCardQualityRules::GetRelicPrice(Definition->BaseQuality);
		if (Price <= 0)
		{
			return SetError(OutError, TEXT("Merchant relic generation resolved an invalid quality price."));
		}
		OutOffer = FGameXXKRouteMerchantOffer();
		OutOffer.OfferId = MakeOfferId(
			RootSeed,
			SourceNodeId,
			RefreshCount,
			EGameXXKRouteMerchantOfferKind::Relic,
			SlotIndex);
		OutOffer.Kind = EGameXXKRouteMerchantOfferKind::Relic;
		OutOffer.ContentId = Definition->Id;
		OutOffer.Quality = Definition->BaseQuality;
		OutOffer.Price = Price;
		return true;
	}

	bool GenerateStock(
		const FGameXXKRuntimeState& State,
		const int32 SourceNodeId,
		const int32 RefreshCount,
		FGameXXKRouteMerchantState& OutMerchant,
		FString* OutError)
	{
		if (SourceNodeId < 0 || RefreshCount < 0)
		{
			return SetError(OutError, TEXT("Merchant stock requires a valid source node and non-negative refresh count."));
		}

		TSet<FName> OwnedRelicIds;
		for (const FGameXXKRelicInstance& Owned : State.CardRun.Relics)
		{
			if (!Owned.RelicId.IsNone())
			{
				OwnedRelicIds.Add(Owned.RelicId);
			}
		}
		TArray<FName> RelicPool;
		for (const FGameXXKRelicDefinition& Definition : FGameXXKRelicCatalog::GetAllDefinitions())
		{
			if (!Definition.Id.IsNone()
				&& !OwnedRelicIds.Contains(Definition.Id)
				&& IsConcreteQuality(Definition.BaseQuality)
				&& FGameXXKCardQualityRules::GetRelicPrice(Definition.BaseQuality) > 0)
			{
				RelicPool.Add(Definition.Id);
			}
		}
		RelicPool.Sort(NameLess);

		const int32 RootSeed = State.CardRun.RouteProgress.RootSeed;
		uint32 RandomState = DeriveStockRandomSeed(RootSeed, SourceNodeId, RefreshCount);
		FGameXXKRouteMerchantState Candidate;
		Candidate.SourceNodeId = SourceNodeId;
		Candidate.OfferSeed = DerivePersistedStockSeed(RootSeed, SourceNodeId, RefreshCount);
		Candidate.RefreshCount = RefreshCount;
		Candidate.Offers.Reserve(FGameXXKRouteMerchantRules::RelicSlotCount);

		TSet<FName> SelectedRelicIds;
		for (int32 SlotIndex = 0; SlotIndex < FGameXXKRouteMerchantRules::RelicSlotCount; ++SlotIndex)
		{
			FName RelicId;
			PickUniqueId(RelicPool, SelectedRelicIds, RandomState, RelicId);
			if (!RelicId.IsNone())
			{
				SelectedRelicIds.Add(RelicId);
			}
			FGameXXKRouteMerchantOffer Offer;
			if (!BuildRelicOffer(RootSeed, SourceNodeId, RefreshCount, SlotIndex, RelicId, Offer, OutError))
			{
				return false;
			}
			Candidate.Offers.Add(MoveTemp(Offer));
		}

		if (Candidate.Offers.Num() != FGameXXKRouteMerchantRules::RelicSlotCount)
		{
			return SetError(OutError, TEXT("Merchant stock generation did not produce all four relic slots."));
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
		if (Merchant.SourceNodeId < 0
			|| Merchant.RefreshCount < 0
			|| Merchant.OfferSeed != DerivePersistedStockSeed(
				State.CardRun.RouteProgress.RootSeed,
				Merchant.SourceNodeId,
				Merchant.RefreshCount)
			|| Merchant.Offers.Num() != FGameXXKRouteMerchantRules::RelicSlotCount)
		{
			return SetError(OutError, TEXT("The saved merchant stock metadata is incomplete or does not match its derived identity."));
		}

		TSet<FName> SeenOfferIds;
		TSet<FName> SeenRelicIds;
		for (int32 Index = 0; Index < Merchant.Offers.Num(); ++Index)
		{
			const FGameXXKRouteMerchantOffer& Offer = Merchant.Offers[Index];
			const EGameXXKRouteMerchantOfferKind ExpectedKind = EGameXXKRouteMerchantOfferKind::Relic;
			if (Offer.Kind != ExpectedKind
				|| Offer.OfferId != MakeOfferId(
					State.CardRun.RouteProgress.RootSeed,
					Merchant.SourceNodeId,
					Merchant.RefreshCount,
					ExpectedKind,
					Index)
				|| Offer.OfferId.IsNone()
				|| SeenOfferIds.Contains(Offer.OfferId))
			{
				return SetError(OutError, TEXT("The saved merchant stock contains an invalid or duplicate slot identity."));
			}
			SeenOfferIds.Add(Offer.OfferId);

			if (Offer.bUnavailable)
			{
				if (!Offer.ContentId.IsNone()
					|| Offer.Quality != EGameXXKCardQuality::Invalid
					|| Offer.Price != 0
					|| Offer.bSold)
				{
					return SetError(OutError, TEXT("A saved unavailable merchant slot has mutable content, quality, price, or sold state."));
				}
				continue;
			}

			if (Offer.ContentId.IsNone() || !IsConcreteQuality(Offer.Quality) || Offer.Price <= 0)
			{
				return SetError(OutError, TEXT("A saved available merchant offer is incomplete."));
			}
			const FGameXXKRelicDefinition* Definition = FGameXXKRelicCatalog::FindDefinition(Offer.ContentId);
			if (!Definition
				|| Definition->BaseQuality != Offer.Quality
				|| FGameXXKCardQualityRules::GetRelicPrice(Offer.Quality) != Offer.Price
				|| SeenRelicIds.Contains(Offer.ContentId))
			{
				return SetError(OutError, TEXT("A saved merchant relic offer violates catalog, quality, price, or uniqueness rules."));
			}
			SeenRelicIds.Add(Offer.ContentId);
		}

		const FGameXXKPendingRouteMerchantPurchase& Pending = Merchant.PendingPurchase;
		if (!IsPendingPurchaseEmpty(Pending))
		{
			return SetError(OutError, TEXT("Relic-only merchant purchases never persist a pending card-replacement transaction."));
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
		if (OutError)
		{
			*OutError = Reason;
		}
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
		if (OutError)
		{
			OutError->Reset();
		}
		const FGameXXKRouteMapNode* MerchantNode = nullptr;
		FString ValidationError;
		if (!ValidateRouteContext(State, MerchantNode, &ValidationError))
		{
			return SetPurchaseFailure(OutPreview, EGameXXKRouteMerchantPurchaseFailure::InvalidRouteContext, ValidationError, OutError);
		}
		if (!ValidateStoredStock(State, State.CardRun.RouteMerchant, &ValidationError))
		{
			return SetPurchaseFailure(OutPreview, EGameXXKRouteMerchantPurchaseFailure::InvalidMerchantStock, ValidationError, OutError);
		}

		const FGameXXKRouteMerchantOffer* Offer = State.CardRun.RouteMerchant.Offers.FindByPredicate([OfferId](const FGameXXKRouteMerchantOffer& Candidate)
		{
			return Candidate.OfferId == OfferId;
		});
		if (!Offer)
		{
			return SetPurchaseFailure(
				OutPreview,
				EGameXXKRouteMerchantPurchaseFailure::StaleOfferId,
				TEXT("The merchant offer id is stale or unknown."),
				OutError);
		}

		OutPreview.Offer = *Offer;
		OutPreview.BalanceBefore = State.CardRun.RouteTravelMoney;
		OutPreview.BalanceAfter = State.CardRun.RouteTravelMoney;
		OutPreview.Price = Offer->Price;
		if (Offer->bUnavailable)
		{
			return SetPurchaseFailure(
				OutPreview,
				EGameXXKRouteMerchantPurchaseFailure::OfferUnavailable,
				TEXT("This merchant slot is unavailable because every legal item was collected."),
				OutError);
		}
		if (Offer->bSold)
		{
			return SetPurchaseFailure(
				OutPreview,
				EGameXXKRouteMerchantPurchaseFailure::OfferAlreadySold,
				TEXT("This merchant offer was already sold."),
				OutError);
		}
		if (State.CardRun.RouteTravelMoney < Offer->Price)
		{
			return SetPurchaseFailure(
				OutPreview,
				EGameXXKRouteMerchantPurchaseFailure::InsufficientTravelMoney,
				FString::Printf(TEXT("Route travel money is short by %d."), Offer->Price - State.CardRun.RouteTravelMoney),
				OutError);
		}
		OutPreview.BalanceAfter = State.CardRun.RouteTravelMoney - Offer->Price;

		if (Offer->Kind == EGameXXKRouteMerchantOfferKind::Relic)
		{
			if (!ReplacementEntryId.IsNone())
			{
				return SetPurchaseFailure(
					OutPreview,
					EGameXXKRouteMerchantPurchaseFailure::InvalidReplacementEntryId,
					TEXT("Relic purchases never accept a replacement EntryId."),
					OutError);
			}
			if (State.CardRun.Relics.ContainsByPredicate([Offer](const FGameXXKRelicInstance& Instance)
			{
				return Instance.RelicId == Offer->ContentId;
			}))
			{
				return SetPurchaseFailure(
					OutPreview,
					EGameXXKRouteMerchantPurchaseFailure::DuplicateRelic,
					TEXT("Route relics are unique and this relic is already owned."),
					OutError);
			}
			const FGameXXKRelicDefinition* Definition = FGameXXKRelicCatalog::FindDefinition(Offer->ContentId);
			if (!Definition || Definition->BaseQuality != Offer->Quality)
			{
				return SetPurchaseFailure(
					OutPreview,
					EGameXXKRouteMerchantPurchaseFailure::InvalidMerchantStock,
					TEXT("The merchant relic no longer matches the immutable catalog."),
					OutError);
			}
			if (State.CardRun.NextRelicAcquisitionOrdinal == MAX_int32)
			{
				return SetPurchaseFailure(
					OutPreview,
					EGameXXKRouteMerchantPurchaseFailure::ArithmeticOverflow,
					TEXT("The next relic acquisition ordinal cannot be safely incremented."),
					OutError);
			}
			OutPreview.FinalQuality = Offer->Quality;
			OutPreview.bCanPurchase = true;
			return true;
		}

		return SetPurchaseFailure(
			OutPreview,
			EGameXXKRouteMerchantPurchaseFailure::InvalidMerchantStock,
			TEXT("Route merchants only stock relics."),
			OutError);
	}

	void CopyPreviewToResult(
		const FGameXXKRouteMerchantPurchasePreview& Preview,
		const FName ReplacementEntryId,
		FGameXXKRouteMerchantPurchaseResult& OutResult)
	{
		OutResult = FGameXXKRouteMerchantPurchaseResult();
		OutResult.bRequiresReplacement = Preview.bRequiresReplacement;
		OutResult.Offer = Preview.Offer;
		OutResult.OfferId = Preview.Offer.OfferId;
		OutResult.CardId = Preview.Offer.Kind == EGameXXKRouteMerchantOfferKind::Card ? Preview.Offer.ContentId : NAME_None;
		OutResult.BalanceBefore = Preview.BalanceBefore;
		OutResult.BalanceAfter = Preview.BalanceAfter;
		OutResult.Price = Preview.Price;
		OutResult.MergeSurvivorEntryId = Preview.MergeSurvivorEntryId;
		OutResult.ConsumedEntryIds = Preview.ConsumedEntryIds;
		OutResult.FinalQuality = Preview.FinalQuality;
		OutResult.TemporaryCountDelta = Preview.TemporaryCountDelta;
		OutResult.CapacityDelta = Preview.CapacityDelta;
		OutResult.ReplacementEntryId = ReplacementEntryId;
		OutResult.EligibleReplacementEntryIds = Preview.EligibleReplacementEntryIds;
		OutResult.Failure = Preview.Failure;
		OutResult.FailureReason = Preview.FailureReason;
	}

}

int32 FGameXXKRouteMerchantRules::GetRefreshCost(const int32 RefreshCount)
{
	if (RefreshCount < 0)
	{
		return 0;
	}
	if (RefreshCount == 0) return 20;
	if (RefreshCount == 1) return 30;
	if (RefreshCount == 2) return 40;
	return 50;
}

bool FGameXXKRouteMerchantRules::ValidateSavedStock(
	const FGameXXKRuntimeState& State,
	FString* OutError)
{
	if (OutError)
	{
		OutError->Reset();
	}
	return ValidateSavedStockCore(State, State.CardRun.RouteMerchant, OutError);
}

bool FGameXXKRouteMerchantRules::EnsureStock(FGameXXKRuntimeState& InOutState, FString* OutError)
{
	if (OutError)
	{
		OutError->Reset();
	}
	const FGameXXKRouteMapNode* MerchantNode = nullptr;
	if (!ValidateRouteContext(InOutState, MerchantNode, OutError))
	{
		return false;
	}
	const FGameXXKRouteMerchantState& Existing = InOutState.CardRun.RouteMerchant;
	if (Existing.SourceNodeId == MerchantNode->NodeId)
	{
		return ValidateStoredStock(InOutState, Existing, OutError);
	}
	if (Existing.SourceNodeId == INDEX_NONE && !IsMerchantEmpty(Existing))
	{
		return SetError(OutError, TEXT("The empty merchant snapshot contains partial persisted metadata."));
	}
	if (Existing.PendingPurchase.bActive)
	{
		return SetError(OutError, TEXT("A pending merchant purchase prevents opening a different merchant node."));
	}

	FGameXXKRouteMerchantState Generated;
	if (!GenerateStock(InOutState, MerchantNode->NodeId, 0, Generated, OutError))
	{
		return false;
	}
	FGameXXKRuntimeState Candidate = InOutState;
	Candidate.CardRun.RouteMerchant = MoveTemp(Generated);
	InOutState = MoveTemp(Candidate);
	return true;
}

bool FGameXXKRouteMerchantRules::GetView(
	const FGameXXKRuntimeState& State,
	FGameXXKRouteMerchantView& OutView,
	FString* OutError)
{
	OutView = FGameXXKRouteMerchantView();
	if (OutError)
	{
		OutError->Reset();
	}
	const FGameXXKRouteMapNode* MerchantNode = nullptr;
	if (!ValidateRouteContext(State, MerchantNode, OutError)
		|| !ValidateStoredStock(State, State.CardRun.RouteMerchant, OutError))
	{
		return false;
	}

	const FGameXXKRouteMerchantState& Merchant = State.CardRun.RouteMerchant;
	OutView.RouteTravelMoney = State.CardRun.RouteTravelMoney;
	OutView.RefreshCost = GetRefreshCost(Merchant.RefreshCount);
	OutView.bRefreshAffordable = OutView.RefreshCost > 0 && State.CardRun.RouteTravelMoney >= OutView.RefreshCost;
	OutView.bRefreshEnabled = OutView.bRefreshAffordable
		&& !Merchant.PendingPurchase.bActive
		&& Merchant.RefreshCount < MAX_int32;
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
			TEXT("行旅钱不足，还差%d"),
			FMath::Max(0, OutView.RefreshCost - State.CardRun.RouteTravelMoney));
	}
	OutView.bHasPendingReplacement = Merchant.PendingPurchase.bActive;
	OutView.bCanLeave = true;

	for (const FGameXXKRouteMerchantOffer& Offer : Merchant.Offers)
	{
		FGameXXKRouteMerchantOfferView OfferView;
		OfferView.SavedOffer = Offer;
		OfferView.bAffordable = !Offer.bUnavailable
			&& Offer.Price > 0
			&& State.CardRun.RouteTravelMoney >= Offer.Price;
		OfferView.bPurchaseEnabled = OfferView.bAffordable
			&& !Offer.bSold
			&& !Merchant.PendingPurchase.bActive;
		if (Offer.bUnavailable)
		{
			OfferView.DisabledReason = TEXT("本次路线已收集完");
		}
		else if (Offer.bSold)
		{
			OfferView.DisabledReason = TEXT("已售");
		}
		else if (Merchant.PendingPurchase.bActive)
		{
			OfferView.DisabledReason = TEXT("请先完成或取消当前卡牌替换");
		}
		else if (!OfferView.bAffordable)
		{
			OfferView.DisabledReason = FString::Printf(
				TEXT("行旅钱不足，还差%d"),
				FMath::Max(0, Offer.Price - State.CardRun.RouteTravelMoney));
		}
		if (Offer.Kind == EGameXXKRouteMerchantOfferKind::Card)
		{
			OutView.CardOffers.Add(MoveTemp(OfferView));
		}
		else
		{
			OutView.RelicOffers.Add(MoveTemp(OfferView));
		}
	}
	return OutView.CardOffers.Num() == CardSlotCount && OutView.RelicOffers.Num() == RelicSlotCount;
}

bool FGameXXKRouteMerchantRules::Refresh(FGameXXKRuntimeState& InOutState, FString* OutError)
{
	if (OutError)
	{
		OutError->Reset();
	}
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
	if (!EnsureStock(Candidate, OutError))
	{
		return false;
	}
	const FGameXXKRouteMerchantState& Existing = Candidate.CardRun.RouteMerchant;
	if (Existing.PendingPurchase.bActive)
	{
		return SetError(OutError, TEXT("Merchant stock cannot refresh while a card replacement is pending."));
	}
	if (Existing.RefreshCount == MAX_int32)
	{
		return SetError(OutError, TEXT("Merchant refresh count cannot be safely incremented."));
	}
	const int32 Cost = GetRefreshCost(Existing.RefreshCount);
	if (Cost <= 0 || Candidate.CardRun.RouteTravelMoney < Cost)
	{
		return SetError(OutError, TEXT("There is not enough route travel money to refresh the merchant."));
	}

	FGameXXKRouteMerchantState Refreshed;
	if (!GenerateStock(Candidate, MerchantNode->NodeId, Existing.RefreshCount + 1, Refreshed, OutError))
	{
		return false;
	}
	Candidate.CardRun.RouteMerchant = MoveTemp(Refreshed);
	Candidate.CardRun.RouteTravelMoney -= Cost;
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

	if (Candidate.CardRun.Relics.ContainsByPredicate([&Preview](const FGameXXKRelicInstance& Instance)
	{
		return Instance.RelicId == Preview.Offer.ContentId;
	}))
	{
		OutResult.Failure = EGameXXKRouteMerchantPurchaseFailure::DuplicateRelic;
		OutResult.FailureReason = TEXT("Route relics are unique and this relic is already owned.");
		return false;
	}
	FString RelicError;
	if (!FGameXXKRelicRules::AcquireRelic(Candidate, Preview.Offer.ContentId, &RelicError))
	{
		OutResult.Failure = EGameXXKRouteMerchantPurchaseFailure::RelicAcquisitionRejected;
		OutResult.FailureReason = RelicError.IsEmpty()
			? TEXT("The relic acquisition was rejected.")
			: RelicError;
		return false;
	}

	FGameXXKRouteMerchantOffer* CandidateOffer = Candidate.CardRun.RouteMerchant.Offers.FindByPredicate([OfferId](const FGameXXKRouteMerchantOffer& Offer)
	{
		return Offer.OfferId == OfferId;
	});
	if (!CandidateOffer || CandidateOffer->bSold || CandidateOffer->bUnavailable)
	{
		OutResult.Failure = EGameXXKRouteMerchantPurchaseFailure::InvalidMerchantStock;
		OutResult.FailureReason = TEXT("The candidate merchant offer changed before commit.");
		return false;
	}
	Candidate.CardRun.RouteTravelMoney -= Preview.Price;
	CandidateOffer->bSold = true;
	Candidate.CardRun.RouteMerchant.PendingPurchase = FGameXXKPendingRouteMerchantPurchase();
	OutResult.bPurchased = true;
	OutResult.bRequiresReplacement = false;
	OutResult.BalanceAfter = Candidate.CardRun.RouteTravelMoney;
	OutResult.Failure = EGameXXKRouteMerchantPurchaseFailure::None;
	OutResult.FailureReason.Reset();
	InOutState = MoveTemp(Candidate);
	return true;
}

bool FGameXXKRouteMerchantRules::CancelPendingPurchase(FGameXXKRuntimeState& InOutState, FString* OutError)
{
	if (OutError)
	{
		OutError->Reset();
	}
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
