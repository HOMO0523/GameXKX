# GameXXK MVP Design

> Status: confirmed by user on 2026-07-01.

## Goal

Build a first playable MVP for 《霞客行》 that demonstrates the full loop:

Main menu -> world map UI -> select unlocked region -> enter town -> accept quest -> follower joins -> enter node dungeon -> fight/event/camp/Boss -> fail and return for trading, or win and unlock the next region.

The MVP favors a complete, inspectable gameplay loop over final art polish. Core rules are implemented in C++; Blueprints, UMG, Paper2D, and PaperZD are used for presentation and editor assembly.

## Current Project Context

- Project: `D:\UE5 demo\GameXXK`
- Engine: UE 5.8
- Enabled plugins: Paper2D, PaperZD, ModelContextProtocol, MCPClientToolset, AllToolsets, EditorScriptingUtilities
- Reference project: `D:\UE5 demo\TestMap`
- TestMap engine: UE 5.4
- TestMap use: migrate as prototype/reference content, not as runtime authority

## MVP Non Goals

The first stage does not include:

- Open-world walking on the world map
- Multiple complete towns
- Full story branches
- Full NPC affection, permanent companion growth, or multi-follower party management
- Full Octopath-style jobs, multi-weapon weakness tables, or complete Boost multi-hit behavior
- Full item rarity, dynamic shop economy, blacksmith strengthening, or crafting
- Saving battle-in-progress or dungeon-node-in-progress state
- Full town art polish
- Enemy and Boss battle animations

## User Flow

1. Game starts at a main menu.
2. Player selects `开始游戏`.
3. Game loads the minimal SaveGame, or creates default progress if no save exists.
4. World map UI opens.
5. World map shows three regions:
   - `青山驿站`, unlocked at start
   - `黄山险径`, represented by the first node dungeon route
   - `潭江渡口`, locked until the first Boss is defeated
6. Player clicks `青山驿站` and enters the town level.
7. Player moves with WASD or arrow keys.
8. Player approaches the quest NPC and presses `F`.
9. Quest dialog opens with `接受` and `离开`.
10. Accepting the quest marks it accepted, adds the quest NPC as a temporary follower, and opens the town exit.
11. Player approaches the exit and presses `F`.
12. If quest is not accepted, show a prompt telling the player to accept the quest first.
13. If quest is accepted, open the UMG node dungeon map.
14. Player completes combat, event, camp, and Boss nodes.
15. Failure exits the dungeon back to town with earned XP and gold kept, used consumables consumed, and quest still accepted.
16. Player can trade with the merchant, refill supplies, and retry.
17. Boss victory completes the quest, unlocks `潭江渡口`, saves progress, and returns the follower to NPC state.

## Scenes And Screens

### Main Menu

- Buttons: `开始游戏`, `退出游戏`
- `开始游戏` loads or creates SaveGame and opens the world map UI.
- `退出游戏` can close the menu or call quit depending on runtime context.
- No continue-game or settings screen in MVP.

### World Map UI

- World map is UI only, not a walkable scene.
- Shows three region entries.
- Locked regions are visually dark.
- Clicking an unlocked region enters its town level.
- Clicking a locked region shows `尚未解锁`.
- First stage only implements the `青山驿站` town. `潭江渡口` can unlock visually and show a future-content prompt when clicked.

### Town Level

- Town name: `青山驿站`
- Layout: straight post-road layout.
- Art: simple Paper2D TileMap or blockout tiles.
- Required spaces:
  - player spawn
  - quest NPC position
  - merchant NPC position
  - exit interaction zone
  - simple road, house, and blocking collision
- Movement: WASD and arrow keys.
- Interaction: approach an interactable and press `F`.
- UI shortcuts:
  - `I`: open inventory
  - right-click character: open character panel
  - `Esc`: close UI or go back

## Characters

### Player Character

- Role: main controllable character and only persistent leveling character.
- Visual base: image 3, deep-blue high ponytail.
- Town movement: use existing 8-direction sprite sheet.
- Missing diagonal directions are filled by horizontal mirroring where appropriate.
- Needed generated actions:
  - 4-direction idle
  - attack
  - hit
  - skill
  - die
- Uses PaperZD for presentation.

### Quest NPC And Temporary Follower

- Role: quest giver, temporary follower, and battle party member.
- Visual base: image 2, blue-gray hat.
- Before quest: stands at quest position.
- After quest accepted:
  - joins as temporary follower
  - physically follows the player in town
  - enters node dungeon and battle as the second party member
- After Boss victory or dungeon failure return:
  - remains a follower if quest is still accepted after failure
  - returns to NPC state after quest completion
