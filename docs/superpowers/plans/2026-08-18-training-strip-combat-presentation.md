# GameXXK Training Strip Combat Presentation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the desktop Training strip continuously animated and readable by separating the one-second authoritative Travel simulation from a real-time Walk/Idle/Attack/Hit/Death presentation, rendering the current authored enemy with health, and removing diagnostic prose from the player-facing strip.

**Architecture:** `FGameXXKTrainingTravelVisualRuntime` becomes a pure presentation state machine. The widget captures immutable before/after Travel snapshots around each authoritative step, queues a bounded combat presentation, and advances it every Slate tick; authoritative rewards, saves, retry semantics, and encounter order remain unchanged. The workbench reuses `FGameXXKBattleAnimationPresentation` descriptors and `FGameXXKBattleAtlasCache` for asynchronous action-atlas loading, while the approved walk atlas and seamless background remain the walking presentation.

**Tech Stack:** Unreal Engine 5.8 C++, UMG/Slate, UE Automation Tests, project UE MCP scripts, cold UBT (`-NoHotReload`), real PIE sampling, visual evidence review.

---

## Scope and protected state

- Work in the current root checkout; do not switch branches or create a worktree while the user worktree is dirty.
- Never stage, edit, save, revert, or replace `Content/GameXXK/Maps/L_Main.umap`. Its preflight SHA256 is `EE6E8394E40298321F2A57CC030018BDD1109EED36248597A7D7F414E387E46B`.
- Do not import concept-image pixels or generate replacement battle sprites. Reuse the approved Training background/walk atlas and the existing battle animation atlases.
- Do not change reward probabilities, cooldowns, save migration, default entry routing, challenge state, or the 3D-town fallback.
- This is the first presentation vertical slice over the current single-target Travel runner. The runtime API must expose current-unit identity and actions in a way that can be expanded to the already-frozen three-enemy/three-party formation without replacing this state machine.
- Do not claim the frozen 3×3 encounter-formation requirement complete from this slice; that requires its own shared challenge/Travel formation change and regression set.

## File structure

- Modify `Source/GameXXK/Public/UI/GameXXKTrainingTravelVisualRuntime.h`: presentation phases, immutable step observation API, action/health getters, smoothing state.
- Modify `Source/GameXXK/Private/UI/GameXXKTrainingTravelVisualRuntime.cpp`: frame-rate-independent phase transitions, queueing, scroll easing, and no-reset looping.
- Modify `Source/GameXXK/Private/Tests/GameXXKTrainingTravelVisualRuntimeTest.cpp`: RED/GREEN contracts for idle entry, lethal and non-lethal sequences, death, smooth scroll, and seamless stage looping.
- Modify `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h`: combat image/health widgets, async atlas cache/session, narrow test accessors, and teardown.
- Modify `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`: capture before/after snapshots, build a compact combat strip, request existing atlases asynchronously, update only on changed frame/resource/value, and remove verbose labels.
- Modify `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`: structural/player-facing contracts for a real enemy image, two health bars, three background tiles, and absence of the old diagnostic labels.
- Modify `Content/Python/gamexxk_probe_training_visual_mvp.py`: expose visual phase, enemy visibility, action, frame, HP, offset, and delta in the PIE evidence payload.
- Modify `docs/production/2026-08-18-desktop-training-2d-hud-migration.md`: dated RED/GREEN, UBT, Automation, PIE, screenshot, and visual evidence.

### Task 1: Lock the pure presentation behavior with failing tests

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKTrainingTravelVisualRuntimeTest.cpp`

- [ ] **Step 1: Replace the old freeze/reset expectations with explicit state transitions**

Create snapshots with `StageId=Training.Normal.1`, `EnemyDefinitionId=Enemy.Ch1.Rooster`, `PlayerHP=100`, `PlayerMaxHP=100`, `EnemyHP=1`, and `EnemyMaxHP=1`. Assert:

```cpp
Runtime.Synchronize(Walking);
Runtime.Tick(0.5f);
TestEqual(TEXT("walking uses the walk presentation"), Runtime.GetVisualPhase(), EGameXXKTrainingTravelVisualPhase::Walking);

Runtime.NotifyTravelStep(Walking, Combat, false, false, false);
TestEqual(TEXT("encounter entry switches the standing hero to idle"), Runtime.GetHeroAction(), EGameXXKBattleAnimationAction::Idle);
TestTrue(TEXT("encounter entry exposes the authored enemy"), Runtime.IsEnemyVisible());
TestEqual(TEXT("encounter entry keeps the authored identity"), Runtime.GetEnemyDefinitionId(), FName(TEXT("Enemy.Ch1.Rooster")));

