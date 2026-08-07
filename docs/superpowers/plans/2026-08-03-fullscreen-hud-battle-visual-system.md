# Fullscreen HUD Battle Visual System Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace battle-map travel and battle-scene actors with a reversible, current-map fullscreen HUD that plays every unit from the canonical 4096×4096 atlas set, presents synchronized attack/hit cinematics, and stays within a 256 MiB incremental texture budget.

**Architecture:** `UGameXXKBattleOverlayCoordinator` owns the reversible pause/render/input transaction, while `UGameXXKBattleBoardWidget` owns one 1920×1080 design stage containing the backdrop, formation visuals, existing battle controls, and cinematic layers. One `UGameXXKBattleUnitVisualWidget` and one MID remain attached to that stage for each unit; `FGameXXKBattleAtlasCache` asynchronously supplies canonical atlases and a marker-driven queue delays visible health, death, turn transitions, and battle exit until presentation catches up.

**Tech Stack:** Unreal Engine 5.8 C++, UMG/Slate, UI-domain dynamic materials, `FStreamableManager`, UE Automation Tests, focused editor Python import/validation, project UE MCP scripts, Python `unittest`, UBT cold builds.

---

## Guardrails and fixed contracts

- Work in `D:\UE5 demo\GameXXK` on `main`; do not create a worktree.
- Do not use UnrealBridge, Live Coding, Hot Reload, or synchronous atlas loading.
- Do not regenerate, overwrite, delete, or retune approved character, monster, PaperSprite, PaperFlipbook, level, camera, or HD2D assets.
- Do not invoke the paid video CLI. The production set is already present: 138 valid BC7-ready atlases plus one intentional missing gray-wolf attack fallback.
- Use the existing generic assets:
  - `/Game/GameXXK/BattleAnimations/Atlases/T_impact_ink_generic_atlas`
  - `/Game/GameXXK/BattleAnimations/Atlases/T_status_buff_generic_atlas`
  - `/Game/GameXXK/BattleAnimations/Atlases/T_status_debuff_generic_atlas`
- Canonical atlas contract: 4096×4096, 8×8, 512-pixel cells, frames `0..59`, 12 source FPS, no mips, bilinear, BC7 with alpha.
- Enemy visuals stay on the left and face right; party visuals stay on the right and face left. Runtime scale remains positive and never mirrors the authored images.
- Formation size is 410×410; cinematic participant size is exactly 820×820.
- Attack and hit run at 2× for 2.5 real seconds. Impact fires once at 1.1 real seconds, starts the generic impact at 4×, updates displayed health, emits readout text, and starts HUD-space shake.
- The route widget object and its scroll offset must survive battle entry and exit unchanged.
- The coordinator restores captured presentation/navigation state only. It must not restore a copied gameplay runtime state, because combat rewards and route progress are authoritative mutations.

## File structure

### New runtime files

| File | Responsibility |
|---|---|
| `Source/GameXXK/Public/UI/GameXXKBattleOverlayCoordinator.h` | Exact reversible overlay snapshot, testable host interface, session token, entry/exit contract. |
| `Source/GameXXK/Private/UI/GameXXKBattleOverlayCoordinator.cpp` | Pause/render/input/route transaction and idempotent cleanup. |
| `Source/GameXXK/Public/UI/GameXXKBattleUnitVisualWidget.h` | One persistent unit visual widget, one MID, formation/cinematic states, real-time frame stepping. |
| `Source/GameXXK/Private/UI/GameXXKBattleUnitVisualWidget.cpp` | UMG image/material creation and atlas frame parameter updates. |
| `Source/GameXXK/Public/UI/GameXXKBattleAtlasCache.h` | Coalesced asynchronous soft loads, pins, weighted LRU, timeout/cancel/session handling. |
| `Source/GameXXK/Private/UI/GameXXKBattleAtlasCache.cpp` | `FStreamableManager` implementation with 256 MiB cap and fallback callbacks. |

### New tests and asset scripts

| File | Responsibility |
|---|---|
| `Source/GameXXK/Private/Tests/GameXXKBattleOverlayCoordinatorTest.cpp` | Transaction restoration, idempotence, failure rollback, stale-session rejection. |
| `Source/GameXXK/Private/Tests/GameXXKBattleUnitVisualWidgetTest.cpp` | Same-instance layout changes, orientation, 60-frame real-time playback. |
| `Source/GameXXK/Private/Tests/GameXXKBattleAtlasCacheTest.cpp` | Async coalescing, failure, timeout, cancellation, weighted LRU. |
| `Content/Python/gamexxk_import_fullscreen_battle_hud_assets.py` | Import the approved backdrop and create the UI atlas material without changing authored art. |
| `scripts/test_fullscreen_battle_hud_asset_import.py` | Pure tests for source paths, asset paths, policy constants, and idempotent plan generation. |

### Existing files modified

| File | Change |
|---|---|
| `Source/GameXXK/Public/MVP/GameXXKLevelFlow.h` / `Private/MVP/GameXXKLevelFlow.cpp` | Keep battle on route map and expose a pure map-load decision. |
| `Source/GameXXK/Public/UI/GameXXKOneGameRouteMapWidget.h` / `Private/UI/GameXXKOneGameRouteMapWidget.cpp` | Read/write current scroll offset without recreating the widget. |
| `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h` / `Private/MVP/GameXXKMVPPlayerController.cpp` | Own coordinator; remove battle camera/presenter path; clean up on travel/destruction. |
| `Source/GameXXK/Public/GameXXKCardTypes.h` / `Private/GameXXKCardRules.cpp` | Capture before/after health on each atomic damage result. |
| `Source/GameXXK/Public/UI/GameXXKBattleAnimationPresentation.h` / `Private/UI/GameXXKBattleAnimationPresentation.cpp` | Immutable event payload, atlas/frame math, generic clip and fallback policy. |
| `Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h` / `Private/UI/GameXXKBattleBoardWidget.cpp` | Common design stage, unit registry, real-time queue, impact marker, visual health overlay, input/terminal gating. |
| `Source/GameXXK/Public/UI/GameXXKBattleAnimationLayerWidget.h` / `Private/UI/GameXXKBattleAnimationLayerWidget.cpp` | Retire duplicated participant images and synchronous loading; retain only reusable effect policy if still referenced. |
| `Content/Python/gamexxk_validate_battle_animation_texture_memory.py` | Validate all canonical atlases and ingest runtime cache statistics. |
| `scripts/test_battle_animation_texture_memory_validator.py` | Cover complete inventory and runtime-budget reports. |
| `scripts/gamexxk_real_play_flow_mcp.py`, `scripts/test_gamexxk_real_play_flow_mcp.py`, `scripts/test_gamexxk_real_play_flow_probe.py`, `Content/Python/gamexxk_probe_real_play_flow.py` | Prove same-map HUD flow and runtime presentation in PIE. |

## Standard verification commands

Use these exact commands throughout the tasks.

