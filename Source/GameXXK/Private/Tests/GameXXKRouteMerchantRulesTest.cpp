#include "GameXXKMVPRules.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCardQualityRules.h"
#include "GameXXKCompanionCatalog.h"
#include "GameXXKEncounterRules.h"
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

	void ExpectRefreshDisabledAndRollback(
		FAutomationTestBase& Test,
		FGameXXKRuntimeState& State,
		const TCHAR* Label)
	{
		const FGameXXKRuntimeState Before = State;
		FGameXXKRouteMerchantView View;
		FString Error;
		Test.TestTrue(*FString::Printf(TEXT("%s view remains readable"), Label),
			FGameXXKRouteMerchantRules::GetView(State, View, &Error));
		Test.TestFalse(*FString::Printf(TEXT("%s view disables refresh"), Label), View.bRefreshEnabled);
		Test.TestFalse(*FString::Printf(TEXT("%s view exposes a reason"), Label), View.RefreshDisabledReason.IsEmpty());
		Test.TestTrue(*FString::Printf(TEXT("%s view is pure"), Label), RuntimeStatesMatch(State, Before));
		Test.TestFalse(*FString::Printf(TEXT("%s refresh rejects"), Label),
			FGameXXKRouteMerchantRules::Refresh(State, &Error));
		Test.TestFalse(*FString::Printf(TEXT("%s refresh reports a reason"), Label), Error.IsEmpty());
		Test.TestTrue(*FString::Printf(TEXT("%s refresh preserves the complete runtime"), Label),
			RuntimeStatesMatch(State, Before));
	}

	void ExpectStaleOfferViewDisabled(
		FAutomationTestBase& Test,
		FGameXXKRuntimeState& State,
		const FName OfferId,
		const FString& ExpectedReasonToken,
		const TCHAR* Label)
	{
		const FGameXXKRuntimeState Before = State;
		auto ValidateProjectedView = [&](const FGameXXKRouteMerchantView& View, const TCHAR* ProjectionLabel)
		{
			const FGameXXKRouteMerchantOfferView* Target = View.CardOffers.FindByPredicate(
				[OfferId](const FGameXXKRouteMerchantOfferView& Offer)
				{
					return Offer.SavedOffer.OfferId == OfferId;
				});
			Test.TestNotNull(*FString::Printf(TEXT("%s %s keeps target visible"), Label, ProjectionLabel), Target);
			if (!Target)
			{
				return;
			}
			Test.TestTrue(*FString::Printf(TEXT("%s %s keeps balance affordability separate"), Label, ProjectionLabel),
				Target->bAffordable);
			Test.TestFalse(*FString::Printf(TEXT("%s %s disables stale purchase"), Label, ProjectionLabel),
				Target->bPurchaseEnabled);
			Test.TestFalse(*FString::Printf(TEXT("%s %s exposes stale reason"), Label, ProjectionLabel),
				Target->DisabledReason.IsEmpty());
			Test.TestTrue(*FString::Printf(TEXT("%s %s reason matches live validation"), Label, ProjectionLabel),
				Target->DisabledReason.Contains(ExpectedReasonToken));
			Test.TestTrue(*FString::Printf(TEXT("%s %s leaves another valid offer enabled"), Label, ProjectionLabel),
				View.CardOffers.ContainsByPredicate([OfferId](const FGameXXKRouteMerchantOfferView& Offer)
				{
					return Offer.SavedOffer.OfferId != OfferId && Offer.bPurchaseEnabled;
				}));
		};

		FGameXXKRouteMerchantView ConstView;
		FString Error;
		const FGameXXKRuntimeState& ConstState = State;
		Test.TestTrue(*FString::Printf(TEXT("%s const view builds"), Label),
			FGameXXKRouteMerchantRules::GetView(ConstState, ConstView, &Error));
		ValidateProjectedView(ConstView, TEXT("const"));
		Test.TestTrue(*FString::Printf(TEXT("%s const view is pure"), Label), RuntimeStatesMatch(State, Before));

		FGameXXKRouteMerchantView MutableView;
		Test.TestTrue(*FString::Printf(TEXT("%s mutable view builds"), Label),
			FGameXXKRouteMerchantRules::GetView(State, MutableView, &Error));
		ValidateProjectedView(MutableView, TEXT("mutable"));
		Test.TestTrue(*FString::Printf(TEXT("%s mutable view is pure"), Label), RuntimeStatesMatch(State, Before));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteMerchantStockCompanionTest,
	"GameXXK.Route.Merchant.Rules.Stock.CarriedCardsAndPersistence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteMerchantStockCompanionTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = MakeMerchantState();
	FString Error;
	if (!TestTrue(TEXT("a locked route generates carried-card merchant stock"),
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
	TestEqual(TEXT("snapshot has exactly four card slots"), CountOffersOfKind(FirstStock, EGameXXKRouteMerchantOfferKind::Card), FGameXXKRouteMerchantRules::CardSlotCount);
	TestEqual(TEXT("snapshot has no relic slots"), CountOffersOfKind(FirstStock, EGameXXKRouteMerchantOfferKind::Relic), 0);
	TSet<FName> SeenCardIds;
	for (int32 Index = 0; Index < FirstStock.Offers.Num(); ++Index)
	{
		const FGameXXKRouteMerchantOffer& Offer = FirstStock.Offers[Index];
		TestFalse(*FString::Printf(TEXT("offer %d has a stable id"), Index), Offer.OfferId.IsNone());
		TestFalse(*FString::Printf(TEXT("offer %d is available in the full-catalog fixture"), Index), Offer.bUnavailable);
		TestFalse(*FString::Printf(TEXT("offer %d starts unsold"), Index), Offer.bSold);
		TestEqual(*FString::Printf(TEXT("slot %d is a card"), Index), Offer.Kind, EGameXXKRouteMerchantOfferKind::Card);
		TestFalse(*FString::Printf(TEXT("card slot %d has owner provenance"), Index), Offer.OwnerMemberId.IsNone());
		TestNotNull(*FString::Printf(TEXT("card slot %d resolves catalog content"), Index), FGameXXKCardCatalog::FindCardDefinition(Offer.ContentId));
		TestEqual(*FString::Printf(TEXT("card slot %d persists next quality"), Index), Offer.NextQuality,
			FGameXXKCardBattleAdapter::GetNextCardQuality(Offer.Quality));
		TestEqual(*FString::Printf(TEXT("card slot %d uses next-quality price"), Index), Offer.Price,
			FGameXXKCardQualityRules::GetCardPrice(Offer.NextQuality));
		TestFalse(*FString::Printf(TEXT("card slot %d is unique in its batch"), Index), SeenCardIds.Contains(Offer.ContentId));
		SeenCardIds.Add(Offer.ContentId);
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
	TestEqual(TEXT("view exposes four card slots"), View.CardOffers.Num(), 4);
	TestEqual(TEXT("view exposes zero relic slots"), View.RelicOffers.Num(), 0);
	TestEqual(TEXT("view exposes ordinary gold"), View.PlayerGold, State.PlayerGold);
	TestEqual(TEXT("legacy view balance alias mirrors ordinary gold"), View.RouteTravelMoney, State.PlayerGold);
	TestEqual(TEXT("view exposes first refresh cost"), View.RefreshCost, 20);
	TestTrue(TEXT("view enables affordable refresh"), View.bRefreshEnabled);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteMerchantStockFallbackTest,
	"GameXXK.Route.Merchant.Rules.Stock.CardShortageAndUnavailable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteMerchantStockFallbackTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState ExhaustedState = MakeMerchantState();
	ExhaustedState.CardRun.HeroSelectedCardIds.SetNum(1);
	FString Error;
	TestTrue(TEXT("short card pools still generate all four persistent slots"), FGameXXKRouteMerchantRules::EnsureStock(ExhaustedState, &Error));
	TestEqual(TEXT("exhausted snapshot still has four slots"), ExhaustedState.CardRun.RouteMerchant.Offers.Num(), FGameXXKRouteMerchantRules::TotalSlotCount);
	int32 AvailableCards = 0;
	int32 UnavailableCards = 0;
	for (int32 Index = 0; Index < ExhaustedState.CardRun.RouteMerchant.Offers.Num(); ++Index)
	{
		const FGameXXKRouteMerchantOffer& Offer = ExhaustedState.CardRun.RouteMerchant.Offers[Index];
		TestEqual(*FString::Printf(TEXT("exhausted slot %d keeps its kind"), Index), Offer.Kind, EGameXXKRouteMerchantOfferKind::Card);
		if (Offer.bUnavailable)
		{
			++UnavailableCards;
			TestFalse(TEXT("unavailable card keeps a stable offer id"), Offer.OfferId.IsNone());
			TestTrue(TEXT("unavailable card has no content"), Offer.ContentId.IsNone());
			TestTrue(TEXT("unavailable card has no owner"), Offer.OwnerMemberId.IsNone());
			TestEqual(TEXT("unavailable card has invalid quality"), Offer.Quality, EGameXXKCardQuality::Invalid);
			TestEqual(TEXT("unavailable card costs zero"), Offer.Price, 0);
		}
		else
		{
			++AvailableCards;
			TestEqual(TEXT("only configured carried card is preserved"), Offer.ContentId, ExhaustedState.CardRun.HeroSelectedCardIds[0]);
		}
	}
	TestEqual(TEXT("one carried card remains available"), AvailableCards, 1);
	TestEqual(TEXT("card shortage produces three explicit unavailable slots"), UnavailableCards, 3);
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
	TestEqual(TEXT("refresh preserves route travel money"), State.CardRun.RouteTravelMoney, MoneyBefore);
	TestEqual(TEXT("refresh debits ordinary gold once"), State.PlayerGold, PlayerGoldBefore - 20);
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
	Insufficient.PlayerGold = View.RefreshCost - 1;
	Insufficient.CardRun.RouteTravelMoney = MAX_int32;
	const FGameXXKRuntimeState InsufficientBefore = Insufficient;
	TestFalse(TEXT("refresh rejects insufficient ordinary gold"), FGameXXKRouteMerchantRules::Refresh(Insufficient, &Error));
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
	FGameXXKRouteMerchantOffer* BaseOffer = FindAvailableOffer(Base, EGameXXKRouteMerchantOfferKind::Card);
	TestNotNull(TEXT("purchase fixture has a card offer"), BaseOffer);
	if (!BaseOffer)
	{
		return false;
	}
	const FName OfferId = BaseOffer->OfferId;
	const int32 Price = BaseOffer->Price;

	FGameXXKRuntimeState Insufficient = Base;
	Insufficient.PlayerGold = Price - 1;
	Insufficient.CardRun.RouteTravelMoney = MAX_int32;
	const FGameXXKRuntimeState BeforePreview = Insufficient;
	FGameXXKRouteMerchantPurchasePreview Preview;
	TestFalse(TEXT("insufficient preview rejects"), FGameXXKRouteMerchantRules::PreviewPurchase(Insufficient, OfferId, NAME_None, Preview, &Error));
	TestEqual(TEXT("insufficient preview reports typed reason"), Preview.Failure, EGameXXKRouteMerchantPurchaseFailure::InsufficientOrdinaryGold);
	TestEqual(TEXT("insufficient preview includes the saved offer"), Preview.Offer.OfferId, OfferId);
	TestEqual(TEXT("insufficient preview includes balance before"), Preview.BalanceBefore, Price - 1);
	TestEqual(TEXT("insufficient preview includes price"), Preview.Price, Price);
	TestTrue(TEXT("preview is pure on failure"), RuntimeStatesMatch(Insufficient, BeforePreview));
	ExpectPurchaseFailureAndRollback(
		*this,
		Insufficient,
		OfferId,
		NAME_None,
		EGameXXKRouteMerchantPurchaseFailure::InsufficientOrdinaryGold,
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
	UnavailableOffer.OwnerMemberId = NAME_None;
	UnavailableOffer.Quality = EGameXXKCardQuality::Invalid;
	UnavailableOffer.NextQuality = EGameXXKCardQuality::Invalid;
	UnavailableOffer.Price = 0;
	UnavailableOffer.bUnavailable = true;
	ExpectPurchaseFailureAndRollback(
		*this,
		Unavailable,
		UnavailableOffer.OfferId,
		NAME_None,
		EGameXXKRouteMerchantPurchaseFailure::OfferUnavailable,
		TEXT("unavailable-slot purchase"));

	FGameXXKRuntimeState MaxQuality = Base;
	const FName MaxOfferId = BaseOffer->OfferId;
	MaxQuality.CardRun.UpgradedCardQualities.Add(BaseOffer->ContentId, EGameXXKCardQuality::Epic);
	ExpectPurchaseFailureAndRollback(
		*this, MaxQuality, MaxOfferId, NAME_None,
		EGameXXKRouteMerchantPurchaseFailure::CardAlreadyMaxQuality,
		TEXT("max-quality card"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteMerchantPurchaseCardQualityTest,
	"GameXXK.Route.Merchant.Rules.Purchase.CardQualityAtomicCommit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteMerchantPurchaseCardQualityTest::RunTest(const FString& Parameters)
{
	FString Error;
	FGameXXKRuntimeState State = MakeMerchantState();
	TestTrue(TEXT("card fixture generates stock"), FGameXXKRouteMerchantRules::EnsureStock(State, &Error));
	FGameXXKRouteMerchantOffer* Offer = FindAvailableOffer(State, EGameXXKRouteMerchantOfferKind::Card);
	TestNotNull(TEXT("card fixture has an available card"), Offer);
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
	TestTrue(TEXT("card preview succeeds"), FGameXXKRouteMerchantRules::PreviewPurchase(State, Offer->OfferId, NAME_None, Preview, &Error));
	TestTrue(TEXT("card preview can purchase"), Preview.bCanPurchase);
	TestFalse(TEXT("card preview requires no replacement"), Preview.bRequiresReplacement);
	TestTrue(TEXT("card preview is pure"), RuntimeStatesMatch(State, BeforePreview));
	FGameXXKRouteMerchantPurchaseResult Result;
	TestTrue(TEXT("card purchase commits"), FGameXXKRouteMerchantRules::Purchase(State, Offer->OfferId, NAME_None, Result));
	TestEqual(TEXT("card purchase adds no relic"), State.CardRun.Relics.Num(), RelicsBefore);
	TestEqual(TEXT("card purchase preserves route money"), State.CardRun.RouteTravelMoney, MoneyBefore);
	TestEqual(TEXT("card purchase debits ordinary gold"), State.PlayerGold, GoldBefore - SavedOffer.Price);
	TestEqual(TEXT("card purchase upgrades authoritative quality"),
		FGameXXKCardBattleAdapter::GetConfiguredCardQuality(State.CardRun, SavedOffer.ContentId), SavedOffer.NextQuality);
	const FGameXXKRouteMerchantOffer* SoldOffer = State.CardRun.RouteMerchant.Offers.FindByPredicate([&SavedOffer](const FGameXXKRouteMerchantOffer& Candidate)
	{
		return Candidate.OfferId == SavedOffer.OfferId;
	});
	TestTrue(TEXT("card purchase marks offer sold"), SoldOffer && SoldOffer->bSold);
	TestEqual(TEXT("card purchase remains in merchant"), State.Screen, EGameXXKScreen::RouteMerchant);
	const FGameXXKRuntimeState SoldCardBeforeReopen = State;
	TestTrue(TEXT("same-node reopen accepts sold card stock"), FGameXXKRouteMerchantRules::EnsureStock(State, &Error));
	TestTrue(TEXT("sold card reopen is byte-stable"), RuntimeStatesMatch(State, SoldCardBeforeReopen));
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
	if (!TestTrue(TEXT("saved-stock fixture generates canonical card-upgrade stock"),
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
	EmptyOffer.OwnerMemberId = NAME_None;
	EmptyOffer.Quality = EGameXXKCardQuality::Invalid;
	EmptyOffer.NextQuality = EGameXXKCardQuality::Invalid;
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
	TestFalse(TEXT("saved stock requires exactly four card slots"),
		FGameXXKRouteMerchantRules::ValidateSavedStock(WrongCount, &Error));

	FGameXXKRuntimeState WrongPrice = State;
	++WrongPrice.CardRun.RouteMerchant.Offers[0].Price;
	TestFalse(TEXT("saved card-upgrade price is derived from next quality"),
		FGameXXKRouteMerchantRules::ValidateSavedStock(WrongPrice, &Error));

	FGameXXKRuntimeState WrongQuality = State;
	WrongQuality.CardRun.RouteMerchant.Offers[0].Quality =
		WrongQuality.CardRun.RouteMerchant.Offers[0].Quality == EGameXXKCardQuality::Common
			? EGameXXKCardQuality::Rare
			: EGameXXKCardQuality::Common;
	TestFalse(TEXT("saved card quality transition must remain canonical"),
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
	TestTrue(TEXT("sold card-upgrade snapshot remains structurally valid"),
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteMerchantCarriedCardUpgradeTest,
	"GameXXK.MVP.RouteMerchant.Rules.FourCarriedCardUpgrades",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteMerchantCarriedCardUpgradeTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = MakeMerchantState();
	const int32 InitialRouteMoney = State.CardRun.RouteTravelMoney;
	const int32 InitialPlayerGold = State.PlayerGold;
	const TArray<FName> InitialHeroCards = State.CardRun.HeroSelectedCardIds;
	const int32 InitialRelicCount = State.CardRun.Relics.Num();
	FString Error;
	TestTrue(TEXT("merchant stock persists"), FGameXXKRouteMerchantRules::EnsureStock(State, &Error));
	FGameXXKRouteMerchantView View;
	TestTrue(TEXT("view builds"), FGameXXKRouteMerchantRules::GetView(State, View, &Error));
	TestEqual(TEXT("exactly four card slots"), View.CardOffers.Num(), 4);
	TestEqual(TEXT("no relic slots"), View.RelicOffers.Num(), 0);

	TSet<FName> CardIds;
	TArray<FGameXXKRouteMerchantOfferView> Purchasable;
	for (const FGameXXKRouteMerchantOfferView& OfferView : View.CardOffers)
	{
		if (!OfferView.SavedOffer.bUnavailable)
		{
			TestFalse(TEXT("offer belongs to deployed owner"), OfferView.SavedOffer.OwnerMemberId.IsNone());
			TestFalse(TEXT("offer card is unique"), CardIds.Contains(OfferView.SavedOffer.ContentId));
			CardIds.Add(OfferView.SavedOffer.ContentId);
			TestTrue(TEXT("offer is below Epic"), OfferView.SavedOffer.Quality < EGameXXKCardQuality::Epic);
			TestEqual(TEXT("next quality is one tier higher"), OfferView.SavedOffer.NextQuality,
				FGameXXKCardBattleAdapter::GetNextCardQuality(OfferView.SavedOffer.Quality));
		}
		if (OfferView.bPurchaseEnabled)
		{
			Purchasable.Add(OfferView);
		}
	}
	if (!TestTrue(TEXT("at least two offers can be bought"), Purchasable.Num() >= 2))
	{
		return false;
	}

	FGameXXKRuntimeState IndependentTwin = MakeMerchantState();
	TestTrue(TEXT("same state and seed generate stock"), FGameXXKRouteMerchantRules::EnsureStock(IndependentTwin, &Error));
	TestTrue(TEXT("same state and seed produce identical stock"),
		MerchantStatesMatch(IndependentTwin.CardRun.RouteMerchant, State.CardRun.RouteMerchant));

	FGameXXKRouteMerchantPurchaseResult First;
	FGameXXKRouteMerchantPurchaseResult Second;
	TestTrue(TEXT("first purchase commits"),
		FGameXXKRouteMerchantRules::Purchase(State, Purchasable[0].SavedOffer.OfferId, NAME_None, First));
	TestTrue(TEXT("second purchase commits"),
		FGameXXKRouteMerchantRules::Purchase(State, Purchasable[1].SavedOffer.OfferId, NAME_None, Second));
	TestTrue(TEXT("both offers are sold"), First.bPurchased && Second.bPurchased);
	TestEqual(TEXT("first authoritative quality upgraded"),
		FGameXXKCardBattleAdapter::GetConfiguredCardQuality(State.CardRun, First.CardId), First.FinalQuality);
	TestEqual(TEXT("second authoritative quality upgraded"),
		FGameXXKCardBattleAdapter::GetConfiguredCardQuality(State.CardRun, Second.CardId), Second.FinalQuality);
	TestEqual(TEXT("ordinary gold pays for both upgrades"),
		State.PlayerGold, InitialPlayerGold - First.Price - Second.Price);
	TestEqual(TEXT("route-travel money is bit-identical after both upgrades"),
		State.CardRun.RouteTravelMoney, InitialRouteMoney);
	TestEqual(TEXT("merchant upgrades do not replace carried cards"), State.CardRun.HeroSelectedCardIds, InitialHeroCards);
	TestEqual(TEXT("merchant upgrades do not acquire relics"), State.CardRun.Relics.Num(), InitialRelicCount);

	const FGameXXKRuntimeState BeforeRepurchase = State;
	FGameXXKRouteMerchantPurchaseResult Repurchase;
	TestFalse(TEXT("each offer can be bought only once"),
		FGameXXKRouteMerchantRules::Purchase(State, First.OfferId, NAME_None, Repurchase));
	TestEqual(TEXT("sold failure is typed"), Repurchase.Failure, EGameXXKRouteMerchantPurchaseFailure::OfferAlreadySold);
	TestTrue(TEXT("sold repurchase is atomic"), RuntimeStatesMatch(State, BeforeRepurchase));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteMerchantPoolAndPlaceholderTest,
	"GameXXK.MVP.RouteMerchant.Rules.EffectivePoolOrderAndPlaceholders",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteMerchantPoolAndPlaceholderTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = MakeMerchantState();
	const TArray<FName> ProfessionCards = FindCardIdsByOwner(EGameXXKCardOwner::Profession);
	FGameXXKPermanentCompanion Companion;
	Companion.InstanceId = TEXT("Companion.Test.Blade");
	Companion.Role = EGameXXKCharacterRole::Blade;
	Companion.bIsActive = true;
	Companion.SelectedCardIds = {ProfessionCards[0], ProfessionCards[1], ProfessionCards[2]};
	State.CardRun.CompanionRoster.PermanentCompanions.Add(Companion);
	State.CardRun.PartySelection.ActivePermanentCompanionInstanceId = Companion.InstanceId;
	const FGameXXKQuestNpcDefinition& QuestNpc = FGameXXKCompanionCatalog::GetQuestNpcDefinitions()[0];
	State.CardRun.ActiveTemporaryQuestNpcId = QuestNpc.NpcId;
	State.CardRun.PartySelection.QuestNpc.NpcId = QuestNpc.NpcId;
	State.CardRun.PartySelection.QuestNpc.SelectedCardIds = {
		QuestNpc.FixedCardIds[0], QuestNpc.FixedCardIds[1], QuestNpc.FixedCardIds[2]};

	TArray<FGameXXKRouteMerchantRules::FDeployedCardCandidate> Pool;
	FString Error;
	TestTrue(TEXT("effective deployed pool builds"),
		FGameXXKRouteMerchantRules::BuildEffectiveDeployedCardPool(State, Pool, &Error));
	TestTrue(TEXT("hero cards lead the effective pool"),
		Pool.Num() > 0 && Pool[0].OwnerMemberId == FName(TEXT("Player")));
	const FName ActiveCompanionId = State.CardRun.PartySelection.ActivePermanentCompanionInstanceId;
	const int32 CompanionIndex = Pool.IndexOfByPredicate([ActiveCompanionId](const auto& Candidate)
	{
		return Candidate.OwnerMemberId == ActiveCompanionId;
	});
	const int32 QuestNpcIndex = Pool.IndexOfByPredicate([&QuestNpc](const auto& Candidate)
	{
		return Candidate.OwnerMemberId == QuestNpc.NpcId;
	});
	TestTrue(TEXT("active companion follows hero in source order"), CompanionIndex > 0);
	TestTrue(TEXT("active task NPC follows active companion in source order"), QuestNpcIndex > CompanionIndex);
	FGameXXKRuntimeState EpicFiltered = State;
	EpicFiltered.CardRun.UpgradedCardQualities.Add(
		EpicFiltered.CardRun.HeroSelectedCardIds[0], EGameXXKCardQuality::Epic);
	TArray<FGameXXKRouteMerchantRules::FDeployedCardCandidate> EpicFilteredPool;
	TestTrue(TEXT("Epic-filtered pool builds"),
		FGameXXKRouteMerchantRules::BuildEffectiveDeployedCardPool(EpicFiltered, EpicFilteredPool, &Error));
	TestFalse(TEXT("Epic configured cards are excluded from offers"),
		EpicFilteredPool.ContainsByPredicate([&EpicFiltered](const auto& Candidate)
		{
			return Candidate.CardId == EpicFiltered.CardRun.HeroSelectedCardIds[0];
		}));

	FGameXXKRuntimeState Duplicate = State;
	FGameXXKPermanentCompanion* ActiveCompanion = Duplicate.CardRun.CompanionRoster.PermanentCompanions.FindByPredicate(
		[ActiveCompanionId](const FGameXXKPermanentCompanion& Companion)
		{
			return Companion.InstanceId == ActiveCompanionId;
		});
	if (TestNotNull(TEXT("fixture owns its active companion"), ActiveCompanion))
	{
		ActiveCompanion->SelectedCardIds.Insert(Duplicate.CardRun.HeroSelectedCardIds[0], 0);
		TArray<FGameXXKRouteMerchantRules::FDeployedCardCandidate> Deduplicated;
		TestTrue(TEXT("duplicate pool still builds"),
			FGameXXKRouteMerchantRules::BuildEffectiveDeployedCardPool(Duplicate, Deduplicated, &Error));
		int32 DuplicateCount = 0;
		for (const auto& Candidate : Deduplicated)
		{
			DuplicateCount += Candidate.CardId == Duplicate.CardRun.HeroSelectedCardIds[0] ? 1 : 0;
		}
		TestEqual(TEXT("cross-owner duplicate card id appears exactly once"), DuplicateCount, 1);
		const auto* Kept = Deduplicated.FindByPredicate([&Duplicate](const auto& Candidate)
		{
			return Candidate.CardId == Duplicate.CardRun.HeroSelectedCardIds[0];
		});
		TestTrue(TEXT("earliest deployed owner retains duplicate provenance"),
			Kept && Kept->OwnerMemberId == FName(TEXT("Player")));
	}

	FGameXXKRuntimeState Short = MakeMerchantState();
	Short.CardRun.HeroSelectedCardIds.SetNum(2);
	Short.CardRun.PartySelection.ActivePermanentCompanionInstanceId = NAME_None;
	for (FGameXXKPermanentCompanion& ShortCompanion : Short.CardRun.CompanionRoster.PermanentCompanions)
	{
		ShortCompanion.bIsActive = false;
	}
	FGameXXKRuntimeState ShortTwin = Short;
	TestTrue(TEXT("short carried pool generates"), FGameXXKRouteMerchantRules::EnsureStock(Short, &Error));
	TestTrue(TEXT("short carried pool deterministically regenerates"), FGameXXKRouteMerchantRules::EnsureStock(ShortTwin, &Error));
	TestTrue(TEXT("short stock is deterministic"),
		MerchantStatesMatch(Short.CardRun.RouteMerchant, ShortTwin.CardRun.RouteMerchant));
	FGameXXKRouteMerchantView ShortView;
	TestTrue(TEXT("short view builds"), FGameXXKRouteMerchantRules::GetView(Short, ShortView, &Error));
	TestEqual(TEXT("short view still exposes four slots"), ShortView.CardOffers.Num(), 4);
	int32 UnavailableCount = 0;
	for (const FGameXXKRouteMerchantOfferView& Offer : ShortView.CardOffers)
	{
		UnavailableCount += Offer.SavedOffer.bUnavailable ? 1 : 0;
	}
	TestEqual(TEXT("short view exposes two unavailable placeholders"), UnavailableCount, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteMerchantRefreshAndStaleTest,
	"GameXXK.MVP.RouteMerchant.Rules.RefreshAndStaleTransactions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteMerchantRefreshAndStaleTest::RunTest(const FString& Parameters)
{
	FString Error;
	FGameXXKRuntimeState State = MakeMerchantState();
	TestTrue(TEXT("refresh fixture generates"), FGameXXKRouteMerchantRules::EnsureStock(State, &Error));
	FGameXXKRouteMerchantOffer* PurchasedOffer = FindAvailableOffer(State, EGameXXKRouteMerchantOfferKind::Card);
	if (!TestNotNull(TEXT("refresh fixture has a card"), PurchasedOffer))
	{
		return false;
	}
	const FGameXXKRouteMerchantOffer SavedPurchased = *PurchasedOffer;
	FGameXXKRouteMerchantPurchaseResult Purchased;
	TestTrue(TEXT("one offer buys before refresh"),
		FGameXXKRouteMerchantRules::Purchase(State, SavedPurchased.OfferId, NAME_None, Purchased));
	const int32 GoldBeforeRefresh = State.PlayerGold;
	const int32 RouteMoneyBeforeRefresh = State.CardRun.RouteTravelMoney;
	TestTrue(TEXT("refresh commits"), FGameXXKRouteMerchantRules::Refresh(State, &Error));
	TestEqual(TEXT("refresh count advances"), State.CardRun.RouteMerchant.RefreshCount, 1);
	TestEqual(TEXT("refresh uses ordinary gold"), State.PlayerGold, GoldBeforeRefresh - 20);
	TestEqual(TEXT("refresh never changes route money"), State.CardRun.RouteTravelMoney, RouteMoneyBeforeRefresh);
	TestTrue(TEXT("sold card remains visible and sold after refresh"),
		State.CardRun.RouteMerchant.Offers.ContainsByPredicate([&SavedPurchased](const auto& Offer)
		{
			return Offer.ContentId == SavedPurchased.ContentId && Offer.bSold;
		}));
	TestEqual(TEXT("sold card quality upgrade persists through refresh"),
		FGameXXKCardBattleAdapter::GetConfiguredCardQuality(State.CardRun, SavedPurchased.ContentId),
		SavedPurchased.NextQuality);

	FGameXXKRuntimeState NoGold = State;
	NoGold.PlayerGold = FGameXXKRouteMerchantRules::GetRefreshCost(NoGold.CardRun.RouteMerchant.RefreshCount) - 1;
	NoGold.CardRun.RouteTravelMoney = MAX_int32;
	const FGameXXKRuntimeState NoGoldBefore = NoGold;
	TestFalse(TEXT("route money cannot fund refresh"), FGameXXKRouteMerchantRules::Refresh(NoGold, &Error));
	TestTrue(TEXT("failed refresh is atomic"), RuntimeStatesMatch(NoGold, NoGoldBefore));

	FGameXXKRuntimeState StaleQuality = MakeMerchantState();
	TestTrue(TEXT("stale quality fixture generates"), FGameXXKRouteMerchantRules::EnsureStock(StaleQuality, &Error));
	FGameXXKRouteMerchantOffer* StaleQualityOffer = FindAvailableOffer(StaleQuality, EGameXXKRouteMerchantOfferKind::Card);
	if (TestNotNull(TEXT("stale quality fixture has a card"), StaleQualityOffer))
	{
		const FName StaleOfferId = StaleQualityOffer->OfferId;
		StaleQuality.CardRun.UpgradedCardQualities.Add(StaleQualityOffer->ContentId, StaleQualityOffer->NextQuality);
		ExpectPurchaseFailureAndRollback(*this, StaleQuality, StaleOfferId, NAME_None,
			EGameXXKRouteMerchantPurchaseFailure::StaleCardQuality, TEXT("stale quality purchase"));
	}

	FGameXXKRuntimeState StaleCarry = MakeMerchantState();
	StaleCarry.CardRun.PartySelection.ActivePermanentCompanionInstanceId = NAME_None;
	for (FGameXXKPermanentCompanion& Companion : StaleCarry.CardRun.CompanionRoster.PermanentCompanions)
	{
		Companion.bIsActive = false;
	}
	TestTrue(TEXT("stale carry fixture generates"), FGameXXKRouteMerchantRules::EnsureStock(StaleCarry, &Error));
	FGameXXKRouteMerchantOffer* StaleCarryOffer = FindAvailableOffer(StaleCarry, EGameXXKRouteMerchantOfferKind::Card);
	if (TestNotNull(TEXT("stale carry fixture has a hero card"), StaleCarryOffer))
	{
		const FName StaleOfferId = StaleCarryOffer->OfferId;
		StaleCarry.CardRun.HeroSelectedCardIds.Remove(StaleCarryOffer->ContentId);
		ExpectPurchaseFailureAndRollback(*this, StaleCarry, StaleOfferId, NAME_None,
			EGameXXKRouteMerchantPurchaseFailure::CardNoLongerCarried, TEXT("stale carried card purchase"));
	}

	FGameXXKRuntimeState StaleOwner = MakeMerchantState();
	StaleOwner.CardRun.HeroSelectedCardIds.Reset();
	const TArray<FName> ProfessionCards = FindCardIdsByOwner(EGameXXKCardOwner::Profession);
	FGameXXKPermanentCompanion OwnerCompanion;
	OwnerCompanion.InstanceId = TEXT("Companion.Test.StaleOwner");
	OwnerCompanion.Role = EGameXXKCharacterRole::Blade;
	OwnerCompanion.bIsActive = true;
	OwnerCompanion.SelectedCardIds = {ProfessionCards[0], ProfessionCards[1]};
	StaleOwner.CardRun.CompanionRoster.PermanentCompanions.Add(OwnerCompanion);
	StaleOwner.CardRun.PartySelection.ActivePermanentCompanionInstanceId = OwnerCompanion.InstanceId;
	TestTrue(TEXT("stale owner fixture generates"), FGameXXKRouteMerchantRules::EnsureStock(StaleOwner, &Error));
	FGameXXKRouteMerchantOffer* StaleOwnerOffer = FindAvailableOffer(StaleOwner, EGameXXKRouteMerchantOfferKind::Card);
	if (TestNotNull(TEXT("stale owner fixture has a companion card"), StaleOwnerOffer))
	{
		const FName StaleOwnerOfferId = StaleOwnerOffer->OfferId;
		StaleOwner.CardRun.PartySelection.ActivePermanentCompanionInstanceId = NAME_None;
		StaleOwner.CardRun.CompanionRoster.PermanentCompanions.Last().bIsActive = false;
		ExpectPurchaseFailureAndRollback(*this, StaleOwner, StaleOwnerOfferId, NAME_None,
			EGameXXKRouteMerchantPurchaseFailure::OwnerNoLongerDeployed, TEXT("stale owner purchase"));
	}

	FGameXXKRuntimeState ExhaustedRefresh = MakeMerchantState();
	ExhaustedRefresh.CardRun.HeroSelectedCardIds.SetNum(1);
	TestTrue(TEXT("exhausted refresh fixture generates"),
		FGameXXKRouteMerchantRules::EnsureStock(ExhaustedRefresh, &Error));
	FGameXXKRouteMerchantOffer* OnlyOffer = FindAvailableOffer(ExhaustedRefresh, EGameXXKRouteMerchantOfferKind::Card);
	if (TestNotNull(TEXT("exhausted refresh fixture has one offer"), OnlyOffer))
	{
		FGameXXKRouteMerchantPurchaseResult OnlyPurchase;
		TestTrue(TEXT("only carried card buys"),
			FGameXXKRouteMerchantRules::Purchase(ExhaustedRefresh, OnlyOffer->OfferId, NAME_None, OnlyPurchase));
		ExpectRefreshDisabledAndRollback(*this, ExhaustedRefresh, TEXT("exhausted placeholder-only pool"));
		const TArray<FName> HeroCards = FindCardIdsByOwner(EGameXXKCardOwner::Hero);
		const FName* NewlyCarriedCardPtr = HeroCards.FindByPredicate([&OnlyPurchase](const FName CardId)
		{
			return CardId != OnlyPurchase.CardId;
		});
		const FName NewlyCarriedCard = NewlyCarriedCardPtr ? *NewlyCarriedCardPtr : NAME_None;
		if (TestFalse(TEXT("fixture finds a newly legal carried card"), NewlyCarriedCard.IsNone()))
		{
			ExhaustedRefresh.CardRun.HeroSelectedCardIds.Add(NewlyCarriedCard);
			FGameXXKRouteMerchantView NewlyEligibleView;
			TestTrue(TEXT("newly legal carried card keeps view readable"),
				FGameXXKRouteMerchantRules::GetView(ExhaustedRefresh, NewlyEligibleView, &Error));
			TestTrue(TEXT("newly legal candidate re-enables refresh for placeholder slots"),
				NewlyEligibleView.bRefreshEnabled);
			const int32 NewlyEligibleRouteMoney = ExhaustedRefresh.CardRun.RouteTravelMoney;
			TestTrue(TEXT("newly legal candidate permits refresh"),
				FGameXXKRouteMerchantRules::Refresh(ExhaustedRefresh, &Error));
			TestEqual(TEXT("newly legal refresh preserves route money"),
				ExhaustedRefresh.CardRun.RouteTravelMoney, NewlyEligibleRouteMoney);
		}
	}

	FGameXXKRuntimeState RefreshStaleCarry = MakeMerchantState();
	TestTrue(TEXT("stale refresh carry fixture generates"),
		FGameXXKRouteMerchantRules::EnsureStock(RefreshStaleCarry, &Error));
	FGameXXKRouteMerchantOffer* RefreshStaleCarryOffer =
		FindAvailableOffer(RefreshStaleCarry, EGameXXKRouteMerchantOfferKind::Card);
	if (TestNotNull(TEXT("stale refresh carry fixture has an offer"), RefreshStaleCarryOffer))
	{
		const FName StaleViewOfferId = RefreshStaleCarryOffer->OfferId;
		RefreshStaleCarry.CardRun.HeroSelectedCardIds.Remove(RefreshStaleCarryOffer->ContentId);
		ExpectStaleOfferViewDisabled(
			*this, RefreshStaleCarry, StaleViewOfferId, TEXT("no longer carries"),
			TEXT("unsold card no longer carried"));
		ExpectRefreshDisabledAndRollback(*this, RefreshStaleCarry, TEXT("unsold card no longer carried"));
	}

	FGameXXKRuntimeState RefreshStaleQuality = MakeMerchantState();
	TestTrue(TEXT("stale refresh quality fixture generates"),
		FGameXXKRouteMerchantRules::EnsureStock(RefreshStaleQuality, &Error));
	FGameXXKRouteMerchantOffer* RefreshStaleQualityOffer =
		FindAvailableOffer(RefreshStaleQuality, EGameXXKRouteMerchantOfferKind::Card);
	if (TestNotNull(TEXT("stale refresh quality fixture has an offer"), RefreshStaleQualityOffer))
	{
		const FName StaleViewOfferId = RefreshStaleQualityOffer->OfferId;
		RefreshStaleQuality.CardRun.UpgradedCardQualities.Add(
			RefreshStaleQualityOffer->ContentId,
			RefreshStaleQualityOffer->NextQuality);
		ExpectStaleOfferViewDisabled(
			*this, RefreshStaleQuality, StaleViewOfferId, TEXT("quality changed"),
			TEXT("unsold card quality changed"));
		ExpectRefreshDisabledAndRollback(*this, RefreshStaleQuality, TEXT("unsold card quality changed"));
	}

	FGameXXKRuntimeState RefreshMaxQuality = MakeMerchantState();
	TestTrue(TEXT("max refresh quality fixture generates"),
		FGameXXKRouteMerchantRules::EnsureStock(RefreshMaxQuality, &Error));
	FGameXXKRouteMerchantOffer* RefreshMaxQualityOffer =
		FindAvailableOffer(RefreshMaxQuality, EGameXXKRouteMerchantOfferKind::Card);
	if (TestNotNull(TEXT("max refresh quality fixture has an offer"), RefreshMaxQualityOffer))
	{
		const FName StaleViewOfferId = RefreshMaxQualityOffer->OfferId;
		RefreshMaxQuality.CardRun.UpgradedCardQualities.Add(
			RefreshMaxQualityOffer->ContentId,
			EGameXXKCardQuality::Epic);
		ExpectStaleOfferViewDisabled(
			*this, RefreshMaxQuality, StaleViewOfferId, TEXT("Epic"),
			TEXT("unsold card reached max quality"));
		ExpectRefreshDisabledAndRollback(*this, RefreshMaxQuality, TEXT("unsold card reached max quality"));
	}

	FGameXXKRuntimeState RefreshStaleOwner = MakeMerchantState();
	RefreshStaleOwner.CardRun.HeroSelectedCardIds.SetNum(1);
	FGameXXKPermanentCompanion RefreshOwnerCompanion;
	RefreshOwnerCompanion.InstanceId = TEXT("Companion.Test.RefreshStaleOwner");
	RefreshOwnerCompanion.Role = EGameXXKCharacterRole::Blade;
	RefreshOwnerCompanion.bIsActive = true;
	RefreshOwnerCompanion.SelectedCardIds = {ProfessionCards[0], ProfessionCards[1], ProfessionCards[2]};
	RefreshStaleOwner.CardRun.CompanionRoster.PermanentCompanions.Add(RefreshOwnerCompanion);
	RefreshStaleOwner.CardRun.PartySelection.ActivePermanentCompanionInstanceId = RefreshOwnerCompanion.InstanceId;
	TestTrue(TEXT("stale refresh owner fixture generates"),
		FGameXXKRouteMerchantRules::EnsureStock(RefreshStaleOwner, &Error));
	FGameXXKRouteMerchantOffer* RefreshStaleOwnerOffer =
		RefreshStaleOwner.CardRun.RouteMerchant.Offers.FindByPredicate(
			[OwnerId = RefreshOwnerCompanion.InstanceId](const FGameXXKRouteMerchantOffer& Offer)
			{
				return Offer.OwnerMemberId == OwnerId && !Offer.bUnavailable && !Offer.bSold;
			});
	if (TestNotNull(TEXT("stale refresh owner fixture offers a companion card"), RefreshStaleOwnerOffer))
	{
		const FName StaleViewOfferId = RefreshStaleOwnerOffer->OfferId;
		RefreshStaleOwner.CardRun.PartySelection.ActivePermanentCompanionInstanceId = NAME_None;
		RefreshStaleOwner.CardRun.CompanionRoster.PermanentCompanions.Last().bIsActive = false;
		ExpectStaleOfferViewDisabled(
			*this, RefreshStaleOwner, StaleViewOfferId, TEXT("no longer deployed"),
			TEXT("unsold owner no longer deployed"));
		ExpectRefreshDisabledAndRollback(*this, RefreshStaleOwner, TEXT("unsold owner no longer deployed"));
	}

	FGameXXKRuntimeState Legacy = MakeMerchantState();
	TestTrue(TEXT("legacy fixture first generates current stock"), FGameXXKRouteMerchantRules::EnsureStock(Legacy, &Error));
	for (FGameXXKRouteMerchantOffer& Offer : Legacy.CardRun.RouteMerchant.Offers)
	{
		Offer.Kind = EGameXXKRouteMerchantOfferKind::Relic;
		Offer.OwnerMemberId = NAME_None;
		Offer.NextQuality = EGameXXKCardQuality::Invalid;
	}
	FGameXXKRuntimeState LegacyTwin = Legacy;
	TestTrue(TEXT("legacy relic snapshot normalizes on first ensure"), FGameXXKRouteMerchantRules::EnsureStock(Legacy, &Error));
	TestTrue(TEXT("same legacy relic snapshot normalizes deterministically"), FGameXXKRouteMerchantRules::EnsureStock(LegacyTwin, &Error));
	TestTrue(TEXT("legacy normalization yields identical card stock"),
		MerchantStatesMatch(Legacy.CardRun.RouteMerchant, LegacyTwin.CardRun.RouteMerchant));
	TestEqual(TEXT("legacy normalization removes every relic offer"),
		CountOffersOfKind(Legacy.CardRun.RouteMerchant, EGameXXKRouteMerchantOfferKind::Relic), 0);

	FGameXXKRuntimeState LegacyView = LegacyTwin;
	for (FGameXXKRouteMerchantOffer& Offer : LegacyView.CardRun.RouteMerchant.Offers)
	{
		Offer.Kind = EGameXXKRouteMerchantOfferKind::Relic;
	}
	FGameXXKRouteMerchantView NormalizedView;
	TestTrue(TEXT("mutable GetView normalizes legacy relic stock on first view"),
		FGameXXKRouteMerchantRules::GetView(LegacyView, NormalizedView, &Error));
	TestEqual(TEXT("normalized legacy view exposes four cards"), NormalizedView.CardOffers.Num(), 4);
	TestEqual(TEXT("normalized legacy view exposes no relics"), NormalizedView.RelicOffers.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteMerchantSaveContractTest,
	"GameXXK.MVP.RouteMerchant.Rules.SaveContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteMerchantSaveContractTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("serialized Card ordinal stays append-compatible"),
		static_cast<uint8>(EGameXXKRouteMerchantOfferKind::Card), static_cast<uint8>(1));
	const FProperty* OwnerProperty = FGameXXKRouteMerchantOffer::StaticStruct()->FindPropertyByName(TEXT("OwnerMemberId"));
	const FProperty* NextQualityProperty = FGameXXKRouteMerchantOffer::StaticStruct()->FindPropertyByName(TEXT("NextQuality"));
	TestTrue(TEXT("owner provenance is a SaveGame field"), OwnerProperty && OwnerProperty->HasAnyPropertyFlags(CPF_SaveGame));
	TestTrue(TEXT("next quality is a SaveGame field"), NextQualityProperty && NextQualityProperty->HasAnyPropertyFlags(CPF_SaveGame));
	TestTrue(TEXT("new typed failures append after legacy values"),
		static_cast<uint8>(EGameXXKRouteMerchantPurchaseFailure::StaleCardQuality)
			> static_cast<uint8>(EGameXXKRouteMerchantPurchaseFailure::ArithmeticOverflow));
	return true;
}

#endif