Runtime.NotifyTravelStep(Combat, NextWalking, true, false, false);
TestEqual(TEXT("settlement starts with the player attack"), Runtime.GetVisualPhase(), EGameXXKTrainingTravelVisualPhase::HeroAttack);
Runtime.Tick(FGameXXKTrainingTravelVisualRuntime::HeroAttackSeconds);
TestEqual(TEXT("attack is followed by enemy hit"), Runtime.GetVisualPhase(), EGameXXKTrainingTravelVisualPhase::EnemyHit);
Runtime.Tick(FGameXXKTrainingTravelVisualRuntime::EnemyHitSeconds);
TestEqual(TEXT("lethal hit is followed by enemy death"), Runtime.GetVisualPhase(), EGameXXKTrainingTravelVisualPhase::EnemyDeath);
Runtime.Tick(FGameXXKTrainingTravelVisualRuntime::EnemyDeathSeconds);
TestEqual(TEXT("death completion resumes walking"), Runtime.GetVisualPhase(), EGameXXKTrainingTravelVisualPhase::Walking);
```

- [ ] **Step 2: Add smooth-scroll and seamless-loop assertions**

Assert that entering combat retains a positive decaying scroll velocity on the first short tick, reaches zero without moving backwards, and stage completion increments `CompletedLoopCount` without resetting either `ScrollOffset` or `WalkFrameIndex`.

- [ ] **Step 3: Add a non-lethal exchange assertion**

Use a Combat before/after pair where enemy HP changes `10→6` and player HP changes `100→98`. Assert the visual sequence is `HeroAttack → EnemyHit → EnemyAttack → HeroHit → EncounterIdle`, and that the health fractions interpolate toward `0.6` and `0.98` rather than jumping to the next encounter snapshot.

- [ ] **Step 4: Run the focused test and verify RED**

Run:

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests "GameXXK.DesktopTraining.TravelVisualRuntime" --automation-report "20260818-training-strip-presentation-red" --json
```

Expected: cold UBT fails on the new missing presentation API, or the focused tests fail on the old Combat freeze and stage-reset behavior. A stale report is not RED evidence.

### Task 2: Implement the real-time presentation state machine

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKTrainingTravelVisualRuntime.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKTrainingTravelVisualRuntime.cpp`

- [ ] **Step 1: Add the visual phases and immutable observation API**

Define:

```cpp
enum class EGameXXKTrainingTravelVisualPhase : uint8
{
    Walking,
    EncounterIdle,
    HeroAttack,
    EnemyHit,
    EnemyAttack,
    HeroHit,
    EnemyDeath,
    HeroDeath,
    Paused
};

void Synchronize(const FGameXXKTrainingTravelRuntime& Runtime);
void NotifyTravelStep(
    const FGameXXKTrainingTravelRuntime& Before,
    const FGameXXKTrainingTravelRuntime& After,
    bool bEncounterCompleted,
    bool bStageCompleted,
    bool bDefeated);
void Tick(float DeltaSeconds);
```

Keep `HeroAttackSeconds=0.28f`, `EnemyHitSeconds=0.14f`, `EnemyAttackSeconds=0.28f`, `HeroHitSeconds=0.14f`, `EnemyDeathSeconds=0.45f`, and `HeroDeathSeconds=0.55f`. Expose getters for phase, elapsed time, hero/enemy action, current enemy ID, visibility, HP fractions, scroll velocity, and loop count.

- [ ] **Step 2: Queue combat snapshots before the gameplay runner resets**

When `Before.Phase==Combat`, retain `Before.EnemyDefinitionId`, old HP, and post-step HP. A lethal step uses enemy HP `0` even if `After` already describes the next encounter. Start the presentation immediately if no event is active; otherwise append to a bounded FIFO. Never derive a death animation from the already-reset `After.EnemyDefinitionId`.

- [ ] **Step 3: Advance phases with remainder-preserving real time**

Consume `DeltaSeconds` in a loop so a large frame can cross multiple short phases without losing time. Non-lethal exchanges return to `EncounterIdle`; lethal exchanges return to the latest synchronized authoritative phase after death. Walking advances the 12 fps walk atlas. Other phases map to the existing battle actions.

- [ ] **Step 4: Ease lane speed and remove the loop reset**

Use a frame-rate-independent exponential response toward `LaneScrollSpeed` while walking and zero otherwise:

```cpp
const float Blend = 1.0f - FMath::Exp(-ScrollResponse * SafeDeltaSeconds);
CurrentScrollSpeed = FMath::Lerp(CurrentScrollSpeed, TargetScrollSpeed, Blend);
ScrollOffset = FMath::Fmod(ScrollOffset + CurrentScrollSpeed * SafeDeltaSeconds, LaneTileWidth);
```

`bStageCompleted` only increments `CompletedLoopCount`; it must not zero `ScrollOffset`, `WalkFrameAccumulator`, or `WalkFrameIndex`.

- [ ] **Step 5: Run focused GREEN verification**

Run the same focused command with report `20260818-training-strip-presentation-green`. Expected: cold UBT exit `0`; every `GameXXK.DesktopTraining.TravelVisualRuntime` test passes with zero errors.

### Task 3: Lock and build the compact real-unit strip

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`

- [ ] **Step 1: Write the failing UMG contract**

After `TakeWidget()`, assert named widgets `TravelEnemyAnimatedUnit`, `TravelHeroAnimatedUnit`, `TravelEnemyHealth`, and `TravelHeroHealth` exist; assert `TravelBackgroundTile_0..2` exist; assert the removed names `TravelVisualStatusText`, `TravelEnemyDebug_0`, `TravelPartyDebug_0`, `TravelPendingRewardSentence`, and `TravelCooldownSentence` do not exist. Advance two one-second steps and assert the enemy unit becomes visible in `EncounterIdle`; advance the combat step and assert the visual runtime begins `HeroAttack` while retaining the defeated enemy ID.

