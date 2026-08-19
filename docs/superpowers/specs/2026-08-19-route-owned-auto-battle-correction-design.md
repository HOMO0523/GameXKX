---
status: approved
owner: user
updated_at: 2026-08-19T18:55:00+08:00
evidence_image: Saved/VisualReview/20260819-challenge-aspect-audit/01-current-challenge.jpg
evidence_review: Saved/VisualReview/20260819-challenge-aspect-audit/luna-max-audit.md
---
# GameXXK 现有路线战斗复用与自动出牌纠偏设计

## 1. 用户最终口径

2026-08-19 用户明确纠偏：局内战斗只需复用当前“进入传送门后，玩家在杀戮尖塔式地图上自己点路线节点，再进入现有战斗”的流程；不要在桌面历练工作台内另造路线图、战斗画布、3 敌 3 我顶栏或只读侧壳。唯一新增能力是在玩家遭遇怪物并进入现有卡牌战斗后，由自动战斗替玩家合法出牌。

本设计取代 `2026-08-17-gamexxk-desktop-training-workbench-design.md` 中所有“合并 ChallengeViewport / 工作台内嵌 BattleBoard / 3+3 战斗顶栏 / 挑战只读侧壳”要求，并恢复 `2026-08-12-gamexxk-idle-desktop-migration-design.md` 第 14、19.2、22 节的核心边界：路线、事件、商店、奖励和重试仍由玩家选择，自动战斗只接管局内卡牌操作。

## 2. 本轮截图证据

本轮新截图 `01-current-challenge.jpg` 显示：

- 工作台纸张被缩在窗口中央，左右产生大面积黑边；
- 工作台内同时绘制路线文字、嵌入 BattleBoard、3+3 站位和控制按钮；
- 战斗角色/HUD 整组越过纸张右边界，路线文字与局外 HUD 重叠；
- Luna max 判定为 P0 不通过，局部 Canvas/父级 containment 错误置信度 82%；静态截图不能证明动画完全缺失。

虽然可以继续修锚点和局部槽位，但这条 UI 架构本身已经被用户否决，因此不再投入表现修补；正确修复是停止创建这层工作台内嵌战斗 UI。

## 3. 正确玩家流程

1. 玩家在城镇/桌面入口进入当前已有路线入口。
2. 游戏显示现有 `UGameXXKOneGameRouteMapWidget`，不显示工作台内的替代路线文字或节点。
3. 玩家亲自选择路线节点；事件、商店、营地、奖励与后续路线均保持人工选择。
4. 怪物/精英/首领节点进入现有全屏 `UGameXXKBattleBoardWidget`。
5. 玩家可以在现有战斗界面打开或关闭“自动战斗”。
6. 自动战斗打开后，只在当前 `EGameXXKScreen::Battle` 且 CardBattle 非终局时行动。
7. 胜利、失败、奖励和返回路线继续走现有权威流程；AI 不点击路线和奖励。

桌面工作台的 `挑战` 动作只能委托现有路线入口。若当前状态不满足路线入口的既有前置条件，应显示明确提示并保持工作台，不得偷偷接受任务、修改跟随者语义或直接伪造 Battle 状态。

## 4. 删除或停用的错误扩展

玩家路径不得再进入以下工作台自造表现：

- `EGameXXKDesktopTrainingViewMode::ChallengeViewport`；
- `BuildChallengeViewport()` 与 `BuildChallengeCombatStrip()`；
- 工作台内嵌的 `ChallengeBattleBoard`；
- 工作台内的 3 敌 + 3 我站位槽；
- 右侧只读训练路线文字和只读仓库侧壳；
- “击败当前遭遇”调试按钮；
- 工作台 `NativeTick` 中的挑战自动推进计时器；
- PlayerController 为工作台挑战保留非 Town 屏幕可见性的例外。

允许暂时保留私有兼容代码以降低一次提交风险，但任何玩家流程、Automation 和性能 profile 都不得再激活它；随后在独立清理提交中删除零引用代码。

## 5. 自动战斗架构

### 5.1 所有权

