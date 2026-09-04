# Desktop HUD Manual Scale and Native Region Alignment Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove resolution-derived HUD scaling, expose manual 50%/75%/100% settings, and prove that the rendered HUD and Win32 native region share one physical origin.

**Architecture:** `GameXXKDesktopTrainingLayout` remains the single owner of desktop HUD scale and native-region coordinate transforms. Work-area dimensions continue to select and position the host, while manual scale alone determines HUD size. The overlay `SWindow` manually manages DPI, so Slate content, input, and the native region use the same client-coordinate units without another monitor-DPI conversion. The fixed maximum transparent host remains unchanged.

**Tech Stack:** Unreal Engine 5.8 C++, UMG/Slate, Win32 `SetWindowRgn`, UE Automation Framework, project cold UBT and UE MCP scripts.

---

### Task 1: Replace adaptive scale with the manual scale contract

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp:1469-1511`
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingLayout.h:103-106`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingLayout.cpp:116-170`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp:8326-8330`

- [ ] **Step 1: Rewrite the adaptive-scale assertions as a failing manual-scale test**

Rename the test to `FGameXXKDesktopTrainingManualWindowScaleTest` / `GameXXK.DesktopTraining.Workbench.ManualWindowScaleAndDirection`. Replace the adaptive assertions with:

```cpp
const FVector2D WorkAreas[] = {
	FVector2D(1280.0f, 680.0f),
	FVector2D(1536.0f, 816.0f),
	FVector2D(1920.0f, 1020.0f),
	FVector2D(2560.0f, 1400.0f)};
for (const FVector2D& WorkArea : WorkAreas)
{
	TestTrue(
		*FString::Printf(TEXT("100 percent remains authored size at %.0fx%.0f"), WorkArea.X, WorkArea.Y),
		FMath::IsNearlyEqual(ResolveDesktopHudMetrics(WorkArea, 100).Scale, 1.0f));
	TestTrue(
		*FString::Printf(TEXT("50 percent remains half size at %.0fx%.0f"), WorkArea.X, WorkArea.Y),
		FMath::IsNearlyEqual(ResolveDesktopHudMetrics(WorkArea, 50).Scale, 0.5f));
}
TestEqual(TEXT("collapsed HUD authored size is 1038x202"),
	GetCollapsedHudLogicalSize(),
	FVector2D(1038.0f, 202.0f));
TestEqual(TEXT("50 percent collapsed HUD resolves to 519x101"),
	GetCollapsedHudLogicalSize()
		* ResolveDesktopHudMetrics(FVector2D(1920.0f, 1020.0f), 50).Scale,
	FVector2D(519.0f, 101.0f));
const FDesktopOverlayPlacement SmallWorkAreaMaximumHost =
	ComputeDesktopOverlayPlacement(
		FVector2D(1536.0f, 816.0f),
		FVector2D(0.5f, 0.08f),
		100,
		true,
		false,
		0.0f,
		true);
TestEqual(TEXT("100 percent maximum host may exceed a small work area"),
	SmallWorkAreaMaximumHost.HudSize,
	FVector2D(1820.0f, 941.0f));
```

Keep the existing upward/downward expansion assertions below this block.

- [ ] **Step 2: Run the focused test and verify RED**