- [ ] **Step 2: Run `GameXXK.DesktopTraining.Workbench.TravelCombatPresentation` and verify RED**

Use `scripts/ai_production_loop.py` with cold UBT and a dedicated `20260818-training-strip-widget-red` Automation report. Expected: the named real-unit widgets and new accessors are missing.

- [ ] **Step 3: Capture immutable snapshots at the mutation boundary**

In `AdvanceTravelForTest`, copy the runner before `AdvanceTrainingTravelStep`, copy it immediately after the call and before failure resolution, then call `TravelVisualRuntime.NotifyTravelStep(Before, After, ...)`. Resolve retry/backtrack afterward and call `Synchronize` with the new authoritative runner. Remove per-encounter `RefreshLayout()` and diagnostic `SetNotice()` calls; settlement data remains accessible through the compact reward controls and tooltips.

- [ ] **Step 4: Build the compact strip without debug prose**

Create three 600 px background tiles named `TravelBackgroundTile_0..2`. Create one enemy image and one hero image at equal, non-stretched sizes, plus real `UProgressBar` health bars. Keep only a compact stage badge, approved collect button, approved failure-retry button, and tooltip-only cooldown/reward explanations. Walking hides the enemy and uses the approved walk atlas; encounter phases show both units with the actions reported by the presentation runtime.

- [ ] **Step 5: Reuse the battle atlas pipeline asynchronously**

Own one `FGameXXKBattleAtlasCache` session in the workbench. Resolve descriptors through `FGameXXKBattleAnimationPresentation::ResolveClipForDefinition`, prefetch hero Idle/Attack/Hit/Death and current-enemy Idle/Attack/Hit/Death while walking, and keep loaded textures in the cache. `NativeDestruct` cancels the session and releases the cache. Do not add another `LoadObject` path for battle atlases.

- [ ] **Step 6: Avoid per-frame invalidation**

Track the last applied action, texture path, frame index, and health fractions. Call `SetBrush`, `SetText`, or `SetPercent` only when the corresponding value changes. Cooldown text is tooltip-only and refreshes on logical second changes, not every Slate frame.

- [ ] **Step 7: Run focused GREEN verification**

Run cold UBT plus:

```text
GameXXK.DesktopTraining.TravelVisualRuntime
GameXXK.DesktopTraining.Workbench.TravelVisualStrip
GameXXK.DesktopTraining.Workbench.TravelVisualLoop
GameXXK.DesktopTraining.Workbench.TravelCombatPresentation
GameXXK.DesktopTraining.Workbench.TransparentDesktopPlacement
```

Expected: all selected tests pass, zero Automation errors, and the `L_Main.umap` hash remains unchanged.

### Task 4: Prove the real PIE behavior and record evidence

**Files:**
- Modify: `Content/Python/gamexxk_probe_training_visual_mvp.py`
- Modify: `docs/production/2026-08-18-desktop-training-2d-hud-migration.md`

- [ ] **Step 1: Extend the PIE probe payload**

For at least 60 samples at 0.1 second intervals, record native tick, logical phase, visual phase, scroll offset/delta, scroll velocity, hero action/frame, enemy action/frame, enemy ID, enemy visibility, and both HP fractions. Do not sleep on the UE game thread; sampling waits stay in the external runner.

- [ ] **Step 2: Save dirty packages through UE MCP, stop PIE, and run a cold build**

Use only `scripts/ue_mcp_client.py`/project MCP tools. Never force-close an editor whose dirty-package state cannot be proven safe. Run UBT with `-NoHotReload`; Live Coding is not verification.

- [ ] **Step 3: Run fresh Automation and PIE**

Run the focused suite above, then launch `/Game/GameXXK/Maps/L_DesktopTrainingHUD` with `scripts/run_training_visual_pie_probe.py --capture --keep-pie`. Save JSON samples and a 1920×1080 screenshot under `Saved/HarnessReports/`.

- [ ] **Step 4: Apply dynamic acceptance checks**

The samples must prove: UE ticks continue; walking offset deltas do not exhibit the old unexplained 0.8–0.9 second zero plateau; combat uses a non-walk action throughout the intentional lane stop; enemy visibility is true during encounter/attack/hit/death; death retains the defeated enemy identity; loop completion does not reset the offset to zero.

- [ ] **Step 5: Review the captured visual evidence**

Review the new screenshot and motion evidence with a suitable method. Record P0/P1/P2 findings covering unit readability, Idle/Attack/Hit/Death evidence, HP clarity, text noise, non-stretched sprites, and comparison against the approved strip reference.

- [ ] **Step 6: Record actual results and remaining scope**

Append commands, exit codes, report paths, screenshot path, visual-review report path, map hash, and current dirty-state caveat to the production note. Explicitly list the not-yet-complete shared 3×3 formation/runtime work; do not mark the overall desktop Training goal complete.
