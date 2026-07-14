# Town HUD Reference Refresh Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the town-facing debug-style HUD, task offer, inventory, merchant, and legacy character panel presentation with reusable crops from the user-owned Tencent Docs reference while preserving existing MVP gameplay behavior.

**Architecture:** Keep `UGameXXKMVPSubsystem` and `AGameXXKMVPPlayerController` as the sole gameplay and input authorities. `UGameXXKTownOverlayWidget` becomes the persistent visual HUD and legacy character shell; `UGameXXKInventoryWindowWidget` keeps its independent inventory/merchant modes but consumes the same paper assets. The player controller owns a visible `UGameXXKQuestDialogWidget`; task NPCs request that dialogue instead of immediately calling `AcceptQuest`, and the acceptance callback completes the already-existing follower/location work.

**Tech Stack:** UE 5.8 C++ UMG, UE Editor Python via `scripts/ue_mcp_client.py`, Python/Pillow for deterministic crop exports, Unreal Automation Tests, UBT through `scripts/ue_tdd_pipeline.py`.

---

## Locked Scope

- Runtime art is cropped only from the Tencent Docs `霞客行 美术参考` image supplied by the user. Do not generate replacement UI art.
- The lower `江湖行` four-card board is not part of this batch.
- This batch covers town HUD, task-offer dialogue, character panel, free inventory, and merchant trade presentation.
- Main menu, region-map page, north-gate route map, battle UI, mail, new currencies, and new quest reward rules are excluded.
- The reference shows several resource icons, but the current runtime has `PlayerGold` and no `Material` item kind. Ship only the live gold atom. Do not display invented resource values or decorative clickable icons.
- Do not use UnrealBridge, Live Coding, Hot Reload, a worktree, or destructive Git commands. Work on `main`; stage only files listed by each task.

## File Map

| File | Responsibility |
|---|---|
| `docs/ui/v1/source_art/qqdoc-2026-07-14/reference-ui-composite.png` | Untouched high-resolution capture of the Tencent Docs reference. |
| `docs/ui/v1/source_art/qqdoc-2026-07-14/crop-manifest.json` | Source image size, crop rectangles, runtime ids, logical sizes, and alpha handling. |
| `docs/ui/v1/source_art/qqdoc-2026-07-14/crops/*.png` | Reusable cropped source atoms; never manually overwritten after export. |
| `scripts/crop_town_hud_reference.py` | Validates the manifest and exports deterministic PNG crops. |
| `scripts/test_town_hud_reference_crops.py` | Validates source/crop existence, dimensions, manifest coverage, and alpha/RGBA output. |
| `Content/Python/gamexxk_import_town_hud_reference_assets.py` | Imports approved crop PNGs into `/Game/GameXXK/UI/V1/Town/Textures`. |
| `Source/GameXXK/Public/UI/GameXXKTownOverlayWidget.h` | HUD visual fields, HUD test accessors, and navigation callbacks. |
| `Source/GameXXK/Private/UI/GameXXKTownOverlayWidget.cpp` | Persistent HUD layout, live gold/quest display, character shell, and paper navigation. |
| `Source/GameXXK/Public/UI/GameXXKQuestDialogWidget.h` | Quest-dialogue lifecycle and test-facing state. |
| `Source/GameXXK/Private/UI/GameXXKQuestDialogWidget.cpp` | Paper task-offer layout, accept/leave actions, and modal state. |
| `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h` | Quest-dialogue ownership and open/close/accept methods. |
| `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp` | Creates the dialogue widget, controls modal input/focus, and refreshes HUD after acceptance. |
| `Source/GameXXK/Public/Town/GameXXKTownNpcActor.h` | Public deferred quest-accept completion method for proxy actor NPCs. |
| `Source/GameXXK/Private/Town/GameXXKTownNpcActor.cpp` | Sends quest `F` to the dialogue and completes follower/location work after confirmation. |
| `Source/GameXXK/Public/Town/GameXXKTownNpcCharacter.h` | Public deferred quest-accept completion method for direct character NPCs. |
| `Source/GameXXK/Private/Town/GameXXKTownNpcCharacter.cpp` | Sends quest `F` to the dialogue and completes follower/location work after confirmation. |
| `Source/GameXXK/Private/UI/GameXXKInventoryWindowWidget.cpp` | Repoints the existing inventory and merchant frame, tabs, slots, and action buttons to the new crop assets. |
| `Source/GameXXK/Private/Tests/GameXXKPlayerFlowWidgetTest.cpp` | Player-controller ownership, modal input, inventory, merchant, and dialogue integration tests. |
| `Source/GameXXK/Private/Tests/GameXXKMVPUIWidgetTest.cpp` | Paper dialogue accept/leave and reskinned inventory behavior tests. |
| `Source/GameXXK/Private/Tests/GameXXKTownShellTest.cpp` | Updates direct quest-NPC acceptance coverage to use deferred confirmation. |

