# 中央投影 Battle HUD 设计

**状态：已确认架构，等待用户审阅本文后进入实施计划。**

## 结论

战斗单位的生命、内力、护甲和状态不再由 `AGameXXKBattleSceneUnitActor` 上的 `UWidgetComponent` 承载。它们改为现有 `UGameXXKBattleBoardWidget` 内的一层集中 UMG Canvas：`BattleProjectedUnitHudLayer`。

每个存活战斗单位在该层拥有一个按稳定 `UnitId` 管理的 `UGameXXKBattleUnitHudWidget`。这个信息牌的数值只读自 `CardRun.ActiveBattle.Units`，位置则由现有玩家控制器的 UnitId→屏幕投影桥持续提供。Actor 保留角色视觉、碰撞、目标高亮、攻击/受击震动与 VFX；它不再拥有、创建或刷新任何资源/状态 WidgetComponent。

这份设计取代下列旧的 Actor HUD 方向：

- `docs/superpowers/plans/2026-07-19-battle-actor-resource-hud.md`
- `docs/superpowers/plans/2026-07-20-battle-actor-hud-and-shared-qi.md`

它不改动主线、招募、存档、卡牌结算数值、地图、角色原画、手调角色蓝图、PaperZD 或相机。

## 已核实的问题

当前 `AGameXXKBattleSceneUnitActor` 在 C++ 中持有两套 Screen-space `UWidgetComponent`：资源组件与状态组件。它们的生命周期和数据填充实际已经走通：`ConfigureFromRuntimeUnit()` 会按 `UnitId` 从卡牌运行时覆盖生命、内力、护甲和状态，然后调用两个 Widget 的刷新函数。

但真实 PIE 截图中，角色与怪物脚下没有可见的资源/状态 UI；探针虽然能读到“组件可见、Widget 有值”，却取不到有效 `screen_rect`。这说明问题在最终渲染/几何路径，不在 HP、MP、护甲或状态的规则数据。

继续调整 Screen-space WidgetComponent 的 SharedLayer、Pivot、DrawSize 或碰撞设置不能构成可验证的解决方案。现有 Board 已经由玩家控制器每帧把场景单位的 `UnitId` 投影到 UMG 坐标，因此把最终显示移到 Board Canvas 是更小、也更稳定的修复边界。

## 用户可见结果

### 单位信息牌

每个存活单位在角色视觉底部下方显示一张小型信息牌：

- 顶行：`我 1P / 我 2P / 我 3P` 或 `敌 1P / 敌 2P / 敌 3P`、名字与当前生命/最大生命；
- 第二行：红色气血条与数值；
- 第三行：我方主角、永久伙伴、临时 NPC 显示蓝色内力条与数值；敌人不显示内力；
- 最后一行：有值才显示护甲图标/层数以及状态图标/层数；
- 状态顺序沿用现有统一规则与低饱和单色水墨图标风格；鼠标悬停图标显示统一说明 Tooltip。

单位信息牌跟随当前相机、角色反馈位移与屏幕尺寸变化。它在不遮挡手牌、中央出牌展示、右下角队伍气力和结束回合区域的前提下，保持与单位底部中心的水平对齐。

### 战斗层级

`UGameXXKBattleBoardWidget` 是唯一的战斗 HUD 根。其层次为：

1. `RootCanvas`：现有战斗界面根；
2. `BattleProjectedUnitHudLayer`：单位资源/状态信息牌；
3. 现有手牌、顶部敌方意图、中央展示与队伍气力；
4. 现有目标箭头与卡牌 Tooltip；
5. modal、奖励与路由离场 UI。

信息牌位于世界角色视觉之上，但低于 Tooltip、目标箭头和需要优先阅读的牌面展示。普通背景、血条和文字都是 `SelfHitTestInvisible`；状态图标的 Hover 子控件只通知 Board 显示 Tooltip，鼠标按下返回未处理，不能吞掉角色目标选择或指向箭头操作。

`UGameXXKBattlePartyQiWidget` 保持 Board 所有，继续显示在手牌右下侧。它表示全队共享的“气力”，不与角色个人的“内力（MP）”混用。

## 职责边界

