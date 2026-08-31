# GameXXK Desktop Training Workbench Layout Reflow Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rebuild the desktop Training workbench and merged challenge canvas on the approved 1672×941 reference geometry so the live UE layout matches the selected concept at 1920×1080 and 2560×1440 without squeezing either axis.

**Architecture:** The widget will render on one 1672×941 reference `UCanvasPanel` inside a centered `UScaleBox`/`USizeBox` pair. Structural rectangles are owned by a small pure C++ layout module and consumed by both the widget and Automation tests; all runtime scaling uses one `min(widthScale, heightScale)` value. This batch changes geometry and presentation only: it reuses existing MasterV2 textures and gameplay read models, does not import concept-image pixels, does not change the default entry map, and does not touch `L_Main.umap`.

**Tech Stack:** Unreal Engine 5.8 C++, UMG/Slate, UE Automation Tests, project UE MCP scripts, cold UBT (`-NoHotReload`), visual evidence comparison.

---

## Scope and protected state

- Execute in the existing root checkout. Do not create or switch worktrees and do not switch branches while the user worktree is dirty.
- Do not stage, edit, save, or revert `Content/GameXXK/Maps/L_Main.umap`.
- Do not import or crop `exec-44e28ab2-41b3-4c65-b85a-ff4a2d189547.png`; it is geometry evidence only.
- Reuse `/Game/GameXXK/UI/MasterV2/Approved/*` and the existing Training walkloop/background assets.
- Do not change training progression, rewards, save migration, enemy formations, or default map routing in this batch.
- If the editor must close for C++ compilation, first save dirty packages through UE MCP. If MCP cannot prove the save, stop without force-closing the editor.

## Frozen geometry

All values below are `X, Y, Width, Height` in the 1672×941 reference space:

| Region | Rect |
|---|---|
| Warehouse | `10, 17, 363, 908` |
| Center shell | `386, 17, 970, 908` |
| Training/tools | `1369, 17, 291, 908` |
| Idle strip | `394, 21, 953, 202` |
| Backpack/content | `397, 244, 945, 533` |
| Bottom navigation | `397, 788, 945, 137` |
| Challenge viewport | `394, 21, 953, 756` |
| Challenge combat strip | `414, 105, 913, 86` |
| Challenge battle board | `414, 203, 913, 470` |

The fit rule is:

```cpp
Scale = FMath::Min(ViewportSize.X / 1672.0f, ViewportSize.Y / 941.0f);
Offset = (ViewportSize - FVector2D(1672.0f, 941.0f) * Scale) * 0.5f;
```

## File structure

- Create `Source/GameXXK/Public/UI/GameXXKDesktopTrainingLayout.h`: public reference rectangles and uniform-fit transform.
- Create `Source/GameXXK/Private/UI/GameXXKDesktopTrainingLayout.cpp`: immutable geometry data and transform implementation.
- Modify `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h`: reference-canvas root members and narrow geometry test accessors.
- Modify `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`: scale-to-fit root, named structural panels, normal workbench reflow, challenge reflow.
- Modify `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`: geometry, uniform-scaling, named-slot, and no-overlap contracts.
- Modify `scripts/run_training_visual_pie_probe.py`: optional capture dimensions and evidence paths.
- Modify `Content/Python/gamexxk_probe_training_visual_mvp.py`: requested capture size and reference-layout snapshot fields.
- Create `scripts/test_run_training_visual_pie_probe.py`: headless argument/payload contract for the visual runner.
- Update `docs/production/2026-08-18-desktop-training-2d-hud-migration.md`: dated cold-build, Automation, PIE, screenshot, and visual evidence.

### Task 1: Add the reference-space geometry contract

**Files:**
- Create: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingLayout.h`
- Create: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingLayout.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`

- [ ] **Step 1: Record the protected-map and worktree preflight**

Run:

```powershell
git status --short --branch
Get-FileHash -Algorithm SHA256 -LiteralPath 'Content/GameXXK/Maps/L_Main.umap'
```

Expected: the existing dirty state is visible and the `L_Main.umap` hash is recorded in the production note before any compile lifecycle action.

- [ ] **Step 2: Write the failing geometry test**

