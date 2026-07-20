# Central Battle HUD Projection Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace invisible actor-owned resource/status widgets with one Board-owned projected UMG HUD that keeps every living battle unit’s HP, MP, armor and statuses readable under its scene visual.

**Architecture:** `UGameXXKBattleBoardWidget` gains a non-intercepting `BattleProjectedUnitHudLayer`, keyed by stable `UnitId`. It builds view data directly from `CardRun.ActiveBattle.Units`, reuses the existing player-controller UnitId→UMG projection bridge for geometry, and hosts a composite unit widget that reuses the current resource and status widgets. Scene actors retain only visual, hit and feedback responsibilities; no actor owns a health/status `UWidgetComponent`.

**Tech Stack:** Unreal Engine 5.8 C++, native UMG, existing GameXXK MVP/CardRun runtime, UE MCP, UBT cold compile, Python real-PIE probe/harness, Unreal Automation Tests.

---

## Execution guardrails

- Work in `D:\UE5 demo\GameXXK` on `main`; do not create a worktree.
- Preserve all unrelated dirty files and user-tuned assets. Stage only files named by each task.
- Do not use UnrealBridge, Live Coding, Hot Reload, a `.sln` launcher, or `dotnet` directly to open the project.
- Before any C++ build, save dirty packages through UE MCP. If MCP cannot save a running editor, stop and ask the user rather than force-close it.
- Every C++ verification uses a cold UBT build followed by a fresh direct `.uproject` editor launch through `scripts/ue_tdd_pipeline.py` or the equivalent direct `Build.bat` command.
- This plan does not modify `.umap`, PaperZD assets, character sprites, PSD sources, placed actor transforms, or camera assets.

## File structure and ownership

| File | Change | Responsibility after migration |
| --- | --- | --- |
| `Source/GameXXK/Public/Private/GameXXKBattlePresentation.*` | Modify | Pure `FGameXXKBattleUnitHudView` construction from authoritative card runtime. |
| `Source/GameXXK/Public/Private/UI/GameXXKBattleUnitHudWidget.*` | Create | One input-transparent composite information plate: resource row + status row. |
| `Source/GameXXK/Public/Private/UI/GameXXKBattleStatusIconWidget.*` | Modify | Keep Hover tooltip while explicitly passing mouse-down through to world targeting. |
| `Source/GameXXK/Public/Private/UI/GameXXKBattleBoardWidget.*` | Modify | Own Canvas layer, UnitId→widget map, safe placement and test inspection seams. |
| `Source/GameXXK/Public/Private/MVP/GameXXKBattleSceneUnitActor.*` | Modify | Remove every resource/status WidgetComponent and expose only a computed world HUD foot anchor. |
| `Source/GameXXK/Public/Private/MVP/GameXXKMVPPlayerController.*` | Modify | Submit both existing center projection (arrow) and new foot projection (HUD) to Board. |
| `Source/GameXXK/Private/Tests/GameXXKCardBattleAdapterTest.cpp` | Modify | Assert authority-first `BattleHudUnitView` creation and P-slot identity. |
| `Source/GameXXK/Private/Tests/GameXXKBattleUnitHudWidgetTest.cpp` | Create | Assert composite widget text, MP policy, armor/status subwidgets and input transparency. |
| `Source/GameXXK/Private/Tests/GameXXKBattleStatusEffectsWidgetTest.cpp` | Modify | Assert status-tooltip icon click handling remains non-consuming. |
| `Source/GameXXK/Private/Tests/GameXXKBattleProjectedUnitHudTest.cpp` | Create | Assert Board child lifecycle, UnitId mapping, deterministic safe layout and state mutation refresh. |
| `Source/GameXXK/Private/Tests/GameXXKBattleSceneActorHudTest.cpp` | Modify | Retire actor WidgetComponent assertions; retain actor runtime/feedback assertions. |
| `Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp` | Modify | Assert computed foot anchor and center/foot projection contracts. |
| `Source/GameXXK/Private/Tests/GameXXKBattleActorHudRetirementTest.cpp` | Modify | Positively assert Board-owned overlay and preserve targeting-arrow/no-interaction bridge regression coverage. |
| `Source/GameXXK/Private/Tests/GameXXKBattleBoardWidgetTest.cpp` | Modify | State that scene actors own visuals while Board owns non-intercepting projected HUD overlays. |
| `Content/Python/gamexxk_probe_real_play_flow.py` | Modify | Normalize fixture results and read `battle_board.unit_huds`, not actor WidgetComponents. |
| `scripts/test_gamexxk_real_play_flow_probe.py` | Modify | Test the new probe payload and empty-string UE Python return contract. |
| `scripts/gamexxk_real_play_flow_mcp.py` | Modify | Join actor identity to Board `unit_huds`; validate central geometry and rendered values. |
| `scripts/test_gamexxk_real_play_flow_mcp.py` | Modify | Supply the new Board-centric fixture payload and verdict assertions. |

