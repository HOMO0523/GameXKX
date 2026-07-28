#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Engine/GameInstance.h"
#include "GameXXKMVPRules.h"
#include "GameXXKRunDeckRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKOneGameRouteMapWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteMapFixedSummaryWidgetTest,
	"GameXXK.MVP.RouteMap.FixedSummary.EconomyProgressAndCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteMapFixedSummaryWidgetTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(GameInstance);
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	State = UGameXXKMVPRules::CreateNewGame();
	State.Screen = EGameXXKScreen::DungeonMap;
	State.CurrentMapId = TEXT("HuangshanRoute");
	State.bDungeonActive = true;
	State.bHasGeneratedRouteMap = true;
	State.CurrentRouteNodeId = 10;
	State.RouteMapNodes = {
		FGameXXKRouteMapNode{10, 0, 0, EGameXXKNodeKind::Start, FVector2D(0.50f, 0.00f), TArray<int32>{11}},
		FGameXXKRouteMapNode{11, 1, 0, EGameXXKNodeKind::Battle, FVector2D(0.35f, 0.30f), TArray<int32>{12}},
		FGameXXKRouteMapNode{12, 2, 0, EGameXXKNodeKind::Merchant, FVector2D(0.65f, 0.60f), TArray<int32>{13}},
		FGameXXKRouteMapNode{13, 3, 0, EGameXXKNodeKind::Boss, FVector2D(0.50f, 0.95f), TArray<int32>{}}};
	State.RouteMapEdges = {
		FGameXXKRouteMapEdge{10, 11},
		FGameXXKRouteMapEdge{11, 12},
		FGameXXKRouteMapEdge{12, 13}};
	State.VisitedRouteNodeIds = {10, 11, 12};
	State.ReachableRouteNodeIds = {13};
	State.CardRun.RouteTravelMoney = 137;
	State.CardRun.RouteCardEntries.Reset();

	FGameXXKRouteCardEntry CapacityEntry;
	CapacityEntry.EntryId = TEXT("Route.Entry.Capacity");
	CapacityEntry.CardId = TEXT("Route.General.PoJiaTuCi");
	CapacityEntry.CurrentQuality = EGameXXKCardQuality::Common;
	CapacityEntry.SourceKind = EGameXXKRouteCardSourceKind::Merchant;
	CapacityEntry.OwnerUnitId = TEXT("Hero.XuXian");
	CapacityEntry.bTemporaryRouteCard = false;
	CapacityEntry.bConsumesRouteCapacity = true;
	CapacityEntry.AcquisitionOrdinal = 0;
	State.CardRun.RouteCardEntries.Add(CapacityEntry);

	FGameXXKRouteCardEntry TemporaryFreeEntry;
	TemporaryFreeEntry.EntryId = TEXT("Route.Entry.TemporaryFree");
	TemporaryFreeEntry.CardId = TEXT("Hero.QingFengYiShi");
	TemporaryFreeEntry.CurrentQuality = EGameXXKCardQuality::Common;
	TemporaryFreeEntry.SourceKind = EGameXXKRouteCardSourceKind::HeroBase;
	TemporaryFreeEntry.OwnerUnitId = TEXT("Hero.XuXian");
	TemporaryFreeEntry.bTemporaryRouteCard = true;
	TemporaryFreeEntry.bConsumesRouteCapacity = false;
	TemporaryFreeEntry.AcquisitionOrdinal = 1;
	State.CardRun.RouteCardEntries.Add(TemporaryFreeEntry);

	UGameXXKOneGameRouteMapWidget* Widget = NewObject<UGameXXKOneGameRouteMapWidget>();
	Widget->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("route-map summary fixture initializes"), Widget->Initialize());
	Widget->NativeConstruct();
	Widget->SetRouteMapViewportGeometry(FVector2D::ZeroVector, FVector2D(1280.0f, 720.0f));
	Widget->RefreshFromState();

	const FGameXXKRouteMapSummaryView Summary = Widget->GetRouteSummaryViewForTest();
	TestTrue(TEXT("route summary validates capacity through the run-deck rules"), Summary.bCapacityValid);
	TestEqual(TEXT("route summary shows route-only money"), Summary.RouteTravelMoney, 137);
	TestEqual(TEXT("route progress excludes Start from completed nodes"), Summary.CompletedNodeCount, 2);
	TestEqual(TEXT("route progress excludes Start from total nodes"), Summary.TotalNodeCount, 3);
	TestEqual(TEXT("capacity follows bConsumesRouteCapacity rather than bTemporaryRouteCard"), Summary.CapacityUsed, 1);
	TestEqual(TEXT("capacity shows the authoritative twelve-slot limit"), Summary.CapacityLimit, FGameXXKRunDeckRules::MaxRouteCardCapacity);

	UWidget* RootWidget = Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("GameXXKOneGameRouteMapRoot")) : nullptr;
	UWidget* ScrollWidget = Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("GameXXKOneGameRouteMapScroll")) : nullptr;
	UWidget* FixedSummaryWidget = Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("GameXXKRouteMapFixedSummary")) : nullptr;
	TestNotNull(TEXT("fixed route summary exists"), FixedSummaryWidget);
	TestTrue(TEXT("route-map root is an overlay"), RootWidget && RootWidget->IsA<UOverlay>());
	TestTrue(TEXT("route map remains inside its scroll box"), ScrollWidget && ScrollWidget->IsA<UScrollBox>());
	TestNotNull(TEXT("fixed summary is a direct root-overlay child outside the scroll box"),
		FixedSummaryWidget ? Cast<UOverlaySlot>(FixedSummaryWidget->Slot) : nullptr);
	TestEqual(TEXT("fixed summary never blocks route-node clicks"),
		FixedSummaryWidget ? FixedSummaryWidget->GetVisibility() : ESlateVisibility::Visible,
		ESlateVisibility::SelfHitTestInvisible);
	TestTrue(TEXT("existing route-node buttons remain bound"), Widget->IsRouteNodeButtonBoundForTest(0));

	State.CardRun.RouteTravelMoney = 91;
	Widget->RefreshFromState();
	TestTrue(TEXT("summary refreshes from state without Tick"), Widget->GetRouteMoneySummaryTextForTest().ToString().Contains(TEXT("91")));
	return true;
}

#endif
