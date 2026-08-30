#include "GameXXKRouteMerchantRules.h"

#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKCardQualityRules.h"
#include "GameXXKMVPRules.h"
#include "GameXXKPartyFormationRules.h"
#include "GameXXKRelicCatalog.h"
#include "GameXXKRelicRules.h"

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

	bool IsFourOfferSnapshotOfKind(
		const FGameXXKRouteMerchantState& Merchant,
		const EGameXXKRouteMerchantOfferKind Kind)
	{
		return Merchant.Offers.Num() == 4
			&& !Merchant.Offers.ContainsByPredicate([Kind](const FGameXXKRouteMerchantOffer& Offer)
			{
				return Offer.Kind != Kind;
			});
	}

	bool IsLegacyRelicSnapshot(const FGameXXKRouteMerchantState& Merchant)
	{
		return IsFourOfferSnapshotOfKind(Merchant, EGameXXKRouteMerchantOfferKind::Relic);
	}

	bool IsLegacyCardOnlySnapshot(const FGameXXKRouteMerchantState& Merchant)
	{
		return IsFourOfferSnapshotOfKind(Merchant, EGameXXKRouteMerchantOfferKind::Card);
	}

	bool IsLegacySnapshot(const FGameXXKRouteMerchantState& Merchant)
	{
		return IsLegacyRelicSnapshot(Merchant) || IsLegacyCardOnlySnapshot(Merchant);
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
		const EGameXXKRouteMerchantOfferKind Kind,
		const int32 SlotIndex)
	{
		FGameXXKRouteMerchantOffer Offer;
		Offer.OfferId = MakeOfferId(RootSeed, SourceNodeId, RefreshCount, Kind, SlotIndex);
		Offer.Kind = Kind;
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
		if (!Definition || !Definition->bOfferEligible || !IsConcreteQuality(Definition->BaseQuality))
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
		FName ActiveNpcId;
		if (FGameXXKPartyFormationRules::ResolveQuestNpcId(State, ActiveNpcId)
			&& ActiveNpcId == OwnerMemberId
			&& State.CardRun.PartySelection.QuestNpc.NpcId == ActiveNpcId)
		{
			OutCardIds = &State.CardRun.PartySelection.QuestNpc.SelectedCardIds;
			return true;
		}
		return false;
	}

	bool ValidateLiveCardOffer(
		const FGameXXKRuntimeState& State,
		const FGameXXKRouteMerchantOffer& Offer,
		EGameXXKRouteMerchantPurchaseFailure& OutFailure,
		FString& OutReason)
	{
		OutFailure = EGameXXKRouteMerchantPurchaseFailure::None;
		OutReason.Reset();
		const TArray<FName>* OwnerCards = nullptr;
		if (!FindDeployedMemberCards(State, Offer.OwnerMemberId, OwnerCards))
		{
			OutFailure = EGameXXKRouteMerchantPurchaseFailure::OwnerNoLongerDeployed;
			OutReason = TEXT("The card owner is no longer deployed.");
			return false;
		}
		if (!OwnerCards || !OwnerCards->Contains(Offer.ContentId))
		{
			OutFailure = EGameXXKRouteMerchantPurchaseFailure::CardNoLongerCarried;
			OutReason = TEXT("The deployed owner no longer carries this card.");
			return false;
		}
		const EGameXXKCardQuality CurrentQuality =
			FGameXXKCardBattleAdapter::GetConfiguredCardQuality(State.CardRun, Offer.ContentId);
		if (CurrentQuality >= EGameXXKCardQuality::Epic)
		{
			OutFailure = EGameXXKRouteMerchantPurchaseFailure::CardAlreadyMaxQuality;
			OutReason = TEXT("The carried card already reached Epic quality.");
			return false;
		}
		if (CurrentQuality != Offer.Quality)
		{
			OutFailure = EGameXXKRouteMerchantPurchaseFailure::StaleCardQuality;
			OutReason = TEXT("The carried card quality changed after stock generation.");
			return false;
		}
		return true;
	}

	bool ValidateLiveRelicOffer(
		const FGameXXKRuntimeState& State,
		const FGameXXKRouteMerchantOffer& Offer,
		EGameXXKRouteMerchantPurchaseFailure& OutFailure,
		FString& OutReason)
	{
		OutFailure = EGameXXKRouteMerchantPurchaseFailure::None;
		OutReason.Reset();
		const FGameXXKRelicDefinition* Definition =
			FGameXXKRelicCatalog::FindDefinition(Offer.ContentId);
		if (!Definition
			|| !Definition->bOfferEligible
			|| Definition->BaseQuality != Offer.Quality
			|| !Offer.OwnerMemberId.IsNone()
			|| Offer.NextQuality != EGameXXKCardQuality::Invalid
			|| FGameXXKCardQualityRules::GetRelicPrice(Offer.Quality) != Offer.Price)
		{
			OutFailure = EGameXXKRouteMerchantPurchaseFailure::InvalidMerchantStock;
			OutReason = TEXT("The merchant relic no longer matches the approved catalog.");
			return false;
		}
		const FGameXXKRelicInstance* Existing = State.CardRun.Relics.FindByPredicate(
			[&Offer](const FGameXXKRelicInstance& Instance)
			{
				return Instance.RelicId == Offer.ContentId;
			});
		if (Existing && !Definition->bStackable)
		{
			OutFailure = EGameXXKRouteMerchantPurchaseFailure::DuplicateRelic;
			OutReason = TEXT("This unique relic is already owned.");
			return false;
		}
		if (State.CardRun.NextRelicAcquisitionOrdinal == MAX_int32)
		{
			OutFailure = EGameXXKRouteMerchantPurchaseFailure::ArithmeticOverflow;
			OutReason = TEXT("The next relic acquisition ordinal cannot be safely incremented.");
			return false;
		}
		return true;
	}

	void BuildEligibleRelicPool(
		const FGameXXKRuntimeState& State,
		TArray<FName>& OutPool)
	{
		OutPool.Reset();
		for (const FGameXXKRelicDefinition& Definition : FGameXXKRelicCatalog::GetAllDefinitions())
		{
			const FGameXXKRelicInstance* Existing = State.CardRun.Relics.FindByPredicate(
				[&Definition](const FGameXXKRelicInstance& Instance)
				{
					return Instance.RelicId == Definition.Id;
				});
			if (!Definition.Id.IsNone()
				&& Definition.bOfferEligible
				&& (!Existing || Definition.bStackable)
				&& IsConcreteQuality(Definition.BaseQuality)
				&& FGameXXKCardQualityRules::GetRelicPrice(Definition.BaseQuality) > 0)
			{
				OutPool.Add(Definition.Id);
			}
		}
		OutPool.Sort(NameLess);
	}

	bool CanRefreshStock(
		const FGameXXKRuntimeState& State,
		const FGameXXKRouteMerchantState& Merchant,
		FString* OutError)
	{
		int32 UnsoldCardSlotCount = 0;
		int32 UnsoldRelicSlotCount = 0;
		TSet<FName> PreservedSoldCardIds;
		TSet<FName> PreservedSoldRelicIds;
		for (const FGameXXKRouteMerchantOffer& Offer : Merchant.Offers)
		{
			if (Offer.bSold)
			{
				if (!Offer.bUnavailable && !Offer.ContentId.IsNone())
				{
					(Offer.Kind == EGameXXKRouteMerchantOfferKind::Card
						? PreservedSoldCardIds
						: PreservedSoldRelicIds).Add(Offer.ContentId);
				}
				continue;
			}
			if (Offer.Kind == EGameXXKRouteMerchantOfferKind::Card)
			{
				++UnsoldCardSlotCount;
			}
			else if (Offer.Kind == EGameXXKRouteMerchantOfferKind::Relic)
			{
				++UnsoldRelicSlotCount;
			}
			if (!Offer.bUnavailable)
			{
				EGameXXKRouteMerchantPurchaseFailure Failure;
				FString Reason;
				const bool bLiveValid = Offer.Kind == EGameXXKRouteMerchantOfferKind::Card
					? ValidateLiveCardOffer(State, Offer, Failure, Reason)
					: ValidateLiveRelicOffer(State, Offer, Failure, Reason);
				if (!bLiveValid)
				{
					return SetError(OutError, FString::Printf(
						TEXT("Merchant refresh rejected stale unsold offer %s: %s"),
						*Offer.OfferId.ToString(),
						*Reason));
				}
			}
		}
		if (UnsoldCardSlotCount <= 0 && UnsoldRelicSlotCount <= 0)
		{
			return SetError(OutError, TEXT("No unsold merchant slot is available to refresh."));
		}

		TArray<FGameXXKRouteMerchantRules::FDeployedCardCandidate> Pool;
		FString PoolError;
		if (!FGameXXKRouteMerchantRules::BuildEffectiveDeployedCardPool(State, Pool, &PoolError))
		{
			return SetError(OutError, PoolError.IsEmpty()
				? TEXT("The deployed carried-card pool is invalid.")
				: PoolError);
		}
		Pool.RemoveAll([&PreservedSoldCardIds](const auto& Candidate)
		{
			return PreservedSoldCardIds.Contains(Candidate.CardId);
		});
		TArray<FName> RelicPool;
		BuildEligibleRelicPool(State, RelicPool);
		RelicPool.RemoveAll([&PreservedSoldRelicIds](const FName RelicId)
		{
			return PreservedSoldRelicIds.Contains(RelicId);
		});
		const bool bCanRefreshCard = UnsoldCardSlotCount > 0 && !Pool.IsEmpty();
		const bool bCanRefreshRelic = UnsoldRelicSlotCount > 0 && !RelicPool.IsEmpty();
		if (!bCanRefreshCard && !bCanRefreshRelic)
		{
			return SetError(OutError, TEXT("No unsold card or relic target is available to refresh."));
		}
		return true;
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
		TArray<FGameXXKRouteMerchantRules::FDeployedCardCandidate> CardPool;
		if (!FGameXXKRouteMerchantRules::BuildEffectiveDeployedCardPool(State, CardPool, OutError))
		{
			return false;
		}
		CardPool.Sort([](const auto& Left, const auto& Right)
		{
			return Left.CardId == Right.CardId
				? Left.OwnerMemberId.ToString() < Right.OwnerMemberId.ToString()
				: Left.CardId.ToString() < Right.CardId.ToString();
		});
		TArray<FName> RelicPool;
		BuildEligibleRelicPool(State, RelicPool);

		TSet<FName> PreservedSoldCardIds;
		TSet<FName> PreservedSoldRelicIds;
		int32 UnsoldCardSlotCount = FGameXXKRouteMerchantRules::CardSlotCount;
		int32 UnsoldRelicSlotCount = FGameXXKRouteMerchantRules::RelicSlotCount;
		if (PreviousStock)
		{
			UnsoldCardSlotCount = 0;
			UnsoldRelicSlotCount = 0;
			for (int32 Index = 0; Index < PreviousStock->Offers.Num(); ++Index)
			{
				const FGameXXKRouteMerchantOffer& Offer = PreviousStock->Offers[Index];
				if (Offer.bSold && !Offer.bUnavailable && !Offer.ContentId.IsNone())
				{
					if (Offer.Kind == EGameXXKRouteMerchantOfferKind::Card)
					{
						PreservedSoldCardIds.Add(Offer.ContentId);
					}
					else if (Offer.Kind == EGameXXKRouteMerchantOfferKind::Relic)
					{
						PreservedSoldRelicIds.Add(Offer.ContentId);
					}
				}
				else if (Offer.Kind == EGameXXKRouteMerchantOfferKind::Card)
				{
					++UnsoldCardSlotCount;
				}
				else if (Offer.Kind == EGameXXKRouteMerchantOfferKind::Relic)
				{
					++UnsoldRelicSlotCount;
				}
			}
			CardPool.RemoveAll([&PreservedSoldCardIds](const auto& Candidate)
			{
				return PreservedSoldCardIds.Contains(Candidate.CardId);
			});
			RelicPool.RemoveAll([&PreservedSoldRelicIds](const FName RelicId)
			{
				return PreservedSoldRelicIds.Contains(RelicId);
			});
		}
		const bool bCanRerollCard = UnsoldCardSlotCount > 0 && !CardPool.IsEmpty();
		const bool bCanRerollRelic = UnsoldRelicSlotCount > 0 && !RelicPool.IsEmpty();
		if (bRequireRerollCandidate && !bCanRerollCard && !bCanRerollRelic)
		{
			return SetError(OutError, TEXT("No unsold card or relic target is available to refresh."));
		}

		const int32 RootSeed = State.CardRun.RouteProgress.RootSeed;
		uint32 RandomState = DeriveStockRandomSeed(RootSeed, SourceNodeId, RefreshCount);
		FGameXXKRouteMerchantState Candidate;
		Candidate.SourceNodeId = SourceNodeId;
		Candidate.OfferSeed = DerivePersistedStockSeed(RootSeed, SourceNodeId, RefreshCount);
		Candidate.RefreshCount = RefreshCount;
		Candidate.Offers.Reserve(FGameXXKRouteMerchantRules::TotalSlotCount);
		for (int32 SlotIndex = 0; SlotIndex < FGameXXKRouteMerchantRules::CardSlotCount; ++SlotIndex)
		{
			if (PreviousStock && PreviousStock->Offers.IsValidIndex(SlotIndex)
				&& PreviousStock->Offers[SlotIndex].Kind == EGameXXKRouteMerchantOfferKind::Card
				&& PreviousStock->Offers[SlotIndex].bSold)
			{
				FGameXXKRouteMerchantOffer Sold = PreviousStock->Offers[SlotIndex];
				Sold.OfferId = MakeOfferId(
					RootSeed, SourceNodeId, RefreshCount, EGameXXKRouteMerchantOfferKind::Card, SlotIndex);
				Candidate.Offers.Add(MoveTemp(Sold));
				continue;
			}
			FGameXXKRouteMerchantOffer Offer = MakeUnavailableOffer(
				RootSeed, SourceNodeId, RefreshCount, EGameXXKRouteMerchantOfferKind::Card, SlotIndex);
			if (!CardPool.IsEmpty())
			{
				const int32 PickIndex = static_cast<int32>(
					NextRandom(RandomState) % static_cast<uint32>(CardPool.Num()));
				const FGameXXKRouteMerchantRules::FDeployedCardCandidate Picked = CardPool[PickIndex];
				CardPool.RemoveAt(PickIndex);
				Offer.bUnavailable = false;
				Offer.ContentId = Picked.CardId;
				Offer.OwnerMemberId = Picked.OwnerMemberId;
				Offer.Quality = Picked.CurrentQuality;
				Offer.NextQuality = FGameXXKCardBattleAdapter::GetNextCardQuality(Picked.CurrentQuality);
				Offer.Price = FGameXXKCardQualityRules::GetCardPrice(Offer.NextQuality);
			}
			Candidate.Offers.Add(MoveTemp(Offer));
		}
		TSet<FName> SelectedRelicIds;
		for (int32 RelicIndex = 0; RelicIndex < FGameXXKRouteMerchantRules::RelicSlotCount; ++RelicIndex)
		{
			const int32 GlobalIndex = FGameXXKRouteMerchantRules::CardSlotCount + RelicIndex;
			if (PreviousStock && PreviousStock->Offers.IsValidIndex(GlobalIndex)
				&& PreviousStock->Offers[GlobalIndex].Kind == EGameXXKRouteMerchantOfferKind::Relic
				&& PreviousStock->Offers[GlobalIndex].bSold)
			{
				FGameXXKRouteMerchantOffer Sold = PreviousStock->Offers[GlobalIndex];
				Sold.OfferId = MakeOfferId(
					RootSeed, SourceNodeId, RefreshCount, EGameXXKRouteMerchantOfferKind::Relic, RelicIndex);
				Candidate.Offers.Add(MoveTemp(Sold));
				continue;
			}
			FName RelicId = NAME_None;
			if (!RelicPool.IsEmpty())
			{
				TArray<FName> LegalRelics = RelicPool.FilterByPredicate(
					[&SelectedRelicIds](const FName CandidateId)
					{
						return !SelectedRelicIds.Contains(CandidateId);
					});
				if (!LegalRelics.IsEmpty())
				{
					const int32 PickIndex = static_cast<int32>(
						NextRandom(RandomState) % static_cast<uint32>(LegalRelics.Num()));
					RelicId = LegalRelics[PickIndex];
					SelectedRelicIds.Add(RelicId);
				}
			}
			FGameXXKRouteMerchantOffer Offer;
			if (!BuildRelicOffer(
					RootSeed, SourceNodeId, RefreshCount, RelicIndex, RelicId, Offer, OutError))
			{
				return false;
			}
			Candidate.Offers.Add(MoveTemp(Offer));
		}
		if (Candidate.Offers.Num() != FGameXXKRouteMerchantRules::TotalSlotCount)
		{
			return SetError(OutError, TEXT("Merchant stock generation did not produce four cards and four relics."));
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
		if (IsLegacySnapshot(Merchant))
		{
			// Four-offer card-only and relic-only snapshots are migrated on open.
			if (Merchant.SourceNodeId < 0
				|| Merchant.RefreshCount < 0
				|| Merchant.OfferSeed != DerivePersistedStockSeed(
					State.CardRun.RouteProgress.RootSeed,
					Merchant.SourceNodeId,
					Merchant.RefreshCount))
			{
				return SetError(OutError, TEXT("The legacy four-offer merchant snapshot has invalid persisted metadata."));
			}
			return true;
		}
		if (Merchant.SourceNodeId < 0 || Merchant.RefreshCount < 0
			|| Merchant.OfferSeed != DerivePersistedStockSeed(
				State.CardRun.RouteProgress.RootSeed, Merchant.SourceNodeId, Merchant.RefreshCount)
			|| Merchant.Offers.Num() != FGameXXKRouteMerchantRules::TotalSlotCount)
		{
			return SetError(OutError, TEXT("The saved merchant stock metadata is incomplete or does not match its derived identity."));
		}
		TSet<FName> SeenOfferIds;
		TSet<FName> SeenCardIds;
		TSet<FName> SeenRelicIds;
		for (int32 Index = 0; Index < Merchant.Offers.Num(); ++Index)
		{
			const FGameXXKRouteMerchantOffer& Offer = Merchant.Offers[Index];
			const bool bCardSlot = Index < FGameXXKRouteMerchantRules::CardSlotCount;
			const EGameXXKRouteMerchantOfferKind ExpectedKind = bCardSlot
				? EGameXXKRouteMerchantOfferKind::Card
				: EGameXXKRouteMerchantOfferKind::Relic;
			const int32 LocalIndex = bCardSlot
				? Index
				: Index - FGameXXKRouteMerchantRules::CardSlotCount;
			if (Offer.Kind != ExpectedKind
				|| Offer.OfferId != MakeOfferId(
					State.CardRun.RouteProgress.RootSeed,
					Merchant.SourceNodeId,
					Merchant.RefreshCount,
					ExpectedKind,
					LocalIndex)
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
			if (bCardSlot)
			{
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
			else
			{
				const FGameXXKRelicDefinition* Definition = FGameXXKRelicCatalog::FindDefinition(Offer.ContentId);
				if (Offer.ContentId.IsNone() || !Offer.OwnerMemberId.IsNone() || !Definition
					|| !Definition->bOfferEligible
					|| Definition->BaseQuality != Offer.Quality
					|| Offer.NextQuality != EGameXXKCardQuality::Invalid
					|| FGameXXKCardQualityRules::GetRelicPrice(Offer.Quality) != Offer.Price
					|| SeenRelicIds.Contains(Offer.ContentId))
				{
					return SetError(OutError, TEXT("A saved merchant relic offer violates definition, quality, price, or uniqueness rules."));
				}
				SeenRelicIds.Add(Offer.ContentId);
			}
		}
		if (!IsPendingPurchaseEmpty(Merchant.PendingPurchase))
		{
			return SetError(OutError, TEXT("Merchant purchases never persist a replacement transaction."));
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
			|| IsLegacySnapshot(State.CardRun.RouteMerchant))
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
				Offer->Kind == EGameXXKRouteMerchantOfferKind::Card
					? TEXT("No upgradable carried card is available for this slot.")
					: TEXT("No purchasable relic is available for this slot."), OutError);
		}
		if (Offer->bSold)
		{
			return SetPurchaseFailure(OutPreview, EGameXXKRouteMerchantPurchaseFailure::OfferAlreadySold,
				TEXT("This merchant offer was already sold."), OutError);
		}
		if (!ReplacementEntryId.IsNone())
		{
			return SetPurchaseFailure(OutPreview, EGameXXKRouteMerchantPurchaseFailure::InvalidReplacementEntryId,
				TEXT("Merchant card and relic purchases never accept a replacement EntryId."), OutError);
		}
		if (State.PlayerGold < Offer->Price)
		{
			return SetPurchaseFailure(OutPreview, EGameXXKRouteMerchantPurchaseFailure::InsufficientOrdinaryGold,
				FString::Printf(TEXT("Ordinary gold is short by %d."), Offer->Price - State.PlayerGold), OutError);
		}
		EGameXXKRouteMerchantPurchaseFailure LiveFailure =
			EGameXXKRouteMerchantPurchaseFailure::InvalidMerchantStock;
		FString LiveReason(TEXT("The merchant offer has an unknown kind."));
		const bool bLiveValid = Offer->Kind == EGameXXKRouteMerchantOfferKind::Card
			? ValidateLiveCardOffer(State, *Offer, LiveFailure, LiveReason)
			: Offer->Kind == EGameXXKRouteMerchantOfferKind::Relic
				? ValidateLiveRelicOffer(State, *Offer, LiveFailure, LiveReason)
				: false;
		if (!bLiveValid)
		{
			if (Offer->Kind != EGameXXKRouteMerchantOfferKind::Card
				&& Offer->Kind != EGameXXKRouteMerchantOfferKind::Relic)
			{
				LiveFailure = EGameXXKRouteMerchantPurchaseFailure::InvalidMerchantStock;
			}
			return SetPurchaseFailure(OutPreview, LiveFailure, LiveReason, OutError);
		}
		OutPreview.BalanceAfter = State.PlayerGold - Offer->Price;
		OutPreview.FinalQuality = Offer->Kind == EGameXXKRouteMerchantOfferKind::Card
			? Offer->NextQuality
			: Offer->Quality;
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
		OutResult.CardId = Preview.Offer.Kind == EGameXXKRouteMerchantOfferKind::Card
			? Preview.Offer.ContentId
			: NAME_None;
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
	FName ActiveNpcId;
	if (FGameXXKPartyFormationRules::ResolveQuestNpcId(State, ActiveNpcId)
		&& ActiveNpcId == State.CardRun.PartySelection.QuestNpc.NpcId)
	{
		AppendConfiguredCards(
			ActiveNpcId,
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
	if (IsLegacySnapshot(Existing)
		&& !ValidateSavedStockCore(InOutState, Existing, OutError))
	{
		return false;
	}
	if (Existing.SourceNodeId == MerchantNode->NodeId && !IsLegacySnapshot(Existing))
	{
		return ValidateStoredStock(InOutState, Existing, OutError);
	}
	if (Existing.SourceNodeId == INDEX_NONE && !IsMerchantEmpty(Existing))
	{
		return SetError(OutError, TEXT("The empty merchant snapshot contains partial persisted metadata."));
	}
	if (Existing.PendingPurchase.bActive && !IsLegacySnapshot(Existing))
	{
		return SetError(OutError, TEXT("A pending merchant purchase prevents opening a different merchant node."));
	}
	const int32 RefreshCount = Existing.SourceNodeId == MerchantNode->NodeId
		? FMath::Max(0, Existing.RefreshCount) : 0;
	const FGameXXKRouteMerchantState* LegacyCardStock =
		Existing.SourceNodeId == MerchantNode->NodeId && IsLegacyCardOnlySnapshot(Existing)
			? &Existing
			: nullptr;
	FGameXXKRouteMerchantState Generated;
	if (!GenerateStock(InOutState, MerchantNode->NodeId, RefreshCount, LegacyCardStock, false, Generated, OutError))
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
		|| IsLegacySnapshot(State.CardRun.RouteMerchant))
	{
		return false;
	}
	const FGameXXKRouteMerchantState& Merchant = State.CardRun.RouteMerchant;
	OutView.PlayerGold = State.PlayerGold;
	OutView.RouteTravelMoney = State.PlayerGold;
	OutView.RefreshCost = GetRefreshCost(Merchant.RefreshCount);
	OutView.bRefreshAffordable = OutView.RefreshCost > 0 && State.PlayerGold >= OutView.RefreshCost;
	FString RefreshEligibilityError;
	const bool bRefreshEligible = !Merchant.PendingPurchase.bActive
		&& Merchant.RefreshCount < MAX_int32
		&& CanRefreshStock(State, Merchant, &RefreshEligibilityError);
	OutView.bRefreshEnabled = OutView.bRefreshAffordable
		&& bRefreshEligible;
	if (Merchant.PendingPurchase.bActive)
	{
		OutView.RefreshDisabledReason = TEXT("请先完成或取消当前卡牌替换");
	}
	else if (Merchant.RefreshCount == MAX_int32)
	{
		OutView.RefreshDisabledReason = TEXT("刷新次数已达上限");
	}
	else if (!bRefreshEligible)
	{
		OutView.RefreshDisabledReason = RefreshEligibilityError.IsEmpty()
			? TEXT("没有可刷新的商品")
			: RefreshEligibilityError;
	}
	else if (!OutView.bRefreshAffordable)
	{
		OutView.RefreshDisabledReason = FString::Printf(
			TEXT("金币不足，还差%d"), FMath::Max(0, OutView.RefreshCost - State.PlayerGold));
	}
	OutView.bHasPendingReplacement = false;
	OutView.bCanLeave = true;
	for (const FGameXXKRouteMerchantOffer& Offer : Merchant.Offers)
	{
		FGameXXKRouteMerchantOfferView OfferView;
		OfferView.SavedOffer = Offer;
		OfferView.bAffordable = !Offer.bUnavailable && Offer.Price > 0 && State.PlayerGold >= Offer.Price;
		bool bLiveValid = true;
		FString LiveDisabledReason;
		if (!Offer.bUnavailable && !Offer.bSold)
		{
			EGameXXKRouteMerchantPurchaseFailure LiveFailure;
			bLiveValid = Offer.Kind == EGameXXKRouteMerchantOfferKind::Card
				? ValidateLiveCardOffer(State, Offer, LiveFailure, LiveDisabledReason)
				: ValidateLiveRelicOffer(State, Offer, LiveFailure, LiveDisabledReason);
		}
		OfferView.bPurchaseEnabled = OfferView.bAffordable && !Offer.bSold && bLiveValid;
		if (Offer.bUnavailable)
		{
			OfferView.DisabledReason = Offer.Kind == EGameXXKRouteMerchantOfferKind::Card
				? TEXT("没有可强化卡牌")
				: TEXT("没有可购买遗物");
		}
		else if (Offer.bSold) { OfferView.DisabledReason = TEXT("已售"); }
		else if (!bLiveValid) { OfferView.DisabledReason = MoveTemp(LiveDisabledReason); }
		else if (!OfferView.bAffordable)
		{
			OfferView.DisabledReason = FString::Printf(
				TEXT("金币不足，还差%d"), FMath::Max(0, Offer.Price - State.PlayerGold));
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
	if (!CanRefreshStock(Candidate, Existing, OutError))
	{
		return false;
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
	if (CandidateOffer->Kind == EGameXXKRouteMerchantOfferKind::Card)
	{
		Candidate.CardRun.UpgradedCardQualities.Add(
			CandidateOffer->ContentId,
			CandidateOffer->NextQuality);
	}
	else if (CandidateOffer->Kind == EGameXXKRouteMerchantOfferKind::Relic)
	{
		FString RelicError;
		if (!FGameXXKRelicRules::AcquireRelic(Candidate, CandidateOffer->ContentId, &RelicError))
		{
			OutResult.Failure = EGameXXKRouteMerchantPurchaseFailure::RelicAcquisitionRejected;
			OutResult.FailureReason = RelicError.IsEmpty()
				? TEXT("The relic acquisition was rejected.")
				: RelicError;
			return false;
		}
	}
	else
	{
		OutResult.Failure = EGameXXKRouteMerchantPurchaseFailure::InvalidMerchantStock;
		OutResult.FailureReason = TEXT("The merchant offer has an unknown kind.");
		return false;
	}
	Candidate.PlayerGold -= CandidateOffer->Price;
	CandidateOffer->bSold = true;
	Candidate.CardRun.RouteMerchant.PendingPurchase = FGameXXKPendingRouteMerchantPurchase();
	OutResult.bPurchased = true;
	OutResult.BalanceAfter = Candidate.PlayerGold;
	OutResult.FinalQuality = CommitPreview.FinalQuality;
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