Add the include and a new test to `GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`:

```cpp
#include "UI/GameXXKDesktopTrainingLayout.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingReferenceGeometryTest,
	"GameXXK.DesktopTraining.Workbench.ReferenceGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingReferenceGeometryTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKDesktopTrainingLayout;
	TestEqual(TEXT("reference canvas is the approved UI Master size"), GetReferenceCanvasSize(), FVector2D(1672.0f, 941.0f));
	TestEqual(TEXT("warehouse matches the selected layout"), GetWarehouseRect(), FVector4(10.0f, 17.0f, 363.0f, 908.0f));
	TestEqual(TEXT("center shell matches the selected layout"), GetCenterShellRect(), FVector4(386.0f, 17.0f, 970.0f, 908.0f));
	TestEqual(TEXT("right shell matches the selected layout"), GetRightShellRect(), FVector4(1369.0f, 17.0f, 291.0f, 908.0f));
	TestEqual(TEXT("idle strip matches the selected layout"), GetIdleStripRect(), FVector4(394.0f, 21.0f, 953.0f, 202.0f));
	TestEqual(TEXT("backpack surface matches the selected layout"), GetContentRect(), FVector4(397.0f, 244.0f, 945.0f, 533.0f));
	TestEqual(TEXT("navigation matches the selected layout"), GetNavigationRect(), FVector4(397.0f, 788.0f, 945.0f, 137.0f));

	const FFitTransform FullHD = MakeFitTransform(FVector2D(1920.0f, 1080.0f));
	const FFitTransform QHD = MakeFitTransform(FVector2D(2560.0f, 1440.0f));
	TestTrue(TEXT("Full HD uses one uniform scale"), FMath::IsNearlyEqual(FullHD.Scale, 1080.0f / 941.0f, KINDA_SMALL_NUMBER));
	TestTrue(TEXT("QHD uses one uniform scale"), FMath::IsNearlyEqual(QHD.Scale, 1440.0f / 941.0f, KINDA_SMALL_NUMBER));
	const FVector4 FullHDNode = FullHD.ApplyRect(FVector4(0.0f, 0.0f, 58.0f, 58.0f));
	const FVector4 QHDNode = QHD.ApplyRect(FVector4(0.0f, 0.0f, 58.0f, 58.0f));
	TestTrue(TEXT("Full HD nodes remain circular"), FMath::IsNearlyEqual(FullHDNode.Z, FullHDNode.W, KINDA_SMALL_NUMBER));
	TestTrue(TEXT("QHD nodes remain circular"), FMath::IsNearlyEqual(QHDNode.Z, QHDNode.W, KINDA_SMALL_NUMBER));
	return true;
}
```

- [ ] **Step 3: Run the cold compile and verify RED**

First save and close through the supported pipeline, then run:

```powershell
python scripts/ai_production_loop.py --run-ubt --json
```

Expected: UBT fails because `UI/GameXXKDesktopTrainingLayout.h` does not exist. A stale editor DLL or pre-existing Automation report is not RED evidence.

- [ ] **Step 4: Implement the minimal layout module**

Create `GameXXKDesktopTrainingLayout.h`:

```cpp
#pragma once

#include "CoreMinimal.h"

namespace GameXXKDesktopTrainingLayout
{
	struct GAMEXXK_API FFitTransform
	{
		float Scale = 1.0f;
		FVector2D Offset = FVector2D::ZeroVector;

		FVector2D ApplyPoint(const FVector2D& Point) const;
		FVector2D ApplySize(const FVector2D& Size) const;
		FVector4 ApplyRect(const FVector4& Rect) const;
	};

	GAMEXXK_API FVector2D GetReferenceCanvasSize();
	GAMEXXK_API FVector4 GetWarehouseRect();
	GAMEXXK_API FVector4 GetCenterShellRect();
	GAMEXXK_API FVector4 GetRightShellRect();
	GAMEXXK_API FVector4 GetIdleStripRect();
	GAMEXXK_API FVector4 GetContentRect();
	GAMEXXK_API FVector4 GetNavigationRect();
	GAMEXXK_API FVector4 GetChallengeViewportRect();
	GAMEXXK_API FVector4 GetChallengeCombatStripRect();
	GAMEXXK_API FVector4 GetChallengeBattleBoardRect();
	GAMEXXK_API FFitTransform MakeFitTransform(const FVector2D& ViewportSize);
}
```

