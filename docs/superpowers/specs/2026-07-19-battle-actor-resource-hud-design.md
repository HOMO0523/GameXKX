# Battle Actor Resource HUD Design

**Date:** 2026-07-19  
**Status:** User-approved component split; awaiting updated implementation-plan review

## Goal

Make each battle unit own separate, readable, screen-stable resource and status-effect HUD components under `AGameXXKBattleSceneUnitActor`. The components must correctly show health, individual qi, armor, and status effects without duplicating the same display in `UGameXXKBattleBoardWidget`.

## User-visible rules

- Every living battle unit shows a red health row formatted as `气血 当前 / 最大`.
- Hero, permanent companion, and temporary support NPC show a blue qi row formatted as `气力 当前 / 最大`.
- Monsters never show a qi row, even when their combat data contains a Mana field.
- Armor and active status icons are rendered by an independent status-effects component below the resource component. Each visual icon is an independent reusable child widget and retains its hover tooltip behavior.
- The HUD is centered at the visible character's foot, not at a generic world offset, and stays legible regardless of camera perspective, PIE viewport size, or DPI scale.
- Shared card-play energy remains a hand-area resource. It is not the per-unit blue qi resource and is never displayed in this component.

## Current root cause

The current actor has one world-space `StatusWidgetComponent` at a fixed `(-76, 0, -82)` offset, and that one widget mixes resource bars with a dynamically rebuilt `StatusBadgeRow`. Separately, the battle board creates a screen-space copy using manually projected positions. This produces two presentation paths, a fixed incorrect anchor, coordinate-space/DPI drift in embedded PIE, and an over-coupled resource/status presentation unit.

The actor also receives HP through its legacy runtime facade but does not retain Mana/MaxMana for its status widget. The card runtime's `FGameXXKCardCombatUnit` is the authoritative source for HP, Mana, armor, and statuses.

## Architecture

```text
AGameXXKBattleSceneUnitActor
├─ BattleVisual (existing PaperFlipbook component)
├─ HudAnchorComponent (new scene component)
│  └─ derives its local location from BattleVisual bounds / character foot
├─ ResourceHudAnchorComponent (new scene component)
│  └─ ResourceHudWidgetComponent (Screen-space WidgetComponent)
│     └─ UGameXXKBattleUnitResourceWidget
├─ StatusEffectsAnchorComponent (new scene component)
│  └─ StatusEffectsWidgetComponent (Screen-space WidgetComponent)
│     └─ UGameXXKBattleUnitStatusEffectsWidget
│        └─ UGameXXKBattleStatusIconWidget × N (armor / each active status)
└─ cached presentation values
   ├─ HP / MaxHP
   ├─ Mana / MaxMana
   ├─ Armor
   └─ Statuses

FGameXXKCardCombatUnit (authoritative battle state)
        ↓ by UnitId
AGameXXKBattleSceneUnitActor refresh
        ├─ resource snapshot → ResourceHudWidgetComponent
        └─ armor/status snapshot → StatusEffectsWidgetComponent
```

### Component responsibilities

1. `HudAnchorComponent` is attached to the actor's visual hierarchy and represents the semantic foot point. It must use the visible flipbook bounds plus a small, explicit local correction only when a specific sprite requires one.
2. `ResourceHudAnchorComponent` and `StatusEffectsAnchorComponent` attach to `HudAnchorComponent` and provide separately adjustable placement for the two visible components. The resource component occupies the upper position; the status-effects component occupies the row directly below it.
3. `ResourceHudWidgetComponent` uses `EWidgetSpace::Screen` and owns only the health and optional qi rows. `UGameXXKBattleUnitResourceWidget` receives resolved HP/Mana values and never creates armor or status icons.
4. `StatusEffectsWidgetComponent` uses `EWidgetSpace::Screen` and owns only armor and active statuses. `UGameXXKBattleUnitStatusEffectsWidget` receives resolved armor/status values and dynamically creates one `UGameXXKBattleStatusIconWidget` per visible armor/status model.
5. `UGameXXKBattleStatusIconWidget` owns one icon, its stack number and its tooltip. It has no actor component or combat authority of its own; it is a reusable child of the actor-owned status-effects component. This avoids one world/screen component per effect while preserving icon-level lifecycle and hover behavior.
6. `UGameXXKBattleBoardWidget` stops creating or positioning battle unit footers. It continues to own hand cards, target arrows, enemy intent cards, and board-level safety presentation.

