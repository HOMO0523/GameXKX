# Permanent NPC Formation and NPC Route-Event Retirement Design

## 1. Goal

Make the Workbench/idle-strip party the only player party throughout the desktop, town, route, battle, save, and reward flows.

The player permanently owns all six named NPCs and always deploys exactly one of them in the three-person party. Entering or leaving town, entering or settling a route, winning, losing, abandoning, saving, loading, or restarting must never clear or silently replace that NPC.

NPC route encounters and the old temporary-NPC-support mechanic are retired. Route encounters consume the party selected before route entry; they never add, invite, replace, unlock, or remove an NPC.

## 2. Confirmed Product Decisions

1. The six named NPCs are permanently owned and available for formation selection. They have no unlock state.
2. A valid modern party always contains the hero, one permanent companion, and one of the six named NPCs.
3. New games default the NPC slot to `Npc.TusiChief`.
4. The party shown by the idle strip is the party sent into the route.
5. A route freezes that party for the route lifetime; route content cannot mutate the persistent formation.
6. All NPC route encounters are removed, including non-roster NPC encounters such as Niu Huan.
7. Non-NPC environmental events and chest events remain. With the current catalog, `Encounter.Event.MountainSpring` is the only ordinary event left until more non-NPC events are authored.
8. The selected NPC survives town map travel, route completion, route defeat, route abandonment, save/load, and process restart.
9. `NPC · 未编入`, temporary support, and invitation UX are invalid in normal gameplay.
10. Runtime functionality is removed while serialization tombstones needed for old-save safety are retained.

## 3. Current Defect and Root Cause

The current implementation mixes two incompatible meanings:

- `OrderedFormation` is documented as the save-authoritative ordered party.
- `ActiveTemporaryQuestNpcId` is a route-provenance field from the older temporary-support design.
- Workbench formation UI reads `ActiveTemporaryQuestNpcId` and prints `NPC · 未编入` when it is empty.
- Route-local cleanup clears `ActiveTemporaryQuestNpcId` and `PartySelection.QuestNpc`.
- Route settlement then treats the NPC formation member as unavailable and replaces that slot.
- The idle-strip travel builder independently falls back to `Npc.TusiChief` whenever the temporary field is empty.

Consequently, the six NPC loadouts and progressions can remain saved while the visible formation says that no NPC is deployed, the idle strip silently shows Tusi Chief, and experience can be assigned to the wrong NPC. This is a gameplay data-authority bug, not a label-only bug.

Town-map travel itself has no legitimate reason to change party membership. Any observed loss across the desktop/town transition is a regression and must be covered explicitly.

## 4. Terminology and Scope Boundary

In this specification:

- **Named NPC roster** means the six permanently owned combat NPC definitions:
  - `Npc.TusiChief`
  - `Npc.SongJinBao`
  - `Npc.YueBai`
  - `Npc.ZhouGuangZu`
  - `Npc.JinGui`
  - `Npc.QiongMeiEr`
- **Persistent formation** means the player's saved hero/companion/NPC selection.
- **Idle-strip party** means the runtime party projection displayed and simulated by the desktop idle strip.
- **Route party snapshot** means the immutable member/card/stat input captured from the persistent formation when a route begins.
- **NPC route encounter** means a route-event catalog entry whose speaker/identity is an NPC. It does not mean an NPC's battle cards.

This unit does not remove the six NPCs, their cards, decks, equipment, progressions, portraits, or battle mechanics. It also does not redesign 3D town actors or the older narrative follower-location state. Those systems must not be conflated with combat formation ownership.

## 5. Single Formation Authority

### 5.1 Persistent source of truth

`FGameXXKCardRunState::OrderedFormation` is the sole save-authoritative party membership source.

A valid modern formation has exactly three unique members:

1. the hero;
2. one valid owned permanent companion;
3. one valid member of the six-NPC catalog.

There is no valid modern empty NPC slot.

`PartySelection.ActivePermanentCompanionInstanceId` and `PartySelection.QuestNpc` are compatibility/loadout projections derived from the ordered formation. They must agree with it but do not independently decide party membership.