Create `GameXXKDesktopTrainingLayout.cpp` with immutable values and the single-scale transform:

```cpp
#include "UI/GameXXKDesktopTrainingLayout.h"

namespace GameXXKDesktopTrainingLayout
{
	namespace
	{
		const FVector2D ReferenceCanvasSize(1672.0f, 941.0f);
		const FVector4 WarehouseRect(10.0f, 17.0f, 363.0f, 908.0f);
		const FVector4 CenterShellRect(386.0f, 17.0f, 970.0f, 908.0f);
		const FVector4 RightShellRect(1369.0f, 17.0f, 291.0f, 908.0f);
		const FVector4 IdleStripRect(394.0f, 21.0f, 953.0f, 202.0f);
		const FVector4 ContentRect(397.0f, 244.0f, 945.0f, 533.0f);
		const FVector4 NavigationRect(397.0f, 788.0f, 945.0f, 137.0f);
		const FVector4 ChallengeViewportRect(394.0f, 21.0f, 953.0f, 756.0f);
		const FVector4 ChallengeCombatStripRect(414.0f, 105.0f, 913.0f, 86.0f);
		const FVector4 ChallengeBattleBoardRect(414.0f, 203.0f, 913.0f, 470.0f);
	}

	FVector2D FFitTransform::ApplyPoint(const FVector2D& Point) const { return Offset + Point * Scale; }
	FVector2D FFitTransform::ApplySize(const FVector2D& Size) const { return Size * Scale; }
	FVector4 FFitTransform::ApplyRect(const FVector4& Rect) const
	{
		const FVector2D Point = ApplyPoint(FVector2D(Rect.X, Rect.Y));
		const FVector2D Size = ApplySize(FVector2D(Rect.Z, Rect.W));
		return FVector4(Point.X, Point.Y, Size.X, Size.Y);
	}

	FVector2D GetReferenceCanvasSize() { return ReferenceCanvasSize; }
	FVector4 GetWarehouseRect() { return WarehouseRect; }
	FVector4 GetCenterShellRect() { return CenterShellRect; }
	FVector4 GetRightShellRect() { return RightShellRect; }
	FVector4 GetIdleStripRect() { return IdleStripRect; }
	FVector4 GetContentRect() { return ContentRect; }
	FVector4 GetNavigationRect() { return NavigationRect; }
	FVector4 GetChallengeViewportRect() { return ChallengeViewportRect; }
	FVector4 GetChallengeCombatStripRect() { return ChallengeCombatStripRect; }
	FVector4 GetChallengeBattleBoardRect() { return ChallengeBattleBoardRect; }

	FFitTransform MakeFitTransform(const FVector2D& ViewportSize)
	{
		FFitTransform Result;
		if (ViewportSize.X <= 0.0f || ViewportSize.Y <= 0.0f)
		{
			return Result;
		}
		Result.Scale = FMath::Min(ViewportSize.X / ReferenceCanvasSize.X, ViewportSize.Y / ReferenceCanvasSize.Y);
		Result.Offset = (ViewportSize - ReferenceCanvasSize * Result.Scale) * 0.5f;
		return Result;
	}
}
```

- [ ] **Step 5: Run focused GREEN verification**

Run:

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests "GameXXK.DesktopTraining.Workbench.ReferenceGeometry" --automation-report "20260818-workbench-reference-geometry" --json
```

Expected: UBT exit code 0; one focused Automation test passes with no error.

- [ ] **Step 6: Commit only the geometry contract**

```powershell
git add -- 'Source/GameXXK/Public/UI/GameXXKDesktopTrainingLayout.h' 'Source/GameXXK/Private/UI/GameXXKDesktopTrainingLayout.cpp' 'Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp'
git diff --cached --name-only
git commit -m "test: freeze desktop training reference geometry"
```

Expected: the staged-name list contains exactly the three files above and never `L_Main.umap`.

### Task 2: Put the reference canvas behind one uniform ScaleBox

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`