Run:

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 160
python scripts/ai_production_loop.py --run-automation --automation-tests GameXXK.DesktopTraining.Workbench.ManualWindowScaleAndDirection --automation-report DesktopHudManualScale_RED --fail-on-automation-warnings --json
```

Expected: UBT succeeds, then the focused automation test fails because the existing scale still varies with the work-area dimensions. The failure must name at least one 1280, 1536, or 2560 manual-scale assertion.

- [ ] **Step 3: Implement the manual scale resolver and remove the automatic helper**

Replace the two old declarations with:

```cpp
GAMEXXK_API float ResolveManualHudScale(int32 HudScalePercent);
```

Remove `BaselineLogicalWorkAreaSize`, `MaximumAutomaticHudScale`, and `WorkAreaSafetyMargin`. Replace both old scale functions with:

```cpp
float ResolveManualHudScale(const int32 HudScalePercent)
{
	return HudScalePercent <= 50 ? 0.5f : 1.0f;
}
```

Set `FDesktopHudResolvedMetrics::Scale` with:

```cpp
Result.Scale = ResolveManualHudScale(HudScalePercent);
```

In `UpdateDesktopOverlayAnchorFromPointer`, replace the fallback call with:

```cpp
const float Scale = bDesktopResolvedMetricsValid
	? DesktopResolvedMetrics.Scale
	: GameXXKDesktopTrainingLayout::ResolveManualHudScale(HudScalePercent);
```

- [ ] **Step 4: Cold-build and verify GREEN**

Run the same two commands from Step 2.

Expected: UBT succeeds without Live Coding, and `ManualWindowScaleAndDirection` passes with no automation warnings.

- [ ] **Step 5: Commit the isolated scale change**

Stage only the manual-scale hunks from the already-dirty test/workbench files, plus the complete clean layout header and implementation. Commit with:

```powershell
git commit -m "fix: use manual desktop HUD scale"
```

Expected: the commit excludes the pre-existing selected-font work and unrelated asset changes.

### Task 2: Share the native-region physical rectangle transform

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp:2187-2242`
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingLayout.h:141-144`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingLayout.cpp:294-437`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp:8571-8591`

- [ ] **Step 1: Add a failing native-region origin assertion**

Append this block to `FGameXXKDesktopTrainingNativeWindowRegionTest` before its final return:

```cpp
const FVector2D FixedContentOffset(271.0f, 49.0f);
constexpr float RegionPadding = 3.0f;
const FVector4 PaddedStripRect = ResolveDesktopNativeRegionRect(
	CollapsedShapes[0],
	FixedContentOffset,
	RegionPadding);
TestEqual(TEXT("native strip begins exactly three physical pixels before the rendered HUD"),
	FVector2D(PaddedStripRect.X, PaddedStripRect.Y),
	FixedContentOffset - FVector2D(RegionPadding, RegionPadding));
TestEqual(TEXT("native strip padding expands both physical axes symmetrically"),
	FVector2D(PaddedStripRect.Z, PaddedStripRect.W),
	FVector2D(1044.0f, 208.0f));
```

- [ ] **Step 2: Cold-build and verify RED**

Run:

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 160
```

Expected: UBT fails because `ResolveDesktopNativeRegionRect` is not declared.

- [ ] **Step 3: Add the shared physical-region transform**

Declare:

```cpp
GAMEXXK_API FVector4 ResolveDesktopNativeRegionRect(
	const FDesktopNativeRegionShape& Shape,
	const FVector2D& HostOffset,
	float Padding);
```

Implement:

```cpp
FVector4 ResolveDesktopNativeRegionRect(
	const FDesktopNativeRegionShape& Shape,
	const FVector2D& HostOffset,
	const float Padding)
{
	const float SafePadding = FMath::Max(0.0f, Padding);
	return FVector4(
		Shape.Rect.X + HostOffset.X - SafePadding,
		Shape.Rect.Y + HostOffset.Y - SafePadding,
		Shape.Rect.Z + SafePadding * 2.0f,
		Shape.Rect.W + SafePadding * 2.0f);
}
```

Use this helper in `ApplyDesktopNativeInputRegion`:

```cpp
const FVector4 Rect =
	GameXXKDesktopTrainingLayout::ResolveDesktopNativeRegionRect(
		Surface,
		HostOffset,
		RegionPadding);
const int32 Left = FMath::FloorToInt(Rect.X);
const int32 Top = FMath::FloorToInt(Rect.Y);
const int32 Right = FMath::CeilToInt(Rect.X + Rect.Z);
const int32 Bottom = FMath::CeilToInt(Rect.Y + Rect.W);
```

