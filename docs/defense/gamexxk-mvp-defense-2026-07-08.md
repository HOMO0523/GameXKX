# GameXXK MVP 答辩文档

更新时间：2026-07-08 10:57 +08:00  
项目路径：`D:\UE5 demo\GameXXK`  
引擎与验证链路：Unreal Engine 5.8、UE MCP、UBT、项目自动化测试

## 1. 项目定位

GameXXK 是一个水墨/HD2D 风格的回合制 RPG MVP。当前版本的目标不是只做静态 playable shell，而是打通一条真实可玩的最小流程：

1. `L_Main` 显示玩家主菜单。
2. 开始游戏进入青山客栈镇子。
3. 玩家使用可编辑蓝图角色 `BP_HeroCharacter`，可移动、可与 NPC 交互。
4. 按 `F` 接任务后任务 NPC 加入跟随状态，并参与存档。
5. 到镇子北侧入口按 `F` 进入路线图。
6. 路线节点进入真实战斗场景。
7. 战斗中右键我方角色弹出技能菜单，选择技能后用墨迹箭头指向敌人并结算。
8. 战斗胜负能返回对应流程，背包、装备、商店、存档能够参与循环。

答辩时可以概括为：本项目已经从菜单、地图、任务、存档、路线、战斗、背包和商店形成了一条闭环 MVP，而不是单张地图或单个 UI 原型。

## 2. 演示流程

建议答辩现场按以下顺序演示：

1. 从 `L_Main` PIE 开始，展示封面主菜单。
2. 点击“开始游戏”，确认真实 OpenLevel 到 `/Game/GameXXK/Maps/L_QingshanInn`。
3. 在镇子中用 WASD 或方向键移动主角，展示 HD2D 角色面片和俯视 45 度视角。
4. 靠近任务 NPC 按 `F` 接任务，观察任务状态变化和任务 NPC 跟随。
5. 按 `I` 打开背包，展示统一背包格、装备槽和道具。
6. 与商人交互，展示商店复用同一套背包/物品体系。
7. 到北侧入口按 `F`，进入独立路线图关卡。
8. 点击路线图战斗节点，进入独立战斗关卡。
9. 右键我方角色，展示技能菜单位置：默认锚点为角色屏幕点加 `X=-500, Y=0`。
10. 点击“普攻”或“鹤羽斩”，移动鼠标展示墨迹箭头跟随鼠标，再点击敌人结算伤害。
11. 展示胜利返回路线图，或失败回城并恢复状态。

## 3. 核心架构

### 场景与流程

- `L_Main`：主菜单入口，不生成可控主角，不显示镇子 HUD。
- `L_QingshanInn`：镇子场景，承载主角、任务 NPC、商人、跟随 NPC、镇子 UI 和北侧路线入口。
- `L_RouteMap`：GameXXK 自有路线图关卡，视觉使用 1Game 风格素材，但 seed、节点状态、点击跳转由 GameXXK C++/UMG 控制。
- `L_BattleScene` / 战斗流程地图：固定 45 度俯视相机，左侧敌人、右侧我方，使用场景 Actor 表现角色和怪物。

### 运行时状态

项目以 `UGameXXKMVPSubsystem` 和 `FGameXXKRuntimeState` 作为 MVP 状态中心。菜单、镇子、路线图、战斗、背包、装备、商店、任务和存档都通过同一套 runtime state 与 rule API 协作，避免 UI 各自维护一套独立状态。

### UI 与输入

- `AGameXXKMVPPlayerController` 统一处理菜单、镇子交互、路线点击、战斗右键菜单和战斗指向输入。
- `UGameXXKBattleBoardWidget` 负责战斗状态、技能按钮、墨迹箭头和目标确认。
- 战斗右键菜单不常驻；右键我方角色才打开，再次右键同一角色会关闭。
- 当前技能菜单调参点：`CommandMenuDefaultOffset(-500, 0)`，位于 `UGameXXKBattleBoardWidget::ResolveCommandMenuAnchor`。
- 墨迹箭头坐标已经从 Slate absolute 鼠标坐标转换为 BattleBoard local 坐标，避免嵌入 PIE 视口中路径偏到画面顶部。

### 存档

存档采用手动保存语义：只有玩家触发保存时才写入。当前保存内容覆盖任务状态、NPC 跟随状态、NPC 位置、玩家进度、背包/装备/金币等 MVP 关键状态。主菜单的 New Game 与 Continue 分离，Continue 面向已有存档槽。

## 4. 关键实现亮点

1. 真实角色蓝图  
   主角使用可编辑 `BP_HeroCharacter`，不是纯 C++ Pawn。角色支持 WASD、方向键、8 向 idle/walk 切换和 `F` 交互。

2. 任务 NPC 跟随  
   任务 NPC 在接任务后进入 follower active 状态，离开一定范围后追随主角，并保持自己的状态和位置存档。

3. GameXXK-owned route map  
   路线图不再依赖原 1Game PlayerController 的测试型逻辑。GameXXK 使用自己的 seed、节点状态、点击状态和关卡跳转，避免路线图重开、节点状态错乱、战斗返回不稳定等问题。