- [ ] **Step 1: Write the failing root-structure test**

In the Slate build contract, add:

```cpp
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"

UScaleBox* ScaleRoot = Cast<UScaleBox>(Widget->WidgetTree->RootWidget);
TestNotNull(TEXT("workbench root is a uniform ScaleBox"), ScaleRoot);
TestTrue(TEXT("workbench root uses ScaleToFit"), ScaleRoot && ScaleRoot->GetStretch() == EStretch::ScaleToFit);
USizeBox* ReferenceBox = ScaleRoot ? Cast<USizeBox>(ScaleRoot->GetContent()) : nullptr;
TestNotNull(TEXT("ScaleBox owns the fixed reference SizeBox"), ReferenceBox);
TestTrue(TEXT("reference width is 1672"), ReferenceBox && FMath::IsNearlyEqual(ReferenceBox->GetWidthOverride(), 1672.0f));
TestTrue(TEXT("reference height is 941"), ReferenceBox && FMath::IsNearlyEqual(ReferenceBox->GetHeightOverride(), 941.0f));
```

- [ ] **Step 2: Run the focused test and verify RED**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests "GameXXK.DesktopTraining.Workbench.SlateBuildContract" --automation-report "20260818-workbench-scale-root-red" --json
```

Expected: the test fails because the current root is a `UCanvasPanel`, not a `UScaleBox`.

- [ ] **Step 3: Implement the scale-to-fit root**

Add transient `UScaleBox`, `USizeBox`, and `UCanvasPanel` members in the widget header. In `BuildProgrammaticLayout`, construct the hierarchy once:

```cpp
RootScaleBox = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("DesktopTrainingScaleRoot"));
RootScaleBox->SetStretch(EStretch::ScaleToFit);
RootScaleBox->SetStretchDirection(EStretchDirection::Both);
RootScaleBox->SetHorizontalAlignment(HAlign_Center);
RootScaleBox->SetVerticalAlignment(VAlign_Center);

ReferenceCanvasBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("DesktopTrainingReferenceBox"));
ReferenceCanvasBox->SetWidthOverride(1672.0f);
ReferenceCanvasBox->SetHeightOverride(941.0f);

RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DesktopTrainingReferenceCanvas"));
ReferenceCanvasBox->SetContent(RootCanvas);
RootScaleBox->SetContent(ReferenceCanvasBox);
WidgetTree->RootWidget = RootScaleBox;
```

Keep `RootCanvas->ClearChildren()` as the only rebuild target. Do not reconstruct the scale root during `RefreshLayout`; this preserves the current Slate release/reattach lifecycle.

- [ ] **Step 4: Run focused and lifecycle GREEN tests**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests "GameXXK.DesktopTraining.Workbench" --automation-report "20260818-workbench-scale-root-green" --json
```

Expected: all `GameXXK.DesktopTraining.Workbench.*` tests pass, including Slate build and NativeConstruct single-build contracts.

- [ ] **Step 5: Commit the scale root**

```powershell
git add -- 'Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h' 'Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp' 'Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp'
git diff --cached --name-only
git commit -m "fix: uniformly scale desktop training canvas"
```

### Task 3: Reflow the normal workbench to the approved three-column geometry

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`

- [ ] **Step 1: Write failing structural-slot assertions**

After `Widget->TakeWidget()`, find named structural widgets and assert their canvas slots:

```cpp
const auto TestNamedRect = [this, Widget](const TCHAR* Name, const FVector4& Expected)
{
	UWidget* Child = Widget->WidgetTree->FindWidget(FName(Name));
	TestNotNull(FString::Printf(TEXT("%s exists"), Name), Child);
	const UCanvasPanelSlot* Slot = Child ? Cast<UCanvasPanelSlot>(Child->Slot) : nullptr;
	TestNotNull(FString::Printf(TEXT("%s is placed on the reference canvas"), Name), Slot);
	if (Slot)
	{
		TestEqual(FString::Printf(TEXT("%s position"), Name), Slot->GetPosition(), FVector2D(Expected.X, Expected.Y));
		TestEqual(FString::Printf(TEXT("%s size"), Name), Slot->GetSize(), FVector2D(Expected.Z, Expected.W));
	}
};