## Runtime Asset Names

All imports target `/Game/GameXXK/UI/V1/Town/Textures` and retain `TC_EditorIcon`, `TMGS_NoMipmaps`, `sRGB=true`, and `never_stream=true` like the existing inventory importer.

| Crop filename | Texture asset | Usage |
|---|---|---|
| `hud_resource_strip.png` | `T_TownHudResourceStrip` | Gold strip backing. |
| `hud_gold_icon.png` | `T_TownHudGoldIcon` | Gold icon; count remains live text. |
| `hud_player_card.png` | `T_TownHudPlayerCard` | Player-status backing. |
| `hud_nav_rail.png` | `T_TownHudNavRail` | Character/inventory rail backing. |
| `hud_nav_character.png` | `T_TownHudNavCharacter` | Character action visual. |
| `hud_nav_inventory.png` | `T_TownHudNavInventory` | Inventory action visual. |
| `hud_quest_tracker.png` | `T_TownHudQuestTracker` | Current objective backing. |
| `interaction_prompt.png` | `T_TownInteractionPrompt` | Contextual F-prompt backing. |
| `quest_dialog_frame.png` | `T_TownQuestDialogFrame` | Central task-offer frame. |
| `paper_button_normal.png` | `T_TownPaperButtonNormal` | Normal paper action button. |
| `paper_button_hover.png` | `T_TownPaperButtonHover` | Hover state. |
| `paper_button_pressed.png` | `T_TownPaperButtonPressed` | Pressed state. |
| `paper_button_disabled.png` | `T_TownPaperButtonDisabled` | Disabled state. |
| `window_frame.png` | `T_TownWindowFrame` | Inventory/merchant/character shell. |
| `window_tab.png` | `T_TownWindowTab` | Inventory/character tab backing. |
| `item_slot.png` | `T_TownItemSlot` | Inventory/equipment slot backing. |
| `modal_confirm_frame.png` | `T_TownModalConfirmFrame` | Merchant confirmation backing. |

### Task 1: Capture, crop, and validate the reference atoms

**Files:**
- Create: `docs/ui/v1/source_art/qqdoc-2026-07-14/reference-ui-composite.png`
- Create: `docs/ui/v1/source_art/qqdoc-2026-07-14/crop-manifest.json`
- Create: `docs/ui/v1/source_art/qqdoc-2026-07-14/crops/*.png`
- Create: `scripts/crop_town_hud_reference.py`
- Create: `scripts/test_town_hud_reference_crops.py`

- [ ] **Step 1: Export the document image at a fixed source resolution.**

  In the signed-in Tencent Docs browser page, set browser zoom so the complete upper UI reference is crisp, capture only the reference artwork without browser chrome, and save that exact source at `docs/ui/v1/source_art/qqdoc-2026-07-14/reference-ui-composite.png`. Do not retouch it. Record its exact pixel width and height in the manifest before adding crop rectangles.

- [ ] **Step 2: Write the crop validation test first.**

  Create `scripts/test_town_hud_reference_crops.py` with this required contract:

  ```python
  REQUIRED_IDS = {
      "hud_resource_strip", "hud_gold_icon", "hud_player_card", "hud_nav_rail",
      "hud_nav_character", "hud_nav_inventory", "hud_quest_tracker", "interaction_prompt",
      "quest_dialog_frame", "paper_button_normal", "paper_button_hover",
      "paper_button_pressed", "paper_button_disabled", "window_frame", "window_tab",
      "item_slot", "modal_confirm_frame",
  }

  def test_manifest_exports_all_runtime_atoms() -> None:
      manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
      assert manifest["source"] == "reference-ui-composite.png"
      assert {entry["id"] for entry in manifest["crops"]} == REQUIRED_IDS
      for entry in manifest["crops"]:
          output = CROP_DIR / entry["file"]
          with Image.open(output) as image:
              assert image.mode == "RGBA"
              assert image.width == entry["output_size"][0]
              assert image.height == entry["output_size"][1]
  ```

