# Fullscreen HUD Battle Visual System Design

**Date:** 2026-07-26
**Status:** User-approved design pending written-spec review

## 1. Context

The current battle presentation is split between two systems:

- `L_BattleTown` and spawned battle-scene actors provide persistent idle visuals.
- `UGameXXKBattleAnimationLayerWidget` provides the fullscreen attack, hit, and death presentation.

This split keeps a 3D battle world alive even though the desired result is a flat illustrated battle screen. It also duplicates presentation responsibilities, keeps world rendering active behind an opaque UI, and complicates switching between idle and action clips.

The replacement is one fullscreen HUD-owned battle visual system. Entering a route battle will no longer load `L_BattleTown`. The current world remains loaded but is frozen and not rendered while the battle HUD is active.

## 2. Goals

- Enter and exit route battles without loading another map.
- Present the complete battle over a fullscreen 16:9 illustrated backdrop.
- Use one HUD animation system for formation idle and central action presentation.
- Reuse one imported atlas asset everywhere a given unit action is shown.
- Keep the existing 60-frame, 12 FPS, 512-pixel-cell animation quality.
- Eliminate battle-scene actors and PaperFlipbook ticking from the runtime battle path.
- Preserve route progress, camera state, and input state when the battle closes.
- Keep the incremental battle-animation texture budget at or below 256 MiB.

## 3. Non-goals

- Do not regenerate or redesign the approved character, monster, background, or effect art.
- Do not convert animation cells from square to 16:9.
- Do not delete shared mesh, material, level-art, PaperSprite, or PaperFlipbook assets from the content library during this migration.
- Do not redesign card rules, enemy intent logic, health rules, rewards, or route progression.
- Do not lock, move, replace, or otherwise alter the active camera.

## 4. Confirmed Decisions

1. The route map opens a fullscreen battle HUD directly; it does not open `L_BattleTown`.
2. The current world is frozen and world rendering is disabled while battle UI is active.
3. The fullscreen backdrop is `SourceAssets/PartyDeck/battle-backdrop/battle_arena_riverside_source_v1.png`.
4. Formation and cinematic presentation use one widget system and the same unit widget instances.
5. Animation assets remain 4096x4096 BC7 atlases arranged as an 8x8 grid of 512x512 cells.
6. Each unit and action has one canonical atlas. Every presentation site references that same texture asset.
7. Base playback is 12 FPS. Attack and hit clips play at 2x speed and last 2.5 seconds in game.
8. Formation units are hidden during a central action cinematic.
9. Enemy units occupy the left side and face right. Party units occupy the right side and face left. Runtime code does not automatically mirror approved art.
10. Victory closes the battle HUD and restores the same route map state.

## 5. System Architecture

### 5.1 Battle Overlay Coordinator

A battle overlay coordinator owns entry and exit as a reversible transaction.

On entry it:

1. Captures the current route widget, route state, input mode, pause state, cursor state, and world-rendering state.
2. Starts or resumes the existing battle runtime state without opening a level.
3. Hides the route and town UI that must not remain interactive.
4. Freezes the underlying world.
5. disables world rendering while keeping Slate/UMG rendering available.
6. Sets UI-only battle input.
7. Adds the fullscreen battle HUD to the viewport.

On exit it performs the inverse operations in a guaranteed cleanup path. Cleanup must run for victory, failure, cancellation, load failure, and widget destruction. The coordinator restores the values it captured rather than assuming defaults.

### 5.2 Unified `BattleVisualHUD`

One fullscreen root `CanvasPanel` owns every battle visual. It contains these layers from back to front:

| Z layer | Responsibility |
|---:|---|
| 0 | Opaque riverside battle backdrop |
| 10 | Formation unit animation widgets |
| 20 | Existing unit HUD, health, intent, cards, qi, and commands |
| 30 | 50% black cinematic dimmer |
| 40 | Central attacker and defender presentation |
| 50 | Ink impact, buff, debuff, and screen-space effects |
| 60 | Damage numbers and other transient combat readouts |

There is no separate formation renderer and cinematic renderer. Each unit owns one `BattleUnitVisualWidget` that stays attached to the root canvas. The system changes its canvas slot, size, Z order, atlas, playback state, and visibility when entering or leaving cinematic mode. No widget is reparented or duplicated.

### 5.3 `BattleUnitVisualWidget`