Cold editor build:

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -NoHotReload
```

Focused Automation Test template, replacing only the quoted test filter with the exact filter named in each task:

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.MVP.LevelFlow;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

MCP preflight and cold PIE smoke:

```powershell
python scripts\ue_mcp_smoke.py --report Saved\HarnessReports\fullscreen-battle-mcp-smoke.json
python scripts\ue_tdd_pipeline.py --pie-duration 0.2 --log-lines 600
```

The editor must be saved through UE MCP before any required editor restart. `scripts/ue_tdd_pipeline.py --filter` is not an Automation Test selector; use `UnrealEditor-Cmd.exe` for focused Automation Tests.

### Task 1: Keep route battles in the current map

**Files:**
- Modify: `Source/GameXXK/Public/MVP/GameXXKLevelFlow.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKLevelFlow.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKLevelFlowTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKOneGameRouteMapAdapterTest.cpp`

- [ ] **Step 1: Write the failing same-map tests**

Add assertions that Battle resolves to the route map and that no load is required while the current package is the route map:

```cpp
TestEqual(TEXT("battle stays on route map"),
    GameXXKLevelFlow::MapForScreen(EGameXXKScreen::Battle),
    FName(TEXT("/Game/GameXXK/Maps/L_RouteMap")));
TestFalse(TEXT("battle overlay does not request route-map reload"),
    GameXXKLevelFlow::RequiresMapLoadForRuntimeState(
        TEXT("/Game/GameXXK/Maps/L_RouteMap"), BattleState));
TestTrue(TEXT("loading a battle save from town still opens route map"),
    GameXXKLevelFlow::RequiresMapLoadForRuntimeState(
        TEXT("/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo"), BattleState));
```

Change the route adapter assertion from `L_BattleTown` to `L_RouteMap` while preserving node execution and route progress assertions.

- [ ] **Step 2: Run the focused tests and verify RED**

Run Automation filters `GameXXK.MVP.LevelFlow` and `GameXXK.MVP.RouteMap.OneGameAdapter` with the standard editor command. Expected: compile failure because `RequiresMapLoadForRuntimeState` does not exist and/or the old battle-map assertion fails.

- [ ] **Step 3: Implement the pure load decision**

Expose and implement:

```cpp
GAMEXXK_API bool RequiresMapLoadForRuntimeState(
    const FString& CurrentPackageName,
    const FGameXXKRuntimeState& State);
```

```cpp
case EGameXXKScreen::Battle:
    return RouteMap;

bool GameXXKLevelFlow::RequiresMapLoadForRuntimeState(
    const FString& CurrentPackageName,
    const FGameXXKRuntimeState& State)
{
    const FName TargetMap = MapForRuntimeState(State);
    return !TargetMap.IsNone() && !MapPackageMatches(CurrentPackageName, TargetMap);
}
```

Make `OpenMapForRuntimeState` call this pure function before `UGameplayStatics::OpenLevel`.

- [ ] **Step 4: Rebuild and verify GREEN**

Run the cold build, then both focused filters. Expected: both tests pass and battle-node execution no longer names `L_BattleTown`.

- [ ] **Step 5: Commit**

```powershell
git add Source/GameXXK/Public/MVP/GameXXKLevelFlow.h Source/GameXXK/Private/MVP/GameXXKLevelFlow.cpp Source/GameXXK/Private/Tests/GameXXKLevelFlowTest.cpp Source/GameXXK/Private/Tests/GameXXKOneGameRouteMapAdapterTest.cpp
git commit -m "feat: keep route battles in current map"
```

### Task 2: Add the reversible battle-overlay transaction

**Files:**
- Create: `Source/GameXXK/Public/UI/GameXXKBattleOverlayCoordinator.h`
- Create: `Source/GameXXK/Private/UI/GameXXKBattleOverlayCoordinator.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKBattleOverlayCoordinatorTest.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKOneGameRouteMapWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKOneGameRouteMapWidget.cpp`

- [ ] **Step 1: Write failing transaction tests**

Create Automation filter `GameXXK.MVP.Battle.OverlayCoordinator`. Use a fake `IGameXXKBattleOverlayHost` that records capture/apply/restore/cancel order. Cover:

```cpp
FGameXXKBattleOverlaySnapshot Before;
Before.bGamePaused = true;
Before.bWorldRenderingEnabled = false;
Before.bShowMouseCursor = false;
Before.bEnableClickEvents = false;
Before.bEnableMouseOverEvents = true;
Before.bMoveInputIgnored = true;
Before.bLookInputIgnored = false;
Before.InputMode = EGameXXKTrackedInputMode::GameOnly;
Before.RouteVisibility = ESlateVisibility::SelfHitTestInvisible;
Before.RouteScrollOffset = 713.0f;

Coordinator.Enter(Host, RouteWidget, BattleWidget);
Coordinator.Exit(Host);
TestTrue(TEXT("exact snapshot restored"), Host.Matches(Before));
TestEqual(TEXT("same route widget restored"), Host.RouteWidget, RouteWidget);
TestEqual(TEXT("scroll restored"), RouteWidget->GetCurrentScrollOffset(), 713.0f);
```

Also assert: failed entry rolls back; repeated `Exit` is harmless; destruction cleanup restores state; travel cleanup restores state; session A callbacks are rejected after session B starts.

- [ ] **Step 2: Run the new filter and verify RED**

Run `GameXXK.MVP.Battle.OverlayCoordinator`. Expected: compile failure because the coordinator and scroll accessors do not exist.

- [ ] **Step 3: Implement snapshot, token, and exact restore**

Define the stable public contract. The coordinator owns transaction order and idempotence; the host owns engine-specific state reads/writes so Task 3 can adapt the existing player controller without coupling this class back to MVP internals:

```cpp
enum class EGameXXKTrackedInputMode : uint8 { GameOnly, GameAndUI, UIOnly };

struct FGameXXKBattleOverlaySnapshot
{
    bool bGamePaused = false;
    bool bWorldRenderingEnabled = true;
    bool bShowMouseCursor = false;
    bool bEnableClickEvents = false;
    bool bEnableMouseOverEvents = false;
    bool bMoveInputIgnored = false;
    bool bLookInputIgnored = false;
    EGameXXKTrackedInputMode InputMode = EGameXXKTrackedInputMode::GameAndUI;
    ESlateVisibility RouteVisibility = ESlateVisibility::Visible;
    float RouteScrollOffset = 0.0f;
};

