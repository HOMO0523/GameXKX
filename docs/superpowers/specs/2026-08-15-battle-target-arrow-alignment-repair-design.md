# Battle Target Arrow Alignment Repair Design

## Context

During card targeting, the battle board paints a curved sequence of ink dabs from the card owner toward the live pointer and places an orange arrow-head texture at the curve endpoint. The previously accepted PIE capture is `Saved/Codex/target_outcome_Single.png`.

The current source adds an `ArrowHeadTipOffsetRatio` translation before rotating the arrow brush. That translation moves the brush about 26.8 local pixels backward along the straight start-to-end direction. Its horizontal and vertical components vary with pointer direction, so the orange arrow appears down and to the right in the reproduced case and no longer joins the ink-dab curve as in the accepted capture. A real PIE diagnostic confirmed that Slate absolute, safe-stage, and board-local pointer conversions are internally consistent; the added brush translation is the visual regression.

## Goal and Visual Contract

Restore the accepted targeting presentation without redesigning it:

- `End` remains the live targeting pointer expressed in battle-board paint space.
- The arrow brush center must equal `End` before rotation.
- Therefore, for an arrow brush of size `ArrowSize`, its unrotated top-left position is exactly `End - ArrowSize * 0.5f`.
- Rotation continues to follow the start-to-end direction.
- The existing curved ink-dab positions, sizes, ordering, textures, and opacity remain unchanged.

Success means the orange arrow once again sits at the ink trail endpoint like the accepted screenshot, without a direction-dependent extra gap or lower-right displacement.

## Scope

The implementation is deliberately narrow:

1. Remove the `ArrowHeadTipOffsetRatio` compensation and restore center-on-end arrow placement in `UGameXXKBattleBoardWidget::NativePaint`.
2. Remove temporary Win32/cursor and recurring `[ArrowPaint]` diagnostics that were added only to identify the regression.
3. Preserve the current stage-space target-hover resolution fix in `UpdateTargetingPointer`; it corrects target hit testing and is independent of arrow layout.
4. Do not edit or reimport the arrow-head or ink-dab texture assets.
5. Do not change card targeting state, click confirmation, target legality, or outcome-preview behavior.

## Test Boundary

Expose or reuse a small deterministic C++ test seam for arrow-head placement rather than testing pixels in an automation test. Given a start point, endpoint, and arrow size, it returns the same top-left position used by production painting. The production result intentionally depends only on the endpoint and size; accepting the start point lets the regression test prove that changing direction cannot translate the brush. A focused automation regression test will assert:

- the arrow brush center equals the endpoint;
- horizontal, vertical, and diagonal targeting directions do not introduce any directional translation;
- the current offset implementation fails this invariant before the production fix is applied.

The production paint path must call the tested placement calculation so the test cannot pass against disconnected duplicate logic.

## Runtime and Failure Behavior

No new runtime state or failure path is introduced. Existing guards remain authoritative: painting is skipped when targeting is inactive, required textures are unavailable, the dab list is empty, or the start/end distance is too short. The repair only changes the final arrow brush geometry after those checks succeed.

## Verification

Verification will be performed in this order:

1. Add and run the focused automation test in its failing state against the direction-dependent offset.
2. Apply the minimal production change and rerun the focused test.
3. Save dirty UE packages through MCP, stop PIE/editor safely as needed, and run a cold UBT build; do not use Live Coding or Hot Reload.
4. Run the existing battle targeting/widget automation coverage and `scripts/gamexxk_battle_target_art_check.py`.
5. Start a fresh editor session, reproduce the same `Outcome.Single` targeting state in real PIE, and capture the repaired frame.
6. Compare the repaired frame side by side with `Saved/Codex/target_outcome_Single.png`, checking arrow/ink continuity and pointer tracking at more than one direction.

## Non-Goals

- Anchoring the visible texture tip precisely under the cursor.
- Cropping, repainting, or reimporting any targeting art.
- Redesigning the Bézier curve or dab spacing.
- Refactoring unrelated battle-board code.