4. 回合制战斗 MVP  
   第一版战斗包含 HP/MP、普攻、鹤羽斩、归元术、防御、金疮药、简单敌人 AI、胜负返回。角色和怪物是场景中的 actor，不是纯 UI 假图。

5. 战斗指向交互  
   右键我方角色打开技能菜单；点击攻击类技能后出现墨迹贝塞尔箭头；箭头起点在选中我方角色，终点跟随鼠标，点击敌人后结算并隐藏。

6. 统一背包/装备/商店  
   背包使用固定格子和整体背板；装备槽与背包不同，装备会影响角色属性；商店复用玩家背包和物品定义，不再是一套独立文本交易系统。

7. 自动化闭环  
   项目用 UE MCP、UBT、`ue_tdd_pipeline.py`、`gamexxk_mvp_playtest.py` 做低负载验证，不使用 UnrealBridge，不使用 Live Coding/Hot Reload 作为编译验证。

## 5. 当前验收证据

最近一次验证命令：

```powershell
python scripts\ue_tdd_pipeline.py --pie-duration 0 --log-lines 80
python scripts\gamexxk_mvp_playtest.py --skip-build --test-timeout 600 --report Saved\HarnessReports\command-offset-green-v2.json
```

验证结果：

- UBT 编译成功。
- UE MCP ready。
- PIE start/stop 成功。
- `GameXXK.MVP` 自动化测试 22/22 success。
- `failed_tests=[]`。
- `flow_coverage.ok=true`。

覆盖的关键验收点包括：

- 主菜单 Start/Continue 流程。
- 镇子 UI、任务 NPC、商人、跟随 NPC。
- 手动 SaveGame 存档语义。
- 镇子入口按 `F` 进入路线图。
- 路线节点进入战斗。
- 战斗 BoardWidget、场景 Actor、回合规则。
- 背包、装备、商店和道具消耗。
- 战斗失败回城与胜利推进。

## 6. 风险与边界

当前 MVP 的重点是流程真实可玩，视觉和数值仍属于第一版：

- 角色、NPC、怪物资源还需要继续统一比例、朝向和脚底贴合。
- 战斗 UI 的布局仍在调参阶段，当前菜单默认偏移为 `(-500, 0)`，后续可继续暴露为配置项。
- 技能表现目前以规则结算和墨迹指向为主，缺少完整动画、特效和音效。
- 路线图 seed、节点池和事件池已经有基础，但内容量和分支深度还需要扩充。
- Git 仓库含 UE 资产，提交和 pack 可能较慢，后续需要定期维护仓库体积。

## 7. 下一步计划

1. 战斗体验  
   增加技能表现、受击反馈、回合提示、死亡表现和结算面板。

2. 数值与怪物配置  
   把怪物属性、技能、掉落和路线节点配置逐步数据化，减少硬编码。

3. 背包与装备 UX  
   增加悬浮说明、右键装备/使用、拖拽、出售数量选择和装备属性对比。

4. 路线图内容  
   扩充事件、商店、营地、精英和 Boss 节点，并做更完整的 seed 复现。

5. 视觉打磨  
   统一 HD2D 相机、角色面片角度、怪物素材裁切、UI 水墨边框和按钮层级。

6. 发布验证  
   增加打包前检查、存档兼容性检查、地图引用检查和资产缺失检查。

## 8. 答辩问答准备

问：为什么不直接复用 1Game 的路线图 PlayerController？  
答：1Game 原逻辑更适合单独测试地图，会在节点点击后重开/刷新自己的地图 UI，和 GameXXK 的主菜单、镇子、存档、战斗返回流程不一致。现在改成 GameXXK-owned route map，只复用视觉素材，逻辑由 GameXXK C++/UMG 管理，流程更稳定。

问：如何证明不是临时 UI shell？  
答：自动化和手测都走真实关卡切换：`L_Main -> L_QingshanInn -> L_RouteMap -> Battle`。镇子中有真实角色蓝图、NPC actor、交互盒、存档状态；战斗中有我方和敌方场景 actor，技能会实际修改 HP/MP 和进度。

问：存档保存了什么？  
答：手动保存记录任务、NPC 跟随状态、NPC 位置、玩家进度、金币、背包、装备、等级、路线进度等 MVP 状态。New Game 与 Continue 分离，避免进入下一关后错误回到第一张地图。

问：战斗当前能玩到什么程度？  
答：当前可以右键我方角色打开技能菜单，选择普攻/鹤羽斩等操作，用墨迹箭头选择敌人，结算伤害、消耗 MP、触发敌人回应，并在胜利或失败后返回对应流程。

问：自动化如何保证稳定？  
答：C++ 通过 UBT 编译；UE MCP 负责启动/停止 PIE 和保存脏包；`gamexxk_mvp_playtest.py` 运行 22 个 `GameXXK.MVP` 自动化测试，覆盖菜单、镇子、路线、战斗、存档、背包、装备和商店。