`ActiveTemporaryQuestNpcId` is retired from normal runtime authority. It may be read only during old-save migration. Migration clears it to `NAME_None`; the serialized field remains only as a compatibility tombstone. UI, idle simulation, route construction, battle construction, rewards, and experience distribution must not consult it.

### 5.2 Shared party resolver

Introduce one shared formation resolver/builder that:

- validates and resolves the three ordered members;
- resolves the selected NPC's saved three-card loadout;
- resolves level and experience from `QuestNpcProgressions`;
- resolves base attributes from the NPC catalog;
- applies the existing equipment snapshot and talent projection at the appropriate gameplay boundary;
- produces stable member IDs used by every consumer.

The Workbench, idle strip, route entry, battle setup, equipment/attribute presentation, card ownership, and experience awards must use this shared resolution path or a snapshot created by it. No consumer may have its own `Npc.TusiChief` fallback.

### 5.3 Formation changes

Applying an NPC candidate in the Workbench:

1. validates that the candidate is one of the six catalog NPCs;
2. replaces the ordered formation's NPC member;
3. projects that NPC's persisted card loadout into `PartySelection.QuestNpc`;
4. leaves every other NPC's loadout, level, experience, and equipment untouched;
5. atomically replaces the running idle-strip party projection if idle travel is active;
6. leaves the backpack's currently viewed character unchanged unless the user separately selects it.

Replacing the idle-strip party projection must preserve the selected stage, current travel phase, encounter cursor, enemy/wave progress, chest cooldowns, accumulated rewards, and reward ledgers. It is not permission to reinitialize or restart idle travel. Retained members preserve their valid runtime condition; the newly selected NPC enters through the same deterministic party-projection rule used for a normal deployed member.

Formation changes are permitted on the desktop Workbench or town Workbench while no route/battle lock is active. They are rejected without partial mutation while a route or battle owns a frozen party.

## 6. Route Party Flow

### 6.1 Entry

Route entry resolves the current persistent formation through the shared party builder and captures an immutable route party snapshot. The snapshot contains the same member identities, NPC loadout, progression-derived stats, equipment projection, and applicable talent projection that the idle-strip party used at the entry boundary.

The persistent formation remains intact. Route initialization may clear old battle, reward, merchant, relic, and pending-event state, but it must not clear or rewrite party membership.

### 6.2 During the route

- Party membership and loadouts are locked.
- Route events cannot insert or replace any member.
- Chapter transitions preserve the same frozen party.
- Battles and battle rewards use the frozen party identities.
- Experience and other member-specific rewards are attributed to those identities.

### 6.3 Exit and settlement

Victory, defeat, abandonment, cancellation, and terminal settlement clear only route-owned state. They preserve:

- `OrderedFormation`;
- the active companion projection;
- the active NPC loadout projection;
- all six NPC loadouts;
- all six NPC progressions;
- all member equipment.

No settlement repair may replace an NPC merely because the retired temporary-provenance field is empty.

## 7. Town and Desktop Travel

Desktop-to-town and town-to-desktop map travel is presentation travel only.

- The GameInstance-owned runtime state keeps the same ordered formation.
- Workbench session capture/restore may preserve UI state, but it does not own gameplay party state.
- Reconstructed PlayerControllers and Workbench widgets resolve the party from the persistent formation.
- Neither map exit, map entry, widget close, widget reconstruction, nor session restoration may initialize a new game or repair a valid party.

## 8. NPC Route-Event Retirement

### 8.1 Definitions removed from generation

Remove these event definitions from the eligible route-event catalog:

- `Encounter.Event.TusiChief`
- `Encounter.Event.SongJinBao`
- `Encounter.Event.YueBai`
- `Encounter.Event.ZhouGuangZu`
- `Encounter.Event.JinGui`
- `Encounter.Event.QiongMeiEr`
- `Encounter.Event.NiuHuan`

Keep:

- `Encounter.Event.MountainSpring`;
- all existing chest encounter definitions.

The resulting repetition of Mountain Spring is accepted for this unit. Adding more environmental events is separate content work.