Each visible battle unit has one widget and one dynamic material instance. Its external contract is state-based:

- `ShowFormationIdle(slot)`
- `ShowCinematic(action, anchor, scale)`
- `HideForCinematic()`
- `RestoreFormation()`
- `RemoveAfterDeath()`

The widget does not resolve combat rules. It receives an atlas, playback rate, loop flag, and elapsed-time source from the presentation controller.

### 5.4 Atlas Playback Clock

One HUD-owned real-time clock advances all active atlas widgets. It does not use world delta time and therefore continues while the world is paused.

The clock emits at 12 Hz for normal playback. A 2x clip advances source frames at 24 frames per real second. Hidden widgets do not receive frame updates.

## 6. Animation Asset Contract

Each canonical action texture must satisfy:

| Property | Value |
|---|---:|
| Atlas size | 4096x4096 |
| Grid | 8 columns x 8 rows |
| Cell size | 512x512 |
| Valid frames | 60 |
| Unused cells | 4 |
| Source FPS | 12 |
| Source duration | 5 seconds |
| Compression | BC7 with alpha |
| Mips | Disabled |
| Filter | Bilinear |

For zero-based frame `F`:

```text
Column = F % 8
Row    = floor(F / 8)
UV     = InputUV / 8 + float2(Column, Row) / 8
```

`F` is always clamped or wrapped within `0..59`; the four empty atlas cells are never sampled.

The asset naming contract remains one texture per unit and action, for example:

```text
T_character_00_hero_idle_atlas
T_character_00_hero_attack_atlas
T_character_00_hero_hit_atlas
T_character_00_hero_death_atlas
```

Formation and cinematic use the same canonical texture object for the same clip. There are no duplicated imports, derived action textures, or runtime copies.

## 7. Layout and Orientation

### 7.1 Fullscreen Stage

- The HUD uses a 1920x1080 design stage.
- Anchors and positions are normalized so the stage scales to the viewport.
- The backdrop preserves its 16:9 aspect ratio and uses scale-to-fill behavior.
- Non-16:9 windows use centered cropping; the backdrop is never stretched.
- Unit animation cells remain square 512x512 content placed within the 16:9 stage.

### 7.2 Formation Slots

The current three-slot staging is retained:

| Side | Slot 1 | Slot 2 | Slot 3 |
|---|---:|---:|---:|
| Enemy, left | X 9.5%, Y 60% | X 24.5%, Y 52% | X 39.5%, Y 44% |
| Party, right | X 90.5%, Y 60% | X 75.5%, Y 52% | X 60.5%, Y 44% |

Enemies use their authored right-facing assets. Party units use their authored left-facing assets. Slot assignment never changes sprite orientation.

### 7.3 Central Cinematic

- All formation units are hidden while a central action is active.
- The enemy participant remains on the left and faces right.
- The party participant remains on the right and faces left.
- Participants are displayed at 200% of their formation size.
- Attacker and defender positions do not swap based on turn ownership.
- The shared impact point is the exact screen center.

## 8. Action Presentation Flow

Combat logic produces immutable presentation events containing attacker, target, action, damage, and before/after health values.

For a single hit:

1. Lock battle input.
2. Asynchronously acquire the attack, hit, and common impact atlases.
3. Hide formation visuals.
4. Move the participating unit widgets to their central anchors.
5. Play attack and hit clips together at 2x speed.
6. At source time 2.2 seconds, which is real time 1.1 seconds at 2x playback:
   - start the common ink impact at 4x speed;
   - trigger screen shake;
   - update displayed health;
   - show damage or result text.
7. At real time 2.5 seconds, restore surviving units to formation idle.
8. A unit with a death event plays death before its widget is removed.
9. Unlock input only after the presentation queue is empty.

Multi-hit actions enqueue this sequence once per hit and play it sequentially. They do not overlap hit sequences.

Gameplay can calculate and commit deterministic results before presentation, but visible health, death, victory, and turn transitions wait for the corresponding presentation markers. This prevents early health changes or premature battle closure.

## 9. Resource Loading and Lifetime

- All animation assets are soft-referenced.
- Battle entry loads only the idle atlas for each active participant.
- The turn flow predicts and asynchronously loads the next attacker, defender, and required effect clips.
- Recent action atlases live in a bounded LRU cache.
- The incremental battle-animation resident budget is 256 MiB.
- Cache eviction removes strong references; it does not force garbage collection during battle.
- Closing the battle clears active action handles and cache references. Cleanup or garbage collection may occur after the HUD is gone.
- No synchronous atlas load is allowed on the action presentation path.

