# Battle HUD, Presentation, and VFX Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make battle HUDs visibly correct in real PIE, add team Qi, and implement deterministic enemy-turn, hand-refill, Buff, and Impact presentation driven by real card-resolution events.

**Architecture:** Retain the existing screen-space `UWidgetComponent`s owned by `AGameXXKBattleSceneUnitActor` for per-unit values and add one board-owned `UGameXXKBattlePartyQiWidget` for the team resource. Extend card resolution with non-persistent semantic presentation events; a local Board/Actor presentation queue consumes them without changing rules, saves, or random state. Extend the real-flow probe to observe actual Slate rectangles and active presentation state; acceptance rejects constructed-but-invisible, overlapping, or semantically incorrect widgets/effects.

**Tech Stack:** Unreal Engine 5.8 C++, UMG/Slate, Paper2D, Niagara, UE MCP project Python, UBT cold build, Unreal Automation tests, Python `unittest`.

---

## Locked data rules

| Data | Authority | Visible on | Text |
| --- | --- | --- | --- |
| HP | `CardUnit.HP`, `CardUnit.MaxHP` | every living unit | `气血 当前 / 最大` |
| MP | `CardUnit.Mana`, `CardUnit.MaxMana` | hero, permanent companion, task NPC | `内力 当前 / 最大` |
| Shared Qi | `ActiveBattle.Deck.SharedEnergy` | one board widget only | `气力 当前` + `本回合共享` |
| Armor / status | `CardUnit.Armor`, `CardUnit.Statuses` | every living unit | icon, stacks, hover tooltip |

- `Mana` means **内力**, never 气力.
- Enemies never show an MP row.
- No Actor HUD reads `SharedEnergy`.
- `SharedEnergy` has no player-facing maximum. Never use `current/99`, a max label, or a percentage bar.
- `CardUnit.Armor` becomes authoritative after one opening `LegacyUnit.Shield -> Armor` projection.
- The panel must be passive (`SelfHitTestInvisible`); card, End Turn, and status hover input keep priority.

## Files and ownership

| File | Change |
| --- | --- |
| `Source/GameXXK/Public/Private/UI/GameXXKBattleUnitResourceWidget.*` | Rename personal “Qi” to MP/内力. |
| `Source/GameXXK/Public/Private/MVP/GameXXKBattleSceneUnitActor.*` | Fix screen HUD ownership, anchor, layers, and runtime refresh. |
| `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp` | Preserve opening shield as runtime armor. |
| `Source/GameXXK/Private/UI/GameXXKBattleStatusIconWidget.cpp` | Lift icon size to 44 logical pixels. |
| `Source/GameXXK/Public/Private/UI/GameXXKBattlePartyQiWidget.*` | New passive shared-Qi widget. |
| `Source/GameXXK/Public/Private/UI/GameXXKBattleBoardWidget.*` | Construct and refresh team-Qi beside hand/End Turn. |
| `Content/Python/gamexxk_probe_real_play_flow.py` | Collect component and actual Slate rectangle data. |
| `scripts/gamexxk_real_play_flow_mcp.py` | Enforce a visual HUD verdict and save screenshots/crops. |
| `scripts/test_gamexxk_real_play_flow_*.py` | Contract tests for diagnostics and verdicts. |

### Verification commands

```powershell
& 'C:\Users\shxuw\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' -m unittest scripts.test_gamexxk_real_play_flow_probe scripts.test_gamexxk_real_play_flow_mcp -v
```

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -NoHotReload
```

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.UI.Battle+GameXXK.MVP.Battle+GameXXK.Integration.CardBattle+GameXXK.Data.CardBattleRuntime+GameXXK.Data.CardCombatRules;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

### Task 1: Add the missing real-PIE HUD observation contract

**Files:**

- Modify: `scripts/test_gamexxk_real_play_flow_probe.py`
- Modify: `scripts/test_gamexxk_real_play_flow_mcp.py`
- Modify: `Content/Python/gamexxk_probe_real_play_flow.py`
- Modify: `scripts/gamexxk_real_play_flow_mcp.py`

- [ ] **Step 1: Write failing Python contract tests.**

Add this source-contract test in `scripts/test_gamexxk_real_play_flow_probe.py`:

```python
def test_battle_probe_contract_requires_live_actor_hud_geometry(self) -> None:
    source = _read_project_python("Content/Python/gamexxk_probe_real_play_flow.py")
    for token in (
        "get_resource_hud_widget_component_for_test",
        "get_status_effects_widget_component_for_test",
        "SlateBlueprintLibrary.local_to_viewport",
        '"battle_hud"',
        '"screen_rect"',
        '"mana_row_visible"',
    ):
        self.assertIn(token, source)
```

Add this pure-verdict test in `scripts/test_gamexxk_real_play_flow_mcp.py`:

```python
def test_battle_hud_verdict_rejects_missing_or_offscreen_actor_rects(self) -> None:
    probe = _valid_battle_hud_probe()
    self.assertTrue(real_flow._battle_hud_verdict(probe, {"width": 1288, "height": 770})["ok"])

    probe["units"][0]["resource_hud"]["screen_rect"] = None
    self.assertFalse(real_flow._battle_hud_verdict(probe, {"width": 1288, "height": 770})["ok"])

    probe = _valid_battle_hud_probe()
    probe["units"][0]["resource_hud"]["screen_rect"] = {
        "left": 1400, "top": 20, "right": 1500, "bottom": 80
    }
    self.assertFalse(real_flow._battle_hud_verdict(probe, {"width": 1288, "height": 770})["ok"])
```

- [ ] **Step 2: Run the new Python tests and confirm the diagnostic contract is red.**

Run:

```powershell
& 'C:\Users\shxuw\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' -m unittest scripts.test_gamexxk_real_play_flow_probe scripts.test_gamexxk_real_play_flow_mcp -v
```

Expected: failure because no `battle_hud` payload or `_battle_hud_verdict` exists.

- [ ] **Step 3: Implement the actual Slate rectangle probe and pure verdict.**

In `Content/Python/gamexxk_probe_real_play_flow.py`, add the geometry helpers:

```python
def _screen_rect(world, widget):
    if widget is None:
        return None
    geometry = widget.get_cached_geometry()
    local_size = geometry.get_local_size()
    if local_size.x <= 0.0 or local_size.y <= 0.0:
        return None
    top_left = unreal.SlateBlueprintLibrary.local_to_viewport(
        world, geometry, unreal.Vector2D(0.0, 0.0))
    bottom_right = unreal.SlateBlueprintLibrary.local_to_viewport(
        world, geometry, local_size)
    return {
        "left": float(min(top_left.x, bottom_right.x)),
        "top": float(min(top_left.y, bottom_right.y)),
        "right": float(max(top_left.x, bottom_right.x)),
        "bottom": float(max(top_left.y, bottom_right.y)),
    }

def _widget_component_summary(world, component):
    widget = component.get_user_widget_object() if component else None
    return {
        "component_visible": bool(component and component.is_visible()
                                  and not component.is_hidden_in_game()),
        "widget_visible": bool(component and component.is_widget_visible()),
        "widget_space": str(component.get_widget_space()) if component else "",
        "draw_size": _vector2_summary(component.get_draw_size()) if component else None,
        "current_draw_size": _vector2_summary(component.get_current_draw_size()) if component else None,
        "pivot": _vector2_summary(component.get_pivot()) if component else None,
        "screen_rect": _screen_rect(world, widget),
    }
```

Use only the correct C++ names when summarizing a battle actor:

```python
resource_component = actor.get_resource_hud_widget_component_for_test()
status_component = actor.get_status_effects_widget_component_for_test()
summary["battle_hud"] = {
    "slot": int(actor.get_slot_number_for_test()),
    "hp": {"current": int(actor.get_current_health_for_test()),
           "max": int(actor.get_max_health_for_test())},
    "mana": {"current": int(actor.get_current_mana_for_test()),
             "max": int(actor.get_max_mana_for_test())},
    "armor": int(actor.get_armor_for_test()),
    "status_text": str(actor.get_status_text_for_test()),
    # This is only the actor-local personal-MP-row visibility switch;
    # it is not the board-owned shared-Qi value.
    "mana_row_visible": bool(actor.should_show_mana_for_test()),
    "resource_hud": _widget_component_summary(world, resource_component),
    "status_hud": _widget_component_summary(world, status_component),
}
```

In `scripts/gamexxk_real_play_flow_mcp.py`, add a deterministic, side-effect-free verdict:

```python
def _rect_intersects(rect: dict | None, viewport: dict) -> bool:
    return bool(rect and rect["right"] > rect["left"] and rect["bottom"] > rect["top"]
                and rect["right"] > 0 and rect["bottom"] > 0
                and rect["left"] < viewport["width"] and rect["top"] < viewport["height"])

def _battle_hud_verdict(probe: dict, viewport: dict) -> dict:
    errors: list[str] = []
    for unit in _battle_hud_units_from_probe(probe):
        resource = unit.get("battle_hud", {}).get("resource_hud", {})
        if not (resource.get("component_visible") and resource.get("widget_visible")
                and _rect_intersects(resource.get("screen_rect"), viewport)):
            errors.append(f"{unit.get('unit_id')}: resource HUD is not visibly on screen")
        if unit.get("is_enemy_unit") and unit.get("battle_hud", {}).get("mana_row_visible"):
            errors.append(f"{unit.get('unit_id')}: enemy MP row is visible")
        if not unit.get("is_enemy_unit") and not unit.get("battle_hud", {}).get("mana_row_visible"):
            errors.append(f"{unit.get('unit_id')}: party MP row is missing")
    return {"ok": not errors, "errors": errors}
