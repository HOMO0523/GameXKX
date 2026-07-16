# PSD HUD 与世界地图重构设计

## 目标

将 GameXXK 的玩家 UI 重构为与交付 PSD 一致的纸墨武侠风格，同时保持所有游戏数据、点击、快捷键、存档、路线和战斗交互可用。玩家入口固定为：主菜单 → 世界地图 → 点击城镇 → 城镇 HUD → 江湖路线 → 战斗。

## 已确认的输入与约束

- PSD 权威产物位于 `C:/Users/shxuw/Downloads/nw-studio-nwueball-https-github-com/nw-studio-nwueball-https-github-com`。
- `work/psd_rebuild/manifest.json` 定义 4096×4096 布局；透明 HUD 原子层可直接使用，所有文字必须保持为运行时 UMG 文本。
- `clean_assets_v2/094.png` 是不透明的完整地图，已烘焙路线、圆点与骷髅节点，不能作为动态地图的最终运行时背景。
- 现有仓库中的 HUD、背包与导入资产处于未提交状态；本设计不得覆盖或回退这些改动。
- 依项目规则，所有工作在根项目 `main` 上完成；不得使用 UnrealBridge、Live Coding 或 Hot Reload 验证。

## 范围

### 包含

1. 主菜单、世界地图、城镇、江湖路线与战斗的完整玩家流。
2. 常驻城镇 HUD，以及背包、商店、任务、任务对话、角色、伙伴和设置入口的统一视觉与可用交互。
3. 世界地图、江湖路线与战斗 HUD 的 PSD 风格化与动态状态呈现。
4. 地图分层资产生产、UE 导入和运行时合成。
5. 已发现的生产 UI 功能缺口：城镇地图入口、世界地图展示、背包材料/任务筛选、分解行为、角色/伙伴面板、战斗板公开 stub。
6. 冷编译、自动化测试、PIE 主流程、存档恢复和截图验证。

### 不包含

- 制作尚不存在的第二座可进入城镇关卡。首版只有青山镇具有可进入关卡目标；其他地点可根据 `UnlockedRegions` 显示“已发现”，但仍禁用点击并明确标注“暂未开放”，不伪造转场。
- 改动用户调过的角色 Sprite、PaperZD、已放置关卡物体、相机或 HD2D 平面参数。

## 玩家流与状态

```text
主菜单 StartGame
  -> RuntimeState.Screen = WorldMap
  -> L_Main 上显示世界地图 UMG
  -> 点击青山镇（SelectWorldRegion(Qingshan)）
  -> RuntimeState.Screen = Town，关卡流转至 L_Qingshan_AsianVillage_Demo
  -> 城镇 HUD：任务 / 背包 / 角色 / 伙伴 / 地图返回 / 江湖入口
  -> 北门或江湖入口：DungeonMap，流转至 L_RouteMap
  -> 路线节点：RouteEvent / RouteCamp / RouteMerchant / Battle
  -> Boss 结算：WorldMap，并更新解锁状态
```

`UGameXXKMVPSubsystem::StartGame()` 已正确停留在 `WorldMap`；问题在于 `UGameXXKMainMenuWidget::StartGame()` 随后立即调用了 `SelectWorldRegion(Qingshan)`。重构移除该第二次调用，不改变存档数据模型。继续游戏按存档里的 `RuntimeState.Screen` 恢复；若存档在城镇或路线中，保持既有恢复语义。

## 视觉与资产契约

### HUD 原子层

来自 PSD 的头像、导航图标、资源图标、加号、邮件、设置和经验框导入为 RGBA `Texture2D`，统一置于 `/Game/GameXXK/UI/PSD/HUD`。名字、等级、战力、货币数值、经验数值、菜单标签和提示一律由 UMG `TextBlock` 渲染。

所有可拉伸的纸张窗口、按钮和条框使用九宫格画刷；填充条使用 `UProgressBar`，不得把示例数值或静态满条烘焙进贴图。缺失的非文字原子层按用户给定的 ImageGen 阶段 1 规则生成透明底图，再经 manifest、导入脚本和 UE 纹理验证进入项目。

### 世界地图与江湖路线拆层

| 层 | 世界地图 | 江湖路线 |
| --- | --- | --- |
| 静态地形 | 无节点、无路线、无文字的山水纸张底图 | 无节点、无路线、无文字的纸墨地形底图 |
| 路径 | 城镇/区域关系的透明路线层（若需要） | 根据 `RouteMapEdges` 生成的透明/程序化连线 |
| 交互对象 | 城镇节点、锁定标记、当前城镇标记 | 起点、战斗、精英、事件、篝火、宝箱、商店、Boss 节点 |
| 动态信息 | 城镇名、章节、锁定说明、玩家标记 | 节点状态、奖励/事件文字、路线进度、玩家标记 |

不得用魔棒或局部裁剪把 `094.png` 中烘焙的骷髅、圆点或虚线路径伪装为动态层。应通过生成/编辑获得同几何比例的干净地形底图、透明路线层与透明节点状态层。现有透明 `095.png` 玩家头像节点可复用。

### 各屏幕布局职责

