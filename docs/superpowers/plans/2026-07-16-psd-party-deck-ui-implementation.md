# PSD Party and Deck UI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the flat companion codex and fixed battle command menu with functional, PSD-faithful character, companion, task, map, backpack and battle/deck interfaces that consume the approved party/deck rules without inventing generic replacement UI.

**Architecture:** The companion/route and card-runtime plans expose all mutations through `UGameXXKMVPSubsystem`; this plan only renders read models and sends validated IDs back through those APIs. Every list/grid gets a right-side paper/ink scrollbar built from audited PSD-derived components. Battle target highlighting and the mouse-following ink arrow remain UMG transient presentation; rules remain authoritative for UnitId validation, cards and resources.

**Tech Stack:** Unreal Engine 5.8 C++, programmatic UMG/Slate painting, existing `T_BattleTargetArrowHead` / `T_BattleTargetInkDab_*`, PSD cut asset import via UE Python, Unreal Automation Tests, cold UBT builds and PIE/MCP verification.

---

## Asset and widget boundary

| Area | Source / owner | Required behavior |
| --- | --- | --- |
| Companion card frame | `...\work\psd_rebuild\clean_assets_v2\057.png`–`059.png` only | Raw 452×516 first-row frame, displayed at the 113:129 ratio. Never use square `060.png`–`062.png`. |
| Scrollbars | Paper/ink strips derived from approved PSD cut components, documented in a manifest | Right edge for every scrollable companion/task/deck/backpack list; no white browser-style scrollbar. |
| Character panel | Existing `docs/ui/town/source_art/Character/` textures | Shared subject modes: hero, active permanent partner, temporary task NPC. |
| Task/map/backpack | Existing `Task`, `Backpack`, map reference cuts | Preserve panel texture identity while adding real NPC/deck/reward data. |
| Battle targeting | Existing battle arrowhead + 12 ink-dab textures | Candidate outline, source-unit anchor, mouse-following arrow and UnitId click confirmation. |

## Task 1: Audit, export and import the PSD UI contract before creating widgets

**Files:**

- Create: `scripts/verify_party_deck_psd_assets.py`
- Create: `scripts/prepare_party_deck_source_art.py`
- Create: `Content/Python/gamexxk_import_party_deck_assets.py`
- Create: `docs/ui/town/source_art/PartyDeck/manifest.json`
- Create: `docs/ui/town/source_art/PartyDeck/README.md`
- Create/import: `Content/GameXXK/UI/Town/Textures/PartyDeck/*`
- Create: `Source/GameXXK/Private/Tests/GameXXKPartyDeckAssetContractTest.cpp`

- [ ] **Step 1: Write the red asset-contract test and verifier.**

The test/verifier must fail until audited assets exist. The manifest contains source absolute path, SHA-256, raw width/height, intended role and destination asset path for each exported file. It must prove:

```text
companion_card_frame_01 source is clean_assets_v2/057.png (or 058/059)
raw size is 452 × 516
display ratio is 113 : 129
no manifest card-frame entry references 060.png, 061.png or 062.png
every declared scroll track/thumb has a PSD component source, never a generic solid-white bitmap
```

`GameXXKPartyDeckAssetContractTest.cpp` must load the imported frame texture and assert its resource path contains `/Game/GameXXK/UI/Town/Textures/PartyDeck/`, that the raw source dimensions are retained in the manifest, and that no widget test is still pointed at an old 216×238 synthetic codex frame.

- [ ] **Step 2: Implement a source-only preparation script.**

`prepare_party_deck_source_art.py` reads only the user-authorized source:

```text
C:\Users\shxuw\Downloads\nw-studio-nwueball-https-github-com\nw-studio-nwueball-https-github-com\work\psd_rebuild\clean_assets_v2
```

It copies the selected first-row frame(s) without resampling, records raw dimensions/checksums, and prepares paper/ink scrollbar components by cropping/reusing approved paper and ink regions documented in the manifest. It must not generate a replacement card frame, hand-draw a scrollbar, or silently fall back to `060`–`062`. If any source is missing or has the wrong dimensions, exit non-zero before writing destinations.

- [ ] **Step 3: Import through UE Python and verify asset paths.**