TestNamedRect(TEXT("WarehousePanel"), GameXXKDesktopTrainingLayout::GetWarehouseRect());
TestNamedRect(TEXT("CenterWorkbenchFrame"), GameXXKDesktopTrainingLayout::GetCenterShellRect());
TestNamedRect(TEXT("TrainingTravelStrip"), GameXXKDesktopTrainingLayout::GetIdleStripRect());
TestNamedRect(TEXT("BackpackPanel"), GameXXKDesktopTrainingLayout::GetContentRect());
TestNamedRect(TEXT("TrainingMapPanel"), GameXXKDesktopTrainingLayout::GetRightShellRect());
TestNamedRect(TEXT("BottomNavigationPanel"), GameXXKDesktopTrainingLayout::GetNavigationRect());
```

Add `#include "Components/CanvasPanelSlot.h"`. Run the test before naming or moving any widget.

- [ ] **Step 2: Verify RED against the current old geometry**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests "GameXXK.DesktopTraining.Workbench.LayoutContract" --automation-report "20260818-workbench-layout-red" --json
```

Expected: named-widget lookup or slot-geometry assertions fail; current geometry still reports warehouse `24,150,320,840` and right panel `1340,150,556,840`.

- [ ] **Step 3: Name structural panels and move the outer rectangles**

Allow `MakePanel` to accept a widget name and construct named borders. Use the layout module for every structural rectangle:

```cpp
UBorder* MakePanel(UWidgetTree* Tree, const FLinearColor& Color, const FName Name = NAME_None)
{
	UBorder* Result = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
	const FSlateBrush Brush = MakeBoxTextureBrush(PanelLargeTexturePath, FVector2D(320.0f, 180.0f));
	if (Brush.GetResourceObject())
	{
		Result->SetBrush(Brush);
	}
	else
	{
		Result->SetBrushColor(Color);
	}
	return Result;
}

const auto AddRect = [](UCanvasPanel* Canvas, UWidget* Child, const FVector4& Rect)
{
	AddCanvas(Canvas, Child, FVector2D(Rect.X, Rect.Y), FVector2D(Rect.Z, Rect.W));
};
```

Place `CenterWorkbenchFrame` before its child surfaces so it stays behind them. Use the six frozen normal-view rectangles exactly; do not retain the old y=150 offsets.

- [ ] **Step 4: Reflow each region internally without changing its data source**

Use these reference-space bounds:

| Content | Position/size rule |
|---|---|
| Warehouse title | `30, 34, 323, 38` |
| Warehouse 4-column grid | origin `30, 126`; cell `68×68`; horizontal pitch `78`; vertical pitch `72` |
| Warehouse page/footer controls | y range `812..902`, inside x `30..353` |
| Idle status/cooldown | x range `414..824`, y range `34..211` |
| Idle encounter slots | three equal `74×92` slots from x `610`; no X/Y distortion |
| Idle party slots | three equal `74×92` slots from x `886`; no X/Y distortion |
| Idle collect/retry controls | x range `1116..1327`, y range `148..205` |
| Backpack title/gold/settings/close | x range `417..1322`, y range `260..302` |
| Six equipment slots | `72×72`, all inside the left half of the backpack surface |
| Backpack 4×5 grid | cell `104×52`; origin `744, 400`; pitch `114×60` |
| Backpack sort/occupancy | y range `706..760` |
| Training title | `1387, 34, 255, 38` |
| Difficulty tabs | three `78×36` tabs, pitch `84`, starting x `1388`, y `84` |
| Training nodes | nine square `58×58` nodes in a 3×3 grid, origin `1390, 158`, pitch `84×104` |
| Current travel label | x `1388`, y `640`, width `252` |
| Challenge/Travel buttons | two `116×58` buttons at x `1388` and `1517`, y `828` |
| Bottom navigation | five `151×112` buttons, origin `421, 800`, pitch `181`; icon brushes remain square |

Keep runtime gold, warehouse instances, backpack items, selected stage, tooltips, cooldowns, and action IDs connected to their existing read models. Do not replace them with values from the concept image.

- [ ] **Step 5: Make square/circular assets enforce one dimension**

For route nodes, navigation discs, equipment slots, and party/enemy slots, calculate a `Diameter` once and pass `FVector2D(Diameter, Diameter)` to both the brush and canvas slot. Do not derive width and height separately from panel ratios.

- [ ] **Step 6: Run normal-shell GREEN tests**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests "GameXXK.DesktopTraining.Workbench" --automation-report "20260818-workbench-layout-green" --json
```

