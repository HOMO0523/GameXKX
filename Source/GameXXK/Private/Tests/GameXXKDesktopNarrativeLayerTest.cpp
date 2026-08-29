#include "Misc/AutomationTest.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/GameInstance.h"
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKDesktopNarrativeLayerWidget.h"
#include "UI/GameXXKDesktopNarrativeStagePresenterWidget.h"
#include "UI/GameXXKDesktopTrainingWorkbenchWidget.h"
#include "UI/GameXXKDialogueHistoryWidget.h"
#include "UI/GameXXKDialoguePanelWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	bool RectInsideHost(const FVector4& Rect, const FVector2D& HostSize)
	{
		return Rect.X >= 0.0f
			&& Rect.Y >= 0.0f
			&& Rect.Z >= 0.0f
			&& Rect.W >= 0.0f
			&& Rect.X + Rect.Z <= HostSize.X + KINDA_SMALL_NUMBER
			&& Rect.Y + Rect.W <= HostSize.Y + KINDA_SMALL_NUMBER;
	}

	bool RectsOverlap(const FVector4& A, const FVector4& B)
	{
		return A.X < B.X + B.Z
			&& A.X + A.Z > B.X
			&& A.Y < B.Y + B.W
			&& A.Y + A.W > B.Y;
	}

	FVector4 GetCanvasRect(const UWidget* Widget)
	{
		const UCanvasPanelSlot* const Slot = Widget
			? Cast<UCanvasPanelSlot>(Widget->Slot)
			: nullptr;
		return Slot
			? FVector4(Slot->GetPosition().X, Slot->GetPosition().Y,
				Slot->GetSize().X, Slot->GetSize().Y)
			: FVector4(-1.0f, -1.0f, -1.0f, -1.0f);
	}

	bool RectNearlyEquals(const FVector4& A, const FVector4& B, const float Tolerance = 0.01f)
	{
		return FMath::IsNearlyEqual(A.X, B.X, Tolerance)
			&& FMath::IsNearlyEqual(A.Y, B.Y, Tolerance)
			&& FMath::IsNearlyEqual(A.Z, B.Z, Tolerance)
			&& FMath::IsNearlyEqual(A.W, B.W, Tolerance);
	}

	bool IsDescendantOf(const UWidget* Child, const UWidget* Ancestor)
	{
		for (const UWidget* Current = Child; Current; Current = Current->GetParent())
		{
			if (Current == Ancestor)
			{
				return true;
			}
		}
		return false;
	}

	UGameXXKDesktopTrainingWorkbenchWidget* MakeWorkbench(
		UGameXXKMVPSubsystem*& OutSubsystem)
	{
		UGameInstance* const GameInstance = NewObject<UGameInstance>();
		OutSubsystem = NewObject<UGameXXKMVPSubsystem>(GameInstance);
		OutSubsystem->GetMutableRuntimeState() = UGameXXKMVPRules::CreateNewGame();
		UGameXXKDesktopTrainingWorkbenchWidget* const Widget =
			NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
		Widget->SetMVPSubsystem(OutSubsystem);
		Widget->ConstructForTest();
		Widget->OpenWorkbench();
		Widget->InitializeDesktopPresentationHostSize(FVector2D(1672.0f, 941.0f));
		return Widget;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopNarrativeStandaloneLayerTest,
	"GameXXK.DesktopNarrative.Layer.Standalone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopNarrativeStandaloneLayerTest::RunTest(const FString& Parameters)
{
	UGameXXKDesktopNarrativeLayerWidget* const Layer =
		NewObject<UGameXXKDesktopNarrativeLayerWidget>();
	Layer->ConstructForTest();
	Layer->TakeWidget();
	TestNotNull(TEXT("narrative owns its named transparent root"),
		Layer->GetNamedWidgetForTest(TEXT("DesktopNarrativeRoot")));
	TestNotNull(TEXT("narrative owns its named acting stage"),
		Layer->GetNamedWidgetForTest(TEXT("DesktopNarrativeStageCanvas")));
	TestNotNull(TEXT("narrative owns its bottom dialogue safe-area host"),
		Layer->GetNamedWidgetForTest(TEXT("DesktopNarrativeDialogueHost")));
	TestNotNull(TEXT("narrative owns its top-right pause button"),
		Layer->GetNamedWidgetForTest(TEXT("DesktopNarrativePauseButton")));
	UGameXXKDialoguePanelWidget* const DialoguePanel = Layer->GetDialoguePanel();
	UGameXXKDialogueHistoryWidget* const DialogueHistory = Layer->GetDialogueHistory();
	if (!TestNotNull(TEXT("layer owns typed dialogue presenter after TakeWidget"), DialoguePanel)
		|| !TestNotNull(TEXT("layer owns typed history presenter after TakeWidget"), DialogueHistory))
	{
		return false;
	}
	UWidget* const NarrativeRoot = Layer->GetNamedWidgetForTest(TEXT("DesktopNarrativeRoot"));
	TestTrue(TEXT("dialogue presenter is an actual narrative-root descendant"),
		IsDescendantOf(DialoguePanel, NarrativeRoot));
	TestTrue(TEXT("history presenter is an actual narrative-root descendant"),
		IsDescendantOf(DialogueHistory, NarrativeRoot));
	TestEqual(TEXT("dialogue presenter starts hidden"),
		DialoguePanel->GetVisibility(), ESlateVisibility::Collapsed);
	TestEqual(TEXT("dialogue presenter starts cleared"),
		DialoguePanel->GetBodyTextForTest(), FText::GetEmpty());
	TestEqual(TEXT("history presenter starts hidden"),
		DialogueHistory->GetVisibility(), ESlateVisibility::Collapsed);
	TestEqual(TEXT("history presenter starts cleared"),
		DialogueHistory->GetHistoryCountForTest(), 0);
	UWidget* const OriginalRoot = Layer->GetNamedWidgetForTest(TEXT("DesktopNarrativeRoot"));
	UWidget* const OriginalPause = Layer->GetNamedWidgetForTest(TEXT("DesktopNarrativePauseButton"));
	Layer->ConstructForTest();
	TestEqual(TEXT("repeated programmatic construction preserves the same named root"),
		Layer->GetNamedWidgetForTest(TEXT("DesktopNarrativeRoot")), OriginalRoot);
	TestEqual(TEXT("repeated programmatic construction cannot duplicate pause bindings"),
		Layer->GetNamedWidgetForTest(TEXT("DesktopNarrativePauseButton")), OriginalPause);

	for (const FName SlotName : {
		FName(TEXT("Left")), FName(TEXT("Center")), FName(TEXT("Right")),
		FName(TEXT("Prop")), FName(TEXT("Vfx"))})
	{
		TestNotNull(*FString::Printf(TEXT("semantic slot %s resolves"), *SlotName.ToString()),
			Layer->FindNarrativeSlot(SlotName));
	}
	TestNull(TEXT("unknown semantic slot does not alias a real container"),
		Layer->FindNarrativeSlot(TEXT("Missing")));
	for (const EGameXXKDesktopNarrativeSlot Slot : {
		EGameXXKDesktopNarrativeSlot::Left,
		EGameXXKDesktopNarrativeSlot::Center,
		EGameXXKDesktopNarrativeSlot::Right,
		EGameXXKDesktopNarrativeSlot::Prop,
		EGameXXKDesktopNarrativeSlot::Vfx})
	{
		UGameXXKDesktopNarrativeStagePresenterWidget* const Presenter =
			Layer->GetStagePresenter(Slot);
		TestNotNull(TEXT("semantic slot owns a production stage presenter"), Presenter);
		TestTrue(TEXT("semantic stage presenter owns ready child content"),
			Presenter && Presenter->IsPresentationReady());
	}
	UGameXXKDesktopNarrativeStagePresenterWidget* const LeftStage =
		Layer->GetStagePresenter(EGameXXKDesktopNarrativeSlot::Left);
	Layer->ApplyStageRolePresentation(
		TEXT("Hero"),
		TEXT("Character.Hero"),
		EGameXXKDesktopNarrativeSlot::Left,
		EGameXXKDesktopNarrativeFacing::Right,
		EGameXXKDesktopNarrativeRoleActionState::Idle,
		NAME_None,
		true);
	TestEqual(TEXT("visible role occupies its real target presenter"),
		LeftStage ? LeftStage->GetPresentedRoleId() : NAME_None,
		FName(TEXT("Hero")));
	Layer->ApplyStageRolePresentation(
		TEXT("Guide"),
		TEXT("Character.Guide"),
		EGameXXKDesktopNarrativeSlot::Left,
		EGameXXKDesktopNarrativeFacing::Right,
		EGameXXKDesktopNarrativeRoleActionState::Idle,
		NAME_None,
		false);
	TestEqual(TEXT("invisible role update cannot clear an unrelated target occupant"),
		LeftStage ? LeftStage->GetPresentedRoleId() : NAME_None,
		FName(TEXT("Hero")));
	Layer->ApplyStageRolePresentation(
		TEXT("Guide"),
		TEXT("Character.Guide"),
		EGameXXKDesktopNarrativeSlot::Left,
		EGameXXKDesktopNarrativeFacing::Right,
		EGameXXKDesktopNarrativeRoleActionState::Idle,
		NAME_None,
		true);
	TestEqual(TEXT("visible same-slot role explicitly replaces the occupant"),
		LeftStage ? LeftStage->GetPresentedRoleId() : NAME_None,
		FName(TEXT("Guide")));
	Layer->ResetStagePresentation();

	for (const FVector2D HostSize : {
		FVector2D(1280.0f, 720.0f),
		FVector2D(1672.0f, 941.0f),
		FVector2D(1920.0f, 1080.0f)})
	{
		Layer->ApplyHostSize(HostSize);
		const FGameXXKDesktopNarrativeLayout Layout = Layer->GetResolvedLayoutForTest();
		TestTrue(TEXT("actual stage slot matches pure geometry"),
			RectNearlyEquals(GetCanvasRect(
				Layer->GetNamedWidgetForTest(TEXT("DesktopNarrativeStageCanvas"))),
				Layout.StageRect));
		TestTrue(TEXT("actual dialogue SafeZone slot matches pure geometry"),
			RectNearlyEquals(GetCanvasRect(
				Layer->GetNamedWidgetForTest(TEXT("DesktopNarrativeDialogueSafeArea"))),
				Layout.DialogueHostRect));
		TestTrue(TEXT("actual pause SafeZone slot matches pure geometry"),
			RectNearlyEquals(GetCanvasRect(
				Layer->GetNamedWidgetForTest(TEXT("DesktopNarrativePauseSafeArea"))),
				Layout.PauseRect));
		TestTrue(TEXT("actual history presenter host matches pure geometry"),
			RectNearlyEquals(GetCanvasRect(
				Layer->GetNamedWidgetForTest(TEXT("DesktopNarrativeHistoryPresenterHost"))),
				Layout.HistoryRect));
		TestTrue(TEXT("actual full-host dialogue presenter host matches Slate host"),
			RectNearlyEquals(GetCanvasRect(
				Layer->GetNamedWidgetForTest(TEXT("DesktopNarrativeDialoguePresenterHost"))),
				FVector4(0.0f, 0.0f, HostSize.X, HostSize.Y)));
		const FVector4 FallbackRect = GetCanvasRect(
			Layer->GetNamedWidgetForTest(TEXT("DesktopNarrativePaperFallback")));
		TestTrue(TEXT("actual paper fallback begins at dialogue-local origin"),
			RectNearlyEquals(FallbackRect,
				FVector4(0.0f, 0.0f, Layout.DialogueHostRect.Z, Layout.DialogueHostRect.W)));
		TestTrue(TEXT("dialogue paper fallback lives inside DialogueHost"),
			Layer->GetNamedWidgetForTest(TEXT("DesktopNarrativePaperFallback"))->GetParent() ==
			Layer->GetNamedWidgetForTest(TEXT("DesktopNarrativeDialogueHost")));
		TestTrue(TEXT("actual paper fallback remains inside its host"),
			FallbackRect.X >= 0.0f && FallbackRect.Y >= 0.0f
				&& FallbackRect.X + FallbackRect.Z <= Layout.DialogueHostRect.Z + 0.01f
				&& FallbackRect.Y + FallbackRect.W <= Layout.DialogueHostRect.W + 0.01f);
		TestTrue(TEXT("separate stage backing tracks stage-local size"),
			RectNearlyEquals(GetCanvasRect(
				Layer->GetNamedWidgetForTest(TEXT("DesktopNarrativeStageBacking"))),
				FVector4(0.0f, 0.0f, Layout.StageRect.Z, Layout.StageRect.W)));
		TestTrue(TEXT("stage stays inside every host"), RectInsideHost(Layout.StageRect, HostSize));
		TestTrue(TEXT("dialogue stays inside every host"), RectInsideHost(Layout.DialogueHostRect, HostSize));
		TestTrue(TEXT("pause stays inside every host"), RectInsideHost(Layout.PauseRect, HostSize));
		TestTrue(TEXT("stage remains horizontally centered"),
			FMath::IsNearlyEqual(Layout.StageRect.X + Layout.StageRect.Z * 0.5f, HostSize.X * 0.5f, 0.5f));
		TestTrue(TEXT("dialogue remains bottom anchored"),
			FMath::IsNearlyEqual(
				HostSize.Y - (Layout.DialogueHostRect.Y + Layout.DialogueHostRect.W),
				HostSize.Y * (60.0f / 1080.0f), 0.5f));
		TestTrue(TEXT("pause remains top-right anchored"),
			FMath::IsNearlyEqual(
				HostSize.X - (Layout.PauseRect.X + Layout.PauseRect.Z),
				HostSize.X * (40.0f / 1920.0f), 0.5f));
		TestFalse(TEXT("dialogue and pause never overlap"),
			RectsOverlap(Layout.DialogueHostRect, Layout.PauseRect));
	}

	Layer->ApplyHostSize(FVector2D(1920.0f, 1080.0f));
	const FGameXXKDesktopNarrativeLayout Baseline = Layer->GetResolvedLayoutForTest();
	TestEqual(TEXT("baseline stage rect is exact"),
		Baseline.StageRect, FVector4(160.0f, 80.0f, 1600.0f, 620.0f));
	TestEqual(TEXT("baseline dialogue rect is exact"),
		Baseline.DialogueHostRect, FVector4(192.0f, 735.0f, 1536.0f, 285.0f));
	TestEqual(TEXT("baseline pause rect is exact"),
		Baseline.PauseRect, FVector4(1770.0f, 40.0f, 110.0f, 48.0f));
	TestEqual(TEXT("baseline history rect is exact"),
		Baseline.HistoryRect, FVector4(120.0f, 120.0f, 720.0f, 760.0f));
	const TMap<FName, FVector4> ExpectedBaselineSlots = {
		{TEXT("Left"), FVector4(240.0f, 180.0f, 440.0f, 440.0f)},
		{TEXT("Center"), FVector4(740.0f, 150.0f, 440.0f, 480.0f)},
		{TEXT("Right"), FVector4(1240.0f, 180.0f, 440.0f, 440.0f)},
		{TEXT("Prop"), FVector4(800.0f, 480.0f, 320.0f, 180.0f)},
		{TEXT("Vfx"), FVector4(160.0f, 80.0f, 1600.0f, 620.0f)}};
	for (const TPair<FName, FVector4>& Expected : ExpectedBaselineSlots)
	{
		TestEqual(*FString::Printf(TEXT("baseline %s slot rect is exact"), *Expected.Key.ToString()),
			Baseline.SlotRects.FindRef(Expected.Key), Expected.Value);
		const FVector4 ActualLocalRect = GetCanvasRect(Layer->FindNarrativeSlot(Expected.Key));
		TestTrue(*FString::Printf(TEXT("actual %s Canvas slot matches baseline geometry"), *Expected.Key.ToString()),
			RectNearlyEquals(ActualLocalRect,
				FVector4(Expected.Value.X - Baseline.StageRect.X,
					Expected.Value.Y - Baseline.StageRect.Y,
					Expected.Value.Z,
					Expected.Value.W)));
	}

	int32 PauseCount = 0;
	Layer->SetPauseRequested(FGameXXKDesktopNarrativePauseRequestedDelegate::CreateLambda(
		[&PauseCount]() { ++PauseCount; }));
	if (UButton* const PauseButton = Layer->GetPauseButtonForTest())
	{
		PauseButton->OnClicked.Broadcast();
	}
	TestEqual(TEXT("the actual pause button emits the single-cast delegate once"), PauseCount, 1);

	Layer->HideLayer();
	Layer->HideLayer();
	TestFalse(TEXT("duplicate hide is idempotent"), Layer->IsLayerVisible());
	Layer->ShowLayer();
	Layer->ShowLayer();
	TestTrue(TEXT("duplicate show is idempotent"), Layer->IsLayerVisible());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopNarrativeDpiGeometryTest,
	"GameXXK.DesktopNarrative.Layer.DpiSharedGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopNarrativeDpiGeometryTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = nullptr;
	UGameXXKDesktopTrainingWorkbenchWidget* const Widget = MakeWorkbench(Subsystem);
	const FVector2D PhysicalHost(1600.0f, 900.0f);
	constexpr float DpiScale = 1.25f;
	const FVector2D SlateHost(1280.0f, 720.0f);
	Widget->InitializeDesktopNarrativeHostForTest(PhysicalHost, DpiScale);
	if (!TestTrue(TEXT("DPI fixture enters narrative successfully"),
		Widget->EnterNarrativePresentationForTest()))
	{
		return false;
	}
	UGameXXKDesktopNarrativeLayerWidget* const Layer = Widget->GetNarrativeLayerForTest();
	if (!TestNotNull(TEXT("DPI fixture owns narrative layer"), Layer))
	{
		return false;
	}
	TestTrue(TEXT("narrative parent slot uses exact Slate host units"),
		RectNearlyEquals(GetCanvasRect(Layer), FVector4(0.0f, 0.0f, SlateHost.X, SlateHost.Y)));
	const FGameXXKDesktopNarrativeLayout VisualLayout = Layer->GetResolvedLayoutForTest();
	TestEqual(TEXT("visual geometry resolves against Slate host size"), VisualLayout.HostSize, SlateHost);
	for (const FVector4 VisualRect : {
		VisualLayout.StageRect, VisualLayout.DialogueHostRect, VisualLayout.PauseRect})
	{
		TestTrue(TEXT("every visual rect fits the DPI-adjusted parent"),
			RectInsideHost(VisualRect, SlateHost));
	}
	const FGameXXKDesktopNarrativeLayout PhysicalLayout =
		ResolveGameXXKDesktopNarrativePhysicalLayout(PhysicalHost);
	const FGameXXKDesktopNativeSurfaceState NativeState =
		Widget->GetDesktopNativeSurfaceStateForTest();
	TestTrue(TEXT("DPI native hit state records narrative ownership"),
		NativeState.bNarrativeLayerActive);
	TestEqual(TEXT("DPI native hit state exposes exactly dialogue and pause"),
		NativeState.InteractiveRects.Num(), 2);
	if (!TestTrue(TEXT("DPI native hit state has required regions before indexing"),
		NativeState.InteractiveRects.Num() >= 2))
	{
		return false;
	}
	TestEqual(TEXT("native hit state keeps exact physical dialogue rect"),
		NativeState.InteractiveRects[0], PhysicalLayout.DialogueHostRect);
	TestEqual(TEXT("native hit state keeps exact physical pause rect"),
		NativeState.InteractiveRects[1], PhysicalLayout.PauseRect);
	TestTrue(TEXT("dialogue visual-to-physical conversion has no DPI divergence"),
		RectNearlyEquals(
			FVector4(VisualLayout.DialogueHostRect.X * DpiScale,
				VisualLayout.DialogueHostRect.Y * DpiScale,
				VisualLayout.DialogueHostRect.Z * DpiScale,
				VisualLayout.DialogueHostRect.W * DpiScale),
			PhysicalLayout.DialogueHostRect));
	TestTrue(TEXT("pause visual-to-physical conversion has no DPI divergence"),
		RectNearlyEquals(
			FVector4(VisualLayout.PauseRect.X * DpiScale,
				VisualLayout.PauseRect.Y * DpiScale,
				VisualLayout.PauseRect.Z * DpiScale,
				VisualLayout.PauseRect.W * DpiScale),
			PhysicalLayout.PauseRect));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopNarrativeWorkbenchTransitionTest,
	"GameXXK.DesktopNarrative.Layer.WorkbenchTransition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopNarrativeWorkbenchTransitionTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = nullptr;
	UGameXXKDesktopTrainingWorkbenchWidget* const Widget = MakeWorkbench(Subsystem);
	const FName CarryItemId(TEXT("Item.Test.DesktopNarrative.Carry"));
	Subsystem->GetMutableRuntimeState().Inventory.Add(CarryItemId, 1);
	TestTrue(TEXT("fixture normalizes a dedicated real carry item"),
		Subsystem->NormalizeDesktopInventoryState());
	Widget->HandleActionClicked(19);
	Widget->TickForTest(0.0f);
	Widget->HandleActionClicked(651);
	Widget->TickForTest(0.0f);
	const FVector2D NonDefaultAnchor(0.23f, 0.61f);
	Widget->SetDesktopWindowPositionNormalizedForTest(NonDefaultAnchor);
	Widget->SetExpandUpwardForTest(true);
	Widget->InitializeDesktopPresentationHostSize(FVector2D(1672.0f, 941.0f));
	const FVector2D AnchorBefore = Widget->GetDesktopWindowPositionNormalizedForTest();
	const FVector2D HudTopLeftBefore = Widget->GetDesktopWindowTopLeftForHost();
	const FVector2D HudSizeBefore = Widget->GetDesktopWindowSizeForHost();
	const int32 HudScaleBefore = Widget->GetHudScalePercentForTest();
	TestTrue(TEXT("fixture begins with the visible workbench child layer"),
		Widget->IsWorkbenchLayerVisibleForTest());
	TestFalse(TEXT("fixture captures ordinary folded bounds before expansion"),
		Widget->IsBackpackExpandedForTest());
	TestTrue(TEXT("fixture expands the workbench"), Widget->OpenBackpack());
	TestTrue(TEXT("fixture opens the real StoryTasks drawer"),
		Widget->ToggleStoryTaskDrawerForTest());
	Widget->HandleActionClicked(3);
	Widget->TickForTest(0.0f);
	TestTrue(TEXT("fixture keeps Tools active with StoryTasks"),
		Widget->IsToolsPanelActiveForTest() && Widget->IsStoryTaskDrawerOpenForTest());
	const int32 ToolSource = Widget->FindFirstBackpackEquipmentSlotForTest();
	if (!TestTrue(TEXT("fixture picks a real equipment entry for tool reservation"),
		ToolSource != INDEX_NONE && Widget->PickUpBackpackSlotForTest(ToolSource)))
	{
		return false;
	}
	Widget->HandleActionClicked(300);
	TestEqual(TEXT("fixture owns one real reserved tool entry"),
		Widget->GetOccupiedToolSlotCountForTest(), 1);
	const int32 CarrySource = Widget->FindBackpackItemSlotForTest(CarryItemId);
	if (!TestTrue(TEXT("fixture begins a real carry preview"),
		CarrySource != INDEX_NONE && Widget->PickUpBackpackSlotForTest(CarrySource)))
	{
		return false;
	}
	Widget->SetNarrativeTransitionModalsForTest(true, true, true);
	TestTrue(TEXT("fixture has settings open"), Widget->IsSettingsPanelOpenForTest());
	TestTrue(TEXT("fixture has difficulty dropdown open"),
		Widget->IsTrainingDifficultyDropdownOpenForTest());
	TestTrue(TEXT("fixture has exit confirmation open"),
		Widget->IsExitConfirmationOpenForTest());
	TestNotNull(TEXT("fixture builds the real settings modal"),
		Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("DesktopHudSettingsPanel")) : nullptr);
	TestNotNull(TEXT("fixture builds the real exit confirmation modal"),
		Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("ExitGameConfirmation")) : nullptr);
	TestTrue(TEXT("modal staging preserves the real carry preview until narrative entry"),
		Widget->IsCarryingItemForTest());
	TestEqual(TEXT("modal staging preserves the real tool reservation until narrative entry"),
		Widget->GetOccupiedToolSlotCountForTest(), 1);
	Widget->SetExpandUpwardForTest(true);
	const bool bExpandUpwardBefore = Widget->IsExpandUpwardForTest();
	TestTrue(TEXT("fixture enters narrative with nondefault upward expansion"),
		bExpandUpwardBefore);

	TestTrue(TEXT("enter narrative succeeds"), Widget->EnterNarrativePresentationForTest());
	TestEqual(TEXT("surface switches to narrative fullscreen"),
		Widget->GetOverlaySurfaceForTest(), EGameXXKDesktopOverlaySurface::NarrativeFullscreen);
	TestTrue(TEXT("narrative layer becomes active"), Widget->IsNarrativeLayerActiveForTest());
	TestFalse(TEXT("only the workbench child root is hidden"), Widget->IsWorkbenchLayerVisibleForTest());
	TestNotEqual(TEXT("outer workbench widget remains present"),
		Widget->GetVisibility(), ESlateVisibility::Collapsed);
	TestTrue(TEXT("narrative locks desktop Tab/story actions"), Widget->IsNarrativeTabLockedForTest());
	TestEqual(TEXT("narrative closes every left drawer"),
		Widget->GetLeftPanelForTest(), EGameXXKDesktopTrainingLeftPanel::None);
	TestFalse(TEXT("narrative folds backpack content"), Widget->IsBackpackExpandedForTest());
	TestFalse(TEXT("narrative rolls back the real carry preview"), Widget->IsCarryingItemForTest());
	TestEqual(TEXT("narrative returns every reserved tool entry"),
		Widget->GetOccupiedToolSlotCountForTest(), 0);
	TestFalse(TEXT("narrative closes settings"), Widget->IsSettingsPanelOpenForTest());
	TestFalse(TEXT("narrative closes difficulty dropdown"),
		Widget->IsTrainingDifficultyDropdownOpenForTest());
	TestFalse(TEXT("narrative closes exit modal"), Widget->IsExitConfirmationOpenForTest());
	TestEqual(TEXT("fullscreen native host begins at monitor work-area origin"),
		Widget->GetDesktopWindowTopLeftForHost(), FVector2D::ZeroVector);
	TestEqual(TEXT("fullscreen native host occupies the selected work area"),
		Widget->GetDesktopWindowSizeForHost(), FVector2D(1672.0f, 941.0f));
	TestEqual(TEXT("enter never overwrites the persisted desktop anchor"),
		Widget->GetDesktopWindowPositionNormalizedForTest(), AnchorBefore);
	TestEqual(TEXT("enter preserves HUD scale preference"),
		Widget->GetHudScalePercentForTest(), HudScaleBefore);
	TestEqual(TEXT("enter preserves upward expansion preference"),
		Widget->IsExpandUpwardForTest(), bExpandUpwardBefore);

	Widget->HandleActionClicked(60);
	TestFalse(TEXT("Tab action is a no-op while narrative is active"),
		Widget->IsBackpackExpandedForTest());
	TestFalse(TEXT("Story action is a no-op while narrative is active"),
		Widget->TriggerStoryTaskButtonForTest());
	TestFalse(TEXT("Story drawer remains closed under narrative lock"),
		Widget->IsStoryTaskDrawerOpenForTest());
	TestTrue(TEXT("double enter is idempotent"), Widget->EnterNarrativePresentationForTest());

	TestTrue(TEXT("exit narrative succeeds"), Widget->ExitNarrativePresentationToFoldedDesktopForTest());
	TestEqual(TEXT("exit restores the workbench surface"),
		Widget->GetOverlaySurfaceForTest(), EGameXXKDesktopOverlaySurface::Workbench);
	TestFalse(TEXT("exit hides narrative"), Widget->IsNarrativeLayerActiveForTest());
	TestTrue(TEXT("exit restores the workbench child layer"), Widget->IsWorkbenchLayerVisibleForTest());
	TestFalse(TEXT("exit unlocks Tab/story actions"), Widget->IsNarrativeTabLockedForTest());
	TestFalse(TEXT("exit always returns folded"), Widget->IsBackpackExpandedForTest());
	TestFalse(TEXT("exit returns the ordinary non-minimized idle strip"), Widget->IsIdleStripFoldedForTest());
	TestEqual(TEXT("exit restores no left panel"),
		Widget->GetLeftPanelForTest(), EGameXXKDesktopTrainingLeftPanel::None);
	TestEqual(TEXT("exit restores the pre-narrative HUD top-left"),
		Widget->GetDesktopWindowTopLeftForHost(), HudTopLeftBefore);
	TestEqual(TEXT("exit restores the pre-narrative HUD size"),
		Widget->GetDesktopWindowSizeForHost(), HudSizeBefore);
	TestEqual(TEXT("exit preserves the persisted desktop anchor"),
		Widget->GetDesktopWindowPositionNormalizedForTest(), AnchorBefore);
	TestEqual(TEXT("expanded-to-narrative-to-exit keeps HUD scale"),
		Widget->GetHudScalePercentForTest(), HudScaleBefore);
	TestEqual(TEXT("expanded-to-narrative-to-exit keeps expansion preference"),
		Widget->IsExpandUpwardForTest(), bExpandUpwardBefore);
	TestTrue(TEXT("stale-state fixture expands after a clean exit"), Widget->OpenBackpack());
	Widget->HandleActionClicked(0);
	TestTrue(TEXT("stale-state fixture opens a left panel"),
		Widget->GetLeftPanelForTest() != EGameXXKDesktopTrainingLeftPanel::None);
	TestTrue(TEXT("second exit unconditionally normalizes stale Workbench state"),
		Widget->ExitNarrativePresentationToFoldedDesktopForTest());
	TestFalse(TEXT("second exit folds stale expanded Workbench"), Widget->IsBackpackExpandedForTest());
	TestEqual(TEXT("second exit clears stale left panel"),
		Widget->GetLeftPanelForTest(), EGameXXKDesktopTrainingLeftPanel::None);
	TestFalse(TEXT("second exit keeps narrative lock clear"), Widget->IsNarrativeTabLockedForTest());
	TestTrue(TEXT("second exit keeps Workbench child visible"), Widget->IsWorkbenchLayerVisibleForTest());
	TestEqual(TEXT("second exit restores canonical HUD top-left"),
		Widget->GetDesktopWindowTopLeftForHost(), HudTopLeftBefore);
	TestEqual(TEXT("second exit restores canonical HUD size"),
		Widget->GetDesktopWindowSizeForHost(), HudSizeBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopNarrativeEnterPreflightTest,
	"GameXXK.DesktopNarrative.Layer.EnterPreflightAtomicity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopNarrativeEnterPreflightTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = nullptr;
	UGameXXKDesktopTrainingWorkbenchWidget* const Widget = MakeWorkbench(Subsystem);
	Widget->TakeWidget();
	if (!TestNotNull(TEXT("preflight fixture owns narrative layer"), Widget->GetNarrativeLayerForTest())
		|| !TestTrue(TEXT("real narrative layer reports presenter readiness after TakeWidget"),
			Widget->GetNarrativeLayerForTest()
				&& Widget->GetNarrativeLayerForTest()->IsPresentationReady()))
	{
		return false;
	}
	TestTrue(TEXT("preflight fixture expands Workbench"), Widget->OpenBackpack());
	TestTrue(TEXT("preflight fixture opens StoryTasks"), Widget->ToggleStoryTaskDrawerForTest());
	const FVector2D TopLeftBefore = Widget->GetDesktopWindowTopLeftForHost();
	const FVector2D SizeBefore = Widget->GetDesktopWindowSizeForHost();
	const EGameXXKDesktopTrainingLeftPanel LeftBefore = Widget->GetLeftPanelForTest();
	const FGameXXKDesktopNativeSurfaceState HitStateBefore =
		Widget->GetDesktopNativeSurfaceStateForTest();
	Widget->SetForceNarrativeLayerUnavailableForTest(true);
	TestFalse(TEXT("incomplete narrative layer rejects entry"),
		Widget->EnterNarrativePresentationForTest());
	TestEqual(TEXT("failed entry preserves Workbench surface"),
		Widget->GetOverlaySurfaceForTest(), EGameXXKDesktopOverlaySurface::Workbench);
	TestTrue(TEXT("failed entry preserves visible Workbench child"),
		Widget->IsWorkbenchLayerVisibleForTest());
	TestFalse(TEXT("failed entry cannot leave Tab locked"), Widget->IsNarrativeTabLockedForTest());
	TestTrue(TEXT("failed entry preserves expanded Workbench presentation"),
		Widget->IsBackpackExpandedForTest());
	TestEqual(TEXT("failed entry preserves open left panel"),
		Widget->GetLeftPanelForTest(), LeftBefore);
	TestEqual(TEXT("failed entry preserves native HUD top-left"),
		Widget->GetDesktopWindowTopLeftForHost(), TopLeftBefore);
	TestEqual(TEXT("failed entry preserves native HUD size"),
		Widget->GetDesktopWindowSizeForHost(), SizeBefore);
	const FGameXXKDesktopNativeSurfaceState HitStateAfter =
		Widget->GetDesktopNativeSurfaceStateForTest();
	TestEqual(TEXT("failed entry preserves native hit ownership"),
		HitStateAfter.bNarrativeLayerActive, HitStateBefore.bNarrativeLayerActive);
	TestEqual(TEXT("failed entry preserves native hit regions"),
		HitStateAfter.InteractiveRects, HitStateBefore.InteractiveRects);
	TestTrue(TEXT("Exit after partial entry failure still succeeds"),
		Widget->ExitNarrativePresentationToFoldedDesktopForTest());
	TestFalse(TEXT("Exit after failure folds Workbench"), Widget->IsBackpackExpandedForTest());
	TestEqual(TEXT("Exit after failure clears left panel"),
		Widget->GetLeftPanelForTest(), EGameXXKDesktopTrainingLeftPanel::None);
	TestFalse(TEXT("Exit after failure leaves no narrative lock"),
		Widget->IsNarrativeTabLockedForTest());
	TestTrue(TEXT("Exit after failure leaves Workbench visible"),
		Widget->IsWorkbenchLayerVisibleForTest());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopNarrativeSessionAndNativeSurfaceTest,
	"GameXXK.DesktopNarrative.Layer.SessionAndNativeSurface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopNarrativeSessionAndNativeSurfaceTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = nullptr;
	UGameXXKDesktopTrainingWorkbenchWidget* const Widget = MakeWorkbench(Subsystem);
	Widget->OpenBackpack();
	Widget->EnterNarrativePresentationForTest();
	const FGameXXKDesktopWorkbenchSessionState Captured =
		Widget->CaptureSessionStateForMapTravel();
	TestFalse(TEXT("narrative capture normalizes to a folded desktop"),
		Captured.bBackpackExpanded);
	TestEqual(TEXT("narrative capture never persists a drawer"),
		Captured.LeftPanel, EGameXXKDesktopTrainingLeftPanel::None);

	const FGameXXKDesktopNativeSurfaceState Surface =
		Widget->GetDesktopNativeSurfaceStateForTest();
	TestTrue(TEXT("native mouse surface records narrative ownership"),
		Surface.bNarrativeLayerActive);
	TestEqual(TEXT("only dialogue and pause are native interactive regions"),
		Surface.InteractiveRects.Num(), 2);
	UGameXXKDesktopNarrativeLayerWidget* const NarrativeLayer =
		Widget->GetNarrativeLayerForTest();
	if (!TestNotNull(TEXT("native surface fixture owns narrative layer"), NarrativeLayer)
		|| !TestNotNull(TEXT("native surface fixture owns history presenter"),
			NarrativeLayer ? NarrativeLayer->GetDialogueHistory() : nullptr)
		|| !TestTrue(TEXT("native surface exposes required dialogue and pause regions"),
			Surface.InteractiveRects.Num() >= 2))
	{
		return false;
	}
	const FGameXXKDesktopNarrativeLayout Layout =
		NarrativeLayer->GetResolvedLayoutForTest();
	if (Surface.InteractiveRects.Num() >= 2)
	{
		TestEqual(TEXT("first native region is the dialogue host"),
			Surface.InteractiveRects[0], Layout.DialogueHostRect);
		TestEqual(TEXT("second native region is pause"),
			Surface.InteractiveRects[1], Layout.PauseRect);
		TestTrue(TEXT("transparent stage gap passes through by production policy"),
			Widget->ShouldDesktopMousePassThroughAtPhysicalPointForTest(FVector2D(20.0f, 20.0f)));
		TestFalse(TEXT("dialogue host is native interactive by production policy"),
			Widget->ShouldDesktopMousePassThroughAtPhysicalPointForTest(FVector2D(
				Surface.InteractiveRects[0].X + Surface.InteractiveRects[0].Z * 0.5f,
				Surface.InteractiveRects[0].Y + Surface.InteractiveRects[0].W * 0.5f)));
		TestFalse(TEXT("pause button is native interactive by production policy"),
			Widget->ShouldDesktopMousePassThroughAtPhysicalPointForTest(FVector2D(
				Surface.InteractiveRects[1].X + Surface.InteractiveRects[1].Z * 0.5f,
				Surface.InteractiveRects[1].Y + Surface.InteractiveRects[1].W * 0.5f)));
	}
	UGameXXKDialogueHistoryWidget* const History = NarrativeLayer->GetDialogueHistory();
	FGameXXKDialogueHistoryEntry HistoryEntry;
	HistoryEntry.SpeakerId = TEXT("Npc.Test");
	HistoryEntry.Text = FText::FromString(TEXT("history hit region"));
	History->PresentHistory({HistoryEntry});
	const FGameXXKDesktopNativeSurfaceState OpenHistorySurface =
		Widget->GetDesktopNativeSurfaceStateForTest();
	TestEqual(TEXT("visible history adds exactly one native hit region"),
		OpenHistorySurface.InteractiveRects.Num(), 3);
	const FVector2D HistoryOnlyPoint(
		Layout.HistoryRect.X + 20.0f,
		Layout.HistoryRect.Y + 20.0f);
	TestTrue(TEXT("history test point stays outside dialogue"),
		HistoryOnlyPoint.Y < Layout.DialogueHostRect.Y);
	TestFalse(TEXT("visible history point becomes HTCLIENT"),
		Widget->ShouldDesktopMousePassThroughAtPhysicalPointForTest(HistoryOnlyPoint));
	History->HideHistory();
	TestEqual(TEXT("hidden history removes its native hit region"),
		Widget->GetDesktopNativeSurfaceStateForTest().InteractiveRects.Num(), 2);
	TestTrue(TEXT("hidden history point returns to passthrough"),
		Widget->ShouldDesktopMousePassThroughAtPhysicalPointForTest(HistoryOnlyPoint));

	UGameXXKMVPSubsystem* RestoredSubsystem = nullptr;
	UGameXXKDesktopTrainingWorkbenchWidget* const Restored =
		MakeWorkbench(RestoredSubsystem);
	Restored->EnterNarrativePresentationForTest();
	Restored->RestoreSessionStateAfterMapTravel(Captured);
	TestEqual(TEXT("restore always chooses Workbench surface"),
		Restored->GetOverlaySurfaceForTest(), EGameXXKDesktopOverlaySurface::Workbench);
	TestFalse(TEXT("restore never auto-starts narrative"),
		Restored->IsNarrativeLayerActiveForTest());
	TestFalse(TEXT("restored workbench stays folded"), Restored->IsBackpackExpandedForTest());
	return true;
}

#endif
