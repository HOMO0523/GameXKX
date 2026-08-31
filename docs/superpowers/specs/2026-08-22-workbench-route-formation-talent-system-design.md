# GameXXK Workbench Parent, Route Choice, Formation, and Talent System Design

Date: 2026-08-22
Status: user-approved design, awaiting written-spec review
Canonical surface: `/Game/GameXXK/Maps/L_DesktopTrainingHUD`

## 1. Objective

Evolve the pure-2D desktop flow without replacing its approved paper-and-ink assets or returning to a 3D scene. The delivered system must:

1. Keep the backpack as the persistent Workbench parent while giving every subordinate surface a correct, consistent close action.
2. Make route events a real three-card choice flow and connect the in-route merchant as a card-upgrade shop.
3. Replace the placeholder formation page with an ordered, future-proof `1P / 2P / 3P` party editor.
4. Replace the placeholder talent grid with a permanent, data-driven, 35-layer talent graph whose effects are consumed by real combat, idle, offline, chest, capacity, and tool rules.

The implementation is split into four independently testable production units and must preserve all unrelated user-tuned assets, maps, PaperZD content, character art, and camera/layout calibration.

## 2. Evidence and current gaps

The user supplied four visual references:

- Reference 1 marks a centered route-event modal, three full selectable cards, per-card art/icon regions, and a top-right close affordance.
- Reference 2 marks the existing Workbench and route-map-inspired node/connection arrangement.
- Reference 3 supplies the interaction model for a large branching talent/rune graph with node details and ranks; its dark pixel skin is reference-only.
- Reference 4 supplies hierarchy and selection-state ideas for an orderly roster/formation editor; its portrait/mobile layout is reference-only.

The historical visual review is recorded at `Saved/HarnessReports/20260822-workbench-parent-event-talent-formation-luna-max.md`.

Targeted code inspection found:

- The Workbench currently rebuilds one large widget tree and uses mixed central/left/right page state.
- Clicking an occupied embedded-backpack slot synchronously rebuilds the parent widget tree while the Slate click callback is active. The outer backpack remains logically expanded, but the newly embedded backpack can become blank. Clicking an empty slot does not reproduce this because it does not request the rebuild.
- Route event rules already expose up to three deterministic choices, but the UI presents them as small text buttons rather than full reward cards.
- A dedicated merchant widget and route merchant state exist, but the current merchant is authored around relic offers rather than the newly approved four carried-card upgrade targets.
- The talent page is twelve fixed placeholder rectangles and has no authoritative talent state.
- The formation model is an implicit fixed hero plus one permanent companion and one task NPC. It cannot represent multiple heroes or arbitrary `1P / 2P / 3P` order.

## 3. Approved architecture and delivery order

Use incremental, layered delivery rather than adding more behavior directly to the monolithic Workbench or rewriting every player-flow widget at once.

Delivery order:

1. **Unit A — Workbench parent and close stack.** Establish stable parent/child lifetimes and fix the embedded-backpack synchronous rebuild bug.
2. **Unit B — Route choices and merchant.** Finish the first playable route event/shop loop using reusable card faces.
3. **Unit C — Ordered formation.** Introduce the persisted generic party order and migrate current selections.
4. **Unit D — Permanent talent graph.** Add the graph, progression catalog, rule projections, capacity gates, offline rules, and chest schedule.

Shared visual primitives may be extracted for approved panel frames, close buttons, selectable card faces, graph nodes, graph lines, and state borders. Route, talent, and formation rules remain separate; a UI primitive must not own gameplay state.

## 4. Unit A — Backpack parent and close stack

### 4.1 Parent lifetime

The Workbench and backpack content are the persistent parent. Opening, closing, refreshing, sorting, equipping, or dragging an item must not synchronously destroy the parent tree from inside the originating Slate input callback.

Embedded backpack left-click and right-click entry points run under the same action-callback guard already used by native Workbench buttons. A requested structural refresh is deferred until the next tick. The item operation commits or enters carry state first; the parent refresh occurs only after the mouse callback has returned.