Expected: all Workbench tests pass; the named structural slots equal the frozen rectangles; circle tests remain equal-width/equal-height.

- [ ] **Step 7: Commit the normal workbench reflow**

```powershell
git add -- 'Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h' 'Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp' 'Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp'
git diff --cached --name-only
git commit -m "fix: match desktop training workbench geometry"
```

### Task 4: Reflow the merged ChallengeViewport without moving the side shells

**Files:**
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`

- [ ] **Step 1: Replace the old challenge expectations with the approved RED contract**

Assert:

```cpp
TestEqual(TEXT("challenge viewport replaces idle plus content"), Widget->GetChallengeViewportRectForTest(), FVector4(394.0f, 21.0f, 953.0f, 756.0f));
TestEqual(TEXT("challenge combat strip stays inside the merged canvas"), Widget->GetChallengeCombatStripRectForTest(), FVector4(414.0f, 105.0f, 913.0f, 86.0f));
TestEqual(TEXT("challenge board receives the wide readable surface"), Widget->GetChallengeBattleBoardRectForTest(), FVector4(414.0f, 203.0f, 913.0f, 470.0f));
TestTrue(TEXT("challenge stops before bottom navigation"), ChallengeViewportRect.Y + ChallengeViewportRect.W <= GameXXKDesktopTrainingLayout::GetNavigationRect().Y);
TestTrue(TEXT("challenge board is wider than the previous 710 px board"), ChallengeBattleBoardRect.Z > 900.0f);
```

- [ ] **Step 2: Run focused RED verification**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests "GameXXK.DesktopTraining.Workbench.LayoutContract" --automation-report "20260818-challenge-layout-red" --json
```

Expected: the old `365,22,960,968` viewport and `395,240,710,535` board fail the new assertions.

- [ ] **Step 3: Apply the challenge rectangles and local toolbar layout**

Use the layout module for viewport, combat strip, and battle board. Place title/status above the strip, and place the two challenge-only controls in a toolbar below the board but above y=756:

```cpp
const FVector2D AutoBattlePosition(430.0f, 690.0f);
const FVector2D AutoBattleSize(170.0f, 54.0f);
const FVector2D AdvancePosition(618.0f, 690.0f);
const FVector2D AdvanceSize(220.0f, 54.0f);
const FVector2D ChallengeHintPosition(860.0f, 694.0f);
const FVector2D ChallengeHintSize(445.0f, 46.0f);
```

Keep `BuildWarehousePanel(true)` and `BuildTrainingMapPanel(true)` on the same frozen side rectangles. Keep navigation locked while challenge is active. Do not create a second inventory snapshot or a second battle session.

- [ ] **Step 4: Run GREEN regression for layout and challenge ownership**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests "GameXXK.DesktopTraining.Workbench" --automation-report "20260818-challenge-layout-green" --json
```

Expected: geometry, read-only side panels, challenge-only auto battle, travel-only retry, and Slate rebuild lifecycle all pass.

- [ ] **Step 5: Commit the challenge reflow**

```powershell
git add -- 'Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp' 'Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp'
git diff --cached --name-only
git commit -m "fix: enlarge desktop training challenge canvas"
```

### Task 5: Add deterministic dual-resolution visual evidence

**Files:**
- Modify: `scripts/run_training_visual_pie_probe.py`
- Modify: `Content/Python/gamexxk_probe_training_visual_mvp.py`
- Create: `scripts/test_run_training_visual_pie_probe.py`

- [ ] **Step 1: Write failing headless tests for capture dimensions**

Create `scripts/test_run_training_visual_pie_probe.py` with these exact cases:

```python
import json
import unittest

from scripts import run_training_visual_pie_probe as runner


