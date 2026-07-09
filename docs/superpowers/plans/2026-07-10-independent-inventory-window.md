# Independent Inventory Window Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a real independent inventory/trade window with its own frame, top-right close `X`, reusable confirmation dialog, visible equip/use/buy/sell actions, and merchant modal input lock.

**Architecture:** Keep `UGameXXKMVPRules` and `UGameXXKMVPSubsystem` as inventory/equipment/trade authority. Add a new C++ UMG widget `UGameXXKInventoryWindowWidget` that owns free-inventory and merchant-trade modes, selection state, frame layout, equipment slots, backpack/stock grids, detail actions, and confirmation dialog. Keep `UGameXXKTownOverlayWidget` as the lightweight town command/status owner and route `I`/merchant `F` through the player controller into the new window.

**Tech Stack:** UE 5.8 C++, programmatic UMG, GameXXK automation tests, UE MCP/UBT validation, manifest-driven UI asset metadata.

---

## File Structure

- Create `docs/ui/inventory/manifests/inventory_window_frame.json`: source-of-truth manifest for the inventory window frame/backplate.
- Create `docs/ui/inventory/manifests/inventory_confirmation_dialog.json`: manifest for the reusable confirmation dialog frame/buttons.
- Create `docs/ui/inventory/manifests/inventory_close_button.json`: manifest for the header `X`.
- Create `docs/ui/inventory/manifests/inventory_equipment_slots.json`: manifest for weapon/armor/accessory slot visuals.
- Create `scripts/validate_inventory_ui_manifests.py`: checks manifest schema and target paths.
- Create `Source/GameXXK/Public/UI/GameXXKInventoryWindowWidget.h`: independent inventory/trade window public API and test helpers.
- Create `Source/GameXXK/Private/UI/GameXXKInventoryWindowWidget.cpp`: programmatic UMG layout, selection, close, equip/use/buy/sell, confirmation dialog.
- Modify `Source/GameXXK/Public/UI/GameXXKTownOverlayWidget.h` and `Source/GameXXK/Private/UI/GameXXKTownOverlayWidget.cpp`: expose less side-panel behavior where needed and keep compatibility tests passing.
- Modify `Source/GameXXK/Public/GameXXKMVPRules.h` and `Source/GameXXK/Private/GameXXKMVPRules.cpp`: add explicit inventory window mode state only if player-controller tests need runtime-visible mode.
- Modify `Source/GameXXK/Public/GameXXKMVPPlayerController.h` and `Source/GameXXK/Private/GameXXKMVPPlayerController.cpp`: own/create the inventory window, route `I`, `Esc`, and merchant open into the new widget; lock input in merchant mode.
- Modify `Source/GameXXK/Private/Tests/GameXXKMVPUIWidgetTest.cpp`: widget-level tests for frame, close, selection, equip, confirmation dialog, buy/sell.
- Modify `Source/GameXXK/Private/Tests/GameXXKPlayerFlowWidgetTest.cpp`: player-controller integration tests for `I`, `Esc`, and modal lock if existing harness has suitable hooks.
- Modify `docs/production/2026-07-03-playable-entry-flow/05-implementation-log.md` and `06-test-results.md`: record implementation and validation evidence.

---

### Task 1: Add Manifest Files And Validator

**Files:**
- Create: `docs/ui/inventory/manifests/inventory_window_frame.json`
- Create: `docs/ui/inventory/manifests/inventory_confirmation_dialog.json`
- Create: `docs/ui/inventory/manifests/inventory_close_button.json`
- Create: `docs/ui/inventory/manifests/inventory_equipment_slots.json`
- Create: `scripts/validate_inventory_ui_manifests.py`

- [x] Write manifest JSON files with `id`, `widget`, `assetType`, `targetTexture`, `canvasSize`, `states`, and component-specific layout fields.
- [x] Write validator that loads every JSON file under `docs/ui/inventory/manifests`, verifies required fields, verifies `canvasSize` is two positive integers, verifies `states` is non-empty, and verifies all `targetTexture` values start with `/Game/GameXXK/UI/Inventory/Textures/`.
- [x] Run `python scripts\validate_inventory_ui_manifests.py`.
- [x] Expected GREEN: JSON report with `ok: true`.