| 单元 | 负责 | 不负责 |
| --- | --- | --- |
| `CardRun.ActiveBattle.Units` | `UnitId`、阵营、角色身份、存活、HP/MaxHP、Mana/MaxMana、Armor、Statuses 的唯一权威数据 | 屏幕坐标、UMG Widget、伤害动画 |
| 卡牌适配器 | 改写权威战斗状态，并同步旧场景兼容投影 | 直接绘制 HUD |
| `AGameXXKBattleSceneUnitActor` | 角色视觉、命中区域、目标高亮、攻击/受击反馈、从 Scene Visual 计算投影锚点 | 血条、内力、护甲、状态 Widget 的创建、层级、刷新 |
| `AGameXXKBattleScenePresenter` | 按 `UnitId` 保留/生成/移除场景 Actor | 存储 UI 数值副本 |
| `AGameXXKMVPPlayerController` | 使用现有 `ProjectWorldLocationToWidgetPosition` 桥按 `UnitId` 提交 UMG 本地坐标 | 根据索引猜测单位身份 |
| `UGameXXKBattleBoardWidget` | 创建、更新、定位、销毁信息牌；手牌、意图、Tooltip、箭头、队伍气力 | 计算伤害、保存临时 UI 位置 |

旧的 `ActiveBattleParty` 与 `ActiveBattleEnemies` 是场景兼容投影，不可用于恢复状态图标：它们没有完整 `Statuses`。集中 HUD 在卡牌战斗中必须直接消费 `CardRun.ActiveBattle.Units`，不得从旧数组或 Actor 缓存反推状态。

## 数据视图

Board 在每次状态刷新时按 `UnitId` 构造一个只读 `BattleHudUnitView`。它不是新的存档结构，也不写回玩法数据。字段固定为：

| 字段 | 来源 | 用途 |
| --- | --- | --- |
| `UnitId` | `FGameXXKCardCombatUnit::UnitId` | 稳定 Widget、Actor 与交互映射主键 |
| `Side`、`Role` | 同一结构 | 决定敌/我视觉规则与 MP 可见性 |
| `SlotNumber` | 现有确定性 P 位映射 | 显示 `我/敌 nP`；不取代 `UnitId` |
| `DisplayName` | 场景兼容定义/展示定义 | 仅显示名称，不承载数值真相 |
| `bLiving` | `FGameXXKCardCombatUnit::bLiving` | 决定生成和隐藏 |
| `HP`、`MaxHP` | `FGameXXKCardCombatUnit` | 红条、数字、受击差异动画 |
| `Mana`、`MaxMana` | `FGameXXKCardCombatUnit` | 我方单位蓝条与数字 |
| `Armor`、`Statuses` | `FGameXXKCardCombatUnit` | 图标、层数、Hover Tooltip |

`StableSortOrder` 只用于确定性排序；任何 UI 子控件、目标箭头和 Actor 查询都只使用 `UnitId`，绝不使用 party/enemy 数组下标作为身份。

## 生命周期与刷新

### 构建与重建

1. Board 进入战斗且 `CardRun.bHasActiveCardBattle` 为真时，读取 `ActiveBattle.Units`。
2. 对每个存活、且能解析到当前场景 Actor 的 `UnitId`，在 `BattleProjectedUnitHudLayer` 中创建或复用一个 `UGameXXKBattleUnitHudWidget`。
3. 该 Widget 接收完整 `BattleHudUnitView`，内部复用现有资源条、状态行、护甲和状态图标样式；不读取 Actor 的 HUD 缓存。
4. `UnitId` 已死亡、离场、不可解析或战斗结束时，移除/折叠对应 Canvas child；不得保留旧位置的假信息牌。

### 数据刷新

Board 的 `RefreshFromState()` 以及现有 `ResolveAndRefreshCardBattleAfterMutation()` 后调用集中 HUD 刷新。所有出牌、目标确认、敌方单张意图、回合结束状态结算、抽牌和奖励转场都经过同一刷新入口。

数据视图只在状态签名变化时重建；签名包含 `UnitId`、存活、P 位、HP/MaxHP、Mana/MaxMana、Armor、状态堆叠和显示名。数值或状态变化时，Widget 立即更新，并可触发既有/后续的血条过渡、护甲变化和状态入场表现。它不等待下一次 Actor 重生。

敌方意图逐牌结算和回合结束 DoT 必须也走相同的即时刷新入口。否则最终数字虽会正确，但受击、血条与状态的逐张可读反馈会漏失。

### 位置刷新

