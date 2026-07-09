# Unified Town Inventory And Shop Design

## Goal

Town inventory, character equipment, and merchant trade must use one player inventory surface instead of separate-looking UI fragments. The shop should add a merchant stock surface beside the same player backpack, not replace the backpack with text.

## Current Problem

The runtime data is already shared through `FGameXXKRuntimeState::Inventory`, `UGameXXKMVPRules`, and `UGameXXKMVPSubsystem`. The visible UI is not shared: `UGameXXKTownOverlayWidget` renders player inventory as fixed backpack slots, renders shop stock as text lines, and renders equipment as independent text boxes. `UGameXXKInventoryWidget` and `UGameXXKTradeWidget` expose API wrappers but do not drive the same visible slot model.

## Design

`UGameXXKTownOverlayWidget` owns a shared town item-slot view model. A slot has a source (`PlayerInventory`, `ShopStock`, or `Equipment`), an item id, quantity, display text, and action availability. Player backpack slots and shop stock slots are generated from that same slot model and use the same ink slot texture, fixed dimensions, and label rules.

Inventory mode shows equipment plus the 20-slot player backpack. Trade mode shows merchant stock slots beside that same player backpack and keeps the same equipment/action context visible. Character mode emphasizes stats/equipment while still using the same equipment slot labels.

For this MVP pass, selection and drag/drop stay out of scope. The concrete user-facing improvement is visual and structural unification: the merchant shelf is a slot grid, the player backpack remains the same grid while trading, and test helpers can prove both surfaces are driven by shared item-slot data.

## Acceptance

- Town inventory mode still exposes 20 fixed 72x72 backpack slots.
- Trade mode shows a shop stock slot grid using the same slot texture and fixed sizing.
- Trade mode keeps the shared player backpack grid visible.
- Shop stock slot count equals `UGameXXKMVPRules::GetShopItemIds().Num()`.
- Player backpack slots refresh after buying/selling through the same runtime inventory.
- Equipment slots remain visible and read equipped weapon/armor/accessory state.
