---
status: approved
owner: user
updated_at: 2026-08-19T21:00:00+08:00
written_reviewed_at: 2026-08-19T21:00:00+08:00
supersedes: []
depends_on:
  - docs/superpowers/specs/2026-08-19-route-owned-auto-battle-correction-design.md
---
# GameXXK 战斗回退与路线挑战退出控件设计

## 1. 用户确认口径

本设计实现两层、彼此独立的退出：

1. 现有全屏战斗界面右上角提供 `自动战斗：开/关` 与 `关闭`。确认关闭当前战斗后，游戏完整恢复到点击该怪物、精英或首领节点之前；本场不结算、不标记节点完成。
2. 现有杀戮尖塔式路线图右上角提供 `关闭挑战`。确认后结束整次路线挑战，按已经完成的路线进度结算累计收益，再返回青山镇。

两种关闭都必须先显示确认弹窗。自动战斗不得点击确认、路线节点或奖励。用户于 2026-08-19 明确确认：战斗关闭采用完整回滚；即使已经进入未领取的战后奖励界面，关闭也视为放弃该节点并完整回滚。

## 2. 玩家流程

### 2.1 战斗内关闭

1. 玩家从路线图点击 Battle、Elite 或 Boss 节点。
2. 游戏在提交节点前保存一个可持久化的战斗入场检查点。
3. 战斗界面右上角显示：
   - `自动战斗：关/开`
   - `关闭`
4. 玩家点击 `关闭` 后打开模态确认层：
   - 标题：`退出当前战斗？`
   - 说明：`将返回进入本场前的路线节点。本场进度与未领取奖励不会保留。`
   - 主按钮：`退出战斗`
   - 次按钮：`继续战斗`
5. `继续战斗` 或 Escape 只关闭弹窗；自动战斗开关保持原值，并在弹窗关闭后恢复运行。
6. `退出战斗` 原子恢复检查点，清除当前 CardBattle、敌方意图、演出队列、目标选择与未领取奖励，返回路线图。

路线图回退后的权威状态必须满足：

- `CurrentRouteNodeId` 恢复为进入本场前的节点；
- 被放弃节点不在 `VisitedRouteNodeIds` 中；
- `ReachableRouteNodeIds` 恢复为点击前的完整候选集合，因此玩家可以重试原节点或选择当时的其他分支；
- `PendingRouteNodeId == INDEX_NONE`；
- 主角 HP/MP 恢复到入场前值；
- 本场没有路线货币、路线卡、遗物、属性、战后奖励或节点完成收益；
- 路线挑战仍保持 active，不进行终局结算。

### 2.2 路线图关闭挑战

1. 路线图固定右上角显示 `关闭挑战`；按钮不属于可滚动路线画布，不会随地图滚动。
2. 点击后打开模态确认层：
   - 标题：`结束本次挑战？`
   - 说明：`将按已完成的路线进度结算，未完成节点不计入。`
   - 预览：`永久金币 +X / 强化石 +Y`
   - 主按钮：`结算并退出`
   - 次按钮：`继续挑战`
3. 取消只关闭弹窗，不改变路线状态。
4. 确认调用现有 `Abandoned` 路线终局结算，成功后返回青山镇；结算失败时保留弹窗与路线状态并显示错误，不允许半退出。

现有已批准的 Abandoned 转换公式保持不变：

- 永久金币 = `RouteTravelMoney / 20` 向下取整；
- 强化石 = `ActualRouteCardAcquisitionCount / 10` 向下取整；
- 结算使用现有持久化 `SettlementId` 保证保存、重载或重复确认都不会重复发奖。

## 3. 状态所有权与回滚检查点

### 3.1 新检查点

在保存权威的 `FGameXXKRuntimeState` 中加入一个小型 `FGameXXKBattleEntryCheckpoint`，而不是保存完整 RuntimeState 副本。字段固定为：

- `bValid`
- `SourceNodeId`
- `PreviousCurrentRouteNodeId`
- `PreviousDungeonNodeIndex`
- `PreviousPlayerHP`
- `PreviousPlayerMP`
- `PreviousVisitedRouteNodeIds`
- `PreviousReachableRouteNodeIds`

检查点带 SaveGame 语义，使玩家在战斗中存档、重启、加载后仍能准确回退。

### 3.2 捕获时机

`UGameXXKMVPRules::SelectRouteNodeById` 对 Battle、Elite、Boss 的事务顺序改为：

1. 校验节点当前可达；
2. 从未变更的路线状态捕获检查点；
3. 设置 `CurrentRouteNodeId` / `PendingRouteNodeId` 并开始战斗；
4. 只有 BeginBattle 全部成功才提交 Candidate；失败不会留下检查点或半提交节点。

### 3.3 清理时机

检查点在以下路径清除：

- 玩家确认战斗回退并成功恢复；
- 玩家选择或跳过战后奖励，节点正式结算；
- 路线 Cleared、Defeated 或 Abandoned 终局结算；
- 新游戏或新路线初始化。

中途 Victory 但奖励尚未选择时，检查点仍然有效，因此右上角关闭可以放弃未领取奖励并回退节点。

### 3.4 旧存档兼容

旧版本战斗存档没有检查点。迁移时：

1. 根据 `PendingRouteNodeId` 的入边与 `VisitedRouteNodeIds` 推导最近的已访问父节点；
2. 将加载时的 HP/MP 作为兼容回退值；
3. 保存当前 Visited/Reachable 集合作为检查点；
4. 若无法唯一推导父节点，战斗关闭按钮保持禁用并显示明确原因，但路线终局退出仍可通过失败/放弃路径完成。

新版本产生的战斗不得依赖此兼容回退。

## 4. Rules 与 Subsystem API

