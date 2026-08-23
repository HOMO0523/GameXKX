# Shared Inventory, Character Equipment, Tools, and Training Chests Design

**Status:** Approved.

## 1. Goal and scope

This work repairs and completes four connected systems on the pure-2D desktop Workbench:

1. shared Backpack/Warehouse storage with reliable two-click left-mouse movement;
2. per-character portraits, stats, decks, and six-slot equipment loadouts;
3. all five Tools modes, persistent item locks, tool progression, ten equipment/gem qualities, and sockets;
4. normal/advanced Training chest counters, strip-side open buttons, deterministic loot, and final chest icons.

Viewing a character remains independent from Formation. This specification does not change party order, active companion selection, active Quest NPC selection, battle rules unrelated to equipment projection, Talents, route events, or merchant behavior.

The implementation is split into three independently testable production units: inventory/character presentation, equipment/tools, and Training chests. They share authoritative transaction helpers but must not place mutation logic in widget callbacks.

## 2. Evidence and current root causes

The current failures are not one visual-only issue:

- `DesktopCarriedItemImage` is drawn last above every target and remains hit-testable. It intercepts the second left click; the root right-click cancellation still works, which explains the reported asymmetry.
- embedded Backpack cells delegate left click to the Workbench carry state, but embedded equipment slots do not. A carried equipment click therefore selects an equipment slot instead of placing or swapping the item.
- `InventoryCentralHeroIdle` always binds `T_MasterV2_HeroFullBody`; `ConfiguredDesktopTrainingCharacterId` never changes the central image.
- instance equipment loadouts are keyed correctly per character, but legacy item fallback reads the top-level hero weapon/armor/accessory for every viewed owner. Empty companion/NPC slots can therefore display hero equipment.
- Dismantle, Enhance, and Reforge have authority rules, but the obstructed carry flow prevents reliable input. Reforge immediately accepts its preview, hiding the intended decision.
- Combine and Socket explicitly return placeholder notices and cannot succeed.
- Training chests are currently inventory material stacks rather than openable chest-owned rewards.

The fix addresses these sources rather than adding more widget-local exceptions.

## 3. Authoritative ownership model

### 3.1 Shared storage

- Backpack and Warehouse are shared by the whole team.
- Item stacks and unequipped equipment instances live in exactly one physical Backpack or Warehouse cell.
- Existing capacities, paging, and the rule that one item type owns one whole stack remain authoritative.
- Tool input cells are non-authoritative reservations. Closing Tools, changing structural pages, persistence boundaries, or cancelling returns every reservation without changing inventory.

### 3.2 Per-character loadouts

The following stable owner IDs each keep an independent six-slot loadout:

- Hero;
- six permanent companion instances;
- six stable Quest NPC IDs.

The six slots remain Weapon, Head, Armor, Belt, Shoes, and Accessory. Switching the viewed owner changes only presentation and the loadout mutation target. It never changes Formation or the running Travel party.

Legacy hero item fallback is permitted only when the viewed owner is Hero. A companion or Quest NPC without an instance in a slot displays a genuinely empty slot.

## 4. Character switching and central presentation

The Workbench maintains one `ActiveBackpackCharacterId` as view-only state.

Switching Hero/Companion/NPC or a member portrait rebuilds these consumers from the same ID:

- center character image;
- display name and attributes;
- Equipment/Deck subpage content;
- six equipped-instance slots;
- equipment comparison target.

Hero keeps the approved current full-body art. Companions and Quest NPCs use the first frame of their authored 2K Idle battle atlas, bottom-centered and normalized to the existing central character frame. Missing art clears the brush instead of showing Hero or a white block.

Roster-category tabs select their stable representative. Member portrait buttons select the exact member. Neither action writes `PartySelection`, `OrderedFormation`, or Training Travel party state.

## 5. Unified two-click carry transaction

### 5.1 Input priority

- Alt+left click toggles lock and never starts a carry.
- Ordinary left click on an occupied Backpack, Warehouse, or Tool cell picks up the whole equipment instance or whole item stack as a non-committing carry preview.
- The carried icon is `HitTestInvisible` and can never block the second click.
- Right click or clicking the original source cell cancels and restores the unchanged origin.
- Owner/page changes cancel the carry before rebuilding.

