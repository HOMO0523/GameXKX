#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/GameInstance.h"
#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKMVPRules.h"
#include "GameXXKRouteMerchantRules.h"
#include "GameXXKRouteMerchantTypes.h"
#include "MVP/GameXXKMVPSubsystem.h"

namespace GameXXKRouteMerchantFacadeTest
{
	bool RuntimeStatesMatch(const FGameXXKRuntimeState& Left, const FGameXXKRuntimeState& Right)
	{
		return FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(&Left, &Right, PPF_None);
	}

	FGameXXKRuntimeState MakeRouteMapMerchantFixture()
	{
		FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
		State.Screen = EGameXXKScreen::DungeonMap;
		State.CurrentMapId = TEXT("HuangshanRoute");
		State.bDungeonActive = true;
		State.bHasGeneratedRouteMap = true;
		State.RouteSeed = 0x6137;
		State.CurrentRouteNodeId = 9;
		State.PendingRouteNodeId = INDEX_NONE;
		State.RouteMapNodes = {
			FGameXXKRouteMapNode{9, 1, 0, EGameXXKNodeKind::Start, FVector2D(0.25f, 0.5f), TArray<int32>{10}},
			FGameXXKRouteMapNode{10, 2, 1, EGameXXKNodeKind::Merchant, FVector2D(0.55f, 0.5f), TArray<int32>{}}};
		State.RouteMapEdges = {FGameXXKRouteMapEdge{9, 10}};
		State.VisitedRouteNodeIds = {9};
		State.ReachableRouteNodeIds = {10};
		State.CardRun.RouteProgress.SchemaVersion = 1;
		State.CardRun.RouteProgress.RootSeed = State.RouteSeed;
		State.CardRun.RouteProgress.ChapterSeeds = {State.RouteSeed};
		State.CardRun.RouteProgress.CurrentChapter = 1;
		State.CardRun.RouteProgress.RouteCombatLevel = 1;
		State.CardRun.bLoadoutLockedForRoute = true;
		State.CardRun.bRouteEconomyInitialized = true;
		State.CardRun.RouteTravelMoney = 500;
		State.PlayerGold = 777;

		for (const FGameXXKCardDefinition& Definition : FGameXXKCardCatalog::GetAllCardDefinitions())
		{
			if (Definition.Owner == EGameXXKCardOwner::Hero)
			{
				State.CardRun.HeroUnlockedCardIds.Add(Definition.Id);
				if (State.CardRun.HeroUnlockedCardIds.Num() == 8)
				{
					break;
				}
			}
		}
		State.CardRun.HeroSelectedCardIds = State.CardRun.HeroUnlockedCardIds;
		return State;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteMerchantFacadeEntryAndTransactionTest,
	"GameXXK.Route.Merchant.Facade.EntryViewRefreshAndPurchase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteMerchantFacadeEntryAndTransactionTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKRouteMerchantFacadeTest;
	FGameXXKRuntimeState State = MakeRouteMapMerchantFixture();
	const int32 PermanentGoldBefore = State.PlayerGold;
	TestTrue(TEXT("selecting a reachable merchant enters atomically"), UGameXXKMVPRules::SelectRouteNodeById(State, 10));
	TestEqual(TEXT("merchant selection opens the dedicated screen"), State.Screen, EGameXXKScreen::RouteMerchant);
	TestEqual(TEXT("merchant selection records the pending node"), State.PendingRouteNodeId, 10);
	TestEqual(TEXT("merchant selection materializes all eight stable slots"),
		State.CardRun.RouteMerchant.Offers.Num(),
		FGameXXKRouteMerchantRules::TotalSlotCount);

	FGameXXKRouteMerchantView View;
	FString Error;
	TestTrue(TEXT("rules facade exposes the persisted merchant view"), UGameXXKMVPRules::GetRouteMerchantView(State, View, &Error));
	TestEqual(TEXT("view exposes four card slots"), View.CardOffers.Num(), 4);
	TestEqual(TEXT("view exposes four relic slots"), View.RelicOffers.Num(), 4);
	TestEqual(TEXT("view exposes ordinary gold"), View.PlayerGold, PermanentGoldBefore);
	TestEqual(TEXT("first refresh costs twenty"), View.RefreshCost, 20);

	const int32 RouteMoneyBeforeRefresh = State.CardRun.RouteTravelMoney;
	TestTrue(TEXT("refresh facade succeeds"), UGameXXKMVPRules::RefreshRouteMerchant(State, &Error));
	TestEqual(TEXT("refresh preserves route money"), State.CardRun.RouteTravelMoney, RouteMoneyBeforeRefresh);
	TestEqual(TEXT("refresh increments the stable count"), State.CardRun.RouteMerchant.RefreshCount, 1);
	TestEqual(TEXT("refresh debits ordinary gold"), State.PlayerGold, PermanentGoldBefore - 20);

	TestTrue(TEXT("refreshed view remains valid"), UGameXXKMVPRules::GetRouteMerchantView(State, View, &Error));
	const FGameXXKRouteMerchantOfferView* PurchasableCard = View.CardOffers.FindByPredicate([](const FGameXXKRouteMerchantOfferView& Offer)
	{
		return Offer.bPurchaseEnabled;
	});
	TestNotNull(TEXT("at least one carried card is purchasable"), PurchasableCard);
	if (!PurchasableCard)
	{
		return false;
	}

	FGameXXKRouteMerchantPurchasePreview Preview;
	TestTrue(
		TEXT("purchase preview facade succeeds"),
		UGameXXKMVPRules::PreviewRouteMerchantPurchase(State, PurchasableCard->SavedOffer.OfferId, NAME_None, Preview, &Error));
	TestTrue(TEXT("a carried card can be upgraded directly"), Preview.bCanPurchase);
	TestFalse(TEXT("a carried-card upgrade never requires replacement"), Preview.bRequiresReplacement);
	const int32 RouteMoneyBeforePurchase = State.CardRun.RouteTravelMoney;
	const int32 GoldBeforePurchase = State.PlayerGold;
	FGameXXKRouteMerchantPurchaseResult Result;
	TestTrue(
		TEXT("purchase facade commits"),
		UGameXXKMVPRules::PurchaseRouteMerchant(State, PurchasableCard->SavedOffer.OfferId, NAME_None, Result));
	TestTrue(TEXT("result reports committed purchase"), Result.bPurchased);
	TestEqual(TEXT("purchase preserves route money"), State.CardRun.RouteTravelMoney, RouteMoneyBeforePurchase);
	TestEqual(TEXT("purchase debits exactly its ordinary-gold price"), State.PlayerGold, GoldBeforePurchase - Result.Price);
	TestEqual(TEXT("purchase changes authoritative configured quality"),
		FGameXXKCardBattleAdapter::GetConfiguredCardQuality(State.CardRun, Result.CardId), Result.FinalQuality);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteMerchantFacadeAtomicFailureAndLeaveTest,
	"GameXXK.Route.Merchant.Facade.AtomicFailureAndLeave",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteMerchantFacadeAtomicFailureAndLeaveTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKRouteMerchantFacadeTest;
	FGameXXKRuntimeState Invalid = MakeRouteMapMerchantFixture();
	Invalid.CardRun.RouteTravelMoney = -1;
	const FGameXXKRuntimeState InvalidBefore = Invalid;
	TestFalse(TEXT("invalid economy rejects merchant entry"), UGameXXKMVPRules::SelectRouteNodeById(Invalid, 10));
	TestTrue(TEXT("failed merchant entry preserves the complete runtime"), RuntimeStatesMatch(Invalid, InvalidBefore));

	FGameXXKRuntimeState State = MakeRouteMapMerchantFixture();
	TestTrue(TEXT("valid merchant opens"), UGameXXKMVPRules::SelectRouteNodeById(State, 10));
	const int32 MoneyBeforeLeave = State.CardRun.RouteTravelMoney;
	TestTrue(TEXT("leaving settles the merchant node"), UGameXXKMVPRules::ResolveMerchantRouteNode(State));
	TestEqual(TEXT("leave never charges an offer"), State.CardRun.RouteTravelMoney, MoneyBeforeLeave);
	TestEqual(TEXT("leave returns to route map"), State.Screen, EGameXXKScreen::DungeonMap);
	TestTrue(TEXT("leave marks merchant node visited"), State.VisitedRouteNodeIds.Contains(10));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteMerchantSubsystemFacadeTest,
	"GameXXK.Route.Merchant.Facade.Subsystem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteMerchantSubsystemFacadeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKRouteMerchantFacadeTest;
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestNotNull(TEXT("subsystem fixture exists"), Subsystem);
	if (!Subsystem)
	{
		return false;
	}
	Subsystem->GetMutableRuntimeState() = MakeRouteMapMerchantFixture();
	TestTrue(TEXT("subsystem selects merchant"), Subsystem->SelectRouteNodeById(10));
	FGameXXKRouteMerchantView View;
	FString Error;
	TestTrue(TEXT("subsystem exposes merchant view"), Subsystem->GetRouteMerchantView(View, &Error));
	TestEqual(TEXT("subsystem view has four card offers"), View.CardOffers.Num(), 4);
	TestEqual(TEXT("subsystem view has four relic offers"), View.RelicOffers.Num(), 4);
	TestTrue(TEXT("subsystem refresh wrapper succeeds"), Subsystem->RefreshRouteMerchant(&Error));
	TestEqual(TEXT("subsystem refresh is visible on next view"), Subsystem->GetRuntimeState().CardRun.RouteMerchant.RefreshCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteMerchantLegacyFirstViewNormalizationTest,
	"GameXXK.Route.Merchant.Facade.LegacyFirstViewNormalization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteMerchantLegacyFirstViewNormalizationTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKRouteMerchantFacadeTest;
	FGameXXKRuntimeState Legacy = MakeRouteMapMerchantFixture();
	TestTrue(TEXT("legacy first-view fixture enters merchant"), UGameXXKMVPRules::SelectRouteNodeById(Legacy, 10));
	Legacy.CardRun.RouteMerchant.Offers.SetNum(4);
	for (FGameXXKRouteMerchantOffer& Offer : Legacy.CardRun.RouteMerchant.Offers)
	{
		Offer.Kind = EGameXXKRouteMerchantOfferKind::Relic;
		Offer.OwnerMemberId = NAME_None;
		Offer.NextQuality = EGameXXKCardQuality::Invalid;
	}
	Legacy.CardRun.RouteMerchant.PendingPurchase.bActive = true;
	Legacy.CardRun.RouteMerchant.PendingPurchase.OfferId = Legacy.CardRun.RouteMerchant.Offers[0].OfferId;
	Legacy.CardRun.RouteMerchant.PendingPurchase.CardId = Legacy.CardRun.RouteMerchant.Offers[0].ContentId;
	Legacy.CardRun.RouteMerchant.PendingPurchase.Price = Legacy.CardRun.RouteMerchant.Offers[0].Price;
	FGameXXKRuntimeState DirectLeaveLegacy = Legacy;

	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	Subsystem->GetMutableRuntimeState() = Legacy;
	FGameXXKRouteMerchantView View;
	FString Error;
	TestTrue(TEXT("first subsystem view normalizes legacy stock without an explicit Ensure call"),
		Subsystem->GetRouteMerchantView(View, &Error));
	TestTrue(TEXT("first subsystem view reports no normalization error"), Error.IsEmpty());
	TestEqual(TEXT("normalized runtime has exactly eight offers"),
		Subsystem->GetRuntimeState().CardRun.RouteMerchant.Offers.Num(),
		FGameXXKRouteMerchantRules::TotalSlotCount);
	int32 NormalizedCardCount = 0;
	for (const FGameXXKRouteMerchantOffer& Offer : Subsystem->GetRuntimeState().CardRun.RouteMerchant.Offers)
	{
		NormalizedCardCount += Offer.Kind == EGameXXKRouteMerchantOfferKind::Card ? 1 : 0;
	}
	TestEqual(TEXT("normalized runtime has exactly four card offers"),
		NormalizedCardCount, 4);
	TestEqual(TEXT("normalized first view has four card offers"), View.CardOffers.Num(), 4);
	TestEqual(TEXT("normalized first view has four relic offers"), View.RelicOffers.Num(), 4);
	TestFalse(TEXT("first facade view discards the legacy active pending flag"),
		Subsystem->GetRuntimeState().CardRun.RouteMerchant.PendingPurchase.bActive);
	TestTrue(TEXT("normalized facade stock can refresh"), Subsystem->RefreshRouteMerchant(&Error));
	TestEqual(TEXT("normalized facade refresh advances once"),
		Subsystem->GetRuntimeState().CardRun.RouteMerchant.RefreshCount, 1);
	TestTrue(TEXT("normalized facade stock can leave"), Subsystem->ResolveMerchantRouteNode());
	TestEqual(TEXT("normalized facade leave returns to route map"),
		Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
	TestTrue(TEXT("facade can directly leave a legacy pending snapshot without first reading a view"),
		UGameXXKMVPRules::ResolveMerchantRouteNode(DirectLeaveLegacy));
	TestEqual(TEXT("direct legacy facade leave returns to route map"),
		DirectLeaveLegacy.Screen, EGameXXKScreen::DungeonMap);
	return true;
}

#endif
