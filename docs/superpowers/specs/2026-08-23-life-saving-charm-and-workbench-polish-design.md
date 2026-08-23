# Life-Saving Charm, Camp, Travel Sprite, and Backpack Controls Design

**Status:** Approved.

## 1. Camp choices

Camp has exactly two choices:

1. Acquire the unique relic `保命护符`.
2. Gain `100` route-local travel money (`RouteTravelMoney`).

Camp never grants a healing powder and does not directly heal the party.

If the party already owns `保命护符`, the relic choice remains visible but is disabled with a clear reason. Relics do not stack.

## 2. 保命护符

- Stable ID: `Relic.LifeSavingTalisman`.
- Display name: `保命护符`.
- Trigger: at the first battle health-loss mutation that would leave a party member below `50% MaxHP`, including enemy damage, self-damage, damage over time, reflected damage, and multi-hit cards. Exactly 50% does not trigger.
- Death prevention: before existing death and terminal-state logic observes the protected packet, clamp only that packet's target to a minimum of `1 HP`. The relic never revives a `0 HP` unit and never reopens a terminal battle phase.
- Effect: after the protected packet, every currently living party member restores `ceil(MaxHP * 30 / 100)`, capped at MaxHP.
- Consumption: remove exactly one `保命护符` instance immediately after the successful trigger.
- Multi-hit: the protected packet consumes the relic; later packets, including later hits in the same sequence, use the unchanged death rules and may defeat the target.
- Compatibility: without the relic, health loss, death, terminal phases, and defeated-owner card cleanup are unchanged.
- Atomicity: one-HP protection, relic removal, party healing, legacy projection, presentation audit, and save state commit together or not at all.
- Scope: battle only. Low HP outside battle does not consume the relic.

The relic is unique and cannot be acquired twice simultaneously.

## 3. Relic art

Approved source image:

`SourceArt/UI/Relics/final/T_Relic_LifeSavingTalisman_v1.png`

- 512x512 PNG
- transparent background
- SHA256 `03CCCC7F770AE0B50AAD37ABADBEADEEA7ADF5852FF18DE85DD81EA89E3193D3`

Import destination:

`/Game/GameXXK/UI/Relics/Icons/T_Relic_LifeSavingTalisman`

## 4. Training Travel party visuals

All six fixed companions and all six Quest NPCs must display their authored idle, attack, hit, and death animations in the compact Travel strip.

Loading policy:

1. request the compact `_1k` atlas first;
2. if that package does not exist or loading fails, request the existing `_2k` atlas for the same identity and action;
3. while neither atlas is ready, clear the Image brush and keep the character area transparent;
4. never show the default opaque white UImage rectangle;
5. when the selected member changes, do not leave the previous member's stale frame visible.

Current 1K assets for Hero, Blade, and Tusi Chief continue to be preferred. Other companions and Quest NPCs use their existing 2K atlases until compact variants are authored.

## 5. Backpack Tab and close button

Tab and close are separate controls.

### Backpack expanded

- The top Tab uses the project's selected-tab background.
- It displays `▲`.
- It never uses CloseInk and never renders a white rectangle.
- A separate approved CloseInk `X` is anchored inside the Backpack paper at its top-right corner.

### Backpack collapsed

- The top Tab uses the project's normal-tab background.
- It displays `▼`.
- No Backpack CloseInk is visible.

Clicking the expanded Tab, the Backpack-local `X`, or pressing keyboard Tab uses the same global close transaction: cancel carried/tool transient state, close Warehouse/Formation/Talents/Tools/Training, and return to the idle strip. Reopening starts from a clean Backpack.

## 6. Verification

- Camp tests prove exactly two choices, no healing powder, unique relic gating, and `+100 RouteTravelMoney` without changing ordinary gold.
- Relic tests cover 49%, exactly 50%, lethal damage clamped to 1 HP before death, all-party healing, self/DoT/enemy damage, multi-hit single consumption followed by an unprotected lethal hit, duplicate acquisition rejection, no-relic compatibility, and rollback.
- Atlas tests cover every companion/NPC identity and all four actions with 1K-preferred/2K-fallback resolution.
- Widget tests prove pending/failed atlas loads are transparent, not white.
- Backpack tests prove selected/normal Tab backgrounds, arrow text, separate paper-local X, shared global action, and no old top-strip CloseInk.
- Real PIE cycles every companion and Quest NPC, exercises both Camp choices, triggers and consumes the relic, and leaves the editor running for user review.

Per user instruction, visual acceptance is performed directly by the primary agent and the user; Luna is not used.
