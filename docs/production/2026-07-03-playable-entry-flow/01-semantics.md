---
unit_id: 2026-07-03-playable-entry-flow
status: verified
owner: codex
updated_at: 2026-07-04T01:27:00+08:00
source_commit: working-tree
depends_on: []
parallel_lock: GameXXK.PlayableEntryFlow
unit_type: gameplay_slice
---

# Semantics

name: Playable entry flow

playable_loop: Main Menu -> Qingshan Town -> F quest accept -> manual save -> north gate F route map -> battle board

required_systems: GameMode defaults, MVP subsystem, player controller UI ownership, Main Menu widget, Town Overlay widget, OneGame Route Map widget, Battle Board widget, quest NPC interaction, town exit interaction, save game

phase_flow: `MainMenu` starts on `L_Main`; `New Game` selects Qingshan and calls `OpenLevel`; town overlay appears on `Town`; quest NPC `F` accepts the quest; `QingshanInn_TownExit` at the north gate changes screen to `DungeonMap` on `F`; route node changes screen to `Battle`

player_actions: click Start/New Game, move with WASD or arrows, press `F` near the quest NPC, click Save, walk to the north gate approach area, press `F` at `QingshanInn_TownExit`, click a route node, right-click enemy in battle

feedback_moments: visible menu, loaded town map, visible town overlay, quest/follower status, persisted NPC location, visible route map, visible battle layout

upgrade_rule: none in this production unit

failure_cases: missing viewport UI, Start click ignored, Start stops on world map, town overlay collapsed, F interaction ignored, save omits NPC/player state, town exit missing, town exit only mutates state without refreshing widgets, battle screen hidden

visual_readability: no debug playable shell in the default player path; route and battle UI may remain programmatic while preserving 1Game-inspired layout contracts

tests_required: C++ automation tests plus real PIE/MCP harness evidence
