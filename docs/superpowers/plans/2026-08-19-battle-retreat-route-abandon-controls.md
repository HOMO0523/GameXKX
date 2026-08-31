# GameXXK Battle Retreat And Route Abandon Controls Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the two confirmed exit controls without changing route ownership: a BattleBoard close action that atomically restores the exact pre-encounter route state, and a fixed route-map close action that previews and applies the existing abandoned-route settlement before returning to Qingshan Town.

**Architecture:** `FGameXXKRuntimeState` owns one small, saved battle-entry checkpoint captured transactionally by `SelectRouteNodeById` before Battle/Elite/Boss entry. Rules perform both retreat and abandoned settlement on candidate copies; the subsystem exposes thin player-flow wrappers and a side-effect-free settlement preview. BattleBoard and OneGameRouteMap own only transient modal state and fixed top-right presentation, pausing input/auto-play while their modal is open. Map travel occurs only after the authoritative transaction succeeds.

**Tech Stack:** Unreal Engine 5.8 C++, USTRUCT SaveGame state, UMG/Slate programmatic widgets, UE Automation, project UE MCP scripts, PowerShell, cold UBT, and visual evidence review.

---

## Preconditions and protected scope

- The route-owned flow and wall-clock auto-battle cadence are already committed. `TickAutoBattleAtRealTime(FPlatformTime::Seconds())` is a precondition, not work to reimplement here.
- Work directly in the root `main` worktree. Do not create a worktree and do not use UnrealBridge.
- Before and after every implementation checkpoint, preserve `Content/GameXXK/Maps/L_Main.umap` at SHA256 `EE6E8394E40298321F2A57CC030018BDD1109EED36248597A7D7F414E387E46B`.
- Never stage the user-modified `scripts/test_battle_camera_framing.py`, root `Private/` or `Public/`, untracked probes, `SourceAssets/`, or `SourceArt/` review/source trees.
- Do not add art assets. Both toolbars and modals reuse existing battle/route textures, typography, and button styling.

## File map

- `Source/GameXXK/Public/GameXXKMVPRules.h`: define `FGameXXKBattleEntryCheckpoint`, add it to `FGameXXKRuntimeState`, and declare the authoritative retreat transaction.
- `Source/GameXXK/Private/GameXXKMVPRules.cpp`: capture/clear/validate the checkpoint, restore it atomically, and keep terminal/reward paths consistent.
- `Source/GameXXK/Public/MVP/GameXXKSaveMigration.h`: introduce save v23.
- `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp`: migrate legacy mid-battle saves and validate current checkpoint invariants.
- `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`: expose retreat, settlement preview, and abandon wrappers.
- `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`: implement thin wrappers without UI or map-travel side effects.
- `Source/GameXXK/Private/Tests/GameXXKBattleRetreatTest.cpp`: lock Battle/Elite/Boss capture, rollback, pending-reward discard, migration, and invariant behavior.
- `Source/GameXXK/Private/Tests/GameXXKRouteSettlementTest.cpp`: lock read-only preview, exact abandoned conversion, and replay/idempotency through the subsystem facade.
- `Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h`: declare top-right controls, modal state, callbacks, layout/test seams, and modal-aware auto-play gating.
- `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`: move auto battle, add Close and the confirmation overlay, perform retreat only after presentation is idle, and remove auto from the bottom-right Qi avoidance rail.
- `Source/GameXXK/Private/Tests/GameXXKCardBattleBoardWidgetTest.cpp`: lock control geometry, modal input gating, cancel/confirm, pending-reward retreat, and auto pause/resume.
- `Source/GameXXK/Public/UI/GameXXKOneGameRouteMapWidget.h`: declare the fixed close control, settlement preview modal, callbacks, error state, and test seams.
- `Source/GameXXK/Private/UI/GameXXKOneGameRouteMapWidget.cpp`: add the fixed RootOverlay toolbar/modal, block scroll/node input while open, preview on every open, and travel only after successful abandon.
- `Source/GameXXK/Private/Tests/GameXXKOneGameRouteMapAdapterTest.cpp`: lock fixed positioning, preview, input gating, cancel, success, and failure behavior.
- `scripts/gamexxk_real_play_flow_mcp.py`: extend the real player-flow harness with Elite cancel/retreat/re-entry and route settlement exit evidence.
- `docs/production/current-goal-acceptance.md` and `docs/production/2026-08-19-goal-progress-evidence.md`: record the final semantics and evidence without claiming the larger desktop-workbench goal is complete.

