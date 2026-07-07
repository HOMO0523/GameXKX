# Battle Command Targeting Design

## Purpose

Replace the current always-visible battle action buttons with a player-facing command interaction:

- right-click a party character to open a contextual battle command menu
- click an attack skill to enter target selection
- show a curved ink-arrow from the selected character toward the mouse
- left-click an enemy to execute the selected skill
- cancel targeting with right-click or Esc

The goal is to make the first combat screen feel like an actual playable RPG battle scene instead of a debug command panel.

## Current Problem

The battle screen currently has two prototype shortcuts:

- `UGameXXKBattleBoardWidget` creates a permanent vertical action button stack.
- `UGameXXKBattleBoardWidget::NativeOnMouseButtonDown` can right-click anywhere on the widget and execute a basic attack against the first living enemy.
- `AGameXXKMVPPlayerController::TryHandleBattleSceneRightClick` can right-click an enemy actor and immediately execute the hero's basic attack.

Those shortcuts prove the rules layer works, but they do not match the desired interaction. The player should start from the party character, choose a command, then target an enemy when the command needs a target.

## Confirmed Decisions

- Command menu anchor: right-click screen coordinate.
- Targeting mode: click a skill once, then the arrow follows the mouse.
- Confirm target: left-click an enemy while targeting.
- Empty-space left-click: keep targeting active.
- Cancel: right-click or Esc.
- During targeting, the command menu stays visible but becomes disabled and semi-transparent, so the player can still see the selected command context.
- Attack command buttons use a pale red ink-button tint.
- Defend uses a pale blue ink-button tint.
- Healing and utility commands use a neutral teal-black ink-button tint.
- Arrow visuals use generated ink art, not plain debug lines.

## Scope

In scope:

- Change battle command buttons from always visible to contextual.
- Allow right-clicking party `AGameXXKBattleSceneUnitActor` instances to open the menu.
- Allow attack commands to enter targeting mode instead of immediately attacking the first enemy.
- Allow targeting mode to execute `BasicAttack` and `CraneWingSlash` against the enemy under the cursor.
- Keep `GuiyuanArt`, `Defend`, and `HealingPowder` as immediate self/party actions for the first pass.
- Draw a curved, stretchable targeting arrow in UMG using the generated ink assets.
- Preserve the existing battle rules in `UGameXXKMVPRules` and `UGameXXKMVPSubsystem`.
- Update automation tests before implementation.

Out of scope for this pass:

- Multi-character party command turns.
- Choosing which party member receives `GuiyuanArt` or `HealingPowder`.
- Enemy-target highlighting animations beyond basic hover/valid-target feedback.
- Full battle logs, damage floaters, turn order UI, or animation timing.
- Replacing the generated ink assets with final hand-authored art.

## Visual Assets

Generated source art is stored under:

- `Content/GameXXK/SourceArt/UI/Battle/battle_target_arrow_head.png`
- `Content/GameXXK/SourceArt/UI/Battle/battle_target_ink_dots_atlas.png`

Chroma-key source files are also kept in the same directory:

- `battle_target_arrow_head_chromakey.png`
- `battle_target_ink_dots_atlas_chromakey.png`

The alpha PNGs have transparent corners and clean subject coverage. They should be imported into `/Game/GameXXK/UI/Battle/Textures` before the UMG implementation references them.

`battle_target_ink_dots_atlas.png` is a source atlas, not the final runtime unit. It contains 12 separate ink dabs in a 4 by 3 layout. The implementation should automatically slice it into individual alpha PNGs before import, for example:

- `battle_target_ink_dab_00.png`
- `battle_target_ink_dab_01.png`
- ...
- `battle_target_ink_dab_11.png`

The slicer should detect each dab's non-transparent bounds, crop with padding, preserve alpha, and write deterministic filenames under `Content/GameXXK/SourceArt/UI/Battle/Generated/`. The original atlas remains as source art. UMG should reference the sliced dab textures or imported equivalents, not manually sample hard-coded regions from the atlas.

The main-menu ink button base should remain the style source for command buttons. Battle command buttons can use the same texture with per-button color tint rather than generating separate button images for every skill.

## Interaction State Machine

`UGameXXKBattleBoardWidget` owns the battle UI interaction state:

- `Hidden`
- `Idle`
- `CommandMenuOpen`
- `TargetingBasicAttack`
- `TargetingCraneWingSlash`

`Hidden` is used outside `EGameXXKScreen::Battle`.

`Idle` shows battle status and HP/MP information, but no command menu.

`CommandMenuOpen` shows the command menu at the latest right-click screen position. It stores:

- selected party index
- selected party screen position
- menu anchor screen position

`TargetingBasicAttack` and `TargetingCraneWingSlash` store:

- selected party index
- selected command
- arrow start screen position
- current mouse screen position

On successful command execution, the widget returns to `Idle`, hides menu and arrow, refreshes battle visuals, and lets `GameXXKLevelFlow` travel if the battle ends.

## Input Flow

`AGameXXKMVPPlayerController` remains the mouse input coordinator.

Right-click behavior:

1. If targeting mode is active, cancel targeting.
2. Otherwise, hit-test the cursor.
3. If the hit actor is a living party `AGameXXKBattleSceneUnitActor`, open the battle command menu at the cursor screen position.
4. Do not attack enemies directly from right-click.

Left-click behavior:

1. If targeting mode is inactive, let normal route-map or UI input continue.
2. If targeting mode is active and the hit actor is a valid enemy, execute the selected targeted skill against that enemy index.
3. If targeting mode is active and the click misses enemies, keep targeting active and keep the arrow visible.

Esc behavior:

1. If targeting mode is active, cancel targeting.
2. If command menu is open, close it.
3. Otherwise, do not add new battle behavior in this pass.

Mouse move behavior:

- While targeting, update the arrow end position every frame or on mouse move.
- The arrow starts at the selected party actor's projected screen position, not the menu anchor.

## Component Responsibilities

### `AGameXXKBattleSceneUnitActor`

This actor should describe its interaction role, not own command execution.

Responsibilities:

- expose `IsEnemyUnit()`
- expose `GetUnitIndex()`
- expose whether a party unit can open commands
- expose whether an enemy unit can receive targeted commands
- refresh its visual from runtime state after actions

The existing `ApplyPrimaryPartyAttack` shortcut should be removed from the player-facing input path or kept only as a test helper during transition.

### `AGameXXKMVPPlayerController`

Responsibilities:

- route right-click, left-click, mouse movement, and Esc to the battle board when the runtime screen is `Battle`
- perform world hit-testing against `AGameXXKBattleSceneUnitActor`
- project selected party actor world location to screen for arrow start
- avoid directly calling `ExecuteBattleBasicAttack` from cursor input

The player controller should call battle-board methods such as:

- `OpenCommandMenuForPartyUnit(PartyIndex, MenuScreenPosition, UnitScreenPosition)`
- `BeginTargetingBattleAction(ActionName)`
- `UpdateTargetingPointer(ScreenPosition)`
- `ConfirmTargetingEnemy(EnemyIndex)`
- `CancelBattleTargeting()`

### `UGameXXKBattleBoardWidget`

Responsibilities:

- display HP/MP/status text
- create and position the contextual command menu
- style command buttons with ink backgrounds and tints
- decide whether each command is enabled
- own targeting UI state
- draw or compose the targeting arrow
- call `UGameXXKMVPSubsystem` command methods
- refresh itself and level flow after command execution

The permanent action box should be replaced by a hidden-by-default menu panel.

### `UGameXXKMVPSubsystem` And `UGameXXKMVPRules`

No new battle-rule semantics are required for the first pass. The UI should continue to call existing methods:

- `ExecuteBattleBasicAttack(PartyIndex, EnemyIndex)`
- `ExecuteBattleCraneWingSlash(PartyIndex, EnemyIndex)`
- `ExecuteBattleGuiyuanArt(PartyIndex)`
- `ExecuteBattleDefend(PartyIndex)`
- `ExecuteBattleHealingPowder(PartyIndex)`

## Arrow Rendering

The first implementation should use UMG rather than scene geometry.

Recommended rendering:

- `BattleBoardWidget` owns a targeting overlay.
- The overlay uses `NativePaint` or child image widgets to draw along a quadratic or cubic Bezier curve.
- Start point is the selected party actor's projected screen position.
- End point is the current mouse screen position.
- Control point is offset perpendicular to the start/end vector to create a slight curve.
- Ink dots are sampled along the curve and rendered using the auto-sliced ink dab textures generated from `battle_target_ink_dots_atlas.png`.
- The arrow head uses `battle_target_arrow_head.png`, rotated to match the curve tangent near the end.

