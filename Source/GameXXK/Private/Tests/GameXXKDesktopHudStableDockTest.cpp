#include "UI/GameXXKDesktopTrainingLayout.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopHudStableDockPlacementTest,
	"GameXXK.DesktopTraining.Workbench.StableDockPlacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopHudStableDockPlacementTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKDesktopTrainingLayout;
	const FVector2D WorkArea(1920.0f, 1020.0f);
	constexpr float NoticeHeight = 52.0f;
	const int32 ScalePercents[] = {50, 75, 100};
	for (const int32 ScalePercent : ScalePercents)
	{
		const FDesktopOverlayPlacement TopCollapsed = ComputeDesktopOverlayPlacement(
			WorkArea,
			FVector2D(0.5f, 0.0f),
			ScalePercent,
			false,
			false,
			NoticeHeight);
		const FDesktopOverlayPlacement DownExpanded = ComputeDesktopOverlayPlacement(
			WorkArea,
			FVector2D(0.5f, 0.0f),
			ScalePercent,
			true,
			false,
			NoticeHeight);
		TestTrue(
			*FString::Printf(TEXT("%d percent downward expansion preserves the idle-strip anchor"), ScalePercent),
			TopCollapsed.StripTopLeft.Equals(DownExpanded.StripTopLeft, 0.01f));
		const FVector2D CollapsedTabOffset(953.0f, 202.0f);
		const FVector2D DownExpandedTabOffset =
			GetExpandedNoticeRailPosition(false) + FVector2D(953.0f, 0.0f);
		TestTrue(
			*FString::Printf(TEXT("%d percent downward expansion preserves the Tab anchor"), ScalePercent),
			(TopCollapsed.HudTopLeft + CollapsedTabOffset * TopCollapsed.Scale).Equals(
				DownExpanded.HudTopLeft + DownExpandedTabOffset * DownExpanded.Scale,
				0.01f));

		const FDesktopOverlayPlacement BottomCollapsed = ComputeDesktopOverlayPlacement(
			WorkArea,
			FVector2D(0.5f, 1.0f),
			ScalePercent,
			false,
			false,
			NoticeHeight);
		const FDesktopOverlayPlacement UpExpanded = ComputeDesktopOverlayPlacement(
			WorkArea,
			FVector2D(0.5f, 1.0f),
			ScalePercent,
			true,
			true,
			NoticeHeight);
		TestTrue(
			*FString::Printf(TEXT("%d percent upward expansion preserves the idle-strip anchor"), ScalePercent),
			BottomCollapsed.StripTopLeft.Equals(UpExpanded.StripTopLeft, 0.01f));
		const FVector2D UpExpandedTabOffset =
			GetExpandedNoticeRailPosition(true) + FVector2D(953.0f, 0.0f);
		TestTrue(
			*FString::Printf(TEXT("%d percent upward expansion preserves the Tab anchor"), ScalePercent),
			(BottomCollapsed.HudTopLeft + CollapsedTabOffset * BottomCollapsed.Scale).Equals(
				UpExpanded.HudTopLeft + UpExpandedTabOffset * UpExpanded.Scale,
				0.01f));

		const float EdgeAnchors[] = {0.0f, 1.0f};
		for (const float EdgeAnchorX : EdgeAnchors)
		{
			const FDesktopOverlayPlacement EdgeCollapsed = ComputeDesktopOverlayPlacement(
				WorkArea,
				FVector2D(EdgeAnchorX, 1.0f),
				ScalePercent,
				false,
				false,
				NoticeHeight);
			const FDesktopOverlayPlacement EdgeExpanded = ComputeDesktopOverlayPlacement(
				WorkArea,
				FVector2D(EdgeAnchorX, 1.0f),
				ScalePercent,
				true,
				true,
				NoticeHeight);
			TestTrue(
				*FString::Printf(
					TEXT("%d percent upward expansion preserves the idle-strip anchor at horizontal edge %.0f"),
					ScalePercent,
					EdgeAnchorX),
				EdgeCollapsed.StripTopLeft.Equals(EdgeExpanded.StripTopLeft, 0.01f));
			TestTrue(
				*FString::Printf(
					TEXT("%d percent upward expansion preserves the Tab anchor at horizontal edge %.0f"),
					ScalePercent,
					EdgeAnchorX),
				(EdgeCollapsed.HudTopLeft + CollapsedTabOffset * EdgeCollapsed.Scale).Equals(
					EdgeExpanded.HudTopLeft + UpExpandedTabOffset * EdgeExpanded.Scale,
					0.01f));
		}
	}

	TestEqual(
		TEXT("expanded idle strip keeps the collapsed logical footprint"),
		GetIdleStripRect(),
		FVector4(318.0f, 0.0f, 1038.0f, 202.0f));

	const FDesktopHudResolvedMetrics FullMetrics = ResolveDesktopHudMetrics(WorkArea, 100);
	const FDesktopOverlayPlacement WorkAreaHost = ResolveDesktopWorkAreaHostPlacement(FullMetrics);
	TestEqual(TEXT("desktop native host covers the physical work area"), WorkAreaHost.HudSize, WorkArea);
	TestEqual(TEXT("desktop native host starts at the work-area origin"), WorkAreaHost.HudTopLeft, FVector2D::ZeroVector);

	const FDesktopOverlayPlacement RightDock = ComputeDesktopOverlayPlacement(
		WorkArea,
		FVector2D(1.0f, 0.5f),
		100,
		true,
		false,
		NoticeHeight,
		true);
	TestTrue(
		TEXT("right-edge warehouse/backpack/training body moves left as one unit"),
		ResolveExpandedBodyFitOffset(
			WorkArea,
			RightDock.HudTopLeft,
			RightDock.ContentOffset,
			RightDock.Scale,
			true,
			true,
			false).Equals(FVector2D(-304.0f, 0.0f), 0.01f));
	const FDesktopOverlayPlacement LeftDock = ComputeDesktopOverlayPlacement(
		WorkArea,
		FVector2D(0.0f, 0.5f),
		100,
		true,
		true,
		NoticeHeight,
		true);
	TestTrue(
		TEXT("left-edge body moves right while retaining the upward body offset"),
		ResolveExpandedBodyFitOffset(
			WorkArea,
			LeftDock.HudTopLeft,
			LeftDock.ContentOffset,
			LeftDock.Scale,
			true,
			true,
			true).Equals(FVector2D(466.0f, -210.0f), 0.01f));

	TestTrue(
		TEXT("50 percent carried icon uses physical client pixels and 120-DPI Slate units"),
		ResolveDesktopCursorSlateRect(
			FVector2D(500.0f, 300.0f),
			0.5f,
			1.25f,
			FVector2D(56.0f, 56.0f)).Equals(
				FVector4(388.8f, 228.8f, 22.4f, 22.4f),
				0.01f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopHudUpwardNativeRegionTest,
	"GameXXK.DesktopTraining.Workbench.UpwardNativeRegion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopHudUpwardNativeRegionTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKDesktopTrainingLayout;
	FDesktopNativeRegionState State;
	State.bExpanded = true;
	State.bExpandUpward = true;
	State.NoticeHeight = 52.0f;
	const TArray<FDesktopNativeRegionShape> Shapes = BuildDesktopNativeRegionShapes(State);

	TestTrue(
		TEXT("upward center content is interactive at its rendered position"),
		IsPointInsideDesktopNativeRegionShapes(Shapes, FVector2D(700.0f, 100.0f)));
	TestTrue(
		TEXT("upward toolbar is interactive at its rendered position"),
		IsPointInsideDesktopNativeRegionShapes(Shapes, FVector2D(1100.0f, 20.0f)));
	TestTrue(
		TEXT("upward notice and Tab rail stays below the fixed strip"),
		IsPointInsideDesktopNativeRegionShapes(Shapes, FVector2D(1200.0f, 945.0f)));
	TestFalse(
		TEXT("the old above-strip Tab rail no longer owns native input"),
		IsPointInsideDesktopNativeRegionShapes(Shapes, FVector2D(950.0f, 700.0f)));
	TestFalse(
		TEXT("the old unshifted lower content band no longer owns native input"),
		IsPointInsideDesktopNativeRegionShapes(Shapes, FVector2D(900.0f, 720.0f)));
	return true;
}

#endif
