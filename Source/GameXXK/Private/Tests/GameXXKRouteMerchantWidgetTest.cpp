#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/SafeZone.h"
#include "Engine/GameInstance.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKMVPRules.h"
#include "GameXXKRouteMerchantTypes.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKRouteMerchantWidget.h"

namespace GameXXKRouteMerchantWidgetTest
{
	FGameXXKRuntimeState MakeMerchantFixture()
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

	UGameXXKRouteMerchantWidget* MakeWidget(UGameXXKMVPSubsystem* Subsystem)
	{
		UGameXXKRouteMerchantWidget* Widget = NewObject<UGameXXKRouteMerchantWidget>();
		Widget->SetMVPSubsystem(Subsystem);
		Widget->Initialize();
		Widget->NativeConstruct();
		Widget->RefreshFromState();
		return Widget;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteMerchantWidgetStructureTest,
	"GameXXK.MVP.RouteMerchant.Widget.SafeAreaFourOffersAndDisabledStates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteMerchantWidgetStructureTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKRouteMerchantWidgetTest;
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(GameInstance);
	Subsystem->GetMutableRuntimeState() = MakeMerchantFixture();
	TestTrue(TEXT("merchant node opens and generates stable stock"), Subsystem->SelectRouteNodeById(10));

	FGameXXKRouteMerchantState& Merchant = Subsystem->GetMutableRuntimeState().CardRun.RouteMerchant;
	Merchant.Offers[0].bSold = true;
	Merchant.Offers[1].ContentId = NAME_None;
	Merchant.Offers[1].Quality = EGameXXKCardQuality::Invalid;
	Merchant.Offers[1].Price = 0;
	Merchant.Offers[1].bUnavailable = true;
	Merchant.Offers[1].bSold = false;
	Subsystem->GetMutableRuntimeState().CardRun.RouteTravelMoney = 0;

	UGameXXKRouteMerchantWidget* Widget = MakeWidget(Subsystem);
	TestNotNull(TEXT("dedicated merchant widget exists"), Widget);
	TestEqual(TEXT("merchant root is self-hit-test-invisible"), Widget->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
	TestEqual(TEXT("merchant design width targets 1920"), Widget->GetDesignResolutionForTest().X, 1920.0);
	TestEqual(TEXT("merchant design height targets 1080"), Widget->GetDesignResolutionForTest().Y, 1080.0);
	TestTrue(TEXT("left merchant column uses about twenty-three percent"), FMath::IsNearlyEqual(Widget->GetMerchantColumnFractionForTest(), 0.23f, 0.01f));
	TestTrue(TEXT("right offers column uses about seventy-seven percent"), FMath::IsNearlyEqual(Widget->GetOffersColumnFractionForTest(), 0.77f, 0.01f));
	UWidget* SafeAreaWidget = Widget->WidgetTree
		? Widget->WidgetTree->FindWidget(TEXT("RouteMerchantSafeArea"))
		: nullptr;
	TestTrue(TEXT("programmatic root contains a safe-area widget"), SafeAreaWidget && SafeAreaWidget->IsA<USafeZone>());

	UHorizontalBox* CardRow = Widget->WidgetTree
		? Cast<UHorizontalBox>(Widget->WidgetTree->FindWidget(TEXT("RouteMerchantCardRow")))
		: nullptr;
	UHorizontalBox* RelicRow = Widget->WidgetTree
		? Cast<UHorizontalBox>(Widget->WidgetTree->FindWidget(TEXT("RouteMerchantRelicRow")))
		: nullptr;
	TestNotNull(TEXT("upper card row exists"), CardRow);
	TestNotNull(TEXT("lower relic row exists"), RelicRow);
	TestEqual(TEXT("upper row contains zero card offers"), CardRow ? CardRow->GetChildrenCount() : 0, 0);
	TestEqual(TEXT("lower row contains four relic offers"), RelicRow ? RelicRow->GetChildrenCount() : 0, 4);
	TestTrue(TEXT("card offer frames are portrait-shaped"), Widget->GetCardFrameSizeForTest().Y > Widget->GetCardFrameSizeForTest().X);
	TestTrue(TEXT("relic offer frames are square"), Widget->GetRelicFrameSizeForTest().Equals(FVector2D(250.0f, 250.0f), 0.1f));
	TestTrue(TEXT("card row reuses the PSD057 frame"), Widget->GetCardFrameResourcePathForTest().Contains(TEXT("T_CardFrame_PSD057")));
	TestEqual(TEXT("all four offer bodies expose tooltips"), Widget->GetOfferTooltipCountForTest(), 4);
	TestTrue(TEXT("only buttons remain hit-testable in the merchant tree"), Widget->HasOnlyButtonHitTargetsForTest());
	TestFalse(TEXT("sold offer buy button is disabled"), Widget->IsOfferPurchaseEnabledForTest(Merchant.Offers[0].OfferId));
	TestFalse(TEXT("unavailable offer buy button is disabled"), Widget->IsOfferPurchaseEnabledForTest(Merchant.Offers[1].OfferId));
	TestFalse(TEXT("insufficient-money offer buy button is disabled"), Widget->IsOfferPurchaseEnabledForTest(Merchant.Offers[2].OfferId));
	TestFalse(TEXT("sold offer exposes a disabled reason"), Widget->GetOfferDisabledReasonForTest(Merchant.Offers[0].OfferId).IsEmpty());
	TestFalse(TEXT("unavailable offer exposes a disabled reason"), Widget->GetOfferDisabledReasonForTest(Merchant.Offers[1].OfferId).IsEmpty());
	TestFalse(TEXT("insufficient-money offer exposes a disabled reason"), Widget->GetOfferDisabledReasonForTest(Merchant.Offers[2].OfferId).IsEmpty());
	TestTrue(TEXT("bottom refresh button shows the first twenty-money price"), Widget->GetRefreshButtonTextForTest().ToString().Contains(TEXT("20")));
	TestFalse(TEXT("bottom leave button has visible text"), Widget->GetLeaveButtonTextForTest().IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteMerchantWidgetActionsTest,
	"GameXXK.MVP.RouteMerchant.Widget.PurchaseRefreshAndLeave",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteMerchantWidgetActionsTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKRouteMerchantWidgetTest;
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(GameInstance);
	Subsystem->GetMutableRuntimeState() = MakeMerchantFixture();
	TestTrue(TEXT("merchant node opens for action test"), Subsystem->SelectRouteNodeById(10));
	UGameXXKRouteMerchantWidget* Widget = MakeWidget(Subsystem);

	FGameXXKRouteMerchantView View;
	FString Error;
	TestTrue(TEXT("merchant view is available before purchase"), Subsystem->GetRouteMerchantView(View, &Error));
	const FGameXXKRouteMerchantOfferView* PurchasableOffer = View.RelicOffers.FindByPredicate([](const FGameXXKRouteMerchantOfferView& Offer)
	{
		return Offer.bPurchaseEnabled;
	});
	TestNotNull(TEXT("fixture has a purchasable relic"), PurchasableOffer);
	if (!PurchasableOffer)
	{
		return false;
	}

	const FName PurchasedOfferId = PurchasableOffer->SavedOffer.OfferId;
	const int32 BalanceBeforePurchase = Subsystem->GetRuntimeState().CardRun.RouteTravelMoney;
	TestTrue(TEXT("visible buy action commits through the subsystem"), Widget->PurchaseOffer(PurchasedOfferId));
	TestTrue(TEXT("widget retains the atomic purchase result"), Widget->GetLastPurchaseResultForTest().bPurchased);
	TestTrue(TEXT("purchase debits route money"), Subsystem->GetRuntimeState().CardRun.RouteTravelMoney < BalanceBeforePurchase);
	TestFalse(TEXT("purchased offer refreshes to disabled/sold"), Widget->IsOfferPurchaseEnabledForTest(PurchasedOfferId));

	TestTrue(TEXT("bottom refresh action commits through the subsystem"), Widget->RefreshStock());
	TestEqual(TEXT("first refresh advances stable stock count"), Subsystem->GetRuntimeState().CardRun.RouteMerchant.RefreshCount, 1);
	TestTrue(TEXT("next refresh label advances to thirty"), Widget->GetRefreshButtonTextForTest().ToString().Contains(TEXT("30")));
	TestTrue(TEXT("second refresh commits through the same bottom action"), Widget->RefreshStock());
	TestTrue(TEXT("third refresh label advances to forty"), Widget->GetRefreshButtonTextForTest().ToString().Contains(TEXT("40")));
	TestTrue(TEXT("third refresh commits through the same bottom action"), Widget->RefreshStock());
	TestTrue(TEXT("fourth refresh label advances to fifty"), Widget->GetRefreshButtonTextForTest().ToString().Contains(TEXT("50")));

	TestTrue(TEXT("leave action resolves the merchant node"), Widget->LeaveMerchant());
	TestEqual(TEXT("leave returns to the route map"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
	TestTrue(TEXT("leave marks the merchant node visited"), Subsystem->GetRuntimeState().VisitedRouteNodeIds.Contains(10));
	return true;
}

#endif
