---
unit_id: 2026-07-03-playable-entry-flow
status: active
owner: codex
updated_at: 2026-07-03T00:00:00+08:00
source_commit: working-tree
depends_on: []
parallel_lock: GameXXK.PlayableEntryFlow
---

# Raw Input

User acceptance request:

> 仿照自动化流程迁移到这个项目中，然后验收：L_Main 进 PIE 有主菜单、Start 可点击并 OpenLevel 到镇子、镇子 UI 可见、F 接任务、NPC 状态保存、进入小地图和战斗图都由按钮真实切换。

Observed failure from manual PIE:

- `L_Main` showed only the hero on a black background, no main menu.
- `L_QingshanInn` had to be opened manually; map switching was not happening from UI.