- [ ] **Step 3: Run the crop test before the exporter and crop files exist.**

  Run: `python scripts/test_town_hud_reference_crops.py`

  Expected: failure identifying the missing manifest or required crop output.

- [ ] **Step 4: Implement deterministic crop export.**

  Create a JSON manifest whose `crops` entries each contain `id`, `file`, `rect`, `output_size`, and `trim_alpha`. Crop rectangles are measured from the recorded, untouched source image. Implement this core in `scripts/crop_town_hud_reference.py`:

  ```python
  with Image.open(source_path).convert("RGBA") as source:
      for spec in manifest["crops"]:
          left, top, right, bottom = spec["rect"]
          image = source.crop((left, top, right, bottom))
          if spec["trim_alpha"]:
              alpha_bounds = image.getchannel("A").getbbox()
              if alpha_bounds:
                  image = image.crop(alpha_bounds)
          image = image.resize(tuple(spec["output_size"]), Image.Resampling.LANCZOS)
          image.save(crop_dir / spec["file"], "PNG")
  ```

  Crop visual backing, icons, frames, tabs, and slots tightly. Do not crop live values, live task body text, live prices, or live item counts into a texture.

- [ ] **Step 5: Run the exporter and crop test.**

  Run: `python scripts/crop_town_hud_reference.py`

  Run: `python scripts/test_town_hud_reference_crops.py`

  Expected: the exporter reports all 17 files; the test exits `0` and confirms all outputs are RGBA at their manifest sizes.

- [ ] **Step 6: Commit the source-art pipeline only.**

  ```powershell
  git add -- docs/ui/v1/source_art/qqdoc-2026-07-14 scripts/crop_town_hud_reference.py scripts/test_town_hud_reference_crops.py
  git commit -m "assets: crop town HUD reference atoms"
  ```

### Task 2: Import the approved crop assets through UE MCP

**Files:**
- Create: `Content/Python/gamexxk_import_town_hud_reference_assets.py`
- Test: `scripts/test_town_hud_reference_crops.py`

- [ ] **Step 1: Extend the crop test with an import-list assertion.**

  Add an assertion that the Python importer declares exactly the 17 `(source crop, UE texture name)` pairs from the Runtime Asset Names table. This prevents a crop from being prepared but never imported.

- [ ] **Step 2: Run the extended test and verify it fails before the UE importer exists.**

  Run: `python scripts/test_town_hud_reference_crops.py`

  Expected: failure that `Content/Python/gamexxk_import_town_hud_reference_assets.py` is missing.

- [ ] **Step 3: Implement the UE importer using the established inventory-import pattern.**

  Use `Content/Python/gamexxk_import_inventory_ui_assets.py` as the behavior reference. Set:

  ```python
  SOURCE_DIR = PROJECT_ROOT / "docs" / "ui" / "v1" / "source_art" / "qqdoc-2026-07-14" / "crops"
  DESTINATION = "/Game/GameXXK/UI/V1/Town/Textures"

  IMPORTS = [
      (SOURCE_DIR / "hud_resource_strip.png", "T_TownHudResourceStrip"),
      (SOURCE_DIR / "hud_gold_icon.png", "T_TownHudGoldIcon"),
      # include every remaining Runtime Asset Names row exactly once
  ]
  ```

  For every imported texture, set no mipmaps, editor-icon compression, sRGB, and never-stream; then save the destination directory recursively.

- [ ] **Step 4: Run the crop/import-list test.**

  Run: `python scripts/test_town_hud_reference_crops.py`

  Expected: exit `0`.

