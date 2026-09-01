#include "Misc/AutomationTest.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Input/HittestGrid.h"
#include "Rendering/DrawElements.h"
#include "Types/PaintArgs.h"
#include "UI/GameXXKBattleBoardWidget.h"
#include "UI/GameXXKBattleGuideBubbleWidget.h"
#include "UI/GameXXKGuideOverlayWidget.h"
#include "Widgets/SWindow.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKGuideSpotlightTestPrivate
{
	double Area(const FSlateRect& Rect)
	{
		return FMath::Max(0.0, static_cast<double>(Rect.Right - Rect.Left))
			* FMath::Max(0.0, static_cast<double>(Rect.Bottom - Rect.Top));
	}

	bool HasPositiveIntersection(const FSlateRect& A, const FSlateRect& B)
	{
		return FMath::Min(A.Right, B.Right) - FMath::Max(A.Left, B.Left) > KINDA_SMALL_NUMBER
			&& FMath::Min(A.Bottom, B.Bottom) - FMath::Max(A.Top, B.Top) > KINDA_SMALL_NUMBER;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKGuideMultiHoleDimRegionTest,
	"GameXXK.Guide.Widget.MultiHoleDimRegions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKGuideMultiHoleDimRegionTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKGuideSpotlightTestPrivate;
	const FVector2D HostSize(1920.0f, 1080.0f);
	const TArray<FSlateRect> Cutouts = {
		FSlateRect(100.0f, 100.0f, 300.0f, 300.0f),
		FSlateRect(250.0f, 150.0f, 450.0f, 350.0f)};
	const TArray<FSlateRect> DimRegions =
		UGameXXKGuideOverlayWidget::BuildDimRegions(HostSize, Cutouts, 0.0f);
	TestFalse(TEXT("multi-hole complement contains paint regions"), DimRegions.IsEmpty());

	double DimArea = 0.0;
	for (int32 RegionIndex = 0; RegionIndex < DimRegions.Num(); ++RegionIndex)
	{
		const FSlateRect& Region = DimRegions[RegionIndex];
		DimArea += Area(Region);
		for (const FSlateRect& Cutout : Cutouts)
		{
			TestFalse(TEXT("dim region never intersects a focus cutout"),
				HasPositiveIntersection(Region, Cutout));
		}
		for (int32 OtherIndex = RegionIndex + 1; OtherIndex < DimRegions.Num(); ++OtherIndex)
		{
			TestFalse(TEXT("dim regions never overlap each other"),
				HasPositiveIntersection(Region, DimRegions[OtherIndex]));
		}
	}
	const double FirstArea = 200.0 * 200.0;
	const double SecondArea = 200.0 * 200.0;
	const double OverlapArea = 50.0 * 150.0;
	const double ExpectedDimArea = HostSize.X * HostSize.Y
		- (FirstArea + SecondArea - OverlapArea);
	TestTrue(TEXT("dim area equals host minus cutout union"),
		FMath::Abs(DimArea - ExpectedDimArea) < 0.1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattleGuideBubbleLayoutTest,
	"GameXXK.Guide.Widget.BattleBubbleSafePlacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleGuideBubbleLayoutTest::RunTest(const FString& Parameters)
{
	const FVector2D HostSize(1920.0f, 1080.0f);
	const FSlateRect YueBaiNearRightEdge(1750.0f, 400.0f, 1850.0f, 550.0f);
	const FSlateRect BubbleRect = UGameXXKBattleGuideBubbleWidget::ResolveBubbleRect(
		YueBaiNearRightEdge,
		HostSize,
		true);
	TestTrue(TEXT("YueBai-anchored bubble prefers the space above 3P"),
		BubbleRect.Bottom <= YueBaiNearRightEdge.Top);
	TestTrue(TEXT("bubble remains inside left/top safe margin"),
		BubbleRect.Left >= 16.0f && BubbleRect.Top >= 16.0f);
	TestTrue(TEXT("bubble remains inside right/bottom safe margin"),
		BubbleRect.Right <= HostSize.X - 16.0f
		&& BubbleRect.Bottom <= HostSize.Y - 16.0f);

	UGameXXKBattleGuideBubbleWidget* Bubble =
		NewObject<UGameXXKBattleGuideBubbleWidget>();
	Bubble->TakeWidget();
	Bubble->PresentBubble(
		FText::FromString(TEXT("气力值：每回合出牌时消耗的点数。")),
		true,
		YueBaiNearRightEdge,
		HostSize,
		true);
	TestTrue(TEXT("battle guide bubble becomes visible"), Bubble->IsBubbleVisible());
	TestTrue(TEXT("information bubble shows Space hint"), Bubble->IsContinueHintVisible());
	TestTrue(TEXT("battle guide bubble uses the clean approved item-slot paper"),
		Bubble->GetPaperTexturePath().Contains(TEXT("T_MasterV2_ItemSlot")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKGuideOverlayMultiTargetPresentationTest,
	"GameXXK.Guide.Widget.MultiTargetPresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKGuideOverlayMultiTargetPresentationTest::RunTest(const FString& Parameters)
{
	UGameXXKGuideOverlayWidget* Overlay = NewObject<UGameXXKGuideOverlayWidget>();
	Overlay->TakeWidget();
	FGameXXKGuideOutput Output;
	Output.bActive = true;
	Output.InputPolicy = EGameXXKGuideInputPolicy::Forced;
	Output.Text = FText::FromString(TEXT("先点击横剑守势，再点击主角。"));
	Output.TargetIds = {
		TEXT("Battle.Hand.HengJianShouShi"),
		TEXT("Battle.Unit.Hero.Target")};
	Output.BubbleAnchorTargetId = TEXT("Battle.Unit.YueBai.Visual");
	const TArray<FSlateRect> Targets = {
		FSlateRect(400.0f, 700.0f, 620.0f, 980.0f),
		FSlateRect(1280.0f, 350.0f, 1480.0f, 650.0f)};
	const TOptional<FSlateRect> BubbleAnchor(
		FSlateRect(1450.0f, 420.0f, 1580.0f, 650.0f));
	Overlay->PresentGuide(Output, Targets, BubbleAnchor);
	TestEqual(TEXT("overlay retains both focus cutouts"),
		Overlay->GetTargetRectsForTest().Num(), 2);
	TestTrue(TEXT("overlay itself never accepts pointer input"),
		Overlay->GetVisibility() == ESlateVisibility::HitTestInvisible);
	TestTrue(TEXT("overlay creates one guide bubble"),
		Overlay->GetBattleBubbleForTest() != nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKGuideOverlayDismissedLayoutParticipationTest,
	"GameXXK.Guide.Widget.DismissedOverlayStaysLayoutable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKGuideOverlayDismissedLayoutParticipationTest::RunTest(
	const FString& Parameters)
{
	UGameXXKGuideOverlayWidget* Overlay =
		NewObject<UGameXXKGuideOverlayWidget>();
	Overlay->TakeWidget();
	Overlay->DismissGuide();

	TestFalse(TEXT("dismissed overlay has no active guide"),
		Overlay->IsGuideVisibleForTest());
	TestFalse(TEXT("dismissed overlay never blocks input"),
		Overlay->IsBlockingInputForTest());
	TestEqual(
		TEXT("dismissed overlay remains in Slate layout for late target geometry"),
		Overlay->GetVisibility(),
		ESlateVisibility::HitTestInvisible);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKGuideOverlayDimPaintTintTest,
	"GameXXK.Guide.Widget.DimPaintIsTranslucentBlack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKGuideOverlayDimPaintTintTest::RunTest(const FString& Parameters)
{
	UGameXXKGuideOverlayWidget* Overlay =
		NewObject<UGameXXKGuideOverlayWidget>();
	const TSharedRef<SWidget> OverlaySlate = Overlay->TakeWidget();
	FGameXXKGuideOutput Output;
	Output.bActive = true;
	Output.InputPolicy = EGameXXKGuideInputPolicy::Forced;
	Output.Text = FText::FromString(TEXT("气力值说明"));
	Output.TargetIds = {TEXT("Battle.Hud.PartyQi")};
	Overlay->PresentGuide(
		Output,
		TArray<FSlateRect>{FSlateRect(760.0f, 460.0f, 880.0f, 580.0f)},
		TOptional<FSlateRect>());
	UGameXXKGuideSpotlightWidget* Spotlight = Overlay->WidgetTree
		? Cast<UGameXXKGuideSpotlightWidget>(
			Overlay->WidgetTree->FindWidget(TEXT("GuideSpotlight")))
		: nullptr;
	TestNotNull(TEXT("dim-paint fixture owns its spotlight painter"), Spotlight);
	if (!Spotlight)
	{
		return false;
	}

	const TSharedRef<SWindow> Window = SNew(SWindow)
		.ClientSize(FVector2D(1280.0f, 720.0f))
		.CreateTitleBar(false);
	FSlateWindowElementList Elements(Window);
	FHittestGrid HittestGrid;
	const FGeometry Geometry = FGeometry::MakeRoot(
		FVector2D(1280.0f, 720.0f),
		FSlateLayoutTransform());
	const FSlateRect CullingRect(0.0f, 0.0f, 1280.0f, 720.0f);
	const FPaintArgs PaintArgs(
		&Window.Get(),
		HittestGrid,
		FVector2D::ZeroVector,
		0.0,
		0.0f);
	Spotlight->NativePaint(
		PaintArgs,
		Geometry,
		CullingRect,
		Elements,
		0,
		FWidgetStyle(),
		true);

	const FSlateDrawElementArray<FSlateBoxElement>& Boxes =
		Elements.GetUncachedDrawElements().Get<(uint8)EElementType::ET_Box>();
	bool bFoundTranslucentBlackDim = false;
	for (const FSlateBoxElement& Box : Boxes)
	{
		const FLinearColor Tint = Box.GetTint();
		if (FMath::IsNearlyZero(Tint.R)
			&& FMath::IsNearlyZero(Tint.G)
			&& FMath::IsNearlyZero(Tint.B)
			&& FMath::IsNearlyEqual(Tint.A, 0.56f, 0.001f))
		{
			bFoundTranslucentBlackDim = true;
			break;
		}
	}
	TestTrue(TEXT("forced guide paints a translucent black dim region"),
		bFoundTranslucentBlackDim);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattleGuideOverlayViewportRootTest,
	"GameXXK.Guide.Widget.BattleOverlayFillsViewportRoot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleGuideOverlayViewportRootTest::RunTest(const FString& Parameters)
{
	UGameXXKBattleBoardWidget* Board = NewObject<UGameXXKBattleBoardWidget>();
	Board->TakeWidget();
	UCanvasPanel* ViewportRoot = Board->GetBattleViewportRootForTest();
	UGameXXKGuideOverlayWidget* GuideOverlay = Board->WidgetTree
		? Cast<UGameXXKGuideOverlayWidget>(
			Board->WidgetTree->FindWidget(TEXT("BattleTutorial01GuideOverlay")))
		: nullptr;
	const UCanvasPanelSlot* GuideSlot = GuideOverlay
		? Cast<UCanvasPanelSlot>(GuideOverlay->Slot)
		: nullptr;
	TestNotNull(TEXT("battle guide overlay exists"), GuideOverlay);
	TestTrue(TEXT("battle guide overlay is a viewport-root sibling"),
		GuideOverlay && GuideOverlay->GetParent() == ViewportRoot);
	TestTrue(TEXT("battle guide overlay stretches across the viewport root"),
		GuideSlot
		&& GuideSlot->GetAnchors().Minimum == FVector2D::ZeroVector
		&& GuideSlot->GetAnchors().Maximum == FVector2D::UnitVector
		&& GuideSlot->GetOffsets() == FMargin(0.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKGuideBubbleAboveDimLayerTest,
	"GameXXK.Guide.Widget.BattleBubblePaintsAboveDimLayer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKGuideBubbleAboveDimLayerTest::RunTest(const FString& Parameters)
{
	UGameXXKGuideOverlayWidget* Overlay =
		NewObject<UGameXXKGuideOverlayWidget>();
	Overlay->TakeWidget();
	FGameXXKGuideOutput Output;
	Output.bActive = true;
	Output.InputPolicy = EGameXXKGuideInputPolicy::Forced;
	Output.Text = FText::FromString(TEXT("气力值说明"));
	Output.TargetIds = {TEXT("Battle.Hud.PartyQi")};
	Output.AllowedActionIds = {TEXT("Action.Guide.Continue")};
	Overlay->PresentGuide(
		Output,
		TArray<FSlateRect>{FSlateRect(760.0f, 460.0f, 880.0f, 580.0f)},
		TOptional<FSlateRect>());

	UWidget* Spotlight = Overlay->WidgetTree
		? Overlay->WidgetTree->FindWidget(TEXT("GuideSpotlight"))
		: nullptr;
	UGameXXKBattleGuideBubbleWidget* Bubble =
		Overlay->GetBattleBubbleForTest();
	const UCanvasPanelSlot* SpotlightSlot = Spotlight
		? Cast<UCanvasPanelSlot>(Spotlight->Slot)
		: nullptr;
	const UCanvasPanelSlot* BubbleSlot = Bubble
		? Cast<UCanvasPanelSlot>(Bubble->Slot)
		: nullptr;
	TestNotNull(TEXT("overlay owns an independent spotlight child"), Spotlight);
	TestTrue(TEXT("spotlight and paper bubble are sibling layers"),
		Spotlight && Bubble && Spotlight->GetParent() == Bubble->GetParent());
	TestTrue(TEXT("paper bubble z-order is above the spotlight"),
		SpotlightSlot && BubbleSlot
		&& BubbleSlot->GetZOrder() > SpotlightSlot->GetZOrder());
	TestEqual(TEXT("overlay retains the guide body text"),
		Overlay->GetGuideTextForTest(), Output.Text);
	TestTrue(TEXT("bubble retains the continue hint"),
		Bubble && Bubble->IsContinueHintVisible());
	return true;
}

#endif