### Task 1: Add and migrate the saved battle-entry checkpoint

**Files:**
- Create: `Source/GameXXK/Private/Tests/GameXXKBattleRetreatTest.cpp`
- Modify: `Source/GameXXK/Public/GameXXKMVPRules.h`
- Modify: `Source/GameXXK/Public/MVP/GameXXKSaveMigration.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKSaveGameTest.cpp`

- [x] **Step 1: Write RED checkpoint and migration tests**

Create Automation tests under `GameXXK.Route.BattleRetreat` with deterministic generated-route save fixtures:

- `CheckpointSchemaRoundTrip`: populate every checkpoint field, pass it through `MakeSaveState` and current-version migration/restore, then assert exact equality.
- `LegacySaveMigration`: serialize a v22 active generated-route battle with exactly one visited inbound parent and no checkpoint; migration to v23 must create a valid deterministic checkpoint, retain load-time HP/MP, and preserve current visited/reachable snapshots.
- `LegacySaveMigrationAmbiguousParent`: give the pending battle node two visited inbound parents; migration succeeds but leaves `bValid == false` and emits a warning, so the UI can disable only battle retreat.
- `CurrentCheckpointValidation`: v23 state is rejected when a valid checkpoint has no generated route, invalid source node, mismatched active/pending battle, duplicate visited/reachable IDs, or a previous-current node not represented by the route graph.

Use an exact parent rule for legacy saves: collect route edges whose `ToNodeId == PendingRouteNodeId` and whose `FromNodeId` is visited; create a checkpoint only when that set contains exactly one unique node.

- [x] **Step 2: Cold-build and verify RED**

Close Unreal Editor only after saving dirty packages through UE MCP, then run:

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -WaitMutex -NoHotReload -NoHotReloadFromIDE -NoUBA -MaxParallelActions=2
```

Expected: compile fails because `FGameXXKBattleEntryCheckpoint`, v23, and migration support do not exist.

- [x] **Step 3: Implement the minimal saved schema**

Add this USTRUCT before `FGameXXKRuntimeState`:

```cpp
USTRUCT(BlueprintType)
struct FGameXXKBattleEntryCheckpoint
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	bool bValid = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	int32 SourceNodeId = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	int32 PreviousCurrentRouteNodeId = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	int32 PreviousDungeonNodeIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	int32 PreviousPlayerHP = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	int32 PreviousPlayerMP = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	TArray<int32> PreviousVisitedRouteNodeIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	TArray<int32> PreviousReachableRouteNodeIds;
};
```

Add `BattleEntryCheckpoint` to runtime state with `SaveGame`. `SourceNodeId` always means the selected encounter node, while `PreviousCurrentRouteNodeId` is the node shown before that click.

Update migration constants exactly:

```cpp
static constexpr int32 BattleRetreatCheckpointIntroducedSaveVersion = 23;
static constexpr int32 CurrentSaveVersion = 23;
```

For pre-v23 saves, populate the checkpoint only for an active generated-route Battle screen with a valid pending Battle/Elite/Boss node and one unique visited inbound parent. Otherwise reset it and append one specific warning for ambiguous/unrecoverable active battles. New games and non-battle legacy saves get the default invalid checkpoint.

- [x] **Step 4: Add compatibility-aware validation**

In `ValidateRuntimeState`, require a valid checkpoint to satisfy all of these:

- generated route and active dungeon are true;
- screen is `Battle` and an active CardBattle exists;
- source equals `PendingRouteNodeId` and resolves to Battle/Elite/Boss;
- previous current either resolves to an existing node or is `INDEX_NONE` only for a valid route-start edge case;
- saved HP/MP are non-negative and do not exceed the runtime's valid player maxima;
- visited and reachable checkpoint arrays contain unique, existing node IDs;
- the source is absent from previous visited and present in previous reachable.

An invalid/default checkpoint remains valid state, including the explicitly warned ambiguous v22 migration case.

- [x] **Step 5: Cold-build and verify GREEN**

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -WaitMutex -NoHotReload -NoHotReloadFromIDE -NoUBA -MaxParallelActions=2
& scripts/run_mvp_test_suites.ps1 -Suites @(
  'GameXXK.Route.BattleRetreat.Checkpoint',
  'GameXXK.Route.BattleRetreat.LegacySaveMigration',
  'GameXXK.MVP.SaveGame'
) -TimeoutSeconds 600
```

