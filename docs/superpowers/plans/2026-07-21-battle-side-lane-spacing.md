# Battle Side-Lane Spacing Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move the fixed 3v3 battle formation and its matching fixed HUD plates 30 cm outward per side so the two inner plates no longer overlap, without changing slot identity, screen-projection behavior, or combat rules.

**Architecture:** `AGameXXKBattleScenePresenter` remains the sole owner of local world-space lane positions, while `UGameXXKBattleBoardWidget` remains the sole owner of the fixed 1920×1080 UMG safe-stage anchors. The two coordinate systems receive matched outward adjustments only; `BattleVisual` offsets, actor hit areas, per-frame projection, resource-bar art, and Party Qi layout stay untouched.

**Tech Stack:** Unreal Engine 5.8 C++, UMG/Slate, Unreal Automation tests, UE MCP lifecycle scripts, UBT cold build.

---

## Scope lock

- Approved local lanes: enemy `Y = -280, -210, -140`; party `Y = +280, +210, +140` for P1/P2/P3.
- Approved fixed HUD X anchors: enemy `.11, .26, .41`; party `.89, .74, .59` for P1/P2/P3. Keep current Y anchors `.60, .52, .44`, `272×142` logical plate size, and `(0.5, 0.0)` alignment.
- The same slot roles remain fixed: 我 1P = permanent companion, 我 2P = hero, 我 3P = temporary task NPC; 敌 1P/2P/3P remain outer/middle/inner.
- This plan intentionally excludes the HP/MP fill-mask repair and Party-Qi orb resize. Those are separate visual changes and must not be bundled into this layout patch.

## File map

| File | Responsibility in this change |
|---|---|
| `Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp` | Defines the authoritative local-world lane contract for the 3v3 scene presentation. |
| `Source/GameXXK/Private/MVP/GameXXKBattleScenePresenter.cpp` | Supplies the immutable local enemy/party P-slot positions used by spawn and refresh. |
| `Source/GameXXK/Private/Tests/GameXXKBattleProjectedUnitHudTest.cpp` | Defines the fixed-safe-stage anchor contract, the no-projection invariant, and the usable inner-lane gap. |
| `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp` | Resolves each card-runtime unit to its stable fixed HUD anchor. |
| `Content/Python/gamexxk_apply_battle_hud_fixture.py` | Existing non-saving 3v3 fixture used only for post-change visual verification; do not modify it. |
| `scripts/gamexxk_real_play_flow_mcp.py` | Existing read-only battle-HUD observation runner used to capture the live 3v3 result; do not modify it. |

