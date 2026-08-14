#include "GameXXKMVPRules.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKCardQualityRules.h"
#include "GameXXKEncounterRules.h"
#include "GameXXKRelicCatalog.h"
#include "GameXXKRouteMerchantRules.h"
#include "GameXXKRouteMerchantTypes.h"
#include "MVP/GameXXKSaveMigration.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	bool NameLess(const FName Left, const FName Right)
	{
		return Left.ToString() < Right.ToString();
	}

	TArray<FName> FindCardIdsByOwner(const EGameXXKCardOwner Owner)
	{
		TArray<FName> Result;
		for (const FGameXXKCardDefinition& Definition : FGameXXKCardCatalog::GetAllCardDefinitions())
		{
			if (Definition.Owner == Owner)
			{
				Result.Add(Definition.Id);
			}
		}
		Result.Sort(NameLess);
		return Result;
	}

	FGameXXKRuntimeState MakeMerchantState()
	{
		FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
		State.Screen = EGameXXKScreen::RouteMerchant;
		State.bDungeonActive = true;
		State.bHasGeneratedRouteMap = true;
		State.RouteSeed = 0x6137;
		State.CurrentRouteNodeId = 9;
		State.PendingRouteNodeId = 10;
		State.RouteMapNodes = {
			FGameXXKRouteMapNode{9, 1, 0, EGameXXKNodeKind::Start, FVector2D(0.25f, 0.5f), TArray<int32>{10}},
			FGameXXKRouteMapNode{10, 2, 1, EGameXXKNodeKind::Merchant, FVector2D(0.55f, 0.5f), TArray<int32>{}}};
		State.RouteMapEdges = {FGameXXKRouteMapEdge{9, 10}};
		State.VisitedRouteNodeIds = {9};
		State.CardRun.RouteProgress.SchemaVersion = 1;
		State.CardRun.RouteProgress.RootSeed = State.RouteSeed;
		State.CardRun.RouteProgress.ChapterSeeds = {State.RouteSeed};
		State.CardRun.RouteProgress.CurrentChapter = 1;
		State.CardRun.RouteProgress.RouteCombatLevel = 1;
		State.CardRun.bLoadoutLockedForRoute = true;
		State.CardRun.bRouteEconomyInitialized = true;
		State.CardRun.RouteTravelMoney = 500;
		State.PlayerGold = 777;

		const TArray<FName> HeroCards = FindCardIdsByOwner(EGameXXKCardOwner::Hero);
		State.CardRun.HeroUnlockedCardIds.Append(HeroCards.GetData(), FMath::Min(8, HeroCards.Num()));
		State.CardRun.HeroSelectedCardIds = State.CardRun.HeroUnlockedCardIds;
		return State;
	}

	bool MerchantStatesMatch(const FGameXXKRouteMerchantState& Left, const FGameXXKRouteMerchantState& Right)
	{
		return FGameXXKRouteMerchantState::StaticStruct()->CompareScriptStruct(&Left, &Right, PPF_None);
	}

	bool RuntimeStatesMatch(const FGameXXKRuntimeState& Left, const FGameXXKRuntimeState& Right)
	{
		return FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(&Left, &Right, PPF_None);
	}

	int32 CountOffersOfKind(const FGameXXKRouteMerchantState& Merchant, const EGameXXKRouteMerchantOfferKind Kind)
	{
		int32 Count = 0;
		for (const FGameXXKRouteMerchantOffer& Offer : Merchant.Offers)
		{
			if (Offer.Kind == Kind)
			{
				++Count;
			}
		}
		return Count;
	}

	bool RouteNodesMatch(const TArray<FGameXXKRouteMapNode>& Left, const TArray<FGameXXKRouteMapNode>& Right)
	{
		if (Left.Num() != Right.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < Left.Num(); ++Index)
		{
			if (!FGameXXKRouteMapNode::StaticStruct()->CompareScriptStruct(&Left[Index], &Right[Index], PPF_None))
			{
				return false;
			}
		}
		return true;
	}

	bool RouteEdgesMatch(const TArray<FGameXXKRouteMapEdge>& Left, const TArray<FGameXXKRouteMapEdge>& Right)
	{
		if (Left.Num() != Right.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < Left.Num(); ++Index)
		{
			if (!FGameXXKRouteMapEdge::StaticStruct()->CompareScriptStruct(&Left[Index], &Right[Index], PPF_None))
			{
				return false;
			}
		}
		return true;
	}

	FGameXXKSaveState MakeLegacyMerchantSnapshot()
	{
		FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
		State.bDungeonActive = true;
		State.RouteSeed = 0x6137;
		State.bHasGeneratedRouteMap = true;
		State.RouteMapNodes = {
			FGameXXKRouteMapNode{9, 1, 0, EGameXXKNodeKind::Start, FVector2D(0.25f, 0.5f), TArray<int32>{10}},
			FGameXXKRouteMapNode{10, 2, 1, EGameXXKNodeKind::Merchant, FVector2D(0.55f, 0.5f), TArray<int32>{}}};
		State.RouteMapEdges = {FGameXXKRouteMapEdge{9, 10}};
		State.VisitedRouteNodeIds = {9, 10};
		State.CurrentRouteNodeId = 10;
		State.PendingRouteNodeId = 10;
		State.CardRun.RouteProgress.SchemaVersion = 1;
		State.CardRun.RouteProgress.RootSeed = State.RouteSeed;
		State.CardRun.RouteProgress.ChapterSeeds = {
			State.RouteSeed,
			FMath::Abs(FGameXXKEncounterRules::DeriveChapterSeed(State.RouteSeed, 2)),
			FMath::Abs(FGameXXKEncounterRules::DeriveChapterSeed(State.RouteSeed, 3))};
		State.CardRun.RouteProgress.CurrentChapter = 1;
		State.CardRun.RouteProgress.RouteCombatLevel = FMath::Clamp(State.PlayerLevel, 1, 20);

		FGameXXKRouteMerchantState& Merchant = State.CardRun.RouteMerchant;
		Merchant.SourceNodeId = 10;
		Merchant.OfferSeed = 0x7351;
		FGameXXKRouteMerchantOffer Offer;
		Offer.OfferId = TEXT("Merchant.10.Relic.0");
		Offer.Kind = EGameXXKRouteMerchantOfferKind::Relic;
		Offer.ContentId = TEXT("Relic.JadeBell");
		Offer.Price = 15;
		Merchant.Offers.Add(Offer);

		FGameXXKSaveState LegacySave = UGameXXKMVPRules::MakeSaveState(State);
		LegacySave.SaveVersion = FGameXXKSaveMigration::RouteMerchantSnapshotIntroducedSaveVersion - 1;
		return LegacySave;
	}

	bool StartAcceptedGeneratedRoute(FGameXXKRuntimeState& OutState)
	{
		OutState = UGameXXKMVPRules::CreateNewGame();
		OutState.RouteSeed = 0x6137;
		return UGameXXKMVPRules::OpenWorldMap(OutState)
			&& UGameXXKMVPRules::EnterWorldRegion(OutState, UGameXXKMVPRules::RegionQingshan())
			&& UGameXXKMVPRules::AcceptTownQuest(OutState)
			&& UGameXXKMVPRules::EnterDungeon(OutState);
	}

	const FGameXXKRouteMapNode* FindNodeOfKind(
		const FGameXXKRuntimeState& State,
		const EGameXXKNodeKind Kind)
	{
		return State.RouteMapNodes.FindByPredicate([Kind](const FGameXXKRouteMapNode& Node)
		{
			return Node.NodeKind == Kind;
		});
	}

	bool OpenGeneratedMerchant(FGameXXKRuntimeState& InOutState, FString* OutError)
	{
		const FGameXXKRouteMapNode* MerchantNode = FindNodeOfKind(InOutState, EGameXXKNodeKind::Merchant);
		if (!MerchantNode)
		{
			return false;
		}
		InOutState.Screen = EGameXXKScreen::RouteMerchant;
		InOutState.CurrentMapId = TEXT("RouteMerchant");
		InOutState.PendingRouteNodeId = MerchantNode->NodeId;
		return FGameXXKRouteMerchantRules::EnsureStock(InOutState, OutError);
	}

	void ForceCardBattleVictory(FGameXXKRuntimeState& InOutState)
	{
		for (FGameXXKCardCombatUnit& Unit : InOutState.CardRun.ActiveBattle.Units)
		{
			if (Unit.Side == EGameXXKCardTargetSide::Enemy)
			{
				Unit.HP = 0;
				Unit.bLiving = false;
			}
		}
		InOutState.CardRun.ActiveBattle.Phase = EGameXXKCardBattlePhase::Victory;
	}

	FGameXXKRouteMerchantOffer* FindAvailableOffer(
		FGameXXKRuntimeState& State,
		const EGameXXKRouteMerchantOfferKind Kind)
	{
		return State.CardRun.RouteMerchant.Offers.FindByPredicate([Kind](const FGameXXKRouteMerchantOffer& Offer)
		{
			return Offer.Kind == Kind && !Offer.bUnavailable && !Offer.bSold;
		});
	}

	FGameXXKRouteMerchantOffer* ForceStackableRelicOffer(FGameXXKRuntimeState& State)
	{
		FGameXXKRouteMerchantOffer* Offer = FindAvailableOffer(State, EGameXXKRouteMerchantOfferKind::Relic);
		if (!Offer)
		{
			return nullptr;
		}
		TSet<FName> OtherOfferedIds;
		for (const FGameXXKRouteMerchantOffer& Existing : State.CardRun.RouteMerchant.Offers)
		{
			if (&Existing != Offer && Existing.Kind == EGameXXKRouteMerchantOfferKind::Relic && !Existing.bUnavailable)
			{
				OtherOfferedIds.Add(Existing.ContentId);
			}
		}
		for (const FGameXXKRelicDefinition& Definition : FGameXXKRelicCatalog::GetAllDefinitions())
		{
			if (Definition.bStackable && !OtherOfferedIds.Contains(Definition.Id))
			{
				Offer->ContentId = Definition.Id;
				Offer->Quality = Definition.BaseQuality;
				Offer->Price = FGameXXKCardQualityRules::GetRelicPrice(Definition.BaseQuality);
				return Offer;
			}
		}
		return nullptr;
	}

	void ExpectPurchaseFailureAndRollback(
		FAutomationTestBase& Test,
		FGameXXKRuntimeState& State,
		const FName OfferId,
		const FName ReplacementEntryId,
		const EGameXXKRouteMerchantPurchaseFailure ExpectedFailure,
		const TCHAR* Label)
	{
		const FGameXXKRuntimeState Before = State;
		FGameXXKRouteMerchantPurchaseResult Result;
		Test.TestFalse(Label, FGameXXKRouteMerchantRules::Purchase(State, OfferId, ReplacementEntryId, Result));
		Test.TestEqual(*FString::Printf(TEXT("%s reports the stable failure enum"), Label), Result.Failure, ExpectedFailure);
		Test.TestFalse(*FString::Printf(TEXT("%s reports a stable reason string"), Label), Result.FailureReason.IsEmpty());
		Test.TestTrue(*FString::Printf(TEXT("%s preserves the complete runtime"), Label), RuntimeStatesMatch(State, Before));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteMerchantStockCompanionTest,
	"GameXXK.Route.Merchant.Rules.Stock.RelicOnlyAndPersistence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteMerchantStockCompanionTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = MakeMerchantState();
	FString Error;
	if (!TestTrue(TEXT("a locked relic-only route generates merchant stock"),
		FGameXXKRouteMerchantRules::EnsureStock(State, &Error)))
	{
		AddError(Error);
		return false;
	}
	TestTrue(TEXT("stock generation reports no error"), Error.IsEmpty());
	const FGameXXKRouteMerchantState FirstStock = State.CardRun.RouteMerchant;
	TestEqual(TEXT("merchant snapshot records pending node"), FirstStock.SourceNodeId, 10);
	TestTrue(TEXT("merchant snapshot has a stable non-zero seed"), FirstStock.OfferSeed != 0);
	TestEqual(TEXT("first snapshot starts before refreshes"), FirstStock.RefreshCount, 0);
	TestEqual(TEXT("snapshot always has four offers"), FirstStock.Offers.Num(), FGameXXKRouteMerchantRules::TotalSlotCount);
	TestEqual(TEXT("snapshot has exactly four relic slots"), CountOffersOfKind(FirstStock, EGameXXKRouteMerchantOfferKind::Relic), FGameXXKRouteMerchantRules::RelicSlotCount);

	TSet<FName> SeenRelicIds;
	for (int32 Index = 0; Index < FirstStock.Offers.Num(); ++Index)
	{
		const FGameXXKRouteMerchantOffer& Offer = FirstStock.Offers[Index];
		TestFalse(*FString::Printf(TEXT("offer %d has a stable id"), Index), Offer.OfferId.IsNone());
		TestFalse(*FString::Printf(TEXT("offer %d is available in the full-catalog fixture"), Index), Offer.bUnavailable);
		TestFalse(*FString::Printf(TEXT("offer %d starts unsold"), Index), Offer.bSold);
		TestEqual(*FString::Printf(TEXT("slot %d is a relic"), Index), Offer.Kind, EGameXXKRouteMerchantOfferKind::Relic);
		const FGameXXKRelicDefinition* Definition = FGameXXKRelicCatalog::FindDefinition(Offer.ContentId);
		TestNotNull(*FString::Printf(TEXT("relic slot %d resolves catalog content"), Index), Definition);
		if (!Definition)
		{
			continue;
		}
		TestEqual(*FString::Printf(TEXT("relic slot %d persists catalog base quality"), Index), Offer.Quality, Definition->BaseQuality);
		TestEqual(*FString::Printf(TEXT("relic slot %d uses quality price"), Index), Offer.Price, FGameXXKCardQualityRules::GetRelicPrice(Offer.Quality));
		TestFalse(*FString::Printf(TEXT("relic slot %d is unique in its batch"), Index), SeenRelicIds.Contains(Offer.ContentId));
		SeenRelicIds.Add(Offer.ContentId);
	}

	FGameXXKRuntimeState IndependentTwin = MakeMerchantState();
	TestTrue(TEXT("independent same-root fixture generates"), FGameXXKRouteMerchantRules::EnsureStock(IndependentTwin, &Error));
	TestTrue(TEXT("root seed, source node, and refresh count deterministically reproduce identical ordered stock"), MerchantStatesMatch(IndependentTwin.CardRun.RouteMerchant, FirstStock));

	TestTrue(TEXT("reopening the same merchant succeeds"), FGameXXKRouteMerchantRules::EnsureStock(State, &Error));
	TestTrue(TEXT("reopening is byte-stable"), MerchantStatesMatch(State.CardRun.RouteMerchant, FirstStock));
	FGameXXKRuntimeState SaveLikeCopy = State;
	TestTrue(TEXT("save-like struct copy reopens"), FGameXXKRouteMerchantRules::EnsureStock(SaveLikeCopy, &Error));
	TestTrue(TEXT("save-like struct copy preserves array order and every offer byte"), MerchantStatesMatch(SaveLikeCopy.CardRun.RouteMerchant, FirstStock));

	const FGameXXKRuntimeState BeforeView = State;
	FGameXXKRouteMerchantView View;
	TestTrue(TEXT("view can be read from saved stock"), FGameXXKRouteMerchantRules::GetView(State, View, &Error));
	TestTrue(TEXT("view read is pure"), RuntimeStatesMatch(State, BeforeView));
	TestEqual(TEXT("view exposes zero card slots"), View.CardOffers.Num(), 0);
	TestEqual(TEXT("view exposes four relic slots"), View.RelicOffers.Num(), FGameXXKRouteMerchantRules::RelicSlotCount);
	TestEqual(TEXT("view exposes route-only balance"), View.RouteTravelMoney, State.CardRun.RouteTravelMoney);
	TestEqual(TEXT("view exposes first refresh cost"), View.RefreshCost, 20);
	TestTrue(TEXT("view enables affordable refresh"), View.bRefreshEnabled);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteMerchantStockFallbackTest,
	"GameXXK.Route.Merchant.Rules.Stock.RelicExhaustionAndUnavailable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteMerchantStockFallbackTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState ExhaustedState = MakeMerchantState();
	FString Error;
	const TArray<FGameXXKRelicDefinition>& Relics = FGameXXKRelicCatalog::GetAllDefinitions();
	for (int32 Index = 0; Index < Relics.Num() - 1; ++Index)
	{
		FGameXXKRelicInstance Owned;
		Owned.RelicId = Relics[Index].Id;
		Owned.Stacks = 1;
		Owned.AcquisitionOrdinal = Index + 1;
		ExhaustedState.CardRun.Relics.Add(Owned);
	}
	TestTrue(TEXT("exhausted relic pools still generate all four persistent slots"), FGameXXKRouteMerchantRules::EnsureStock(ExhaustedState, &Error));
	TestEqual(TEXT("exhausted snapshot still has four slots"), ExhaustedState.CardRun.RouteMerchant.Offers.Num(), FGameXXKRouteMerchantRules::TotalSlotCount);
	int32 AvailableRelics = 0;
	int32 UnavailableRelics = 0;
	for (int32 Index = 0; Index < ExhaustedState.CardRun.RouteMerchant.Offers.Num(); ++Index)
	{
		const FGameXXKRouteMerchantOffer& Offer = ExhaustedState.CardRun.RouteMerchant.Offers[Index];
		TestEqual(*FString::Printf(TEXT("exhausted slot %d keeps its kind"), Index), Offer.Kind, EGameXXKRouteMerchantOfferKind::Relic);
		if (Offer.bUnavailable)
		{
			++UnavailableRelics;
			TestFalse(TEXT("unavailable relic keeps a stable offer id"), Offer.OfferId.IsNone());
			TestTrue(TEXT("unavailable relic has no content"), Offer.ContentId.IsNone());
			TestEqual(TEXT("unavailable relic has invalid quality"), Offer.Quality, EGameXXKCardQuality::Invalid);
			TestEqual(TEXT("unavailable relic costs zero"), Offer.Price, 0);
		}
		else
		{
			++AvailableRelics;
			TestEqual(TEXT("last legal relic is preserved"), Offer.ContentId, Relics.Last().Id);
		}
	}
	TestEqual(TEXT("one unowned relic remains available"), AvailableRelics, 1);
	TestEqual(TEXT("relic shortage produces three explicit unavailable slots"), UnavailableRelics, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteMerchantRefreshTest,
	"GameXXK.Route.Merchant.Rules.Stock.RefreshAtomicity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteMerchantRefreshTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = MakeMerchantState();
	FString Error;
	TestTrue(TEXT("initial stock generates before refresh"), FGameXXKRouteMerchantRules::EnsureStock(State, &Error));
	const FGameXXKRouteMerchantState BeforeRefresh = State.CardRun.RouteMerchant;
	const int32 PlayerGoldBefore = State.PlayerGold;
	const int32 MoneyBefore = State.CardRun.RouteTravelMoney;
	TestEqual(TEXT("refresh zero costs twenty"), FGameXXKRouteMerchantRules::GetRefreshCost(0), 20);
	TestEqual(TEXT("refresh one costs thirty"), FGameXXKRouteMerchantRules::GetRefreshCost(1), 30);
	TestEqual(TEXT("refresh two costs forty"), FGameXXKRouteMerchantRules::GetRefreshCost(2), 40);
	TestEqual(TEXT("refresh three costs fifty"), FGameXXKRouteMerchantRules::GetRefreshCost(3), 50);
	TestEqual(TEXT("later refresh remains fifty"), FGameXXKRouteMerchantRules::GetRefreshCost(999), 50);
	TestEqual(TEXT("invalid refresh count has no valid cost"), FGameXXKRouteMerchantRules::GetRefreshCost(-1), 0);
	TestTrue(TEXT("refresh succeeds"), FGameXXKRouteMerchantRules::Refresh(State, &Error));
	TestEqual(TEXT("refresh debits route travel money once"), State.CardRun.RouteTravelMoney, MoneyBefore - 20);
	TestEqual(TEXT("refresh never touches permanent gold"), State.PlayerGold, PlayerGoldBefore);
	TestEqual(TEXT("refresh count advances once"), State.CardRun.RouteMerchant.RefreshCount, 1);
	TestEqual(TEXT("refresh replaces the complete four-slot stock"), State.CardRun.RouteMerchant.Offers.Num(), FGameXXKRouteMerchantRules::TotalSlotCount);
	for (int32 Index = 0; Index < FGameXXKRouteMerchantRules::TotalSlotCount; ++Index)
	{
		TestTrue(*FString::Printf(TEXT("refresh replaces slot %d identity"), Index), State.CardRun.RouteMerchant.Offers[Index].OfferId != BeforeRefresh.Offers[Index].OfferId);
	}
	FGameXXKRouteMerchantView View;
	TestTrue(TEXT("post-refresh view succeeds"), FGameXXKRouteMerchantRules::GetView(State, View, &Error));
	TestEqual(TEXT("post-refresh view exposes next cost"), View.RefreshCost, 30);

	FGameXXKRuntimeState Insufficient = State;
	Insufficient.CardRun.RouteTravelMoney = View.RefreshCost - 1;
	const FGameXXKRuntimeState InsufficientBefore = Insufficient;
	TestFalse(TEXT("refresh rejects insufficient route money"), FGameXXKRouteMerchantRules::Refresh(Insufficient, &Error));
	TestTrue(TEXT("insufficient refresh preserves the complete runtime"), RuntimeStatesMatch(Insufficient, InsufficientBefore));
	FGameXXKRouteMerchantView InsufficientView;
	TestTrue(TEXT("insufficient view still reads"), FGameXXKRouteMerchantRules::GetView(Insufficient, InsufficientView, &Error));
	TestFalse(TEXT("insufficient view disables refresh"), InsufficientView.bRefreshEnabled);
	TestFalse(TEXT("insufficient view exposes a stable reason"), InsufficientView.RefreshDisabledReason.IsEmpty());

	FGameXXKRuntimeState Overflow = State;
	Overflow.CardRun.RouteMerchant.RefreshCount = MAX_int32;
	const FGameXXKRuntimeState OverflowBefore = Overflow;
	TestFalse(TEXT("refresh count overflow is rejected"), FGameXXKRouteMerchantRules::Refresh(Overflow, &Error));
	TestTrue(TEXT("refresh MAX reaches the explicit overflow guard"), Error.Contains(TEXT("safely incremented")));
	TestTrue(TEXT("overflow rejection preserves the complete runtime"), RuntimeStatesMatch(Overflow, OverflowBefore));

	FGameXXKRuntimeState InvalidContext = State;
	InvalidContext.Screen = EGameXXKScreen::DungeonMap;
	const FGameXXKRuntimeState InvalidContextBefore = InvalidContext;
	TestFalse(TEXT("refresh rejects a non-merchant screen"), FGameXXKRouteMerchantRules::Refresh(InvalidContext, &Error));
	TestTrue(TEXT("invalid-context refresh preserves the complete runtime"), RuntimeStatesMatch(InvalidContext, InvalidContextBefore));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteMerchantPurchaseValidationTest,
	"GameXXK.Route.Merchant.Rules.Purchase.ValidationAndRollback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteMerchantPurchaseValidationTest::RunTest(const FString& Parameters)
{
	FString Error;
	FGameXXKRuntimeState Base = MakeMerchantState();
	TestTrue(TEXT("purchase validation fixture generates stock"), FGameXXKRouteMerchantRules::EnsureStock(Base, &Error));
	FGameXXKRouteMerchantOffer* BaseOffer = FindAvailableOffer(Base, EGameXXKRouteMerchantOfferKind::Relic);
	TestNotNull(TEXT("purchase fixture has a relic offer"), BaseOffer);
	if (!BaseOffer)
	{
		return false;
	}
	const FName OfferId = BaseOffer->OfferId;
	const int32 Price = BaseOffer->Price;

	FGameXXKRuntimeState Insufficient = Base;
	Insufficient.CardRun.RouteTravelMoney = Price - 1;
	const FGameXXKRuntimeState BeforePreview = Insufficient;
	FGameXXKRouteMerchantPurchasePreview Preview;
	TestFalse(TEXT("insufficient preview rejects"), FGameXXKRouteMerchantRules::PreviewPurchase(Insufficient, OfferId, NAME_None, Preview, &Error));
	TestEqual(TEXT("insufficient preview reports typed reason"), Preview.Failure, EGameXXKRouteMerchantPurchaseFailure::InsufficientTravelMoney);
	TestEqual(TEXT("insufficient preview includes the saved offer"), Preview.Offer.OfferId, OfferId);
	TestEqual(TEXT("insufficient preview includes balance before"), Preview.BalanceBefore, Price - 1);
	TestEqual(TEXT("insufficient preview includes price"), Preview.Price, Price);
	TestTrue(TEXT("preview is pure on failure"), RuntimeStatesMatch(Insufficient, BeforePreview));
	ExpectPurchaseFailureAndRollback(
		*this,
		Insufficient,
		OfferId,
		NAME_None,
		EGameXXKRouteMerchantPurchaseFailure::InsufficientTravelMoney,
		TEXT("insufficient purchase"));

	FGameXXKRuntimeState Sold = Base;
	Sold.CardRun.RouteMerchant.Offers[0].bSold = true;
	ExpectPurchaseFailureAndRollback(
		*this,
		Sold,
		Sold.CardRun.RouteMerchant.Offers[0].OfferId,
		NAME_None,
		EGameXXKRouteMerchantPurchaseFailure::OfferAlreadySold,
		TEXT("sold purchase"));

	FGameXXKRuntimeState Stale = Base;
	ExpectPurchaseFailureAndRollback(
		*this,
		Stale,
		TEXT("Merchant.Stale.Offer"),
		NAME_None,
		EGameXXKRouteMerchantPurchaseFailure::StaleOfferId,
		TEXT("stale-id purchase"));

	FGameXXKRuntimeState Unavailable = Base;
	FGameXXKRouteMerchantOffer& UnavailableOffer = Unavailable.CardRun.RouteMerchant.Offers[0];
	UnavailableOffer.ContentId = NAME_None;
	UnavailableOffer.Quality = EGameXXKCardQuality::Invalid;
	UnavailableOffer.Price = 0;
	UnavailableOffer.bUnavailable = true;
	ExpectPurchaseFailureAndRollback(
		*this,
		Unavailable,
		UnavailableOffer.OfferId,
		NAME_None,
		EGameXXKRouteMerchantPurchaseFailure::OfferUnavailable,
		TEXT("unavailable-slot purchase"));

	FGameXXKRuntimeState DuplicateRelic = Base;
	FGameXXKRouteMerchantOffer* StackableOffer = ForceStackableRelicOffer(DuplicateRelic);
	TestNotNull(TEXT("duplicate fixture resolves a stackable relic"), StackableOffer);
	if (StackableOffer)
	{
		FGameXXKRelicInstance Owned;
		Owned.RelicId = StackableOffer->ContentId;
		Owned.Stacks = 7;
		Owned.AcquisitionOrdinal = 1;
		DuplicateRelic.CardRun.Relics.Add(Owned);
		const FGameXXKRuntimeState OwnedRelicBeforeReopen = DuplicateRelic;
		TestTrue(TEXT("same-node reopen preserves an offer that became owned after stock generation"), FGameXXKRouteMerchantRules::EnsureStock(DuplicateRelic, &Error));
		TestTrue(TEXT("owned-relic reopen is byte-stable"), RuntimeStatesMatch(DuplicateRelic, OwnedRelicBeforeReopen));
		ExpectPurchaseFailureAndRollback(
			*this,
			DuplicateRelic,
			StackableOffer->OfferId,
			NAME_None,
			EGameXXKRouteMerchantPurchaseFailure::DuplicateRelic,
			TEXT("duplicate relic"));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteMerchantPurchaseRelicTest,
	"GameXXK.Route.Merchant.Rules.Purchase.RelicUniqueAtomicCommit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteMerchantPurchaseRelicTest::RunTest(const FString& Parameters)
{
	FString Error;
	FGameXXKRuntimeState State = MakeMerchantState();
	TestTrue(TEXT("relic fixture generates stock"), FGameXXKRouteMerchantRules::EnsureStock(State, &Error));
	FGameXXKRouteMerchantOffer* Offer = FindAvailableOffer(State, EGameXXKRouteMerchantOfferKind::Relic);
	TestNotNull(TEXT("relic fixture has an available relic"), Offer);
	if (!Offer)
	{
		return false;
	}
	const FGameXXKRouteMerchantOffer SavedOffer = *Offer;
	const int32 MoneyBefore = State.CardRun.RouteTravelMoney;
	const int32 GoldBefore = State.PlayerGold;
	const int32 RelicsBefore = State.CardRun.Relics.Num();
	FGameXXKRouteMerchantPurchasePreview Preview;
	const FGameXXKRuntimeState BeforePreview = State;
	TestTrue(TEXT("relic preview succeeds"), FGameXXKRouteMerchantRules::PreviewPurchase(State, Offer->OfferId, NAME_None, Preview, &Error));
	TestTrue(TEXT("relic preview can purchase"), Preview.bCanPurchase);
	TestFalse(TEXT("relic preview requires no replacement"), Preview.bRequiresReplacement);
	TestTrue(TEXT("relic preview is pure"), RuntimeStatesMatch(State, BeforePreview));
	FGameXXKRouteMerchantPurchaseResult Result;
	TestTrue(TEXT("relic purchase commits"), FGameXXKRouteMerchantRules::Purchase(State, Offer->OfferId, NAME_None, Result));
	TestEqual(TEXT("relic purchase adds exactly one relic"), State.CardRun.Relics.Num(), RelicsBefore + 1);
	TestEqual(TEXT("relic purchase adds selected id"), State.CardRun.Relics.Last().RelicId, SavedOffer.ContentId);
	TestEqual(TEXT("relic purchase debits once"), State.CardRun.RouteTravelMoney, MoneyBefore - SavedOffer.Price);
	TestEqual(TEXT("relic purchase preserves permanent gold"), State.PlayerGold, GoldBefore);
	const FGameXXKRouteMerchantOffer* SoldOffer = State.CardRun.RouteMerchant.Offers.FindByPredicate([&SavedOffer](const FGameXXKRouteMerchantOffer& Candidate)
	{
		return Candidate.OfferId == SavedOffer.OfferId;
	});
	TestTrue(TEXT("relic purchase marks offer sold"), SoldOffer && SoldOffer->bSold);
	TestEqual(TEXT("relic purchase remains in merchant"), State.Screen, EGameXXKScreen::RouteMerchant);
	const FGameXXKRuntimeState SoldRelicBeforeReopen = State;
	TestTrue(TEXT("same-node reopen accepts sold relic stock"), FGameXXKRouteMerchantRules::EnsureStock(State, &Error));
	TestTrue(TEXT("sold relic reopen is byte-stable"), RuntimeStatesMatch(State, SoldRelicBeforeReopen));

	FGameXXKRuntimeState RelicOverflow = MakeMerchantState();
	TestTrue(TEXT("relic overflow fixture generates stock"), FGameXXKRouteMerchantRules::EnsureStock(RelicOverflow, &Error));
	FGameXXKRouteMerchantOffer* OverflowOffer = FindAvailableOffer(RelicOverflow, EGameXXKRouteMerchantOfferKind::Relic);
	TestNotNull(TEXT("relic overflow fixture has offer"), OverflowOffer);
	if (OverflowOffer)
	{
		RelicOverflow.CardRun.NextRelicAcquisitionOrdinal = MAX_int32;
		ExpectPurchaseFailureAndRollback(
			*this,
			RelicOverflow,
			OverflowOffer->OfferId,
			NAME_None,
			EGameXXKRouteMerchantPurchaseFailure::ArithmeticOverflow,
			TEXT("MAX relic ordinal purchase"));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteMerchantSavedStockValidationTest,
	"GameXXK.Route.Merchant.SaveState.CanonicalValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteMerchantSavedStockValidationTest::RunTest(const FString& Parameters)
{
	FString Error;
	FGameXXKRuntimeState State = MakeMerchantState();
	if (!TestTrue(TEXT("saved-stock fixture generates canonical relic stock"),
		FGameXXKRouteMerchantRules::EnsureStock(State, &Error)))
	{
		return false;
	}
	TestTrue(TEXT("canonical generated stock passes the context-free save validator"),
		FGameXXKRouteMerchantRules::ValidateSavedStock(State, &Error));

	FGameXXKRuntimeState AwayFromMerchant = State;
	AwayFromMerchant.Screen = EGameXXKScreen::DungeonMap;
	AwayFromMerchant.PendingRouteNodeId = INDEX_NONE;
	TestTrue(TEXT("saved stock remains valid after leaving its active merchant screen"),
		FGameXXKRouteMerchantRules::ValidateSavedStock(AwayFromMerchant, &Error));

	FGameXXKRuntimeState UnavailableSlot = State;
	const FName RemovedContentId = UnavailableSlot.CardRun.RouteMerchant.Offers[0].ContentId;
	FGameXXKRouteMerchantOffer& EmptyOffer = UnavailableSlot.CardRun.RouteMerchant.Offers[0];
	EmptyOffer.ContentId = NAME_None;
	EmptyOffer.Quality = EGameXXKCardQuality::Invalid;
	EmptyOffer.Price = 0;
	EmptyOffer.bUnavailable = true;
	EmptyOffer.bSold = false;
	TestTrue(TEXT("a canonical unavailable slot is represented by an empty persisted payload"),
		FGameXXKRouteMerchantRules::ValidateSavedStock(UnavailableSlot, &Error));
	UnavailableSlot.CardRun.RouteMerchant.Offers[0].ContentId = RemovedContentId;
	TestFalse(TEXT("an unavailable slot cannot retain catalog content"),
		FGameXXKRouteMerchantRules::ValidateSavedStock(UnavailableSlot, &Error));

	FGameXXKRuntimeState WrongCount = State;
	WrongCount.CardRun.RouteMerchant.Offers.Pop();
	TestFalse(TEXT("saved stock requires exactly four relic slots"),
		FGameXXKRouteMerchantRules::ValidateSavedStock(WrongCount, &Error));

	FGameXXKRuntimeState WrongPrice = State;
	++WrongPrice.CardRun.RouteMerchant.Offers[0].Price;
	TestFalse(TEXT("saved relic prices are derived from catalog quality"),
		FGameXXKRouteMerchantRules::ValidateSavedStock(WrongPrice, &Error));

	FGameXXKRuntimeState WrongQuality = State;
	WrongQuality.CardRun.RouteMerchant.Offers[0].Quality =
		WrongQuality.CardRun.RouteMerchant.Offers[0].Quality == EGameXXKCardQuality::Common
			? EGameXXKCardQuality::Rare
			: EGameXXKCardQuality::Common;
	TestFalse(TEXT("saved relic quality must match the catalog"),
		FGameXXKRouteMerchantRules::ValidateSavedStock(WrongQuality, &Error));

	FGameXXKRuntimeState WrongSeed = State;
	WrongSeed.CardRun.RouteMerchant.OfferSeed ^= 1;
	TestFalse(TEXT("saved stock seed is derived from route, node, and refresh count"),
		FGameXXKRouteMerchantRules::ValidateSavedStock(WrongSeed, &Error));

	FGameXXKRuntimeState WrongRefreshIdentity = State;
	++WrongRefreshIdentity.CardRun.RouteMerchant.RefreshCount;
	TestFalse(TEXT("refresh count and every persisted offer identity must stay in sync"),
		FGameXXKRouteMerchantRules::ValidateSavedStock(WrongRefreshIdentity, &Error));

	FGameXXKRuntimeState WrongOfferIdentity = State;
	WrongOfferIdentity.CardRun.RouteMerchant.Offers[0].OfferId = TEXT("Merchant.Tampered.R.0");
	TestFalse(TEXT("saved offer identities are deterministic and slot-stable"),
		FGameXXKRouteMerchantRules::ValidateSavedStock(WrongOfferIdentity, &Error));

	FGameXXKRuntimeState SoldSnapshot = State;
	SoldSnapshot.CardRun.RouteMerchant.Offers[0].bSold = true;
	FGameXXKRelicInstance PurchasedRelic;
	PurchasedRelic.RelicId = SoldSnapshot.CardRun.RouteMerchant.Offers[0].ContentId;
	PurchasedRelic.Stacks = 1;
	PurchasedRelic.AcquisitionOrdinal = 1;
	SoldSnapshot.CardRun.Relics.Add(PurchasedRelic);
	SoldSnapshot.CardRun.NextRelicAcquisitionOrdinal = 1;
	TestTrue(TEXT("sold snapshot content remains valid after relic ownership changes"),
		FGameXXKRouteMerchantRules::ValidateSavedStock(SoldSnapshot, &Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteMerchantV10SaveMigrationTest,
	"GameXXK.Route.Merchant.SaveState.V10Migration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteMerchantV10SaveMigrationTest::RunTest(const FString& Parameters)
{
	FString Error;
	FGameXXKRuntimeState CurrentState;
	if (!TestTrue(TEXT("current-save fixture enters a generated route"), StartAcceptedGeneratedRoute(CurrentState))
		|| !TestTrue(TEXT("current-save fixture opens a generated merchant"), OpenGeneratedMerchant(CurrentState, &Error)))
	{
		return false;
	}
	const FGameXXKRouteMerchantState CurrentMerchant = CurrentState.CardRun.RouteMerchant;
	FGameXXKSaveState CurrentSave = UGameXXKMVPRules::MakeSaveState(CurrentState);
	FGameXXKSaveState CurrentRoundTrip;
	FGameXXKSaveMigrationReport CurrentReport;
	TestTrue(TEXT("a current-v10 save accepts canonical generated merchant stock"),
		FGameXXKSaveMigration::MigrateToCurrent(CurrentSave, CurrentRoundTrip, CurrentReport));
	TestTrue(TEXT("current-v10 validation preserves merchant stock byte-for-byte"),
		MerchantStatesMatch(CurrentRoundTrip.RuntimeState.CardRun.RouteMerchant, CurrentMerchant));
	FGameXXKRuntimeState OffscreenCurrentState = CurrentState;
	OffscreenCurrentState.Screen = EGameXXKScreen::DungeonMap;
	OffscreenCurrentState.CurrentMapId = TEXT("HuangshanRoute");
	OffscreenCurrentState.PendingRouteNodeId = INDEX_NONE;
	FGameXXKSaveState OffscreenCurrentSave = UGameXXKMVPRules::MakeSaveState(OffscreenCurrentState);
	FGameXXKSaveState OffscreenCurrentRoundTrip;
	FGameXXKSaveMigrationReport OffscreenCurrentReport;
	TestTrue(TEXT("a current-v10 save accepts retained stock after the merchant node is no longer pending"),
		FGameXXKSaveMigration::MigrateToCurrent(
			OffscreenCurrentSave,
			OffscreenCurrentRoundTrip,
			OffscreenCurrentReport));
	TestTrue(TEXT("offscreen current-v10 validation preserves merchant stock byte-for-byte"),
		MerchantStatesMatch(
			OffscreenCurrentRoundTrip.RuntimeState.CardRun.RouteMerchant,
			CurrentMerchant));

	FGameXXKRuntimeState LegacyState;
	if (!TestTrue(TEXT("v9 fixture enters a generated route"), StartAcceptedGeneratedRoute(LegacyState))
		|| !TestTrue(TEXT("v9 fixture generates stock before a later reward"), OpenGeneratedMerchant(LegacyState, &Error)))
	{
		return false;
	}
	const FGameXXKRouteMapNode* BattleNode = FindNodeOfKind(LegacyState, EGameXXKNodeKind::Battle);
	if (!TestNotNull(TEXT("v9 fixture has a generated battle node"), BattleNode))
	{
		return false;
	}
	LegacyState.Screen = EGameXXKScreen::DungeonMap;
	LegacyState.CurrentMapId = TEXT("HuangshanRoute");
	LegacyState.PendingRouteNodeId = INDEX_NONE;
	LegacyState.ReachableRouteNodeIds = {BattleNode->NodeId};
	if (!TestTrue(TEXT("v9 fixture starts a generated battle after retaining merchant stock"),
		UGameXXKMVPRules::SelectRouteNodeById(LegacyState, BattleNode->NodeId)))
	{
		return false;
	}
	ForceCardBattleVictory(LegacyState);
	if (!TestTrue(TEXT("v9 fixture creates a real pending route reward"),
		UGameXXKMVPRules::ResolveBattleVictory(LegacyState, false)))
	{
		return false;
	}
	TestEqual(TEXT("v9 fixture has three tiered pending reward options"), LegacyState.CardRun.PendingReward.Options.Num(), 3);

	const TArray<FGameXXKRouteMapNode> ExpectedNodes = LegacyState.RouteMapNodes;
	const TArray<FGameXXKRouteMapEdge> ExpectedEdges = LegacyState.RouteMapEdges;
	const int32 ExpectedCurrentNodeId = LegacyState.CurrentRouteNodeId;
	const int32 ExpectedPendingNodeId = LegacyState.PendingRouteNodeId;
	const bool bExpectedEconomyInitialized = LegacyState.CardRun.bRouteEconomyInitialized;
	const int32 ExpectedTravelMoney = LegacyState.CardRun.RouteTravelMoney;

	FGameXXKSaveState LegacySave = UGameXXKMVPRules::MakeSaveState(LegacyState);
	LegacySave.SaveVersion = FGameXXKSaveMigration::RouteMerchantStockSchemaIntroducedSaveVersion - 1;
	FGameXXKSaveState MigratedSave;
	FGameXXKSaveMigrationReport Report;
	TestTrue(TEXT("v9 migrates by discarding only its obsolete merchant stock schema"),
		FGameXXKSaveMigration::MigrateToCurrent(LegacySave, MigratedSave, Report));
	TestTrue(TEXT("v9 migration leaves a canonical empty merchant snapshot"),
		MerchantStatesMatch(MigratedSave.RuntimeState.CardRun.RouteMerchant, FGameXXKRouteMerchantState()));
	TestTrue(TEXT("v9 merchant migration preserves route nodes byte-for-byte"),
		RouteNodesMatch(MigratedSave.RuntimeState.RouteMapNodes, ExpectedNodes));
	TestTrue(TEXT("v9 merchant migration preserves route edges byte-for-byte"),
		RouteEdgesMatch(MigratedSave.RuntimeState.RouteMapEdges, ExpectedEdges));
	TestEqual(TEXT("v9 merchant migration preserves the current node"),
		MigratedSave.RuntimeState.CurrentRouteNodeId, ExpectedCurrentNodeId);
	TestEqual(TEXT("v9 merchant migration preserves the pending node"),
		MigratedSave.RuntimeState.PendingRouteNodeId, ExpectedPendingNodeId);
	TestEqual(TEXT("v9 migration clears the pre-tiering pending reward options"),
		MigratedSave.RuntimeState.CardRun.PendingReward.Options.Num(), 0);
	TestEqual(TEXT("v9 migration clears the pre-tiering pending reward cards"),
		MigratedSave.RuntimeState.CardRun.PendingReward.CardIds.Num(), 0);
	TestFalse(TEXT("v9 migration re-arms the reward gate for the next victory"),
		MigratedSave.RuntimeState.CardRun.bActiveBattleRewardResolved);
	TestEqual(TEXT("v9 merchant migration preserves route-economy initialization"),
		MigratedSave.RuntimeState.CardRun.bRouteEconomyInitialized, bExpectedEconomyInitialized);
	TestEqual(TEXT("v9 merchant migration preserves route travel money"),
		MigratedSave.RuntimeState.CardRun.RouteTravelMoney, ExpectedTravelMoney);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteMerchantSaveStateTest,
	"GameXXK.Route.Merchant.SaveState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteMerchantSaveStateTest::RunTest(const FString& Parameters)
{
	const FGameXXKRuntimeState NewRunState = UGameXXKMVPRules::CreateNewGame();
	const FGameXXKPendingRouteMerchantPurchase EmptyPendingPurchase;
	TestFalse(TEXT("required pending-purchase API defaults inactive"), EmptyPendingPurchase.bActive);
	TestEqual(TEXT("new run has no merchant source node"), NewRunState.CardRun.RouteMerchant.SourceNodeId, INDEX_NONE);
	TestEqual(TEXT("new run has no merchant seed"), NewRunState.CardRun.RouteMerchant.OfferSeed, 0);
	TestTrue(TEXT("new run has no merchant offers"), NewRunState.CardRun.RouteMerchant.Offers.IsEmpty());
	TestFalse(TEXT("new run has no pending merchant purchase"), NewRunState.CardRun.RouteMerchant.PendingPurchase.bActive);

	const FGameXXKSaveState LegacySave = MakeLegacyMerchantSnapshot();
	const TArray<FGameXXKRouteMapNode> ExpectedRouteNodes = LegacySave.RuntimeState.RouteMapNodes;
	const TArray<FGameXXKRouteMapEdge> ExpectedRouteEdges = LegacySave.RuntimeState.RouteMapEdges;
	const int32 ExpectedCurrentNodeId = LegacySave.RuntimeState.CurrentRouteNodeId;
	const int32 ExpectedPendingNodeId = LegacySave.RuntimeState.PendingRouteNodeId;
	const FGameXXKRouteProgress ExpectedRouteProgress = LegacySave.RuntimeState.CardRun.RouteProgress;
	TestTrue(TEXT("legacy fixture contains a nonempty merchant snapshot"), !LegacySave.RuntimeState.CardRun.RouteMerchant.Offers.IsEmpty());
	FGameXXKSaveState MigratedSave;
	FGameXXKSaveMigrationReport Report;
	TestTrue(TEXT("older save clears an obsolete merchant snapshot"), FGameXXKSaveMigration::MigrateToCurrent(LegacySave, MigratedSave, Report));
	TestTrue(TEXT("migration report succeeds"), Report.bSucceeded);
	TestEqual(TEXT("migrated legacy run has no merchant source node"), MigratedSave.RuntimeState.CardRun.RouteMerchant.SourceNodeId, INDEX_NONE);
	TestEqual(TEXT("migrated legacy run has no merchant seed"), MigratedSave.RuntimeState.CardRun.RouteMerchant.OfferSeed, 0);
	TestTrue(TEXT("migrated legacy run has no merchant offers"), MigratedSave.RuntimeState.CardRun.RouteMerchant.Offers.IsEmpty());
	TestFalse(TEXT("migrated legacy run has no pending merchant purchase"), MigratedSave.RuntimeState.CardRun.RouteMerchant.PendingPurchase.bActive);
	TestTrue(TEXT("legacy merchant reset leaves route nodes byte-identical"), RouteNodesMatch(MigratedSave.RuntimeState.RouteMapNodes, ExpectedRouteNodes));
	TestTrue(TEXT("legacy merchant reset leaves route edges byte-identical"), RouteEdgesMatch(MigratedSave.RuntimeState.RouteMapEdges, ExpectedRouteEdges));
	TestEqual(TEXT("legacy merchant reset leaves current node unchanged"), MigratedSave.RuntimeState.CurrentRouteNodeId, ExpectedCurrentNodeId);
	TestEqual(TEXT("legacy merchant reset leaves pending node unchanged"), MigratedSave.RuntimeState.PendingRouteNodeId, ExpectedPendingNodeId);
	TestTrue(
		TEXT("legacy merchant reset leaves three-chapter route progress byte-identical"),
		FGameXXKRouteProgress::StaticStruct()->CompareScriptStruct(
			&MigratedSave.RuntimeState.CardRun.RouteProgress,
			&ExpectedRouteProgress,
			PPF_None));
	return true;
}

#endif