- 路线和战斗状态继续由 `UGameXXKMVPSubsystem` 与现有 CardBattle runtime 权威持有。
- 自动战斗开关是 `UGameXXKMVPSubsystem` 的 `Transient` 会话状态，不 bump SaveVersion；同一次运行内跨怪物战保留，重启游戏恢复为关闭。
- 自动行动由现有 `UGameXXKBattleBoardWidget` 的 Tick 驱动，因为该 Widget 已拥有卡牌提交、目标选择、待选牌和演出队列边界。
- 工作台不再持有自动战斗计时器或 BattleBoard 实例。

### 5.2 每次自动行动

Board 仅在以下条件同时满足时尝试一步：

- 自动战斗已开启；
- 当前屏幕为 `Battle` 且存在 active CardBattle；
- 战斗未 Victory/Defeat；
- 没有进行中的卡牌/敌方意图表现、commit、目标箭头或未完成的 Board mutation。

行动顺序：

1. 若有 ForcedDiscard、Insight 或 HeroTaskSearch 选择，按稳定候选顺序调用 Board 现有提交 API；
2. 否则遍历当前手牌，使用 `FGameXXKCardBattleAdapter::BuildCardPlayPreview` 读取合法性；
3. 选择第一张合法牌；需要手动目标时按 `CandidateViews` 的稳定顺序选第一个合法 UnitId；
4. 通过 `ClickCardInHand` 和 `ConfirmTargetingUnit` 提交，复用现有 `QueueMutationPresentation`；
5. 没有合法牌时调用 `EndCardPlayerPhase`；
6. 敌方意图、伤害、死亡和下一回合继续由现有 Board 演出链推进。

AI 不复制伤害、治疗、护甲、DOT、地势或职业规则，也不直接改 `RuntimeState` 绕过 Board 演出。

### 5.3 开关 UI

在现有 BattleBoard 的结束回合区域旁增加一个使用现有墨按钮样式的文字按钮：

- `自动战斗：关`
- `自动战斗：开`

不新增图标、不改变战斗画幅、不覆盖任何角色或 PaperZD/atlas 资产。按钮只改变自动战斗会话状态；关闭时等待当前演出提交链完成后停止下一步。

## 6. 错误与终局处理

- 非法/过期动作不重试同一卡牌；刷新权威状态后等待下一 tick。
- PendingChoice 候选为空或 runtime 非法时关闭自动战斗并显示现有 Board 错误文本。
- Victory/Defeat 立即停止自动行动，不自动选择奖励、不自动重试、不自动退出。
- 路线图、事件、商店、营地和奖励屏幕永远不触发自动行动。

## 7. 验收标准

### 自动化

- 工作台挑战动作进入现有路线图或在前置条件不足时保持工作台并提示；从不创建嵌入 ChallengeViewport。
- 进入 DungeonMap/Battle 时工作台关闭，现有 RouteMap/BattleBoard 可见。
- 自动战斗关闭时现有手动流程不变。
- 自动战斗每一步都通过 CardCheck/Board 提交 API；手动目标牌选合法目标。
- ForcedDiscard、Insight、HeroTaskSearch 能自动选择；无合法牌时结束回合。
- 自动战斗不选择路线节点、事件、商店、奖励或重试。
- 演出进行中不会提交第二个动作；Victory/Defeat 后停止。

### 真实 PIE / 视觉

- 从传送门进入现有路线图，玩家手动点击怪物节点。
- 进入与当前正式流程相同的全屏 BattleBoard，不出现工作台纸张、右侧重复路线文字、3+3 顶栏或纸外角色/HUD。
- 开启自动战斗后至少观察一张自动牌、一次合法目标、一次结束回合和一次敌方意图演出。
- 两个不同时刻的角色/atlas 帧号或截图证明动画在推进；不能只用单张静帧宣称动画通过。
- `L_Main.umap`、角色 sprites、PaperZD、关卡和 HD2D 手调资产哈希/状态不变。

## 8. 非目标

- 不重做路线图或 BattleBoard 视觉；
- 不让 AI 选择路线、事件、商店、营地、奖励、重试或退出；
- 不保留工作台内嵌战斗作为“备用模式”；
- 不为本修复新增宝箱、天赋、掉率或训练关卡规则；
- 不 bump SaveVersion；
- 不修改用户手调地图、角色动画资产或 HD2D 参数。
