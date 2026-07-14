# GameXXK Town HUD Reference Refresh Design

**Status:** User-approved design discussion on 2026-07-14; awaiting user review of this written specification.

**Scope:** Re-skin the town-facing HUD and its existing task, character, inventory, and merchant windows with cropped visual atoms from the Tencent Docs `霞客行 美术参考` reference. Preserve the existing MVP gameplay, inventory, save, merchant, and route rules. The one deliberate interaction change is that task-NPC `F` opens a task dialogue before the player chooses to accept or leave.

## 1. Product Decision

The lower `江湖行` four-card illustration is not a production UI requirement. It is only a style reference. The production priority is the reference's upper resource icons and the task/map/inventory/character window language:

- warm parchment, inked irregular borders, muted teal/ochre accents, and hand-painted small icons;
- a compact persistent resource strip instead of debug-style text;
- a player status card and a small right-side navigation rail;
- consistent paper windows for task dialogue, task log, character, inventory, and merchant trade.

The result is a visual and layout refresh. It does not add currencies, task rewards, mail, arena, or other unimplemented systems just because a visual appears in the reference.

## 2. Screen and Input Contract

### 2.1 Town HUD

The town HUD is visible while the player moves in the town.

```text
Top resource strip       : existing gold and real material/resource counts only
Player status card       : portrait, level, HP/XP or other already-owned live state
Right navigation rail    : character, task, map, inventory, system only when each action exists
Quest tracker            : current task title and concise objective
World interaction prompt : contextual F prompt near the interactable, never a permanent menu button
```

- Resource icons are display atoms, not fake buttons. A resource that has no real data is hidden, not displayed as `0` merely to imitate the reference.
- Every visible navigation icon has a live click target and opens the corresponding existing window or a clearly implemented system overlay. A non-implemented reference icon is excluded from the runtime HUD.
- The town keeps its existing `WASD`/arrow movement, `F` interaction, `I` inventory, and `Esc` close/system behavior. No additional shortcuts are assumed by this design.
- When a modal window is visible, movement and world interaction are blocked, the mouse is shown, and closing it restores game viewport input.

### 2.2 Task NPC Dialogue

```text
Town HUD
  -> approach task NPC and press F
  -> central parchment dialogue
       -> Accept: invoke the existing quest-acceptance path once
       -> Leave : close without changing state
  -> close dialogue and restore the town HUD
```

The dialogue has an NPC title/portrait region, a dynamic task title and description region, a rewards region only when existing task data supplies rewards, and two paper-button actions. It must show an already-accepted task as progress/next objective rather than offering duplicate acceptance.

### 2.3 Existing Panels

| Player action | Existing functional destination | New presentation requirement |
|---|---|---|
| Right-rail character icon | character panel | parchment player sheet, ink tabs, live stats and equipment slots |
| Right-rail task icon | task panel | paper task list with state tabs and current objective emphasis |
| Right-rail map icon | existing world-map command | paper map presentation; north-gate route map remains a different screen |
| Right-rail inventory icon or `I` | free inventory | reference-style frame, tabs, item-slot atoms, live quantities |
| Merchant `F` | merchant inventory mode | same inventory shell, with live buy/sell panes and price presentation |
| `Esc` or system icon | existing close/system path | paper pause/system panel when that existing path is presented |

No screen gets a parallel game-state implementation. Controls continue to dispatch existing commands and the MVP subsystem remains the authority for state and travel.

## 3. Reference-Crop Asset Workflow

All style images used by this batch come from the user-supplied Tencent Docs reference, not from newly generated art.

```text
Tencent Docs source image
  -> preserve one unedited high-resolution reference export
  -> crop reusable no-text or fixed-text visual atoms
  -> inspect alpha/background and trim transparent edges
  -> import approved runtime copies into UE
```

Source and derivative locations:

```text
docs/ui/v1/source_art/qqdoc-2026-07-14/
  reference-ui-composite.png
  crop-manifest.json
  crops/

Content/GameXXK/UI/V1/Town/
  HUD/
  Dialogue/
  Windows/
  Atoms/
```

The original composite is never imported as a single runtime background. Crops retain their original painted texture; they are not redrawn or AI-regenerated.

### 3.1 Crop Manifest

| Runtime id | Crop purpose | Text policy |
|---|---|---|
| `A_HUD_ResourceStrip_Base` | parchment/ink backing behind resource icons | no live text baked in |
| `A_HUD_GoldIcon` | gold visual | live count beside it |
| `A_HUD_MaterialIcon_*` | real material/resource visuals | live count beside each icon |
| `A_HUD_PlayerCard` | player-status backing and portrait surround | portrait, level, HP/XP are live |
| `A_HUD_RightNavFrame` | vertical navigation backing | interactive icons placed separately |
| `A_HUD_QuestTracker` | current-objective backing | task title/progress are live |
| `A_QuestDialog_Frame` | central task dialogue frame | all task content is live |
| `A_Button_Paper_Normal/Hover/Pressed/Disabled` | common action-button visuals | fixed label may be a separate crop; state text remains readable |
| `A_Window_Frame` | shared character/task/inventory/shop frame | titles and contents are live or separate fixed-label crops |
| `A_Window_Tab` | shared panel tabs | fixed tab label may be cropped separately |
| `A_ItemSlot` | inventory/equipment slot frame | item image and count are live |

Each crop records source coordinates, native dimensions, intended logical size at the 1920x1080 design canvas, alpha/background handling, and any nine-slice margins in `crop-manifest.json`.

## 4. Layering and Data Rules

Each town-facing widget uses the same layer order:

```text
0  background / dimmer
10 painted parchment, frames, ink decoration
20 live data: portraits, items, task descriptions, numbers
30 interaction: UButtons, focus, tooltips, F prompt
40 modal dialogue or confirmation
```

- Fixed decoration and approved fixed labels may be cropped art.
- Gold, materials, level, HP/XP, item counts, prices, task text, task progress, player name, and save information remain live UMG text or images.
- A panel refreshes through its existing state source whenever it opens. There is no extra reward, inventory, or economy mutation in this visual pass.
- Hidden panels are `Collapsed` or non-hit-testable and may never intercept town movement or `F`.

## 5. Implementation Boundaries

The visual implementation wraps existing player-facing widgets rather than replacing their ownership rules:

- `UGameXXKTownOverlayWidget` gains the reference-layout town HUD shell.
- `UGameXXKQuestDialogWidget` becomes the visible `F` task dialogue before its existing accept operation is invoked.
- `UGameXXKInventoryWindowWidget` keeps free-inventory and merchant modes; only its presentation layer is replaced.
- character and task panels receive the same shared frame atoms and navigation treatment.
- the town player controller remains responsible for input mode and widget mounting.

The following are explicitly outside this batch: main-menu reskin, full world-map reskin, north-gate route-map reskin, battle UI reskin, new currencies, mail, new quest reward rules, and gameplay-rule rewrites.

## 6. Delivery Order and Acceptance

1. Capture the reference export and create the crop manifest plus reusable town atoms.
2. Apply the atoms to the town HUD: resource strip, player card, navigation rail, quest tracker, and contextual `F` prompt.
3. Implement the paper task dialogue around the existing accept/leave flow.
4. Apply the shared window shell to task, character, inventory, and merchant views.
5. Verify in PIE: town movement works; task NPC `F` opens the dialogue; Accept and Leave behave correctly; `I` opens the reskinned inventory; merchant `F` opens the reskinned trade mode; `Esc` closes panels and restores town input.

Acceptance requires a screenshot review at 1920x1080 and a real PIE interaction pass. No button may be visible without an actual action or an explicit locked state.