- [ ] **Step 5: Import in the running UE editor through MCP and save.**

  Run:

  ```powershell
  python -c "from scripts.ue_mcp_client import UnrealMCPClient; c=UnrealMCPClient(); c.require_connected(); print(c.run_project_python_file('Content/Python/gamexxk_import_town_hud_reference_assets.py')); print(c.save_dirty_packages())"
  ```

  Expected: `imported_count` is `17`, every returned asset path begins `/Game/GameXXK/UI/V1/Town/Textures/`, and dirty packages are saved.

- [ ] **Step 6: Commit the UE import script and crop metadata only.**

  ```powershell
  git add -- Content/Python/gamexxk_import_town_hud_reference_assets.py docs/ui/v1/source_art/qqdoc-2026-07-14/crop-manifest.json
  git commit -m "assets: import town HUD reference textures"
  ```

### Task 3: Write failing HUD and inventory visual-contract tests

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKPlayerFlowWidgetTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKMVPUIWidgetTest.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKTownOverlayWidget.h`

- [ ] **Step 1: Add test-only HUD observability declarations.**

  Add these methods to `UGameXXKTownOverlayWidget`:

  ```cpp
  UFUNCTION(BlueprintPure, Category = "GameXXK|Town|Test")
  int32 GetDisplayedGoldForTest() const;

  UFUNCTION(BlueprintPure, Category = "GameXXK|Town|Test")
  FString GetTownHudGoldIconPathForTest() const;

  UFUNCTION(BlueprintPure, Category = "GameXXK|Town|Test")
  bool HasTownHudInventoryActionForTest() const;
  ```

- [ ] **Step 2: Add failing assertions for the visual contract.**

  In `FGameXXKTownOverlayCommandsTest`, after town setup, assert:

  ```cpp
  TestEqual(TEXT("town HUD displays runtime gold"), TownOverlay->GetDisplayedGoldForTest(), Subsystem->GetRuntimeState().PlayerGold);
  TestEqual(TEXT("town HUD loads the cropped gold icon"), TownOverlay->GetTownHudGoldIconPathForTest(), FString(TEXT("/Game/GameXXK/UI/V1/Town/Textures/T_TownHudGoldIcon.T_TownHudGoldIcon")));
  TestTrue(TEXT("town HUD exposes a real inventory action"), TownOverlay->HasTownHudInventoryActionForTest());
  ```

  In `FGameXXKMVPUIWidgetTest`, assert the inventory window reports the new frame texture path:

  ```cpp
  TestEqual(TEXT("inventory uses town paper window frame"), InventoryWindow->GetWindowFrameResourcePathForTest(), FString(TEXT("/Game/GameXXK/UI/V1/Town/Textures/T_TownWindowFrame.T_TownWindowFrame")));
  ```

- [ ] **Step 3: Compile and run the focused automation tests to prove red.**

  Run a cold compile followed by the focused filters using the project TDD pipeline; do not use Live Coding or Hot Reload. The tests must fail only because the new HUD accessors/asset path contract are absent.

- [ ] **Step 4: Keep the red tests uncommitted on `main` and proceed directly to Task 4.**

  The repository's canonical workflow is `main`; do not create a broken intermediate commit. Task 4 makes these tests green and commits the complete HUD slice together.

### Task 4: Rebuild the persistent town HUD with the cropped paper atoms

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKTownOverlayWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKTownOverlayWidget.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKPlayerFlowWidgetTest.cpp`

- [ ] **Step 1: Implement texture/brush helpers and new HUD fields.**

  In the anonymous namespace of `GameXXKTownOverlayWidget.cpp`, add the fixed root and paths:

  ```cpp
  const FString TownUiTextureRoot(TEXT("/Game/GameXXK/UI/V1/Town/Textures/"));
  const FString TownHudResourceStripPath(TownUiTextureRoot + TEXT("T_TownHudResourceStrip.T_TownHudResourceStrip"));
  const FString TownHudGoldIconPath(TownUiTextureRoot + TEXT("T_TownHudGoldIcon.T_TownHudGoldIcon"));
  const FString TownHudPlayerCardPath(TownUiTextureRoot + TEXT("T_TownHudPlayerCard.T_TownHudPlayerCard"));
  const FString TownHudNavRailPath(TownUiTextureRoot + TEXT("T_TownHudNavRail.T_TownHudNavRail"));
  const FString TownHudQuestTrackerPath(TownUiTextureRoot + TEXT("T_TownHudQuestTracker.T_TownHudQuestTracker"));
  ```

  Add transient fields for the resource-strip border, gold icon image, gold text, player card, quest tracker/title text, inventory navigation button, and character navigation button. Keep `CachedStatusText` and legacy command test APIs so existing tests remain valid, but stop rendering the full debug status text to the player.

