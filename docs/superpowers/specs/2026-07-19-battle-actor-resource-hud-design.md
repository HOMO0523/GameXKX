# Battle Actor Resource HUD Design

**Date:** 2026-07-19  
**Status:** User-approved design; awaiting implementation-plan review

## Goal

Make each battle unit own its readable, screen-stable foot HUD as components of `AGameXXKBattleSceneUnitActor`. The HUD must correctly show health, individual qi, armor, and status effects without duplicating the same display in `UGameXXKBattleBoardWidget`.

## User-visible rules

- Every living battle unit shows a red health row formatted as `气血 当前 / 最大`.
- Hero, permanent companion, and temporary support NPC show a blue qi row formatted as `气力 当前 / 最大`.
- Monsters never show a qi row, even when their combat data contains a Mana field.
- Armor and active status icons remain below the resource rows. Icons retain their current hover tooltip behavior.
- The HUD is centered at the visible character's foot, not at a generic world offset, and stays legible regardless of camera perspective, PIE viewport size, or DPI scale.
- Shared card-play energy remains a hand-area resource. It is not the per-unit blue qi resource and is never displayed in this component.

## Current root cause

The current actor has a world-space `StatusWidgetComponent` at a fixed `(-76, 0, -82)` offset. Separately, the battle board creates a screen-space copy using manually projected positions. That produces two presentation paths, a fixed incorrect anchor, and coordinate-space/DPI drift in embedded PIE.

The actor also receives HP through its legacy runtime facade but does not retain Mana/MaxMana for its status widget. The card runtime's `FGameXXKCardCombatUnit` is the authoritative source for HP, Mana, armor, and statuses.

## Architecture

```text
AGameXXKBattleSceneUnitActor
├─ BattleVisual (existing PaperFlipbook component)
├─ HudAnchorComponent (new scene component)
│  └─ derives its local location from BattleVisual bounds / character foot
├─ StatusWidgetComponent (existing WidgetComponent, moved to Screen space)
│  └─ UGameXXKBattleUnitStatusWidget
└─ cached presentation values
   ├─ HP / MaxHP
   ├─ Mana / MaxMana
   ├─ Armor
   └─ Statuses

FGameXXKCardCombatUnit (authoritative battle state)
        ↓ by UnitId
AGameXXKBattleSceneUnitActor refresh
        ↓
StatusWidgetComponent user widget
```

### Component responsibilities

1. `HudAnchorComponent` is attached to the actor's visual hierarchy and represents the semantic foot point. It must use the visible flipbook bounds plus a small, explicit local correction only when a specific sprite requires one.
2. `StatusWidgetComponent` remains owned by the actor, attaches to `HudAnchorComponent`, and uses `EWidgetSpace::Screen`. It is the only visible unit-resource HUD.
3. `UGameXXKBattleUnitStatusWidget` owns visual layout only. It receives already-resolved values and never mutates battle state.
4. `UGameXXKBattleBoardWidget` stops creating or positioning battle unit footers. It continues to own hand cards, target arrows, enemy intent cards, and board-level safety presentation.

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

## Layout and interaction

- The footer uses a larger screen-space size that can fit two resource rows and status icons without shrinking badges.
- Health and qi are separate rows, each with a PSD-consistent frame/fill treatment. Health fill is red; qi fill is muted blue.
- Text sits above or within its own row at readable contrast and always contains current and maximum values; it never relies only on bar fill percentage.
- Status badges preserve `SelfHitTestInvisible` on the noninteractive footer layer and `Visible` tooltip-bearing icon children, so selecting a target through the HUD remains possible while status tooltips still work.
- The old world-space widget cannot remain visibly rendered alongside the screen-space component. It may only be retained as a nonvisual hit bridge if the dedicated hover regression test proves it is needed.

## Acceptance tests

1. A hero actor reads a known authoritative `HP/MaxHP/Mana/MaxMana` snapshot and renders both labeled rows with matching values.
2. A permanent companion and a temporary support NPC render their blue qi rows from their own values.
3. An enemy actor never renders a blue qi row, including when its runtime data has nonzero Mana.
4. After a card consumes Mana, refreshing the actor changes the qi number and fill from the authoritative card unit in the same battle state.
5. After damage, healing, armor, or status mutation, the actor widget shows the matching authoritative values and no stale duplicate footer exists on the board.
6. The hero, companion, support NPC, standard monster, black bear, and tiger boss keep the HUD centered at their visible foot point at supported viewport sizes.
7. Status icons remain hoverable, do not block legal target clicks, and retain a physical readable size after PIE DPI scaling.

## Out of scope

- Changing card costs or the shared-energy economy.
- Adding new combat resources beyond the existing individual Mana/MaxMana pair.
- Reauthoring character sprites, camera placement, or battle terrain.