## 10. World, Input, and Camera Behavior

While the battle HUD is open:

- The coordinator ensures the underlying world is paused and records whether it was already paused before battle entry.
- Player, NPC, and AI world input is disabled.
- World rendering is disabled through Unreal's world-rendering control so the opaque HUD does not conceal a still-rendering 3D scene.
- HUD animation uses a real-time Slate/UI clock and remains active.
- The camera is not locked, moved, replaced, or given a new view target.

Unreal Engine 5.8 API reference: [Set Enable World Rendering](https://dev.epicgames.com/documentation/unreal-engine/BlueprintAPI/Rendering/SetEnableWorldRendering?lang=en-US).

When the HUD closes, the system restores the exact prior world-rendering, pause, cursor, and input states. Because camera state is never changed, there is no camera restoration step that can introduce drift.

## 11. Migration Scope

- Replace the route-node transition to `L_BattleTown` with battle-overlay entry.
- Make the battle board capable of existing in the current world and owning the fullscreen background.
- Fold the current animation layer behavior into the unified visual controller.
- Stop spawning `AGameXXKBattleSceneUnitActor` for runtime battle presentation.
- Remove battle presentation actors and visible 3D set dressing from `L_BattleTown` if the map is retained for editor/reference purposes.
- Keep shared content assets and user-tuned art intact.
- Leave old PaperSprite and PaperFlipbook assets in the repository until a later, separately approved cleanup.

## 12. Failure Handling

- Missing action atlas: use the unit idle atlas plus the common impact effect and continue resolution.
- Failed or timed-out async load: log the unit/action path, use fallback presentation, and never block the battle state machine.
- Missing idle atlas: use an explicit placeholder visual while keeping the unit selectable and functional.
- Exit during a pending load: cancel the request or invalidate it with a battle-session token so late callbacks cannot touch a destroyed HUD.
- Any exit path: run overlay cleanup and restore route, input, pause, rendering, and cursor state.
- Invalid frame metadata: reject the asset at import/validation time rather than sample an empty atlas cell at runtime.

## 13. Verification and Acceptance

### 13.1 Automated Tests

- Frames 0, 7, 8, and 59 resolve to the correct atlas cells.
- No runtime frame index reaches cells 60 through 63.
- A normal loop advances at 12 FPS.
- A 2x action finishes in 2.5 seconds.
- The hit marker fires at 1.1 seconds within one rendered-frame tolerance.
- Enemy and party slots resolve to the confirmed normalized anchors and authored orientations.
- State transitions cannot double-resolve damage, unlock input early, or close before death presentation.
- Load failure, timeout, cancellation, and widget destruction all restore the overlay transaction.
- Route-node battle entry does not request a map load.

### 13.2 PIE Acceptance Flow

1. Open the route map and select a battle node.
2. Verify that the current level does not change.
3. Verify that the 16:9 riverside backdrop fills the viewport and the 3D world is not rendered.
4. Verify up to three enemies on the left and three party members on the right playing 12 FPS idle.
5. Trigger a party attack and an enemy attack.
6. Verify formation visuals hide, both participants display centrally at 200%, and authored facing remains correct.
7. Verify synchronized 2x attack/hit playback, the 1.1-second impact, screen shake, and health update.
8. Verify multi-hit playback is sequential.
9. Verify death presentation removes the defeated unit.
10. Complete the battle and verify restoration of the same route map and node progress with no camera change.

### 13.3 Performance Acceptance

- Target 60 FPS at 1920x1080 with a 16.67 ms frame budget.
- No runtime battle-scene actors or battle PaperFlipbook ticks.
- Incremental resident battle-animation textures stay within 256 MiB.
- No new texture-pool over-budget warning.
- No synchronous action-atlas load or forced garbage collection during battle.
- Validate at 1288x770 PIE, 1920x1080, and one non-16:9 viewport.

## 14. Definition of Done

The design is complete when route battles open and close as a reversible fullscreen HUD overlay, the illustrated background and all unit/action visuals are rendered by one atlas-driven HUD system, no battle map or camera transition occurs, world rendering is suspended behind the HUD, action timing and orientation match the approved presentation, and the verified runtime remains within the stated frame and texture budgets.