新增两个权威事务：

- `UGameXXKMVPRules::RetreatCurrentBattleToRoute(FGameXXKRuntimeState&)`
  - 只接受 active generated-route Battle/Elite/Boss；
  - 恢复检查点；
  - 清除 active CardBattle、legacy battle projection、PendingReward、enemy intents 与本地节点提交；
  - 保留路线经济、已完成节点、已取得路线卡和遗物；
  - 返回 `DungeonMap`，不调用路线终局结算。
- `UGameXXKMVPSubsystem::AbandonDungeonToTown()`
  - 薄封装现有 `UGameXXKMVPRules::AbandonDungeonToTown`；
  - 使用现有 Abandoned settlement；
  - 成功后由 UI 调用 `GameXXKLevelFlow::OpenMapForRuntimeState`。

另提供只读 `PreviewAbandonedRouteSettlement`，供路线确认弹窗显示精确的金币与强化石，不提前写入收据或奖励。

所有事务先在 Candidate 上完成并校验，再一次性覆盖 State；任何失败都不得留下半清理、半结算或错位节点。

## 5. UI 结构与布局

### 5.1 BattleBoard

- `自动战斗` 从右下角 End Turn/Qi 操作轨移到 16:9 Battle safe stage 的固定右上角。
- `关闭` 与自动战斗位于同一横向工具条：自动战斗在左，关闭在右。
- 工具条使用现有战斗墨迹按钮纹理和字体，不新增图片资产。
- `结束回合` 保持右下角独立按钮；Party Qi 只避让结束回合和手牌，不再为自动战斗预留右下角空间。
- 右上工具条必须避开顶端敌方意图卡、标题安全区和窗口边界，在 1280×720、1672×941、1920×1080 均不重叠。

BattleBoard 自己拥有 transient 确认层状态。确认层覆盖 safe stage，包含半透明遮罩、现有纸张面板、说明文字与两个现有样式按钮。弹窗显示期间：

- 手牌、目标、结束回合、自动战斗和关闭按钮全部锁定；
- 自动战斗不修改 session 开关，只暂停行动；
- 任意在途演出先自然完成，退出事务只在 Board 无 commit/presentation mutation 时执行；
- 若玩家在演出中点击关闭，只打开确认层，不截断已提交的演出。

### 5.2 RouteMap

- `关闭挑战` 加在 `UGameXXKOneGameRouteMapWidget` 的 `RootOverlay` 固定右上角，位于 RouteScrollBox 之上且避开滚动条。
- 路线确认层同样属于 RootOverlay，打开时阻止节点、拖拽和滚动输入。
- 结算预览每次打开弹窗时从当前权威 State 重新计算，避免陈旧数字。

## 6. 自动战斗后台节拍修复

精英关卡实机取证表明，现有自动节拍误用被编辑器/窗口后台节流的 UMG `InDeltaTime`：25 秒墙钟只累计约 0.63 秒，长战斗看起来永久不动。

本设计把生产自动节拍改为单调墙钟：

- `NativeTick` 读取 `FPlatformTime::Seconds()`；
- `TickAutoBattleAtRealTime` 比较绝对时间间隔；
- 每个可行动 tick 最多提交一个 Board 动作；
- 演出、敌方意图、待选项、目标选择、Victory/Defeat 和确认弹窗门禁保持不变；
- 窗口后台节流只降低每秒检查次数，不再把 0.75 秒行动间隔拉长到几十秒。

## 7. 错误处理

- 战斗检查点缺失或不匹配：确认按钮禁用并显示原因，不猜测节点、不直接结束整条路线。
- 战斗回退事务失败：保持战斗与弹窗，自动战斗仍暂停。
- 路线结算预览失败：不开放确认按钮。
- 路线 Abandoned Apply 失败：不切地图、不清路线、不重复写奖励。
- 地图切换仅在权威事务成功后发生。

## 8. 测试与验收

### 8.1 Rules / Save

- Battle、Elite、Boss 入场都捕获精确检查点；非战斗节点不捕获。
- 战斗中回退恢复 previous current/visited/reachable/HP/MP，清除 pending/active battle，且被放弃节点仍可达。
- Victory 未领赏回退丢弃奖励并回到前一节点。
- 回退不改变 RouteTravelMoney、已获得路线卡、Relics 或已完成节点。
- 战斗中保存/加载后仍可回退；旧存档兼容推导有确定性。
- Abandoned 结算精确使用 `/20` 与 `/10`，并保持 settlement 幂等。

### 8.2 UI

- Battle 右上角存在 `自动战斗` 与 `关闭`；右下角只保留 Party Qi 与 `结束回合`。
- RouteMap 右上角存在固定 `关闭挑战`，滚动路线不会移动该按钮。
- 两个弹窗的取消路径零状态变更，确认路径只调用各自权威事务。
- 弹窗期间自动战斗不行动，关闭弹窗后恢复。
- 1280×720、1672×941、1920×1080 布局无重叠、裁切或错误拉伸。

### 8.3 实机流程

真实 PIE/MCP 必须验证：

1. 城镇出口进入路线图；
2. 玩家点击 Elite；
3. 自动战斗在后台节流条件下继续行动；
4. 战斗关闭取消不变、确认完整回退到 Elite 前节点；
5. 再次进入并完成一个节点；
6. 路线关闭弹窗显示精确预览；
7. 确认后按已完成进度只结算一次并返回青山镇。

## 9. 非目标

- 不让 AI 选择关闭确认、路线节点、事件、商店、营地或奖励。
- 不新增自动路线、自动重试或自动领奖。
- 不修改地图资产、角色 sprites、PaperZD、HD2D plane、战斗背景或现有路线美术。
- 不把战斗重新嵌入桌面工作台。