### Task 1: Lock the outward world-lane contract with a red Automation test

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp:214-275`
- Modify: `Source/GameXXK/Private/MVP/GameXXKBattleScenePresenter.cpp:241-247`
- Test: `GameXXK.MVP.Battle.SceneActors`

- [ ] **Step 1: Change the scene-actor assertions to the approved coordinates before changing production code.**

  In the existing party assertion block, replace the three expected vectors and the anchored hero vector with the exact approved values:

  ```cpp
  TestEqual(TEXT("party 2P uses the approved outward middle coordinate"), HeroPlacement->Location, FVector(-20.0f, 210.0f, 90.0f));
  TestEqual(TEXT("party 1P uses the approved outward outer coordinate"), BladePlacement->Location, FVector(-80.0f, 280.0f, 90.0f));
  TestEqual(TEXT("party 3P uses the approved outward inner coordinate"), QuestNpcPlacement->Location, FVector(40.0f, 140.0f, 90.0f));
  ```

  Replace the anchored expectation with:

  ```cpp
  TownBattleAnchor + FVector(-20.0f, 210.0f, 90.0f)
  ```

  Resolve `Enemy.Middle` alongside the existing outer and inner pointers. Assert all three exact enemy lanes:

  ```cpp
  TestEqual(TEXT("enemy 1P uses the approved outward outer coordinate"), EnemyOuterPlacement->Location, FVector(-80.0f, -280.0f, 90.0f));
  TestEqual(TEXT("enemy 2P uses the approved outward middle coordinate"), EnemyMiddlePlacement->Location, FVector(-20.0f, -210.0f, 90.0f));
  TestEqual(TEXT("enemy 3P uses the approved outward inner coordinate"), EnemyInnerPlacement->Location, FVector(40.0f, -140.0f, 90.0f));
  ```

  Retain the existing role/P-slot and monotonic-toward-center assertions; add no gameplay-state changes to this test.

- [ ] **Step 2: Cold-compile and run the focused test to prove the new contract is red.**

  Use the project lifecycle script so any interactive editor is saved through UE MCP before it is closed; never use Live Coding or Hot Reload:

  ```powershell
  python scripts\ue_tdd_pipeline.py --pie-duration 0 --log-lines 120
  ```

  In the fresh MCP editor produced by that script, run the focused Automation test through the existing console bridge:

  ```powershell
  @'
  import sys, time
  from pathlib import Path
  sys.path.insert(0, str(Path('scripts').resolve()))
  from ue_mcp_client import UnrealMCPClient

  client = UnrealMCPClient(timeout=60.0)
  if not client.connect():
      raise SystemExit('UE MCP unavailable')
  client.clear_log_buffer()
  print(client.execute_console_command('Automation RunTests GameXXK.MVP.Battle.SceneActors'))
  time.sleep(5.0)
  print('\n'.join(client.get_recent_log_lines(num_lines=300, pattern='GameXXK.MVP.Battle.SceneActors')))
  '@ | python -
  ```

  Expected: the test compiles but reports coordinate expectation failures, because production still contains the old `±250/±180/±110` Y lanes.

- [ ] **Step 3: Apply the minimal immutable-lane implementation.**

  In `AGameXXKBattleScenePresenter::BuildUnitPlacementsForState`, replace only the two fixed arrays with:

  ```cpp
  const FVector EnemySlotLocations[] = {
      FVector(-80.0f, -280.0f, 90.0f), FVector(-20.0f, -210.0f, 90.0f), FVector(40.0f, -140.0f, 90.0f)};
  const FVector PartySlotLocations[] = {
      FVector(-80.0f, 280.0f, 90.0f), FVector(-20.0f, 210.0f, 90.0f), FVector(40.0f, 140.0f, 90.0f)};
  ```

  Do not alter `BuildUnitPlacementsForStateAtAnchor`, `RefreshBattleScene`, the actor label code, `BattleVisual` relative offsets, camera transform, or hit-area ownership.

- [ ] **Step 4: Rebuild and verify the scene contract is green.**

  Repeat the exact cold-build and focused Automation commands from Step 2.

  Expected: `GameXXK.MVP.Battle.SceneActors` succeeds; its role-derived P-slot identities, scene-anchor addition, and retain/spawn/remove behavior remain green.

### Task 2: Lock matching fixed-HUD anchors and the real inner-lane clearance

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleProjectedUnitHudTest.cpp:22-253`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp:93-131`
- Test: `GameXXK.UI.Battle.FixedSlotUnitHud`

- [ ] **Step 1: Make the fixed-HUD test expect the approved anchors and an actual 1114×626 inner-lane gap.**

  Change every existing expected fixed anchor to:

  ```cpp
  // Party: permanent companion P1, hero P2, task NPC P3.
  FVector2D(0.89f, 0.60f)
  FVector2D(0.74f, 0.52f)
  FVector2D(0.59f, 0.44f)

  // Enemy: outer P1, middle P2, inner P3.
  FVector2D(0.11f, 0.60f)
  FVector2D(0.26f, 0.52f)
  FVector2D(0.41f, 0.44f)
  ```

  Add this helper below `AssertFixedHudSlot` so the test measures the Board's resolved anchors rather than only repeating constants:

  ```cpp
  void AssertApprovedInnerLaneClearance(
      FAutomationTestBase& Test,
      UGameXXKBattleBoardWidget* const Board)
  {
      constexpr float SafeStageWidth = 1920.0f;
      constexpr float CurrentPieWidth = 1114.0f;
      Test.TestNotNull(TEXT("clearance test has a Board"), Board);
      if (!Board)
      {
          return;
      }
      const float HalfPlateWidth = FixedUnitHudSize.X * 0.5f;
      const FVector2D EnemyInnerAnchor = Board->GetProjectedUnitHudAnchorPositionForTest(TEXT("Enemy.Tiger"));
      const FVector2D PartyInnerAnchor = Board->GetProjectedUnitHudAnchorPositionForTest(TEXT("Npc.TusiChief"));
      const FVector2D EnemyOuterAnchor = Board->GetProjectedUnitHudAnchorPositionForTest(TEXT("Enemy.MoneyRat"));
      const FVector2D PartyOuterAnchor = Board->GetProjectedUnitHudAnchorPositionForTest(TEXT("Partner.Blade"));
      const float EnemyInnerRight = EnemyInnerAnchor.X * SafeStageWidth + HalfPlateWidth;
      const float PartyInnerLeft = PartyInnerAnchor.X * SafeStageWidth - HalfPlateWidth;
      const float GapAtCurrentPie = (PartyInnerLeft - EnemyInnerRight) * CurrentPieWidth / SafeStageWidth;
      const float EnemyOuterLeft = EnemyOuterAnchor.X * SafeStageWidth - HalfPlateWidth;
      const float PartyOuterRight = PartyOuterAnchor.X * SafeStageWidth + HalfPlateWidth;

      Test.TestTrue(TEXT("inner enemy and party HUD plates have at least 40 physical pixels of clearance at 1114-wide PIE"), GapAtCurrentPie >= 40.0f);
      Test.TestTrue(TEXT("outer enemy HUD remains inside the 1920-wide safe stage"), EnemyOuterLeft >= 0.0f);
      Test.TestTrue(TEXT("outer party HUD remains inside the 1920-wide safe stage"), PartyOuterRight <= SafeStageWidth);
  }
  ```

  Call `AssertApprovedInnerLaneClearance(*this, Board);` immediately after the six initial `AssertFixedHudSlot` calls. Keep the existing `RegisterBattleUnitHudScreenPosition` / `NativeTick` checks, but update their expected anchors as well; that keeps the no-per-frame-projection rule covered.

- [ ] **Step 2: Run the matching focused Automation test red.**

  Use the Task 1 cold-build command after saving the editor through MCP, then run:

  ```powershell
  @'
  import sys, time
  from pathlib import Path
  sys.path.insert(0, str(Path('scripts').resolve()))
  from ue_mcp_client import UnrealMCPClient

  client = UnrealMCPClient(timeout=60.0)
  if not client.connect():
      raise SystemExit('UE MCP unavailable')
  client.clear_log_buffer()
  print(client.execute_console_command('Automation RunTests GameXXK.UI.Battle.FixedSlotUnitHud'))
  time.sleep(5.0)
  print('\n'.join(client.get_recent_log_lines(num_lines=300, pattern='GameXXK.UI.Battle.FixedSlotUnitHud')))
  '@ | python -
  ```

  Expected: the anchor expectations fail while `TryResolveFixedUnitHudLayout` still supplies `.14/.29/.44` and `.86/.71/.56`; the new clearance assertion also fails against those old inner anchors.

- [ ] **Step 3: Apply only the approved fixed-anchor substitutions.**

  In `TryResolveFixedUnitHudLayout`, replace the six `Anchor = FVector2D(...)` values with:

  ```cpp
  // Party
  case 1: Anchor = FVector2D(0.89f, 0.60f); break;
  case 2: Anchor = FVector2D(0.74f, 0.52f); break;
  case 3: Anchor = FVector2D(0.59f, 0.44f); break;

  // Enemy
  case 1: Anchor = FVector2D(0.11f, 0.60f); break;
  case 2: Anchor = FVector2D(0.26f, 0.52f); break;
  case 3: Anchor = FVector2D(0.41f, 0.44f); break;
  ```

  Leave `BattleHudSafeStageDesignSize`, `FixedUnitHudWidgetSize`, `ResolveBattleHudSafeStageLayout`, `NativeTick`, party-Qi avoidance, and all card/intent/tooltip layers unchanged.

- [ ] **Step 4: Rebuild and verify fixed HUD layout green.**

  Repeat the Task 2 focused command. Expected: all six plates retain their immutable side and P-slot, the old bogus actor-foot registrations do not move them, the wide/tall safe-stage checks still pass, the inner clearance is at least 40 physical pixels at the observed 1114-wide viewport, and both outer plates remain in the safe stage.

### Task 3: Verify the coupled 3v3 composition in a real PIE without mutating saved progress

**Files:**
- Modify: none
- Use: `Content/Python/gamexxk_apply_battle_hud_fixture.py`
- Use: `Content/Python/gamexxk_probe_real_play_flow.py`
- Use: `scripts/gamexxk_real_play_flow_mcp.py`

- [ ] **Step 1: Run the two affected Automation suites together on the cold-built binary.**

  After the Task 2 build starts its fresh MCP editor, issue one combined test queue:

  ```powershell
  @'
  import sys, time
  from pathlib import Path
  sys.path.insert(0, str(Path('scripts').resolve()))
  from ue_mcp_client import UnrealMCPClient

  client = UnrealMCPClient(timeout=60.0)
  if not client.connect():
      raise SystemExit('UE MCP unavailable')
  client.clear_log_buffer()
  print(client.execute_console_command('Automation RunTests GameXXK.MVP.Battle.SceneActors+GameXXK.UI.Battle.FixedSlotUnitHud'))
  time.sleep(8.0)
  print('\n'.join(client.get_recent_log_lines(num_lines=500, pattern='GameXXK.')))
  '@ | python -
  ```

  Expected: both test names finish with Success. A failure in either is a blocker because position and HUD must remain coupled.

- [ ] **Step 2: Enter the existing non-saving 3v3 fixture and collect battle-HUD evidence.**

  Keep the fresh editor open, then run:

  ```powershell
  python scripts\gamexxk_real_play_flow_mcp.py --battle-hud-observation --keep-pie --timeout 180 --report 'Saved\HarnessReports\battle-side-lane-spacing.json'
  ```

  Expected: the report records a live fixture with three enemy and three party HUDs; no `unit_hud_overlaps_unit_hud`, no `unit_hud_overlaps_hand_card_box`, no `unit_hud_overlaps_party_qi`, and no `unit_hud_overlaps_end_turn_button` errors. The fixture is transient and must be cleared by the script’s cleanup path before any save.

- [ ] **Step 3: Perform the visual acceptance check at the current viewport and one tall viewport.**

  In the kept PIE session, inspect the battle after the fixture has populated it. Confirm all items below before calling the layout done:

  - enemy characters form the left-side diagonal and party characters form the mirrored right-side diagonal;
  - 我 1P stays outermost, 我 2P hero stays in the middle, 我 3P task NPC stays nearest center; enemy labels retain the same outer-to-inner ordering;
  - every red/green/status plate appears beneath its matching character without overlap between the two inner plates;
  - neither outer plate touches the side boundary, hand, Party Qi, or end-turn rail;
  - resizing to a tall viewport does not re-anchor a plate or create overlap, because the 16:9 safe stage scales as one composition.

  Do not use per-frame world-to-screen projection, Slate Inspector, or a D3D12-only screenshot path for this check.

- [ ] **Step 4: Preserve shared-worktree safety.**

  Before any optional commit, run:

  ```powershell
  git diff -- Source/GameXXK/Private/MVP/GameXXKBattleScenePresenter.cpp Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp Source/GameXXK/Private/Tests/GameXXKBattleProjectedUnitHudTest.cpp
  ```

  Expected: the diff contains only the twelve approved numerical lane/anchor substitutions and the corresponding test assertions/helper. Do not stage or commit unrelated existing user changes in this shared dirty worktree.

## Plan self-review

- **Spec coverage:** Task 1 covers the approved six world-space lane coordinates and preserves actor role/anchor behavior. Task 2 covers the approved six fixed-safe-stage anchors, the 40-pixel inner clearance target at the actual 1114-wide PIE view, safe-stage bounds, and no-projection behavior. Task 3 verifies the combined live 3v3 result and the required non-overlap boundaries.
- **Scope control:** HP/MP fill texture/material work and Party-Qi asset sizing are excluded explicitly; no material, resource, card, combat, camera, terrain, actor-component, or save-state code is modified.
- **Type consistency:** All formation edits use `FVector` local locations; all UMG edits use `FVector2D` normalized anchors; the test computes `float` clearance using the already-tested `272×142` logical plate. The Automation names match the current macros: `GameXXK.MVP.Battle.SceneActors` and `GameXXK.UI.Battle.FixedSlotUnitHud`.
- **Placeholder scan:** this plan contains no unresolved action placeholders; every code-edit step lists the exact target values and every verification step gives an executable PowerShell command and expected outcome.