| 屏幕 / 组件 | 视觉职责 | 功能职责 |
| --- | --- | --- |
| `UGameXXKMainMenuWidget` | 主封面、纸墨按钮、存档/设置层 | 开始仅进入世界地图；继续恢复存档 |
| 扩展 `UGameXXKWorldMapWidget` | 山水地图、城镇节点、锁定状态、返回/设置 | 点击青山镇进入 Town；无关卡目标的地点禁止转场并给出可读提示 |
| `UGameXXKTownHudWidget` | 左上角色组、顶部资源、左侧导航、邮件/设置 | 任务、背包、角色、伙伴、返回世界地图、江湖入口 |
| `UGameXXKInventoryWindowWidget` | 九宫格纸张、标签、物品/详情/动作框 | 背包、装备、商店、筛选、排序、分解、强化、确认 |
| `UGameXXKTaskPanelWidget` / `UGameXXKQuestDialogWidget` | 任务纸张面板与 NPC 对话 | F 接取、追踪、关闭、输入锁 |
| 角色 / 伙伴生产面板 | 角色卡、属性、装备、伙伴列表 | 从 RuntimeState 展示数据，提供现有可用选择/关闭路径 |
| `UGameXXKOneGameRouteMapWidget` | 分层路线、节点和返回 HUD | 拖拽、点击、可达状态、流转至事件/战斗 |
| `UGameXXKBattleBoardWidget` | 战斗状态、单位信息、命令、目标提示 | 右键命令、左键目标、技能/道具/结算 |

## 运行时集成原则

1. `AGameXXKMVPPlayerController` 保持现有 Widget 创建权、Z 序和输入模式：城镇 HUD 在游戏上方，地图与战斗为主要全屏层，库存/任务/对话为模态高层。
2. 扩展 `UGameXXKWorldMapWidget` 成为实际生产 Widget，由控制器创建、刷新、设置焦点和销毁/隐藏；保留其 `TrySelectRegion` 选区 API，但不能继续只做测试桥接。
3. 所有新的可点击对象先验证 RuntimeState，再调用 `UGameXXKMVPSubsystem` 或 `GameXXKMVPCommandRouter`；UI 不直接修改关卡或存档字段。
4. 锁定城镇、锁定路线节点、缺失资源和无效状态必须安全禁用按钮并显示运行时说明，不得留空白热点或崩溃。
5. 旧 `TownOverlayWidget` 的公共测试接口保持可用，即使生产视觉不再依赖其折叠的面板。

## 功能完成标准

- 新游戏可以停在世界地图，青山镇节点可点击，锁定地点不可点击。
- 城镇地图导航能够返回世界地图；城镇退出/江湖入口仍可进入路线图。
- 所有现有 HUD 导航有真实可用的目标面板，不以临时文本框替代。
- 背包五个分类都遵循物品类别；分解产生定义的材料或明确拒绝不可分解物品，绝不调用出售动作冒充分解。
- 任务接取、交易、装备、排序、强化、确认、快捷键、Esc 和输入锁保持可用。
- 路线节点的显示、命中框和 `AGameXXKMVPPlayerController::HandleRouteMapPrimaryClick` 的坐标一致；可达节点能进入对应事件/战斗。
- 战斗板不存在公开返回常量 `false` 的交互 stub；命令、目标和结算都对应 RuntimeState。
- 任务、追随者、NPC 位置、世界地图解锁和路线状态通过手动存档恢复。

## 验证策略

1. 为新世界地图、主菜单不自动进城、城镇地图返回、HUD 动态文字/条、库存筛选与分解、路线层与命中框、战斗命令补齐自动化测试。
2. 运行 PSD/manifest 资产检查，确认每个导入层为 RGBA、无烘焙动态文字，且地图层不存在整张烘焙节点图作为交互源。
3. UE MCP 保存相关包后关闭编辑器，使用 UBT 冷编译 `GameXXKEditor Win64 Development`。
4. 运行针对 UI、库存、世界/路线地图、战斗板和玩家流程的自动化测试。
5. 使用 `scripts/gamexxk_real_play_flow_mcp.py` 更新后的步骤做 PIE 验证：开始 → 世界地图 → 青山镇 → F 接任务 → 手动存档 → 江湖路线 → 节点 → 战斗。
6. 在 16:9 基准分辨率捕获世界地图、城镇 HUD、背包、任务、路线和战斗截图，核对 PSD 的结构、动态状态和未遮挡点击区域。

## 兼容性与风险控制

- 当前 `MainMenuWidget` 的直接入城逻辑、现有真机流程脚本和部分截图测试必须同步改为世界地图中转，避免旧验收脚本误判。
- 控制器既有的 RouteMap 点击后备路径依赖 Widget 几何；改布局后必须通过单元测试和 PIE 点击验证而非仅检查可见性。
- 用户正在调整的城镇 HUD/背包文件保持为基础改动：在其上增量整合，绝不 `checkout`、重置或批量替换。
- 新生成美术只服务于缺失的非文字层；不能把可编辑文字、样例数值或完整 UI 截图导入为最终运行时按钮背景。
