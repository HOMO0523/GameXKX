# Independent Inventory Window Design

## Goal

Replace the current town side-panel inventory/shop presentation with a real player-facing inventory window. The same inventory window must support two contexts:

- Free inventory: opened by `I` during town play, non-modal, player movement remains available.
- Merchant trade: opened by pressing `F` on the merchant NPC, modal, player movement and world interactions are locked until the window closes.

The window must make bought equipment usable from the visible UI. A player who buys `青锋短剑`, `竹编轻甲`, `鹤纹护符`, or `墨砚坠饰` must be able to equip it into the correct equipment slot without relying on test-only APIs.

## Current Problem

The data layer already supports inventory, consumables, equipment, buying, and selling through `UGameXXKMVPRules` and `UGameXXKMVPSubsystem`. `UGameXXKInventoryWidget::EquipItemById` also proves that equipment can be applied by API.

The visible town UI does not expose that interaction. `UGameXXKTownOverlayWidget` currently owns a side panel with command buttons, equipment text boxes, backpack slots, merchant stock slots, and purchase confirmation. This is useful for MVP validation, but it still feels like a debug/demo panel because:

- The open/close controls live in the side HUD button list instead of inside the window.
- Inventory, character, equipment, and merchant surfaces are laid out as one mixed panel.
- Backpack slots have no selected-item state or action surface.
- Equipment slots are text labels, not interactive slots.
- Bought equipment has no visible path from backpack item to equipment slot.
- Merchant mode and ordinary inventory mode share visuals, but not a clear input model.

## Window Model

Create an independent inventory window surface. The town HUD should only open or close it; the window owns its title bar, close button, tabs, selected item state, equipment slots, detail panel, and transaction controls.

This must not be a cluster of floating slot grids. The inventory needs a real window frame that visually and functionally contains all inventory content.

### Window Frame

The inventory window is built around a dedicated frame/backplate widget.

The frame is responsible for:

- Total window bounds and safe padding.
- Title-bar region.
- Close button placement.
- Currency/status area placement.
- Inner content clipping and spacing.
- Optional modal dim/backdrop attachment in merchant mode.

The frame should use a water-ink/parchment style consistent with the main menu and item slots. If no final frame art exists, the first implementation can use a generated or programmatic parchment/ink backplate, but it must still be one coherent container behind all slots and panels.

Required frame regions:

- Header: title, gold readout, close `X`.
- Left content rail: equipment slots in free inventory, merchant stock in trade mode.
- Center content: backpack grid.
- Right content: selected item detail and action buttons.
- Footer or low-priority strip: optional hints/status text. Avoid putting core close/navigation controls here.

### Close Button

The close control is a real button inside the window header, not an external TownOverlay command button.

Requirements:

- Use an `X` icon or clear close glyph.
- Position at the top-right of the inventory window frame.
- Hit target should be larger than the visual mark.
- `X`, `Esc`, and context toggle (`I` for free inventory) all route through the same close method.
- Closing free inventory only hides the window.
- Closing merchant trade also clears pending trade confirmation and restores player/world input.

The old side-panel `关闭面板` button should not be the primary close mechanism for the new inventory window.

The window has two modes:

### Free Inventory Mode

Opened by `I` or the town backpack button.

- Non-modal.
- Player movement remains active.
- Mouse interaction over the window is captured by UI.
- `I`, `Esc`, or the title-bar close button closes the window.
- Supports viewing items, using consumables, equipping equipment, and viewing equipped slots.

### Merchant Trade Mode

Opened by pressing `F` on the merchant NPC.

- Modal.
- Player movement, `F` interactions, and world clicks are locked while open.
- The scene may dim behind the window.
- `Esc` or the title-bar close button closes the transaction and restores player input.
- Shows merchant stock and the player's backpack in the same window.
- Supports buying from merchant stock, selling from player backpack, and equipping already-owned equipment if useful.

## Layout

Use one reusable window shell:

- Title bar: `行囊` in free mode, `商铺` in merchant mode.
- Right-side close button inside the title bar.
- Currency readout in the title bar.
- Optional tabs or mode labels for `全部`, `消耗`, `装备`, and later `材料`.
- Left column: equipment slots in free mode, merchant stock in trade mode.
- Center: player backpack grid.
- Right column: selected item detail and valid actions.

