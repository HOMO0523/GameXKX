# Decoupled Party Composition and Position Order Design

**Status:** Approved design; supersedes the generic ordered-member model in Section 6 of `2026-08-22-workbench-route-formation-talent-system-design.md`.

## 1. Goal

Formation has two separate responsibilities:

1. Select exactly one Hero, one permanent Companion, and one Quest Character.
2. Arrange those three categories into the `1P / 2P / 3P` combat and presentation order.

Changing a selected character must never change position order. Swapping position order must never change the selected three-character roster.

## 2. Authoritative persisted model

The next save schema stores a decoupled formation state:

```text
Composition
  SelectedHeroId
  SelectedCompanionId
  SelectedQuestNpcId

PositionOrder
  [Hero, Companion, QuestNpc] // one category per 1P / 2P / 3P slot
```

`PositionOrder` is a permutation of exactly these three category values:

- `Hero`
- `Companion`
- `QuestNpc`

The default is:

```text
1P = Hero
2P = Companion
3P = QuestNpc
```

The generic v24 `Members` array remains only as a migration source. It is no longer authoritative after migration.

## 3. Validation invariants

A current formation is valid only when all conditions hold:

- all three selected IDs are present;
- the selected Hero resolves to an owned/current Hero;
- the selected Companion resolves to one of the six fixed companion roles;
- the selected Quest NPC resolves to an owned and formation-eligible quest character;
- the three selected entities are distinct;
- `PositionOrder` contains exactly three entries;
- every category appears exactly once;
- no unknown category or duplicate position category is present.

Current-version invalid data is rejected. Only older save versions use migration recovery.

## 4. Fixed companion roster

The game owns exactly one persistent companion for each role:

- 刀客
- 守卫
- 药师
- 射手
- 法师
- 阵师

Player-facing recruitment, random replacement, and dismissal are removed. Switching the selected Companion changes only `SelectedCompanionId`; it never deletes or resets any companion.

Every fixed companion retains its own progression data. In this delivery:

- all cards belonging to each companion are immediately unlocked and selectable;
- each companion is initialized with one catalog-valid default deck;
- changing the active companion preserves every companion's progression and selected deck;
- future card locking and level-gated selection are explicitly deferred.

Existing saves already contain one companion per role, so no duplicate-instance merge is required.

## 5. Persistent quest-character selection

`SelectedQuestNpcId` is persistent formation data. It is not the same thing as route-local NPC provenance.

When a route begins:

1. resolve the persistent selected Quest NPC;
2. project that NPC and its selected deck into the route-local active NPC fields;
3. keep the persistent formation unchanged.

When a route ends:

1. clear route-local NPC provenance and route battle state;
2. keep `SelectedQuestNpcId` unchanged;
3. keep `PositionOrder` unchanged.

An event may not silently replace the persistent Quest NPC. A future unlock event may add a Quest NPC to the available candidate set, but the player changes the selected NPC only from the Formation screen.

## 6. Ordered-member resolution

All runtime consumers use one resolver:

```text
Composition + PositionOrder -> ordered member refs for 1P / 2P / 3P
```

The resolver first validates the composition and category permutation, then maps each category to its selected member ID. Compatibility fields are projections only and never decide order.

The following consumers must use the same resolved array:

- idle strip portraits;
- Training Travel party and first attacker;
- route/card battle party order;
- status bars and targeting order;
- route merchant carried-card owner order;
- any future party-wide presentation.

## 7. Compatibility behavior

Legacy APIs are compatibility adapters:

- selecting an active companion updates only `SelectedCompanionId`;
- selecting a town Quest NPC updates only `SelectedQuestNpcId`;
- neither operation changes `PositionOrder`;
- clearing the selected companion or Quest NPC is rejected because all three categories are mandatory;
- recruitment, random replacement, and dismissal are unavailable to players.

Compatibility mirrors are regenerated from the authoritative composition for older systems that still read active companion or route-local NPC fields.

## 8. Formation screen

Formation replaces the Backpack center and has a local top-right `X` returning to Backpack.

### 8.1 Three-character composition

The upper section has three fixed category cards:

- 主角
- 伙伴
- 任务角色

Selecting a category filters the candidate area to that category only. The current Hero count is one, but the Hero picker supports future additional heroes.

Companion candidates display only the six approved profession labels. Long generated or instance names are not shown.

### 8.2 Position order

The lower section has three separate slots:

- `1P`
- `2P`
- `3P`

Each slot displays its category label and the currently selected character for that category. Selecting two position slots swaps their categories. It never changes any selected character ID.

Replacing a Hero, Companion, or Quest NPC automatically updates whichever P-slot currently contains that category.

### 8.3 Draft and commit

Opening Formation copies authoritative composition and order into a draft.

- `Apply Formation` validates and commits the complete draft atomically.
- the local Formation `X`, Backpack parent `X`, or `Tab` discards the draft.
- a failed apply leaves composition, position order, compatibility mirrors, Travel state, and battle state unchanged.

## 9. Save migration

Allocate the next append-only save version for the decoupled formation model.

### 9.1 v24 migration

For a valid v24 array containing exactly one Hero, one Companion, and one Quest NPC:

- derive the three selected IDs from their categories;
- derive `PositionOrder` from their existing array positions.

For a v24 array that does not contain exactly one of each category:

- preserve the valid Hero selection when possible;
- preserve the first valid selected Companion when possible;
- select the existing persistent/current Quest NPC, otherwise use the deterministic default owned Quest NPC;
- use the default `Hero / Companion / QuestNpc` position order unless all three old category positions can be preserved unambiguously.

### 9.2 pre-v24 migration

Derive composition from legacy Hero, active Companion, and current/owned Quest NPC fields. Use the default position order.

Migration never removes or overwrites existing companion progression, decks, equipment, or quest-character loadouts. Current-version malformed data is rejected rather than silently repaired.

## 10. Error handling and transaction rules

Every formation mutation uses a candidate copy:

1. validate Town/Workbench availability and battle/route locks;
2. validate all three selected IDs;
3. validate the category permutation;
4. project compatibility fields on the candidate;
5. validate the complete runtime/save invariants;
6. commit once.

Failure returns a user-readable reason and performs no partial mutation.

## 11. Acceptance tests

Tests must prove:

- exactly one Hero, Companion, and Quest NPC are required;
- `PositionOrder` is an exact category permutation;
- changing a selected member preserves position order;
- swapping positions preserves all three selected IDs;
- default order is `Hero / Companion / QuestNpc`;
- v24 arrays migrate without losing an unambiguous old order;
- malformed current saves are rejected;
- fixed companions retain progression and have all cards plus valid default decks;
- recruitment, replacement, and dismissal are unavailable;
- route entry projects the selected Quest NPC into route-local fields;
- route settlement clears route-local fields but preserves composition and order;
- battle, Travel, idle strip, and merchant all resolve the same 1P/2P/3P order;
- draft apply/cancel/global close semantics are atomic;
- real PIE and Luna Max show distinct composition and position-order sections with readable category labels.

## 12. Deferred work

- companion level-gated card locking;
- additional Hero acquisition;
- additional companion roles;
- formation respec costs or cooldowns.