Expected: UBT succeeds; all new checkpoint/migration tests and the full SaveGame suite pass.

- [x] **Step 6: Commit the schema checkpoint**

Stage only the files in this task and commit:

```powershell
git commit -m "feat: save route battle entry checkpoints"
```

### Task 2: Capture, clear, and atomically restore the encounter checkpoint

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleRetreatTest.cpp`
- Modify: `Source/GameXXK/Public/GameXXKMVPRules.h`
- Modify: `Source/GameXXK/Private/GameXXKMVPRules.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`

- [x] **Step 1: Write RED retreat transaction tests**

Add:

- `CheckpointCaptureKinds`: after selecting Battle, Elite, and Boss separately, assert `bValid`, `SourceNodeId == clicked node`, previous current/index/HP/MP, and exact previous visited/reachable arrays. A Camp or Event selection must leave the checkpoint invalid.
- `CheckpointCaptureFailureAtomicity`: force battle construction failure and compare the whole runtime state to its snapshot; no checkpoint or current/pending-node mutation may leak.
- `NormalEliteBossRollback`: for each encounter kind, snapshot previous current/index/HP/MP/visited/reachable, enter battle, mutate HP/MP and combat state, retreat, then compare every checkpoint-owned field exactly. Assert `Screen == DungeonMap`, `CurrentMapId == "HuangshanRoute"`, `PendingRouteNodeId == INDEX_NONE`, no active CardBattle, no legacy battle units/intents, no pending reward, invalid checkpoint, and the abandoned encounter remains reachable and unvisited.
- `PendingVictoryRewardRollback`: enter a generated Elite encounter, force Victory, call `ResolveBattleVictory(false)` once to create the saved three-choice reward, then retreat. Assert the offer and unresolved battle disappear and no reward card/relic/currency/node completion survives.
- `PreservesCompletedRouteEconomy`: seed travel money, acquired route-card count, relics, inventory, and an already completed node before the target encounter; mutate only combat-local fields and retreat. Assert all pre-entry economy and prior progress are byte-for-byte unchanged.
- `RejectsInvalidCheckpointAtomically`: missing/mismatched checkpoint returns false and whole-state comparison remains equal.
- `ClearsCheckpointOnCommittedRewardAndTerminal`: selecting/skipping a reward, Cleared, Defeated, and Abandoned each leave the checkpoint invalid.

- [x] **Step 2: Verify RED**

Cold-build. Expected: compile fails because `RetreatCurrentBattleToRoute` and subsystem facade do not exist; once declarations are present but capture is absent, behavior tests fail.

- [x] **Step 3: Capture before mutation and centralize clearing**

In the Battle/Elite/Boss branch of `SelectRouteNodeById`, fill `Candidate.BattleEntryCheckpoint` from untouched `State` before setting `CurrentRouteNodeId` or `PendingRouteNodeId`. Run `BeginBattle` on the candidate and assign to `State` only after it succeeds.

Add a helper that resets `BattleEntryCheckpoint` and call it from:

- successful retreat;
- generated-route reward/node finalization after the reward is selected or skipped;
- `ClearActiveBattle` only on paths that truly commit/terminate the encounter;
- route initialization/new game;
- terminal Cleared, Defeated, and Abandoned settlement.

Do not clear the checkpoint when Victory merely opens `PendingReward`; the player can still abandon that encounter.

- [x] **Step 4: Implement the authoritative retreat transaction**

Declare and implement:

```cpp
UFUNCTION(BlueprintCallable, Category = "GameXXK|Route")
static bool RetreatCurrentBattleToRoute(UPARAM(ref) FGameXXKRuntimeState& State);
```

The implementation must:

1. reject anything except an active generated-route Battle/Elite/Boss matching a valid checkpoint;
2. work on `FGameXXKRuntimeState Candidate = State`;
3. restore previous current/index/HP/MP/visited/reachable arrays;
4. set `PendingRouteNodeId = INDEX_NONE`, `Screen = DungeonMap`, and the generated route map ID;
5. clear CardBattle, legacy battle party/enemies, enemy intent/presentation projections, pending reward, target/choice residue, and checkpoint;
6. validate the candidate, then move it into `State` once.

Add a subsystem wrapper `RetreatCurrentBattleToRoute()` that only delegates to rules. It must not travel maps, close widgets, toggle auto battle, or save implicitly.

- [x] **Step 5: Verify focused and route/card regressions**

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -WaitMutex -NoHotReload -NoHotReloadFromIDE -NoUBA -MaxParallelActions=2
& scripts/run_mvp_test_suites.ps1 -Suites @(
  'GameXXK.Route.BattleRetreat',
  'GameXXK.Integration.CardRoute',
  'GameXXK.Integration.CardBattle',
  'GameXXK.Route.Settlement'
) -TimeoutSeconds 900
```