### 4.2 Page relationships

| Surface | Relationship | Local close behavior |
|---|---|---|
| Formation | Replaces the central backpack content | Return central content to Backpack |
| Talents | Replaces the central backpack content | Return central content to Backpack |
| Warehouse | Independent left rail | Close Warehouse only |
| Tools | Independent right rail | Close Tools only |
| Training | Independent right rail | Close Training only |
| Route event | Overlay owned by route flow | Return to route map; node remains unresolved |
| Camp | Overlay owned by route flow | Return to route map; node remains unresolved |
| Merchant | Dedicated route node surface | No `X`; only `Leave Merchant` completes and exits |
| Route map | Top-level active route | Open settlement confirmation |
| Battle | Existing battle surface | Preserve retreat-confirmation semantics |

Workbench local close buttons reuse `/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_CloseInk` and are anchored in their panel's local coordinates. They do not use desktop, world, or cross-window coordinates.

### 4.3 Global Backpack/Tab close

The Backpack parent's top-right `X` and the global `Tab` input are equivalent global-close actions:

The controls are visually separate. The `X` is anchored inside the Backpack paper at its top-right corner. The top Tab always keeps the project tab skin: selected background with `▲` while expanded, normal background with `▼` while collapsed. CloseInk is never drawn in the Tab container.

1. Cancel any transient carried item without mutating inventory.
2. Return all transient tool reservations to their authoritative containers.
3. Close Warehouse.
4. Reset the central page to Backpack.
5. Close Tools and Training.
6. Clear active child navigation state.
7. Collapse the Backpack parent while leaving the idle/Travel strip running.

Reopening through `Tab` starts from a clean Backpack page. It must not restore the previously open Warehouse, Formation, Talents, Tools, or Training combination. Persistent gameplay data remains intact; only transient presentation/navigation state resets.

## 5. Unit B — Route event cards and card-upgrade merchant

### 5.1 Event presentation

Route events remain overlays above a visible, dimmed route map. Each event renders three equal-width, full-card hit targets using approved card/paper assets. Every card includes:

- art or an explicit reward icon;
- reward/choice name;
- full effect description;
- availability or disabled reason;
- selected/unselected state.

Clicking a card changes only the selected choice. A separate bottom `Confirm` action atomically resolves the chosen reward and completes the node. The event `X` closes the overlay and returns to the route map without resolving the node. Reopening the same pending node returns the same deterministic three options. Camp retains its own rest/supply interaction and is not forced into the three-card template.

Camp reward semantics are superseded by `2026-08-23-life-saving-charm-and-workbench-polish-design.md`: Camp offers the unique `保命护符` or 100 route-local travel money, never healing powder or direct healing.

### 5.2 Merchant entry and exit

The merchant remains an independent route-map node. It has no top-right `X`. The only exit is `Leave Merchant`, which cancels any uncommitted transient selection, completes the merchant node, and returns to the route map.

Purchased upgrades commit immediately. Closing/reopening the app or re-entering the merchant through a saved unresolved state must preserve stock, sold state, refresh count, ordinary gold, and upgraded card qualities.

### 5.3 Four carried-card offers

Merchant stock is selected deterministically from the combined carried-card pool of current `1P`, `2P`, and `3P`:

- include the configured carried cards of each deployed member;
- exclude cards already at maximum quality;
- exclude duplicate card IDs;
- retain owner/member provenance for display and validation;
- show at most four offers;
- show a disabled `No upgradable card` placeholder for missing slots.

Each offer card shows its owner, current quality, next quality/effect preview, and ordinary-gold price. Each of the four offers may be purchased once, and the player may buy multiple offers when funds permit. Purchasing deducts the existing permanent/idle ordinary-gold balance (`PlayerGold`) and upgrades the authoritative carried card from `Common -> Rare -> Epic`; it does not create a new or temporary route card. Merchant purchase and refresh never deduct route-travel money.

