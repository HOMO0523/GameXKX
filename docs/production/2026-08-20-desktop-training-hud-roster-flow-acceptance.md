---
status: implemented
owner: codex
updated_at: 2026-08-21T13:09:02+08:00
source_commit: 91786825ee8040cbb4683fbf7798998e1f26210f
working_tree: clean for this scope after documentation follow-up; unrelated user files remain untracked
---
# 桌面历练 HUD 角色入口、编队与直接挑战验收

本记录只验收 2026-08-20 用户明确要求的 HUD 流程纠错，不把整个桌面历练产品或其余未确认美术宣称为完成。

## 冻结裁决

- 禁止继续使用用户否决的星点长条底、`T_MasterV2_ButtonNeutral` 同类底，以及通用 `T_MasterV2_TabNormal/Selected` 代用品。
- 属性、装备（背包）、卡组三个既有页签的正确双态资源是唯一状态基准：未选中 `003_tab_1`，选中 `004_tab_2`。
- 主角、伙伴、NPC 三个入口固定在中栏内容区左下，必须显示真实角色头像；入口只改变查看对象，不得顺便换队。
- 编队是独立页面。伙伴或 NPC 只有在玩家明确点击“编入队伍”后，才可写入权威队伍槽。
- 挑战不依赖青山镇任务、不隐式接任务，也不要求先经过城镇入口；当前已解锁关卡可直接挑战，已通关关卡允许重复挑战。既有关卡与难度解锁顺序保持不变。

## 实现结果

- 中栏新增独立编队页；底部导航使用单一焦点状态，进入页面本身不修改队伍。
- 角色入口移到中栏左下并改用真实头像；伙伴/NPC 候选层向上展开。
- 查看角色与写入队伍已拆成两条动作路径；查看宋金宝时，当前任务 NPC 仍保持土司首领，只有“编入队伍”才会改变队伍。
- 挑战按钮绕过任务前置，直接创建/复用共享 RouteMap 与 BattleBoard 并进入真实 `Battle`；修复 HUD-only 启动时共享战斗层尚未创建导致的黑屏。
- 按钮文本禁止自动换行，`排序`、`伙伴`、`编入队伍` 等在三种验收分辨率下保持单行。
- `gamexxk_probe_training_visual_mvp.py` 增加 HUD、任务、战斗状态及 `open-backpack` 探针。

## 验收矩阵

| 条件 | 结果 | 证据 |
|---|---:|---|
| 三个角色入口位于中栏左下且不压顶部页签、排序或底部导航 | PASS | 三分辨率背包截图 |
| 入口使用真实头像且状态底仅为 `003_tab_1` / `004_tab_2` | PASS | 视觉复核 + `CharacterRosterPlacementAndViewIsolation` |
| 查看伙伴/NPC 不修改永久伙伴、任务 NPC 或 Travel 队伍 | PASS | Automation + 真实点击查看宋金宝后顶部队伍仍为土司首领 |
| 编队为独立中栏页面 | PASS | 三分辨率编队截图 |
| 只有“编入队伍”写入对应权威队伍槽 | PASS | Automation + 真实点击链 |
| 底部入口最多一个高亮 | PASS | Automation + 三分辨率视觉复核 |
| 未接任务可直接挑战且任务状态不变 | PASS | `QuestState=NOT_ACCEPTED`，点击后 `Screen=Battle`、`training_challenge_battle_active=true` |
| 不写入地图、Sprite、PaperZD、镜头或 HD2D Plane | PASS | `L_Main.umap` 哈希复核；工作树无上述资产改动 |

## 自动化与编译

- 冷 UBT：`GameXXKEditor Win64 Development -NoHotReload -NoHotReloadFromIDE -NoUBA -MaxParallelActions=2`；`Result: Succeeded`，2026-08-20 21:08，`C:/Users/qlma/AppData/Local/UnrealBuildTool/Log.txt`。
- `GameXXK.Training`：21/21，0 failed、0 error；`Saved/Automation/TrainingClearedReplay-20260820/index.json`，报告 `Saved/HarnessReports/20260820-204839-ai-production-loop.md`。
- `GameXXK.DesktopTraining`：30/30，0 failed、0 error；`Saved/Automation/DesktopTrainingHudRosterFinalNoWrap-20260820/index.json`，报告 `Saved/HarnessReports/20260820-210929-ai-production-loop.md`。
- 非失败 warning 仅为已知的部分可选 1K atlas 缺失与离线 EOS 诊断，不影响本纠错包结论。

## 视觉与真实点击证据

三分辨率背包/编队：

- `Saved/Codex/hud_final2_backpack_1672x941.png`
- `Saved/Codex/hud_final2_backpack_1920x1080.png`
- `Saved/Codex/hud_final2_backpack_2560x1440.png`
- `Saved/Codex/hud_final2_formation_1672x941.png`
- `Saved/Codex/hud_final2_formation_1920x1080.png`
- `Saved/Codex/hud_final2_formation_2560x1440.png`

真实点击链：

- `Saved/Codex/hud_final_accept_npc_1672x941.png`
- `Saved/Codex/hud_final_accept_training_1672x941.png`
- `Saved/Codex/hud_final_accept_battle_1672x941.png`

Automation HighResShot 不包含 Slate，`Saved/VisualReview/.../hud_*` 黑图不得作为视觉证据。

## 保护检查

- 活动分支：根项目 `main`，未创建 worktree，未使用 UnrealBridge、Live Coding 或 Hot Reload 做验证。
- `Content/GameXXK/Maps/L_Main.umap` SHA256：`20BC157561D1BBA58D57E0B679DBD66BD0BD2515DDBC3EEF9B5AC40948A0827E`，与本包开始时一致。
- 本包未修改或保存地图、角色 Sprite、PaperZD、放置关卡、镜头或 HD2D Plane。
