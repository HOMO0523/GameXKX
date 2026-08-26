#include "GameXXKMVPRules.h"
#include "GameXXKCardBattleAdapter.h"
#include "GameXXKRouteEconomyRules.h"
#include "MVP/GameXXKLevelFlow.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Misc/AutomationTest.h"
#include "UI/GameXXKMVPHUD.h"
#include "UI/GameXXKOneGameRouteMapWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKOneGameRouteMapAdapterTest,
	"GameXXK.MVP.RouteMap.OneGameAdapter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKOneGameRouteMapAdapterTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestNotNull(TEXT("subsystem exists"), Subsystem);

	TestTrue(TEXT("new game starts"), Subsystem->StartGame());
	TestTrue(TEXT("Qingshan can be selected"), Subsystem->SelectWorldRegion(UGameXXKMVPRules::RegionQingshan()));
	TestTrue(TEXT("quest can be accepted"), Subsystem->AcceptQuest());
	TestTrue(TEXT("accepted quest enters route map"), Subsystem->OpenDungeonFromTownExit());
	TestEqual(TEXT("route map screen is active"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
	TestTrue(TEXT("route map is generated when entering dungeon"), Subsystem->GetRuntimeState().bHasGeneratedRouteMap);
	TestTrue(TEXT("route map has at least ten nodes"), Subsystem->GetRuntimeState().RouteMapNodes.Num() >= 10);
	TestTrue(TEXT("route map has branching edges"), Subsystem->GetRuntimeState().RouteMapEdges.Num() > Subsystem->GetRuntimeState().RouteMapNodes.Num());
	TestEqual(TEXT("route map starts with one reachable node"), Subsystem->GetRuntimeState().ReachableRouteNodeIds.Num(), 1);

	UGameXXKOneGameRouteMapWidget* RouteWidget = NewObject<UGameXXKOneGameRouteMapWidget>();
	RouteWidget->SetMVPSubsystem(Subsystem);
	RouteWidget->RefreshFromState();

	AGameXXKMVPHUD* HUD = NewObject<AGameXXKMVPHUD>();
	HUD->SetMVPSubsystemForTest(Subsystem);
	UGameXXKOneGameRouteMapWidget* HUDRouteWidget = HUD->CreateRouteMapWidgetForTest();
	TestNotNull(TEXT("HUD creates route map adapter widget"), HUDRouteWidget);
	TestTrue(TEXT("HUD retains route map adapter widget"), HUD->HasRouteMapWidget());
	TestEqual(TEXT("HUD route widget receives same MVP subsystem"), HUDRouteWidget ? HUDRouteWidget->GetMVPSubsystem() : nullptr, Subsystem);
	TestTrue(TEXT("HUD route widget keeps 1Game class reference"), HUDRouteWidget && HUDRouteWidget->IsOneGameRouteWidgetClassConfigured());
	TestFalse(TEXT("HUD suppresses legacy canvas route map when UMG adapter exists"), HUD->ShouldDrawLegacyRouteMapForTest());

	TestTrue(TEXT("adapter keeps a 1Game route widget class reference"), RouteWidget->IsOneGameRouteWidgetClassConfigured());
	TestTrue(TEXT("adapter points at 1Game UI_地图选择 asset"), RouteWidget->GetOneGameRouteWidgetClassPath().Contains(TEXT("/Game/1Game/UI/")));
	TestTrue(TEXT("adapter configures 1Game node and line visual classes"), RouteWidget->AreOneGameVisualClassesConfigured());
	TestTrue(TEXT("adapter points at 1Game route node widget"), RouteWidget->GetOneGameNodeWidgetClassPath().Contains(TEXT("UI_地图选择-关卡")));
	TestTrue(TEXT("adapter points at 1Game route line widget"), RouteWidget->GetOneGameLineWidgetClassPath().Contains(TEXT("UI_地图选择-关卡-线")));
	TestTrue(TEXT("adapter points at 1Game boss widget"), RouteWidget->GetOneGameBossWidgetClassPath().Contains(TEXT("UI_地图选择-Boss")));
	TestFalse(TEXT("adapter avoids loading 1Game blueprint visuals by default"), RouteWidget->ShouldUseOneGameBlueprintVisualWidgets());
	TestTrue(TEXT("adapter configures safe 1Game texture visuals"), RouteWidget->AreOneGameTextureVisualsConfigured());

	UGameInstance* RefreshGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* RefreshSubsystem = NewObject<UGameXXKMVPSubsystem>(RefreshGameInstance);
	AGameXXKMVPHUD* RefreshHUD = NewObject<AGameXXKMVPHUD>();
	RefreshHUD->SetMVPSubsystemForTest(RefreshSubsystem);
	UGameXXKOneGameRouteMapWidget* RefreshRouteWidget = RefreshHUD->CreateRouteMapWidgetForTest();
	TestNotNull(TEXT("refresh test route widget exists"), RefreshRouteWidget);
	TestTrue(TEXT("refresh test route widget initializes"), RefreshRouteWidget && RefreshRouteWidget->Initialize());
	if (RefreshRouteWidget)
	{
		RefreshRouteWidget->NativeConstruct();
		RefreshRouteWidget->RefreshFromState();
	}
	TestEqual(TEXT("route widget starts collapsed on main menu"), RefreshRouteWidget ? RefreshRouteWidget->GetVisibility() : ESlateVisibility::Visible, ESlateVisibility::Collapsed);
	TestTrue(TEXT("refresh test starts new game"), RefreshSubsystem->StartGame());
	TestTrue(TEXT("refresh test enters town"), RefreshSubsystem->SelectWorldRegion(UGameXXKMVPRules::RegionQingshan()));
	TestTrue(TEXT("refresh test accepts quest"), RefreshSubsystem->AcceptQuest());
	TestTrue(TEXT("refresh test enters dungeon"), RefreshSubsystem->OpenDungeonFromTownExit());
	TestEqual(TEXT("route widget remains stale before auxiliary refresh"), RefreshRouteWidget ? RefreshRouteWidget->GetVisibility() : ESlateVisibility::Visible, ESlateVisibility::Collapsed);
	RefreshHUD->RefreshAuxiliaryWidgetsForTest();
	TestEqual(TEXT("auxiliary refresh makes route widget visible"), RefreshRouteWidget ? RefreshRouteWidget->GetVisibility() : ESlateVisibility::Collapsed, ESlateVisibility::Visible);
	TestEqual(TEXT("late route widget refresh creates generated node visuals"), RefreshRouteWidget ? RefreshRouteWidget->GetCreatedNodeVisualWidgetCount() : 0, RefreshSubsystem->GetRuntimeState().RouteMapNodes.Num());
	TestEqual(TEXT("late route widget refresh creates generated line visuals"), RefreshRouteWidget ? RefreshRouteWidget->GetCreatedLineVisualWidgetCount() : 0, RefreshSubsystem->GetRuntimeState().RouteMapEdges.Num());
	RefreshSubsystem->GetMutableRuntimeState().Screen = EGameXXKScreen::RouteEvent;
	RefreshHUD->RefreshAuxiliaryWidgetsForTest();
	TestEqual(TEXT("pending question or chest keeps the route map visible beneath its HUD modal"), RefreshRouteWidget ? RefreshRouteWidget->GetVisibility() : ESlateVisibility::Collapsed, ESlateVisibility::Visible);
	TestEqual(TEXT("pending question or chest preserves route node visuals beneath its HUD modal"), RefreshRouteWidget ? RefreshRouteWidget->GetCreatedNodeVisualWidgetCount() : 0, RefreshSubsystem->GetRuntimeState().RouteMapNodes.Num());

	TestTrue(TEXT("adapter initializes widget tree"), RouteWidget->Initialize());
	RouteWidget->NativeConstruct();
	RouteWidget->SetRouteMapViewportGeometry(FVector2D::ZeroVector, FVector2D(1280.0f, 720.0f));
	RouteWidget->RefreshFromState();
	TestTrue(TEXT("adapter creates a route background visual"), RouteWidget->HasRouteBackgroundVisualForTest());
	TestTrue(TEXT("adapter uses the 1Game map background texture"), RouteWidget->GetRouteBackgroundTexturePathForTest().Contains(TEXT("图层_1")));
	const FVector2D RouteContentSize = RouteWidget->GetRouteContentSizeForTest();
	UWidget* RouteContentSizeWidget = nullptr;
	UWidget* RouteBackgroundWidget = nullptr;
	if (RouteWidget->WidgetTree)
	{
		RouteWidget->WidgetTree->ForEachWidget([&RouteContentSizeWidget, &RouteBackgroundWidget](UWidget* Widget)
		{
			if (!Widget)
			{
				return;
			}
			if (Widget->GetFName() == TEXT("GameXXKOneGameRouteMapContentSize"))
			{
				RouteContentSizeWidget = Widget;
			}
			else if (Widget->GetFName() == TEXT("GameXXKOneGameRouteMapBackground"))
			{
				RouteBackgroundWidget = Widget;
			}
		});
	}
	TestNotNull(TEXT("route content size widget is present in the UMG tree"), RouteContentSizeWidget);
	const UScrollBoxSlot* RouteContentScrollSlot = RouteContentSizeWidget ? Cast<UScrollBoxSlot>(RouteContentSizeWidget->Slot) : nullptr;
	TestNotNull(TEXT("route content size widget is owned by the route scroll box"), RouteContentScrollSlot);
	if (RouteContentScrollSlot)
	{
		TestEqual(TEXT("route content scroll slot is centered horizontally in the canvas viewport"), RouteContentScrollSlot->GetHorizontalAlignment(), HAlign_Center);
	}
	TestNotNull(TEXT("route background widget is present in the UMG tree"), RouteBackgroundWidget);
	const UCanvasPanelSlot* RouteBackgroundCanvasSlot = RouteBackgroundWidget ? Cast<UCanvasPanelSlot>(RouteBackgroundWidget->Slot) : nullptr;
	TestNotNull(TEXT("route background shares the scroll canvas with route nodes"), RouteBackgroundCanvasSlot);
	if (RouteBackgroundCanvasSlot)
	{
		TestEqual(TEXT("route background canvas width matches route content width"), RouteBackgroundCanvasSlot->GetSize().X, RouteContentSize.X);
		TestEqual(TEXT("route background canvas height matches route content height"), RouteBackgroundCanvasSlot->GetSize().Y, RouteContentSize.Y);
	}
	TestTrue(TEXT("route content fills the full route viewport width"), RouteContentSize.X >= 1280.0f);
	TestTrue(TEXT("route content keeps a tall scrollable route above the screen"), RouteContentSize.Y > 720.0f);
	TestTrue(TEXT("route map shows the approved paper/ink scroll bar for the player-facing view"), RouteWidget->IsRouteScrollBarVisibleForTest());
	TestTrue(TEXT("route map provides a blank-canvas drag surface"), RouteWidget->HasRouteDragSurfaceForTest());
	TestEqual(TEXT("route map initially scrolls to the bottom of the route"), RouteWidget->GetLastAppliedScrollOffsetForTest(), RouteWidget->GetMaxScrollOffsetForTest());
	const float BottomScrollOffset = RouteWidget->GetLastAppliedScrollOffsetForTest();
	TestTrue(TEXT("route map drag can move upward from the bottom"), RouteWidget->ApplyRouteMapDragDeltaForTest(160.0f));
	TestTrue(TEXT("route map drag decreases the scroll offset when content is dragged downward"), RouteWidget->GetLastAppliedScrollOffsetForTest() < BottomScrollOffset);
	TestTrue(TEXT("route map drag clamps at the bottom"), RouteWidget->ApplyRouteMapDragDeltaForTest(-100000.0f));
	TestEqual(TEXT("route map drag bottom clamp matches max scroll"), RouteWidget->GetLastAppliedScrollOffsetForTest(), RouteWidget->GetMaxScrollOffsetForTest());
	TestEqual(TEXT("adapter creates one visual per generated route node"), RouteWidget->GetCreatedNodeVisualWidgetCount(), Subsystem->GetRuntimeState().RouteMapNodes.Num());
	TestEqual(TEXT("adapter creates one visual per generated route edge"), RouteWidget->GetCreatedLineVisualWidgetCount(), Subsystem->GetRuntimeState().RouteMapEdges.Num());
	for (int32 ButtonIndex = 0; ButtonIndex < Subsystem->GetRuntimeState().RouteMapNodes.Num(); ++ButtonIndex)
	{
		TestTrue(
			FString::Printf(TEXT("generated route node button %d is bound for real UMG clicks"), ButtonIndex),
			RouteWidget->IsRouteNodeButtonBoundForTest(ButtonIndex));
	}
	TestEqual(TEXT("fallback route node visual uses a border shell"), RouteWidget->GetCreatedNodeVisualWidgetClassName(0), FString(TEXT("Border")));
	UWidget* FirstRouteNodeWidget = nullptr;
	UWidget* FirstRouteNodeButtonWidget = nullptr;
	UWidget* RouteDragSurfaceWidget = nullptr;
	if (RouteWidget->WidgetTree)
	{
		RouteWidget->WidgetTree->ForEachWidget([&FirstRouteNodeWidget, &FirstRouteNodeButtonWidget, &RouteDragSurfaceWidget](UWidget* Widget)
		{
			if (!Widget)
			{
				return;
			}
			if (Widget->GetFName() == TEXT("RouteNodeFallbackVisual0"))
			{
				FirstRouteNodeWidget = Widget;
			}
			else if (Widget->GetFName() == TEXT("RouteNodeButton0"))
			{
				FirstRouteNodeButtonWidget = Widget;
			}
			else if (Widget->GetFName() == TEXT("GameXXKOneGameRouteMapDragSurface"))
			{
				RouteDragSurfaceWidget = Widget;
			}
		});
	}
	const UCanvasPanelSlot* RouteDragSurfaceSlot = RouteDragSurfaceWidget ? Cast<UCanvasPanelSlot>(RouteDragSurfaceWidget->Slot) : nullptr;
	const UBorder* FirstRouteNodeBorder = Cast<UBorder>(FirstRouteNodeWidget);
	TestNotNull(TEXT("fallback route node shell is a border widget"), FirstRouteNodeBorder);
	if (FirstRouteNodeBorder)
	{
		TestEqual(TEXT("fallback route node border does not draw a square backing"), FirstRouteNodeBorder->GetBrushColor().A, 0.0f);
	}
	const UButton* FirstRouteNodeButton = Cast<UButton>(FirstRouteNodeButtonWidget);
	TestNotNull(TEXT("route node hit button exists"), FirstRouteNodeButton);
	if (FirstRouteNodeButton)
	{
		TestEqual(TEXT("route node hit button remains hit-testable"), FirstRouteNodeButton->GetRenderOpacity(), 1.0f);
	}
	const UCanvasPanelSlot* FirstRouteNodeButtonSlot = FirstRouteNodeButtonWidget ? Cast<UCanvasPanelSlot>(FirstRouteNodeButtonWidget->Slot) : nullptr;
	TestNotNull(TEXT("route drag surface has a canvas slot"), RouteDragSurfaceSlot);
	TestNotNull(TEXT("route node button has a canvas slot"), FirstRouteNodeButtonSlot);
	if (RouteDragSurfaceSlot && FirstRouteNodeButtonSlot)
	{
		TestTrue(TEXT("route node hit button is layered above the drag surface"), FirstRouteNodeButtonSlot->GetZOrder() > RouteDragSurfaceSlot->GetZOrder());
	}
	TestEqual(TEXT("fallback route line visual uses a footprint image"), RouteWidget->GetCreatedLineVisualWidgetClassName(0), FString(TEXT("Image")));
	UWidget* FirstRouteLineWidget = nullptr;
	if (RouteWidget->WidgetTree)
	{
		RouteWidget->WidgetTree->ForEachWidget([&FirstRouteLineWidget](UWidget* Widget)
		{
			if (Widget && Widget->GetFName() == TEXT("RouteLineFallbackVisual0"))
			{
				FirstRouteLineWidget = Widget;
			}
		});
	}
	const UImage* FirstRouteLineImage = Cast<UImage>(FirstRouteLineWidget);
	TestNotNull(TEXT("fallback route line visual is an image widget"), FirstRouteLineImage);
	const UObject* FirstRouteLineBrushResource = FirstRouteLineImage ? FirstRouteLineImage->GetBrush().GetResourceObject() : nullptr;
	TestNotNull(TEXT("fallback route line image has a brush resource"), FirstRouteLineBrushResource);
	if (FirstRouteLineBrushResource)
	{
		TestTrue(TEXT("fallback route line uses the 1Game footprint texture"), FirstRouteLineBrushResource->GetPathName().Contains(TEXT("脚印")));
	}
	const UCanvasPanelSlot* FirstRouteLineCanvasSlot = FirstRouteLineWidget ? Cast<UCanvasPanelSlot>(FirstRouteLineWidget->Slot) : nullptr;
	TestNotNull(TEXT("fallback route line has a canvas slot"), FirstRouteLineCanvasSlot);
	if (FirstRouteLineCanvasSlot)
	{
		TestTrue(TEXT("fallback footprint route line is thick enough to read"), FirstRouteLineCanvasSlot->GetSize().Y >= 24.0f);
	}
	const UTexture2D* FirstRouteLineTexture = Cast<UTexture2D>(FirstRouteLineBrushResource);
	TestNotNull(TEXT("fallback route line brush resource is a texture"), FirstRouteLineTexture);
	if (FirstRouteLineImage && FirstRouteLineCanvasSlot && FirstRouteLineTexture)
	{
		const FBox2f RouteLineUVRegion = FirstRouteLineImage->GetBrush().GetUVRegion();
		const int32 FirstRouteLineTextureWidth = FirstRouteLineTexture->GetSizeX() > 0
			? FirstRouteLineTexture->GetSizeX()
			: FirstRouteLineTexture->GetImportedSize().X;
		const float ExpectedUVMaxX = FMath::Clamp(
			FirstRouteLineCanvasSlot->GetSize().X / static_cast<float>(FirstRouteLineTextureWidth),
			0.0f,
			1.0f);
		TestTrue(TEXT("fallback footprint route line UV width follows displayed line length"), FMath::IsNearlyEqual(RouteLineUVRegion.Max.X, ExpectedUVMaxX, 0.01f));
		TestEqual(TEXT("fallback footprint route line uses the full source height"), RouteLineUVRegion.Max.Y, 1.0f);
	}
	UGameInstance* ShortLineGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* ShortLineSubsystem = NewObject<UGameXXKMVPSubsystem>(ShortLineGameInstance);
	FGameXXKRuntimeState& ShortLineState = ShortLineSubsystem->GetMutableRuntimeState();
	ShortLineState = UGameXXKMVPRules::CreateNewGame();
	ShortLineState.Screen = EGameXXKScreen::DungeonMap;
	ShortLineState.CurrentMapId = TEXT("HuangshanRoute");
	ShortLineState.bDungeonActive = true;
	ShortLineState.bHasGeneratedRouteMap = true;
	ShortLineState.CurrentRouteNodeId = 100;
	ShortLineState.RouteMapNodes.Reset();
	ShortLineState.RouteMapEdges.Reset();
	ShortLineState.VisitedRouteNodeIds.Reset();
	ShortLineState.ReachableRouteNodeIds.Reset();
	ShortLineState.RouteMapNodes.Emplace(100, 0, 0, EGameXXKNodeKind::Start, FVector2D(0.50f, 0.00f), TArray<int32>{101});
	ShortLineState.RouteMapNodes.Emplace(101, 1, 0, EGameXXKNodeKind::Battle, FVector2D(0.50f, 0.10f), TArray<int32>{});
	ShortLineState.RouteMapEdges.Emplace(100, 101);
	ShortLineState.ReachableRouteNodeIds.Add(100);

	UGameXXKOneGameRouteMapWidget* ShortLineRouteWidget = NewObject<UGameXXKOneGameRouteMapWidget>();
	ShortLineRouteWidget->SetMVPSubsystem(ShortLineSubsystem);
	TestTrue(TEXT("short-line route widget initializes"), ShortLineRouteWidget->Initialize());
	ShortLineRouteWidget->NativeConstruct();
	ShortLineRouteWidget->SetRouteMapViewportGeometry(FVector2D::ZeroVector, FVector2D(1280.0f, 720.0f));
	ShortLineRouteWidget->RefreshFromState();

	UWidget* ShortRouteLineWidget = nullptr;
	if (ShortLineRouteWidget->WidgetTree)
	{
		ShortLineRouteWidget->WidgetTree->ForEachWidget([&ShortRouteLineWidget](UWidget* Widget)
		{
			if (Widget && Widget->GetFName() == TEXT("RouteLineFallbackVisual0"))
			{
				ShortRouteLineWidget = Widget;
			}
		});
	}
	const UImage* ShortRouteLineImage = Cast<UImage>(ShortRouteLineWidget);
	const UCanvasPanelSlot* ShortRouteLineCanvasSlot = ShortRouteLineWidget ? Cast<UCanvasPanelSlot>(ShortRouteLineWidget->Slot) : nullptr;
	const UTexture2D* ShortRouteLineTexture = ShortRouteLineImage
		? Cast<UTexture2D>(ShortRouteLineImage->GetBrush().GetResourceObject())
		: nullptr;
	TestNotNull(TEXT("short route line visual is an image widget"), ShortRouteLineImage);
	TestNotNull(TEXT("short route line has a canvas slot"), ShortRouteLineCanvasSlot);
	TestNotNull(TEXT("short route line uses a texture brush"), ShortRouteLineTexture);
	if (ShortRouteLineImage && ShortRouteLineCanvasSlot && ShortRouteLineTexture)
	{
		const int32 ShortRouteLineTextureWidth = ShortRouteLineTexture->GetSizeX() > 0
			? ShortRouteLineTexture->GetSizeX()
			: ShortRouteLineTexture->GetImportedSize().X;
		const float TextureWidth = static_cast<float>(ShortRouteLineTextureWidth);
		const FBox2f ShortRouteLineUVRegion = ShortRouteLineImage->GetBrush().GetUVRegion();
		const float ExpectedUVMaxX = FMath::Clamp(ShortRouteLineCanvasSlot->GetSize().X / TextureWidth, 0.0f, 1.0f);
		TestTrue(TEXT("short route line is shorter than the footprint source texture"), ShortRouteLineCanvasSlot->GetSize().X < TextureWidth);
		TestTrue(TEXT("fallback short footprint route line crops the source texture instead of compressing all 1500px"), ShortRouteLineUVRegion.Max.X < 1.0f);
		TestTrue(TEXT("fallback short footprint route line UV width follows displayed line length"), FMath::IsNearlyEqual(ShortRouteLineUVRegion.Max.X, ExpectedUVMaxX, 0.01f));
		TestEqual(TEXT("fallback short footprint route line uses the full source height"), ShortRouteLineUVRegion.Max.Y, 1.0f);
	}
	TestFalse(TEXT("fallback current node label is bound"), RouteWidget->GetCreatedNodeVisualLabel(0).IsEmpty());
	TestFalse(TEXT("fallback next node label is bound"), RouteWidget->GetCreatedNodeVisualLabel(1).IsEmpty());
	TestTrue(TEXT("fallback start node uses 1Game camp texture"), RouteWidget->GetCreatedNodeVisualIconPath(0).Contains(TEXT("篝火")));
	TestTrue(TEXT("fallback future battle node uses 1Game disabled monster texture"), RouteWidget->GetCreatedNodeVisualIconPath(1).Contains(TEXT("小怪灰色")));

	const TArray<FGameXXKOneGameRouteNode> InitialNodes = RouteWidget->BuildAdapterNodes();
	TestEqual(TEXT("adapter exposes generated route nodes"), InitialNodes.Num(), Subsystem->GetRuntimeState().RouteMapNodes.Num());
	TestEqual(TEXT("first route node is Start"), InitialNodes[0].NodeKind, EGameXXKNodeKind::Start);
	TestEqual(TEXT("second route node is Battle"), InitialNodes[1].NodeKind, EGameXXKNodeKind::Battle);
	TestTrue(TEXT("current route node is enabled"), InitialNodes[0].bEnabled);
	TestFalse(TEXT("future route node is disabled"), InitialNodes[1].bEnabled);
	TestEqual(TEXT("battle maps to 1Game small enemy room"), InitialNodes[1].RoomType, EGameXXKOneGameRouteRoomType::SmallEnemy);

	TestFalse(TEXT("adapter rejects future node click"), RouteWidget->ExecuteRouteNode(1));
	TestTrue(TEXT("adapter executes current start node"), RouteWidget->ExecuteRouteNode(0));
	TestTrue(TEXT("start node is recorded as visited"), Subsystem->GetRuntimeState().VisitedRouteNodeIds.Contains(0));
	TestTrue(TEXT("start node unlocks connected next route nodes"), Subsystem->GetRuntimeState().ReachableRouteNodeIds.Contains(1));
	TestTrue(TEXT("start node unlocks a branch route node"), Subsystem->GetRuntimeState().ReachableRouteNodeIds.Num() > 1);
	TestEqual(TEXT("start node keeps route map visible"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);

	RouteWidget->RefreshFromState();
	const TArray<FGameXXKOneGameRouteNode> AfterStartNodes = RouteWidget->BuildAdapterNodes();
	TestFalse(TEXT("visited start node is disabled after advance"), AfterStartNodes[0].bEnabled);
	TestTrue(TEXT("battle node becomes enabled after start"), AfterStartNodes[1].bEnabled);
	TestTrue(TEXT("enabled battle node swaps to 1Game monster texture"), RouteWidget->GetCreatedNodeVisualIconPath(1).EndsWith(TEXT("/小怪.小怪")));
	TestTrue(TEXT("adapter executes battle node"), RouteWidget->ExecuteRouteNode(1));
	TestEqual(TEXT("battle node opens battle screen"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Battle);
	TestEqual(
		TEXT("battle node stays on GameXXK-owned route map"),
		GameXXKLevelFlow::MapForRuntimeState(Subsystem->GetRuntimeState()),
		FName(TEXT("/Game/GameXXK/Maps/L_RouteMap")));
	for (FGameXXKCardCombatUnit& Unit : Subsystem->GetMutableRuntimeState().CardRun.ActiveBattle.Units)
	{
		if (Unit.Side == EGameXXKCardTargetSide::Enemy)
		{
			Unit.HP = 0;
			Unit.bLiving = false;
		}
	}
	Subsystem->GetMutableRuntimeState().CardRun.ActiveBattle.Phase = EGameXXKCardBattlePhase::Victory;
	TestTrue(TEXT("battle victory opens the saved tiered three-choice route reward"), Subsystem->ResolveBattleVictory(false));
	TestEqual(TEXT("battle victory keeps the battle screen active until the reward is resolved"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Battle);
	TestEqual(TEXT("battle victory exposes exactly three saved route reward options"), Subsystem->GetRuntimeState().CardRun.PendingReward.Options.Num(), 3);
	FString RewardError;
	TestTrue(FString::Printf(TEXT("skipping the saved route reward unlocks route progression: %s"), *RewardError),
		FGameXXKCardBattleAdapter::SkipPendingRouteReward(Subsystem->GetMutableRuntimeState(), &RewardError));
	TestTrue(TEXT("resolved reward completes the route battle victory"), Subsystem->ResolveBattleVictory(false));
	TestEqual(TEXT("battle victory returns to route-map screen"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
	TestEqual(
		TEXT("battle victory targets GameXXK-owned route map"),
		GameXXKLevelFlow::MapForRuntimeState(Subsystem->GetRuntimeState()),
		FName(TEXT("/Game/GameXXK/Maps/L_RouteMap")));

	UGameInstance* SparseGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* SparseSubsystem = NewObject<UGameXXKMVPSubsystem>(SparseGameInstance);
	FGameXXKRuntimeState& SparseState = SparseSubsystem->GetMutableRuntimeState();
	SparseState = UGameXXKMVPRules::CreateNewGame();
	SparseState.Screen = EGameXXKScreen::DungeonMap;
	SparseState.CurrentMapId = TEXT("HuangshanRoute");
	SparseState.bDungeonActive = true;
	SparseState.bHasGeneratedRouteMap = true;
	SparseState.CurrentRouteNodeId = 10;
	SparseState.PendingRouteNodeId = INDEX_NONE;
	SparseState.RouteMapNodes.Reset();
	SparseState.RouteMapEdges.Reset();
	SparseState.VisitedRouteNodeIds.Reset();
	SparseState.ReachableRouteNodeIds.Reset();
	SparseState.RouteMapNodes.Emplace(10, 0, 0, EGameXXKNodeKind::Start, FVector2D(0.00f, 0.00f), TArray<int32>{20});
	SparseState.RouteMapNodes.Emplace(20, 1, 0, EGameXXKNodeKind::Battle, FVector2D(1.00f, 1.00f), TArray<int32>{});
	SparseState.RouteMapEdges.Emplace(10, 20);
	SparseState.ReachableRouteNodeIds.Add(10);
	SparseState.CardRun.RouteProgress.CurrentChapter = 1;
	TestTrue(TEXT("sparse route fixture initializes its route economy"), FGameXXKRouteEconomyRules::InitializeRoute(SparseState.CardRun));

	UGameXXKOneGameRouteMapWidget* SparseRouteWidget = NewObject<UGameXXKOneGameRouteMapWidget>();
	SparseRouteWidget->SetMVPSubsystem(SparseSubsystem);
	TestTrue(TEXT("sparse route widget initializes"), SparseRouteWidget->Initialize());
	SparseRouteWidget->NativeConstruct();
	SparseRouteWidget->SetRouteMapViewportGeometry(FVector2D::ZeroVector, FVector2D(1280.0f, 720.0f));
	SparseRouteWidget->RefreshFromState();

	const auto SparseVisualStates = SparseRouteWidget->GetRouteNodeVisualStatesForTest();
	const FVector2D SparseRouteContentSize = SparseRouteWidget->GetRouteContentSizeForTest();
	TestTrue(TEXT("sparse route content still has scrollable height"), SparseRouteContentSize.Y >= 520.0f);
	if (SparseVisualStates.Num() > 0)
	{
		TestTrue(TEXT("enabled sparse node has viewport-adjusted hit position"), SparseVisualStates[0].ViewportHitBoxPosition.Y <= SparseVisualStates[0].HitBoxPosition.Y + 0.1f);
	}
	if (SparseVisualStates.Num() >= 2)
	{
		const float MinRouteCenterX = FMath::Min(SparseVisualStates[0].CanvasPosition.X, SparseVisualStates[1].CanvasPosition.X);
		const float MaxRouteCenterX = FMath::Max(SparseVisualStates[0].CanvasPosition.X, SparseVisualStates[1].CanvasPosition.X);
		const float RouteCenterX = (MinRouteCenterX + MaxRouteCenterX) * 0.5f;
		TestTrue(TEXT("extreme route nodes stay inside the centered parchment lane"), MinRouteCenterX >= SparseRouteContentSize.X * 0.20f && MaxRouteCenterX <= SparseRouteContentSize.X * 0.80f);
		TestTrue(TEXT("centered parchment lane stays centered in the route canvas"), FMath::IsNearlyEqual(RouteCenterX, SparseRouteContentSize.X * 0.5f, 1.0f));
	}
	TestEqual(TEXT("sparse adapter exposes one visual state per route node"), SparseVisualStates.Num(), 2);
	TestEqual(TEXT("sparse first visual keeps real node id"), SparseVisualStates[0].NodeId, 10);
	TestEqual(TEXT("sparse first visual exposes concrete route command"), SparseVisualStates[0].CommandName, FName(TEXT("RouteNode10")));
	TestTrue(TEXT("sparse first visual exposes enabled hit area"), SparseVisualStates[0].bEnabled && SparseVisualStates[0].HitBoxSize.X > 0.0f && SparseVisualStates[0].HitBoxSize.Y > 0.0f);
	TestEqual(TEXT("sparse second visual keeps real node id"), SparseVisualStates[1].NodeId, 20);
	TestEqual(TEXT("sparse second visual exposes concrete route command"), SparseVisualStates[1].CommandName, FName(TEXT("RouteNode20")));
	TestFalse(TEXT("sparse future node starts disabled"), SparseVisualStates[1].bEnabled);
	TestFalse(TEXT("sparse node id is not accidentally treated as array index"), SparseRouteWidget->ExecuteRouteNode(10));
	TestTrue(TEXT("sparse adapter executes by real node id"), SparseRouteWidget->ExecuteRouteNodeById(10));
	TestTrue(TEXT("sparse start node is visited by id"), SparseSubsystem->GetRuntimeState().VisitedRouteNodeIds.Contains(10));
	TestTrue(TEXT("sparse battle node unlocks by id"), SparseSubsystem->GetRuntimeState().ReachableRouteNodeIds.Contains(20));
	SparseRouteWidget->RefreshFromState();
	TestTrue(TEXT("sparse adapter executes unlocked battle by real node id"), SparseRouteWidget->ExecuteRouteNodeById(20));
	TestEqual(TEXT("sparse battle node opens battle screen"), SparseSubsystem->GetRuntimeState().Screen, EGameXXKScreen::Battle);

	return true;
}

namespace
{
	bool RectanglesOverlap(const FBox2D& First, const FBox2D& Second)
	{
		return First.bIsValid
			&& Second.bIsValid
			&& First.Min.X < Second.Max.X
			&& First.Max.X > Second.Min.X
			&& First.Min.Y < Second.Max.Y
			&& First.Max.Y > Second.Min.Y;
	}

	bool BuildRouteAbandonWidgetFixture(
		UGameXXKMVPSubsystem*& OutSubsystem,
		UGameXXKOneGameRouteMapWidget*& OutWidget)
	{
		OutSubsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
		if (!OutSubsystem
			|| !OutSubsystem->StartGame()
			|| !OutSubsystem->SelectWorldRegion(UGameXXKMVPRules::RegionQingshan())
			|| !OutSubsystem->AcceptQuest()
			|| !OutSubsystem->OpenDungeonFromTownExit())
		{
			return false;
		}
		OutWidget = NewObject<UGameXXKOneGameRouteMapWidget>();
		OutWidget->SetMVPSubsystem(OutSubsystem);
		if (!OutWidget->Initialize())
		{
			return false;
		}
		OutWidget->NativeConstruct();
		OutWidget->SetRouteMapViewportGeometry(FVector2D::ZeroVector, FVector2D(1920.0f, 1080.0f));
		OutWidget->RefreshFromState();
		return true;
	}

	bool RouteRuntimeStatesEqual(const FGameXXKRuntimeState& Left, const FGameXXKRuntimeState& Right)
	{
		return FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(&Left, &Right, PPF_None);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteMapAbandonFixedGeometryTest,
	"GameXXK.MVP.RouteMap.AbandonConfirmation.FixedTopRightGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteMapAbandonFixedGeometryTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = nullptr;
	UGameXXKOneGameRouteMapWidget* Widget = nullptr;
	if (!TestTrue(TEXT("fixed-close fixture builds"), BuildRouteAbandonWidgetFixture(Subsystem, Widget)))
	{
		return false;
	}
	UOverlay* RootOverlay = Widget->GetRouteRootOverlayForTest();
	USizeBox* CloseContainer = Widget->GetRouteCloseChallengeContainerForTest();
	UButton* CloseButton = Widget->GetRouteCloseChallengeButtonForTest();
	TestNotNull(TEXT("route map exposes its fixed RootOverlay"), RootOverlay);
	TestNotNull(TEXT("route map owns a fixed Close Challenge container"), CloseContainer);
	TestNotNull(TEXT("route map owns a Close Challenge button"), CloseButton);
	const UObject* CloseNormalResource = CloseButton ? CloseButton->GetStyle().Normal.GetResourceObject() : nullptr;
	TestEqual(
		TEXT("route map close uses the exact approved CloseInk texture"),
		CloseNormalResource ? CloseNormalResource->GetPathName() : FString(),
		FString(TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_CloseInk.T_MasterV2_CloseInk")));
	TestNull(TEXT("route map close is an image-only button with no legacy text content"),
		CloseButton ? CloseButton->GetContent() : nullptr);
	TestNull(TEXT("route map widget tree no longer owns the legacy Close Challenge label"),
		Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("RouteCloseChallengeLabel")) : nullptr);
	TestTrue(TEXT("route map close hover tint remains visibly distinct"),
		CloseButton
		&& CloseButton->GetStyle().Hovered.TintColor.GetSpecifiedColor()
			!= CloseButton->GetStyle().Normal.TintColor.GetSpecifiedColor());
	TestTrue(TEXT("route map close pressed tint remains visibly distinct"),
		CloseButton
		&& CloseButton->GetStyle().Pressed.TintColor.GetSpecifiedColor()
			!= CloseButton->GetStyle().Normal.TintColor.GetSpecifiedColor());
	TestTrue(TEXT("Close Challenge container is a direct RootOverlay child"), CloseContainer && CloseContainer->GetParent() == RootOverlay);
	TestNotNull(TEXT("Close Challenge uses an Overlay slot rather than the scroll canvas"), CloseContainer ? Cast<UOverlaySlot>(CloseContainer->Slot) : nullptr);
	TestTrue(TEXT("Close Challenge is layered after the route scroll box"),
		RootOverlay && CloseContainer && Widget->GetRouteScrollBoxForTest()
		&& RootOverlay->GetChildIndex(CloseContainer) > RootOverlay->GetChildIndex(Widget->GetRouteScrollBoxForTest()));

	for (const FVector2D ViewportSize : {
		FVector2D(1280.0f, 720.0f),
		FVector2D(1672.0f, 941.0f),
		FVector2D(1920.0f, 1080.0f)})
	{
		const FBox2D Rect = Widget->ResolveRouteCloseChallengeRectForTest(ViewportSize);
		TestTrue(TEXT("fixed Close Challenge rectangle is valid"), Rect.bIsValid);
		TestTrue(TEXT("fixed Close Challenge stays inside viewport"),
			Rect.Min.X >= 0.0 && Rect.Min.Y >= 0.0 && Rect.Max.X <= ViewportSize.X && Rect.Max.Y <= ViewportSize.Y);
		const double Scale = FMath::Min(ViewportSize.X / 1920.0, ViewportSize.Y / 1080.0);
		const FBox2D SummaryRect(
			FVector2D(28.0, 24.0) * Scale,
			FVector2D(360.0, 180.0) * Scale);
		const FBox2D ScrollBarRect(
			FVector2D(ViewportSize.X - 28.0 * Scale, 0.0),
			ViewportSize);
		TestFalse(TEXT("fixed Close Challenge avoids the left summary"), RectanglesOverlap(Rect, SummaryRect));
		TestFalse(TEXT("fixed Close Challenge leaves a scrollbar safety gap"), RectanglesOverlap(Rect, ScrollBarRect));
	}
	const FBox2D FullHdRect = Widget->ResolveRouteCloseChallengeRectForTest(FVector2D(1920.0f, 1080.0f));
	TestTrue(TEXT("full-HD CloseInk x keeps the approved 72px right safety margin"), FMath::IsNearlyEqual(FullHdRect.Min.X, 1774.0, 0.01));
	TestTrue(TEXT("full-HD Close Challenge y matches Luna evidence"), FMath::IsNearlyEqual(FullHdRect.Min.Y, 48.0, 0.01));
	TestTrue(TEXT("full-HD CloseInk uses the same square size as route events"), FullHdRect.GetSize().Equals(FVector2D(74.0f, 74.0f), 0.01f));
	const FBox2D BeforeScrollRect = Widget->ResolveRouteCloseChallengeRectForTest(FVector2D(1920.0f, 1080.0f));
	Widget->ApplyRouteMapDragDeltaForTest(160.0f);
	TestTrue(TEXT("scrolling route content never moves fixed Close Challenge"),
		Widget->ResolveRouteCloseChallengeRectForTest(FVector2D(1920.0f, 1080.0f)).Min.Equals(BeforeScrollRect.Min, 0.01f));
	if (CloseButton)
	{
		CloseButton->OnClicked.Broadcast();
	}
	TestTrue(TEXT("the production CloseInk click opens the settlement modal"),
		Widget->IsRouteAbandonConfirmationOpenForTest());
	TestTrue(TEXT("the settlement modal opened by CloseInk can return to the route"),
		Widget->CancelRouteAbandonConfirmationForTest());
	Widget->ApplyMissingRouteCloseInkResourceForTest();
	UTextBlock* FallbackLabel = CloseButton ? Cast<UTextBlock>(CloseButton->GetContent()) : nullptr;
	TestEqual(TEXT("missing CloseInk resource exposes a clear text fallback"),
		FallbackLabel ? FallbackLabel->GetText().ToString() : FString(),
		FString(TEXT("X")));
	TestTrue(TEXT("missing CloseInk fallback remains enabled and clickable"),
		CloseButton && CloseButton->GetIsEnabled());
	if (CloseButton)
	{
		CloseButton->OnClicked.Broadcast();
	}
	TestTrue(TEXT("fallback X keeps the same production settlement click path"),
		Widget->IsRouteAbandonConfirmationOpenForTest());
	TestTrue(TEXT("fallback X settlement modal can still return to route"),
		Widget->CancelRouteAbandonConfirmationForTest());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteMapAbandonPreviewCancelTest,
	"GameXXK.MVP.RouteMap.AbandonConfirmation.PreviewCancelAndInputGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteMapAbandonPreviewCancelTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = nullptr;
	UGameXXKOneGameRouteMapWidget* Widget = nullptr;
	if (!TestTrue(TEXT("preview-cancel fixture builds"), BuildRouteAbandonWidgetFixture(Subsystem, Widget)))
	{
		return false;
	}
	Subsystem->GetMutableRuntimeState().CardRun.RouteTravelMoney = 99;
	Subsystem->GetMutableRuntimeState().CardRun.RouteProgress.ActualRouteCardAcquisitionCount = 29;
	Widget->RefreshFromState();
	const FGameXXKRuntimeState BeforeModal = Subsystem->GetRuntimeState();
	const float ScrollBeforeModal = Widget->GetLastAppliedScrollOffsetForTest();
	const int32 ReachableNodeId = Subsystem->GetRuntimeState().ReachableRouteNodeIds[0];

	TestTrue(TEXT("Close Challenge opens settlement confirmation"), Widget->OpenRouteAbandonConfirmationForTest());
	TestTrue(TEXT("route abandon confirmation is visible"), Widget->IsRouteAbandonConfirmationOpenForTest());
	TestTrue(TEXT("valid route enables settlement confirmation"), Widget->IsRouteAbandonConfirmEnabledForTest());
	UButton* ModalConfirmButton = Widget->WidgetTree
		? Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("RouteAbandonConfirmButton")))
		: nullptr;
	TestTrue(TEXT("square CloseInk does not shrink the existing modal action brush"),
		ModalConfirmButton
		&& ModalConfirmButton->GetStyle().Normal.ImageSize.Equals(FVector2D(192.0f, 64.0f), 0.01f));
	UTextBlock* ModalTitle = Widget->WidgetTree
		? Cast<UTextBlock>(Widget->WidgetTree->FindWidget(TEXT("RouteAbandonModalTitle")))
		: nullptr;
	UTextBlock* ModalDescription = Widget->WidgetTree
		? Cast<UTextBlock>(Widget->WidgetTree->FindWidget(TEXT("RouteAbandonModalDescription")))
		: nullptr;
	UTextBlock* ConfirmLabel = Widget->WidgetTree
		? Cast<UTextBlock>(Widget->WidgetTree->FindWidget(TEXT("RouteAbandonConfirmLabel")))
		: nullptr;
	UTextBlock* CancelLabel = Widget->WidgetTree
		? Cast<UTextBlock>(Widget->WidgetTree->FindWidget(TEXT("RouteAbandonCancelLabel")))
		: nullptr;
	TestEqual(TEXT("modal uses the approved settlement title"),
		ModalTitle ? ModalTitle->GetText().ToString() : FString(),
		FString(TEXT("本次路线结算")));
	TestEqual(TEXT("modal confirm returns to idle workbench"),
		ConfirmLabel ? ConfirmLabel->GetText().ToString() : FString(),
		FString(TEXT("确认结算并返回挂机")));
	TestEqual(TEXT("modal cancel keeps the route active"),
		CancelLabel ? CancelLabel->GetText().ToString() : FString(),
		FString(TEXT("继续路线")));
	const FString PreviewCopy = Widget->GetRouteAbandonPreviewTextForTest().ToString();
	TestTrue(TEXT("settlement preview identifies already-earned rewards"), PreviewCopy.Contains(TEXT("已获奖励")));
	TestTrue(TEXT("settlement preview lists ordinary-gold conversion"), PreviewCopy.Contains(TEXT("普通金币 +4")));
	TestTrue(TEXT("settlement preview lists earned item rewards"), PreviewCopy.Contains(TEXT("强化石 +2")));
	TestTrue(TEXT("settlement preview lists completed progress"), PreviewCopy.Contains(TEXT("已完成进度")));
	TestTrue(TEXT("settlement preview warns unresolved progress is forfeited"), PreviewCopy.Contains(TEXT("未解决")));
	TestTrue(TEXT("settlement description explains only earned content is settled"),
		ModalDescription && ModalDescription->GetText().ToString().Contains(TEXT("已获")));
	TestTrue(TEXT("opening preview has no runtime side effects"), RouteRuntimeStatesEqual(Subsystem->GetRuntimeState(), BeforeModal));
	TestFalse(TEXT("modal disables route scrolling"), Widget->GetRouteScrollBoxForTest() && Widget->GetRouteScrollBoxForTest()->GetIsEnabled());
	TestFalse(TEXT("modal blocks drag scroll"), Widget->ApplyRouteMapDragDeltaForTest(160.0f));
	TestEqual(TEXT("modal preserves scroll offset"), Widget->GetLastAppliedScrollOffsetForTest(), ScrollBeforeModal);
	TestFalse(TEXT("modal blocks route node execution"), Widget->ExecuteRouteNodeById(ReachableNodeId));
	TestTrue(TEXT("blocked route input preserves runtime"), RouteRuntimeStatesEqual(Subsystem->GetRuntimeState(), BeforeModal));

	TestTrue(TEXT("Continue Route cancels settlement confirmation"), Widget->CancelRouteAbandonConfirmationForTest());
	TestFalse(TEXT("cancel hides route abandon confirmation"), Widget->IsRouteAbandonConfirmationOpenForTest());
	TestTrue(TEXT("cancel preserves runtime exactly"), RouteRuntimeStatesEqual(Subsystem->GetRuntimeState(), BeforeModal));
	TestTrue(TEXT("cancel restores route scrolling"), Widget->GetRouteScrollBoxForTest() && Widget->GetRouteScrollBoxForTest()->GetIsEnabled());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteMapAbandonConfirmAndFailureTest,
	"GameXXK.MVP.RouteMap.AbandonConfirmation.ConfirmOnceAndFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteMapAbandonConfirmAndFailureTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = nullptr;
	UGameXXKOneGameRouteMapWidget* Widget = nullptr;
	if (!TestTrue(TEXT("confirm fixture builds"), BuildRouteAbandonWidgetFixture(Subsystem, Widget)))
	{
		return false;
	}
	Subsystem->GetMutableRuntimeState().CardRun.RouteTravelMoney = 99;
	Subsystem->GetMutableRuntimeState().CardRun.RouteProgress.ActualRouteCardAcquisitionCount = 29;
	const int32 GoldBefore = Subsystem->GetRuntimeState().PlayerGold;
	const int32 StonesBefore = UGameXXKMVPRules::GetItemCount(
		Subsystem->GetRuntimeState(),
		UGameXXKMVPRules::ItemEnhancementStone());
	TestTrue(TEXT("confirm fixture opens modal"), Widget->OpenRouteAbandonConfirmationForTest());
	TestTrue(TEXT("confirm applies abandoned settlement"), Widget->ConfirmRouteAbandonForTest());
	TestEqual(TEXT("confirm returns to town"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);
	TestEqual(TEXT("confirm returns to canonical DesktopTrainingHUD"),
		Subsystem->GetRuntimeState().CurrentMapId,
		FName(TEXT("DesktopTrainingHUD")));
	TestFalse(TEXT("confirm ends active route"), Subsystem->GetRuntimeState().bDungeonActive);
	TestFalse(TEXT("confirm clears generated route topology"), Subsystem->GetRuntimeState().bHasGeneratedRouteMap);
	TestEqual(TEXT("confirm awards previewed gold once"), Subsystem->GetRuntimeState().PlayerGold, GoldBefore + 4);
	TestEqual(TEXT("confirm awards previewed stones once"),
		UGameXXKMVPRules::GetItemCount(Subsystem->GetRuntimeState(), UGameXXKMVPRules::ItemEnhancementStone()),
		StonesBefore + 2);
	TestFalse(TEXT("a second confirmation is rejected"), Widget->ConfirmRouteAbandonForTest());
	TestEqual(TEXT("a second confirmation cannot duplicate gold"), Subsystem->GetRuntimeState().PlayerGold, GoldBefore + 4);

	UGameXXKMVPSubsystem* RetrySubsystem = nullptr;
	UGameXXKOneGameRouteMapWidget* RetryWidget = nullptr;
	if (!TestTrue(TEXT("valid-preview failure fixture builds"), BuildRouteAbandonWidgetFixture(RetrySubsystem, RetryWidget)))
	{
		return false;
	}
	RetrySubsystem->GetMutableRuntimeState().CardRun.RouteTravelMoney = 99;
	RetrySubsystem->GetMutableRuntimeState().CardRun.RouteProgress.ActualRouteCardAcquisitionCount = 29;
	const int32 RetryGoldBefore = RetrySubsystem->GetRuntimeState().PlayerGold;
	TestTrue(TEXT("valid-preview failure fixture opens an enabled modal"),
		RetryWidget->OpenRouteAbandonConfirmationForTest());
	RetrySubsystem->GetMutableRuntimeState().PlayerGold = MAX_int32;
	const FGameXXKRuntimeState RetryFailureBefore = RetrySubsystem->GetRuntimeState();
	UButton* RetryConfirmButton = RetryWidget->WidgetTree
		? Cast<UButton>(RetryWidget->WidgetTree->FindWidget(TEXT("RouteAbandonConfirmButton")))
		: nullptr;
	if (RetryConfirmButton)
	{
		RetryConfirmButton->OnClicked.Broadcast();
	}
	TestTrue(TEXT("real confirm broadcast keeps modal open after transaction failure"),
		RetryWidget->IsRouteAbandonConfirmationOpenForTest());
	TestTrue(TEXT("failed confirm rolls back every authoritative runtime property"),
		RouteRuntimeStatesEqual(RetrySubsystem->GetRuntimeState(), RetryFailureBefore));
	TestTrue(TEXT("failed confirm exposes a Chinese settlement error"),
		RetryWidget->GetRouteAbandonErrorForTest().Contains(TEXT("结算失败")));
	UTextBlock* RetryErrorText = RetryWidget->WidgetTree
		? Cast<UTextBlock>(RetryWidget->WidgetTree->FindWidget(TEXT("RouteAbandonModalError")))
		: nullptr;
	TestTrue(TEXT("failed confirm keeps its concrete error visible"),
		RetryErrorText
		&& RetryErrorText->GetVisibility() == ESlateVisibility::HitTestInvisible
		&& RetryErrorText->GetText().ToString().Contains(TEXT("结算失败")));
	TestTrue(TEXT("failed confirm resets in-progress and re-enables retry"),
		RetryWidget->IsRouteAbandonConfirmEnabledForTest());
	RetrySubsystem->GetMutableRuntimeState().PlayerGold = RetryGoldBefore;
	if (RetryConfirmButton)
	{
		RetryConfirmButton->OnClicked.Broadcast();
	}
	TestFalse(TEXT("same valid modal closes after retry succeeds"),
		RetryWidget->IsRouteAbandonConfirmationOpenForTest());
	TestEqual(TEXT("retry returns to Town"), RetrySubsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);
	TestEqual(TEXT("retry awards previewed gold exactly once"),
		RetrySubsystem->GetRuntimeState().PlayerGold,
		RetryGoldBefore + 4);
	const int32 RetryGoldAfter = RetrySubsystem->GetRuntimeState().PlayerGold;
	TestFalse(TEXT("post-success retry is rejected"), RetryWidget->ConfirmRouteAbandonForTest());
	TestEqual(TEXT("post-success retry cannot duplicate gold"),
		RetrySubsystem->GetRuntimeState().PlayerGold,
		RetryGoldAfter);

	UGameXXKMVPSubsystem* InvalidSubsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	TestTrue(TEXT("invalid preview fixture starts"), InvalidSubsystem->StartGame());
	InvalidSubsystem->GetMutableRuntimeState().Screen = EGameXXKScreen::DungeonMap;
	InvalidSubsystem->GetMutableRuntimeState().CurrentMapId = TEXT("HuangshanRoute");
	UGameXXKOneGameRouteMapWidget* InvalidWidget = NewObject<UGameXXKOneGameRouteMapWidget>();
	InvalidWidget->SetMVPSubsystem(InvalidSubsystem);
	TestTrue(TEXT("invalid preview widget initializes"), InvalidWidget->Initialize());
	InvalidWidget->NativeConstruct();
	const FGameXXKRuntimeState InvalidBefore = InvalidSubsystem->GetRuntimeState();
	TestTrue(TEXT("invalid preview still opens an explanatory modal"), InvalidWidget->OpenRouteAbandonConfirmationForTest());
	TestFalse(TEXT("invalid preview disables settlement confirmation"), InvalidWidget->IsRouteAbandonConfirmEnabledForTest());
	TestFalse(TEXT("invalid preview exposes a concrete error"), InvalidWidget->GetRouteAbandonErrorForTest().IsEmpty());
	TestFalse(TEXT("invalid preview cannot confirm"), InvalidWidget->ConfirmRouteAbandonForTest());
	TestTrue(TEXT("failed settlement remains atomic"), RouteRuntimeStatesEqual(InvalidSubsystem->GetRuntimeState(), InvalidBefore));
	return true;
}

#endif