## Task 1: Repair the real-PIE fixture bridge before using it as HUD evidence

**Files:**

- Modify: `Content/Python/gamexxk_probe_real_play_flow.py:544-552`
- Modify: `scripts/test_gamexxk_real_play_flow_probe.py`

- [ ] **Step 1: Write failing pure-Python coverage for UE’s empty-string success return.**

Add this to `RealPlayFlowProbeTest` after loading the probe module:

```python
def test_fixture_result_treats_single_empty_out_string_as_success(self) -> None:
    module = _load_probe_module()

    self.assertEqual((True, ""), module._fixture_apply_result(""))
    self.assertEqual((True, "detail"), module._fixture_apply_result("detail"))
    self.assertEqual((False, ""), module._fixture_apply_result(None))
    self.assertEqual((True, ""), module._fixture_apply_result((True, "")))
    self.assertEqual((False, "rejected"), module._fixture_apply_result((False, "rejected")))
```

- [ ] **Step 2: Run the focused test and confirm the first assertion fails on the current `bool("")` behavior.**

Run:

```powershell
python scripts/test_gamexxk_real_play_flow_probe.py
```

Expected: the empty-string case fails while the pre-existing source-contract tests still execute.

- [ ] **Step 3: Normalize UE Python return variants without treating an empty out string as a failed bool.**

Replace `_fixture_apply_result` with this branch order:

```python
def _fixture_apply_result(value):
    """Normalize UE Python's bool/out-parameter return variants."""
    if value is None:
        return False, ""
    if isinstance(value, str):
        # UE 5.8 returns the sole FString out parameter directly for a bool + one-out call.
        # An empty string is therefore a successful call with no error text.
        return True, value
    if isinstance(value, (tuple, list)):
        ok = bool(value[0]) if value else False
        error = "" if len(value) < 2 or value[1] is None else str(value[1])
        return ok, error
    if isinstance(value, dict):
        return (
            bool(value.get("return_value", value.get("ok", False))),
            str(value.get("out_error", value.get("error", "")) or ""),
        )
    return bool(value), ""
```

- [ ] **Step 4: Re-run the focused test.**

Run:

```powershell
python scripts/test_gamexxk_real_play_flow_probe.py
```

Expected: exit code `0`; fixture-result cases and existing probe contract tests pass.

- [ ] **Step 5: Commit only the bridge repair.**

```powershell
git add -- Content/Python/gamexxk_probe_real_play_flow.py scripts/test_gamexxk_real_play_flow_probe.py
git commit -m "fix: normalize UE fixture out-string results"
```

## Task 2: Add an authority-first HUD view and composite unit widget

**Files:**

- Modify: `Source/GameXXK/Public/GameXXKBattlePresentation.h`
- Modify: `Source/GameXXK/Private/GameXXKBattlePresentation.cpp`
- Create: `Source/GameXXK/Public/UI/GameXXKBattleUnitHudWidget.h`
- Create: `Source/GameXXK/Private/UI/GameXXKBattleUnitHudWidget.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKBattleStatusIconWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleStatusIconWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBattleAdapterTest.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKBattleUnitHudWidgetTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleStatusEffectsWidgetTest.cpp`

- [ ] **Step 1: Add failing tests for a `UnitId`-keyed view and the composite widget.**

In `GameXXKCardBattleAdapterTest.cpp`, use an existing active-card-battle fixture and add checks equivalent to:

```cpp
FGameXXKBattleUnitHudView HeroView;
TestTrue(TEXT("HUD view resolves a stable hero UnitId"),
    FGameXXKBattlePresentation::BuildUnitHudView(
        PresentationRuntime, TEXT("Player"), FText::FromString(TEXT("主角")), HeroView));
TestEqual(TEXT("HUD view preserves UnitId"), HeroView.UnitId, FName(TEXT("Player")));
TestEqual(TEXT("HUD view uses fixed hero P slot"), HeroView.SlotNumber, 1);
TestEqual(TEXT("HUD view reads card HP"), HeroView.CurrentHP, 72);
TestEqual(TEXT("HUD view reads card MP"), HeroView.CurrentMana, 18);
TestEqual(TEXT("HUD view reads card armor"), HeroView.Armor, 7);
TestTrue(TEXT("HUD view keeps poison stacks"), HeroView.Statuses.ContainsByPredicate([](const FGameXXKCardStatusStack& Stack)
{
    return Stack.Status == EGameXXKCardStatus::Poison && Stack.Stacks == 2;
}));
```

