#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/SafeZone.h"
#include "Engine/GameInstance.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKMVPRules.h"
#include "GameXXKRouteMerchantTypes.h"
#include "GameXXKRunDeckRules.h"
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

	void FillRouteCapacity(FGameXXKRuntimeState& State)
	{
		State.CardRun.RouteCardEntries.Reset();
		for (int32 Index = 0; Index < FGameXXKRunDeckRules::MaxRouteCardCapacity; ++Index)
		{
			FGameXXKRouteCardEntry Entry;
			Entry.EntryId = FName(*FString::Printf(TEXT("Widget.Capacity.Entry.%02d"), Index));
			Entry.CardId = FName(*FString::Printf(TEXT("Widget.Capacity.Card.%02d"), Index));
			Entry.CurrentQuality = EGameXXKCardQuality::Common;
			Entry.SourceKind = EGameXXKRouteCardSourceKind::RouteReward;
			Entry.OwnerUnitId = TEXT("Player");
			Entry.bTemporaryRouteCard = true;
			Entry.bConsumesRouteCapacity = true;
			Entry.AcquisitionOrdinal = Index;
			State.CardRun.RouteCardEntries.Add(Entry);
		}
		State.CardRun.NextRouteCardEntryOrdinal = FGameXXKRunDeckRules::MaxRouteCardCapacity;
		State.CardRun.RouteProgress.ActualRouteCardAcquisitionCount = 0;
	}

	const FGameXXKRouteMerchantOffer* FindOffer(
		const FGameXXKRuntimeState& State,
		const FName OfferId)
	{
		return State.CardRun.RouteMerchant.Offers.FindByPredicate([OfferId](const FGameXXKRouteMerchantOffer& Offer)
		{
			return Offer.OfferId == OfferId;
		});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteMerchantWidgetStructureTest,
	"GameXXK.MVP.RouteMerchant.Widget.SafeAreaSixOffersAndDisabledStates",
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
	TestEqual(TEXT("upper row contains three card offers"), CardRow ? CardRow->GetChildrenCount() : 0, 3);
	TestEqual(TEXT("lower row contains three relic offers"), RelicRow ? RelicRow->GetChildrenCount() : 0, 3);
	TestTrue(TEXT("card offer frames are portrait-shaped"), Widget->GetCardFrameSizeForTest().Y > Widget->GetCardFrameSizeForTest().X);
	TestTrue(TEXT("relic offer frames are square"), Widget->GetRelicFrameSizeForTest().Equals(FVector2D(250.0f, 250.0f), 0.1f));
	TestTrue(TEXT("card row reuses the PSD057 frame"), Widget->GetCardFrameResourcePathForTest().Contains(TEXT("T_CardFrame_PSD057")));
	TestEqual(TEXT("all six offer bodies expose tooltips"), Widget->GetOfferTooltipCountForTest(), 6);
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
	"GameXXK.MVP.RouteMerchant.Widget.PurchaseRefreshCancelAndLeave",
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
	const FGameXXKRouteMerchantOfferView* PurchasableOffer = View.CardOffers.FindByPredicate([](const FGameXXKRouteMerchantOfferView& Offer)
	{
		return Offer.bPurchaseEnabled;
	});
	TestNotNull(TEXT("fixture has a purchasable card"), PurchasableOffer);
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

	FGameXXKRouteMerchantOffer* PendingOffer = Subsystem->GetMutableRuntimeState().CardRun.RouteMerchant.Offers.FindByPredicate([](const FGameXXKRouteMerchantOffer& Offer)
	{
		return Offer.Kind == EGameXXKRouteMerchantOfferKind::Card && !Offer.bUnavailable && !Offer.bSold;
	});
	TestNotNull(TEXT("refreshed stock has a card for pending-cancel fixture"), PendingOffer);
	if (PendingOffer)
	{
		FGameXXKPendingRouteMerchantPurchase& Pending = Subsystem->GetMutableRuntimeState().CardRun.RouteMerchant.PendingPurchase;
		Pending.bActive = true;
		Pending.OfferId = PendingOffer->OfferId;
		Pending.CardId = PendingOffer->ContentId;
		Pending.Price = PendingOffer->Price;
		Widget->RefreshFromState();
		TestTrue(TEXT("pending replacement can be cancelled from the widget"), Widget->CancelPendingPurchase());
		TestFalse(TEXT("cancel clears saved pending purchase"), Subsystem->GetRuntimeState().CardRun.RouteMerchant.PendingPurchase.bActive);
		TestTrue(TEXT("cancel without a pending purchase remains idempotent"), Widget->CancelPendingPurchase());
		TestTrue(TEXT("idempotent cancel leaves no action error"), Widget->GetLastActionErrorForTest().IsEmpty());
		TestFalse(TEXT("purchase without an offer reports an action error"), Widget->PurchaseOffer(NAME_None));
		TestFalse(TEXT("action error remains nonempty"), Widget->GetLastActionErrorForTest().IsEmpty());
		TestEqual(
			TEXT("LastActionError is rendered in the merchant HUD"),
			Widget->GetDisplayedLastActionErrorForTest().ToString(),
			Widget->GetLastActionErrorForTest());
		UWidget* ErrorTextWidget = Widget->WidgetTree
			? Widget->WidgetTree->FindWidget(TEXT("RouteMerchantLastActionError"))
			: nullptr;
		TestNotNull(TEXT("LastActionError has a concrete text widget"), ErrorTextWidget);
		TestEqual(
			TEXT("nonempty LastActionError is visibly rendered"),
			ErrorTextWidget ? ErrorTextWidget->GetVisibility() : ESlateVisibility::Collapsed,
			ESlateVisibility::HitTestInvisible);
	}

	TestTrue(TEXT("leave action resolves the merchant node"), Widget->LeaveMerchant());
	TestEqual(TEXT("leave returns to the route map"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
	TestTrue(TEXT("leave marks the merchant node visited"), Subsystem->GetRuntimeState().VisitedRouteNodeIds.Contains(10));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteMerchantWidgetReplacementSelectionTest,
	"GameXXK.MVP.RouteMerchant.Widget.FullCapacityReplacementSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteMerchantWidgetReplacementSelectionTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKRouteMerchantWidgetTest;
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(GameInstance);
	Subsystem->GetMutableRuntimeState() = MakeMerchantFixture();
	TestTrue(TEXT("merchant node opens for replacement test"), Subsystem->SelectRouteNodeById(10));
	FillRouteCapacity(Subsystem->GetMutableRuntimeState());

	FGameXXKRouteMerchantView View;
	FString Error;
	TestTrue(TEXT("full-capacity merchant view is available"), Subsystem->GetRouteMerchantView(View, &Error));
	const FGameXXKRouteMerchantOfferView* CardOffer = View.CardOffers.FindByPredicate([](const FGameXXKRouteMerchantOfferView& Offer)
	{
		return Offer.bPurchaseEnabled;
	});
	TestNotNull(TEXT("fixture has a purchasable card offer"), CardOffer);
	if (!CardOffer)
	{
		return false;
	}

	const FName OfferId = CardOffer->SavedOffer.OfferId;
	const int32 Price = CardOffer->SavedOffer.Price;
	const int32 BalanceBefore = Subsystem->GetRuntimeState().CardRun.RouteTravelMoney;
	const int32 GoldBefore = Subsystem->GetRuntimeState().PlayerGold;
	const TArray<FGameXXKRouteCardEntry> DeckBefore = Subsystem->GetRuntimeState().CardRun.RouteCardEntries;
	UGameXXKRouteMerchantWidget* Widget = MakeWidget(Subsystem);

	TestTrue(TEXT("buying a card at capacity opens replacement selection"), Widget->PurchaseOffer(OfferId));
	const TArray<FName> EligibleEntryIds = Widget->GetEligibleReplacementEntryIdsForTest();
	TestTrue(TEXT("replacement selection panel is visible"), Widget->IsReplacementSelectionVisibleForTest());
	TestEqual(
		TEXT("all twelve eligible stable EntryIds are rendered"),
		EligibleEntryIds.Num(),
		FGameXXKRunDeckRules::MaxRouteCardCapacity);
	for (const FGameXXKRouteCardEntry& Entry : DeckBefore)
	{
		TestTrue(TEXT("each full-deck EntryId has a replacement action"), EligibleEntryIds.Contains(Entry.EntryId));
	}
	TestEqual(TEXT("opening replacement selection does not debit"), Subsystem->GetRuntimeState().CardRun.RouteTravelMoney, BalanceBefore);
	const FGameXXKRouteMerchantOffer* PendingOffer = FindOffer(Subsystem->GetRuntimeState(), OfferId);
	TestTrue(TEXT("opening replacement selection does not sell the offer"), PendingOffer && !PendingOffer->bSold);

	UGameXXKRouteMerchantWidget* RecreatedWidget = MakeWidget(Subsystem);
	TestTrue(TEXT("a recreated widget recovers the pending replacement panel"), RecreatedWidget->IsReplacementSelectionVisibleForTest());
	TestEqual(
		TEXT("a recreated widget recovers all twelve stable EntryIds through preview"),
		RecreatedWidget->GetEligibleReplacementEntryIdsForTest().Num(),
		FGameXXKRunDeckRules::MaxRouteCardCapacity);
	const TArray<FName> RecreatedEntryIds = RecreatedWidget->GetEligibleReplacementEntryIdsForTest();
	const int32 ReplacementChoiceIndex = 4;
	const FName ReplacementEntryId = RecreatedEntryIds.IsValidIndex(ReplacementChoiceIndex)
		? RecreatedEntryIds[ReplacementChoiceIndex]
		: NAME_None;
	TestFalse(TEXT("replacement test has a stable candidate"), ReplacementEntryId.IsNone());
	UButton* ReplacementChoiceButton = RecreatedWidget->WidgetTree
		? Cast<UButton>(RecreatedWidget->WidgetTree->FindWidget(
			*FString::Printf(TEXT("RouteMerchantReplacementChoice%d"), ReplacementChoiceIndex)))
		: nullptr;
	TestNotNull(TEXT("replacement choice is backed by a real UButton"), ReplacementChoiceButton);
	if (!ReplacementChoiceButton)
	{
		return false;
	}
	ReplacementChoiceButton->OnClicked.Broadcast();
	TestTrue(TEXT("clicking the rendered EntryId commits through the existing purchase facade"), RecreatedWidget->GetLastPurchaseResultForTest().bPurchased);
	TestEqual(TEXT("replacement commits one exact debit"), Subsystem->GetRuntimeState().CardRun.RouteTravelMoney, BalanceBefore - Price);
	TestEqual(TEXT("replacement never touches permanent gold"), Subsystem->GetRuntimeState().PlayerGold, GoldBefore);
	TestFalse(TEXT("replacement clears pending purchase"), Subsystem->GetRuntimeState().CardRun.RouteMerchant.PendingPurchase.bActive);
	TestFalse(TEXT("replacement panel closes after commit"), RecreatedWidget->IsReplacementSelectionVisibleForTest());
	const FGameXXKRouteMerchantOffer* PurchasedOffer = FindOffer(Subsystem->GetRuntimeState(), OfferId);
	TestTrue(TEXT("replacement atomically sells the selected offer"), PurchasedOffer && PurchasedOffer->bSold);
	TestFalse(TEXT("selected stable EntryId is removed"), Subsystem->GetRuntimeState().CardRun.RouteCardEntries.ContainsByPredicate([ReplacementEntryId](const FGameXXKRouteCardEntry& Entry)
	{
		return Entry.EntryId == ReplacementEntryId;
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteMerchantWidgetReplacementCancelTest,
	"GameXXK.MVP.RouteMerchant.Widget.FullCapacityReplacementCancel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteMerchantWidgetReplacementCancelTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKRouteMerchantWidgetTest;
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(GameInstance);
	Subsystem->GetMutableRuntimeState() = MakeMerchantFixture();
	TestTrue(TEXT("merchant node opens for cancel test"), Subsystem->SelectRouteNodeById(10));
	FillRouteCapacity(Subsystem->GetMutableRuntimeState());

	FGameXXKRouteMerchantView View;
	FString Error;
	TestTrue(TEXT("cancel fixture merchant view is available"), Subsystem->GetRouteMerchantView(View, &Error));
	const FGameXXKRouteMerchantOfferView* CardOffer = View.CardOffers.FindByPredicate([](const FGameXXKRouteMerchantOfferView& Offer)
	{
		return Offer.bPurchaseEnabled;
	});
	TestNotNull(TEXT("cancel fixture has a purchasable card offer"), CardOffer);
	if (!CardOffer)
	{
		return false;
	}

	const FName OfferId = CardOffer->SavedOffer.OfferId;
	const int32 BalanceBefore = Subsystem->GetRuntimeState().CardRun.RouteTravelMoney;
	const TArray<FGameXXKRouteCardEntry> DeckBefore = Subsystem->GetRuntimeState().CardRun.RouteCardEntries;
	UGameXXKRouteMerchantWidget* Widget = MakeWidget(Subsystem);
	TestTrue(TEXT("cancel fixture opens replacement selection"), Widget->PurchaseOffer(OfferId));
	TestTrue(TEXT("cancel fixture exposes twelve choices"), Widget->GetEligibleReplacementEntryIdsForTest().Num() == FGameXXKRunDeckRules::MaxRouteCardCapacity);
	UButton* CancelButton = Widget->WidgetTree
		? Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("RouteMerchantCancelButton")))
		: nullptr;
	TestNotNull(TEXT("pending replacement exposes a concrete cancel button"), CancelButton);
	TestEqual(
		TEXT("pending replacement makes cancel visible"),
		CancelButton ? CancelButton->GetVisibility() : ESlateVisibility::Collapsed,
		ESlateVisibility::Visible);
	TestTrue(TEXT("pending replacement enables cancel"), CancelButton && CancelButton->GetIsEnabled());
	if (!CancelButton)
	{
		return false;
	}
	CancelButton->OnClicked.Broadcast();
	TestFalse(TEXT("visible cancel action clears the pending replacement"), Subsystem->GetRuntimeState().CardRun.RouteMerchant.PendingPurchase.bActive);
	TestFalse(TEXT("cancel closes replacement selection"), Widget->IsReplacementSelectionVisibleForTest());
	TestEqual(TEXT("cancel does not debit route money"), Subsystem->GetRuntimeState().CardRun.RouteTravelMoney, BalanceBefore);
	TestEqual(TEXT("cancel preserves deck size"), Subsystem->GetRuntimeState().CardRun.RouteCardEntries.Num(), DeckBefore.Num());
	for (int32 Index = 0; Index < DeckBefore.Num(); ++Index)
	{
		TestTrue(
			TEXT("cancel preserves each stable deck entry"),
			FGameXXKRouteCardEntry::StaticStruct()->CompareScriptStruct(
				&Subsystem->GetRuntimeState().CardRun.RouteCardEntries[Index],
				&DeckBefore[Index],
				PPF_None));
	}
	const FGameXXKRouteMerchantOffer* CancelledOffer = FindOffer(Subsystem->GetRuntimeState(), OfferId);
	TestTrue(TEXT("cancel leaves the offer unsold"), CancelledOffer && !CancelledOffer->bSold);
	return true;
}

#endif