现有 `AGameXXKMVPPlayerController::RefreshBattleCardTargetingBridge()` 已在每帧按 `UnitId` 调用 `ProjectWorldLocationToWidgetPosition` 并写入 `RegisteredBattleUnitScreenPositions`。集中 HUD 直接扩展这一桥：

1. 投影点使用角色 `BattleVisual` 的底部中心（而不是旧 WidgetComponent Anchor）；
2. Board 取得该 `UnitId` 的 Canvas 本地投影点；
3. 按信息牌宽度居中、顶边位于视觉底部下方的固定小间距定位 Canvas Slot；
4. 若会压到手牌安全区，信息牌仅垂直上移到安全线，水平中心仍保持对齐；
5. 每帧只更新位置，不重复读取或改写玩法数据；
6. 投影无效、视口尚未建立或 Actor 尚未生成时，当前帧折叠该牌并在下一帧重试，绝不使用上一次坐标。

这样角色 idle 震动、受击震动、镜头位移、窗口缩放和画面比例变化会自动让信息牌重新贴位。没有新的 Actor Blueprint 组件，也不需要在 Actor Tick 中刷新 UI。

## 交互契约

- 所有卡牌（手牌、敌方意图、奖励、招募、卡组编辑）维持“有卡即有 Hover Tooltip”的已有约定；本设计不改变卡牌点击与目标箭头状态机。
- 单位状态图标 Hover 显示状态名、层数、触发时机和当前真实效果；Tooltip 从 `BattleHudUnitView.Statuses` 与同一状态定义构造。
- 信息牌的非图标部分不拦截鼠标。状态图标点击也不消费事件，因此用户在信息牌区域仍可选择角色/怪物作为卡牌目标。
- 当前被卡牌选为合法目标时，Actor 继续负责自身轮廓高亮；集中 HUD 只同步显示，不能以 Widget 命中测试取代世界目标确认。

## 错误处理与降级

| 情况 | 处理 |
| --- | --- |
| `ActiveBattle` 不存在或当前不在战斗画面 | 清空 `BattleProjectedUnitHudLayer`，不显示旧数值 |
| 权威单位存活但场景 Actor 尚未生成 | 暂不显示该牌，记录一次按 `UnitId` 去重的诊断并持续重试 |
| 场景 Actor 存在但权威单位不存在/已死亡 | 立即移除 HUD child，不伪造默认生命值 |
| 投影失败或坐标不在可绘制视口 | 当前帧折叠；坐标恢复后重新显示；不复用陈旧矩形 |
| 状态图标资源缺失 | 使用已定义的通用低饱和单色状态符号与真实文本/层数；不丢失状态信息 |
| Board 被销毁、离开战斗或奖励流程覆盖 | 释放所有 Unit HUD child 和 Hover Tooltip 状态，禁止跨地图保留 UI 引用 |

## 迁移范围

### 删除/停止使用的旧路径

- 从 `AGameXXKBattleSceneUnitActor` 删除资源/状态 `UWidgetComponent`、对应 WidgetComponent getter、Widget 刷新函数、SharedLayer/DrawSize/Pivot 配置和旧 HUD Anchor 链。
- 删除 Actor 内对 `UGameXXKBattleUnitResourceWidget`、`UGameXXKBattleUnitStatusEffectsWidget` 的创建和可见性控制。
- `LabelText` 不再作为卡牌战斗中的主要 P 位/生命信息；集中信息牌负责此显示，避免与场景文本重复。
- 禁止把旧 WidgetComponent 仅隐藏后长期保留为“备用真相”或测试依据。最终验收中不存在 actor-owned resource/status WidgetComponent。

### 新增/修改的集中路径

- 扩展 `UGameXXKBattleBoardWidget` 的 RootCanvas，加入非交互 `BattleProjectedUnitHudLayer`、按 `UnitId` 的 HUD map 与测试快照；
- 新增复合 `UGameXXKBattleUnitHudWidget`，内部组合现有资源行和状态行；
- 复用玩家控制器现有 UnitId 投影桥，并改用视觉底部中心为投影锚点；
- 将现有资源/状态 Widget 保留为 Board 的普通 UMG 子 Widget，而不是 Actor Component 的 UserWidget；
- 将真实 PIE 探针与验收脚本改读 Board 的 `unit_huds`，不再读 Actor WidgetComponent。

预期影响文件：