Backpack and stock slots use the same slot component:

- Ink slot background.
- Item icon image.
- Quantity or price label.
- Selected highlight.
- Disabled/unavailable visual state.

Equipment slots become real slots:

- Weapon slot.
- Armor slot.
- Accessory slot.

Each equipment slot shows the equipped item icon/name or an empty-slot placeholder. Equipment slots must be selectable. Drag/drop can be added later, but the slot model should be ready for it.

## Interaction Rules

The first implementation should prioritize a stable click-driven loop.

### Item Selection

Clicking any backpack, stock, or equipment slot sets:

- `SelectedItemId`
- `SelectedSlotSource`
- `SelectedSlotIndex` or equipment slot type

The detail panel refreshes from the selected item.

### Detail Actions

The detail panel shows only valid actions:

- Consumable in backpack: `使用`
- Equipment in backpack: `装备`
- Equipped item slot: `卸下` can be hidden in the first pass if unequip is out of scope
- Merchant stock item: `购买`
- Player backpack item in merchant mode: `出售`

For MVP, `装备`, `使用`, `购买`, and `出售` are enough.

### Equip

When the selected backpack item is a weapon, armor, or accessory, the detail panel shows `装备`. Pressing it calls the same existing equipment rule path:

`UGameXXKMVPSubsystem::EquipItem(ItemId)` -> `UGameXXKMVPRules::EquipItem(State, ItemId)`

After success:

- The corresponding equipment slot updates.
- Player stats refresh.
- The selected item remains selected unless the user changes selection.
- The detail panel reflects equipped state.

### Buy And Sell

Merchant mode reuses the same slot and detail system.

Selecting a merchant stock item shows item details and `购买`.
Selecting a player backpack item shows item details and `出售`.

Buying, selling, and future destructive actions use a dedicated confirmation widget, not ad hoc inline text inside the detail column.

### Confirmation Dialog Widget

Create a separate reusable confirmation dialog widget for transaction and risk actions.

Examples:

- `购买 金疮药 10金？`
- `卖出 金疮药 5金？`
- `丢弃 金疮药 x1？` when discard is later added.

The dialog owns:

- Dialog backplate/frame.
- Prompt text.
- Optional item icon and amount/price summary.
- Confirm button.
- Cancel button.

The dialog is modal relative to the inventory window:

- While open, clicking other inventory slots does not change selection.
- Confirm executes the pending action and closes the dialog.
- Cancel closes the dialog without changing inventory/gold/equipment.
- `Esc` cancels the dialog first; if no dialog is open, `Esc` closes the window.

In free inventory mode, the confirmation dialog only blocks the inventory window. In merchant trade mode, the whole trade window is already modal to gameplay, and the confirmation dialog is modal within that trade window.

Implementation should model confirmation explicitly:

- `PendingConfirmationAction`: `None`, `Buy`, `Sell`, `Discard`
- `PendingConfirmationItemId`
- `PendingConfirmationQuantity`
- `PendingConfirmationPrice`

The old `PurchaseConfirmationPanel` embedded in `UGameXXKTownOverlayWidget` is a temporary MVP path. The independent inventory window should replace it with the reusable dialog widget.

## Input Ownership

The player controller should distinguish window context from raw town panel mode.

Free inventory mode:

- UI accepts mouse clicks over the window.
- Character movement input remains enabled.
- `I` toggles the window.
- `Esc` closes the window if it is open.

Merchant trade mode:

- UI owns focus.
- Character movement input is disabled or ignored.
- World `F` interactions are disabled or ignored.
- Closing the window restores movement and town interactions.

This should be expressed explicitly in state, not inferred from visibility alone.

## Suggested State

Keep runtime inventory authority in `FGameXXKRuntimeState::Inventory` and equipment fields. Add UI state at the widget/player-controller layer unless persistence is needed.

Suggested widget/controller state:

- `EGameXXKInventoryWindowMode`: `None`, `FreeInventory`, `MerchantTrade`
- `FName SelectedItemId`
- `EGameXXKInventorySlotSource`: `PlayerBackpack`, `MerchantStock`, `Equipment`
- `EGameXXKEquipmentSlot`: `None`, `Weapon`, `Armor`, `Accessory`
- `FName PendingTradeItemId`
- `EGameXXKTradeAction`: `None`, `Buy`, `Sell`
- `bool bModalInputLock`