- The follower has HP, speed, attack, defense, basic attack, and one skill.
- The follower does not gain XP, change equipment, persist growth, or keep long-term state.
- Needed generated actions:
  - 4-direction idle
  - attack
  - hit
  - skill
  - die

### Merchant NPC

- Role: trading.
- Visual base: image 1, green clothes and long hair.
- Static or simple idle presentation is enough for MVP.
- Does not follow the player.

### Enemy And Boss

- Two normal enemy types.
- One Boss type.
- Enemies and Boss use static pixel art or icons in MVP.
- No enemy or Boss animation required in the first stage.

## Quest

### Quest Story

Use a short story premise: the quest NPC asks the player to help explore or escort through `黄山险径`.

### Quest States

- NotAccepted
- Accepted
- Completed

### Quest Rules

- NotAccepted:
  - town exit does not open the node dungeon
  - exit interaction prompts the player to accept the quest first
- Accepted:
  - quest NPC becomes a follower
  - town exit opens the node dungeon UI
  - failure returns to town and keeps the quest accepted
- Completed:
  - follower returns to NPC state
  - `潭江渡口` unlocks on world map
  - progress is saved

## Node Dungeon

### TestMap Reference

The TestMap project contains a useful Slay-the-Spire-style map prototype:

- `Content/1Game/UI/UI_地图选择.uasset`
- `Content/1Game/UI/UI_地图选择-关卡.uasset`
- `Content/1Game/UI/UI_地图选择-关卡-线.uasset`
- `Content/1Game/UI/UI_地图选择-Boss.uasset`
- `Content/1Game/Data/*`
- `Content/1Game/Texture/*`

MVP migration approach:

- Copy the entire `Content/1Game` folder from TestMap into GameXXK under `Content/Prototype/TestMap/1Game`.
- Preserve dependencies so the prototype can be opened in UE 5.8.
- Do not set TestMap `BP_PlayerController` or `BP_Gamemode` as GameXXK defaults.
- Use TestMap UI style, icons, gray/bright states, and line presentation as reference assets.
- New GameXXK C++ rules own dungeon state and progression.
- UMG widgets display rules state and send click events back to GameXXK APIs.

### MVP Dungeon Shape

Use a fixed 5-node branch map:

```text
Start
  -> Battle
  -> Event
Both paths merge
  -> Camp
  -> Boss
```

### Node Types

- Battle
- Event
- Camp
- Boss

### Node Rules

- Only reachable nodes can be selected.
- Completed nodes become completed/visited.
- Unreachable nodes are dark or disabled.
- Event node presents two choices:
  - gain gold
  - gain item
- Camp node presents two choices:
  - heal party HP
  - gain one healing item
- Boss victory completes the quest and unlocks the next world map region.

## Battle

### Party Shape

- Player side: player character plus one temporary follower.
- Enemy side: two enemies for normal battles.
- Boss battle: one Boss, with tuning allowed to be sturdier than normal enemies.

### Combat Mechanics

Battle is a minimal Octopath-like turn-based prototype:

- Speed determines turn order.
- Commands:
  - basic attack
  - skill
  - defend
  - item
- Each actor gains 1 BP at the start of its turn, up to 3.
- Skills spend BP.
- Player character has two skills.
- Follower has one skill.
- Enemy has one skill.
- Normal enemies have one weakness type.
- Hitting a weakness reduces shield by 1.
- Shield reaching 0 causes break for 1 round.
- Broken enemies skip or lose their next effective action and take increased damage if implemented in tuning.
- Boss may have more shield or a second weakness, but the first implementation should keep the rule surface small.

### Victory And Failure

Victory:

- Awards XP and gold to the player.
- May award items.
- Marks node complete.
- Boss victory completes quest, unlocks `潭江渡口`, and saves.

Failure:

- Exits the node dungeon.
- Returns to `青山驿站`.
- Keeps quest accepted.
- Keeps follower active.
- Keeps earned XP and gold.
- Consumes used consumables.
- Does not save current dungeon node state.
- Allows trade and retry.

### Leveling

- Only the player character gains XP.
- Follower does not level.
- Level thresholds are fixed.
- Level up increases HP, attack, defense, and speed.
- Speed growth should be numerically modest so turn order remains tunable.

## Inventory And Equipment

### Inventory Data

Player inventory contains:

- gold
- consumable item stacks
- equipment item stacks or entries

### Equipment

Slots:

- weapon
- armor

Rules:

- weapon adds attack
- armor adds defense and HP
- player can change equipment in town or world map UI
- player cannot change equipment in node dungeon or battle
- follower equipment is not editable in MVP

### Dungeon Snapshot

