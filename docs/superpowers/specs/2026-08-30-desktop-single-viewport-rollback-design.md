# Desktop Single-Viewport Rollback Design

## Goal

Restore the desktop training Workbench to a single UE GameViewport host so the idle strip and its controls are usable without a second large native window appearing behind it. This is a deliberately reversible stabilization step, not the final desktop-overlay architecture.

## Confirmed problem and version boundary

- Commit `0107aebc89a250f7d6c420a80b39080fb5de6729` introduced a dedicated per-pixel-transparent `SWindow`, a hidden primary GameViewport window, and per-tick enforcement that hides the primary window while the Workbench is visible.
- Its parent, `24b96533f5a834ddda4fe2136921e313ad526f11`, hosted the Workbench with `AddToViewport(200)` and had no independent desktop overlay window.
- The current selective story rollback uses controller/Workbench behavior from `16bd8bb`, but that revision still contains the dual-window policy. Therefore reverting story work alone cannot remove the black input-blocking native window.
- Reverting the whole `0107aeb` commit is unsafe: its seven relevant controller, Workbench, layout, and test files contain roughly 7,175 additions and 649 deletions, including substantial unrelated Workbench behavior and presentation work.

## Selected approach

Apply a Viewport-only rollback inside the current codebase:

1. Keep the current Workbench widget tree, idle-strip interaction, task/narrative state, layout calculations, and assets.
2. Stop creating or owning the dedicated `DesktopTrainingOverlayWindow`.
3. Stop hiding, showing, minimizing, restoring, or re-hiding the primary GameViewport window on behalf of the Workbench.
4. Attach the Workbench to the ordinary player viewport at z-order 200, following the pre-overlay controller pattern.
5. Let battle, town, task, inventory, and narrative widgets continue to share that same viewport and their existing z-order/input routing.

The rejected minimize/restore experiment stored as `viewport-policy-minimize-experiment-20260830` is not part of this design and must not be applied.

## Runtime behavior

### Startup and idle Workbench

- `/Game/GameXXK/Maps/L_DesktopTrainingHUD` starts in the primary UE game window.
- The Workbench is created once and added to the primary viewport.
- No second `SWindow` named `GameXXKDesktopOverlay` is created.
- The controller does not execute any per-tick native-window visibility enforcement.
- The primary game window remains visible. It is expected to occupy its configured rectangle and to intercept desktop input; desktop click-through is explicitly outside this stabilization step.

### Opening and closing UI

- Workbench visibility is controlled only through the existing UMG state/visibility path.
- Tab folding, idle-strip buttons, task-button movement, panels, and dialogue behavior remain owned by their existing widget/session logic.
- Opening battle, town, task, inventory, formation, or narrative UI must not trigger native-window show/hide operations.
- Closing a panel or ending play must not restore a different native window because none was hidden by this feature.

### Failure handling

- If a player viewport is unavailable, retain the existing non-viewport fallback used by controller tests; do not create a native overlay as a fallback.
- Repeated `EnsureDesktopTrainingWorkbenchWidget` calls must not add duplicate viewport entries.
- Shutdown must remain safe when the Workbench was never created or was already removed.

## Code boundary

Expected production edits are limited to:

- `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`

Expected test edits are limited to the existing controller/Workbench automation tests needed to replace the old transparent-overlay contract with the single-viewport contract.

The following are out of scope unless a failing regression test proves they are required:

- Workbench visual/layout code
- Idle-strip state machines and input routing
- Narrative and guide code
- Task/reward rules
- Native overlay plugin removal
- Assets, maps, camera settings, PaperZD content, or user-staged deletions
- A new desktop click-through or tray architecture

## Test-first implementation contract

Before production changes, update/add automation coverage that fails against the current dual-window implementation and proves:

1. Desktop training selects the single-viewport host policy.
2. Ensuring the Workbench attaches it to the player viewport once at z-order 200.
3. The controller no longer constructs a dedicated overlay `SWindow` for normal runtime.
4. Opening, closing, and ticking the Workbench do not change primary GameViewport native-window visibility.
5. Existing Workbench interaction, native-region-independent layout, and player-flow suites remain green where their requirements still apply.

The obsolete test that requires a per-pixel transparent overlay window must be replaced, not silently weakened. Native-window-region tests may remain only if they still cover reusable Workbench logic without requiring a second host window.

## Verification and acceptance

Verification must use a cold UBT build, not Live Coding or Hot Reload, followed by targeted automation and a real standalone run on the canonical 2D map.

Acceptance requires all of the following:

- UBT succeeds.
- The single-viewport and existing relevant automation suites pass.
- Process/window enumeration shows one visible UE game viewport and no independent `GameXXKDesktopOverlay` window.
- The previously reproduced separate large black window is absent.
- Idle-strip controls, Tab folding, task button, task panel, and entry into battle remain clickable in the UE window.
- No unrelated tracked or untracked assets are changed, and the three user-staged scrollbar deletions remain staged.

Visual evidence must be captured after runtime verification and reviewed with a method suitable for the available evidence.

## Reversibility

- Keep the current named stashes intact, especially `pre-selective-workbench-rollback-20260830` and `viewport-policy-minimize-experiment-20260830`.
- Record the exact targeted controller/test diff before runtime verification.
- If the single-window trial is rejected, revert only this trial's controller/test changes; do not apply a repository-wide reset or touch user-owned changes.

## Deferred decision

After the Workbench is stable, choose separately between:

- keeping the single UE window permanently, or
- designing a new desktop-native host with explicit ownership and click-through behavior.

That future decision must not be mixed into this rollback trial.