```

- [ ] **Step 4: Re-run the Python tests and capture the red live baseline.**

Run the command from Step 2, then use UE MCP to run the project probe while PIE is in battle. Write the result to `Saved/HarnessReports/battle_actor_hud_before.json`.

Expected: Python tests pass; the current real PIE verdict remains red until actor rendering is repaired.

- [ ] **Step 5: Commit the diagnostics only.**

```powershell
git add scripts/test_gamexxk_real_play_flow_probe.py scripts/test_gamexxk_real_play_flow_mcp.py Content/Python/gamexxk_probe_real_play_flow.py scripts/gamexxk_real_play_flow_mcp.py
git commit -m "test: observe battle actor HUD in real flow"
```

### Task 2: Correct the per-unit MP semantics

**Files:**

- Modify: `Source/GameXXK/Public/UI/GameXXKBattleUnitResourceWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleUnitResourceWidget.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKBattleSceneUnitActor.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKBattleSceneUnitActor.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleUnitResourceWidgetTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleSceneActorHudTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp`

- [ ] **Step 1: Write failing tests for MP text and party/enemy visibility.**

Replace old personal-`Qi` test names and assertions with:

```cpp
ResourceWidget->SetUnitVitals(TEXT("1P"), FText::FromString(TEXT("主角")), 72, 100, 0, 0, true);
TestEqual(TEXT("party MP label is inner power"),
    ResourceWidget->GetManaDisplayTextForTest(), FString(TEXT("内力 0 / 0")));
TestTrue(TEXT("party MP remains visible at zero"),
    ResourceWidget->IsManaRowVisibleForTest());

ResourceWidget->SetUnitVitals(TEXT("敌1"), FText::FromString(TEXT("黑熊")), 72, 100, 18, 30, false);
TestFalse(TEXT("enemy never renders MP"),
    ResourceWidget->IsManaRowVisibleForTest());

TestTrue(TEXT("hero renders MP"), HeroActor->ShouldShowManaForTest());
TestTrue(TEXT("companion renders MP"), CompanionActor->ShouldShowManaForTest());
TestTrue(TEXT("task NPC renders MP"), NpcActor->ShouldShowManaForTest());
TestFalse(TEXT("enemy hides MP"), EnemyActor->ShouldShowManaForTest());
```

- [ ] **Step 2: Cold-build and run the focused tests to prove the old API is red.**

Run the UBT cold-build command, then:

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.UI.Battle.UnitResourceWidget+GameXXK.MVP.Battle.SceneActorHud+GameXXK.MVP.Battle.SceneActors;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

Expected: failure because `SetUnitVitals`, `GetManaDisplayTextForTest`, and `ShouldShowManaForTest` do not yet exist.

- [ ] **Step 3: Implement the MP-only interface and use the existing PSD mana frame.**

In `GameXXKBattleUnitResourceWidget.h` define:

```cpp
void SetUnitVitals(const FString& InSlotLabel, const FText& InDisplayName,
    int32 InCurrentHP, int32 InMaxHP, int32 InCurrentMana, int32 InMaxMana, bool bInShowMana);
FString GetManaDisplayTextForTest() const;
float GetManaPercentForTest() const;
bool IsManaRowVisibleForTest() const;

UPROPERTY(Transient) TObjectPtr<UHorizontalBox> ManaRow;
UPROPERTY(Transient) TObjectPtr<UTextBlock> ManaText;
UPROPERTY(Transient) TObjectPtr<UProgressBar> ManaBar;
bool bShowMana = false;
```

In the `.cpp`, retain the imported `T_TownPsd_CharacterManaFrame` brush and render:

```cpp
const FString ManaLabel = FString::Printf(
    TEXT("内力 %d / %d"), FMath::Max(0, CurrentMana), FMath::Max(0, MaxMana));
ManaText->SetText(FText::FromString(ManaLabel));
ManaBar->SetPercent(MaxMana > 0
    ? static_cast<float>(CurrentMana) / static_cast<float>(MaxMana) : 0.0f);
