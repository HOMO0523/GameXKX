#include "UI/GameXXKOneGameRouteMapWidget.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteNodeFeedbackTest,
	"GameXXK.MVP.RouteMap.SelectionFeedback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteNodeFeedbackTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	Subsystem->GetMutableRuntimeState().Screen = EGameXXKScreen::DungeonMap;
	UGameXXKOneGameRouteMapWidget* Widget = NewObject<UGameXXKOneGameRouteMapWidget>();
	Widget->SetMVPSubsystem(Subsystem);
	Widget->Initialize();
	Widget->NativeConstruct();
	const TArray<FGameXXKRouteMapNode> Nodes = {
		{10, 0, 0, EGameXXKNodeKind::Start, FVector2D(0.5f, 0.0f), {20}},
		{20, 1, 0, EGameXXKNodeKind::Battle, FVector2D(0.5f, 0.05f), {30}},
		{30, 2, 0, EGameXXKNodeKind::Chest, FVector2D(0.5f, 1.0f), {}}};
	int32 Executions = 0;
	Widget->SetTransientRouteProjection(Nodes, {{10, 20}, {20, 30}}, {}, FText::GetEmpty(), {10}, {20},
		FGameXXKTransientRouteNodeExecuted::CreateLambda([&Executions](int32 NodeId)
		{
			++Executions;
			return NodeId == 20;
		}));
	Widget->RefreshFromState();
	Widget->SetRouteMapViewportGeometry(FVector2D::ZeroVector, FVector2D(1280.0f, 720.0f));
	Widget->RefreshFromState();
	UImage* EligibleIcon = Cast<UImage>(Widget->WidgetTree->FindWidget(TEXT("RouteNodeFallbackIcon1")));
	TestNotNull(TEXT("test inspects the actual icon image"), EligibleIcon);
	bool bSawTinyWiggle = false;
	const FVector2D NodeBeforeWiggle = Widget->GetRouteNodeVisualStatesForTest()[1].CanvasPosition;
	for (int32 Tick = 0; Tick < 300; ++Tick)
	{
		Widget->NativeTick(FGeometry(), 0.01f);
		if (EligibleIcon)
		{
			const auto Transform = EligibleIcon->GetRenderTransform();
			bSawTinyWiggle |= FMath::Abs(Transform.Angle) > 0.1f;
			if (FMath::Abs(Transform.Angle) > 2.21f || Transform.Translation.Size() > 1.21f)
				AddError(TEXT("eligible icon exceeds tiny-wiggle bounds"));
		}
	}
	TestTrue(TEXT("eligible icon periodically wiggles"), bSawTinyWiggle);
	TestTrue(TEXT("wiggle never moves node layout or hit targets"), Widget->GetRouteNodeVisualStatesForTest()[1].CanvasPosition.Equals(NodeBeforeWiggle));
	UButton* Button = Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("RouteNodeButton1")));
	if (!TestNotNull(TEXT("reachable node has a real clickable button"), Button)) return false;
	Button->OnClicked.Broadcast();
	TestEqual(TEXT("click leaves the map visible while the fast ink circle is drawn"), Executions, 0);
	UImage* Circle = Cast<UImage>(Widget->WidgetTree->FindWidget(TEXT("RouteNodeSelectionCircle1")));
	if (!TestNotNull(TEXT("selected node renders a brush circle"), Circle)) return false;
	TestNotNull(TEXT("circle uses the imported user atlas"), Circle->GetBrush().GetResourceObject());
	TestEqual(TEXT("circle cannot intercept clicks or drags"), Circle->GetVisibility(), ESlateVisibility::HitTestInvisible);
	TestFalse(TEXT("pending selection rejects another node"), Widget->SelectRouteNodeWithFeedback(30));
	Button->OnClicked.Broadcast();
	TestEqual(TEXT("double click does not execute early"), Executions, 0);
	Widget->NativeTick(FGeometry(), 0.08f);
	const int32 MidFrame = Widget->GetRouteNodeVisualStatesForTest()[1].SelectionCircleFrame;
	TestTrue(TEXT("circle advances through the drawing segment"), MidFrame > 3 && MidFrame < 16);
	Widget->RefreshFromState();
	TestEqual(TEXT("refresh does not restart the stroke"), Widget->GetRouteNodeVisualStatesForTest()[1].SelectionCircleFrame, MidFrame);
	Widget->NativeTick(FGeometry(), 0.09f);
	TestEqual(TEXT("complete circle is shown before entering the node"), Widget->GetRouteNodeVisualStatesForTest()[1].SelectionCircleFrame, 16);
	TestEqual(TEXT("completed stroke holds briefly on the map"), Executions, 0);
	Widget->NativeTick(FGeometry(), 0.06f);
	const FVector2D CenteredNode = Widget->GetRouteNodeVisualStatesForTest()[1].CanvasPosition;
	TestTrue(TEXT("click centers selected layer using vertical scroll only"),
		FMath::IsNearlyEqual(static_cast<float>(CenteredNode.Y) - Widget->GetLastAppliedScrollOffsetForTest(), 360.0f, 0.5f));
	TestEqual(TEXT("centering preserves node X"), CenteredNode.X, NodeBeforeWiggle.X);
	TestEqual(TEXT("one accepted node executes after the quick stroke"), Executions, 1);
	TestEqual(TEXT("successful selection leaves a static circle before settlement"), Widget->GetRouteNodeVisualStatesForTest()[1].SelectionCircleFrame, 32);
	Widget->NativeTick(FGeometry(), 1.0f);
	TestEqual(TEXT("later ticks cannot execute the node again"), Executions, 1);

	Widget->SetTransientRouteProjection(Nodes, {{10, 20}, {20, 30}}, {}, FText::GetEmpty(), {10, 20}, {30},
		FGameXXKTransientRouteNodeExecuted::CreateLambda([&Executions](int32)
		{
			++Executions;
			return true;
		}));
	Widget->RefreshFromState();
	TestFalse(TEXT("automatic start is not marked as a player selection"), Widget->GetRouteNodeVisualStatesForTest()[0].bSelectionCircleVisible);
	TestEqual(TEXT("visited node restores the static circle without replay"), Widget->GetRouteNodeVisualStatesForTest()[1].SelectionCircleFrame, 32);
	Widget->SetRouteMapViewportGeometry(FVector2D(80.0f, 40.0f), FVector2D(1280.0f, 720.0f));
	Widget->RestoreScrollOffset(120.0f);
	Widget->RefreshFromState();
	Circle = Cast<UImage>(Widget->WidgetTree->FindWidget(TEXT("RouteNodeSelectionCircle1")));
	const UCanvasPanelSlot* CircleSlot = Circle ? Cast<UCanvasPanelSlot>(Circle->Slot) : nullptr;
	if (TestNotNull(TEXT("selection circle belongs to the scrolling route canvas"), CircleSlot))
	{
		const FVector2D PaintedCenter = CircleSlot->GetPosition() + CircleSlot->GetSize() * FVector2D(0.5f, 135.5f / 256.0f);
		TestTrue(TEXT("painted circle center matches its node after resize and scroll"),
			PaintedCenter.Equals(Widget->GetRouteNodeVisualStatesForTest()[1].CanvasPosition, 0.01f));
	}
	TestEqual(TEXT("resize retains the static circle"), Widget->GetRouteNodeVisualStatesForTest()[1].SelectionCircleFrame, 32);
	TestTrue(TEXT("next reachable node starts feedback"), Widget->SelectRouteNodeWithFeedback(30));
	Subsystem->GetMutableRuntimeState().Screen = EGameXXKScreen::Battle;
	Widget->RefreshFromState();
	Widget->NativeTick(FGeometry(), 1.0f);
	TestEqual(TEXT("leaving the map cancels deferred node execution"), Executions, 1);
	Subsystem->GetMutableRuntimeState().Screen = EGameXXKScreen::DungeonMap;
	Widget->RefreshFromState();
	TestTrue(TEXT("returning to the map permits a fresh selection"), Widget->SelectRouteNodeWithFeedback(30));
	Widget->ClearTransientRouteProjection();
	Widget->SetTransientRouteProjection(Nodes, {{10, 20}, {20, 30}}, {}, FText::GetEmpty(), {10}, {20},
		FGameXXKTransientRouteNodeExecuted::CreateLambda([&Executions](int32)
		{
			++Executions;
			return true;
		}));
	Widget->RefreshFromState();
	TestFalse(TEXT("new route with reused node ids has no stale circle"), Widget->GetRouteNodeVisualStatesForTest()[1].bSelectionCircleVisible);
	TestTrue(TEXT("new route can select a node"), Widget->SelectRouteNodeWithFeedback(20));
	Widget->NativeOnKeyDown(FGeometry(), FKeyEvent(EKeys::Q, FModifierKeysState(), 0, false, 0, 0));
	Widget->NativeTick(FGeometry(), 1.0f);
	TestEqual(TEXT("opening the story tree cancels deferred node execution"), Executions, 1);
	TestTrue(TEXT("selection works again after the shortcut cancellation"), Widget->SelectRouteNodeWithFeedback(20));
	Widget->NativeDestruct();
	Widget->NativeTick(FGeometry(), 1.0f);
	TestEqual(TEXT("destruction cancels deferred node execution"), Executions, 1);
	return true;
}

#endif