- [ ] **Step 4: Cold-build and run focused region tests**

Run:

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 160
python scripts/ai_production_loop.py --run-automation --automation-tests "GameXXK.DesktopTraining.Workbench.NativeWindowRegion+GameXXK.DesktopTraining.Workbench.ManualWindowScaleAndDirection+GameXXK.DesktopTraining.Workbench.StablePresentationScale" --automation-report DesktopHudRegionAlignment_GREEN --fail-on-automation-warnings --json
```

Expected: all three focused tests pass and the region-origin assertion permits only the three-pixel padding.

- [ ] **Step 5: Commit the isolated region change**

Stage only these region-transform hunks from dirty shared files and commit with:

```powershell
git commit -m "fix: align desktop HUD native region"
```

### Task 3: Verify the trial in the canonical desktop HUD

**Files:**
- Update: `docs/production/current-goal-acceptance.md`
- Create: `docs/production/2026-09-04-desktop-hud-manual-scale-trial.md`

- [ ] **Step 1: Run the complete workbench regression scope**

Run:

```powershell
python scripts/ai_production_loop.py --run-automation --automation-tests GameXXK.DesktopTraining.Workbench --automation-report DesktopHudManualScale_Workbench_GREEN --fail-on-automation-warnings --json
```

Expected scoped result: the new scale/region/manual-DPI tests pass without warnings. The full current baseline remains 68/72: `InnerGeometry`, `TravelCombatPresentation`, `TravelPartyAtlasAsyncFallback`, and `TravelPartyAtlasFallbackInventory` are pre-existing failures already recorded in `current-goal-acceptance.md`; do not weaken them for this task.

- [ ] **Step 2: Open the canonical presentation and measure the native window**

Launch `/Game/GameXXK/Maps/L_DesktopTrainingHUD`, open the collapsed desktop overlay at manual 100%, and inspect the Win32 window with `GetWindowRect`, `GetClientRect`, `GetWindowRgn`, and `GetRgnBox`.

Expected on the current 1536x816 work area:

```text
manual scale = 1.0
fixed transparent host = 1820x941
collapsed HUD = 1038x254
native region bounding box = 1044-1045 x 260-261 after integer rounding
native region left/top = rendered HUD left/top - 3px
```

Live execution additionally verified the manual-DPI boundary. Before removing the duplicate 125% conversion, the 1820x941 host and 1044/1045-wide region contained a rendered HUD only about 80% of its authored size. The final implementation removes `DesktopInputDpiScale` and `PhysicalPixelsToSlateHost`, uses `ResolveDesktopSlateHostGeometry` for the UMG slot, and keeps `WM_DPICHANGED`/`WM_DISPLAYCHANGE` only for placement refresh.

- [ ] **Step 3: Inspect visible behavior**

Capture the collapsed HUD, then press Tab and capture the expanded HUD. Confirm the idle strip's left edge is complete, the notice/control rail is not clipped, the transparent background remains clear, closed regions pass mouse input through, and Tab expansion does not expose white pixels.

- [ ] **Step 4: Record the evidence and limitation**

Write the measured window/region dimensions, automation report path, screenshot paths, and the accepted 1536x816 overflow limitation to `docs/production/2026-09-04-desktop-hud-manual-scale-trial.md`. Add a dated pointer to `docs/production/current-goal-acceptance.md` without replacing the active card-progress status.

- [ ] **Step 5: Commit the acceptance record**

```powershell
git commit -m "docs: record desktop HUD manual scale trial"
```

Expected: only the two acceptance documents and intentional evidence metadata are committed.

### Follow-up: Add the 75% manual setting

The user subsequently requested a third manual scale and removal of the obsolete automatic-adaptation sentence. Commit `27318be` adds the 75% action, button, scale resolution, and persistence while retaining 50% and 100%. Commit `09d9034` verifies that a newly created HUD reloads the persisted 75% value. `DesktopHud75Reload_GREEN` passed 2/2 with no warnings or errors.