UCLASS()
class GAMEXXK_API UGameXXKBattleOverlayCoordinator : public UObject
{
    GENERATED_BODY()
public:
    bool Enter(IGameXXKBattleOverlayHost& Host,
        UGameXXKOneGameRouteMapWidget& RouteWidget,
        UGameXXKBattleBoardWidget& BattleWidget);
    void Exit(IGameXXKBattleOverlayHost& Host);
    void InvalidateSession();
    uint64 GetSessionToken() const;
    bool IsCurrentSession(uint64 Candidate) const;
    bool IsActive() const;
};
```

Define `IGameXXKBattleOverlayHost` in the same header with `CaptureBattleOverlaySnapshot`, `ApplyBattleOverlayEntry`, `CancelBattleVisualLoads`, and `RestoreBattleOverlaySnapshot`. `Enter` captures, records the exact route widget/scroll, increments the token, and calls `ApplyBattleOverlayEntry`; a false return immediately restores. `Exit` invalidates the token before cancellation, then restores exactly once. Task 3 makes `AGameXXKMVPPlayerController` implement this host interface. Its entry implementation will flush pressed keys; acquire only missing move/look ignore increments; pause with `UGameplayStatics::SetGamePaused`; disable rendering with `UGameplayStatics::SetEnableWorldRendering`; collapse route; show battle; and apply tracked UI-only input focused on battle. Its restore implementation reverses those operations from the captured snapshot.

Add exact route scroll accessors:

```cpp
float UGameXXKOneGameRouteMapWidget::GetCurrentScrollOffset() const;
void UGameXXKOneGameRouteMapWidget::RestoreScrollOffset(float InOffset);
```

Prevent `RefreshFromState()` from calling `ApplyInitialScrollOffset()` after the first initialization.

- [ ] **Step 4: Rebuild and verify GREEN**

Run the cold build and `GameXXK.MVP.Battle.OverlayCoordinator`. Expected: all ordinary, non-default initial-state, rollback, destruction, travel, idempotence, and stale-session cases pass.

- [ ] **Step 5: Commit**

```powershell
git add Source/GameXXK/Public/UI/GameXXKBattleOverlayCoordinator.h Source/GameXXK/Private/UI/GameXXKBattleOverlayCoordinator.cpp Source/GameXXK/Private/Tests/GameXXKBattleOverlayCoordinatorTest.cpp Source/GameXXK/Public/UI/GameXXKOneGameRouteMapWidget.h Source/GameXXK/Private/UI/GameXXKOneGameRouteMapWidget.cpp
git commit -m "feat: add reversible battle overlay transaction"
```

### Task 3: Wire the controller without touching the camera

**Files:**
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKPlayerFlowWidgetTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBattleInputBridgeTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp`

- [ ] **Step 1: Write failing controller tests**

Extend `GameXXK.MVP.UI.PlayerControllerOwnsFlowWidgets` and `GameXXK.Integration.CardBattle.ControllerInputBridge` to capture the active camera/view target before battle entry and assert:

```cpp
TestEqual(TEXT("map package unchanged"), WorldPackageAfter, WorldPackageBefore);
TestEqual(TEXT("view target unchanged"), Controller->GetViewTarget(), ViewTargetBefore);
TestEqual(TEXT("camera location unchanged"), CameraAfter.Location, CameraBefore.Location);
TestEqual(TEXT("camera rotation unchanged"), CameraAfter.Rotation, CameraBefore.Rotation);
TestEqual(TEXT("camera fov unchanged"), CameraAfter.FOV, CameraBefore.FOV);
TestTrue(TEXT("route widget identity retained"), Controller->GetRouteMapWidget() == RouteWidgetBefore);
TestTrue(TEXT("battle coordinator active"), Controller->IsBattleOverlayActive());
```

Repurpose `GameXXK.MVP.Battle.SceneActors` to assert the runtime player-flow path spawns no presenter, unit actor, or battle camera and uses HUD-root shake state instead.

- [ ] **Step 2: Run the three filters and verify RED**

Run the three exact filters above. Expected: old presenter/camera behavior breaks the new assertions.

- [ ] **Step 3: Replace presenter/camera calls with coordinator calls**

Make `AGameXXKMVPPlayerController` implement `IGameXXKBattleOverlayHost`, providing exact engine-specific capture/apply/cancel/restore methods. Add controller ownership:

```cpp
UPROPERTY(Transient)
TObjectPtr<UGameXXKBattleOverlayCoordinator> BattleOverlayCoordinator;

void EnterBattleOverlay();
void ExitBattleOverlay();
bool IsBattleOverlayActive() const;
```

Route all controller input-mode mutations through one tracked helper. On `RefreshFromState`, enter the overlay when `Screen == Battle`; exit when leaving Battle. Delete calls from the runtime path to `EnsureBattleScenePresenter`, `ApplyBattleSceneCamera`, `SetViewTarget`, battle-scene actor refresh, and camera-manager shake. Keep the old assets/classes in the repository. Call `ExitBattleOverlay()` in `EndPlay` and immediately before actual world travel.

- [ ] **Step 4: Rebuild and verify GREEN**

Run the cold build and all three focused filters. Expected: same controller widgets, no camera mutation, no runtime presentation actors, cleanup on destruction/travel.

- [ ] **Step 5: Commit**

```powershell
git add Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp Source/GameXXK/Private/Tests/GameXXKPlayerFlowWidgetTest.cpp Source/GameXXK/Private/Tests/GameXXKCardBattleInputBridgeTest.cpp Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp
git commit -m "refactor: drive battle presentation from fullscreen hud"
```

### Task 4: Capture immutable before/after combat presentation data

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKCardTypes.h`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKBattleAnimationPresentation.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleAnimationPresentation.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardCombatRulesTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleAnimationPresentationTest.cpp`

- [ ] **Step 1: Write failing snapshot and event tests**

Assert an ordinary hit, agility avoid, guard redirect, armor-only hit, reflected hit, and lethal hit all preserve the actual resolved target health boundary. Add a multi-hit event-order test:

```cpp
TestEqual(TEXT("first hit before"), Events[0].TargetHealthBefore, 100);
TestEqual(TEXT("first hit after"), Events[0].TargetHealthAfter, 100);
TestTrue(TEXT("first hit avoided"), Events[0].bAvoided);
TestEqual(TEXT("second hit before"), Events[1].TargetHealthBefore, 100);
TestEqual(TEXT("second hit after"), Events[1].TargetHealthAfter, 86);
TestEqual(TEXT("third hit before"), Events[2].TargetHealthBefore, 86);
TestEqual(TEXT("third hit after"), Events[2].TargetHealthAfter, 72);
TestTrue(TEXT("ordinals stable"), Events[2].HitOrdinal == 2);
```

- [ ] **Step 2: Run tests and verify RED**

Run `GameXXK.Data.CardCombatRules` and `GameXXK.MVP.Battle.AnimationPresentation`. Expected: compile failure for the new fields/event type.

- [ ] **Step 3: Add health snapshots and immutable event conversion**

Extend `FGameXXKCardDamageResult`:

```cpp
UPROPERTY(BlueprintReadWrite, EditAnywhere)
int32 TargetHealthBefore = 0;

UPROPERTY(BlueprintReadWrite, EditAnywhere)
int32 TargetHealthAfter = 0;
```

At the point where `ApplyCombatDirectDamageInternal` has resolved redirect/avoid and has a resolved target, capture `TargetHealthBefore` before mutation and `TargetHealthAfter` after every return path. Define:

```cpp
struct GAMEXXK_API FGameXXKBattlePresentationEvent
{
    uint64 EventId = 0;
    int32 HitOrdinal = 0;
    FName AttackerUnitId = NAME_None;
    FName TargetUnitId = NAME_None;
    bool bAttackerEnemy = false;
    bool bTargetEnemy = false;
    int32 HealthDamage = 0;
    int32 TargetHealthBefore = 0;
    int32 TargetHealthAfter = 0;
    bool bAvoided = false;
    bool bTargetDefeated = false;
};
```

Replace `BuildCombatRequests` with `BuildPresentationEvents`, preserving damage-result array order and using the resolved source/target IDs. Only mark death on the final lethal event for a unit.

- [ ] **Step 4: Rebuild and verify GREEN**

Run the cold build and both focused filters. Expected: snapshot values match actual mutation, redirect source/target IDs remain stable, multi-hit order is deterministic.

- [ ] **Step 5: Commit**

