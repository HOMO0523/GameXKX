#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/GameInstance.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKMVPRules.h"
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
	TestEqual(TEXT("merchant selection materializes all four stable slots"), State.CardRun.RouteMerchant.Offers.Num(), 4);

	FGameXXKRouteMerchantView View;
	FString Error;
	TestTrue(TEXT("rules facade exposes the persisted merchant view"), UGameXXKMVPRules::GetRouteMerchantView(State, View, &Error));
	TestEqual(TEXT("view exposes zero card slots"), View.CardOffers.Num(), 0);
	TestEqual(TEXT("view exposes four relic slots"), View.RelicOffers.Num(), 4);
	TestEqual(TEXT("view exposes route-only money"), View.RouteTravelMoney, 500);
	TestEqual(TEXT("first refresh costs twenty"), View.RefreshCost, 20);

	const int32 RouteMoneyBeforeRefresh = State.CardRun.RouteTravelMoney;
	TestTrue(TEXT("refresh facade succeeds"), UGameXXKMVPRules::RefreshRouteMerchant(State, &Error));
	TestEqual(TEXT("refresh debits only the current refresh price"), State.CardRun.RouteTravelMoney, RouteMoneyBeforeRefresh - 20);
	TestEqual(TEXT("refresh increments the stable count"), State.CardRun.RouteMerchant.RefreshCount, 1);
	TestEqual(TEXT("refresh never touches permanent gold"), State.PlayerGold, PermanentGoldBefore);

	TestTrue(TEXT("refreshed view remains valid"), UGameXXKMVPRules::GetRouteMerchantView(State, View, &Error));
	const FGameXXKRouteMerchantOfferView* PurchasableRelic = View.RelicOffers.FindByPredicate([](const FGameXXKRouteMerchantOfferView& Offer)
	{
		return Offer.bPurchaseEnabled;
	});
	TestNotNull(TEXT("at least one relic is purchasable"), PurchasableRelic);
	if (!PurchasableRelic)
	{
		return false;
	}

	FGameXXKRouteMerchantPurchasePreview Preview;
	TestTrue(
		TEXT("purchase preview facade succeeds"),
		UGameXXKMVPRules::PreviewRouteMerchantPurchase(State, PurchasableRelic->SavedOffer.OfferId, NAME_None, Preview, &Error));
	TestTrue(TEXT("a relic can be bought directly"), Preview.bCanPurchase);
	TestFalse(TEXT("a relic purchase never requires replacement"), Preview.bRequiresReplacement);
	const int32 RouteMoneyBeforePurchase = State.CardRun.RouteTravelMoney;
	FGameXXKRouteMerchantPurchaseResult Result;
	TestTrue(
		TEXT("purchase facade commits"),
		UGameXXKMVPRules::PurchaseRouteMerchant(State, PurchasableRelic->SavedOffer.OfferId, NAME_None, Result));
	TestTrue(TEXT("result reports committed purchase"), Result.bPurchased);
	TestEqual(TEXT("purchase debits exactly its price"), State.CardRun.RouteTravelMoney, RouteMoneyBeforePurchase - Result.Price);
	TestEqual(TEXT("purchase still never touches permanent gold"), State.PlayerGold, PermanentGoldBefore);
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
	TestEqual(TEXT("subsystem view has zero card offers"), View.CardOffers.Num(), 0);
	TestEqual(TEXT("subsystem view has four relic offers"), View.RelicOffers.Num(), 4);
	TestTrue(TEXT("subsystem refresh wrapper succeeds"), Subsystem->RefreshRouteMerchant(&Error));
	TestEqual(TEXT("subsystem refresh is visible on next view"), Subsystem->GetRuntimeState().CardRun.RouteMerchant.RefreshCount, 1);
	return true;
}

#endif
