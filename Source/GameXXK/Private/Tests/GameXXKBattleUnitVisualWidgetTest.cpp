#include "Misc/AutomationTest.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UI/GameXXKBattleUnitVisualWidget.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGameXXKBattleAnimationClipDescriptor MakeUnitVisualClip(const TCHAR* AssetId)
	{
		FGameXXKBattleAnimationClipDescriptor Clip;
		Clip.AssetId = AssetId;
		Clip.TexturePath = FSoftObjectPath(TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture"));
		Clip.FrameCount = 60;
		Clip.Columns = 8;
		Clip.Rows = 8;
		Clip.SourceFramesPerSecond = 12.0f;
		Clip.PlaybackRate = 1.0f;
		return Clip;
	}

	UGameXXKBattleUnitVisualWidget* MakeAttachedUnitVisual(UCanvasPanel*& OutHost)
	{
		OutHost = NewObject<UCanvasPanel>();
		UGameXXKBattleUnitVisualWidget* const Widget = NewObject<UGameXXKBattleUnitVisualWidget>();
		OutHost->AddChildToCanvas(Widget);
		Widget->TakeWidget();
		return Widget;
	}

	int32 CountUnitImages(const UGameXXKBattleUnitVisualWidget* const Widget)
	{
		TArray<UWidget*> AllChildWidgets;
		if (Widget && Widget->WidgetTree)
		{
			Widget->WidgetTree->GetAllWidgets(AllChildWidgets);
		}
		return AllChildWidgets.FilterByPredicate([](const UWidget* Child)
		{
			return Child && Child->IsA<UImage>();
		}).Num();
	}

	void AssertAtlasCell(
		FAutomationTestBase& Test,
		UGameXXKBattleUnitVisualWidget* const Widget,
		const TCHAR* Label,
		const int32 ExpectedColumn,
		const int32 ExpectedRow)
	{
		UMaterialInstanceDynamic* const Material = Widget ? Widget->GetAtlasMaterialForTest() : nullptr;
		Test.TestNotNull(FString::Printf(TEXT("%s has an atlas MID"), Label), Material);
		Test.TestEqual(
			FString::Printf(TEXT("%s writes the expected FrameColumn"), Label),
			Material ? Material->K2_GetScalarParameterValue(TEXT("FrameColumn")) : -1.0f,
			static_cast<float>(ExpectedColumn));
		Test.TestEqual(
			FString::Printf(TEXT("%s writes the expected FrameRow"), Label),
			Material ? Material->K2_GetScalarParameterValue(TEXT("FrameRow")) : -1.0f,
			static_cast<float>(ExpectedRow));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattleUnitVisualWidgetIdentityTest,
	"GameXXK.UI.Battle.UnitVisualWidget.IdentityAndLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleUnitVisualWidgetIdentityTest::RunTest(const FString& Parameters)
{
	UCanvasPanel* Host = nullptr;
	UGameXXKBattleUnitVisualWidget* const Widget = MakeAttachedUnitVisual(Host);
	const FVector2D FormationAnchor(0.755f, 0.52f);
	const FVector2D CinematicAnchor(0.307292f, 0.5f);
	const FGameXXKBattleAnimationClipDescriptor IdleClip = MakeUnitVisualClip(TEXT("hero_idle"));
	const FGameXXKBattleAnimationClipDescriptor AttackClip = MakeUnitVisualClip(TEXT("hero_attack"));
	UTexture2D* const Atlas = NewObject<UTexture2D>(Widget);

	Widget->ConfigureUnit(TEXT("Player"), false, FormationAnchor, IdleClip);
	Widget->SetAtlas(Atlas);
	Widget->ShowFormationIdle();

	UGameXXKBattleUnitVisualWidget* const OriginalPointer = Widget;
	UPanelWidget* const OriginalParent = Widget->GetParent();
	UPanelSlot* const OriginalSlot = Widget->Slot;
	UImage* const OriginalImage = Widget->GetUnitImageForTest();
	UMaterialInstanceDynamic* const OriginalMaterial = Widget->GetAtlasMaterialForTest();
	const UCanvasPanelSlot* const FormationSlot = Cast<UCanvasPanelSlot>(OriginalSlot);
	TestNotNull(TEXT("unit visual creates its atlas image"), OriginalImage);
	TestEqual(TEXT("unit visual owns exactly one image"), CountUnitImages(Widget), 1);
	TestNotNull(TEXT("unit visual creates one atlas MID from the canonical UI material"), OriginalMaterial);
	TestEqual(TEXT("formation size is the canonical 410 square"), Widget->GetPresentedSize(), FVector2D(410.0f, 410.0f));
	TestEqual(TEXT("formation uses Z 10"), FormationSlot ? FormationSlot->GetZOrder() : INDEX_NONE, 10);
	TestEqual(TEXT("formation keeps its normalized anchor"),
		FormationSlot ? FormationSlot->GetAnchors().Minimum : FVector2D::ZeroVector,
		FormationAnchor);
	TestEqual(TEXT("formation center aligns around the authored unit"),
		FormationSlot ? FormationSlot->GetAlignment() : FVector2D::ZeroVector,
		FVector2D(0.5f, 0.5f));
	TestEqual(TEXT("targeting center converts the current anchor into 1920 by 1080 stage pixels"),
		Widget->GetStageCenter(),
		FVector2D(FormationAnchor.X * 1920.0f, FormationAnchor.Y * 1080.0f));
	TestTrue(TEXT("party art keeps its authored orientation without mirroring"),
		Widget->GetRenderTransform().Scale.X > 0.0f && Widget->GetRenderTransform().Scale.Y > 0.0f);

	Widget->ShowCinematic(AttackClip, CinematicAnchor);
	const UCanvasPanelSlot* const CinematicSlot = Cast<UCanvasPanelSlot>(Widget->Slot);
	TestEqual(TEXT("cinematic reuses the same widget object"), Widget, OriginalPointer);
	TestEqual(TEXT("cinematic retains the original parent"), Widget->GetParent(), OriginalParent);
	TestEqual(TEXT("cinematic retains the original canvas slot"), Widget->Slot.Get(), OriginalSlot);
	TestEqual(TEXT("cinematic retains the original image"), Widget->GetUnitImageForTest(), OriginalImage);
	TestEqual(TEXT("cinematic retains the original MID"), Widget->GetAtlasMaterialForTest(), OriginalMaterial);
	TestEqual(TEXT("cinematic size is exactly doubled"), Widget->GetPresentedSize(), FVector2D(820.0f, 820.0f));
	TestEqual(TEXT("cinematic uses Z 40"), CinematicSlot ? CinematicSlot->GetZOrder() : INDEX_NONE, 40);
	TestEqual(TEXT("cinematic uses its requested normalized anchor"),
		CinematicSlot ? CinematicSlot->GetAnchors().Minimum : FVector2D::ZeroVector,
		CinematicAnchor);
	TestEqual(TEXT("cinematic targeting center follows the current stage anchor"),
		Widget->GetStageCenter(),
		FVector2D(CinematicAnchor.X * 1920.0f, CinematicAnchor.Y * 1080.0f));
	TestTrue(TEXT("cinematic retains authored orientation"),
		Widget->GetRenderTransform().Scale.X > 0.0f && Widget->GetRenderTransform().Scale.Y > 0.0f);

	Widget->HideForCinematic();
	TestEqual(TEXT("temporary cinematic hiding preserves the parent"), Widget->GetParent(), OriginalParent);
	TestEqual(TEXT("temporary cinematic hiding preserves the slot"), Widget->Slot.Get(), OriginalSlot);
	Widget->RestoreFormation();
	TestEqual(TEXT("restoring formation preserves the parent"), Widget->GetParent(), OriginalParent);
	TestEqual(TEXT("restoring formation preserves the image"), Widget->GetUnitImageForTest(), OriginalImage);
	TestEqual(TEXT("restoring formation preserves the MID"), Widget->GetAtlasMaterialForTest(), OriginalMaterial);
	TestEqual(TEXT("formation to cinematic to restore still owns exactly one image"), CountUnitImages(Widget), 1);
	TestEqual(TEXT("restoring formation returns to the 410 square"), Widget->GetPresentedSize(), FVector2D(410.0f, 410.0f));
	TestEqual(TEXT("restoring formation returns to the original stage center"),
		Widget->GetStageCenter(),
		FVector2D(FormationAnchor.X * 1920.0f, FormationAnchor.Y * 1080.0f));

	UCanvasPanel* EnemyHost = nullptr;
	UGameXXKBattleUnitVisualWidget* const EnemyWidget = MakeAttachedUnitVisual(EnemyHost);
	EnemyWidget->ConfigureUnit(TEXT("Enemy.Ch1.Rooster"), true, FVector2D(0.245f, 0.52f), IdleClip);
	EnemyWidget->SetAtlas(NewObject<UTexture2D>(EnemyWidget));
	EnemyWidget->ShowFormationIdle();
	TestTrue(TEXT("enemy art also keeps its authored orientation without mirroring"),
		EnemyWidget->GetRenderTransform().Scale.X > 0.0f && EnemyWidget->GetRenderTransform().Scale.Y > 0.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattleUnitVisualWidgetClockTest,
	"GameXXK.UI.Battle.UnitVisualWidget.AbsoluteTimePlayback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleUnitVisualWidgetClockTest::RunTest(const FString& Parameters)
{
	UCanvasPanel* Host = nullptr;
	UGameXXKBattleUnitVisualWidget* const Widget = MakeAttachedUnitVisual(Host);
	const FGameXXKBattleAnimationClipDescriptor IdleClip = MakeUnitVisualClip(TEXT("hero_idle"));
	Widget->ConfigureUnit(TEXT("Player"), false, FVector2D(0.755f, 0.52f), IdleClip);
	Widget->SetAtlas(NewObject<UTexture2D>(Widget));
	Widget->ShowFormationIdle();

	Widget->AdvanceAtRealTime(100.0);
	TestEqual(TEXT("first absolute-time sample starts on frame zero"), Widget->GetCurrentFrameForTest(), 0);
	AssertAtlasCell(*this, Widget, TEXT("frame zero"), 0, 0);
	Widget->AdvanceAtRealTime(100.0 + 7.0 / 12.0);
	TestEqual(TEXT("absolute-time playback reaches frame seven with zero world delta"), Widget->GetCurrentFrameForTest(), 7);
	AssertAtlasCell(*this, Widget, TEXT("frame seven"), 7, 0);
	Widget->AdvanceAtRealTime(100.0 + 8.0 / 12.0 - 1.0e-6);
	TestEqual(TEXT("a real time sample just before a frame boundary remains on the prior frame"),
		Widget->GetCurrentFrameForTest(),
		7);
	Widget->AdvanceAtRealTime(100.0 + 8.0 / 12.0);
	TestEqual(TEXT("absolute-time playback crosses the atlas row at frame eight"), Widget->GetCurrentFrameForTest(), 8);
	AssertAtlasCell(*this, Widget, TEXT("frame eight"), 0, 1);
	Widget->AdvanceAtRealTime(100.0 + 59.0 / 12.0);
	TestEqual(TEXT("absolute-time playback reaches the final authored frame"), Widget->GetCurrentFrameForTest(), 59);
	AssertAtlasCell(*this, Widget, TEXT("frame fifty-nine"), 3, 7);
	Widget->AdvanceAtRealTime(105.0);
	TestEqual(TEXT("looping idle wraps frame sixty back to zero"), Widget->GetCurrentFrameForTest(), 0);
	AssertAtlasCell(*this, Widget, TEXT("wrapped frame zero"), 0, 0);

	const int32 WritesAtFrameZero = Widget->GetFrameParameterWriteCountForTest();
	Widget->AdvanceAtRealTime(105.0);
	TestEqual(TEXT("sampling the same frame does not rewrite material parameters"),
		Widget->GetFrameParameterWriteCountForTest(),
		WritesAtFrameZero);
	Widget->HideForCinematic();
	Widget->AdvanceAtRealTime(106.0);
	TestEqual(TEXT("hidden widgets never write frame parameters"),
		Widget->GetFrameParameterWriteCountForTest(),
		WritesAtFrameZero);

	Widget->ShowCinematic(MakeUnitVisualClip(TEXT("hero_attack")), FVector2D(0.307292f, 0.5f));
	Widget->AdvanceAtRealTime(200.0);
	Widget->AdvanceAtRealTime(205.0);
	TestEqual(TEXT("non-looping action clamps at its final frame"), Widget->GetCurrentFrameForTest(), 59);
	Widget->AdvanceAtRealTime(204.0);
	TestEqual(TEXT("a backward absolute clock restarts the active clip at frame zero"), Widget->GetCurrentFrameForTest(), 0);
	const int32 WritesBeforeInvalidClock = Widget->GetFrameParameterWriteCountForTest();
	Widget->AdvanceAtRealTime(std::numeric_limits<double>::quiet_NaN());
	Widget->AdvanceAtRealTime(std::numeric_limits<double>::infinity());
	TestEqual(TEXT("non-finite absolute time is ignored without material writes"),
		Widget->GetFrameParameterWriteCountForTest(),
		WritesBeforeInvalidClock);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattleUnitVisualWidgetLifecycleTest,
	"GameXXK.UI.Battle.UnitVisualWidget.AtlasAndTerminalLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleUnitVisualWidgetLifecycleTest::RunTest(const FString& Parameters)
{
	UCanvasPanel* Host = nullptr;
	UGameXXKBattleUnitVisualWidget* const Widget = MakeAttachedUnitVisual(Host);
	FGameXXKBattleAnimationClipDescriptor InvalidIdle;
	Widget->ConfigureUnit(TEXT("Player"), false, FVector2D(0.755f, 0.52f), InvalidIdle);
	UTexture2D* const InitialAtlas = NewObject<UTexture2D>(Widget);
	Widget->SetAtlas(InitialAtlas);
	Widget->ShowFormationIdle();
	TestEqual(TEXT("a missing idle preserves formation geometry"), Widget->GetPresentedSize(), FVector2D(410.0f, 410.0f));
	TestEqual(TEXT("a missing idle hides only the image"),
		Widget->GetUnitImageForTest() ? Widget->GetUnitImageForTest()->GetVisibility() : ESlateVisibility::Visible,
		ESlateVisibility::Hidden);

	const FGameXXKBattleAnimationClipDescriptor IdleClip = MakeUnitVisualClip(TEXT("hero_idle"));
	Widget->ConfigureUnit(TEXT("Player"), false, FVector2D(0.755f, 0.52f), IdleClip);
	Widget->ShowFormationIdle();
	TestEqual(TEXT("a valid clip and atlas reveal the image"),
		Widget->GetUnitImageForTest() ? Widget->GetUnitImageForTest()->GetVisibility() : ESlateVisibility::Hidden,
		ESlateVisibility::SelfHitTestInvisible);
	UMaterialInstanceDynamic* const OriginalMaterial = Widget->GetAtlasMaterialForTest();
	TestTrue(TEXT("the MID owns the supplied atlas before it is cleared"),
		OriginalMaterial && OriginalMaterial->K2_GetTextureParameterValue(TEXT("AtlasTexture")) == InitialAtlas);
	Widget->SetAtlas(nullptr);
	TestNull(TEXT("clearing the atlas releases the strong texture reference"), Widget->GetAtlasForTest());
	TestEqual(TEXT("clearing the atlas keeps the existing MID"), Widget->GetAtlasMaterialForTest(), OriginalMaterial);
	TestTrue(TEXT("clearing the atlas removes the old texture override from the MID"),
		OriginalMaterial && OriginalMaterial->K2_GetTextureParameterValue(TEXT("AtlasTexture")) != InitialAtlas);
	TestEqual(TEXT("clearing the atlas hides the image"),
		Widget->GetUnitImageForTest() ? Widget->GetUnitImageForTest()->GetVisibility() : ESlateVisibility::Visible,
		ESlateVisibility::Hidden);
	const int32 WritesBeforeMissingAtlasAdvance = Widget->GetFrameParameterWriteCountForTest();
	Widget->AdvanceAtRealTime(0.0);
	TestEqual(TEXT("a missing atlas prevents frame parameter writes"),
		Widget->GetFrameParameterWriteCountForTest(),
		WritesBeforeMissingAtlasAdvance);

	UTexture2D* const TerminalAtlas = NewObject<UTexture2D>(Widget);
	Widget->SetAtlas(TerminalAtlas);
	Widget->ShowFormationIdle();
	Widget->AdvanceAtRealTime(0.0);
	TestEqual(TEXT("rebinding an atlas restarts frame writes from frame zero"),
		Widget->GetFrameParameterWriteCountForTest(),
		WritesBeforeMissingAtlasAdvance + 1);
	AssertAtlasCell(*this, Widget, TEXT("rebound frame zero"), 0, 0);
	const int32 WritesBeforeRemoval = Widget->GetFrameParameterWriteCountForTest();
	UPanelSlot* const OriginalSlot = Widget->Slot;
	Widget->RemoveAfterDeath();
	TestNull(TEXT("terminal death is the only lifecycle transition that detaches the widget"), Widget->GetParent());
	TestTrue(TEXT("terminal death marks the visual removed"), Widget->IsRemovedForTest());
	TestTrue(TEXT("terminal death releases the atlas override retained by the MID"),
		OriginalMaterial && OriginalMaterial->K2_GetTextureParameterValue(TEXT("AtlasTexture")) != TerminalAtlas);
	Widget->RestoreFormation();
	TestNull(TEXT("a terminally removed visual cannot be reattached by restore"), Widget->GetParent());
	TestTrue(TEXT("terminal removal discards the old canvas slot"), Widget->Slot != OriginalSlot);
	Widget->AdvanceAtRealTime(5.0);
	TestEqual(TEXT("terminally removed widgets never write frame parameters"),
		Widget->GetFrameParameterWriteCountForTest(),
		WritesBeforeRemoval);

	return true;
}

#endif
