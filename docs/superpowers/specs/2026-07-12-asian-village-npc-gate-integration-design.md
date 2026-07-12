# Asian Village NPC and Gate Integration

## Goal

Move the two user-placed town NPCs from the vendor demo map into the playable GameXXK copy without changing their authored transforms, and use `SM_statue_01` as the visible gate anchor for the existing town-exit interaction.

## Source and Target

- Source map: `/Game/Asian_Village/maps/Asian_Village_Demo`
- Playable target: `/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo`
- Quest NPC: `BP_NpcCharacter`, role `Quest`, transform copied exactly from the source map.
- Merchant NPC: `BP_MerchantCharacter`, role `Merchant`, transform copied exactly from the source map.
- Gate anchor: the corresponding `SM_statue_01` in the target map, identified by mesh identity and the source transform near `(19930, 5240, 1610)`.
- Player start: copy the source `PlayerStart` transform `(20170, 1700, 1592.0007)`, rotation `(0, 0, 0)`, into the playable target map and label it `PlayerStart_QingshanInn`.

## Runtime Ownership

The target map contains the authored NPC actors. `AGameXXKMVPGameMode` recognizes the target map, discovers placed `AGameXXKTownNpcCharacter` actors by role, and only spawns a fallback when a required role is absent. It restores quest/follower state onto the placed quest NPC. This prevents duplicate NPCs and preserves level composition.

The statue remains a visual static-mesh actor. A separate invisible `AGameXXKTownExitActor`, labelled `QingshanInn_TownExit`, is placed at the statue approach area. The existing `F` interaction and route-map transition remain unchanged.

## Safety and Validation

- Save dirty packages through UE MCP before switching maps.
- Do not modify the source NPC transforms, character visuals, camera tuning, or vendor meshes.
- Validate one quest NPC, one merchant NPC, and one town exit in the target map.
- Validate exactly one gameplay PlayerStart and verify its capsule has walkable support without embedding.
- Validate ground support beneath both NPC capsules and the gate interaction approach.
- PIE acceptance: main menu opens the target map at the authored PlayerStart; both NPCs appear; `F` accepts the quest; merchant interaction opens; `F` at the statue enters the route map; no duplicate runtime NPCs are created.

## Disk Constraint

C: has critically low free space. This integration avoids derived-data rebuilds where possible. Cache cleanup or relocation is a separate operation and requires an explicit file list before deletion.