```powershell
git add Source/GameXXK/Public/GameXXKCardTypes.h Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Public/UI/GameXXKBattleAnimationPresentation.h Source/GameXXK/Private/UI/GameXXKBattleAnimationPresentation.cpp Source/GameXXK/Private/Tests/GameXXKCardCombatRulesTest.cpp Source/GameXXK/Private/Tests/GameXXKBattleAnimationPresentationTest.cpp
git commit -m "feat: snapshot combat health for presentation"
```

### Task 5: Lock atlas frame math and fallback descriptors

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKBattleAnimationPresentation.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleAnimationPresentation.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleAnimationPresentationTest.cpp`

- [ ] **Step 1: Add failing contract tests**

Test frames 0, 7, 8, 59; normal loop wrap; non-loop clamp; 2× duration; generic effect paths; and the intentional gray-wolf attack miss:

```cpp
TestEqual(TEXT("frame 59 column"), UV59.Min.X, 7.0f / 8.0f);
TestEqual(TEXT("frame 59 row"), UV59.Min.Y, 7.0f / 8.0f);
TestEqual(TEXT("loop never samples cell 60"), CalculateFrameIndex(Clip, 5.0f, true), 0);
TestEqual(TEXT("attack duration"), GetRuntimeDuration(AttackClip), 2.5f);
TestEqual(TEXT("impact marker"), GetImpactRuntimeSeconds(), 1.1f);
TestEqual(TEXT("impact playback"), ResolveGenericClip(Impact).PlaybackRate, 4.0f);
TestTrue(TEXT("buff generic exists"), ResolveGenericClip(Buff).IsValid());
TestTrue(TEXT("debuff generic exists"), ResolveGenericClip(Debuff).IsValid());
TestFalse(TEXT("gray wolf attack uses runtime fallback"), ResolveClip(TEXT("Enemy_07"), true, Attack).IsValid());
```

- [ ] **Step 2: Run `GameXXK.MVP.Battle.AnimationPresentation` and verify RED**

Expected: at least the generic playback/fallback assertions fail against the current policy or the tightened boundary checks expose a mismatch.

- [ ] **Step 3: Make the descriptor contract explicit**

Keep frame count at 60, force modulo/clamp before row/column math, return 2× for Attack/Hit, 4× for Impact, 1× for Idle/Death/Buff/Debuff, and preserve the three exact generic soft paths. Remove runtime dependence on `ResolveIdleFlipbookPath`; leave it only as a deprecated compatibility function until Task 12 removes callers.

- [ ] **Step 4: Rebuild and verify GREEN**

Run the cold build and the focused filter. Expected: no frame index can produce cells 60–63.

- [ ] **Step 5: Commit**

```powershell
git add Source/GameXXK/Public/UI/GameXXKBattleAnimationPresentation.h Source/GameXXK/Private/UI/GameXXKBattleAnimationPresentation.cpp Source/GameXXK/Private/Tests/GameXXKBattleAnimationPresentationTest.cpp
git commit -m "test: lock battle atlas playback contract"
```

### Task 6: Import the fullscreen backdrop and UI atlas material

**Files:**
- Create: `Content/Python/gamexxk_import_fullscreen_battle_hud_assets.py`
- Create: `scripts/test_fullscreen_battle_hud_asset_import.py`
- Preserve: `SourceAssets/PartyDeck/battle-backdrop/battle_arena_riverside_source_v1.png`
- Create through UE import: `Content/GameXXK/UI/Battle/Textures/T_BattleArena_Riverside_GeneratedV1.uasset`
- Create through UE import: `Content/GameXXK/UI/Battle/Materials/M_BattleUnitAtlasUI.uasset`

- [ ] **Step 1: Write failing pure tests**

Test exact paths and policy without launching UE:

```python
def test_asset_contract(self):
    self.assertEqual(SOURCE_IMAGE.name, "battle_arena_riverside_source_v1.png")
    self.assertEqual(BACKDROP_PACKAGE, "/Game/GameXXK/UI/Battle/Textures/T_BattleArena_Riverside_GeneratedV1")
    self.assertEqual(MATERIAL_PACKAGE, "/Game/GameXXK/UI/Battle/Materials/M_BattleUnitAtlasUI")
    self.assertEqual(ATLAS_COLUMNS, 8)
    self.assertEqual(ATLAS_ROWS, 8)
    self.assertEqual(SOURCE_IMAGE_SHA256, "ab8b882de676b74693cb2d3c279ca11f2126aaa4fdf382dedaa810b34ac1a3a8")
```

Also assert the script contains no world-plane lookup, no level actor mutation, and no authored asset deletion.

- [ ] **Step 2: Run pure tests and verify RED**

```powershell
python -B -m unittest scripts.test_fullscreen_battle_hud_asset_import -v
```

Expected: import failure because the new script does not exist.

- [ ] **Step 3: Implement idempotent asset creation**

The script must:

```python
SOURCE_IMAGE = PROJECT_ROOT / "SourceAssets/PartyDeck/battle-backdrop/battle_arena_riverside_source_v1.png"
BACKDROP_PACKAGE = "/Game/GameXXK/UI/Battle/Textures/T_BattleArena_Riverside_GeneratedV1"
MATERIAL_PACKAGE = "/Game/GameXXK/UI/Battle/Materials/M_BattleUnitAtlasUI"
ATLAS_COLUMNS = 8
ATLAS_ROWS = 8
```

Import the backdrop as a UI texture with sRGB enabled, no mips, and bilinear filtering. Create a UI-domain translucent unlit material with scalar parameters `FrameColumn` and `FrameRow`, texture parameter `AtlasTexture`, and UV expression `TexCoord / 8 + float2(FrameColumn, FrameRow) / 8`. Save only the generated texture and material packages. Exit nonzero if the source hash differs.

- [ ] **Step 4: Run pure tests, then import through UE tooling**

Run the pure test and expect PASS. Start the editor through the project’s normal UE 5.8 path, execute the script with focused editor Python through UE MCP, save the two generated packages, then verify both package paths through UE MCP. Do not open or modify `L_BattleScene` or `L_BattleTown`.

- [ ] **Step 5: Commit**

```powershell
git add Content/Python/gamexxk_import_fullscreen_battle_hud_assets.py scripts/test_fullscreen_battle_hud_asset_import.py Content/GameXXK/UI/Battle/Textures/T_BattleArena_Riverside_GeneratedV1.uasset Content/GameXXK/UI/Battle/Materials/M_BattleUnitAtlasUI.uasset
git commit -m "feat: import fullscreen battle hud assets"
```

### Task 7: Implement one persistent atlas widget per unit

**Files:**
- Create: `Source/GameXXK/Public/UI/GameXXKBattleUnitVisualWidget.h`
- Create: `Source/GameXXK/Private/UI/GameXXKBattleUnitVisualWidget.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKBattleUnitVisualWidgetTest.cpp`

- [ ] **Step 1: Write failing widget-state tests**

Create filter `GameXXK.UI.Battle.UnitVisualWidget`. Verify the widget remains the same object and parent across transitions:

```cpp
Widget->ConfigureUnit(TEXT("Hero"), false, PartySlot, IdleClip);
UPanelWidget* OriginalParent = Widget->GetParent();
Widget->ShowFormationIdle();
TestEqual(TEXT("formation size"), Widget->GetPresentedSize(), FVector2D(410.0f));
Widget->ShowCinematic(AttackClip, CinematicAnchor);
TestTrue(TEXT("same widget"), Widget == OriginalPointer);
TestTrue(TEXT("same parent"), Widget->GetParent() == OriginalParent);
TestEqual(TEXT("cinematic size exactly doubled"), Widget->GetPresentedSize(), FVector2D(820.0f));
TestTrue(TEXT("authored orientation retained"), Widget->GetRenderTransform().Scale.X > 0.0f);
```

Advance a fake real-time clock while world delta is zero and assert frame 0→7→8→59→0. Assert hidden widgets do not write material parameters.

- [ ] **Step 2: Run the new filter and verify RED**

Expected: compile failure because `UGameXXKBattleUnitVisualWidget` does not exist.

- [ ] **Step 3: Implement the state-only widget API**

Use this public surface:

```cpp
void ConfigureUnit(FName UnitId, bool bEnemy,
    const FVector2D& FormationAnchor,
    const FGameXXKBattleAnimationClipDescriptor& IdleClip);