Expected: all selected tests pass and the existing Elite auto-battle test still reaches a terminal phase.

- [x] **Step 6: Commit the rules transaction**

```powershell
git commit -m "feat: restore pre-encounter route state"
```

### Task 3: Expose exact abandoned-route preview and subsystem settlement

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKRouteSettlementTest.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`

- [x] **Step 1: Write RED preview/facade tests**

Add `GameXXK.Route.Settlement.AbandonSubsystemFacade` covering:

- preview of 99 route travel money returns permanent gold `4`;
- preview of 29 actual route-card acquisitions returns enhancement stones `2`;
- preview does not mutate runtime state, receipt ID, inventory, or permanent gold;
- confirming through `AbandonDungeonToTown()` applies those exact amounts, sets `Screen == Town`, clears active route/battle/checkpoint, and records the receipt ID;
- repeating confirmation cannot duplicate rewards;
- invalid/inactive route preview and apply return false without mutation.

- [x] **Step 2: Verify RED**

Expected: compile fails because the subsystem preview and abandon wrappers do not exist.

- [x] **Step 3: Implement thin, side-effect-free facade methods**

Expose:

```cpp
bool PreviewAbandonedRouteSettlement(FGameXXKRouteSettlementReceipt& OutReceipt, FString* OutError = nullptr) const;
bool AbandonDungeonToTown();
```

Preview calls `FGameXXKRouteSettlementRules::Preview(RuntimeState, Abandoned, ...)` only. Apply delegates to `UGameXXKMVPRules::AbandonDungeonToTown(RuntimeState)`. Neither method performs map travel or writes UI state.

- [x] **Step 4: Verify GREEN and commit**

```powershell
& scripts/run_mvp_test_suites.ps1 -Suites @('GameXXK.Route.Settlement') -TimeoutSeconds 360
git commit -m "feat: expose abandoned route settlement"
```

Expected: the full route-settlement suite passes with unchanged `/20`, `/10`, and idempotency semantics.

### Task 4: Move auto battle top-right and add BattleBoard retreat confirmation

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBattleBoardWidgetTest.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`

- [x] **Step 1: Write RED UI geometry and modal tests**

Add focused tests under `GameXXK.Integration.CardBattle.BoardRetreat`:

