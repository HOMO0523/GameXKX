#include "GameXXKMVPRules.h"
#include "MVP/GameXXKLevelFlow.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "Engine/GameInstance.h"
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

	TestTrue(TEXT("adapter initializes widget tree"), RouteWidget->Initialize());
	RouteWidget->NativeConstruct();
	RouteWidget->RefreshFromState();
	TestEqual(TEXT("adapter creates one visual per generated route node"), RouteWidget->GetCreatedNodeVisualWidgetCount(), Subsystem->GetRuntimeState().RouteMapNodes.Num());
	TestEqual(TEXT("adapter creates one visual per generated route edge"), RouteWidget->GetCreatedLineVisualWidgetCount(), Subsystem->GetRuntimeState().RouteMapEdges.Num());
	TestEqual(TEXT("fallback route node visual uses a border shell"), RouteWidget->GetCreatedNodeVisualWidgetClassName(0), FString(TEXT("Border")));
	TestEqual(TEXT("fallback route line visual uses a border shell"), RouteWidget->GetCreatedLineVisualWidgetClassName(0), FString(TEXT("Border")));
	TestFalse(TEXT("fallback current node label is bound"), RouteWidget->GetCreatedNodeVisualLabel(0).IsEmpty());
	TestFalse(TEXT("fallback next node label is bound"), RouteWidget->GetCreatedNodeVisualLabel(1).IsEmpty());
	TestTrue(TEXT("fallback start node uses 1Game footprint texture"), RouteWidget->GetCreatedNodeVisualIconPath(0).Contains(TEXT("脚印")));
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
		TEXT("battle node targets GameXXK-owned 1Game battle map"),
		GameXXKLevelFlow::MapForRuntimeState(Subsystem->GetRuntimeState()),
		FName(TEXT("/Game/GameXXK/Maps/L_Battle_1Game")));

	return true;
}

#endif