- [ ] **Step 2: Replace the top-left debug stack with a 1920x1080 anchored HUD.**

  In `BuildProgrammaticLayout`, build this order on `RootCanvas`:

  ```text
  Top-left    T_TownHudResourceStrip + T_TownHudGoldIcon + live "{PlayerGold}"
  Bottom-right T_TownHudPlayerCard + live player level/HP summary
  Right side  T_TownHudNavRail + separate character and inventory UButtons
  Top-centre  T_TownHudQuestTracker + live current objective
  World       existing interaction prompt remains contextual and outside the permanent menu
  ```

  Set all visual `UImage` widgets to `HitTestInvisible`. The two visible rail buttons use separate `UButton` hit targets, with only `Normal`, `Hovered`, `Pressed`, and `Disabled` brushes sourced from the cropped paper button state assets.

- [ ] **Step 3: Route the inventory rail action through the actual player-facing inventory window.**

  Replace `HandleInventoryClicked` with:

  ```cpp
  void UGameXXKTownOverlayWidget::HandleInventoryClicked()
  {
      if (AGameXXKMVPPlayerController* Controller = Cast<AGameXXKMVPPlayerController>(GetOwningPlayer()))
      {
          Controller->OpenFreeInventoryWindow();
      }
  }
  ```

  Include `MVP/GameXXKMVPPlayerController.h` in this `.cpp`. Keep `HandleCharacterClicked` on the existing `OpenCharacterPanel` command so it opens the existing character mode in the reskinned shell. Do not add map, mail, or system icons in this task because those views are not yet player-facing.

- [ ] **Step 4: Populate only live, existing data in `ConfigureProgrammaticLayout`.**

  Use the runtime state exactly as follows:

  ```cpp
  GoldTextBlock->SetText(FText::AsNumber(State->PlayerGold));
  QuestTrackerTextBlock->SetText(
      State->QuestState == EGameXXKQuestState::Accepted
          ? NSLOCTEXT("GameXXKTownOverlay", "QuestToNorthGate", "前往北门")
          : NSLOCTEXT("GameXXKTownOverlay", "QuestFindGuide", "寻找引路人"));
  ```

  Do not render material icons until `EGameXXKItemKind` gains a real `Material` state and a runtime count source.

- [ ] **Step 5: Compile and run the focused HUD tests to prove green.**

  Expected: the HUD reports runtime gold, uses the new crop asset path, keeps the legacy route-button prohibition, and the rail inventory action opens the independent inventory window.

- [ ] **Step 6: Commit the HUD-only change.**

  ```powershell
  git add -- Source/GameXXK/Public/UI/GameXXKTownOverlayWidget.h Source/GameXXK/Private/UI/GameXXKTownOverlayWidget.cpp Source/GameXXK/Private/Tests/GameXXKPlayerFlowWidgetTest.cpp
  git commit -m "feat: reskin persistent town HUD"
  ```

### Task 5: Mount a paper task dialogue before quest acceptance

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKQuestDialogWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKQuestDialogWidget.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- Modify: `Source/GameXXK/Public/Town/GameXXKTownNpcActor.h`
- Modify: `Source/GameXXK/Private/Town/GameXXKTownNpcActor.cpp`
- Modify: `Source/GameXXK/Public/Town/GameXXKTownNpcCharacter.h`
- Modify: `Source/GameXXK/Private/Town/GameXXKTownNpcCharacter.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKPlayerFlowWidgetTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKMVPUIWidgetTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKTownShellTest.cpp`