Entering the node dungeon creates a snapshot:

- player character stats
- follower stats
- equipment-derived bonuses
- consumable quantities

Consumable usage in battle is deducted from the snapshot and then synchronized back to main inventory on failure or success.

## Trading

### Merchant

- Separate merchant NPC in town.
- Interact with `F`.
- Opens trading UI.

### Trading UI

Panels:

- player inventory
- NPC sell area
- NPC buyback/acquisition area

Rules:

- Player can buy from NPC sell area.
- Player can place items into NPC acquisition area and confirm sale.
- Item data stores buy price and sell price directly.
- Initial shop stock:
  - healing item
  - normal weapon
  - normal armor

No dynamic pricing, relationship discount, stock refresh, blacksmith, or crafting in MVP.

## Character Panel

Right-click opens character panel.

Player panel:

- name
- level
- HP
- attack
- defense
- speed
- weakness or weapon type display
- equipment slots
- inventory entry

Follower panel:

- name
- HP
- attack
- defense
- speed
- weakness or weapon type display
- equipment display only

Battle does not use right-click panels in MVP. Battle has its own command UI.

## SaveGame

Persisted:

- unlocked regions
- quest state
- player level and current XP
- player gold

Not persisted in MVP:

- battle-in-progress state
- node-dungeon-in-progress state
- follower location
- detailed inventory and equipment
- merchant stock changes

Save points:

- when starting a new save from main menu defaults
- after Boss victory
- after failure if player XP or gold changed

## Technical Architecture

### C++ Owns Rules

Core systems should be implemented in C++:

- world map region unlock state
- quest state
- town interaction rules
- follower state
- node dungeon model and reachable-node logic
- battle model and turn resolution
- inventory, equipment, and trading data
- SaveGame read/write

### Blueprint And UMG Own Presentation

Blueprint, UMG, Paper2D, and PaperZD should handle:

- town level assembly
- TileMap or blockout art
- widget layout
- PaperZD animation graph and flipbook assignment
- right-click panel presentation
- node map display
- battle command UI

### Data Driven Objects

Use data assets or data tables for:

- items
- equipment stats
- skills
- enemies
- Boss
- shop stock
- dungeon node definitions
- region definitions

### Testability

Rules should be testable without a fully interactive UE scene. Minimum verification targets:

- quest not accepted blocks dungeon entry
- accepting quest enables follower and dungeon entry
- node map exposes only reachable nodes
- event and camp choices apply rewards
- battle speed ordering is deterministic for known stats
- weakness hit reduces shield and break lasts one round
- failure returns to town with XP/gold kept and used consumables consumed
- Boss victory unlocks `潭江渡口`
- trade buy and sell modify gold and item stacks correctly
- SaveGame persists region, quest state, player level, current XP, and gold

## Acceptance Demo

The MVP is accepted when the following can be demonstrated:

1. Start at main menu.
2. Click `开始游戏`.
3. See world map UI.
4. Click `青山驿站`.
5. Move in town with WASD or arrow keys.
6. Approach task NPC and press `F`.
7. See dialog and accept quest.
8. See NPC become follower and follow the player.
9. Approach town exit and press `F`.
10. See node dungeon UI using TestMap-inspired style.
11. Complete a combat node.
12. Complete an event node choice or alternate route.
13. Complete camp choice.
14. Lose a battle in one run and return to town.
15. Confirm earned XP/gold remain and used consumables are gone.
16. Trade with merchant to buy supplies or sell an item.
17. Retry dungeon.
18. Defeat Boss.
19. Return to world map and see `潭江渡口` unlocked.
20. Restart or reload and see minimal SaveGame state restored.

## Implementation Approach

Recommended order:

1. Add C++ data types and deterministic rule tests for quest, inventory, trading, dungeon, and battle.
2. Migrate TestMap `Content/1Game` into `Content/Prototype/TestMap/1Game`.
3. Build world map, main menu, and town blockout.
4. Build interaction, quest, follower, and merchant flow.
5. Connect node dungeon UI to new C++ dungeon state.
6. Build battle UI and connect to C++ battle state.
7. Generate and import missing character sprites.
8. Wire SaveGame and acceptance demo path.

## Risks

- Scope is larger than a tiny MVP because it includes town movement, follower movement, trading, inventory, SaveGame, and an Octopath-like battle prototype.
- PaperZD asset setup and generated sprite consistency may take iteration.
- Migrated UE 5.4 TestMap assets may require UE 5.8 upgrade and reference repair.
- Blueprint prototype logic should be studied but not allowed to become the source of global runtime truth.
- If time gets tight, reduce presentation first: keep C++ rules and UI flow, defer battle animations and town polish.
