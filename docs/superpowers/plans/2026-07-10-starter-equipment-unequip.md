# Starter Equipment Unequip Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** Add unequip interaction for equipment slots and seed New Game backpacks with wooden sword, cloth armor, and cloth talisman test equipment.

**Architecture:** Keep equipment state in `FGameXXKRuntimeState` as existing equipped item ids. Add starter item ids and definitions in `UGameXXKMVPRules`, expose `UnequipItem`, then let `UGameXXKInventoryWindowWidget` show `卸下` when an equipment slot is selected. Add generated item art through the existing inventory source-art/import path.

**Tech Stack:** UE 5.8 C++, UMG programmatic widgets, project Python import script, built-in image generation, Unreal Automation.

---

### Task 1: Rules And Tests

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKMVPFlowTest.cpp`
- Modify: `Source/GameXXK/Public/GameXXKMVPRules.h`
- Modify: `Source/GameXXK/Private/GameXXKMVPRules.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`

- [x] **Step 1: Write the failing test**

Add assertions that `CreateNewGame()` includes `Item.WoodenSword`, `Item.StarterClothArmor`, and `Item.ClothTalisman`, that equipping the wooden sword changes attack, and that `UnequipItem(State, Item.WoodenSword())` clears the weapon slot and restores attack.

- [x] **Step 2: Run red verification**

Run: `python scripts/gamexxk_mvp_playtest.py --skip-build --test-timeout 600 --report Saved/HarnessReports/starter-equipment-red.json`

Expected: FAIL because starter item ids and `UnequipItem` do not exist yet.

- [x] **Step 3: Implement rules**

Add three item ids, definitions, public accessors, initial inventory entries in `CreateNewGame`, and `UnequipItem` that clears the matching equipped slot and recalculates stats.

- [x] **Step 4: Run green verification**

Run: `python scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 120`, then `python scripts/gamexxk_mvp_playtest.py --skip-build --test-timeout 600 --report Saved/HarnessReports/starter-equipment-green.json`.

Expected: PASS with `failed_tests=[]`.

### Task 2: Inventory Window Unequip UI

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKInventoryWindowWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKInventoryWindowWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKMVPUIWidgetTest.cpp`

- [x] **Step 1: Write the failing widget test**

Select an equipped armor slot, assert the primary action text is `卸下`, execute it, then assert the armor slot is empty and defense returns to its previous value.

- [x] **Step 2: Implement widget path**

Allow `ExecuteSelectedPrimaryActionForTest` to handle `EGameXXKInventorySlotSource::Equipment` by calling `Subsystem->UnequipItem(SelectedItemId)`. In `RefreshDetailPanel`, set `CurrentPrimaryActionText` to `卸下` for selected equipment slots.

- [x] **Step 3: Verify**

Run the same green verification commands from Task 1.

### Task 3: Starter Item Art

**Files:**
- Create: `docs/ui/inventory/source_art/item_wooden_sword.png`
- Create: `docs/ui/inventory/source_art/item_starter_cloth_armor.png`
- Create: `docs/ui/inventory/source_art/item_cloth_talisman.png`
- Modify: `Content/Python/gamexxk_import_inventory_ui_assets.py`
- Modify: `Source/GameXXK/Private/UI/GameXXKInventoryWindowWidget.cpp`

- [x] **Step 1: Generate art**

Use built-in image generation to create three consistent water-ink item icons, save them under `docs/ui/inventory/source_art`.

- [x] **Step 2: Import textures**

Add the three source files to `IMPORTS` as `T_ItemWoodenSword`, `T_ItemStarterClothArmor`, and `T_ItemClothTalisman`. Run the import script through UE MCP/Python.

- [x] **Step 3: Wire icon paths**

Map the three new item ids to their Texture2D paths in `ResolveItemIconTexturePath`.

- [x] **Step 4: Verify manifests and automation**

Run `python scripts/validate_inventory_ui_manifests.py`, `python scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 120`, and the MVP playtest.