class FakeClient:
    def __init__(self):
        self.calls = []

    def run_project_python_file(self, path, argv):
        self.calls.append((path, list(argv)))
        return {
            "stdout": json.dumps(
                {
                    "ok": True,
                    "reference_canvas": [1672.0, 941.0],
                    "warehouse_rect": [10.0, 17.0, 363.0, 908.0],
                    "content_rect": [397.0, 244.0, 945.0, 533.0],
                    "right_rect": [1369.0, 17.0, 291.0, 908.0],
                    "challenge_rect": [394.0, 21.0, 953.0, 756.0],
                },
                ensure_ascii=False,
            )
        }


class TrainingVisualCaptureSizeTest(unittest.TestCase):
    def test_accepts_full_hd_and_qhd(self):
        self.assertEqual(runner.parse_capture_size(1920, 1080), (1920, 1080))
        self.assertEqual(runner.parse_capture_size(2560, 1440), (2560, 1440))

    def test_rejects_non_positive_dimensions(self):
        with self.assertRaises(ValueError):
            runner.parse_capture_size(0, 1080)
        with self.assertRaises(ValueError):
            runner.parse_capture_size(1920, -1)

    def test_observe_phase_forwards_capture_dimensions(self):
        client = FakeClient()
        payload = runner._run_phase(
            client,
            "observe",
            "--capture",
            "--capture-width",
            "2560",
            "--capture-height",
            "1440",
        )
        self.assertTrue(payload["ok"])
        self.assertEqual(client.calls[0][0], runner.PROBE)
        self.assertEqual(
            client.calls[0][1],
            [
                "--phase",
                "observe",
                "--capture",
                "--capture-width",
                "2560",
                "--capture-height",
                "1440",
            ],
        )


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Run the headless test and verify RED**

```powershell
python -m unittest scripts.test_run_training_visual_pie_probe -v
```

Expected: import or missing-function failure because `parse_capture_size` and dimension arguments do not exist.

- [ ] **Step 3: Implement size forwarding and geometry snapshot**

Add this validated helper to the external runner:

```python
def parse_capture_size(width: int, height: int) -> tuple[int, int]:
    if width <= 0 or height <= 0:
        raise ValueError("capture dimensions must be positive")
    return width, height
```

Add parser flags with defaults `1920` and `1080`. In `run`, call `parse_capture_size` before connecting and forward:

```python
capture_width, capture_height = parse_capture_size(args.capture_width, args.capture_height)
observe_extra = []
if args.capture:
    observe_extra = [
        "--capture",
        "--capture-width", str(capture_width),
        "--capture-height", str(capture_height),
    ]
observed = _run_phase(client, "observe", *observe_extra)
```

The UE phase parses the same two integer flags, executes this only when `--capture` is present, and returns immediately:

```python
unreal.SystemLibrary.execute_console_command(
    world,
    f"r.SetRes {capture_width}x{capture_height}w",
)
unreal.SystemLibrary.execute_console_command(
    world,
    f"HighResShot {capture_width}x{capture_height}",
)
payload.update(
    {
        "capture_requested": True,
        "capture_size": [capture_width, capture_height],
        "reference_canvas": [1672.0, 941.0],
        "warehouse_rect": list(_call(widget, "get_warehouse_rect_for_test") or []),
        "content_rect": list(_call(widget, "get_content_rect_for_test") or []),
        "right_rect": list(_call(widget, "get_right_panel_rect_for_test") or []),
        "challenge_rect": list(_call(widget, "get_challenge_viewport_rect_for_test") or []),
    }
)
```

Expose the three missing structural rect accessors on the widget by returning the layout-module constants. The UE phase must not sleep on the game thread.

- [ ] **Step 4: Run headless GREEN verification**

```powershell
python -m unittest scripts.test_run_training_visual_pie_probe -v
```

Expected: all new runner tests pass without launching Unreal Editor.

- [ ] **Step 5: Run real PIE captures at both acceptance resolutions**

With the freshly cold-built editor and MCP ready, run:

```powershell
python scripts/run_training_visual_pie_probe.py --capture --capture-width 1920 --capture-height 1080 --observe-seconds 2.0
python scripts/run_training_visual_pie_probe.py --capture --capture-width 2560 --capture-height 1440 --observe-seconds 2.0
```