- [ ] **Step 1: Add the failing player-flow tests.**

  Add controller test accessors `GetQuestDialogWidgetForTest()` and `IsQuestDialogModalInputLockedForTest()`. Extend `FGameXXKPlayerControllerOwnsFlowWidgetsTest` to assert:

  ```cpp
  TestNotNull(TEXT("player controller owns quest dialogue"), PlayerController->GetQuestDialogWidgetForTest());
  TestTrue(TEXT("quest F opens dialogue without accepting"), PlayerController->OpenQuestDialogueForTest(QuestNpc, HeroPawn));
  TestEqual(TEXT("opening dialogue preserves unaccepted quest"), Subsystem->GetRuntimeState().QuestState, EGameXXKQuestState::NotAccepted);
  TestTrue(TEXT("quest dialogue locks town input"), PlayerController->IsQuestDialogModalInputLockedForTest());
  TestTrue(TEXT("dialogue leave closes without mutation"), PlayerController->GetQuestDialogWidgetForTest()->LeaveQuestOfferForTest());
  ```

  Add the corresponding accept assertion only after reopening the offer:

  ```cpp
  TestTrue(TEXT("dialogue accept completes quest offer"), PlayerController->GetQuestDialogWidgetForTest()->AcceptQuest());
  TestEqual(TEXT("dialogue accept marks quest accepted"), Subsystem->GetRuntimeState().QuestState, EGameXXKQuestState::Accepted);
  ```

- [ ] **Step 2: Compile and run the focused tests to prove red.**

  Expected: compilation or tests fail because the controller-owned dialogue lifecycle does not exist and NPC `F` still accepts directly.

- [ ] **Step 3: Implement a self-contained modal quest widget.**

  Expand `UGameXXKQuestDialogWidget` with `OpenQuestOffer`, `LeaveQuestOffer`, `IsQuestOfferVisibleForTest`, and a dynamically bound `OnQuestOfferAccepted` delegate. Build the root programmatically using `T_TownQuestDialogFrame` and the four paper button state textures. The two button handlers are:

  ```cpp
  void UGameXXKQuestDialogWidget::HandleAcceptClicked()
  {
      if (AcceptQuest())
      {
          OnQuestOfferAccepted.Broadcast();
          CloseQuestOffer();
      }
  }

  void UGameXXKQuestDialogWidget::HandleLeaveClicked()
  {
      CloseQuestOffer();
  }
  ```

  `AcceptQuest()` remains the only call that invokes `UGameXXKMVPSubsystem::AcceptQuest()`.

- [ ] **Step 4: Make the player controller own and focus the dialogue.**

  Add `QuestDialogWidgetClass` and `QuestDialogWidget` properties alongside the existing inventory widget. Add `OpenQuestDialogueForNpc(AGameXXKTownNpcActor*, APawn*)` and `OpenQuestDialogueForNpcCharacter(AGameXXKTownNpcCharacter*, APawn*)`; both bind the dialog completion to the correct source NPC and use a shared private `ApplyQuestDialogueInputMode(bool bOpen)`.

  The open branch must use `FInputModeUIOnly`, set the widget focus, show the cursor, and call `FlushPressedKeys()`. The close branch must call `ApplyPlayerFlowInputMode()` so the town returns to game-viewport focus. Register the widget in `EnsurePlayerFlowWidgets` at viewport Z-order `130`.

- [ ] **Step 5: Defer NPC quest completion until the dialog confirms.**

  Add this public method to both NPC classes:

  ```cpp
  bool CompleteQuestOffer(APawn* InstigatorPawn);
  ```

  Move the existing `Subsystem->AcceptQuest()`, `ActivateFollower`, and `RecordQuestNpcLocation` block from `ApplyDefaultInteraction` into this method. In the quest branch of `ApplyDefaultInteraction`, call the appropriate player-controller `OpenQuestDialogueForNpc...` method instead. Merchant behavior remains unchanged.

- [ ] **Step 6: Compile and run focused tests to prove green.**

  Verify that opening the dialogue causes no quest/follower mutation, Leave causes no mutation, Accept applies the existing follower/location behavior once, and closing restores town input. Update the direct-NPC save test in `GameXXKTownShellTest.cpp` to invoke the deferred confirmation instead of expecting immediate acceptance from `ApplyDefaultInteraction`.