- `TopRightToolbarGeometry`: initialize the Board at 1280x720, 1672x941, and 1920x1080. Assert `BattleAutoPlayButton` and `BattleCloseButton` exist in one top-right toolbar, Close is to the right of Auto, both remain inside the 16:9 safe stage, and neither overlaps enemy intent/title safe bounds. Assert End Turn remains bottom-right and Party Qi does not overlap the hand or End Turn.
- `ModalCancelNoMutation`: click Close, verify title/body/buttons, modal visibility, and full runtime snapshot equality; click Continue/Escape and verify state remains equal and the session auto toggle retains its previous value.
- `ModalPausesInputsAndAuto`: with auto enabled, open the modal and attempt a card click, target confirm, pending-choice submit, End Turn, auto toggle, Close, and `AdvanceAutoBattleAtRealTimeForTest`; every attempt must be rejected/no-op. After cancel, a later wall-clock step may advance normally.
- `ModalWaitsForPresentation`: queue a committed card presentation, open Close, assert the presentation drains normally but retreat is not applied until the player confirms while mutation gates are idle.
- `ModalConfirmRollback`: confirm from normal/Elite/Boss and pending-Victory-reward fixtures. Assert the rules transaction result and route screen; no reward or node completion remains.
- `ModalInvalidCheckpoint`: a legacy ambiguous fixture displays the error and disables `退出战斗` while `继续战斗` remains usable.

- [x] **Step 2: Cold-build and verify RED**

Expected: tests fail because Close, modal seams, and top-right toolbar do not exist and auto still occupies the bottom-right rail.

- [x] **Step 3: Build the fixed top-right toolbar**

Replace the current bottom-right auto slot with a fixed safe-stage top-right `UHorizontalBox` or equivalent canvas-hosted toolbar named `BattleTopRightToolbar`:

- `BattleAutoPlayButton` on the left;
- `BattleCloseButton` on the right with label `关闭`;
- existing `StyleBattleActionButton` and existing fonts/colors;
- constant gap and safe-stage inset, resolved through one layout helper used by production and tests.

Keep `BattleEndTurnButton` and Party Qi bottom-right. Remove `AutoBattleRect` from `FGameXXKBattlePartyQiLayout` and all bottom-right overlap logic that treats auto as part of the action rail.

- [x] **Step 4: Build transient modal state and callbacks**

Add a Board-owned overlay containing backdrop, existing paper panel, title `退出当前战斗？`, body `将返回进入本场前的路线节点。本场进度与未领取奖励不会保留。`, primary `退出战斗`, and secondary `继续战斗`.

Modal behavior:

- opening never changes the session auto flag or runtime state;
- all Board mutation entry points and `CanAdvanceAutoBattle()` reject while open;
- current presentation queues continue draining;
- confirm stays disabled until the checkpoint is valid and Board presentation/commit mutation is idle;
- successful confirm calls `Subsystem->RetreatCurrentBattleToRoute()`, closes the modal, calls `GameXXKLevelFlow::OpenMapForRuntimeState(Subsystem)`, then `NotifyPlayerFlowStateChanged()`;
- failed confirm keeps battle/modal open and shows the returned/derived reason;
- cancel/Escape only closes the modal.

The auto player must never invoke either confirmation callback.

- [x] **Step 5: Verify focused and complete BattleBoard suites**

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -WaitMutex -NoHotReload -NoHotReloadFromIDE -NoUBA -MaxParallelActions=2
& scripts/run_mvp_test_suites.ps1 -Suites @(
  'GameXXK.Integration.CardBattle.BoardRetreat',
  'GameXXK.Integration.CardBattle.BoardAutoPlay',
  'GameXXK.Integration.CardBattle.BoardPartyQiResponsive',
  'GameXXK.Integration.CardBattle'
) -TimeoutSeconds 900
```

Expected: all tests pass, including `BoardAutoPlayEliteEncounter` and `BoardAutoPlayRealTimeCadence`.

- [x] **Step 6: Commit BattleBoard controls**

```powershell
git commit -m "feat: add battle retreat confirmation"
```

### Task 5: Add the fixed route-map close and settlement confirmation

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKOneGameRouteMapAdapterTest.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKOneGameRouteMapWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKOneGameRouteMapWidget.cpp`

- [x] **Step 1: Write RED route-map UI tests**

Add tests under `GameXXK.MVP.RouteMap.AbandonConfirmation`:

- `FixedTopRightGeometry`: assert `RouteCloseChallengeButton` is a direct/fixed `RootOverlay` child above `RouteScrollBox`, remains at the same viewport position after scroll/drag, stays inside 1280x720, 1672x941, and 1920x1080, and does not overlap the scrollbar or fixed summary.
- `PreviewAndCancel`: seed 99 travel money and 29 acquisitions, open modal, assert `永久金币 +4 / 强化石 +2`, compare runtime snapshot before/after opening and cancel, and keep the route visible.
- `BlocksRouteInput`: while open, node click, drag, wheel/scroll, and another Close are no-ops; modal buttons remain interactive.
- `ConfirmAppliesOnce`: confirm, assert exact rewards once, terminal `Abandoned` receipt, active route cleared, and Town map selected only after success.
- `PreviewOrApplyFailure`: an invalid route disables confirm or retains the modal with an error; runtime/map remain unchanged.

- [x] **Step 2: Cold-build and verify RED**

Expected: tests fail because the fixed button/modal/test seams do not exist.

- [x] **Step 3: Add the fixed RootOverlay toolbar and modal**

Add `RouteCloseChallengeButton` to `RootOverlay`, not the scroll canvas. Anchor it top-right above the scroll layer. Reuse existing route paper/ink styles and label it `关闭挑战`.

Add a higher-z-order RootOverlay modal with:

- title `结束本次挑战？`;
- body `将按已完成的路线进度结算，未完成节点不计入。`;
- fresh preview text `永久金币 +X / 强化石 +Y` every time it opens;
- primary `结算并退出` and secondary `继续挑战`.

When open, block generated-node execution, drag surface, wheel/scroll mutation, and repeated open. Do not hide or move the route behind it.

- [x] **Step 4: Wire authoritative confirm and failure handling**

Open calls `PreviewAbandonedRouteSettlement` without mutation. Confirm calls `AbandonDungeonToTown`; only on success call `GameXXKLevelFlow::OpenMapForRuntimeState`, close modal, and notify flow refresh. On preview/apply failure, keep the modal and route state, disable confirmation where appropriate, and show a concrete error.