ManaRow->SetVisibility(bShowMana
    ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
```

In the actor, replace `bShowQi` with `bShowMana` and use identity only:

```cpp
const bool bShowMana = !bEnemy;
ResourceWidget->SetUnitVitals(
    SlotLabel, DisplayName, CurrentHealth, MaxHealth, CurrentMana, MaxMana, bShowMana);
```

- [ ] **Step 4: Re-run focused Automation tests.**

Run the command from Step 2.

Expected: PASS; all individual-resource text is `内力`, party rows remain visible at `0 / 0`, and enemies collapse their MP row.

- [ ] **Step 5: Commit the terminology repair.**

```powershell
git add Source/GameXXK/Public/UI/GameXXKBattleUnitResourceWidget.h Source/GameXXK/Private/UI/GameXXKBattleUnitResourceWidget.cpp Source/GameXXK/Public/MVP/GameXXKBattleSceneUnitActor.h Source/GameXXK/Private/MVP/GameXXKBattleSceneUnitActor.cpp Source/GameXXK/Private/Tests/GameXXKBattleUnitResourceWidgetTest.cpp Source/GameXXK/Private/Tests/GameXXKBattleSceneActorHudTest.cpp Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp
git commit -m "fix: distinguish battle MP from shared qi"
```

### Task 3: Project initial shield into authoritative runtime armor

**Files:**

- Modify: `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBattleAdapterTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp`

- [ ] **Step 1: Add a failing one-way projection test.**

```cpp
LegacyEnemy.Shield = 7;
TestTrue(TEXT("battle starts"),
    FGameXXKCardBattleAdapter::BeginCardBattle(
        State, EGameXXKNodeKind::Battle, EGameXXKCardTerrain::Plain, 20260720, &Error));
FGameXXKCardCombatUnit* CardEnemy = FindCardUnit(State.CardRun.ActiveBattle, LegacyEnemy.UnitId);
TestNotNull(TEXT("runtime enemy exists"), CardEnemy);
TestEqual(TEXT("opening shield projects to runtime armor"), CardEnemy->Armor, 7);

CardEnemy->Armor = 4;
TestTrue(TEXT("runtime armor projects back once"),
    FGameXXKCardBattleAdapter::SyncCardBattleToLegacyProjection(State, &Error));
TestEqual(TEXT("legacy is compatibility projection only"),
    FindLegacyUnit(State, LegacyEnemy.UnitId)->Shield, 4);
```

- [ ] **Step 2: Run the adapter test and confirm it fails at opening armor.**

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.Integration.CardBattleAdapter;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

Expected: the old `Unit.Armor = 0` causes the opening assertion to fail.

- [ ] **Step 3: Replace the zero initialization with one clamped projection.**

In `MakeCardCombatUnit`:

```cpp
// Card combat owns armor after this one compatibility projection.
Unit.Armor = FMath::Clamp(LegacyUnit.Shield, 0, 99);
```

Keep the existing later `CardUnit.Armor -> LegacyUnit.Shield` synchronization. Do not add a legacy-shield read to actor refresh or card resolution.

- [ ] **Step 4: Re-run adapter plus actor tests.**

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.Integration.CardBattleAdapter+GameXXK.MVP.Battle.SceneActors;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

Expected: PASS; runtime armor starts at seven and card mutations project backward once.

- [ ] **Step 5: Commit the authority-boundary change.**

```powershell
git add Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp Source/GameXXK/Private/Tests/GameXXKCardBattleAdapterTest.cpp Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp
git commit -m "fix: project opening shield into card armor"
```

### Task 4: Fix the actor HUD anchor, component sizing, and screen layer

**Files:**

- Modify: `Source/GameXXK/Public/MVP/GameXXKBattleSceneUnitActor.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKBattleSceneUnitActor.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleSceneActorHudTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp`

- [ ] **Step 1: Add failing structure/lifecycle assertions.**

```cpp
const UWidgetComponent* Resource = HeroActor->GetResourceHudWidgetComponentForTest();
const UWidgetComponent* Status = HeroActor->GetStatusEffectsWidgetComponentForTest();
TestEqual(TEXT("resource size"), Resource->GetDrawSize(), FVector2D(320.0f, 104.0f));
TestEqual(TEXT("status size"), Status->GetDrawSize(), FVector2D(320.0f, 54.0f));
TestEqual(TEXT("resource grows down from its top"), Resource->GetPivot(), FVector2D(0.5f, 0.0f));
TestEqual(TEXT("status grows down from its top"), Status->GetPivot(), FVector2D(0.5f, 0.0f));
TestEqual(TEXT("resource remains input transparent"),
    UGameXXKBattleUnitResourceWidget::GetRootHitTestVisibilityForTest(),
    ESlateVisibility::SelfHitTestInvisible);
```

Also retain the dead-unit test, requiring both widget components to hide together.

- [ ] **Step 2: Build and run actor tests to show the old implicit pivot layout fails.**

Run the cold build and:

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.MVP.Battle.SceneActorHud+GameXXK.MVP.Battle.SceneActors;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

Expected: failure because the old components are `300x96/300x46` and the resource component uses a bottom pivot.

- [ ] **Step 3: Attach HUD anchor to scene root and configure deterministic screen layers.**

In the constructor:

```cpp
HudAnchorComponent->SetupAttachment(SceneRoot);
ResourceHudWidgetComponent->SetupAttachment(HudAnchorComponent);
StatusEffectsWidgetComponent->SetupAttachment(HudAnchorComponent);

ResourceHudWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
ResourceHudWidgetComponent->SetDrawSize(FVector2D(320.0f, 104.0f));
ResourceHudWidgetComponent->SetPivot(FVector2D(0.5f, 0.0f));
ResourceHudWidgetComponent->SetInitialSharedLayerName(TEXT("GameXXKBattleActorHud"));
ResourceHudWidgetComponent->SetInitialLayerZOrder(20);

StatusEffectsWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
StatusEffectsWidgetComponent->SetDrawSize(FVector2D(320.0f, 54.0f));
StatusEffectsWidgetComponent->SetPivot(FVector2D(0.5f, 0.0f));
StatusEffectsWidgetComponent->SetInitialSharedLayerName(TEXT("GameXXKBattleActorHud"));
StatusEffectsWidgetComponent->SetInitialLayerZOrder(21);
```

In `RefreshHudAnchor`, derive the foot from the actual visual bounds and stack the rows without using a child of the rotated visual as the transform parent:

```cpp
const FVector VisualFoot = BattleVisualComponent->Bounds.Origin
    - FVector(0.0f, 0.0f, BattleVisualComponent->Bounds.BoxExtent.Z);
HudAnchorComponent->SetWorldLocation(VisualFoot + FVector(0.0f, 0.0f, -10.0f));
ResourceHudWidgetComponent->SetRelativeLocation(FVector::ZeroVector);
StatusEffectsWidgetComponent->SetRelativeLocation(FVector(0.0f, 0.0f, -40.0f));
```

- [ ] **Step 4: Verify C++ and then query live geometry.**

Run the actor Automation command from Step 2. Then use UE MCP to save `Saved/HarnessReports/battle_actor_hud_after_anchor.json` with the project probe.

Expected: every living unit reports visible screen-space resource HUD with a nonzero `screen_rect`; a missing rectangle remains a failure, not a pass.

- [ ] **Step 5: Commit the actor HUD repair.**

```powershell
git add Source/GameXXK/Public/MVP/GameXXKBattleSceneUnitActor.h Source/GameXXK/Private/MVP/GameXXKBattleSceneUnitActor.cpp Source/GameXXK/Private/Tests/GameXXKBattleSceneActorHudTest.cpp Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp
git commit -m "fix: anchor battle unit HUD below actor feet"
```

### Task 5: Raise status icon readability without blocking hover

**Files:**

- Modify: `Source/GameXXK/Private/UI/GameXXKBattleStatusIconWidget.cpp`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleUnitStatusEffectsWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleStatusEffectsWidgetTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleSceneActorHudTest.cpp`

- [ ] **Step 1: Write failing tests for 44px status badges and real descriptions.**

```cpp
StatusWidget->SetStatuses(
    { MakeStatus(EGameXXKCardStatus::Poison, 2), MakeStatus(EGameXXKCardStatus::Bleed, 3) }, 7);
TestTrue(TEXT("icons are at least 44 logical pixels"),
    StatusWidget->GetIconSizeForTest().X >= 44.0f
    && StatusWidget->GetIconSizeForTest().Y >= 44.0f);
TestEqual(TEXT("armor plus two statuses are visible"),
    StatusWidget->GetVisibleBadgeCountForTest(), 3);
TestEqual(TEXT("poison hover text"),
    StatusWidget->GetBadgeTooltipForTest(EGameXXKCardStatus::Poison),
    FString(TEXT("中毒：回合结束时受到层数×2伤害。")));
TestEqual(TEXT("bleed hover text"),
    StatusWidget->GetBadgeTooltipForTest(EGameXXKCardStatus::Bleed),
    FString(TEXT("流血：回合结束时受到层数×3伤害。")));
```

- [ ] **Step 2: Run the status test and confirm the old 38px floor fails.**

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.UI.Battle.StatusEffectsWidget;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

Expected: failure at the 44px assertion.

- [ ] **Step 3: Set the icon size and preserve passive container input.**

In `GameXXKBattleStatusIconWidget.cpp`:

```cpp
constexpr float StatusIconSize = 44.0f;
RootSizeBox->SetWidthOverride(StatusIconSize);
RootSizeBox->SetHeightOverride(StatusIconSize);
```

In `GameXXKBattleUnitStatusEffectsWidget.cpp` keep the parent passive and only collapse a truly empty row:

```cpp
BadgeRow->SetVisibility(
    VisibleBadges.Num() > 0 ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
```

Do not replace the existing single-color, low-saturation ink-wash brushes or tooltip binding.

- [ ] **Step 4: Re-run the status suite.**

Run the command from Step 2.

Expected: PASS; armor, poison, and bleed remain hoverable and the container does not capture card input.

- [ ] **Step 5: Commit the status readability change.**

```powershell
git add Source/GameXXK/Private/UI/GameXXKBattleStatusIconWidget.cpp Source/GameXXK/Private/UI/GameXXKBattleUnitStatusEffectsWidget.cpp Source/GameXXK/Private/Tests/GameXXKBattleStatusEffectsWidgetTest.cpp Source/GameXXK/Private/Tests/GameXXKBattleSceneActorHudTest.cpp
git commit -m "fix: enlarge battle status badges"
```

### Task 6: Create a passive, PSD-styled team-Qi widget

**Files:**

- Create: `Source/GameXXK/Public/UI/GameXXKBattlePartyQiWidget.h`
- Create: `Source/GameXXK/Private/UI/GameXXKBattlePartyQiWidget.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKBattlePartyQiWidgetTest.cpp`

- [ ] **Step 1: Write the failing isolated widget test.**

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKBattlePartyQiWidgetTest,
    "GameXXK.Battle.PartyQiWidget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattlePartyQiWidgetTest::RunTest(const FString& Parameters)
{
    UGameXXKBattlePartyQiWidget* Widget = NewObject<UGameXXKBattlePartyQiWidget>();
    TestTrue(TEXT("native panel builds"), Widget->PrepareForBoardEmbedding());
    Widget->SetSharedQi(5);
    TestEqual(TEXT("true current Qi"), Widget->GetSharedQiForTest(), 5);
    TestEqual(TEXT("no fake maximum"), Widget->GetDisplayTextForTest(), FString(TEXT("气力 5")));
    TestEqual(TEXT("team scope subtitle"), Widget->GetSubtitleForTest(), FString(TEXT("本回合共享")));
    TestTrue(TEXT("passive input"), Widget->IsInputTransparentForTest());
    return true;
}
```

- [ ] **Step 2: Cold-build and run the isolated test.**

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.Battle.PartyQiWidget;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

Expected: compile failure because the class does not exist.

- [ ] **Step 3: Implement the native widget using the imported PSD mana-frame texture.**

In the header:

```cpp
UCLASS()
class GAMEXXK_API UGameXXKBattlePartyQiWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    bool PrepareForBoardEmbedding();
    void SetSharedQi(int32 InSharedQi);
    int32 GetSharedQiForTest() const;
    FString GetDisplayTextForTest() const;
    FString GetSubtitleForTest() const;
    bool IsInputTransparentForTest() const;
private:
    UPROPERTY(Transient) TObjectPtr<UTextBlock> QiText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> SubtitleText;
    int32 SharedQi = 0;
};
```

Build with `/Game/GameXXK/UI/Town/Textures/PSD/Character/T_TownPsd_CharacterManaFrame.T_TownPsd_CharacterManaFrame` as a paper frame, low-saturation ink-blue text, and no progress bar:

```cpp
SharedQi = FMath::Max(0, InSharedQi);
QiText->SetText(FText::FromString(FString::Printf(TEXT("气力 %d"), SharedQi)));
SubtitleText->SetText(FText::FromString(TEXT("本回合共享")));
SetVisibility(ESlateVisibility::SelfHitTestInvisible);
```

- [ ] **Step 4: Re-run the isolated widget test.**

Run the command from Step 2.

Expected: PASS; only the true current value and team-scope subtitle render.

- [ ] **Step 5: Commit the reusable widget.**

```powershell
git add Source/GameXXK/Public/UI/GameXXKBattlePartyQiWidget.h Source/GameXXK/Private/UI/GameXXKBattlePartyQiWidget.cpp Source/GameXXK/Private/Tests/GameXXKBattlePartyQiWidgetTest.cpp
git commit -m "feat: add battle shared qi widget"
```

### Task 7: Embed team Qi between the hand and End Turn

**Files:**

- Modify: `Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBattleBoardWidgetTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleBoardWidgetTest.cpp`

- [ ] **Step 1: Write failing Board tests for data source, input, and layout.**

```cpp
Board->RefreshFromState();
TestTrue(TEXT("Qi panel visible in battle"), Board->IsPartyQiVisibleForTest());
TestEqual(TEXT("Qi reads the deck"), Board->GetPartyQiForTest(),
    State.CardRun.ActiveBattle.Deck.SharedEnergy);
TestEqual(TEXT("Qi text is current value"), Board->GetPartyQiTextForTest(),
    FString::Printf(TEXT("气力 %d"), State.CardRun.ActiveBattle.Deck.SharedEnergy));
TestTrue(TEXT("Qi does not steal input"), Board->IsPartyQiHitTestTransparentForTest());
TestEqual(TEXT("safe right-side slot"), Board->GetPartyQiSlotOffsetsForTest(),
    FMargin(-416.0f, -143.0f, 170.0f, 72.0f));