Runtime save data does not need to store selected UI item or open window mode. It should continue storing actual inventory, gold, equipment, quest, NPC state, and player location.

## Scope

In scope for the first implementation:

- Independent inventory window shell.
- Real total window frame/backplate containing all inventory UI.
- Header `X` close button inside the window.
- Reusable confirmation dialog widget with confirm/cancel buttons.
- Free inventory mode opened by `I`.
- Merchant trade modal opened by merchant `F`.
- Shared backpack slot component.
- Merchant stock slot component using the same slot model.
- Interactive weapon, armor, and accessory equipment slots.
- Click item to select.
- Detail panel with valid actions.
- Equip bought equipment through visible UI.
- Use consumables through visible UI.
- Buy and sell through visible UI with the reusable confirmation dialog.
- Close button inside the window.
- `I` and `Esc` close behavior.
- Modal input lock in merchant mode.

Out of scope for the first implementation:

- Drag/drop equip.
- Stack splitting.
- Multi-quantity purchase selector.
- Sorting beyond simple category tabs or fixed ordering.
- Unequip into backpack unless needed by the equipment implementation.
- Gamepad navigation.
- Final art polish beyond using existing ink slot and item icons.

## Acceptance

- `L_QingshanInn` PIE can open the free inventory with `I`.
- Free inventory appears inside one coherent window frame/backplate; slots are not visually floating on the screen.
- Free inventory mode does not block WASD movement.
- Free inventory has its own top-right `X` close button and can close with `I` or `Esc`.
- Pressing `F` on the merchant opens merchant trade mode.
- Merchant trade appears inside the same coherent window frame/backplate, with merchant stock and player backpack contained by the window.
- Merchant trade mode blocks WASD movement and other `F` interactions until closed.
- Merchant trade mode has its own top-right `X` close button and can close with `Esc`.
- Bought equipment appears in the player backpack grid.
- Selecting bought equipment shows an `装备` action.
- Pressing `装备` equips the item into the correct equipment slot.
- Equipped weapon/armor/accessory slots visually update.
- Player stats update after equipment changes.
- Selecting merchant stock shows `购买`.
- Pressing `购买` opens the reusable confirmation dialog.
- Confirming purchase in the dialog changes gold and backpack quantity.
- Canceling purchase in the dialog leaves gold and backpack quantity unchanged.
- Selecting a player backpack item in merchant mode shows `出售`.
- Pressing `出售` opens the reusable confirmation dialog.
- Confirming sale in the dialog changes gold and backpack quantity.
- Canceling sale in the dialog leaves gold and backpack quantity unchanged.
- All actions use the existing `UGameXXKMVPSubsystem` / `UGameXXKMVPRules` inventory authority.

## Test Strategy

Add tests before implementation:

- Unit/widget test: bought weapon can be selected in the visible inventory window and equipped through the detail action.
- Unit/widget test: inventory window exposes a top-right close button and coherent frame/backplate.
- Unit/widget test: buy/sell actions open the reusable confirmation dialog with confirm/cancel controls.
- Unit/widget test: canceling the confirmation dialog does not change gold, quantity, or equipment.
- Unit/widget test: equipment slot state reflects equipped weapon, armor, and accessory.
- Unit/widget test: free inventory mode reports no modal input lock.
- Unit/widget test: merchant trade mode reports modal input lock.
- PIE/player-controller test: `I` opens/closes free inventory while movement input remains allowed.
- PIE/player-controller test: merchant `F` opens trade mode and blocks movement until close.
- MVP flow test: buy an equipment item from merchant, equip it through visible UI, and verify stats.

## Migration Notes

The existing `UGameXXKTownOverlayWidget` can be reduced over time:

- Keep town status and lightweight command entry points.
- Move backpack, character/equipment, and merchant trade UI into the independent inventory window.
- Preserve current helper APIs temporarily for tests while the new window takes over visible interaction.
- Avoid rewriting inventory rules. The UI should call existing subsystem methods.