Create `GameXXKBattleUnitHudWidgetTest.cpp` with one hero and one enemy view. Require a native tree, `SelfHitTestInvisible` root, resource/status child widgets, hero text `气血 72 / 100` and `内力 18 / 30`, an armor/poison badge, and a collapsed enemy MP row.

- [ ] **Step 2: Cold compile and run the two automation tests to prove the new type/widget symbols are absent.**

Save through MCP, close via the approved pipeline, then run:

```powershell
& "D:\UE_5.8\Engine\Build\BatchFiles\Build.bat" GameXXKEditor Win64 Development -Project="D:\UE5 demo\GameXXK\GameXXK.uproject" -NoHotReload
```

Expected: build fails specifically because `FGameXXKBattleUnitHudView`, `BuildUnitHudView`, or `UGameXXKBattleUnitHudWidget` does not yet exist. Do not use Live Coding or Hot Reload.

- [ ] **Step 3: Define the pure view contract in `GameXXKBattlePresentation`.**

Add this adjacent to `FGameXXKBattlePresentationSlot`:

```cpp
struct GAMEXXK_API FGameXXKBattleUnitHudView
{
    FName UnitId = NAME_None;
    EGameXXKCardTargetSide Side = EGameXXKCardTargetSide::Invalid;
    EGameXXKCharacterRole Role = EGameXXKCharacterRole::Invalid;
    FText DisplayName;
    int32 SlotNumber = INDEX_NONE;
    bool bLiving = false;
    bool bShowMana = false;
    int32 CurrentHP = 0;
    int32 MaxHP = 0;
    int32 CurrentMana = 0;
    int32 MaxMana = 0;
    int32 Armor = 0;
    TArray<FGameXXKCardStatusStack> Statuses;
};
```

Add this exact public builder declaration:

```cpp
static bool BuildUnitHudView(
    const FGameXXKCardBattleRuntime& Runtime,
    FName UnitId,
    const FText& DisplayName,
    FGameXXKBattleUnitHudView& OutView);
```

Its implementation finds the `FGameXXKCardCombatUnit` by `UnitId`, returns `false` for a missing/none ID, and copies all numerical/status fields only from that card-runtime unit. It assigns `SlotNumber = GetSlotNumber(Runtime, UnitId)` and `bShowMana = Unit->Side == EGameXXKCardTargetSide::Party`. `DisplayName` is an allowed display-only input; it never supplies gameplay numbers.

- [ ] **Step 4: Implement `UGameXXKBattleUnitHudWidget` as an ordinary UMG child, not a component widget.**

Expose this minimal contract:

```cpp
UCLASS()
class GAMEXXK_API UGameXXKBattleUnitHudWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void SetUnitView(const FGameXXKBattleUnitHudView& InView);
    bool PrepareForBoardEmbedding();
    UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
    FName GetUnitIdForTest() const;
    UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
    EGameXXKCardTargetSide GetSideForTest() const;
    UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
    int32 GetSlotNumberForTest() const;
    UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
    UGameXXKBattleUnitResourceWidget* GetResourceWidgetForTest() const;
    UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
    UGameXXKBattleUnitStatusEffectsWidget* GetStatusEffectsWidgetForTest() const;
    static ESlateVisibility GetRootHitTestVisibilityForTest();
};
```

`PrepareForBoardEmbedding()` must initialize one `UVerticalBox`, construct `UGameXXKBattleUnitResourceWidget` and `UGameXXKBattleUnitStatusEffectsWidget` as children, call their existing `PrepareForScreenSpaceEmbedding()` helpers, and set the wrapper/root to `SelfHitTestInvisible`. `SetUnitView()` uses `FGameXXKBattlePresentation::FormatSlotLabel`, then calls the existing `SetUnitVitals()` and `SetStatusEffects()` APIs. It collapses itself when `!InView.bLiving`.

Add an explicit non-consuming status-icon mouse-down path so Hover Tooltip remains available without blocking world targeting:

```cpp
// GameXXKBattleStatusIconWidget.h
#include "Input/Reply.h"

protected:
    virtual FReply NativeOnMouseButtonDown(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent) override;

// GameXXKBattleStatusIconWidget.cpp
FReply UGameXXKBattleStatusIconWidget::NativeOnMouseButtonDown(
    const FGeometry& InGeometry,
    const FPointerEvent& InMouseEvent)
{
    return FReply::Unhandled();
}
```

Expose a development-only boolean seam named `DoesMouseDownPassThroughForTest()` that returns `true` only for this contract, and assert it together with the existing visible Hover target and `HitTestInvisible` tooltip in `GameXXKBattleStatusEffectsWidgetTest.cpp`.

- [ ] **Step 5: Run focused automation tests after a cold build.**

Run the cold build above, then run:

```powershell
& "D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UE5 demo\GameXXK\GameXXK.uproject" -Unattended -NoSplash -NoSound -NullRHI "-ExecCmds=Automation RunTests GameXXK.UI.Battle;Quit" "-TestExit=Automation Test Queue Empty" -log -stdout -FullStdOutLogOutput
& "D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UE5 demo\GameXXK\GameXXK.uproject" -Unattended -NoSplash -NoSound -NullRHI "-ExecCmds=Automation RunTests GameXXK.Integration.CardBattle.Adapter;Quit" "-TestExit=Automation Test Queue Empty" -log -stdout -FullStdOutLogOutput
```

Expected: all selected tests report `Result={Success}`; never replace these tests with a Live Coding check.

- [ ] **Step 6: Commit the data/widget unit.**

```powershell
git add -- Source/GameXXK/Public/GameXXKBattlePresentation.h Source/GameXXK/Private/GameXXKBattlePresentation.cpp Source/GameXXK/Public/UI/GameXXKBattleUnitHudWidget.h Source/GameXXK/Private/UI/GameXXKBattleUnitHudWidget.cpp Source/GameXXK/Public/UI/GameXXKBattleStatusIconWidget.h Source/GameXXK/Private/UI/GameXXKBattleStatusIconWidget.cpp Source/GameXXK/Private/Tests/GameXXKCardBattleAdapterTest.cpp Source/GameXXK/Private/Tests/GameXXKBattleUnitHudWidgetTest.cpp Source/GameXXK/Private/Tests/GameXXKBattleStatusEffectsWidgetTest.cpp
git commit -m "feat: add authority-first battle unit HUD widget"
```

## Task 3: Add the Board-owned Canvas layer, UnitId map and safe projected layout

**Files:**

- Modify: `Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKBattleProjectedUnitHudTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleActorHudRetirementTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleBoardWidgetTest.cpp`

- [ ] **Step 1: Write failing Board tests for child lifecycle, data ownership and placement.**

Create `GameXXKBattleProjectedUnitHudTest.cpp` with a card runtime containing hero `72/100, 18/30, armor 7, poison 2`, partner, quest NPC and an enemy with poison/bleed. Assert:

```cpp
TestEqual(TEXT("one Board child per living UnitId"), Board->GetProjectedUnitHudCountForTest(), 4);
TestNotNull(TEXT("hero HUD is found by UnitId"), Board->GetProjectedUnitHudForTest(TEXT("Player")));
TestEqual(TEXT("enemy HUD has no MP row"),
    Board->GetProjectedUnitHudForTest(TEXT("Enemy.BlackBear"))->GetResourceWidgetForTest()->GetManaRowVisibilityForTest(),
    ESlateVisibility::Collapsed);
```

Use the new deterministic layout seam with `CanvasSize(1920, 1080)`, `Anchor(960, 400)`, `WidgetSize(320, 158)` and invalid obstacle rectangles. Assert `Rect.Min == FVector2D(800, 408)` and `Rect.Max == FVector2D(1120, 566)`. Add a second input where the hand rectangle intersects the first candidate and assert the returned rectangle moves upward, keeps the same X center, and does not overlap hand/Qi/end-turn rectangles.

Update `GameXXKBattleActorHudRetirementTest` from negative “no Board footer” checks to positive checks for `BattleProjectedUnitHudLayer`, a hero child found by `UnitId`, and transparent root visibility. Retain its targeting-arrow and zero-`UWidgetInteractionComponent` checks.

- [ ] **Step 2: Cold compile and observe missing Board APIs/tests.**

Save through MCP, run the same cold `Build.bat` command, and confirm the compiler reports the missing Board layer/map/layout APIs or projected-HUD test class.

- [ ] **Step 3: Add the Board layer and explicit test seams.**

Add fields and APIs with these names:

```cpp
UPROPERTY(Transient)
TObjectPtr<UCanvasPanel> BattleProjectedUnitHudLayer;

UPROPERTY(Transient)
TMap<FName, TObjectPtr<UGameXXKBattleUnitHudWidget>> ProjectedUnitHuds;

UPROPERTY(Transient)
TMap<FName, FVector2D> RegisteredBattleUnitHudScreenPositions;

void RegisterBattleUnitHudScreenPosition(FName UnitId, FVector2D ScreenPosition);
void RefreshProjectedUnitHuds();
void RefreshProjectedUnitHudPositions();

UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
UCanvasPanel* GetBattleProjectedUnitHudLayerForTest() const;

UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
UGameXXKBattleUnitHudWidget* GetProjectedUnitHudForTest(FName UnitId) const;

int32 GetProjectedUnitHudCountForTest() const;

UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
FVector2D GetProjectedUnitHudAnchorPositionForTest(FName UnitId) const;
```

`BuildProgrammaticLayout()` creates `BattleProjectedUnitHudLayer` once, adds it to `RootCanvas`, and sets `SelfHitTestInvisible`. `RefreshFromState()` calls `RefreshProjectedUnitHuds()` after current card/intent/party-Qi refreshes. `NativeTick()` calls only `RefreshProjectedUnitHudPositions()`; it must not mutate card state.

Build each view by iterating `State.CardRun.ActiveBattle.Units`. Use `FGameXXKBattlePresentation::BuildUnitHudView()` for numeric/status data. Resolve `DisplayName` by matching the existing legacy party/enemy projection only by `Id`; if no display name exists, use `FText::FromName(UnitId)`. Do not read legacy HP, MP, Shield or defeat state.

- [ ] **Step 4: Implement the safe-layout helper and Canvas placement.**

Add a pure layout result/seam to the Board header:

```cpp
struct FGameXXKBattleProjectedUnitHudLayout
{
    FVector2D SlotPosition = FVector2D::ZeroVector;
    FBox2D Rect = FBox2D(EForceInit::ForceInit);
    bool bLiftedForObstacle = false;
};

FGameXXKBattleProjectedUnitHudLayout ResolveProjectedUnitHudLayoutForTest(
    FVector2D Anchor,
    FVector2D WidgetSize,
    FVector2D CanvasSize,
    const FBox2D& HandRect,
    const FBox2D& QiRect,
    const FBox2D& EndTurnRect,
    const FBox2D& ShowcaseRect) const;
```

The production helper uses the same calculation. Start at `Anchor.X - WidgetSize.X * 0.5f, Anchor.Y + 8.0f`; clamp only viewport edges horizontally; if it intersects any valid obstacle, move it vertically to `Obstacle.Min.Y - 10.0f - WidgetSize.Y` and recheck all obstacles. Set Canvas-slot alignment to `(0, 0)`, `Size` to the desired widget size and z-order below card tooltip/arrow layers. Normal in-bounds actors retain the anchor center within two pixels; only edge clamping may alter X.

Get hand/Qi/end-turn rectangles from existing Board slots and `ResolvePartyQiLayout`; derive the showcase rectangle from `EnemyIntentShowcaseCard` when visible. If `CanvasSize` or a UnitId projection is invalid, collapse the child for that frame rather than retaining stale coordinates.

- [ ] **Step 5: Run Board-focused automation after a cold build.**

Run the cold build, then:

```powershell
& "D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UE5 demo\GameXXK\GameXXK.uproject" -Unattended -NoSplash -NoSound -NullRHI "-ExecCmds=Automation RunTests GameXXK.UI.Battle;Quit" "-TestExit=Automation Test Queue Empty" -log -stdout -FullStdOutLogOutput
& "D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UE5 demo\GameXXK\GameXXK.uproject" -Unattended -NoSplash -NoSound -NullRHI "-ExecCmds=Automation RunTests GameXXK.MVP.Battle.BoardWidget;Quit" "-TestExit=Automation Test Queue Empty" -log -stdout -FullStdOutLogOutput
```

Expected: all projected-layer, retirement and Board tests report success; hand/Qi behavior remains covered.

- [ ] **Step 6: Commit the Board-owned HUD layer.**

```powershell
git add -- Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp Source/GameXXK/Private/Tests/GameXXKBattleProjectedUnitHudTest.cpp Source/GameXXK/Private/Tests/GameXXKBattleActorHudRetirementTest.cpp Source/GameXXK/Private/Tests/GameXXKBattleBoardWidgetTest.cpp
git commit -m "feat: project unit HUDs through battle board"
```

## Task 4: Retire Actor WidgetComponents and extend the existing projection bridge