- [x] **Step 5: Verify route UI and settlement regressions**

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -WaitMutex -NoHotReload -NoHotReloadFromIDE -NoUBA -MaxParallelActions=2
& scripts/run_mvp_test_suites.ps1 -Suites @(
  'GameXXK.MVP.RouteMap.AbandonConfirmation',
  'GameXXK.MVP.RouteMap',
  'GameXXK.Route.Settlement'
) -TimeoutSeconds 720
```

Expected: all selected tests pass; scroll behavior is unchanged outside the modal.

- [x] **Step 6: Commit route close controls**

```powershell
git commit -m "feat: settle route from map close control"
```

### Task 6: Full verification, real PIE/MCP, visual audit, and rolling evidence

**Files:**
- Modify: `scripts/gamexxk_real_play_flow_mcp.py`
- Modify: `docs/production/current-goal-acceptance.md`
- Modify: `docs/production/2026-08-19-goal-progress-evidence.md`
- Evidence: `Saved/HarnessReports/*`
- Evidence: `Saved/VisualReview/20260819-battle-retreat-route-abandon/*`

- [ ] **Step 1: Extend the real-flow harness before implementation claims**

Make the harness perform and report these named checkpoints:

1. enter route map through the accepted town-exit flow;
2. click an Elite node as the player;
3. enable auto battle and observe at least two authoritative actions under a deliberately throttled/background interval;
4. open Battle Close and cancel; compare route/combat state;
5. reopen and confirm after presentation is idle; assert previous current/visited/reachable/HP/MP and Elite retryability;
6. re-enter and complete one encounter manually/through auto, then resolve its reward as the player;
7. open route Close, capture exact preview, cancel once, reopen and confirm;
8. assert Town return and exactly-once permanent gold/stone delta.

The harness may drive player clicks; production auto-play must still never select nodes, rewards, or confirmation buttons.

- [ ] **Step 2: Run cold UBT and broad automated gates**

With the editor saved and closed:

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -WaitMutex -NoHotReload -NoHotReloadFromIDE -NoUBA -MaxParallelActions=2
python scripts/ai_production_loop.py --run-script-tests --script-tests all --json
& scripts/run_mvp_test_suites.ps1 -Suites @(
  'GameXXK.Route.BattleRetreat',
  'GameXXK.Route.Settlement',
  'GameXXK.MVP.SaveGame',
  'GameXXK.MVP.RouteMap',
  'GameXXK.Integration.CardRoute',
  'GameXXK.Integration.CardBattle',
  'GameXXK.DesktopTraining.Workbench',
  'GameXXK.Training'
) -TimeoutSeconds 1500
python scripts/harness_state_validator.py --json
git diff --check
```

Expected: cold UBT succeeds; script tests/harness validator pass; all selected new and regression suites pass. Keep the known unrelated `GameXXK.MVP.UI.MainMenuPlayerFlow.SaveMigration` baseline separate if it reproduces; do not count it as a new pass or hide it.

- [ ] **Step 3: Run real PIE/MCP acceptance**

Use only project MCP automation:

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 5
python scripts/gamexxk_real_play_flow_mcp.py --timeout 600 --report Saved/HarnessReports/battle-retreat-route-abandon-real-flow.json
```

If UE MCP is unavailable while the editor may have dirty packages, stop and report the real blocker rather than force-closing it.

- [ ] **Step 4: Capture and review multi-resolution visual evidence**

Capture Battle and RouteMap states at 1280x720, 1672x941, and 1920x1080, including both confirmation modals. Store them under `Saved/VisualReview/20260819-battle-retreat-route-abandon/` and review them with a method suitable for the available evidence.

Acceptance:

- Battle top-right toolbar is legible, inside the safe stage, and clear of enemy intent/title content;
- End Turn and Party Qi remain bottom-right with no auto-button reservation or overlap;
- route Close is fixed while the map scrolls;
- both modal panels are centered, undistorted, and block the underlying hit targets;
- no aspect-ratio stretching, repeated workbench battle shell, or new/generated art appears.

- [ ] **Step 5: Record evidence and protect user assets**

Update the rolling pointer and evidence log with exact report paths/counts and the known unrelated baseline. Then run:

```powershell
(Get-FileHash -Algorithm SHA256 'Content/GameXXK/Maps/L_Main.umap').Hash
git status --short
git diff --check
```

Expected protected hash:

```text
EE6E8394E40298321F2A57CC030018BDD1109EED36248597A7D7F414E387E46B
```

Stage only the harness/docs changes from this task and commit:

```powershell
git commit -m "test: verify two-level route exit flow"
```

## Completion boundary

This feature is complete only when all conditions hold together:

- Battle, Elite, and Boss entry capture a saved exact pre-encounter checkpoint.
- Battle Close cancel is a no-op; confirm restores previous node/index/HP/MP/visited/reachable, discards active combat and any unclaimed reward, and keeps the encounter retryable.
- Committed reward and every terminal route outcome clear the checkpoint.
- Legacy v22 active battles migrate deterministically when possible and safely disable only retreat when ambiguous.
- Battle top-right contains `自动战斗` then `关闭`; End Turn stays bottom-right; modal pauses all mutation without changing the auto session flag.
- RouteMap top-right contains fixed `关闭挑战`; its modal previews the exact existing `/20` and `/10` abandoned settlement and applies it exactly once.
- Neither auto-play nor either UI transaction selects routes, events, shops, rewards, retries, or confirmations on the player's behalf.
- Cold UBT, focused/broad Automation, SaveGame migration, real PIE/MCP, three-resolution screenshots, and the listed visual checks pass.
- Protected maps, tuned art, animations, camera transforms, HD2D planes, and user files remain untouched.
