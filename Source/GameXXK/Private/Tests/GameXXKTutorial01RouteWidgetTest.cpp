#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKTutorial01RouteRules.h"
#include "UI/GameXXKOneGameRouteMapWidget.h"

#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTutorial01RouteWidgetTest,
	"GameXXK.Tutorial01.RouteWidget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTutorial01RouteWidgetTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem =
		NewObject<UGameXXKMVPSubsystem>(GameInstance);
	if (!TestTrue(TEXT("route widget fixture starts a playable runtime"),
		Subsystem && Subsystem->EnsureQingshanTownRuntimeForDirectMap()))
	{
		return false;
	}
	Subsystem->GetMutableRuntimeState().Screen = EGameXXKScreen::DungeonMap;
	const FGameXXKRuntimeState RuntimeBefore = Subsystem->GetRuntimeStateCopy();

	FGameXXKTutorial01RouteState Route;
	FGameXXKTutorial01RouteRules::Initialize(Route);
	int32 ExecutedNodeId = INDEX_NONE;
	EGameXXKTutorial01RouteAction ExecutedAction =
		EGameXXKTutorial01RouteAction::None;
	FGameXXKTransientRouteNodeExecuted ExecuteDelegate =
		FGameXXKTransientRouteNodeExecuted::CreateLambda(
			[&Route, &ExecutedNodeId, &ExecutedAction](const int32 NodeId)
			{
				ExecutedNodeId = NodeId;
				return FGameXXKTutorial01RouteRules::RequestNode(
					Route,
					NodeId,
					ExecutedAction);
			});

	UGameXXKOneGameRouteMapWidget* Widget =
		NewObject<UGameXXKOneGameRouteMapWidget>();
	Widget->SetMVPSubsystem(Subsystem);
	Widget->TakeWidget();
	Widget->SetRouteMapViewportGeometry(
		FVector2D::ZeroVector,
		FVector2D(1280.0f, 720.0f));
	Widget->SetTransientRouteProjection(
		FGameXXKTutorial01RouteRules::BuildNodes(Route),
		FGameXXKTutorial01RouteRules::BuildEdges(),
		FGameXXKTutorial01RouteRules::BuildLabels(),
		FGameXXKTutorial01RouteRules::BuildCompletionNotice(Route),
		Route.VisitedNodeIds,
		Route.ReachableNodeIds,
		ExecuteDelegate);
	Widget->RefreshFromState();

	TestTrue(TEXT("widget reports transient route mode"),
		Widget->IsUsingTransientRouteProjectionForTest());
	TestEqual(TEXT("exactly three tutorial node visuals"),
		Widget->GetCreatedNodeVisualWidgetCount(),
		3);
	TestEqual(TEXT("exactly two tutorial edge visuals"),
		Widget->GetCreatedLineVisualWidgetCount(),
		2);
	TestFalse(TEXT("ordinary summary is hidden in tutorial mode"),
		Widget->IsOrdinaryRouteSummaryVisibleForTest());
	TestFalse(TEXT("completion notice is hidden before victory"),
		Widget->IsTransientCompletionNoticeVisibleForTest());
	TestTrue(TEXT("completion notice text starts empty"),
		Widget->GetTransientCompletionNoticeForTest().IsEmpty());

	const TArray<FGameXXKOneGameRouteNode> InitialNodes =
		Widget->BuildAdapterNodes();
	TestEqual(TEXT("adapter exposes fixed three nodes"), InitialNodes.Num(), 3);
	if (InitialNodes.Num() == 3)
	{
		TestEqual(TEXT("start label is exact"),
			InitialNodes[0].Label.ToString(),
			FString(TEXT("起点")));
		TestEqual(TEXT("battle label is exact"),
			InitialNodes[1].Label.ToString(),
			FString(TEXT("0-1 战斗")));
		TestEqual(TEXT("return label is exact"),
			InitialNodes[2].Label.ToString(),
			FString(TEXT("返回青山镇")));
		TestTrue(TEXT("start is visited"), InitialNodes[0].bVisited);
		TestFalse(TEXT("start is disabled"), InitialNodes[0].bEnabled);
		TestTrue(TEXT("battle alone is enabled"), InitialNodes[1].bEnabled);
		TestFalse(TEXT("return is initially disabled"), InitialNodes[2].bEnabled);
	}

	TestFalse(TEXT("visited start cannot execute"),
		Widget->ExecuteRouteNodeById(
			FGameXXKTutorial01RouteRules::StartNodeId));
	TestTrue(TEXT("battle executes through transient delegate"),
		Widget->ExecuteRouteNodeById(
			FGameXXKTutorial01RouteRules::BattleNodeId));
	TestEqual(TEXT("delegate receives stable battle id"),
		ExecutedNodeId,
		FGameXXKTutorial01RouteRules::BattleNodeId);
	TestEqual(TEXT("delegate reports battle action"),
		ExecutedAction,
		EGameXXKTutorial01RouteAction::StartBattle);
	TestFalse(TEXT("return remains locked before victory"),
		Widget->ExecuteRouteNodeById(
			FGameXXKTutorial01RouteRules::ReturnTownNodeId));

	TestEqual(TEXT("transient click preserves ordinary route seed"),
		Subsystem->GetRuntimeState().RouteSeed,
		RuntimeBefore.RouteSeed);
	TestTrue(TEXT("transient click preserves ordinary route nodes"),
		Subsystem->GetRuntimeState().RouteMapNodes.Num()
			== RuntimeBefore.RouteMapNodes.Num());
	TestTrue(TEXT("transient click preserves ordinary visited ids"),
		Subsystem->GetRuntimeState().VisitedRouteNodeIds
			== RuntimeBefore.VisitedRouteNodeIds);
	TestTrue(TEXT("transient click preserves ordinary reachable ids"),
		Subsystem->GetRuntimeState().ReachableRouteNodeIds
			== RuntimeBefore.ReachableRouteNodeIds);

	TestTrue(TEXT("route accepts victory"),
		FGameXXKTutorial01RouteRules::MarkVictory(Route));
	Widget->SetTransientRouteProjection(
		FGameXXKTutorial01RouteRules::BuildNodes(Route),
		FGameXXKTutorial01RouteRules::BuildEdges(),
		FGameXXKTutorial01RouteRules::BuildLabels(),
		FGameXXKTutorial01RouteRules::BuildCompletionNotice(Route),
		Route.VisitedNodeIds,
		Route.ReachableNodeIds,
		ExecuteDelegate);
	Widget->RefreshFromState();
	const TArray<FGameXXKOneGameRouteNode> VictoryNodes =
		Widget->BuildAdapterNodes();
	TestTrue(TEXT("battle is visited after victory"),
		VictoryNodes.Num() == 3 && VictoryNodes[1].bVisited);
	TestTrue(TEXT("return unlocks after victory"),
		VictoryNodes.Num() == 3 && VictoryNodes[2].bEnabled);
	TestTrue(TEXT("completion notice becomes visible"),
		Widget->IsTransientCompletionNoticeVisibleForTest());
	TestEqual(TEXT("completion notice is exact"),
		Widget->GetTransientCompletionNoticeForTest().ToString(),
		FString(TEXT("0-1 完成")));
	TestTrue(TEXT("return executes through transient delegate"),
		Widget->ExecuteRouteNodeById(
			FGameXXKTutorial01RouteRules::ReturnTownNodeId));
	TestEqual(TEXT("delegate reports return action"),
		ExecutedAction,
		EGameXXKTutorial01RouteAction::ReturnTown);

	Widget->ClearTransientRouteProjection();
	Widget->RefreshFromState();
	TestFalse(TEXT("clear restores ordinary adapter mode"),
		Widget->IsUsingTransientRouteProjectionForTest());
	TestTrue(TEXT("ordinary summary returns after clear"),
		Widget->IsOrdinaryRouteSummaryVisibleForTest());

	return true;
}

#endif