**Files:**

- Modify: `Source/GameXXK/Public/MVP/GameXXKBattleSceneUnitActor.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKBattleSceneUnitActor.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleSceneActorHudTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp`

- [ ] **Step 1: Replace actor component assertions with a failing computed-anchor contract.**

Remove assertions requiring `HudAnchor`, resource/status anchors, `EWidgetSpace::Screen`, DrawSize, Pivot, shared layer, WidgetClass and component visibility. Add assertions equivalent to:

```cpp
TestNull(TEXT("battle actor owns no WidgetComponent"), HeroActor->FindComponentByClass<UWidgetComponent>());
const FVector FootAnchor = HeroActor->GetBattleHudProjectionWorldLocation();
TestFalse(TEXT("computed HUD foot anchor is finite"), FootAnchor.ContainsNaN());
TestTrue(TEXT("computed HUD foot anchor is below the visual center"),
    FootAnchor.Z <= HeroActor->GetBattleVisualComponent()->Bounds.Origin.Z);
```

Preserve checks that `ConfigureFromRuntimeUnit()` reads card-runtime HP/MP/Armor/Statuses, target highlighting does not reset flipbook playback, temporary NPC uses 3P, and actor feedback still restores the visual.

- [ ] **Step 2: Cold compile and confirm that `GetBattleHudProjectionWorldLocation()` is missing.**

Run the approved MCP save + cold `Build.bat` workflow. Expected: compiler failure names the new pure Actor method or retired component checks.

- [ ] **Step 3: Remove all actor-owned resource/status UI and add a pure foot-anchor getter.**

Delete these actor members and their implementation paths:

```cpp
HudAnchorComponent;
ResourceHudAnchorComponent;
StatusEffectsAnchorComponent;
ResourceHudWidgetComponent;
StatusEffectsWidgetComponent;
RefreshHudAnchor();
RefreshResourceHudWidget();
RefreshStatusEffectsWidget();
```

Also remove their forward declarations, test getters, constructor setup, visibility updates and feedback-Tick calls. Keep `ResolveCardRuntimePresentation()` only if actor feedback/legacy scene logic still needs its data cache; it must not create or refresh UI.

Add this public non-component scene query:

```cpp
UFUNCTION(BlueprintPure, Category = "GameXXK|BattleScene")
FVector GetBattleHudProjectionWorldLocation() const;
```

Its implementation returns `GetActorLocation()` if `BattleVisual` is absent; otherwise it returns the visual’s bottom center:

```cpp
const FBoxSphereBounds& Bounds = BattleVisual->Bounds;
return Bounds.Origin - FVector(0.0f, 0.0f, Bounds.BoxExtent.Z);
```

This is a computed query only. It creates no Blueprint component and stores no HUD state.

- [ ] **Step 4: Extend the player-controller bridge without changing card-arrow source behavior.**

Keep `RegisteredBattleUnitScreenPositions` and `RegisterBattleUnitScreenPosition()` for the existing card arrow’s visual-center source. In `RefreshBattleCardTargetingBridge()` project both points:

```cpp
FVector2D CenterPosition;
if (UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(this, UnitCenter, CenterPosition, true))
{
    BattleBoardWidget->RegisterBattleUnitScreenPosition(UnitActor->GetUnitId(), CenterPosition);
}

FVector2D HudFootPosition;
if (UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
        this, UnitActor->GetBattleHudProjectionWorldLocation(), HudFootPosition, true))
{
    BattleBoardWidget->RegisterBattleUnitHudScreenPosition(UnitActor->GetUnitId(), HudFootPosition);
}
```

`ClearBattleUnitScreenPositions()` clears both maps. It must continue to set Actor target highlights using the existing legal UnitId set, and it must not alter current targeting state when a projection is temporarily unavailable.

- [ ] **Step 5: Cold build and run actor/targeting regression tests.**

Run the cold build, then:

```powershell
& "D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UE5 demo\GameXXK\GameXXK.uproject" -Unattended -NoSplash -NoSound -NullRHI "-ExecCmds=Automation RunTests GameXXK.MVP.Battle.SceneActorHud;Quit" "-TestExit=Automation Test Queue Empty" -log -stdout -FullStdOutLogOutput
& "D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UE5 demo\GameXXK\GameXXK.uproject" -Unattended -NoSplash -NoSound -NullRHI "-ExecCmds=Automation RunTests GameXXK.MVP.Battle.SceneActors;Quit" "-TestExit=Automation Test Queue Empty" -log -stdout -FullStdOutLogOutput
& "D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UE5 demo\GameXXK\GameXXK.uproject" -Unattended -NoSplash -NoSound -NullRHI "-ExecCmds=Automation RunTests GameXXK.UI.Battle.ActorHudRetirement;Quit" "-TestExit=Automation Test Queue Empty" -log -stdout -FullStdOutLogOutput
& "D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UE5 demo\GameXXK\GameXXK.uproject" -Unattended -NoSplash -NoSound -NullRHI "-ExecCmds=Automation RunTests GameXXK.Integration.CardBattle.ControllerInputBridge;Quit" "-TestExit=Automation Test Queue Empty" -log -stdout -FullStdOutLogOutput
```

Expected: all actor/targeting tests report success and no test refers to resource/status WidgetComponents.

- [ ] **Step 6: Commit the actor retirement and dual-projection bridge.**

```powershell
git add -- Source/GameXXK/Public/MVP/GameXXKBattleSceneUnitActor.h Source/GameXXK/Private/MVP/GameXXKBattleSceneUnitActor.cpp Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp Source/GameXXK/Private/Tests/GameXXKBattleSceneActorHudTest.cpp Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp
git commit -m "refactor: retire actor-owned battle HUD components"
```

## Task 5: Move the real-PIE probe and verdict to Board-owned overlay evidence

**Files:**

- Modify: `Content/Python/gamexxk_probe_real_play_flow.py:696-1115`
- Modify: `scripts/test_gamexxk_real_play_flow_probe.py`
- Modify: `scripts/gamexxk_real_play_flow_mcp.py`
- Modify: `scripts/test_gamexxk_real_play_flow_mcp.py`

- [ ] **Step 1: Write failing Python tests for the Board-centric payload.**

Replace the old source assertions for `_widget_component_summary` with assertions that the probe defines and uses:

```python
def _battle_projected_unit_hud_summary(world, board, unit_id):
    ...

"unit_hud_layer"
"unit_huds"
"projected_anchor"
"screen_rect"
"rendered"
```

Require that the actor-summary slice no longer contains `get_resource_hud_widget_component_for_test` or `get_status_effects_widget_component_for_test`. Add fixture-verdict cases in `test_gamexxk_real_play_flow_mcp.py` for a missing Board unit HUD, an offscreen rect, a stale/mismatched UnitId, a health/MP mismatch, an enemy MP row that is visible, and a hero status row missing `毒 2`.

- [ ] **Step 2: Run the Python tests and verify they fail before probe migration.**

Run:

```powershell
python scripts/test_gamexxk_real_play_flow_probe.py
python scripts/test_gamexxk_real_play_flow_mcp.py
```

Expected: failures identify the old actor WidgetComponent payload assumptions.

- [ ] **Step 3: Implement Board-based probe output.**

Keep `_screen_rect(world, widget)` and `_widget_screen_summary(world, widget)`. Delete `_widget_component_summary` from battle HUD collection. Extend `_battle_board_summary()` to emit this exact top-level shape:

```python
{
    "path": _object_path(board),
    "class": _class_path(board),
    "visible": bool_or_none,
    "visibility": enum_name_or_none,
    "shared_energy": shared_energy,
    "party_qi": widget_summary,
    "hand_card_box": widget_summary,
    "end_turn_button": widget_summary,
    "unit_hud_layer": widget_summary,
    "unit_huds": {unit_id: unit_hud_summary},
    "errors": [],
}
```

For each scene Actor `UnitId`, ask the Board for `GetProjectedUnitHudForTest(UnitId)`, read the composite widget’s resource/status children using their existing test APIs, and include the Board-recorded projected anchor. The `unit_huds` map—not the Actor—is the source for `screen_rect`, `rendered`, `mana_row_visible`, icon IDs and icon stack labels.

Change the MCP harness join to `actor.unit_id -> battle_board.unit_huds[unit_id]`. A passing verdict requires: a visible layer; exactly one visible in-viewport HUD for every living scene Actor; no orphan UnitId entries; valid resource/status ordering; X-center within two pixels of projected anchor except an explicitly reported viewport-edge clamp; no overlap with hand/Qi/end-turn/showcase; and rendered fixture values matching the authoritative expected values. Existing card arrow, Party Qi and screenshot crop checks remain, but crop from the Board unit HUD rect.

- [ ] **Step 4: Re-run Python tests.**

Run:

```powershell
python scripts/test_gamexxk_real_play_flow_probe.py
python scripts/test_gamexxk_real_play_flow_mcp.py
```