const int32 Before = State.CardRun.ActiveBattle.Deck.SharedEnergy;
PlayKnownOneEnergyCard(State);
Board->RefreshFromState();
TestEqual(TEXT("successful card charges exactly once"),
    Board->GetPartyQiForTest(), Before - 1);
```

- [ ] **Step 2: Run Board tests and confirm the panel seams are missing.**

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.Integration.CardBattle.BoardTargeting+GameXXK.MVP.Battle.BoardWidget;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

Expected: compile/test failure for the missing party-Qi methods.

- [ ] **Step 3: Construct one panel and refresh it on the existing layout bus.**

In the board header:

```cpp
class UGameXXKBattlePartyQiWidget;
UGameXXKBattlePartyQiWidget* GetPartyQiWidgetForTest() const { return PartyQiWidget; }
void RefreshPartyQi();
UPROPERTY(Transient) TObjectPtr<UGameXXKBattlePartyQiWidget> PartyQiWidget;
```

In `BuildProgrammaticLayout`:

```cpp
PartyQiWidget = WidgetTree->ConstructWidget<UGameXXKBattlePartyQiWidget>(
    UGameXXKBattlePartyQiWidget::StaticClass(), TEXT("BattlePartyQiWidget"));
PartyQiWidget->PrepareForBoardEmbedding();
UCanvasPanelSlot* PartyQiSlot = RootCanvas->AddChildToCanvas(PartyQiWidget);
PartyQiSlot->SetAnchors(FAnchors(1.0f, 1.0f));
PartyQiSlot->SetAlignment(FVector2D(0.0f, 0.0f));
PartyQiSlot->SetOffsets(FMargin(-416.0f, -143.0f, 170.0f, 72.0f));
PartyQiSlot->SetZOrder(35);
```

Call `RefreshPartyQi()` immediately after `RefreshHandCards()` in `RefreshProgrammaticLayout`:

```cpp
void UGameXXKBattleBoardWidget::RefreshPartyQi()
{
    if (!PartyQiWidget)
    {
        return;
    }
    const FGameXXKCardBattleRuntime* Battle = GetActiveCardBattle();
    const bool bVisible = Battle && IsBattleBoardVisible() && !IsRewardSelectionVisible();
    PartyQiWidget->SetVisibility(
        bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    if (bVisible)
    {
        PartyQiWidget->SetSharedQi(Battle->Deck.SharedEnergy);
    }
}
```

- [ ] **Step 4: Run Board tests through the enemy phase.**

Run the command from Step 2.

Expected: PASS; Qi decreases once after a successful card, remains visible during intents, and shows the actual next-player-phase reset.

- [ ] **Step 5: Commit the Board integration.**

```powershell
git add Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp Source/GameXXK/Private/Tests/GameXXKCardBattleBoardWidgetTest.cpp Source/GameXXK/Private/Tests/GameXXKBattleBoardWidgetTest.cpp
git commit -m "feat: show shared qi beside battle hand"
```

### Task 8: Make status/Qi visual acceptance deterministic in real PIE

**Files:**

- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- Create: `Content/Python/gamexxk_apply_battle_hud_fixture.py`
- Modify: `Content/Python/gamexxk_probe_real_play_flow.py`
- Modify: `scripts/gamexxk_real_play_flow_mcp.py`
- Modify: `scripts/test_gamexxk_real_play_flow_mcp.py`

- [ ] **Step 1: Write a failing verdict test for statuses and overlap.**

```python
def test_battle_hud_verdict_requires_status_rect_and_non_overlapping_team_qi(self) -> None:
    probe = _valid_battle_hud_probe()
    probe["units"][0]["armor"] = 7
    probe["units"][0]["status_text"] = "中毒 2；流血 3"
    probe["units"][0]["status_hud"]["screen_rect"] = None
    self.assertFalse(real_flow._battle_hud_verdict(probe, {"width": 1288, "height": 770})["ok"])

    probe = _valid_battle_hud_probe()
    probe["party_qi"]["screen_rect"] = probe["end_turn_rect"]
    self.assertFalse(real_flow._battle_hud_verdict(probe, {"width": 1288, "height": 770})["ok"])
```

- [ ] **Step 2: Run the Python verdict test and confirm the visual gate is red.**

```powershell
& 'C:\Users\shxuw\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' -m unittest scripts.test_gamexxk_real_play_flow_mcp -v
```

Expected: failure until status, team-Qi, hand, and End Turn rectangles are part of the probe/verdict.

- [ ] **Step 3: Add a development-only, non-saving runtime fixture.**

In `GameXXKMVPSubsystem.h`:

```cpp
#if WITH_DEV_AUTOMATION_TESTS
UFUNCTION(BlueprintCallable, Category = "GameXXK|Test")
bool ApplyBattleHudFixtureForTest(FString& OutError);
#endif
```

In `GameXXKMVPSubsystem.cpp`, mutate only an already-active transient card runtime and call the established compatibility sync:

```cpp
#if WITH_DEV_AUTOMATION_TESTS
bool UGameXXKMVPSubsystem::ApplyBattleHudFixtureForTest(FString& OutError)
{
    FGameXXKCardBattleRuntime& Battle = RuntimeState.CardRun.ActiveBattle;
    FGameXXKCardCombatUnit* PartyUnit = Battle.Units.FindByPredicate(
        [](const FGameXXKCardCombatUnit& Unit)
        { return Unit.Side == EGameXXKCardTargetSide::Party && Unit.bLiving; });
    FGameXXKCardCombatUnit* EnemyUnit = Battle.Units.FindByPredicate(
        [](const FGameXXKCardCombatUnit& Unit)
        { return Unit.Side == EGameXXKCardTargetSide::Enemy && Unit.bLiving; });
    if (!PartyUnit || !EnemyUnit)
    {
        OutError = TEXT("Battle HUD fixture requires one living party unit and one living enemy unit.");
        return false;
    }
    PartyUnit->HP = FMath::Max(1, PartyUnit->MaxHP - 13);
    PartyUnit->Mana = FMath::Max(0, PartyUnit->MaxMana - 4);
    PartyUnit->Armor = 7;
    EnemyUnit->Statuses = {
        { EGameXXKCardStatus::Poison, 2 },
        { EGameXXKCardStatus::Bleed, 3 },
    };
    Battle.Deck.SharedEnergy = 2;
    return FGameXXKCardBattleAdapter::SyncCardBattleToLegacyProjection(RuntimeState, &OutError);
}
#endif
```

Create a project Python command using the existing `_get_mvp_subsystem(world)` helper:

```python
import unreal
from gamexxk_probe_real_play_flow import _get_mvp_subsystem

def run():
    world = unreal.EditorLevelLibrary.get_game_world()
    subsystem = _get_mvp_subsystem(world)
    if subsystem is None:
        return {"ok": False, "error": "GameXXKMVPSubsystem is unavailable."}
    result = subsystem.apply_battle_hud_fixture_for_test()
    ok, error = result if isinstance(result, tuple) else (bool(result), "")
    return {"ok": bool(ok), "error": str(error)}
```

Extend the probe to call `Board.get_party_qi_widget_for_test()` and use `_screen_rect` for the panel, `HandCardBox`, and `EndTurnButton`.

- [ ] **Step 4: Make the real-flow harness wait for and capture render evidence.**

In `scripts/gamexxk_real_play_flow_mcp.py`, after battle opens, run the fixture, wait for a passing verdict, then emit and capture:

```python
self.event("battle_actor_hud_probe", **battle_hud_probe)
self.screenshot("real_flow_battle_hud.png")
self.screenshot("real_flow_battle_hud_hero_crop.png",
                crop=battle_hud_probe["hero_hud_crop"])
self.screenshot("real_flow_battle_hud_enemy_crop.png",
                crop=battle_hud_probe["enemy_hud_crop"])
```

The verdict must enforce:

```python
assert party_qi["value"] == shared_energy
assert _rect_intersects(party_qi["screen_rect"], viewport)
assert not _rects_overlap(party_qi["screen_rect"], end_turn_rect)
assert not _rects_overlap(party_qi["screen_rect"], hand_rect)
assert all(_rect_intersects(unit["resource_hud"]["screen_rect"], viewport)
           for unit in alive_units)
assert all(not unit["mana_row_visible"] for unit in alive_enemy_units)
assert all(unit["mana_row_visible"] for unit in alive_party_units)
assert _rect_intersects(status_fixture_unit["status_hud"]["screen_rect"], viewport)
```

- [ ] **Step 5: Run real flow and commit the visual gate.**

```powershell
& 'C:\Users\shxuw\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' scripts\gamexxk_real_play_flow_mcp.py --keep-pie --timeout 180 --report 'Saved\HarnessReports\battle-actor-hud-real-flow.json'
```

Expected: a passing report and `Saved/Codex/real_flow_battle_hud.png` plus readable hero/enemy HUD crops. Then:

```powershell
git add Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp Content/Python/gamexxk_apply_battle_hud_fixture.py Content/Python/gamexxk_probe_real_play_flow.py scripts/gamexxk_real_play_flow_mcp.py scripts/test_gamexxk_real_play_flow_mcp.py
git commit -m "test: gate battle HUD on PIE render evidence"
```

### Task 9: Cold-build, real-flow, and player-facing regression pass

**Files:**

- Modify: `docs/production/` only when an existing acceptance record explicitly includes the battle HUD gate.

- [ ] **Step 1: Save dirty packages through UE MCP before the editor is closed.**

Call the project UE MCP save operation and record its response. Do not force-close an editor with unsaved user changes.

- [ ] **Step 2: Perform one clean cold build.**

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -NoHotReload
```

Expected: `Result: Succeeded`; no Live Coding or Hot Reload is used as verification.

- [ ] **Step 3: Run focused automation, MVP automation, and the real flow.**

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.UI.Battle+GameXXK.MVP.Battle+GameXXK.Integration.CardBattle+GameXXK.Data.CardBattleRuntime+GameXXK.Data.CardCombatRules;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
& 'C:\Users\shxuw\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' scripts\gamexxk_mvp_playtest.py --skip-build --test-timeout 600 --report 'Saved\HarnessReports\battle-hud-mvp.json'
& 'C:\Users\shxuw\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' scripts\gamexxk_real_play_flow_mcp.py --keep-pie --timeout 180 --report 'Saved\HarnessReports\battle-hud-final.json'
```

Expected: every suite passes and the retained 1288×770 PIE screenshot visibly shows HP for enemies/party, MP only for party, armor/poison/bleed, and team Qi beside the hand.

- [ ] **Step 4: Auto-open only the project path and do the final hover inspection.**

Launch `D:\UE5 demo\GameXXK\GameXXK.uproject` with the project automation workflow, not `dotnet.exe`, an editor executable without the project, Live Coding, or Hot Reload. In the retained PIE session, hover one status icon and one card: their tooltips must appear above the actor HUD layer.

- [ ] **Step 5: Commit only an actual acceptance-record update.**

```powershell
git status --short
git add docs/production/<updated-battle-hud-acceptance-record>.md
git commit -m "docs: record battle HUD PIE acceptance"
```

If no acceptance record changed, do not create an empty commit. Preserve unrelated dirty files and report exact build/test commands and screenshot/report paths.

## Self-review

### Spec coverage

- Tasks 2 and 4 keep per-unit HUD actor-owned and place HP/MP/status below the actor foot.
- Tasks 2 and 7 separate MP (`内力`) from the team resource (`气力`).
- Task 3 fixes one-time shield-to-armor projection and preserves card runtime authority.
- Task 5 raises icon readability and preserves status hover tooltips.
- Tasks 6 and 7 add passive PSD-styled team Qi with no fabricated maximum.
- Tasks 1 and 8 make real screen rectangles, overlap safety, and screenshots mandatory instead of trusting actor creation.
- Task 9 provides cold-build, automation, real PIE, and project-path launch evidence.

### Placeholder scan

Every implementation step names a concrete type/method and every test step names a command and expected result; the forbidden-placeholder scan is clean.

### Type consistency

- Individual MP: `SetUnitVitals`, `ManaRow`, `ShouldShowManaForTest`.
- Team resource: `SharedEnergy`, `SetSharedQi`, `UGameXXKBattlePartyQiWidget`.
- Render geometry: `_screen_rect` and `screen_rect`, validated by `_battle_hud_verdict`.
- Fixture authority: card runtime changes followed by `SyncCardBattleToLegacyProjection`.

## Presentation and VFX extension

The following tasks extend Tasks 1–9 above. Execute Tasks 1–9 first because the event effects depend on actor-owned HUDs, a visible shared-Qi widget, and the real PIE geometry contract.

### Task 10: Enable Niagara and migrate only validated battle-effect dependencies

**Files:**

- Modify: `GameXXK.uproject`
- Modify: `Source/GameXXK/GameXXK.Build.cs`
- Create: `Content/Python/gamexxk_audit_battle_vfx_assets.py`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleVfxAssetTest.cpp`
- Migrate into: `D:\UE5 demo\GameXXK\Content\StylizedSlashesV2\`
- Migrate into: `D:\UE5 demo\GameXXK\Content\sA_PickupSet_1\`
- Migrate into: `D:\UE5 demo\GameXXK\Content\Basic_Healing_VFX\`
- Migrate into after core validation: `D:\UE5 demo\GameXXK\Content\Basic_VFX\`

- [ ] **Step 1: Write the failing asset-load test for the six approved systems.**

Create `GameXXKBattleVfxAssetTest.cpp`:

```cpp
#include "NiagaraSystem.h"
#include "UObject/SoftObjectPath.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKBattleVfxAssetTest,
    "GameXXK.Battle.VfxAssets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleVfxAssetTest::RunTest(const FString& Parameters)
{
    const TArray<FSoftObjectPath> RequiredSystems = {
        FSoftObjectPath(TEXT("/Game/StylizedSlashesV2/Particles/NiagaraSystems/NS_Slash_Ink.NS_Slash_Ink")),
        FSoftObjectPath(TEXT("/Game/StylizedSlashesV2/Particles/NiagaraSystems/NS_SlashImpact_Ink.NS_SlashImpact_Ink")),
        FSoftObjectPath(TEXT("/Game/sA_PickupSet_1/Fx/NiagaraSystems/NS_Shield.NS_Shield")),
        FSoftObjectPath(TEXT("/Game/sA_PickupSet_1/Fx/NiagaraSystems/NS_Energy_1.NS_Energy_1")),
        FSoftObjectPath(TEXT("/Game/Basic_Healing_VFX/VFX/NEW_Updated_VFX/Healing_VFX_Set_NEW/NS_Healing_NEW.NS_Healing_NEW")),
        FSoftObjectPath(TEXT("/Game/Basic_Healing_VFX/VFX/NEW_Updated_VFX/Healing_VFX_Set_NEW/NS_Mana_NEW.NS_Mana_NEW")),
    };

    for (const FSoftObjectPath& Path : RequiredSystems)
    {
        TestNotNull(*Path.ToString(), Cast<UNiagaraSystem>(Path.TryLoad()));
    }
    return true;
}
```

- [ ] **Step 2: Run the test and verify the project has no approved Niagara assets yet.**

Run the cold build followed by:

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.Battle.VfxAssets;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

Expected: asset loads fail until the selected effects and dependency closure have been migrated.

- [ ] **Step 3: Enable Niagara, migrate selected dependency closures through Unreal, and write an audit script.**

Add an explicit target plugin entry **inside the existing `Plugins` array** in `GameXXK.uproject` (with the surrounding JSON comma placement preserved):

```json
{
  "Name": "Niagara",
  "Enabled": true
}
```

Add the module only because C++ will load/spawn Niagara systems:

```csharp
PublicDependencyModuleNames.AddRange(new string[] {
    "Core", "CoreUObject", "Engine", "EngineSettings", "InputCore",
    "EnhancedInput", "UMG", "Slate", "SlateCore", "Paper2D", "Niagara"
});
```

Open `D:\UE5 demo\zzz\我的项目\我的项目.uproject` in its registered UE 5.4 source editor, without saving changes to that source project. Select exactly the six systems listed in Step 1, use Unreal **Migrate**, and set the destination to `D:\UE5 demo\GameXXK\Content`. Allow Unreal to include only each selected system's dependency closure. Do not migrate `Maps`, `*_BuiltData`, `Blueprints`, `.fbx`, or `.png` source files.

Create an audit project Python file that checks the resulting packages by exact path:

```python
import unreal

SYSTEM_PATHS = (
    "/Game/StylizedSlashesV2/Particles/NiagaraSystems/NS_Slash_Ink.NS_Slash_Ink",
    "/Game/StylizedSlashesV2/Particles/NiagaraSystems/NS_SlashImpact_Ink.NS_SlashImpact_Ink",
    "/Game/sA_PickupSet_1/Fx/NiagaraSystems/NS_Shield.NS_Shield",
    "/Game/sA_PickupSet_1/Fx/NiagaraSystems/NS_Energy_1.NS_Energy_1",
    "/Game/Basic_Healing_VFX/VFX/NEW_Updated_VFX/Healing_VFX_Set_NEW/NS_Healing_NEW.NS_Healing_NEW",
    "/Game/Basic_Healing_VFX/VFX/NEW_Updated_VFX/Healing_VFX_Set_NEW/NS_Mana_NEW.NS_Mana_NEW",
)

def run():
    missing = [path for path in SYSTEM_PATHS if unreal.load_asset(path) is None]
    return {"ok": not missing, "missing": missing, "loaded": len(SYSTEM_PATHS) - len(missing)}
```

- [ ] **Step 4: Verify asset loading, package upgrade, and source style safety.**

Open the migrated assets in UE 5.8, wait for shader compilation, run the Automation test from Step 2 and run the project Python audit through UE MCP.

Expected: six systems load without redirector/missing-reference errors. If a source system still references a missing `/Game/StylizedSlashesVol2` or `/Game/Lights_VFX_Pack` package, exclude that system from runtime registry and repair/re-save it before adding it to the test list. After those six pass, migrate the remaining useful Niagara systems and their dependencies from all four named packs (including `Basic_VFX` for poison/continuous-damage variants) using the same exclusions. Register an expansion effect only after its own exact-path audit and battle-scene playback pass; the six core systems remain the required smoke-test baseline.

- [ ] **Step 5: Commit configuration, audit, and only successfully migrated runtime assets.**

```powershell
git add GameXXK.uproject Source/GameXXK/GameXXK.Build.cs Content/Python/gamexxk_audit_battle_vfx_assets.py Source/GameXXK/Private/Tests/GameXXKBattleVfxAssetTest.cpp Content/StylizedSlashesV2 Content/sA_PickupSet_1 Content/Basic_Healing_VFX Content/Basic_VFX
git commit -m "feat: add validated battle vfx assets"
```

### Task 11: Emit non-persistent semantic presentation events from real card resolution

**Files:**

- Modify: `Source/GameXXK/Public/GameXXKCardTypes.h`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Modify: `Source/GameXXK/Public/GameXXKCardBattleAdapter.h`
- Modify: `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBattleRuntimeTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBattleAdapterTest.cpp`

- [ ] **Step 1: Write failing semantic-event tests covering Buff, Impact, and DOT.**

Add tests against the existing concrete runtime fixtures that resolve self-armor, mana, direct impact, on-hit status, and end-phase DOT. The event data is presentation-only: it must not be `SaveGame` data and must be populated only after the atomic runtime mutation has validated.

```cpp
FGameXXKCardPlayResult SelfBuffResult;
TestTrue(TEXT("self armor card resolves"), GameXXKCardRules::ResolveCardPlay(
    EndRoundRuntime, EndRoundRuntime.Deck.Hand[0].InstanceId, NAME_None, SelfBuffResult, &Error));
TestTrue(TEXT("armor event is present"),
    SelfBuffResult.PresentationEvents.ContainsByPredicate([](const FGameXXKBattlePresentationEvent& Event)
    { return Event.Type == EGameXXKBattlePresentationEventType::ArmorGain
        && Event.TargetUnitId == TEXT("Hero") && Event.AppliedAmount > 0; }));

FGameXXKCardPlayResult AttackResult;
TestTrue(TEXT("enemy hit resolves"), GameXXKCardRules::ResolveCardPlay(
    AttackRuntime, AttackRuntime.Deck.Hand[0].InstanceId, TEXT("Enemy"), AttackResult, &Error));
TestTrue(TEXT("impact carries health damage"),
    AttackResult.PresentationEvents.ContainsByPredicate([](const FGameXXKBattlePresentationEvent& Event)
    { return Event.Type == EGameXXKBattlePresentationEventType::DirectImpact && Event.AppliedAmount > 0; }));

TArray<FGameXXKCardDamageResult> DotResults;
TArray<FGameXXKBattlePresentationEvent> DotPresentationEvents;
TestTrue(TEXT("poison tick resolves"), GameXXKCardRules::BeginNextPlayerCardRound(
    DotRuntime, DotResults, DotPresentationEvents, &Error));
TestTrue(TEXT("DOT is a semantic event"),
    DotPresentationEvents.ContainsByPredicate([](const FGameXXKBattlePresentationEvent& Event)
    { return Event.Type == EGameXXKBattlePresentationEventType::DotTick
        && Event.DamageKind == EGameXXKCardDamageKind::DamageOverTime; }));
```

- [ ] **Step 2: Run focused card-runtime and adapter tests to establish the red baseline.**

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.Data.CardBattleRuntime+GameXXK.Integration.CardBattleAdapter;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

Expected: compile failure because `PresentationEvents` and semantic event types do not exist.

- [ ] **Step 3: Define the event type and emit it at each successful atomic resolution.**

In `GameXXKCardTypes.h`:

```cpp
UENUM(BlueprintType)
enum class EGameXXKBattlePresentationEventType : uint8
{
    Invalid, CardCast, DirectImpact, ArmorAbsorb, Avoid, Heal, ArmorGain,
    ArmorExpired, ManaGain, StatusAdded, StatusRemoved, DotTick, EnergyChanged, Draw,
    Redirect, Defeat
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKBattlePresentationEvent
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite) EGameXXKBattlePresentationEventType Type = EGameXXKBattlePresentationEventType::Invalid;
    UPROPERTY(BlueprintReadWrite) FName SourceUnitId = NAME_None;
    UPROPERTY(BlueprintReadWrite) FName TargetUnitId = NAME_None;
    UPROPERTY(BlueprintReadWrite) FName SourceCardInstanceId = NAME_None;
    UPROPERTY(BlueprintReadWrite) FName SourceCardId = NAME_None;
    UPROPERTY(BlueprintReadWrite) EGameXXKCardDamageKind DamageKind = EGameXXKCardDamageKind::Invalid;
    UPROPERTY(BlueprintReadWrite) EGameXXKCardStatus Status = EGameXXKCardStatus::Invalid;
    UPROPERTY(BlueprintReadWrite) int32 RequestedAmount = 0;
    UPROPERTY(BlueprintReadWrite) int32 AppliedAmount = 0;
    UPROPERTY(BlueprintReadWrite) int32 OldValue = 0;
    UPROPERTY(BlueprintReadWrite) int32 NewValue = 0;
    UPROPERTY(BlueprintReadWrite) int32 HitIndex = 0;
    UPROPERTY(BlueprintReadWrite) bool bRedirected = false;
    UPROPERTY(BlueprintReadWrite) bool bAvoided = false;
};
```

Add `TArray<FGameXXKBattlePresentationEvent> PresentationEvents;` to `FGameXXKCardPlayResult` with `BlueprintReadWrite` but **without** `SaveGame`. Extend all phase boundary APIs in both `GameXXKCardRules` and `FGameXXKCardBattleAdapter` with a separate non-persistent output argument immediately after `OutDamageResults`:

```cpp
bool EndPlayerCardPhase(Runtime, OutDamageResults, OutPresentationEvents, OutError);
bool BeginNextPlayerCardRound(Runtime, OutDamageResults, OutPresentationEvents, OutError);
bool ResolveNextEnemyIntent(State, OutIntent, OutDamageResults,
    OutPresentationEvents, bOutIntentsFinished, OutError);