`Content/Python/gamexxk_import_party_deck_assets.py` imports only manifest-listed PNGs into `Content/GameXXK/UI/Town/Textures/PartyDeck/` with stable names such as:

```text
T_PartyDeck_CompanionCardFrame_01
T_PartyDeck_ScrollTrack_PaperInk
T_PartyDeck_ScrollThumb_Ink
```

It must not overwrite existing user-tuned character, PaperZD, level or camera assets. Run it through the project UE MCP workflow, save packages, then run the Python verifier and the automation asset-contract test. Record any missing source component as a blocking asset gap, not a made-up substitute.

- [ ] **Step 4: Run the asset gate.**

```powershell
python scripts/verify_party_deck_psd_assets.py
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.UI.PartyDeck.AssetContract;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

Expected result: first-row frame and paper/ink scrollbar contract are proven before any panel is visually replaced.

## Task 2: Replace the generic companion codex with a real three-column roster

**Files:**

- Create: `Source/GameXXK/Public/UI/GameXXKCompanionRosterWidget.h`
- Create: `Source/GameXXK/Private/UI/GameXXKCompanionRosterWidget.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKTownHudWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKTownHudWidget.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCompanionCodexWidgetTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCompanionCodexRulesTest.cpp`

- [ ] **Step 1: Write red widget tests.**

Replace old assertions for blank generic `UBorder` cards and the old 216×238 frame. Assert:

```cpp
TestEqual(TEXT("permanent companion grid has three columns"), Roster->GetGridColumnCountForTest(), 3);
TestEqual(TEXT("permanent roster renders at most twelve slots"), Roster->GetPermanentSlotCapacityForTest(), 12);
TestTrue(TEXT("roster card uses first-row PSD frame"), Roster->GetCardFrameResourcePathForTest().Contains(TEXT("T_PartyDeck_CompanionCardFrame_01")));
TestTrue(TEXT("roster has paper ink right scrollbar"), Roster->HasPaperInkScrollbarForTest());
TestEqual(TEXT("five category labels follow approved naming"), Roster->GetCategoryLabelsForTest(), ApprovedLabels);
TestTrue(TEXT("task NPC entries are read-only"), Roster->IsTaskNpcEntryReadOnlyForTest(TEXT("Npc.TusiLeader")));
```

Test recruitment result preview, full-12 dismissal confirmation, cancel retaining the same offer, active-state indication for at most one permanent companion, and opening a roster entry into the shared character panel.

- [ ] **Step 2: Create a dedicated roster widget rather than extending the Town HUD's fake cards.**

`UGameXXKCompanionRosterWidget` owns a `UScrollBox` with a three-column `UUniformGridPanel`, a right-side scroll bar styled exclusively by Task 1 assets, and five PSD-backed editable category labels renamed to `全部 / 刀卫 / 医猎 / 术阵 / 任务`.

- `全部` shows all permanent partners; `刀卫`, `医猎`, `术阵` each group two approved roles; `任务` lists exactly six named NPCs read-only.
- Permanent cards show source-art frame, profile/role color strip, level, star, selected/active/new marks and partner-card count. Task cards show black/light-gray NPC treatment with no recruit/upgrade/equipment action.
- The grid's source is subsystem read-only views. Click permanent card opens partner subject mode; click task card opens read-only NPC/task configuration. The recruit action opens its immutable offer, and no button calculates a new seed.
- On full roster, a modal must require one explicit dismissal selection followed by confirmation. Cancel hides only the modal and leaves pending offer/result unchanged.

`UGameXXKTownHudWidget` keeps navigation only: remove the old flat codex overlay/card factory and call the player controller to show/hide this dedicated widget. Do not duplicate card or roster state in Town HUD.

- [ ] **Step 3: Build and run companion UI tests green.**

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.UI.CompanionRoster+GameXXK.MVP.Codex.RulesDiscovery;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

## Task 3: Turn the existing character panel into the shared skill/configuration panel

**Files:**

- Modify: `Source/GameXXK/Public/UI/GameXXKCharacterPanelWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKCharacterPanelWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCharacterPanelWidgetTest.cpp` (create if absent)
- Modify: player controller files only for widget ownership/open/close if needed

- [ ] **Step 1: Write red subject/configuration tests.**

Test hero, active permanent partner and task NPC views with the existing PSD tabs. Required assertions: hero has exactly eight ordered skill slots; permanent partner exactly five and shows locked cards/unlock condition; NPC exactly three selected from its fixed four and labels itself `任务同行，不可培养`; selecting/dragging an unlocked card changes only subsystem configuration state; locked/duplicate/wrong-owner cards are rejected; panel list uses paper/ink scrollbar.

- [ ] **Step 2: Add subject and skill-slot read models.**

Add a public subject identity (`Hero`, `PermanentCompanion`, `TaskNpc`) and a stable subject ID to `FGameXXKCharacterSummary` or an adjacent view struct. Keep `SetCharacterSummary` compatibility but add read-only builders that pull attributes, equipment eligibility, selected cards, available/locked cards and exact unlock text from the subsystem.

Reuse existing character title/attribute/equipment/skills/talent texture assets. The `技能` tab has an available-card scroll pane on the left and an ordered slot strip on the right. Players can click a card then a slot, or drag between slots; the only mutations are `SetHeroSelectedCards`, `SetCompanionSelectedCards`, and `SetTaskNpcSelectedCards`. The slot order is visible and explained as configuration/read order only; it does not bypass battle shuffling.

- [ ] **Step 3: Run green.**

Run a cold build and `Automation RunTests GameXXK.UI.CharacterPanel`. Expected result: one visual language, no extra generic bottom details panel, and exact 8/5/3 constraints.

## Task 4: Add task-NPC selection, map deck viewer and post-battle reward overlays

**Files:**

- Modify: `Source/GameXXK/Public/UI/GameXXKTaskPanelWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKTaskPanelWidget.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKOneGameRouteMapWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKOneGameRouteMapWidget.cpp`
- Create: `Source/GameXXK/Public/UI/GameXXKRouteDeckWidget.h`
- Create: `Source/GameXXK/Private/UI/GameXXKRouteDeckWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKRouteMapWidgetTest.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKRouteDeckWidgetTest.cpp`

- [ ] **Step 1: Write red task/map/reward tests.**

Test that a task row shows the bound NPC passive, terrain preference and four cards, permits exactly three selections, and cannot start a route with an invalid selection. Test the route deck viewer groups 18–30 cards by hero/partner/NPC/route, does not expose draw-pile order, has a paper/ink scrollbar, and labels temporary cards. Test the battle reward overlay has three first-row PSD cards, offers select/skip, requires a replacement click at 30 only on a temporary route card, and does not let a route node advance before choice/skip.

- [ ] **Step 2: Implement task page selection and route deck view.**

Extend the existing Task panel instead of creating a generic dialog. It shows the six NPC identities only when the task/path makes them available, uses actual task panel paper/list components, calls `SetActiveTaskNpc` / `SetTaskNpcSelectedCards`, and shows an explicit 3/4 counter.

`UGameXXKRouteDeckWidget` is hosted by the map HUD and battle reward flow. It renders the canonical run-deck groups and card metadata in read-only mode; its scrollable list uses Task 1 scrollbar assets. The map renders `FGameXXKRouteMapNode::Terrain` as the approved terrain seal/reference, not as a baked static node map. It must display current terrain, reward type and the `本次牌库` entry without leaking hidden draw order.

- [ ] **Step 3: Implement reward choice behavior.**

The reward overlay binds only to `PendingRouteReward.OfferedCardIds`. It renders three distinct first-row framed cards, then:

1. clicking an offer calls `ChoosePendingRouteReward(CardId)` when deck has room;
2. at 30, it opens a second state listing only temporary route card instances, permanently owned cards visibly locked/unselectable;
3. `跳过` calls `SkipPendingRouteReward`;
4. only a successful subsystem response closes the overlay and permits map continuation.

No panel is allowed to directly append a card or mark a route node complete.

- [ ] **Step 4: Run green.**

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.UI.TaskPanel+GameXXK.UI.RouteDeck+GameXXK.UI.RouteMap;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

## Task 5: Replace fixed battle commands with the hand, draw/discard and UnitId arrow-targeting interaction

**Files:**

- Modify: `Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleBoardWidgetTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKPlayerFlowWidgetTest.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKBattleHandWidgetTest.cpp`

- [ ] **Step 1: Write red battle-board interaction tests before deleting old action buttons.**

Replace `TargetingBasicAttack` / `TargetingCraneWingSlash`, enemy-index confirmation and selected-party command-menu tests with this contract:

```cpp
TestTrue(TEXT("clicking target card enters generic targeting"), Board->SelectHandCardForTest(CardInstanceId));
TestTrue(TEXT("candidate enemy receives target outline"), Board->IsUnitTargetHighlightedForTest(EnemyUnitId));
TestFalse(TEXT("wrong side is not highlighted"), Board->IsUnitTargetHighlightedForTest(FriendlyUnitId));
TestEqual(TEXT("arrow starts at card owner anchor"), Board->GetTargetingArrowStartForTest(), HeroAnchor);
Board->UpdateTargetingPointer(FVector2D(640.0f, 360.0f));
TestEqual(TEXT("arrow tracks pointer while targeting"), Board->GetTargetingPointerPositionForTest(), FVector2D(640.0f, 360.0f));
TestTrue(TEXT("clicking valid enemy submits its UnitId"), Board->ConfirmTargetingUnitForTest(EnemyUnitId));
TestFalse(TEXT("invalid friendly click has no cost or card move"), Board->ConfirmTargetingUnitForTest(FriendlyUnitId));
```

Add equivalent `SingleAlly` tests: friendly unit gets the highlight, enemy does not, pointer still follows mouse and friendly click works. Add `None`, `Self`, `AllEnemies`, `AllAllies`, `RandomEnemy`, `LowestHealthAlly`, no-candidate and cancel cases. Verify hand default order equals rules draw order; hover raises only display z-order; temporary display reorder never changes rule arrays; draw/discard counts update after card play; End Turn is enabled when no pending target/choice and blocked only while targeting, insight or forced discard is unresolved.

`GameXXKBattleHandWidgetTest.cpp` must assert the maximum five-card first-row ratio, centered light overlap, focus/hover elevation, read-only draw aggregate/discard-history views, and that no hand-display drag invokes a state-mutating subsystem API. Add target-state tests for `CardCheck`, `TargetingCard`, valid/invalid hover, automatic lock preview, pending choice, cancellation and resolving state; an invalid/blank click must leave the same selected `InstanceId`, resource totals and card zones intact.

- [ ] **Step 2: Replace the widget interaction mode and hand layout.**

Replace old enum values with `Hidden`, `Idle`, `CardCheck`, `TargetingCard`, `HoverValid`, `HoverInvalid`, `PendingChoice` and transient `Resolving`. Build the lower HUD from real hand instances returned by `GetBattleHand`: a centered five-card first-row-ratio strip, draw pile count on left, discard pile count on right, narrow intent strip, status label and End Turn button. There are no free Basic/Crane/Guiyuan/Defend/Healing buttons after this task.

`SelectHandCard(CardInstanceId)` enters `CardCheck`, calls `BuildBattleCardPlayPreview` / `GetBattleCardTargetRequest`, and reads source UnitId, presentation mode and the candidate-view list directly from rules. If it requires manual input, it stores the returned request and selected card/source UnitId, paints only `bCanSelect` candidate outlines, keeps non-selectable actors gray with their rule reason, and enters `TargetingCard`; it does **not** call `PlayBattleCard`. If it has no manual input, it displays any `AutomaticTargetUnitIds` as a short lock marker and calls `PlayBattleCard(CardInstanceId, NAME_None)` immediately. E/M/owner/condition failure leaves the card in place and surfaces the rule hint.

During `TargetingCard`, `NativePaint` reuses the existing arrow head and 12 ink-dab textures: the arrow tail starts at the projected card-owner actor anchor and its head follows the current mouse position. Candidate outlines are painted around projected candidate anchors using the existing ink-dab art, with a brighter lock mark while pointer is inside a candidate hit area. Apply the target presentation color from rules:朱砂 enemy, 玉绿 ally, 赭墨 any-unit; draw Self/AllEnemies/AllAllies/automatic target seals rather than a fake drag arrow. A left click on blank/invalid actor leaves targeting active; right click/Esc calls `CancelBattleTargeting` and cannot touch rules state. On valid actor click, pass `AGameXXKBattleSceneUnitActor::GetUnitId()` to `PlayBattleCard`; never pass an enemy or party array index.

- [ ] **Step 3: Refactor controller click routing for both sides.**

`AGameXXKMVPPlayerController::TryHandleBattleSceneLeftClick` must look up any live battle scene unit under the cursor, not `FindBattleSceneUnitUnderCursor(true)` limited to enemies. It asks the board whether that actor's stable UnitId is a current candidate, then confirms exactly that ID. It projects actor visual bounds to provide an anchor map keyed by UnitId; the board must use the card owner's anchor, not the legacy `SelectedPartyIndex` or menu click position. Existing actor `GetUnitId()` / `GetBattleVisualComponent()` stay source of identity/anchor; do not retune sprite assets or transforms.

- [ ] **Step 4: Implement draw, discard, insight and display-order affordances.**

Draw pile opens only aggregate/source counts; discard opens a reverse-chronological read-only list. The five-card hand defaults to logic draw order left-to-right; hover/focus raises visual z-order. Optional hand-internal drag only changes a transient `DisplayHandOrder` list owned by the widget and resets on next draw; it cannot call a rules mutation. `PendingChoice` renders the forced discard or Insight overlay supplied by rules, validates exactly the required IDs/order, and uses the corresponding subsystem method; it blocks End Turn but never asks the UI to change cards directly.

- [ ] **Step 5: Build and run green.**

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:/UE5 demo/GameXXK/GameXXK.uproject' -WaitMutex -NoHotReloadFromIDE
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.UI.BattleBoard+GameXXK.UI.PlayerFlow+GameXXK.PartyDeck.CardRules;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

## Task 6: Add shared partner backpack source and universal paper/ink scroll behavior

**Files:**

- Modify: `Source/GameXXK/Public/UI/GameXXKInventoryWindowWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKInventoryWindowWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKInventoryWindowWidgetTest.cpp`
- Modify: any new roster/task/deck scroll widget files from Tasks 2–5

- [ ] **Step 1: Write red inventory/scroll tests.**

Test that inventory source `PartnerBackpack` exposes shared partner equipment, experience materials, contract seals and card-unlock materials; it does not duplicate regular consumables/quest items. Test every roster, skill-list, task NPC list, route deck, discard list and partner inventory list with overflow exposes the right-side paper/ink scrollbar and no default white track.

- [ ] **Step 2: Implement source and reusable scrollbar styling.**

Append `PartnerBackpack` to `EGameXXKInventorySlotSource` (do not renumber/replace existing source values) and add a mode/filter path using the existing backpack textures. Resolve item ownership through subsystem state, not local widget arrays. Factor the Task 1 scroll rail/thumb style into a small shared UI helper so every relevant `UScrollBox` uses the same PSD-derived assets, thickness and right-edge placement.

- [ ] **Step 3: Run green.**

Run `Automation RunTests GameXXK.UI.InventoryWindow+GameXXK.UI.PartyDeck.Scrollbars` after a cold build.

## Task 7: Visual and playable-flow acceptance

- [ ] **Step 1: Manually inspect all five required panels at runtime.**

Verify `角色、伙伴图鉴、任务、地图、背包` one by one at normal and overflow content counts. Confirm first-row frames, correct ownership colors, NPC black/gray treatment, right scrollbars, no square frame, no generic white scrollbar, and no recreated hero/NPC face art.

- [ ] **Step 2: Run final automated and live flow verification.**

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 5
python scripts/gamexxk_real_play_flow_mcp.py --timeout 600 --report Saved/HarnessReports/party-deck-real-flow.json
```

Expected result: `L_Main → Start/New Game → L_QingshanInn → F quest → town route → node → battle` stays playable; deck configuration, target-card arrow interaction, reward choice and close/back input do not trap the player.

- [ ] **Step 3: Inspect staging before any commit.**

Run `git diff --check` and inspect every staged hunk. Assets and generated imports must be limited to manifest-listed PartyDeck files; never mass-stage the existing dirty worktree.