Refresh rerolls unpurchased eligible targets and uses the existing increasing route-merchant refresh-price progression. Purchased slots remain visibly sold. Purchase and refresh operations are transactional: insufficient money, stale ownership, changed quality, or an exhausted pool does not partially mutate state.

## 6. Unit C — Ordered `1P / 2P / 3P` formation

> **Superseded:** Unit C is fully specified by
> `2026-08-23-decoupled-party-formation-design.md`. Its authoritative model
> stores exactly one selected Hero, one fixed persistent Companion, and one
> persistent Quest NPC separately from a `1P / 2P / 3P` category permutation.
> The prior generic ordered-member array and mixed select/reorder interaction no
> longer apply.

## 7. Unit D — Permanent 35-layer talent graph

### 7.1 Presentation and interaction

Talents replace the Backpack's central content and have a local `X` that returns to Backpack. The approved paper/map skin remains; reference-image dark pixel styling is not imported.

The main surface is a two-dimensional pannable graph. A center root fans into four directions at 45-degree angles. Lines render behind nodes. A fixed detail rail displays selected-node name, rank, next effect, prerequisite, ordinary-gold balance, price, and upgrade action without covering the graph.

Hidden successors are not drawn until their predecessor is purchased. Revealed but unavailable nodes show their lock reason. Nodes expose rank as `current / maximum`; ordinary repeatable nodes have five ranks. There is no respec or refund in this delivery.

### 7.2 Root and four one-time entries

The center root and each first branch entry cost 2,500 ordinary gold and can be purchased only once.

| Node | One-time effect |
|---|---|
| Center root | Reveal four branches and unlock Warehouse page 2 (Warehouse starts at page 1) |
| Upper-left combat entry | Party Attack +5, MaxHP +5, Defense +5 |
| Lower-left capacity/chest entry | Backpack capacity +5 (new game goes from 20 to 25) and reveal capacity/chest tracks |
| Upper-right idle/offline entry | Unlock offline rewards and reveal online/offline gold, experience, time, and chest tracks |
| Lower-right tools entry | Unlock Dismantle, Combine, Enhance, Reforge, and Socket together |

### 7.3 Unified price curve

All ordinary talent nodes use a maximum depth of 35 and the same price rule:

```text
price(costTier) = round_to_nearest_100(2500 * 1.35 ^ costTier)
```

The root and four one-time entries override `costTier` to zero and cost 2,500. The first repeatable node after an entry uses `costTier = 1` (3,400 after rounding). Every rank of the same node costs the same price. A deeper node has a larger `costTier`; the price does not depend on how many ranks were purchased in the current node.

The 35-node capacity path from Backpack 25 to 200 costs approximately 1,757,301,500 ordinary gold when fully ranked, excluding the root/entry and milestone Warehouse-page nodes. Currency and price calculations use 64-bit integers.

### 7.4 Branch density

- Capacity/chest and idle/offline rewards are full 35-layer long branches with many fine-grained parallel tracks.
- Combat and tools are short branches of approximately ten layers (about one quarter of a long branch) and sit near the center so players can finish their foundations earlier.
- The graph catalog remains data-driven, but no authored node may exceed the branch, rank, cost-tier, or aggregate caps below.

### 7.5 Combat branch effects and caps

Fixed party-stat nodes grant +5 per rank and are hard-capped independently:

- Attack: +200 maximum from talents.
- MaxHP: +200 maximum from talents.
- Defense: +200 maximum from talents.

In-route percentage nodes grant +2 percentage points per rank and are hard-capped independently:

- Attack percentage: +100% maximum.
- Final damage percentage: +100% maximum.
- Defense percentage: +100% maximum.
- MaxHP percentage: +100% maximum.

Additional caps:

- Critical chance: 20% maximum from talents.
- Critical damage bonus: +50% maximum from talents.
- Idle movement speed is one five-rank node that changes the real inter-encounter Walking duration: `5.0 -> 4.5 -> 4.0 -> 3.5 -> 3.0 -> 2.5 seconds`.

The five-second value remains the zero-rank baseline. Even at 2.5 seconds, the full visible Walking interval begins only after prior hit/death presentation has finished; enemies, combat party sprites, and health bars remain hidden during it.