bool CompleteEnemyCardPhase(State, OutDamageResults, OutPresentationEvents, OutError);
bool ResolveEnemyPhase(State, OutDamageResults, OutPresentationEvents, OutError);
```

Initialize those local event arrays next to the existing local damage-result arrays and assign them to the outgoing arguments only after runtime validation and the `MoveTemp(NewRuntime)` commit. Update every test and call site together; do not add an overload that can silently discard presentation events.

When a mutation is actually applied, append the corresponding event from the same branch that changes runtime data:

```cpp
AppendPresentationEvent(OutEvents, EGameXXKBattlePresentationEventType::ArmorGain,
    SourceUnitId, Target->UnitId, CardInstance.Id, Definition.Id,
    EGameXXKCardDamageKind::Invalid, EGameXXKCardStatus::Invalid,
    Effect.Magnitude, AppliedArmor, OldArmor, Target->Armor);
```

For direct damage, append `Redirect`, `Avoid`, `ArmorAbsorb`, and/or `DirectImpact` using the existing `FGameXXKCardDamageResult` **at the branch that still has the `FGameXXKCardDamageContext`, card instance, card definition, and hit index**. Do not infer a damage kind or an on-hit status from the refreshed widget state. Capture real before/after stack counts around every status mutation and append `StatusAdded`/`StatusRemoved` only when the applied amount is non-zero. For end-phase damage, append one `DotTick` per damaged unit with `DamageKind::DamageOverTime`, `Status::None`, and no invented source card; combined Bleed/Poison/Burn damage is one combined tick, not a misleading single-status tick. Emit `ArmorExpired` using old/new armor values when phase start clears armor. Emit `EnergyChanged` and `Draw` at their real deck mutations so the board can animate only committed changes.

- [ ] **Step 4: Re-run runtime and adapter tests including multi-hit, redirected, and status cases.**

Run the command from Step 2.

Expected: PASS; a self Buff has armor/mana/status events, a direct hit distinguishes impact/armor/avoid/redirect, and DOT remains a DOT event.

- [ ] **Step 5: Commit the data-only presentation bridge.**

```powershell
git add Source/GameXXK/Public/GameXXKCardTypes.h Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Public/GameXXKCardBattleAdapter.h Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp Source/GameXXK/Private/Tests/GameXXKCardBattleRuntimeTest.cpp Source/GameXXK/Private/Tests/GameXXKCardBattleAdapterTest.cpp
git commit -m "feat: emit battle presentation events"
```

### Task 12: Add actor-owned effect anchors, queues, and HUD micro-feedback

**Files:**

- Modify: `Source/GameXXK/Public/MVP/GameXXKBattleSceneUnitActor.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKBattleSceneUnitActor.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKBattleUnitResourceWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleUnitResourceWidget.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKBattleUnitStatusEffectsWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleUnitStatusEffectsWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleStatusEffectsWidgetTest.cpp`

- [ ] **Step 1: Write failing actor queue and HUD pulse tests.**

```cpp
HeroActor->QueuePresentationEventForTest(
    MakePresentationEvent(EGameXXKBattlePresentationEventType::ArmorGain, HeroUnitId, 8));