## Data contract

`AGameXXKBattleSceneUnitActor` resolves a presentation snapshot by `UnitId` from `CardRun.ActiveBattle.Units` whenever card combat is active:

| Field | Source | Display use |
|---|---|---|
| `HP`, `MaxHP` | authoritative card combat unit | red health fill and `气血 当前 / 最大` |
| `Mana`, `MaxMana` | authoritative card combat unit | blue qi fill and `气力 当前 / 最大` |
| `Armor` | authoritative card combat unit | armor badge |
| `Statuses` | authoritative card combat unit | sorted status badges and tooltips |

Before card combat is initialized, the existing validated legacy runtime unit remains a temporary fallback. After initialization, the actor must not show stale legacy values when the authoritative card unit differs.

`bShowQi` is true only when the unit is not an enemy and `MaxMana > 0`. This includes temporary support NPCs. `bShowQi` is false for every enemy.

The Actor sends two separate immutable presentation inputs during refresh:

| Consumer | Inputs it may receive | Inputs it must not receive |
|---|---|---|
| `UGameXXKBattleUnitResourceWidget` | slot/name, HP/MaxHP, Mana/MaxMana, `bShowQi` | Armor, Statuses |
| `UGameXXKBattleUnitStatusEffectsWidget` | Armor, Statuses | HP, Mana, shared hand energy |
| `UGameXXKBattleStatusIconWidget` | one resolved `FGameXXKBattleStatusBadgeModel` | runtime state, actor lookup, card mutation APIs |

## Layout and interaction

- The resource and status-effects components are independently positioned under the same Actor foot anchor. Their screen-space offsets are calibrated together so resource rows remain above the icon row without overlap.
- The resource component uses a larger screen-space size that can fit two rows. Health and qi are separate rows, each with a PSD-consistent frame/fill treatment. Health fill is red; qi fill is muted blue.
- Text sits above or within its own row at readable contrast and always contains current and maximum values; it never relies only on bar fill percentage.
- The status-effects component's container preserves `SelfHitTestInvisible`; each independent icon child is `Visible` and tooltip-bearing. Adding or removing a debuff rebuilds only the status icon children, not the resource component.
- The old world-space widget and its `UWidgetInteractionComponent` hover bridge are removed. In UE 5.8, a screen-space `UWidgetComponent` has no traceable collision body, so status-icon hover must use the native screen-layer UMG pointer path instead. The acceptance pass must prove that the visible badge child still receives hover while the noninteractive footer area lets card-target clicks reach the actor's `HitArea`.

## Acceptance tests

1. A hero actor owns a distinct screen-space `ResourceHudWidgetComponent` and `StatusEffectsWidgetComponent`, both attached under its semantic foot-anchor hierarchy.
2. A hero actor reads a known authoritative `HP/MaxHP/Mana/MaxMana` snapshot and renders both labeled resource rows with matching values.
3. A permanent companion and a temporary support NPC render their blue qi rows from their own values; an enemy actor never renders a blue qi row, including when its runtime data has nonzero Mana.
4. After a card consumes Mana, refreshing the actor changes only the resource component's qi number and fill from the authoritative card unit in the same battle state.
5. After armor or status mutation, refreshing the actor changes only the status-effects component's independent icon children, including stack counts and tooltips. Damage/healing does not recreate its status icon children.
6. The hero, companion, support NPC, standard monster, black bear, and tiger boss keep both components centered as a vertical group at their visible foot point at supported viewport sizes.
7. Status icons remain hoverable, do not block legal target clicks, and retain a physical readable size after PIE DPI scaling. No stale duplicate footer exists on the board.

## Out of scope

- Changing card costs or the shared-energy economy.
- Adding new combat resources beyond the existing individual Mana/MaxMana pair.
- Reauthoring character sprites, camera placement, or battle terrain.
- Making every individual armor/status icon an Actor-level `UWidgetComponent`; icon-level independence is provided by reusable children of the single actor-owned status-effects component.