The first pass can use the 12 generated dabs as a fixed pool. It does not need a general-purpose atlas parser, but it should not require hand-cutting or manually maintained atlas rectangles.

Targeting colors:

- Basic attack: pale red tint.
- Crane Wing Slash: brighter red or red-white tint.
- Invalid target hover: reduced alpha.

## Button Styling

Command buttons should use the main-menu ink button style as the base:

- white text
- brush-shaped background
- no plain rectangular debug button
- fixed button size to prevent layout shift
- disabled state lowers opacity but preserves readability

Attack buttons:

- pale red tint
- labels: `普攻`, `鹤羽斩`

Defense button:

- pale blue tint
- label: `防御`

Utility buttons:

- neutral teal-black tint
- labels: `归元术`, `金疮药`

## Tests

Add tests before implementation.

Widget tests:

- battle board starts with no visible command menu in battle idle state
- opening command menu for party index `0` makes the menu visible at the supplied screen position
- command menu exposes enabled `普攻`, `鹤羽斩`, and `防御` when hero is ready
- clicking `普攻` enters targeting mode instead of immediately damaging the first enemy
- targeting mode exposes arrow state with start and end screen positions
- left-clicking empty space while targeting keeps targeting active
- cancel targeting hides arrow and returns to command-menu or idle state according to the final implementation
- `防御`, `归元术`, and `金疮药` still execute immediate actions when enabled

Asset processing tests:

- the ink atlas slicer produces exactly 12 dab PNGs
- each dab PNG has transparent corners
- each dab PNG has nonzero opaque or partially opaque subject pixels
- rerunning the slicer produces the same filenames and does not require manual cleanup

Player-controller tests:

- right-clicking a party battle actor opens the command menu and does not damage enemies
- right-clicking an enemy battle actor no longer applies direct basic attack
- targeting mode plus left-clicking an enemy calls the selected targeted action
- targeting mode plus right-click cancels instead of attacking
- Esc cancels targeting

Actor tests:

- party battle actor reports it can open commands when alive
- defeated party actor cannot open commands
- living enemy actor can be targeted
- defeated enemy actor cannot be targeted

Flow tests:

- after a targeted attack kills the last enemy, victory resolves and level flow returns to the route map
- after a targeted action leads to hero defeat, battle failure returns to town with full HP

Automation:

- Use UBT for C++ compile verification.
- Use `scripts/gamexxk_mvp_playtest.py --skip-build` for `GameXXK.MVP` automation after compile.
- Use UE MCP only for importing/saving texture packages when the editor is available.
- Do not use UnrealBridge.

## Acceptance

This pass is done when:

- Entering `L_BattleScene` no longer shows permanent action buttons.
- Right-clicking the hero or party actor opens the command menu near the cursor.
- Right-clicking an enemy does not directly attack.
- Command buttons use ink-style backgrounds and the agreed color tints.
- `普攻` and `鹤羽斩` enter targeting mode.
- Targeting mode shows a curved ink arrow from party actor to mouse.
- The ink-dot atlas is automatically sliced into 12 reusable dab assets before import/use.
- Left-clicking a valid enemy executes the selected skill against that enemy.
- Left-clicking empty space keeps targeting active.
- Right-click or Esc cancels targeting.
- Immediate commands still work.
- Battle victory and failure flows still pass existing MVP automation.

## Risks

The largest risk is input ownership between UMG and world hit-testing. The player controller should be the only place that decides which world actor is under the cursor. The widget should own UI state and execution calls, not perform world traces.

Another risk is arrow performance if every mouse move creates many widgets. The first implementation should either draw in `NativePaint` or reuse a fixed pool of dot images.

The generated ink atlas is acceptable for the first pass, but automatic slicing may need conservative padding and alpha thresholds to avoid clipping brush edges. Keep the slicer deterministic and non-destructive so the atlas can be regenerated or reprocessed without hand work.

The existing tests currently expect direct right-click attack behavior. Those tests should be updated to the new interaction contract instead of preserving the shortcut.