TestEqual(TEXT("armor effect waits on buff anchor"),
    HeroActor->GetActivePresentationEventTypeForTest(),
    EGameXXKBattlePresentationEventType::ArmorGain);
TestTrue(TEXT("buff anchor is valid"), IsValid(HeroActor->GetBuffFxAnchorForTest()));
TestTrue(TEXT("armor Niagara is attached"), HeroActor->IsBuffFxVisibleForTest());

HeroActor->QueuePresentationEventForTest(
    MakePresentationEvent(EGameXXKBattlePresentationEventType::DirectImpact, EnemyUnitId, HeroUnitId, 18));
TestTrue(TEXT("impact anchor is valid"), IsValid(HeroActor->GetImpactFxAnchorForTest()));
TestEqual(TEXT("impact queue type"),
    HeroActor->GetActivePresentationEventTypeForTest(),
    EGameXXKBattlePresentationEventType::DirectImpact);

ResourceWidget->PlayHealthDeltaFeedback(72, 54);
TestTrue(TEXT("health tween is active"), ResourceWidget->IsHealthFeedbackActiveForTest());
StatusWidget->PlayStatusAddedFeedback(EGameXXKCardStatus::Poison);
TestTrue(TEXT("status icon pulse is active"), StatusWidget->IsStatusPulseActiveForTest(EGameXXKCardStatus::Poison));
```

- [ ] **Step 2: Run the actor/widget tests and observe the absence of FX anchors and feedback state.**

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.MVP.Battle.SceneActors+GameXXK.UI.Battle.StatusEffectsWidget+GameXXK.UI.Battle.UnitResourceWidget;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

Expected: compile/test failure because the actor has only one wobble state and widgets have no delta feedback APIs.

- [ ] **Step 3: Implement independent anchors and a local actor effect queue.**

Add two scene anchors to the actor rather than using the foot HUD anchor. They attach to `SceneRoot`, have their own visual-bounds refresh, and never attach to `BattleVisual`, `HudAnchorComponent`, or the screen-space widget components:

```cpp
BuffFxAnchorComponent = CreateDefaultSubobject<USceneComponent>(TEXT("BuffFxAnchor"));
BuffFxAnchorComponent->SetupAttachment(SceneRoot);
ImpactFxAnchorComponent = CreateDefaultSubobject<USceneComponent>(TEXT("ImpactFxAnchor"));
ImpactFxAnchorComponent->SetupAttachment(SceneRoot);
```

Keep a non-persistent queue and spawn only mapped, validated systems:

```cpp
void AGameXXKBattleSceneUnitActor::QueuePresentationEvent(
    const FGameXXKBattlePresentationEvent& Event)
{
    PendingPresentationEvents.Add(Event);
    if (!ActivePresentationEvent.IsSet())
    {
        BeginNextPresentationEvent();
    }
}