Expected: both commands exit `0`; every fixture shape is Board-owned and no test requires a `UWidgetComponent` screen rectangle.

- [ ] **Step 5: Commit the probe/harness migration.**

```powershell
git add -- Content/Python/gamexxk_probe_real_play_flow.py scripts/test_gamexxk_real_play_flow_probe.py scripts/gamexxk_real_play_flow_mcp.py scripts/test_gamexxk_real_play_flow_mcp.py
git commit -m "test: verify battle HUD through board overlay"
```

## Task 6: Perform cold-build, automation and visual acceptance

**Files:**

- Modify only if an acceptance failure exposes a concrete defect in a prior task.
- Evidence: `Saved/Codex/` screenshots and the real-play harness report.

- [ ] **Step 1: Check the working tree and save the running editor through UE MCP.**

Run a targeted status review. If the editor is running, call the project’s MCP save path; only proceed when it reports no dirty packages. Do not use a force-kill fallback if MCP is unavailable.

- [ ] **Step 2: Run a full cold compile and fresh MCP editor launch.**

Run:

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 8
```

Expected: UBT succeeds with `-NoHotReload`, a direct `GameXXK.uproject` editor process starts with MCP on port `18765`, PIE starts and stops cleanly, and the report has no failed `[TDD]` lines. This is compile evidence; it is not a substitute for the focused automation tests.

- [ ] **Step 3: Run the focused C++ automation suite in a fresh commandlet editor.**

Run each filter separately so a bad filter cannot hide another test:

```powershell
& "D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UE5 demo\GameXXK\GameXXK.uproject" -Unattended -NoSplash -NoSound -NullRHI "-ExecCmds=Automation RunTests GameXXK.UI.Battle;Quit" "-TestExit=Automation Test Queue Empty" -log -stdout -FullStdOutLogOutput
& "D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UE5 demo\GameXXK\GameXXK.uproject" -Unattended -NoSplash -NoSound -NullRHI "-ExecCmds=Automation RunTests GameXXK.MVP.Battle;Quit" "-TestExit=Automation Test Queue Empty" -log -stdout -FullStdOutLogOutput
& "D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\UE5 demo\GameXXK\GameXXK.uproject" -Unattended -NoSplash -NoSound -NullRHI "-ExecCmds=Automation RunTests GameXXK.Integration.CardBattle;Quit" "-TestExit=Automation Test Queue Empty" -log -stdout -FullStdOutLogOutput
```

Expected: no `Automation Test Failed`, no `No automation tests matched`, and every selected result is `Success`.

- [ ] **Step 4: Exercise the real player flow, then use the repaired fixture for readable-state evidence.**

Use `scripts/gamexxk_real_play_flow_mcp.py` to travel Main Menu → Qingshan town → accept quest → route node → battle. In battle invoke `Content/Python/gamexxk_probe_real_play_flow.py --apply-battle-hud-fixture`, capture the returned Board `unit_huds`, and save a screenshot under `Saved/Codex/`.

Required evidence in the same report/screenshot pair:

```text
hero/partner/NPC: HP + MP rows visible under their matching scene sprites
enemy: HP + armor/status visible; MP row collapsed
statuses: poison/bleed icon count and stack labels match fixture
party Qi: still right of hand, separate from MP
all living UnitIds: Board rect valid, in viewport, centered on current projected anchor
no HUD rect: overlaps hand, Party Qi, end turn, or central showcased card
```

- [ ] **Step 5: Verify feedback re-projection and cleanup.**

Use one targeted card or enemy intent, then take a second probe/screenshot during or immediately after actor feedback. Confirm the same UnitId’s overlay moves with the fresh projected anchor and its health/status values refresh after the mutation. Clear the fixture with `--clear-battle-hud-fixture`, confirm the raw battle view no longer contains fixture values, and stop PIE cleanly through MCP.

- [ ] **Step 6: Commit only verified implementation files and the acceptance evidence summary.**

Before committing, run `git diff --check`, review exactly staged paths, and ensure no user assets are staged. Commit with:

```powershell
git add -- Source/GameXXK Content/Python/gamexxk_probe_real_play_flow.py scripts/gamexxk_real_play_flow_mcp.py scripts/test_gamexxk_real_play_flow_probe.py scripts/test_gamexxk_real_play_flow_mcp.py
git commit -m "feat: render projected battle unit HUD"
```

If prior tasks already committed all production changes, commit only a concise acceptance record in `docs/production/` that lists the actual build command, automation filters, harness report path and screenshot path. Do not claim visual completion without both the probe data and screenshot.