- [ ] **Step 7: Commit the dialogue interaction slice only after its tests are green.**

  ```powershell
  git add -- Source/GameXXK/Public/UI/GameXXKQuestDialogWidget.h Source/GameXXK/Private/UI/GameXXKQuestDialogWidget.cpp Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp Source/GameXXK/Public/Town/GameXXKTownNpcActor.h Source/GameXXK/Private/Town/GameXXKTownNpcActor.cpp Source/GameXXK/Public/Town/GameXXKTownNpcCharacter.h Source/GameXXK/Private/Town/GameXXKTownNpcCharacter.cpp Source/GameXXK/Private/Tests/GameXXKPlayerFlowWidgetTest.cpp Source/GameXXK/Private/Tests/GameXXKMVPUIWidgetTest.cpp Source/GameXXK/Private/Tests/GameXXKTownShellTest.cpp
  git commit -m "feat: show paper quest dialogue before acceptance"
  ```

### Task 6: Re-skin inventory, merchant, and character shell without changing their rules

**Files:**
- Modify: `Source/GameXXK/Private/UI/GameXXKInventoryWindowWidget.cpp`
- Modify: `Source/GameXXK/Private/UI/GameXXKTownOverlayWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKMVPUIWidgetTest.cpp`

- [ ] **Step 1: Add failing asset-path tests for the three reused window atoms.**

  Add `GetWindowFrameResourcePathForTest`, `GetItemSlotResourcePathForTest`, and `GetActionButtonNormalResourcePathForTest` to `UGameXXKInventoryWindowWidget`. Assert:

  ```cpp
  TestEqual(TEXT("paper inventory frame path"), InventoryWindow->GetWindowFrameResourcePathForTest(), FString(TEXT("/Game/GameXXK/UI/V1/Town/Textures/T_TownWindowFrame.T_TownWindowFrame")));
  TestEqual(TEXT("paper item slot path"), InventoryWindow->GetItemSlotResourcePathForTest(), FString(TEXT("/Game/GameXXK/UI/V1/Town/Textures/T_TownItemSlot.T_TownItemSlot")));
  TestEqual(TEXT("paper action normal path"), InventoryWindow->GetActionButtonNormalResourcePathForTest(), FString(TEXT("/Game/GameXXK/UI/V1/Town/Textures/T_TownPaperButtonNormal.T_TownPaperButtonNormal")));
  ```

- [ ] **Step 2: Run focused tests to prove red.**

  Expected: failures because inventory still points at `/Game/GameXXK/UI/Inventory/Textures/` and lacks the accessors.

- [ ] **Step 3: Repoint the inventory texture constants and preserve all live content.**

  Replace the texture root/constants at the top of `GameXXKInventoryWindowWidget.cpp` with:

  ```cpp
  const FString TextureRoot(TEXT("/Game/GameXXK/UI/V1/Town/Textures/"));
  const FString WindowFrameTexturePath(TextureRoot + TEXT("T_TownWindowFrame.T_TownWindowFrame"));
  const FString ConfirmationDialogTexturePath(TextureRoot + TEXT("T_TownModalConfirmFrame.T_TownModalConfirmFrame"));
  const FString BackpackSlotTexturePath(TextureRoot + TEXT("T_TownItemSlot.T_TownItemSlot"));
  const FString ActionButtonNormalTexturePath(TextureRoot + TEXT("T_TownPaperButtonNormal.T_TownPaperButtonNormal"));
  const FString ActionButtonHoverTexturePath(TextureRoot + TEXT("T_TownPaperButtonHover.T_TownPaperButtonHover"));
  const FString ActionButtonPressedTexturePath(TextureRoot + TEXT("T_TownPaperButtonPressed.T_TownPaperButtonPressed"));
  const FString ActionButtonDisabledTexturePath(TextureRoot + TEXT("T_TownPaperButtonDisabled.T_TownPaperButtonDisabled"));
  ```

  Change `MakeTextureButtonStyle` to accept the four state paths and use each in its matching `FButtonStyle` brush. Keep all existing slot selection, buying, selling, equipping, use-item, confirmation, and modal-lock methods unchanged.

- [ ] **Step 4: Use the cropped tab atom in both inventory modes.**

  Add a `UImage` using `T_TownWindowTab` behind `TitleTextBlock` in `BuildProgrammaticLayout`; keep `TitleTextBlock` live so the same tab reads `行囊` in free mode and `商铺` in merchant mode. The tab image is `HitTestInvisible` and has no independent click behavior.