### 7.6 Backpack and Warehouse capacity

- New-game Backpack logical capacity starts at 20.
- The capacity entry increases it to 25.
- Thirty-five five-rank capacity nodes grant one Backpack slot per rank, reaching 200.
- Backpack physical capacity remains 200.
- Warehouse starts with one 36-slot page.
- The center root unlocks page 2.
- Reaching Backpack total capacity 50, 100, 150, and 200 reveals one-time nodes for Warehouse pages 3, 4, 5, and 6 respectively.
- Warehouse physical capacity remains 200; unused cells on the final page stay locked/absent.

An old save migrates with enough logical Backpack capacity and Warehouse pages to contain every existing occupied physical slot. Migration never deletes, overwrites, or silently moves an item. With no respec, purchased capacity never shrinks.

### 7.7 Online and offline reward tracks

Each of the following has its own complete 35-layer track. Every node has five ranks and grants +2% per rank, so every track reaches an additional +350% at completion:

- online idle gold;
- online idle experience;
- offline idle gold;
- offline idle experience;
- normal-chest relative drop-rate multiplier;
- advanced-chest relative drop-rate multiplier;
- offline gold accumulation-time multiplier;
- offline experience accumulation-time multiplier.

Offline gold and experience accumulation start at 24 hours. Their completed +350% time tracks produce a final maximum of 108 hours (`24 * 4.5`). Reward amount multipliers and accumulation-time multipliers are separate and cannot substitute for each other.

### 7.8 Offline chest accumulation time

Offline chest accumulation starts at eight hours. It uses a separate 35-layer track whose nodes have five ranks; each rank adds three minutes. A node adds fifteen minutes when fully ranked. The completed track adds 525 minutes, producing a final maximum of 16 hours 45 minutes.

### 7.9 Chest qualification windows and drops

Normal and advanced chests each have a base drop chance of 25%. Talent drop-rate bonuses are relative multipliers, not additive percentage points:

```text
effectiveChance = clamp(baseChance * (1 + talentBonus), 0%, 100%)
```

Thus +100% changes 25% to 50%; +350% calculates 112.5% and clamps to 100%.

Chest time windows grant roll eligibility only. They never grant a chest directly.

- While the game is online, regardless of session duration: normal window 2 minutes, advanced window 3 minutes.
- Offline simulation uses normal 4-minute and advanced 6-minute windows, bounded by the unlocked offline chest accumulation time.

On encounter completion:

- a normal wave checks only an eligible normal-chest roll;
- an elite wave checks only an eligible advanced-chest roll;
- a Boss wave checks only an eligible advanced-chest roll and is not guaranteed to drop one.

If a window is not yet eligible, encounter completion performs no roll and does not reset its timer. If eligible, the deterministic roll consumes that eligibility and resets only the matching chest timer.

### 7.10 Tool branch

The tool entry unlocks all five tool modes at once. Two short parallel tracks then improve tool-only rewards:

- Tool experience gain: +5% per rank, five ranks per node, ten layers, +250% maximum.
- Tool gold gain: +5% per rank, five ranks per node, ten layers, +250% maximum.

Tool experience affects only tool level. Tool level affects only the item level of newly crafted equipment. Tool gold affects only gold produced by tool operations such as dismantling. Neither modifier affects combat stats, online/offline idle rewards, route settlement, quest rewards, or merchant currency.

### 7.11 Talent transaction rules

An upgrade is transactional:

1. Re-read authoritative ordinary gold and current rank.
2. Validate reveal state, prerequisites, rank maximum, aggregate cap, and price.
3. Deduct ordinary gold and increment rank in one commit.
4. Recompute the shared talent projection.
5. Refresh the selected node, newly revealed successors, and affected Workbench values.

Failure leaves gold, rank, capacities, timers, and projections unchanged. UI disabled state is advisory; rules revalidate every request.

## 8. Route-map top-level settlement