void AGameXXKBattleSceneUnitActor::BeginNextPresentationEvent()
{
    if (PendingPresentationEvents.IsEmpty())
    {
        ActivePresentationEvent.Reset();
        return;
    }
    ActivePresentationEvent = PendingPresentationEvents[0];
    PendingPresentationEvents.RemoveAt(0);
    UNiagaraSystem* System = ResolveNiagaraSystemForEvent(*ActivePresentationEvent);
    USceneComponent* Anchor = UsesBuffAnchor(*ActivePresentationEvent)
        ? BuffFxAnchorComponent : ImpactFxAnchorComponent;
    ActiveFxComponent = System
        ? UNiagaraFunctionLibrary::SpawnSystemAttached(System, Anchor,
            NAME_None, FVector::ZeroVector, FRotator::ZeroRotator,
            EAttachLocation::KeepRelativeOffset, true)
        : nullptr;
}
```

Store only soft references to the validated Niagara systems in the actor. If a mapped system fails to load, log its exact path once, finish the presentation event through its deterministic fallback duration, and do not block combat input. Advance the queue from the actor tick using `UNiagaraComponent::OnSystemFinished` plus a bounded fallback duration; retain the existing motion state until both motion and queued presentation work have settled. Update anchors from visual bounds every refresh, but do not attach them to the `BattleVisual` flipbook. Map `ArmorGain` to `NS_Shield`, `ManaGain` to `NS_Energy_1`, `Heal` to `NS_Healing_NEW`, `DirectImpact` to `NS_SlashImpact_Ink`, and `CardCast` to `NS_Slash_Ink` only when the card's presentation calls for an attack.

Add resource tween state that interpolates from old to new values in `NativeTick`, and preserve a status-id-to-icon map so a changed status badge can pulse without rebuilding away its hover/Tooltip target.

- [ ] **Step 4: Re-run actor and widget tests.**

Run the command from Step 2.

Expected: PASS; anchor positions are independent from foot HUD, exact event types queue and retire, HP/MP values tween, and status Tooltip hit targets remain valid during pulses.

- [ ] **Step 5: Commit actor/HUD effect feedback.**

```powershell
git add Source/GameXXK/Public/MVP/GameXXKBattleSceneUnitActor.h Source/GameXXK/Private/MVP/GameXXKBattleSceneUnitActor.cpp Source/GameXXK/Public/UI/GameXXKBattleUnitResourceWidget.h Source/GameXXK/Private/UI/GameXXKBattleUnitResourceWidget.cpp Source/GameXXK/Public/UI/GameXXKBattleUnitStatusEffectsWidget.h Source/GameXXK/Private/UI/GameXXKBattleUnitStatusEffectsWidget.cpp Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp Source/GameXXK/Private/Tests/GameXXKBattleStatusEffectsWidgetTest.cpp
git commit -m "feat: add battle actor effect feedback"
```

### Task 13: Add a hand presentation state that does not conflict with Hover or targeting

**Files:**

- Modify: `Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBattleBoardWidgetTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBattleBoardEnemyIntentPresentationTest.cpp`

- [ ] **Step 1: Write failing discard/refill state tests.**

```cpp
Board->EndCardPlayerPhase();
TestTrue(TEXT("discard presentation starts"),
    Board->IsHandPresentationActiveForTest());
TestEqual(TEXT("discard state"),
    Board->GetHandPresentationStateForTest(),
    EGameXXKHandPresentationState::Discarding);

AdvanceUntilEnemyCompletion(Board);
TestEqual(TEXT("rule returns to player"), Runtime.Phase, EGameXXKCardBattlePhase::Player);
TestEqual(TEXT("refill starts once"),
    Board->GetHandPresentationStateForTest(),
    EGameXXKHandPresentationState::Refilling);
TestFalse(TEXT("hand is locked while cards deal"),
    Board->IsHandCardSlotEnabledForTest(0));

Board->AdvanceHandRefillPresentationForTest(1.0f);
TestEqual(TEXT("presentation settles"),
    Board->GetHandPresentationStateForTest(),
    EGameXXKHandPresentationState::Stable);
TestTrue(TEXT("first valid card unlocks"),
    Board->IsHandCardSlotEnabledForTest(0));
```

- [ ] **Step 2: Run the existing Board tests and confirm the current static hand refresh has no state.**

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.Integration.CardBattle.BoardTargeting+GameXXK.Integration.CardBattle.BoardEnemyIntentPresentation;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

Expected: compile/test failure because `EGameXXKHandPresentationState` and the hand animation test seam do not exist.

- [ ] **Step 3: Implement nested card motion wrappers and explicit phase-completion trigger.**

Define the local-only state:

```cpp
enum class EGameXXKHandPresentationState : uint8
{
    Stable, Discarding, Refilling
};

struct FGameXXKHandPresentationStateData
{
    EGameXXKHandPresentationState State = EGameXXKHandPresentationState::Stable;
    TArray<FName> IncomingInstanceIds;
    int32 ArrivedCount = 0;
    float Elapsed = 0.0f;
    int32 CompletedRound = INDEX_NONE;
};
```

In `BuildProgrammaticLayout`, place each card `UButton` inside a `USizeBox` Motion Wrapper; retain arrays for wrappers and inner buttons. `AdvanceHandCardHoverMotion` continues to write only the inner button transform. Add `AdvanceHandPresentation` to write only wrapper scale, translation, and opacity.

Start discard explicitly in `EndCardPlayerPhase` after the adapter successfully enters `Enemy`. Start refill only in `CompleteEnemyIntentPresentation` after `CompleteEnemyCardPhase` returns success, only if the committed runtime phase is `Player`, and before `ResolveAndRefreshCardBattleAfterMutation` refreshes visible slots. Never start a refill after Victory, Defeat, or a recoverable completion failure:

```cpp
StartHandRefillPresentation(MutableState.CardRun.ActiveBattle.RoundNumber,
    MutableState.CardRun.ActiveBattle.Deck.Hand);
```

Refill each actual `CardInstanceId` in runtime order using 0.22-second flight and 0.07-second stagger. During `Discarding` or `Refilling`, `RefreshHandCards` binds data but keeps input disabled and never resets wrapper transforms.

- [ ] **Step 4: Re-run Board tests, including duplicate-refresh and Hover coexistence cases.**

Run the command from Step 2.

Expected: PASS; all enemy intents produce one refill, `RefreshFromState()` does not restart it, and Hover applies only after wrapper motion reaches Stable.

- [ ] **Step 5: Commit deterministic discard/refill presentation.**

```powershell
git add Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp Source/GameXXK/Private/Tests/GameXXKCardBattleBoardWidgetTest.cpp Source/GameXXK/Private/Tests/GameXXKCardBattleBoardEnemyIntentPresentationTest.cpp
git commit -m "feat: animate battle discard and hand refill"
```

### Task 14: Queue enemy intent beats and route semantic effects to the correct actors

**Files:**

- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBattleBoardEnemyIntentPresentationTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp`

The existing display-only enum is `None, Reveal, Resolve, Settle`. Replace it deliberately rather than introducing a second parallel state machine:

```cpp
enum class EGameXXKEnemyIntentPresentationState : uint8
{
    None, Focus, Cast, TargetLock, Impact, Settle
};
```

Expose exactly one `#if WITH_DEV_AUTOMATION_TESTS` read seam in `UGameXXKBattleBoardWidget`:

```cpp
EGameXXKEnemyIntentPresentationState GetEnemyIntentPresentationStateForTest() const;
```

- [ ] **Step 1: Write failing tests for separated intent beats and result-specific feedback.**