### 5.2 Destinations

The second left click supports:

- empty Backpack/Warehouse cell: move;
- occupied Backpack/Warehouse cell: atomic swap;
- empty Tool cell: reserve;
- occupied Tool cell: swap reservations;
- compatible character equipment slot: equip;
- occupied compatible equipment slot: equip the carried instance and return the displaced instance to the carried instance's original Backpack/Warehouse cell.

An equipment-slot transaction validates owner, slot, route lock, source cell, displaced instance, and final capacity before committing. Wrong slot, stale source, locked route, or any failed validation leaves runtime state, physical cells, loadouts, carry state, and UI unchanged.

Tool reservations remember authoritative source container and slot. Tool completion validates that every input still matches its source before consuming anything.

## 6. Persistent locks

The save state adds:

- a set of locked equipment instance IDs;
- a set of locked item IDs.

An item lock applies to the entire stack of that item type. Lock state survives movement between Backpack and Warehouse and save/load.

Locked entries:

- may be moved, swapped, equipped, unequipped, enhanced, reforged, and socketed manually;
- are excluded from every auto-fill operation;
- cannot be consumed by Combine or Dismantle, even when manually reserved;
- display the approved `/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_CardLockedIcon` in the upper-right of Backpack, Warehouse, Equipment, and Tool cells.

Training chests do not enter inventory and cannot be locked.

## 7. Ten equipment and gem qualities

Serialized enum values append after the existing first three; existing values never change:

| Rank | Chinese | Enum |
|---:|---|---|
| 1 | 普通 | Common |
| 2 | 稀有 | Rare |
| 3 | 珍稀 | Epic |
| 4 | 传奇 | Legendary |
| 5 | 不朽 | Immortal |
| 6 | 至宝 | Treasure |
| 7 | 超凡 | Transcendent |
| 8 | 天界 | Celestial |
| 9 | 登神 | Ascendant |
| 10 | 宇宙 | Cosmic |

The same order applies to equipment quality, affix tier, and gem quality.

### 7.1 Equipment affixes

Affix counts are:

- Common 1;
- Rare 2;
- Epic 3;
- Legendary 4;
- Immortal and every higher quality 5.

Affix type remains unique per equipment instance and affix tier cannot exceed equipment quality. Existing affix magnitude rules extend deterministically to appended tiers without renumbering old serialized values.

### 7.2 Socket counts

Every equipment instance has at least one socket. Capacity is:

```text
Common–Immortal: 1
Treasure:         2
Transcendent:     3
Celestial:        4
Ascendant:        5
Cosmic:           6
```

Serialized socket contents have exactly the quality-derived capacity after normalization. Upgrading equipment quality appends empty sockets and never removes an existing gem.

### 7.3 Gems

There are Attack, Defense, and Max Health gems. Gems are stackable items keyed by type and quality.

Gem values double per quality rank:

```text
Attack/Defense: 1, 2, 4, 8, 16, 32, 64, 128, 256, 512
Max Health:    10,20,40,80,160,320,640,1280,2560,5120
```

Socketed gems contribute through the same loadout snapshot used by battle and Training projection. Each socket accepts one gem. Replacing a gem returns the old gem to shared Backpack; if that result cannot fit, the entire replacement rejects.

## 8. Tool progression

Tool level ranges from 1 through 10. Cumulative experience uses checked `int64` math.

The experience required to advance from the current level is:

```text
CurrentToolLevel * 100
```

Level 10 is capped. Quality experience multipliers are `9^(quality rank - 1)`:

```text
1, 9, 81, 729, 6561, 59049, 531441, 4782969, 43046721, 387420489
```

Awards are:

- Dismantle: sum of the multiplier for every consumed equipment instance;
- equipment/gem Combine: nine times the input-quality multiplier;
- Enhance: target-equipment multiplier after a successful level increase;
- Reforge: target-equipment multiplier exactly once when a paid preview is created, regardless of accepting new or keeping old affixes;
- Socket: target-equipment multiplier after a successful insert/replace.

The separately specified Tool Experience talent projection may multiply these awards when that talent graph is integrated; it does not alter item level or any non-tool experience.

## 9. Tool modes