- `Source/GameXXK/Public/Private/UI/GameXXKBattleBoardWidget.*`
- 新增 `Source/GameXXK/Public/Private/UI/GameXXKBattleUnitHudWidget.*`
- `Source/GameXXK/Public/Private/MVP/GameXXKBattleSceneUnitActor.*`
- `Source/GameXXK/Public/Private/MVP/GameXXKMVPPlayerController.*`
- 视注册接口需要，`Source/GameXXK/Public/Private/MVP/GameXXKBattleScenePresenter.*`
- 现有 `GameXXKBattleUnitResourceWidget.*`、`GameXXKBattleUnitStatusEffectsWidget.*` 与状态图标 Widget 的父级/测试接口
- `Content/Python/gamexxk_probe_real_play_flow.py`
- `scripts/gamexxk_real_play_flow_mcp.py`、`scripts/test_gamexxk_real_play_flow_probe.py`、`scripts/test_gamexxk_real_play_flow_mcp.py`
- `GameXXKBattleSceneActorHudTest.cpp`、`GameXXKBattleSceneActorTest.cpp`、`GameXXKBattleActorHudRetirementTest.cpp`、`GameXXKBattleBoardWidgetTest.cpp` 及新增的 Board Unit HUD 测试。

不修改 `.umap`、角色/怪物素材、PSD 切图、已摆放的场景 Actor 或相机资产。

## 验收与测试

### 自动化测试

1. 一个存活的 `CardRun.ActiveBattle.Units` 单位只生成一个同 `UnitId` 的 Board Canvas 信息牌；重复刷新不重复生成。
2. 权威卡牌数据覆盖旧投影：fixture 中主角、伙伴、NPC 的 HP/MP、敌人的 HP/Armor、毒/流血状态均与 `BattleHudUnitView` 和子 Widget 文本/图标一致。
3. 敌人始终隐藏内力；主角、永久伙伴和临时 NPC 显示内力；队伍气力仍只出现在手牌右下。
4. 卡牌单位死亡、离场或战斗结束后，Board child 移除；不会留下旧坐标、旧状态或默认满血显示。
5. 变更 HP、内力、护甲或状态后，同一次卡牌刷新立即更新 Board 信息牌；敌方意图与回合末 DoT 同样覆盖。
6. `AGameXXKBattleSceneUnitActor` 不含资源/状态 WidgetComponent；测试不再断言 Widget Space、DrawSize、Pivot、SharedLayer 或 Actor HUD Anchor 层级。
7. Board overlay 根为 `SelfHitTestInvisible`；状态图标 Hover 生成 Tooltip，且在信息牌区域点击仍能到达原有角色目标选择路径。
8. 同一 `UnitId` 在 Actor、投影桥、HUD map、P 位和目标箭头之间一一对应；乱序数组或空 P 位不改变身份。
9. 投影点移动/镜头变化后，信息牌中心 X 与当前投影锚点 X 的偏差不超过 2 像素；资源行在状态行上方，且两者完整落在视口内。
10. 信息牌不与手牌、队伍气力、结束回合、中央展示牌发生矩形重叠；必要时只进行垂直安全上移。

### 真实 PIE 验收

1. 按主线进入路线战斗，确认角色/怪物脚下均有清晰可读的集中 HUD，而不是粗略统计面板。
2. 通过可控 fixture/真实出牌使主角 HP、MP、护甲和状态变化；确认数值不是满血假象，且图标层数正确。
3. 让怪物执行一张意图牌：攻击者短震、目标受击后对应信息牌即时变化；主角受击仍保留轻微镜头震动。
4. 加载永久伙伴和临时 NPC 时，三名我方单位都显示各自 HP/MP/状态；无该单位时没有孤儿 HUD。
5. 改变窗口/相机并触发角色反馈：所有牌仍在角色下方、不会跑到屏幕外、不会压住手牌或结束回合。
6. 截图与探针同时通过：探针从 Board `unit_huds` 取得有效屏幕矩形和渲染数据，截图中真实存在对应像素。禁止以“Actor 组件存在”代替视觉验收。

## 与现有自动化问题的关系

UE Python fixture 桥当前会把成功但返回空字符串的调用误判为失败，导致真实 PIE 刷新链被提前跳过。这是独立的测试桥缺陷，不属于 HUD 架构，也不改变本设计的数据来源；但必须在执行真实 PIE 验收前一并修正，否则会产生“权威数据已变化、Board 未收到刷新”的假阴性。