### Task 2: Add RED Widget Tests For Independent Window

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKMVPUIWidgetTest.cpp`
- Create: `Source/GameXXK/Public/UI/GameXXKInventoryWindowWidget.h`

- [x] Add includes and tests expecting `UGameXXKInventoryWindowWidget` to exist.
- [x] Test free inventory open: `OpenFreeInventoryForTest()` returns true, `GetWindowModeForTest()` is `FreeInventory`, `HasWindowFrameForTest()` true, `HasCloseButtonForTest()` true, and `IsModalInputLockActiveForTest()` false.
- [x] Test merchant trade open: `OpenMerchantTradeForTest()` returns true, mode is `MerchantTrade`, frame and close button exist, modal lock is true.
- [x] Test bought equipment visible UI path: buy equipment, open free inventory, select backpack item, call `ExecuteSelectedPrimaryActionForTest()`, and assert the matching equipment slot changes.
- [x] Test confirmation dialog: select shop slot 0 in merchant mode, call `RequestSelectedBuyForTest()`, assert dialog visible with confirm/cancel, cancel keeps gold/quantity unchanged, confirm changes gold/quantity.
- [x] Run `python scripts\ue_tdd_pipeline.py --pie-duration 0 --log-lines 80`.
- [x] Expected RED: compile failure for missing `UGameXXKInventoryWindowWidget` or missing methods.

### Task 3: Implement Minimal Independent Inventory Window Widget

**Files:**
- Create: `Source/GameXXK/Public/UI/GameXXKInventoryWindowWidget.h`
- Create: `Source/GameXXK/Private/UI/GameXXKInventoryWindowWidget.cpp`
- Modify: `Source/GameXXK/GameXXK.Build.cs` only if module dependencies are missing.

- [x] Define `EGameXXKInventoryWindowMode` and confirmation/selection state needed for the first independent window pass.
- [x] Implement a programmatic widget root with a full-window frame/backplate, header, title, gold text, top-right `X`, and hidden confirmation dialog layer.
- [x] Implement free mode and merchant mode open/close methods.
- [x] Implement backpack selection from `FGameXXKRuntimeState::Inventory`.
- [x] Implement merchant stock selection from `UGameXXKMVPRules::GetShopItemIds()`.
- [x] Implement equipment slot readback from `EquippedWeapon`, `EquippedArmor`, and `EquippedAccessory`.
- [x] Implement selection and detail primary action: consumable uses item, equipment equips item.
- [x] Implement buy/sell request opening the confirmation dialog; confirm executes `BuyItem` or `SellItem`; cancel clears pending state.
- [x] Run `python scripts\ue_tdd_pipeline.py --pie-duration 0 --log-lines 100`.
- [x] Expected GREEN: new widget tests compile and pass through automation or at least UBT/PIE smoke, depending on pipeline mode.

### Task 4: Wire Player Controller And Merchant Interaction

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKMVPPlayerController.h`
- Modify: `Source/GameXXK/Private/GameXXKMVPPlayerController.cpp`
- Modify: town interaction code only if merchant `F` currently routes only to `OpenTrade`.

- [x] Player controller creates and owns `UGameXXKInventoryWindowWidget`.
- [x] `I` toggles free inventory mode.
- [x] `Esc` closes the inventory window before preserving existing battle targeting behavior.
- [x] Merchant `F` opens merchant trade mode and activates modal input lock for real player-controller interactions.
- [x] Movement input checks the inventory window modal lock and ignores WASD/axis movement only in merchant trade mode.
- [x] Run `python scripts\ue_tdd_pipeline.py --pie-duration 0 --log-lines 100`.
- [x] Expected GREEN.

### Task 5: Verify Real MVP Flow And Document

**Files:**
- Modify: `docs/production/2026-07-03-playable-entry-flow/05-implementation-log.md`
- Modify: `docs/production/2026-07-03-playable-entry-flow/06-test-results.md`

- [x] Run `python scripts\validate_inventory_ui_manifests.py`.
- [x] Run `python scripts\gamexxk_mvp_playtest.py --test-timeout 600 --report Saved\HarnessReports\independent-inventory-window-green.json`.
- [x] Run `python scripts\harness_state_validator.py --require-units --json`.
- [x] Run `git diff --check`.
- [x] If the editor is running, save dirty packages through `scripts/ue_mcp_client.py`.
- [x] Append implementation and test evidence to production docs.
- [x] Commit and push.