void SetAtlas(UTexture2D* AtlasTexture);
void SetPlaybackClip(const FGameXXKBattleAnimationClipDescriptor& Clip, bool bLooping);
void ShowFormationIdle();
void ShowCinematic(const FGameXXKBattleAnimationClipDescriptor& Clip, const FVector2D& Anchor);
void HideForCinematic();
void RestoreFormation();
void RemoveAfterDeath();
void AdvanceAtRealTime(double AbsoluteSeconds);
FVector2D GetPresentedSize() const;
```

Create one `UImage` and one MID from `/Game/GameXXK/UI/Battle/Materials/M_BattleUnitAtlasUI`. Set `AtlasTexture`, `FrameColumn`, and `FrameRow`; update the existing canvas slot’s position, size, and Z order. Never reparent, duplicate, or apply negative scale.

- [ ] **Step 4: Rebuild and verify GREEN**

Run the cold build and `GameXXK.UI.Battle.UnitVisualWidget`. Expected: all state, layout, clock, and orientation assertions pass.

- [ ] **Step 5: Commit**

```powershell
git add Source/GameXXK/Public/UI/GameXXKBattleUnitVisualWidget.h Source/GameXXK/Private/UI/GameXXKBattleUnitVisualWidget.cpp Source/GameXXK/Private/Tests/GameXXKBattleUnitVisualWidgetTest.cpp
git commit -m "feat: add persistent atlas unit widget"
```

### Task 8: Add coalesced asynchronous atlas loading with a 256 MiB LRU

**Files:**
- Create: `Source/GameXXK/Public/UI/GameXXKBattleAtlasCache.h`
- Create: `Source/GameXXK/Private/UI/GameXXKBattleAtlasCache.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKBattleAtlasCacheTest.cpp`

- [ ] **Step 1: Write failing cache tests**

Create filter `GameXXK.UI.Battle.AtlasCache`. Use an injectable fake loader and clock. Assert duplicate requests use one load, active pins are not evicted, completion calls every waiter, stale tokens are rejected, timeout/failure/cancel invoke fallback exactly once, and weighted resident bytes never exceed `256 * 1024 * 1024` after unpinned eviction.

```cpp
Cache.Acquire(Path, Session, CallbackA);
Cache.Acquire(Path, Session, CallbackB);
TestEqual(TEXT("coalesced request count"), Loader.RequestCount(Path), 1);
Loader.Complete(Path, Texture, 16 * 1024 * 1024);
TestEqual(TEXT("both waiters complete"), CompletedCallbacks, 2);
TestTrue(TEXT("resident cap honored"), Cache.GetResidentBytes() <= 256ll * 1024ll * 1024ll);
```

- [ ] **Step 2: Run the new filter and verify RED**

Expected: compile failure because `FGameXXKBattleAtlasCache` does not exist.

- [ ] **Step 3: Implement cache ownership and fallback semantics**

Expose:

```cpp
struct FGameXXKBattleAtlasCacheStats
{
    int64 ResidentBytes = 0;
    int64 PeakResidentBytes = 0;
    int32 SyncLoadCount = 0;
    int32 ForcedGcCount = 0;
    int32 ActiveRequestCount = 0;
};

void Acquire(const FSoftObjectPath& Path, uint64 SessionToken,
    TFunction<void(UTexture2D*, EGameXXKAtlasLoadResult)> Completion);
void Pin(const FSoftObjectPath& Path);
void Unpin(const FSoftObjectPath& Path);
void AdvanceTimeouts(double AbsoluteSeconds);
void CancelSession(uint64 SessionToken);
void Clear();
const FGameXXKBattleAtlasCacheStats& GetStats() const;
```

Production loading uses `UAssetManager::GetStreamableManager().RequestAsyncLoad`. Maintain one handle and waiter list per path. Update weighted LRU on successful access, evict oldest unpinned entries until under budget, never call `TryLoad`, `LoadSynchronous`, `CollectGarbage`, or forced GC. The callback carries `Loaded`, `Missing`, `TimedOut`, `Cancelled`, or `StaleSession` so the board can select idle/placeholder fallback and continue.

- [ ] **Step 4: Rebuild and verify GREEN**

Run the cold build and `GameXXK.UI.Battle.AtlasCache`. Expected: all asynchronous and budget cases pass; stats report zero synchronous loads and zero forced GCs.

- [ ] **Step 5: Commit**

```powershell
git add Source/GameXXK/Public/UI/GameXXKBattleAtlasCache.h Source/GameXXK/Private/UI/GameXXKBattleAtlasCache.cpp Source/GameXXK/Private/Tests/GameXXKBattleAtlasCacheTest.cpp
git commit -m "feat: add asynchronous battle atlas cache"
```

### Task 9: Build the common 1920×1080 stage and formation visuals

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleBoardWidgetTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleProjectedUnitHudTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBattleInputBridgeTest.cpp`

- [ ] **Step 1: Write failing common-stage tests**

Assert one `1920×1080` design stage owns every battle element and one visual widget exists for every active unit. Verify exact slots:

```cpp
const FVector2D EnemySlots[] = {{0.095f, 0.60f}, {0.245f, 0.52f}, {0.395f, 0.44f}};
const FVector2D PartySlots[] = {{0.905f, 0.60f}, {0.755f, 0.52f}, {0.605f, 0.44f}};
TestEqual(TEXT("backdrop z"), Board->GetLayerZ(EGameXXKBattleHudLayer::Backdrop), 0);
TestEqual(TEXT("formation z"), Board->GetLayerZ(EGameXXKBattleHudLayer::Formation), 10);
TestEqual(TEXT("controls z"), Board->GetLayerZ(EGameXXKBattleHudLayer::Controls), 20);
TestTrue(TEXT("target bridge uses unit widget"), TargetPoint == VisualWidget->GetStageCenter());
```

Assert the backdrop uses scale-to-fill centered cropping, visual idles are requested asynchronously, missing idle shows a selectable placeholder, and existing unit HUD/cards remain above formation visuals.

- [ ] **Step 2: Run the three focused filters and verify RED**

Run `GameXXK.MVP.Battle.BoardWidget`, `GameXXK.UI.Battle.FixedSlotUnitHud`, and `GameXXK.Integration.CardBattle.ControllerInputBridge`. Expected: current separate layer/scene projection behavior fails.

- [ ] **Step 3: Rebuild the board root without changing card rules**

Construct an opaque root backdrop using `T_BattleArena_Riverside_GeneratedV1`, then a ScaleToFit 1920×1080 `CanvasPanel` design stage. Move existing controls into the stage at Z=20. Register unit visuals by stable UnitId at Z=10:

```cpp
TMap<FName, TObjectPtr<UGameXXKBattleUnitVisualWidget>> UnitVisuals;
TUniquePtr<FGameXXKBattleAtlasCache> AtlasCache;
double LastSlateSeconds = 0.0;
```

At battle entry request only active idle atlases. Use `FSlateApplicationBase::Get().GetCurrentTime()` and an `AdvanceVisualsAtRealTime(double AbsoluteSeconds)` test seam. Update targeting geometry from the UMG unit widget, not a projected scene actor.

- [ ] **Step 4: Rebuild and verify GREEN**

Run the cold build and all three filters. Expected: canonical anchors, same UMG targets, correct layer order, placeholder selection, and paused-world idle advancement pass.

- [ ] **Step 5: Commit**

```powershell
git add Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp Source/GameXXK/Private/Tests/GameXXKBattleBoardWidgetTest.cpp Source/GameXXK/Private/Tests/GameXXKBattleProjectedUnitHudTest.cpp Source/GameXXK/Private/Tests/GameXXKCardBattleInputBridgeTest.cpp
git commit -m "feat: render battle formation in common hud stage"
```

### Task 10: Implement marker-driven synchronized attack and hit cinematics

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleAnimationLayerWidgetTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleProjectedUnitHudTest.cpp`

- [ ] **Step 1: Write failing queue/timeline tests**

Repurpose `GameXXK.MVP.Battle.AnimationLayerWidget` as a board-presentation timeline test. Assert:

```cpp
Board->QueuePresentation(Event);
Board->AdvanceVisualsAtRealTime(0.0);
TestTrue(TEXT("all formation hidden"), Board->AreFormationUnitsHidden());
TestEqual(TEXT("participants doubled"), Board->GetCinematicSize(), FVector2D(820.0f));
Board->AdvanceVisualsAtRealTime(1.099);
TestEqual(TEXT("health still before marker"), Board->GetDisplayedHealth(Target), Event.TargetHealthBefore);
Board->AdvanceVisualsAtRealTime(1.101);
TestEqual(TEXT("health updates at marker"), Board->GetDisplayedHealth(Target), Event.TargetHealthAfter);
TestEqual(TEXT("impact starts once"), Board->GetImpactCount(), 1);
TestEqual(TEXT("hud shake starts once"), Board->GetHudShakeCount(), 1);
Board->AdvanceVisualsAtRealTime(2.5);
TestTrue(TEXT("survivors restored"), Board->IsFormationIdle(Target));
```

Add a large-delta case `0.0 → 3.0` that still fires the marker once, completes once, and carries `0.5` seconds of overflow into the next queued event.

- [ ] **Step 2: Run timeline and HUD tests and verify RED**

Run `GameXXK.MVP.Battle.AnimationLayerWidget` and `GameXXK.UI.Battle.FixedSlotUnitHud`. Expected: current separate images, early health refresh, synchronous load, or large-delta behavior fails.

- [ ] **Step 3: Implement absolute-time boundary crossing**

Add a queue state with `StartSeconds`, `bImpactFired`, `bCompletionFired`, and immutable event. Prefetch attacker attack, target hit, and generic impact before playback; use loaded idle plus impact if the action texture is unavailable. On start: hide every formation visual, move only participants to side-stable central anchors, set 820×820, start Attack/Hit at 2×, and show 50% black dimmer at Z=30. On crossing 1.1 seconds exactly once: start Impact at 4× at exact screen center Z=50, start HUD-root translation shake, update the displayed-health overlay, and show damage/avoid text at Z=60. On crossing 2.5 seconds: restore survivors; if lethal, enqueue Death before removal; consume overflow into the next event.

Keep all shake in the design-stage render transform. Never call camera-manager shake or mutate view target.

- [ ] **Step 4: Rebuild and verify GREEN**

Run the cold build and both filters. Expected: timing, large delta, overflow, exact-once markers, side/orientation, visible health, damage text, and fallback behavior pass.

- [ ] **Step 5: Commit**

```powershell
git add Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp Source/GameXXK/Private/Tests/GameXXKBattleAnimationLayerWidgetTest.cpp Source/GameXXK/Private/Tests/GameXXKBattleProjectedUnitHudTest.cpp
git commit -m "feat: add synchronized battle action timeline"
```

### Task 11: Gate gameplay transitions and integrate multi-hit/status presentation

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBattleBoardWidgetTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBattleBoardEnemyIntentPresentationTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleStatusEffectsWidgetTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBattleRuntimeTest.cpp`

- [ ] **Step 1: Write failing input/terminal/multi-hit tests**

Add Automation filter `GameXXK.Integration.CardBattle.BoardPresentationGate` to `GameXXKCardBattleBoardWidgetTest.cpp`. Cover player card input, targeting, end-turn input, and enemy intent advancement while the queue is nonempty. Assert a three-packet action plays sequentially, and terminal state waits:

```cpp
TestFalse(TEXT("second hit cannot start early"), Board->HasStartedEvent(Events[1]));
Board->AdvanceVisualsAtRealTime(2.5);
TestTrue(TEXT("second hit starts after first"), Board->HasStartedEvent(Events[1]));
TestFalse(TEXT("victory not exposed before lethal death"), Board->HasClosedBattleOverlay());
Board->DrainPresentationForTest();
TestTrue(TEXT("victory closes after death"), Board->HasClosedBattleOverlay());
TestTrue(TEXT("route reward committed once"), RewardCount == 1);
```

For status changes, assert a positive status delta queues the generic Buff clip and a negative status delta queues the generic Debuff clip, with the large status icon/readout at center and no per-status video load.

- [ ] **Step 2: Run the four focused filters and verify RED**

Run `GameXXK.Integration.CardBattle.BoardPresentationGate`, `GameXXK.Integration.CardBattle.BoardEnemyIntentPresentation`, `GameXXK.UI.Battle.StatusEffectsWidget`, and `GameXXK.Data.CardBattleRuntime`. Expected: runtime mutation currently refreshes/terminates or advances intent before presentation drain.

- [ ] **Step 3: Add a presentation lock and deferred completion**

Add:

```cpp
bool bPresentationLocked = false;
TOptional<EGameXXKCardBattlePhase> DeferredTerminalPhase;
TFunction<void()> DeferredTurnContinuation;
```

After deterministic rule resolution, create presentation events before ordinary widget refresh exposes post-state. While locked, reject card clicks, target clicks, end turn, and enemy-intent phase advancement. Apply only presentation overlays at markers. When the queue and any lethal death clip are empty: reconcile all visible HUD from authoritative runtime, run the stored terminal/reward/turn continuation once, and unlock. Preserve packet order for reflected/reactive damage. Use only the existing generic buff/debuff atlases for all status animations.

- [ ] **Step 4: Rebuild and verify GREEN**

Run the cold build and all four filters. Expected: sequential multi-hit, exact-once reward/terminal, delayed enemy intents, locked input, generic status effects, and final runtime reconciliation pass.

- [ ] **Step 5: Commit**