The route map's top-right `X` does not immediately abandon the route. It opens a settlement confirmation that previews currently earned rewards, ordinary-gold conversion, route progress, and anything forfeited by ending early.

- `Cancel` closes the confirmation and stays on the route map.
- `Confirm Settlement` uses the authoritative route settlement transaction, awards only earned rewards, clears the active route, returns to `Town` on `L_DesktopTrainingHUD`, and restores the 2D idle/Travel Workbench.
- Confirming cannot load a 3D map, reopen a legacy town flow, or duplicate a settlement receipt.

## 9. Persistence and migration

The next available save-version migration adds:

- ordered three-member formation;
- talent node ranks;
- logical Backpack capacity and unlocked Warehouse pages;
- tool-unlock and tool-progression state;
- chest-window state required for deterministic continuation;
- route merchant carried-card offers and sold/refresh state where not already represented.

Rules normalize malformed or out-of-range data. Aggregate talent effects are derived from node ranks and are not separately serialized as a second source of truth. Old inventory arrays and authored card/companion/NPC data remain authoritative and are not recreated by migration.

## 10. Verification and acceptance

### Unit A

- RED/GREEN automation proves occupied embedded-backpack left/right clicks defer parent rebuild and never blank the backpack.
- Local close behavior matches the relationship table.
- Backpack `X` and `Tab` close every Workbench child, cancel carry/tool reservations, collapse to the idle strip, and reopen to a clean Backpack.
- Parent layout build count does not change every Travel tick.

### Unit B

- Event choices render as three full cards, persist deterministically, select without committing, commit only on Confirm, and remain unresolved after `X`.
- Merchant has no `X` and exits only through Leave Merchant.
- Merchant creates up to four unique, owner-labelled, non-max carried-card offers from `1P/2P/3P`.
- Multiple purchases, increasing refresh price, sold state, save/load, insufficient funds, stale quality, and fewer-than-four candidates are covered.
- Real PIE proves route event -> confirm -> route map -> merchant -> multiple card upgrades -> leave -> route map.

### Unit C

- Formation migration preserves current Hero/Companion/NPC order.
- Swap, replace, draft discard, atomic apply, duplicate rejection, at-least-one-hero validation, and save/load order are covered.
- Idle strip, Travel attack order, battle order, and merchant offer ownership follow `1P / 2P / 3P`.
- Companion labels expose only the six approved profession names.

### Unit D

- Catalog validation proves one root, four 45-degree entries, maximum depth 35, valid prerequisites, no cycles, five-rank ordinary nodes, and valid cost tiers.
- Price tests cover 2,500 root/entries, first repeatable price 3,400, per-node equal rank price, 1.35 depth progression, 64-bit totals, and the complete capacity path.
- Rule tests cover every fixed/percentage/critical cap, movement-speed delay table, 200 Backpack slots, Warehouse page milestones, +350% reward tracks, 108-hour offline gold/experience cap, 16:45 offline chest cap, and +250% tool tracks.
- Chest tests prove online duration never degrades the normal-2/advanced-3-minute windows, offline simulation uses normal-4/advanced-6-minute windows, and cover 25% relative chance, 100% clamp, type-specific encounter settlement, and non-guaranteed Boss rolls.
- Save migration proves occupied items are retained without overwrite or movement.
- Real PIE and Luna Max screenshots prove the tree fans from the center, lines remain behind nodes, selections/details are readable, no child panel loses its close affordance, and the Workbench paper/ink layout remains unchanged outside the requested surfaces.

## 11. Explicit non-goals

- Do not copy the dark pixel skin from the talent reference or the vertical/mobile skin from the formation reference.
- Do not generate replacement UI art, text, character portraits, or icons with image generation.
- Do not restore the retired temporary route-card system.
- Do not make merchant upgrades create new cards.
- Do not permit talent effects to exceed rule-layer caps.
- Do not use Live Coding or Hot Reload for verification.
- Do not load a 3D town as a routine development or acceptance surface.
- Do not edit protected maps, PaperZD assets, character sprites, or manually tuned HD2D values.