### 8.2 Runtime and UX removed

Remove all reachable production behavior for:

- `NpcSupportChoice` creation;
- `TemporaryNpcSupport` choice resolution;
- `AcceptRouteEventNpcSupport` player actions;
- invitation buttons;
- support-slot availability tests;
- support-occupied disabled states;
- `邀请同行`, `已有任务支援`, `临时 NPC`, and equivalent player-facing copy.

The route event panel must present only catalog-approved environmental choices or chest choices.

### 8.3 Serialization tombstones

Do not remove or reorder the serialized numeric slot currently occupied by `EGameXXKRouteEncounterRewardKind::TemporaryNpcSupport`. Mark it hidden/deprecated and ensure no new catalog definition emits it. This prevents the following `Relic` value from being reinterpreted in old data.

Keep a deprecated, unreachable `AcceptRouteEventNpcSupport` compatibility facade for this implementation and one migration boundary. It must return failure and perform no mutation. It is not a gameplay feature. Focused UE reference verification is still required, but physical removal belongs to a later cleanup after the compatibility boundary expires.

## 9. Save Migration and Repair

Implementation introduces one new save migration boundary named `PermanentNpcFormationIntroducedSaveVersion`, immediately after the implementation-time current schema version.

Migration is candidate-based and atomic: validation failure must not partially mutate live runtime state.

### 9.1 Formation recovery order

For an old or malformed save, recover one active NPC in this order:

1. the first valid NPC reference in `OrderedFormation`;
2. a valid `PartySelection.QuestNpc.NpcId`;
3. a valid legacy `ActiveTemporaryQuestNpcId`;
4. `Npc.TusiChief` as the final one-time recovery default.

Then:

- write a valid three-member ordered formation while preserving the valid hero and companion choices;
- project the recovered NPC's owned card loadout into `PartySelection.QuestNpc`;
- normalize all six NPC loadouts and progressions;
- clear `ActiveTemporaryQuestNpcId` to `NAME_None` while retaining its serialized tombstone field;
- validate the complete runtime state before commit.

Tusi Chief is allowed as a new-game default and one-time migration recovery only. It is not a per-consumer runtime fallback.

### 9.2 Pending removed encounters

If an old save is currently inside or has pending one of the seven removed NPC encounters:

1. preserve `SourceNodeId`, `ChoiceSeed`, route progress, screen intent, and the unresolved node;
2. select an eligible non-NPC event deterministically from the saved `ChoiceSeed`;
3. replace the pending encounter identity and presentation identity with that event;
4. clear recruitment/support flags;
5. grant no reward and do not settle the node;
6. leave the persistent and frozen party unchanged.

With the approved current catalog this deterministically resolves to `Encounter.Event.MountainSpring`.

If the non-NPC event catalog is unexpectedly empty, migration clears the invalid pending encounter and returns the player to the route map with the node unresolved. It must not leave the player on an unresolvable event screen or fail the entire save solely because removed content was pending.

## 10. UI Semantics

- The formation page always shows the resolved NPC's portrait and display name.
- All six NPC candidate buttons are enabled whenever formation editing itself is unlocked.
- There is no NPC unlock badge, locked state, recruitment state, or empty state.
- Viewing a character does not change formation.
- Only the explicit formation apply action changes the selected NPC.
- User-facing Workbench copy says `NPC`, not `任务 NPC` or `临时 NPC`, where it describes permanent formation ownership.
- Card-system internal IDs such as `QuestNpc`/`TaskNpc` are not broadly renamed in this unit; that unrelated high-risk refactor is not required to fix player-facing semantics.

## 11. Failure Handling

- Invalid candidate selection is rejected without changing formation or the idle runner.
- An invalid modern NPC reference is repaired only at load/new-game normalization boundaries, not silently on every UI refresh.
- A shared party-resolution failure prevents route entry or runner rebuild and reports failure; it does not substitute Tusi Chief behind the player's back.
- A failed idle-runner rebuild leaves both persistent formation and the previous valid runtime runner unchanged.
- A route-event migration failure follows the safe route-map fallback described above.
- The deprecated support facade always fails without state mutation.