Every mode presents exact input requirements, costs, output preview, experience, disabled reason, and a confirmation boundary.

### 9.1 Dismantle

- accepts 1–9 unlocked, unequipped equipment instances;
- rejects locked/stale inputs;
- previews base output per equipment: 10 ordinary gold, 1 Enhancement Stone, and 1 Refinement Sand;
- commits the existing atomic batch rule and tool experience together.

### 9.2 Combine

Combine exposes:

- Equipment / Item selector;
- Auto Fill button;
- persisted `Include Warehouse` toggle, default on;
- selected crafting level from 1 through current Tool Level.

Crafting level bands overlap at their boundary:

```text
1:  1–10
2: 10–20
3: 20–30
...
10: 90–100
```

Equipment Combine consumes exactly nine unlocked, unequipped equipment instances of one quality. Set, slot, item level, and enhancement level may differ. It creates one next-quality instance with a deterministic random modern set, slot, affixes, and an item level inside the selected crafting band. Legacy and Starter definitions are not output candidates. Cosmic equipment cannot combine.

Gem Combine consumes exactly nine gems of the same type and quality, then creates one gem of the same type at the next quality. Other item types are not combinable. Cosmic gems cannot combine.

Manual first input locks type and quality. When empty, Auto Fill selects the lowest combinable quality with nine eligible inputs. It excludes locked/equipped/reserved inputs, searches Backpack, and searches Warehouse only when the persisted toggle is on. Selection is deterministic: container priority Backpack then Warehouse, unenhanced before enhanced, lower item level, then stable ID.

The output uses a cell freed by this transaction: a freed Backpack cell first, otherwise a freed Warehouse cell. The nine removals and one creation commit atomically.

### 9.3 Enhance

- accepts one equipment instance;
- permits a locked instance;
- consumes the existing Enhancement Stone cost;
- preserves the existing +0 through +10 cap;
- refreshes item presentation, character stats, material count, and tool experience after commit.

### 9.4 Reforge

- accepts one equipment instance and permits a locked instance;
- consumes the existing Refinement Sand cost;
- displays old and candidate affix values;
- requires `Use New Affix` or `Keep Old Affix` instead of auto-accepting;
- never refunds the paid sand, matching existing authority;
- awards tool experience once per preview, never again during resolution.

### 9.5 Socket

- first input is one equipment instance;
- second input is one gem stack;
- exposes a socket selector from 1 through the equipment's capacity;
- permits a locked equipment instance;
- consumes one selected gem and writes it to the selected socket;
- returns a replaced gem to Backpack atomically;
- rejects full-capacity/stale/invalid-type cases without mutation.

## 10. Training chest wallet and loot

Training chests are not items. The save state stores stable chest tokens so accumulated chests retain their source item level:

```text
Tier, SourceStageId, SourceItemLevel, AcquisitionOrdinal
```

`SourceItemLevel` is the player/combat level at the moment the chest is earned, clamped to 1–100. UI counts are derived totals for normal and advanced tokens.

Online rewards append a token immediately. Offline collection appends one token per earned chest using the saved Travel stage and collection-time combat level. Pre-v25 inventory chest stacks migrate into tokens; because they contain no historical source, they use the saved current Travel stage and clamped player level. The legacy item entries and physical cells are removed.

The two chest buttons are vertical at the right edge of the always-on Training strip:

- normal icon and owned count;
- advanced icon and owned count.

Left click opens one token of that tier. Right click opens tokens of only that tier until none remain or the next reward cannot fit.

Each token uses its acquisition ordinal plus a persisted open ordinal for a deterministic roll. Reloading cannot reroll an unopened token after a committed open.

### 10.1 Loot tables

Normal chest:

- 50% one random Common modern equipment instance at the token's source item level;
- 50% one equally weighted item result: Common Attack gem, Common Defense gem, Common Max Health gem, 1 Enhancement Stone, or 1 Refinement Sand.

Advanced chest:

- 50% one random Rare modern equipment instance at the token's source item level;
- 50% one equally weighted item result: Rare Attack gem, Rare Defense gem, Rare Max Health gem, 3 Enhancement Stones, or 3 Refinement Sand.