Expected for both: `ok: true`, native tick advances, scroll offset advances, walk frame advances, atlas/background paths are non-empty, and returned geometry matches the same 1672×941 reference rectangles.

- [ ] **Step 6: Commit the visual evidence runner**

```powershell
git add -- 'scripts/run_training_visual_pie_probe.py' 'Content/Python/gamexxk_probe_training_visual_mvp.py' 'scripts/test_run_training_visual_pie_probe.py'
git diff --cached --name-only
git commit -m "test: capture desktop training at two resolutions"
```

### Task 6: Cold verification, visual review, and production evidence

**Files:**
- Modify: `docs/production/2026-08-18-desktop-training-2d-hud-migration.md`

- [ ] **Step 1: Run the full cold gate for this batch**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests "GameXXK.DesktopTraining.Workbench" --automation-report "20260818-workbench-layout-final" --json
python scripts/ai_production_loop.py --run-script-tests --script-test-tag headless --json
git diff --check
```

Expected: cold UBT exit code 0, all Workbench Automation tests pass, headless manifest passes without launching the editor, and `git diff --check` has no whitespace error.

- [ ] **Step 2: Capture the same states as the reference**

Capture at 1920×1080 and 2560×1440:

1. Normal workbench with Warehouse + Backpack + Training map.
2. Tools replacing the right map.
3. Formation replacing the center backpack.
4. Challenge viewport with read-only side shells.

The normal-workbench screenshot must show all three panel tops aligned near y=17, the idle strip fully inside the center column, the right map narrower than the warehouse, five navigation buttons inside the center width, and round nodes/discs.

- [ ] **Step 3: Review the captured visual evidence**

Compare the selected reference with each fresh PIE screenshot using a suitable method. Measure P0/P1/P2 differences in outer rectangles, top alignment, column widths, content clipping, circle aspect ratio, text readability, and challenge-canvas size.

Expected: no P0/P1/P2 geometry mismatch. Any remaining difference must be P3 art polish and must not come from stretching or concept-image reuse.

- [ ] **Step 4: Record evidence and recheck protected assets**

Append commands, timestamps, report paths, screenshot paths, visual-review result, before/after `L_Main.umap` SHA256, and exact staged files to the migration production note. Then run:

```powershell
Get-FileHash -Algorithm SHA256 -LiteralPath 'Content/GameXXK/Maps/L_Main.umap'
git status --short
git diff --name-only -- 'Content/GameXXK/Maps/L_Main.umap' 'SourceAssets' 'SourceArt'
```

Expected: the protected map hash is unchanged from Task 1; no new SourceAssets/SourceArt files were introduced by this batch.

- [ ] **Step 5: Commit only the dated evidence**

```powershell
git add -- 'docs/production/2026-08-18-desktop-training-2d-hud-migration.md' 'docs/superpowers/specs/2026-08-17-gamexxk-desktop-training-workbench-design.md' 'docs/superpowers/plans/2026-08-18-desktop-training-workbench-layout-reflow.md'
git diff --cached --name-only
git commit -m "docs: verify desktop training layout reflow"
```

## Completion checkpoint for this batch

This plan is complete only when all of the following are backed by fresh evidence:

- Normal and challenge structural rectangles equal the frozen 1672×941 values.
- One ScaleBox transform preserves aspect ratio at both 1920×1080 and 2560×1440.
- No node, navigation disc, equipment slot, or character image is scaled with different X/Y factors.
- The ChallengeViewport ends before the bottom navigation and exposes a battle board wider than 900 reference pixels.
- Existing warehouse/backpack/map data bindings and challenge/travel input ownership tests stay green.
- Cold UBT, Workbench Automation, headless script tests, real PIE scrolling, and the listed visual checks all pass.
- `L_Main.umap`, untracked user art, 3D town rollback assets, and default-entry routing are untouched by this batch.

Passing this checkpoint does not complete the full desktop-training goal. PSD icon production, remaining workbench semantics, progression/reward/save coverage, performance envelopes, and final default-entry approval remain separate acceptance packages.
