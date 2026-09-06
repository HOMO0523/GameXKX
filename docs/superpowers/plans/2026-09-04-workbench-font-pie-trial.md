# Workbench Font PIE Trial Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Show the selected JiangHu Runtime Font on every `UTextBlock` in the canonical pure-2D desktop workbench during PIE while preserving each block's existing size, color, outline, and layout.

**Architecture:** Add one private, file-local typography pass that loads `/Game/GameXXK/UI/Fonts/Trial/FF_Trial_ZhHans_JiangHuGuFeng_Font` and replaces only the `FontObject` and typeface name on text blocks. Run it after initial Slate reconstruction and after programmatic layout refreshes, using `UWidgetTree::ForEachWidgetAndDescendants` so the embedded backpack is included.

**Tech Stack:** UE 5.8 C++, UMG `UTextBlock`, Runtime Composite Font asset, Unreal Automation Tests, cold UBT through `scripts/ue_tdd_pipeline.py`, UE MCP, PIE on `/Game/GameXXK/Maps/L_DesktopTrainingHUD`.

---

### Task 1: Prove the current workbench does not use the selected font

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`

- [x] **Step 1: Add the failing automation test**

Add `GameXXK.DesktopTraining.Workbench.SelectedRuntimeFont`. It creates the workbench, calls `TakeWidget`, walks `WidgetTree->ForEachWidgetAndDescendants`, requires at least one `UTextBlock`, and asserts every text block's `FSlateFontInfo::FontObject` path is:

```text
/Game/GameXXK/UI/Fonts/Trial/FF_Trial_ZhHans_JiangHuGuFeng_Font.FF_Trial_ZhHans_JiangHuGuFeng_Font
```

The test also records the first mismatching widget name and font path for diagnosis.

- [x] **Step 2: Cold-build and verify RED**

Run:

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 120
python scripts/ai_production_loop.py --run-automation --automation-tests GameXXK.DesktopTraining.Workbench.SelectedRuntimeFont --automation-report FontWorkbench_RED --fail-on-automation-warnings --json
```

Expected: UBT succeeds; the focused automation test fails because existing text blocks still use the engine default font.

### Task 2: Apply the selected Runtime Font without altering layout

**Files:**
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`

- [x] **Step 1: Add the minimal typography pass**

In the existing anonymous namespace, load the selected `UFont` through a weak cache. Add a helper that walks `ForEachWidgetAndDescendants`; for each `UTextBlock`, copy its existing `FSlateFontInfo`, set only `FontObject` to the selected Runtime Font and `TypefaceFontName` to `Default`, then call `SetFont`.

Call the pass:

1. after `Super::RebuildWidget()` returns, so nested user-widget trees exist;
2. after `BuildWorkbenchShell()` in `BuildProgrammaticLayout()`, so refresh-created text is updated.

- [x] **Step 2: Cold-build and verify GREEN**

Run:

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 160
python scripts/ai_production_loop.py --run-automation --automation-tests GameXXK.DesktopTraining.Workbench.SelectedRuntimeFont --automation-report FontWorkbench_GREEN --fail-on-automation-warnings --json
```

Expected: UBT succeeds; the focused test passes with no unexpected warnings.

- [x] **Step 3: Inspect the exact diff**

Run `git diff --check` and inspect only the two scoped C++ files. Confirm no font-size, color, geometry, input, gameplay, or localization text changed.

### Task 3: Verify the canonical PIE surface

**Files:**
- Create: `Saved/HarnessReports/20260904-workbench-font-pie-trial.md`

- [x] **Step 1: Start PIE through UE MCP**

Use the editor launched by the GREEN pipeline. Confirm the editor map is `/Game/GameXXK/Maps/L_DesktopTrainingHUD`, start PIE in the viewport, and wait for the workbench to appear.

- [x] **Step 2: Inspect the live workbench**

Capture the visible editor/PIE window and check the top toolbar, buttons, navigation, notices, and embedded backpack. Confirm JiangHu glyph shapes are visible while controls remain aligned and readable.

- [x] **Step 3: Record the boundary**

Record build/test evidence, the Runtime Font path, PIE map, visual observations, and the remaining boundary: BattleBoard and other independent root widgets have not yet been migrated.