One chest is one atomic candidate transaction. Stackable output may enter an existing unlocked or locked stack because it does not consume it. Equipment needs one free Backpack physical cell. If the next predetermined result cannot fit, that token and every remaining token stay unopened. Rewards never spill into Warehouse.

## 11. Chest art

The approved direction is the Q-version ink-cartoon equipment-icon style: irregular thick ink contour, rounded proportions, broad hand-painted color masses, and low micro-detail.

Current selected candidates:

- `SourceArt/UI/Items/Chests/final/T_Item_TrainingNormalChest_v3_candidate.png`
- `SourceArt/UI/Items/Chests/final/T_Item_TrainingAdvancedChest_v3_candidate.png`

Before import, both are background-extracted into real RGBA, centered on equal square transparent canvases, and resized to matching 512×512 source masters without changing their chest silhouettes. Import targets are:

- `/Game/GameXXK/UI/Items/T_Item_TrainingNormalChest`
- `/Game/GameXXK/UI/Items/T_Item_TrainingAdvancedChest`

They use the same UI texture settings as existing equipment/item icons.

## 12. Save migration and validation

This feature claims save version 25 as `EquipmentToolsAndChestWalletIntroducedSaveVersion`. Any later Ordered Formation redesign must append after it rather than reuse 25.

Migration:

- preserves all existing slots, equipment instances, loadouts, materials, Training progression, and rewards;
- converts legacy normal/advanced chest item counts into tokens and removes their item/slot projections;
- initializes lock sets empty;
- initializes Tool Level 1, experience 0, selected crafting level 1, and Include Warehouse on;
- normalizes old three-quality equipment without changing quality ordinals;
- creates socket arrays at the capacity derived from existing quality and leaves them empty.

Validation rejects unknown qualities, invalid affix/socket counts, duplicate locked IDs, locks for nonexistent entries, invalid gems, chest ordinals/tokens, escaped Tool reservations, out-of-range Tool state, or any item/equipment present in multiple owners/containers.

## 13. Error handling and atomicity

Every mutation follows candidate-copy validation and a single public commit. Errors surface as stable user-facing text and preserve:

- source/destination cells;
- item quantities;
- equipment ownership and loadouts;
- locks;
- Tool inputs, materials, gold, experience, and pending Reforge;
- gem sockets;
- chest token/order state.

No UI callback loops over repeated public mutations for a batch. Combine, Dismantle, right-click open-all, cross-container swap, equipment replacement, and gem replacement each call a bounded authoritative transaction.

## 14. Verification

### 14.1 Inventory and character presentation

- all 13 owners show the correct center art, stats, deck size, and independent loadout;
- viewing never changes Formation or running Travel;
- non-Hero empty slots never inherit Hero legacy items;
- carry icon is non-hit-testable;
- empty placement and occupied swaps pass for Backpack, Warehouse, Tool, and Equipment destinations;
- incompatible/stale/full/route-locked operations roll back byte-identically.

### 14.2 Locks and tools

- Alt+left click persists lock state and renders the approved icon on every surface;
- auto-fill never selects locked entries;
- Dismantle/Combine reject manually supplied locked entries;
- all ten quality ordinals, names, affix counts, socket counts, and next-quality transitions are exact;
- quality XP powers of nine use checked `int64` and Tool Level caps at 10;
- crafting bands include their confirmed overlapping endpoints;
- equipment and gem 9-to-1 paths consume exactly nine and produce exactly one;
- Enhance, Reforge preview resolution, and every socket index have causal tests.

### 14.3 Chests

- old item chests migrate exactly once;
- icons/counts remain visible beside the strip while Backpack is collapsed or expanded;
- left click opens one; right click never crosses chest tier;
- equipment/item 50/50 branch and five item outcomes are seed-deterministic;
- full Backpack stops before consuming the blocked token;
- open-all has a hard token-count bound and no reroll on reload.

### 14.4 Delivery

- deterministic art dimension/alpha/hash checks;
- cold UHT/UBT, no Live Coding or Hot Reload;
- focused inventory/equipment/economy/Workbench/Training/SaveGame suites, then proportional broad regression;
- direct visual review without Luna per user instruction;
- real PIE on `/Game/GameXXK/Maps/L_DesktopTrainingHUD`, left running for user testing.
