# Unified Town Inventory And Shop Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make town backpack, equipment, and merchant trade use one shared item-slot model and visual language.

**Architecture:** Keep runtime inventory authority in `UGameXXKMVPRules` and `UGameXXKMVPSubsystem`. Refactor only the town overlay UI to build player backpack slots, equipment slots, and shop stock slots from a small `FGameXXKTownItemSlotView` model. Add tests that prove shop stock is no longer text-only and that trade mode reuses the player backpack grid.

**Tech Stack:** UE 5.8 C++, UMG programmatic widgets, GameXXK automation tests, UE MCP/UBT verification.

---

### Task 1: Add Failing UI Coverage

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKTownOverlayWidget.h`
- Modify: `Source/GameXXK/Private/Tests/GameXXKMVPUIWidgetTest.cpp`

- [ ] Add test helpers on `UGameXXKTownOverlayWidget` for shop stock slot count, shop stock slot texture path, first shop stock slot id, and first player backpack slot id.
- [ ] Extend `GameXXK.MVP.UI.WidgetBasesDriveRules` to assert that trade mode creates fixed-size shop stock slots named `TownShopStockSlotSize_00`, uses `/Game/GameXXK/UI/Inventory/Textures/T_InkBackpackSlot`, exposes stock count equal to `GetShopItemIds().Num()`, and still exposes the same `TownSharedInventoryGrid`.
- [ ] Run `python scripts\ue_tdd_pipeline.py --pie-duration 0 --log-lines 60`.
- [ ] Expected RED: compile failure for missing test helper methods or test failure for missing shop stock slot grid.

### Task 2: Implement Shared Slot Model In Town Overlay

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKTownOverlayWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKTownOverlayWidget.cpp`

- [ ] Add `EGameXXKTownItemSlotSource` and `FGameXXKTownItemSlotView` near the town overlay widget declaration.
- [ ] Replace text-only shop stock with `ShopStockGrid` and fixed 72x72 buttons built by the same slot helper as player backpack slots.
- [ ] Preserve the existing player backpack grid and equipment slots, but populate them from `FGameXXKTownItemSlotView`.
- [ ] Add test helper implementations for stock count, slot texture, first shop item id, and first player item id.
- [ ] Run `python scripts\ue_tdd_pipeline.py --pie-duration 0 --log-lines 80`.
- [ ] Expected GREEN: UBT succeeds and PIE smoke starts/stops.

### Task 3: Verify MVP Flow And Document

**Files:**
- Modify: `docs/production/2026-07-03-playable-entry-flow/05-implementation-log.md`
- Modify: `docs/production/2026-07-03-playable-entry-flow/06-test-results.md`

- [ ] Run `python scripts\gamexxk_mvp_playtest.py --skip-build --test-timeout 600 --report Saved\HarnessReports\unified-town-inventory-shop-green.json`.
- [ ] Run `git diff --check`.
- [ ] Run `python scripts\harness_state_validator.py --require-units --json`.
- [ ] Save dirty packages with `scripts/ue_mcp_client.py` before final status.
- [ ] Append the implementation and verification evidence to production docs.