```cpp
Board->AdvanceEnemyIntentPresentationForTest(0.12f);
TestEqual(TEXT("intent focuses before resolving"),
    Board->GetEnemyIntentPresentationStateForTest(),
    EGameXXKEnemyIntentPresentationState::Focus);

Board->AdvanceEnemyIntentPresentationForTest(0.16f);
TestEqual(TEXT("intent execution card is on stage"),
    Board->GetEnemyIntentPresentationStateForTest(),
    EGameXXKEnemyIntentPresentationState::Cast);

Board->AdvanceEnemyIntentPresentationForTest(0.20f);
TestEqual(TEXT("impact happens after cast"),
    Board->GetEnemyIntentPresentationStateForTest(),
    EGameXXKEnemyIntentPresentationState::Impact);

PlayerController->RefreshBattleSceneAfterCardMutation(
    EnemyUnitId, { MakeArmorAbsorbResult(EnemyUnitId, HeroUnitId, 8) }, PresentationEvents);
TestFalse(TEXT("armor-only result does not shake the target"),
    HeroActor->WasHitFeedbackPlayedForTest());
TestTrue(TEXT("armor-only result queues shield effect"),
    HeroActor->HasQueuedPresentationTypeForTest(EGameXXKBattlePresentationEventType::ArmorAbsorb));
```

- [ ] **Step 2: Run enemy-intent and actor tests to verify the old three-state display is insufficient.**

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.Integration.CardBattle.BoardEnemyIntentPresentation+GameXXK.MVP.Battle.SceneActors;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

Expected: compile/test failure because `Focus`, `Cast`, `Impact`, and presentation event routing are absent.

- [ ] **Step 3: Refine enemy sequencing and controller dispatch without moving rules timing.**

Expand the local Board state to `Focus`, `Cast`, `TargetLock`, `Impact`, and `Settle`. `Focus` raises the existing top intent card; `Cast` plays the attacker sprite motion; `TargetLock` shows only the read-only intent arrow; `Impact` calls `ResolveNextEnemyIntent` exactly once and queues returned events; `Settle` waits for that event sequence and either advances to the next saved intent or calls `CompleteEnemyIntentPresentation`. Keep the actual rules mutation at the `Impact` boundary and retain its recoverable failure behavior. Update all code paths that previously compared `Reveal`/`Resolve`, including the showcase opacity and visible-card emphasis.

Add event-array input to the controller:

```cpp
void AGameXXKMVPPlayerController::RefreshBattleSceneAfterCardMutation(
    FName AttackerUnitId,
    const TArray<FGameXXKCardDamageResult>& DamageResults,
    const TArray<FGameXXKBattlePresentationEvent>& PresentationEvents);
```

After refreshing retained scene actors, route every event to its stable `TargetUnitId`, then choose legacy motion only from actual result semantics:

```cpp
if (DamageResult.HealthDamage > 0)
{
    TargetActor->PlayHitFeedback();
}
else if (DamageResult.ArmorAbsorbed > 0)
{
    TargetActor->QueuePresentationEvent(MakeArmorAbsorbEvent(DamageResult));
}
else if (DamageResult.bAvoidedByAgility)
{
    TargetActor->QueuePresentationEvent(MakeAvoidEvent(DamageResult));
}
```

Use a read-only enemy intent arrow state based on `RegisteredBattleUnitScreenPositions`; do not reuse the player targeting interaction or mutate `InteractionMode`.

Keep source and target feedback distinct: queue `CardCast` on the attacker, and route `DirectImpact`, `ArmorAbsorb`, `Avoid`, `Heal`, `ArmorGain`, `ManaGain`, status, DOT, and defeat events to their target. The controller must tolerate a target actor no longer existing after defeat; skip only that visual event and never rerun the rules action. Pass the same event array at every `RefreshBattleSceneAfterCardMutation` call site: manual player card play, automatic card resolution, enemy intent impact, and enemy-phase completion DOT.

- [ ] **Step 4: Re-run sequenced-intent tests.**

Run the command from Step 2.

Expected: PASS; actor motion and impact occur after target lock, armor/avoid never use normal hit shake, and the final settle starts exactly one hand refill after phase completion.

- [ ] **Step 5: Commit sequenced enemy presentation.**

```powershell
git add Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp Source/GameXXK/Private/Tests/GameXXKCardBattleBoardEnemyIntentPresentationTest.cpp Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp
git commit -m "feat: sequence enemy card presentation and impact"
```

### Task 15: Make real PIE acceptance prove assets, effects, HUDs, and hand animation

**Files:**

- Modify: `Content/Python/gamexxk_probe_real_play_flow.py`
- Modify: `Content/Python/gamexxk_apply_battle_hud_fixture.py`
- Modify: `scripts/gamexxk_real_play_flow_mcp.py`
- Modify: `scripts/test_gamexxk_real_play_flow_probe.py`
- Modify: `scripts/test_gamexxk_real_play_flow_mcp.py`

- [ ] **Step 1: Write failing Python verdict tests for active effect state and hand refill.**

```python
def test_battle_hud_verdict_requires_effect_and_refill_evidence(self) -> None:
    probe = _valid_battle_hud_probe()
    probe["presentation"] = {
        "hand_state": "Refilling",
        "arrived_count": 3,
        "incoming_count": 5,
        "input_locked": True,
        "active_effects": [{"type": "ArmorGain", "target_unit_id": "Hero"}],
    }
    self.assertTrue(real_flow._battle_presentation_verdict(probe)["ok"])

    probe["presentation"]["input_locked"] = False
    self.assertFalse(real_flow._battle_presentation_verdict(probe)["ok"])
```

- [ ] **Step 2: Run the Python test and observe missing probe fields.**

```powershell
& 'C:\Users\shxuw\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' -m unittest scripts.test_gamexxk_real_play_flow_probe scripts.test_gamexxk_real_play_flow_mcp -v
```

Expected: failure because the live probe does not yet report presentation state.

- [ ] **Step 3: Extend the fixture and probe with screen-safe presentation state.**

The fixture must produce a living party unit with ArmorGain and a living enemy with Poison, Bleed, and a direct-impact-ready health delta. Extend the actor probe to report:

```python
{
    "unit_id": "Hero",
    "active_presentation_event": "ArmorGain",
    "buff_fx_visible": True,
    "impact_fx_visible": False,
    "buff_anchor_screen": {"x": 604, "y": 412},
    "impact_anchor_screen": {"x": 604, "y": 442},
}
```

Report Board state without reading private widget internals:

```python
{
    "presentation": {
        "hand_state": "Refilling",
        "arrived_count": 3,
        "incoming_count": 5,
        "input_locked": True,
        "shared_qi_value": 3,
        "active_intent_stage": "Impact"
    }
}
```

Use current Slate geometry helpers to verify every effect anchor stays above the actor foot HUD, below Tooltip layers, and inside the 1288×770 screenshot.

- [ ] **Step 4: Run the retained real flow and inspect staged screenshots.**

```powershell
& 'C:\Users\shxuw\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' scripts\gamexxk_real_play_flow_mcp.py --keep-pie --timeout 240 --report 'Saved\HarnessReports\battle-presentation-real-flow.json'
```

Expected output includes:

```text
Saved/Codex/real_flow_enemy_impact.png
Saved/Codex/real_flow_self_buff.png
Saved/Codex/real_flow_hand_refill.png
Saved/Codex/real_flow_battle_hud.png
```

The report must reject an invisible Niagara component, an effect overlapping the foot HUD, a refilling hand with unlocked input, or a refill that begins before the last enemy intent finishes.

- [ ] **Step 5: Cold-build, run all relevant suites, and commit acceptance changes.**

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -NoHotReload
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.UI.Battle+GameXXK.MVP.Battle+GameXXK.Integration.CardBattle+GameXXK.Data.CardBattleRuntime+GameXXK.Data.CardCombatRules;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

```powershell
git add Content/Python/gamexxk_probe_real_play_flow.py Content/Python/gamexxk_apply_battle_hud_fixture.py scripts/gamexxk_real_play_flow_mcp.py scripts/test_gamexxk_real_play_flow_probe.py scripts/test_gamexxk_real_play_flow_mcp.py
git commit -m "test: verify battle presentation in real pie"
```

## Extension self-review

### Spec coverage

- Tasks 10 and 15 migrate and verify the curated Niagara source assets with UE 5.8, while excluding demonstrations and unsupported effects.
- Task 11 provides the required semantic bridge for Buff, Impact, status, multi-hit, redirect, and DOT; it does not pollute persisted combat state.
- Task 12 separates body/impact anchors from foot HUD and adds HP, MP, armor, and status micro-feedback.
- Task 13 implements end-turn discard and final-enemy-only hand refill while preserving Hover/targeting transforms.
- Task 14 makes enemy cards, target lock, actor motion, impact, and settle occur in a readable order.
- Task 15 gives real PIE screenshots and an automated verdict for both VFX and the previously planned HUD/Qi behavior.

### Placeholder scan

Each new task contains an exact file list, failure assertion, implementation snippet, test command, expected state, and narrow commit set.

### Type consistency

- Data bridge: `EGameXXKBattlePresentationEventType` and `FGameXXKBattlePresentationEvent`.
- Actor consumer: `QueuePresentationEvent`.
- Hand consumer: `EGameXXKHandPresentationState`.
- Real-flow fields: `presentation.hand_state`, `presentation.arrived_count`, and `active_presentation_event`.

## Execution mode

The user selected option 1: execute this plan sequentially with fresh implementation, specification-review, and code-quality-review agents per task. The project-level `AGENTS.md` constraint overrides the generic worktree recommendation: all agents work carefully in the root project on `main`, stage only their task files, and preserve unrelated dirty changes.
