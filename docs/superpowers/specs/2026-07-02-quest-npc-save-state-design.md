# Quest NPC Save State Design

Goal:
- Persist the task NPC state for the MVP town flow: not accepted, accepted/following, and the task NPC's last known town position.

Scope:
- Only the task NPC is persisted.
- Merchant NPC and the spare follower NPC keep their fixed spawn locations.
- The existing quest state remains the source of truth for whether a task is not accepted, accepted, or completed.

Runtime State:
- `FGameXXKRuntimeState` keeps `QuestState` and `bFollowerJoined`.
- Add a task NPC location pair: `bHasQuestNpcLocation` and `QuestNpcLocation`.
- New games and not-accepted quests use the fixed task NPC spawn position and leave `bHasQuestNpcLocation=false`.

Save State:
- `FGameXXKSaveState` stores `bFollowerJoined`, `bHasQuestNpcLocation`, and `QuestNpcLocation`.
- Restoring old saves remains compatible: accepted quests restore `bFollowerJoined=true` even if an older save lacks the explicit flag.

Town Flow:
- Pressing `F` on the task NPC when `QuestState=NotAccepted` accepts the quest, activates follower behavior, records the task NPC location, and autosaves.
- While the task NPC follows the hero, movement updates the runtime task NPC location. The NPC throttles disk autosaves so follower movement can survive reload without saving every frame.
- When the town map spawns the task NPC, the GameMode applies the saved location if one exists and reactivates follow targeting when `bFollowerJoined=true`.

Acceptance:
- New/未接任务: task NPC spawns at the default point and does not follow.
- 接任务后: task NPC follows the hero and `Follower: Yes` persists.
- 读档/回城: task NPC restores from the saved position and continues following when the save says follower joined.