## 12. Automated Acceptance

### 12.1 Ownership and formation

- New game has exactly six persisted NPC loadouts and six NPC progressions.
- Every catalog NPC is selectable without an unlock predicate.
- New game resolves Tusi Chief as the active NPC.
- Switching to each of the six NPCs updates the ordered formation and active loadout projection while preserving the other five NPC records.
- A valid modern state cannot resolve an empty NPC member.

### 12.2 Shared identity and data

- Formation UI NPC identity equals the shared formation resolver's NPC identity.
- `TrainingTravelRuntime.PartyUnits[2].UnitId` equals that identity.
- Route party/battle NPC identity equals the frozen entry identity.
- NPC level, experience, equipment owner, selected cards, and awarded experience all belong to the same identity.
- Removing private consumer fallbacks cannot change the selected NPC.
- Switching NPC during active idle travel changes only the party projection and does not reset stage, phase, encounter/wave progress, chest timers, accumulated rewards, or reward ledgers.

### 12.3 Lifecycle preservation

For a non-default selection such as Yue Bai, verify identity preservation across:

- desktop to town;
- town to desktop;
- repeated map travel;
- route entry;
- ordinary events;
- chapter transition;
- route clear;
- defeat return;
- abandonment return;
- save/load;
- process restart restore.

Formation editing is rejected during the route and succeeds again afterward without changing the previously selected NPC.

### 12.4 Event retirement and migration

- The eligible ordinary event catalog contains none of the seven removed IDs.
- No approved encounter choice emits `TemporaryNpcSupport`.
- Environmental event and chest resolution still work.
- The deprecated enum value retains its serialized ordinal and relic data remains correctly interpreted.
- Each removed pending encounter migrates deterministically without reward, settlement, party mutation, or stuck UI.
- The deprecated support facade returns failure and preserves a byte/struct-equivalent state.
- Source/UI policy tests find no reachable invitation/support copy or action binding.

### 12.5 Corrupt and legacy saves

- Each recovery source in the specified priority order is exercised.
- A fully empty/corrupt NPC selection repairs once to Tusi Chief.
- After migration, a second normalization is idempotent and does not re-repair or change identity.
- All six loadouts, progressions, and equipment ownership survive migration.

## 13. PIE Acceptance

On the canonical `/Game/GameXXK/Maps/L_DesktopTrainingHUD` flow:

1. select Yue Bai in the formation page and apply;
2. confirm the idle strip immediately shows Yue Bai;
3. enter the 3D town only for this explicitly scoped town-travel regression, then exit back to the desktop;
4. confirm Yue Bai remains selected;
5. start a route and confirm the third party member is Yue Bai;
6. finish or abandon the route and confirm the idle strip still shows Yue Bai;
7. save, close, restart, and load; confirm Yue Bai remains selected;
8. switch to another NPC and repeat the key identity checks.

Any automatic Tusi Chief fallback, empty NPC, route-event replacement, wrong experience recipient, input blockage, or failure to return to a usable Workbench fails acceptance.

## 14. Non-Goals

- Authoring replacement environmental route-event content.
- Reworking the six NPC card designs or 4-of-3 loadouts.
- Reworking NPC equipment, progression curves, talents, or battle animations.
- Reworking the permanent companion roster.
- Reworking 3D town visuals, town actor placement, narrative follower movement, or quest storytelling.
- Adding the postponed story/task panel beyond the existing inert placeholder button.
- Broadly renaming serialized `QuestNpc`/`TaskNpc` types and CardIds.

## 15. Completion Standard

The implementation is complete only when:

- one persistent formation authority drives every party consumer;
- all six named NPCs remain permanently available;
- the selected NPC survives every approved lifecycle boundary;
- all NPC route encounters and reachable temporary-support behavior are gone;
- old saves migrate without party loss, unintended rewards, enum reinterpretation, or stuck route-event UI;
- focused automation, cold UBT, and the scoped PIE flow pass with evidence;
- unrelated user-tuned assets and existing working-tree changes remain untouched.