```powershell
git add Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp Source/GameXXK/Private/Tests/GameXXKCardBattleBoardWidgetTest.cpp Source/GameXXK/Private/Tests/GameXXKCardBattleBoardEnemyIntentPresentationTest.cpp Source/GameXXK/Private/Tests/GameXXKBattleStatusEffectsWidgetTest.cpp Source/GameXXK/Private/Tests/GameXXKCardBattleRuntimeTest.cpp
git commit -m "feat: synchronize battle rules with hud presentation"
```

### Task 12: Retire runtime scene actors, flipbooks, and duplicate animation images

**Files:**
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKBattleAnimationLayerWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleAnimationLayerWidget.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleActorHudRetirementTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKPartyDeckBattleSceneUnitActorTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleBoardWidgetTest.cpp`
- Preserve: all existing `Content/GameXXK/BattleAnimations/Flipbooks`, sprites, scene actors, levels, meshes, and manually tuned assets

- [ ] **Step 1: Write failing retirement assertions**

Update filter `GameXXK.UI.Battle.ActorHudRetirement` to scan the runtime controller/board path and assert:

```cpp
TestEqual(TEXT("runtime scene presenter count"), PresenterCount, 0);
TestEqual(TEXT("runtime unit actor count"), BattleUnitActorCount, 0);
TestEqual(TEXT("runtime flipbook component count"), BattleFlipbookTickCount, 0);
TestEqual(TEXT("duplicate central participant images"), Board->GetDuplicateParticipantImageCount(), 0);
TestEqual(TEXT("synchronous animation loads"), Board->GetAtlasCacheStats().SyncLoadCount, 0);
```

Keep content-existence assertions for old actor/flipbook assets so the migration cannot delete shared content.

- [ ] **Step 2: Run retirement tests and verify RED**

Run `GameXXK.UI.Battle.ActorHudRetirement` and `GameXXK.MVP.Battle.PartyDeckVisualMapping`. Expected: runtime references or duplicate layer images remain.

- [ ] **Step 3: Remove runtime call sites only**

Delete controller/board calls that spawn/fetch/update `AGameXXKBattleScenePresenter`, `AGameXXKBattleSceneUnitActor`, battle cameras, or idle PaperFlipbooks. Remove `LeftAnimationImage` and `RightAnimationImage` from the active animation-layer path; route all participant/effect work through board-owned unit widgets and direct-stage effect images. Remove all action-path `TryLoad()` calls. Do not delete content files or retune assets.

After the legacy projected presentation is retired, shift every formation character and monster visual slightly upward in 1920x1080 design-stage space so the compact name/intent strip and health/armor/status row have clear room around the unit instead of covering the illustration. Apply one shared vertical offset to both sides, preserve all authored left/right facing and X anchors, and leave the Task 10 central 820x820 cinematic anchors unchanged. Lock the offset with a board-widget test and tune the exact small value from PIE evidence during Task 12.

- [ ] **Step 4: Rebuild and verify GREEN**

Run the cold build and both filters. Expected: runtime counts are zero while shared content still resolves.

- [ ] **Step 5: Commit**

```powershell
git add Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp Source/GameXXK/Public/UI/GameXXKBattleAnimationLayerWidget.h Source/GameXXK/Private/UI/GameXXKBattleAnimationLayerWidget.cpp Source/GameXXK/Private/Tests/GameXXKBattleActorHudRetirementTest.cpp Source/GameXXK/Private/Tests/GameXXKPartyDeckBattleSceneUnitActorTest.cpp
git commit -m "refactor: retire runtime battle scene renderers"
```

### Task 13: Validate the full atlas inventory and runtime memory budget

**Files:**
- Modify: `Content/Python/gamexxk_validate_battle_animation_texture_memory.py`
- Modify: `scripts/test_battle_animation_texture_memory_validator.py`
- Read: `SourceAssets/AnimationProduction/production_v1/manifest.json`

- [ ] **Step 1: Write failing complete-inventory tests**

Extend the Python tests to assert:

```python
self.assertEqual(report["manifest_entry_count"], 139)
self.assertEqual(report["valid_atlas_count"], 138)
self.assertEqual(report["idle_count"], 34)
self.assertEqual(report["attack_count"], 33)
self.assertEqual(report["hit_count"], 34)
self.assertEqual(report["death_count"], 34)
self.assertEqual(report["generic_effect_count"], 3)
self.assertEqual(report["runtime_cache"]["sync_load_count"], 0)
self.assertEqual(report["runtime_cache"]["forced_gc_count"], 0)
self.assertLessEqual(report["runtime_cache"]["peak_resident_bytes"], 256 * 1024 * 1024)
```

Assert every validated texture is 4096², 8×8/60-frame metadata, no mips, bilinear, BC7-capable, and has the canonical package path. Assert only `enemy_07_graywolf_attack` is the intentional failed-no-retry entry.

- [ ] **Step 2: Run pure tests and verify RED**

```powershell
python -B -m unittest scripts.test_battle_animation_texture_memory_validator -v
```

Expected: the current six-texture pilot report lacks full counts/runtime cache fields.

- [ ] **Step 3: Add `--all` and runtime-stat ingestion**

Implement CLI arguments:

```python
parser.add_argument("--all", action="store_true")
parser.add_argument("--runtime-stats", type=Path)
parser.add_argument("--output", type=Path, required=True)
```

When `--all` is set, enumerate exactly the valid manifest outputs plus the three generic entries and compare their UE texture settings/resource bytes. Merge the PIE-produced cache JSON and fail if peak resident bytes exceed the cap or either forbidden counter is nonzero.

- [ ] **Step 4: Run pure tests and generate a static report**

```powershell
python -B -m unittest scripts.test_battle_animation_texture_memory_validator -v
python Content\Python\gamexxk_validate_battle_animation_texture_memory.py --all --output Saved\HarnessReports\battle-atlas-static-validation.json
```

Expected: tests pass; static report has 138 valid textures and one intentional fallback entry. Runtime budget fields are marked `awaiting_pie_report` until Task 14 supplies them, not silently treated as passing.

- [ ] **Step 5: Commit**

```powershell
git add Content/Python/gamexxk_validate_battle_animation_texture_memory.py scripts/test_battle_animation_texture_memory_validator.py
git commit -m "test: validate complete battle atlas budget"
```

### Task 14: Rewrite the real PIE flow for the same-map fullscreen HUD

**Files:**
- Modify: `scripts/gamexxk_real_play_flow_mcp.py`
- Modify: `scripts/test_gamexxk_real_play_flow_mcp.py`
- Modify: `scripts/test_gamexxk_real_play_flow_probe.py`
- Modify: `Content/Python/gamexxk_probe_real_play_flow.py`

- [ ] **Step 1: Write failing harness/probe tests**

Replace the `L_BattleTown`, battle-camera, presenter, actor, and PaperFlipbook expectations with this report contract:

```python
required = {
    "map_before", "map_during_battle", "view_target_before", "view_target_during_battle",
    "battle_overlay_visible", "route_widget_identity_preserved", "route_scroll_before",
    "route_scroll_after", "world_paused", "world_rendering_enabled",
    "backdrop_asset", "active_unit_visuals", "presentation_phase", "impact_count",
    "displayed_health_before", "displayed_health_after", "runtime_cache",
    "battle_scene_actor_count", "battle_flipbook_tick_count"
}
self.assertEqual(report["map_during_battle"], report["map_before"])
self.assertEqual(report["view_target_during_battle"], report["view_target_before"])
self.assertFalse(report["world_rendering_enabled"])
self.assertEqual(report["battle_scene_actor_count"], 0)
self.assertEqual(report["battle_flipbook_tick_count"], 0)
```

Probe exact formation anchors, 12 FPS idle frame progress, positive X scale, 820×820 central size, 1.1-second marker, sequential multi-hit, lethal death-before-removal, and exact restoration after victory.

- [ ] **Step 2: Run pure harness tests and verify RED**

```powershell
python -B -m unittest scripts.test_gamexxk_real_play_flow_mcp scripts.test_gamexxk_real_play_flow_probe -v
```

Expected: old report schema and `L_BattleTown` assumptions fail.

- [ ] **Step 3: Implement the new probe schema and MCP orchestration**

Keep the existing main-menu → town → quest/F → route-node flow. At battle-node activation, record map and camera before/after, query the coordinator, board stage, visual registry, timeline phase, displayed health overlay, actor/tick counts, and cache stats. Drive one player attack, one enemy attack, one multi-hit, and battle completion through normal UI/runtime paths. Save reports under `Saved/HarnessReports` and return nonzero on any contract failure.

- [ ] **Step 4: Run pure tests and real PIE acceptance**

```powershell
python -B -m unittest scripts.test_gamexxk_real_play_flow_mcp scripts.test_gamexxk_real_play_flow_probe -v
python scripts\ue_mcp_smoke.py --report Saved\HarnessReports\fullscreen-battle-mcp-smoke.json
python scripts\gamexxk_real_play_flow_mcp.py
```

Expected: pure tests pass; real report proves same map/view target, paused and non-rendering world, full HUD visuals, marker-driven health, zero scene actors/flipbook ticks, exact route restoration, and cache stats suitable for Task 13.

- [ ] **Step 5: Commit**

```powershell
git add scripts/gamexxk_real_play_flow_mcp.py scripts/test_gamexxk_real_play_flow_mcp.py scripts/test_gamexxk_real_play_flow_probe.py Content/Python/gamexxk_probe_real_play_flow.py
git commit -m "test: verify fullscreen hud battle flow in pie"
```

### Task 15: Cold-build, full regression, viewport, and performance acceptance

**Files:**
- Modify: `docs/superpowers/specs/2026-07-26-fullscreen-hud-battle-visual-system-design.md`
- Create: `docs/production/fullscreen-hud-battle-visual-system.md`
- Generate: `Saved/HarnessReports/fullscreen-battle-final.json`
- Generate: `Saved/HarnessReports/battle-atlas-runtime-validation.json`

- [ ] **Step 1: Run all pure tests**

```powershell
python -B -m unittest scripts.test_fullscreen_battle_hud_asset_import scripts.test_battle_animation_texture_memory_validator scripts.test_gamexxk_real_play_flow_mcp scripts.test_gamexxk_real_play_flow_probe -v
python scripts\harness_state_validator.py
```

Expected: all tests pass and production-unit records validate.

- [ ] **Step 2: Run a true cold build**

Close the editor only after UE MCP saves dirty packages, then run the standard cold build. Expected: exit code 0 with no Live Coding/Hot Reload output used as verification.

- [ ] **Step 3: Run focused and full Automation Tests**

Run these filters individually, then the full project filter:

```text
GameXXK.MVP.LevelFlow
GameXXK.MVP.Battle.OverlayCoordinator
GameXXK.MVP.UI.PlayerControllerOwnsFlowWidgets
GameXXK.Integration.CardBattle.ControllerInputBridge
GameXXK.MVP.Battle.AnimationPresentation
GameXXK.UI.Battle.UnitVisualWidget
GameXXK.UI.Battle.AtlasCache
GameXXK.MVP.Battle.BoardWidget
GameXXK.MVP.Battle.AnimationLayerWidget
GameXXK.UI.Battle.FixedSlotUnitHud
GameXXK.Integration.CardBattle.BoardPresentationGate
GameXXK.Integration.CardBattle.BoardEnemyIntentPresentation
GameXXK.UI.Battle.StatusEffectsWidget
GameXXK.Data.CardCombatRules
GameXXK.Data.CardBattleRuntime
GameXXK.UI.Battle.ActorHudRetirement
GameXXK.MVP.Battle.PartyDeckVisualMapping
GameXXK
```

Use the standard `UnrealEditor-Cmd.exe` command for each filter. Expected: zero failed tests and zero incomplete tests.

- [ ] **Step 4: Run real PIE at three viewport shapes**

Run the real flow at 1288×770, 1920×1080, and one non-16:9 viewport supported by the harness. Inspect captured frames for centered backdrop cropping, square unit cells, left/right authored facing, exact 2× central scale, common center impact, no clipping, and restored route scroll. Record the exact viewport dimensions and screenshot paths in `docs/production/fullscreen-hud-battle-visual-system.md`.

- [ ] **Step 5: Run runtime memory validation**

```powershell
python Content\Python\gamexxk_validate_battle_animation_texture_memory.py --all --runtime-stats Saved\HarnessReports\fullscreen-battle-final.json --output Saved\HarnessReports\battle-atlas-runtime-validation.json
```

Expected: peak incremental atlas residency ≤256 MiB, synchronous loads 0, forced GC 0, actor count 0, PaperFlipbook tick count 0, and no new texture-pool over-budget warning.

- [ ] **Step 6: Run the cold PIE smoke**

```powershell
python scripts\ue_tdd_pipeline.py --pie-duration 0.2 --log-lines 600
```

Expected: cold compile/launch/PIE smoke succeeds and the log contains no assertion, ensure, fatal, texture-pool over-budget, synchronous atlas-load, or forced-GC failure.

- [ ] **Step 7: Update acceptance records**

Set the design spec status to `Implemented and verified`. In `docs/production/fullscreen-hud-battle-visual-system.md`, record commit, build command/result, Automation filters/results, report paths, viewport captures, atlas counts, peak bytes, and any intentional gray-wolf fallback evidence. Do not mark a criterion passed without its command/report evidence.

- [ ] **Step 8: Commit final verification records**

```powershell
git add docs/superpowers/specs/2026-07-26-fullscreen-hud-battle-visual-system-design.md docs/production/fullscreen-hud-battle-visual-system.md
git commit -m "docs: certify fullscreen hud battle visual system"
```

## Completion checklist

- Route battle entry and exit keep the same map, camera, route widget, and route scroll.
- World pause/render/input/cursor states restore exactly, including non-default initial values and abnormal exits.
- One 16:9 backdrop fills the viewport behind one 1920×1080 common battle stage.
- One persistent UMG/MID visual per unit serves formation and cinematic presentation without reparenting or mirroring.
- All active idles and actions use canonical atlas texture objects; no runtime PaperFlipbook or scene-actor presentation remains.
- Attack/hit run together at 2×, impact occurs once at 1.1 seconds at screen center, and health/readout/shake update at that marker.
- Multi-hit packets play sequentially; lethal death finishes before removal and terminal transition.
- Missing gray-wolf attack, timeout, cancellation, destruction, and stale callback paths continue with fallback and cannot strand input.
- Generic impact/Buff/Debuff atlases are reused; no additional generation or paid model credits are consumed.
- Full atlas validation reports 138 valid textures, one intentional failed-no-retry action, and a runtime peak at or below 256 MiB.
- Cold build, full Automation, real PIE flow, viewport checks, and performance reports all pass before completion is claimed.