- [ ] **Step 5: Apply the shared frame to the legacy character panel inside `TownOverlay`.**

  Set `ActivePanelBackplate` to `T_TownWindowFrame` and `InventoryGrid`/equipment slot visuals to `T_TownItemSlot` when `TownPanelMode` is `Character`. Do not create a second character state model or a second inventory list.

- [ ] **Step 6: Run focused inventory and player-flow tests to prove green.**

  Expected: free inventory still opens via `I`, merchant F still toggles independent trade mode, existing buy/sell/equip/use tests pass, and all three new resource-path assertions pass.

- [ ] **Step 7: Commit the window presentation slice.**

  ```powershell
  git add -- Source/GameXXK/Private/UI/GameXXKInventoryWindowWidget.cpp Source/GameXXK/Private/UI/GameXXKTownOverlayWidget.cpp Source/GameXXK/Private/Tests/GameXXKMVPUIWidgetTest.cpp
  git commit -m "feat: apply paper style to inventory and merchant"
  ```

### Task 7: Compile, verify in PIE, and record visual evidence

**Files:**
- Create: `docs/production/evidence/town-hud-reference-refresh/pie-town-hud.png`
- Create: `docs/production/evidence/town-hud-reference-refresh/pie-quest-dialog.png`
- Create: `docs/production/evidence/town-hud-reference-refresh/pie-inventory.png`
- Create: `docs/production/evidence/town-hud-reference-refresh/pie-merchant.png`
- Create: `docs/production/evidence/town-hud-reference-refresh/verification.json`

- [ ] **Step 1: Save dirty packages through UE MCP before a cold compile.**

  Run:

  ```powershell
  python -c "from scripts.ue_mcp_client import UnrealMCPClient; c=UnrealMCPClient(); c.require_connected(); print(c.save_dirty_packages())"
  ```

  Expected: `save_result` is true and `dirty_after` is false. If UE MCP is unavailable while the editor may contain unsaved work, stop and ask the user before closing it.

- [ ] **Step 2: Run cold C++ verification.**

  Run: `python scripts/ue_tdd_pipeline.py --pie-duration 10`

  Expected: UBT exits `0`; PIE starts and stays alive; no relevant compilation or runtime assertion failure is reported. Do not treat `--check-only`, Live Coding, or Hot Reload as compile proof.

- [ ] **Step 3: Run the crop validator after the build.**

  Run: `python scripts/test_town_hud_reference_crops.py`

  Expected: exit `0`.

- [ ] **Step 4: Perform the real playable pass in PIE.**

  Record at 1920x1080:

  ```text
  Town loads with paper resource strip, live gold, player card, quest tracker, and only functional rail icons.
  I opens the reskinned free inventory; Esc returns control to the town.
  Merchant F opens the reskinned merchant mode; its buy confirmation uses the paper modal.
  Quest NPC F opens the task dialogue without accepting.
  Leave returns to town with quest unchanged.
  Reopen then Accept; follower activates, HUD objective changes, and north-gate behavior remains quest-gated.
  ```

  Save the four screenshots and a JSON report containing the build command, crop-test result, PIE result, screenshots, and the observed input transitions.

- [ ] **Step 5: Check the exact diff and commit evidence only after all verification passes.**

  ```powershell
  git diff --check
  git add -- docs/production/evidence/town-hud-reference-refresh
  git commit -m "test: verify town HUD reference refresh"
  ```

## Plan Self-Review

| Spec requirement | Plan task |
|---|---|
| Tencent Docs crops only; no generated art | Tasks 1-2 |
| Persistent paper town HUD with real resource data | Tasks 3-4 |
| No invented currencies or non-functional icons | Locked Scope and Task 4 |
| Task NPC F opens accept/leave dialogue | Task 5 |
| Preserve follower/location behavior after accept | Task 5 |
| Free inventory and merchant reuse existing gameplay rules | Task 6 |
| Existing input and modal behavior remain usable | Tasks 4-5 and Task 7 |
| Cold compile and real PIE evidence | Task 7 |

Placeholder scan: no unresolved implementation marker is used. Type names and method names introduced by a task are defined in that task before later tasks depend on them.
