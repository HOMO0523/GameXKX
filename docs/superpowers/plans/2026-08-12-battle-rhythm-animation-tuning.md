# GameXXK 类杀戮尖塔战斗动效节奏实施计划

> **执行要求：** 使用 `executing-plans` 逐任务实施；运行时行为使用 `test-driven-development`，提交前使用 `verification-before-completion`。不得改变已确认 UI 布局、角色资源、背景、相机或 HD2D 手调参数。

**目标：** 将现有长串中央特写改成“出牌立即确认、首击可读、多段紧凑、轻重明确、死亡干净”的战斗节奏，同时保持权威伤害顺序、异步 atlas 回退、interaction lock 和 continuation 恰好一次。

**架构：** 在纯 `FGameXXKBattleAnimationPresentation` 中解析每个 immutable event 的 rhythm profile；Board queue 按 profile 驱动完整 atlas 播放、命中点、数字和震屏。主动手牌成功结算后增加 0.18 秒 render-only commit；布局 slot、anchor、size 不变。

**技术栈：** UE 5.8 C++、UMG render transform、Unreal Automation、UE MCP 真实 PIE（不用 UnrealBridge）、冷 UBT。

**权威规格：** `docs/superpowers/specs/2026-08-12-balance-and-battle-rhythm-tuning-design.md`

---

## Task 1：纯节奏模型

**文件：**

- 修改：`Source/GameXXK/Public/UI/GameXXKBattleAnimationPresentation.h`
- 修改：`Source/GameXXK/Private/UI/GameXXKBattleAnimationPresentation.cpp`
- 修改：`Source/GameXXK/Private/Tests/GameXXKBattleAnimationPresentationTest.cpp`

### 1.1 RED

新增 `FGameXXKBattlePresentationRhythm`，测试锁定：

- 首击：0.82 s / impact 0.30 s。
- 后续命中：0.30 s / impact 0.10 s。
- 闪避：0.45 s / impact 0.16 s / shake 0。
- 死亡：0.90 s。
- 状态脉冲：0.30 s。
- 轻/中/重/致命的 shake、duration、readout peak。
- TargetHealthBefore<=0、负伤害、NaN/Infinity 不得产生非法时长或 transform。
- `FitClipToDuration` 让最后一帧在目标时长前可达，并保留 atlas 尺寸/路径。

### 1.2 RED/GREEN

冷 UBT 后精确运行：

```text
GameXXK.Presentation.BattleAnimation
```

只实现纯解析器，不碰 Board。完成后同过滤器全绿。

### 1.3 提交

```powershell
git add Source/GameXXK/Public/UI/GameXXKBattleAnimationPresentation.h Source/GameXXK/Private/UI/GameXXKBattleAnimationPresentation.cpp Source/GameXXK/Private/Tests/GameXXKBattleAnimationPresentationTest.cpp
git commit -m "test: lock battle rhythm timing"
git push origin main
```

---

## Task 2：Board 队列按事件节奏播放

**文件：**

- 修改：`Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h`
- 修改：`Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`
- 修改：`Source/GameXXK/Private/Tests/GameXXKBattleAnimationLayerWidgetTest.cpp`
- 修改：`Source/GameXXK/Private/Tests/GameXXKCardBattleBoardWidgetTest.cpp`
- 邻接：`Source/GameXXK/Private/Tests/GameXXKBattleProjectedUnitHudTest.cpp`

### 2.1 RED

将旧 1.1/2.5/5.0 断言替换为新规格，并补：

1. 五包在 2.02 秒内依序完成，每包 impact/completion 各一次。
2. 每一包 impact 前显示旧 HP，越过自身 impact 后显示 immutable 新 HP。
3. lethal hit 先完成 attack/hit，再播放 0.90 秒 Death，再开始后一事件。
4. 大 delta 能跨多段，但不能跳过 impact、重复 completion 或错误携带 epoch。
5. atlas 晚到时以当前 entry 的目标时长重新 fit，并追到同一绝对帧。
6. 现有 410/820 尺寸、父节点、Z order、dimmer 与 viewport cover 不变。

### 2.2 实现

`FBattlePresentationQueueEntry` 保存 resolved rhythm。替换 Board 的三个固定 duration 常量：

- attack/death duration 与 impact 从 entry rhythm 读取。
- Start 和异步 Upgrade 都调用 `FitClipToDuration`。
- 仍只在 impact 更新 HUD；不预写 live HP。
- source-less、redirect、avoid、death pin、session reset 路径保持原义。

### 2.3 GREEN

冷 UBT 后：

```text
GameXXK.Presentation.BattleAnimationLayer
GameXXK.Integration.CardBattle.BoardPresentationGate
GameXXK.Presentation.ProjectedUnitHud
```

再跑 `GameXXK.Integration.CardBattle` 整组，0 failed/0 error。

### 2.4 提交

```powershell
git add Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp Source/GameXXK/Private/Tests/GameXXKBattleAnimationLayerWidgetTest.cpp Source/GameXXK/Private/Tests/GameXXKCardBattleBoardWidgetTest.cpp Source/GameXXK/Private/Tests/GameXXKBattleProjectedUnitHudTest.cpp
git commit -m "feat: accelerate battle presentation rhythm"
git push origin main
```

---

## Task 3：伤害数字与分级震屏

**文件：**

- 修改：`Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h`
- 修改：`Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`
- 修改：`Source/GameXXK/Private/Tests/GameXXKBattleAnimationLayerWidgetTest.cpp`

### 3.1 RED

增加 test seams 获取：当前 shake amplitude/duration、readout scale/opacity、root translation。断言：

- avoid 不震屏。
- 5%、20%、45%、致死分别命中轻/中/重/致命档。
- impact 瞬间 readout 达峰，之后回落和淡出。
- shake/readout 在 entry 完成、session reset、widget teardown 后归零。
- 重复绝对时间样本不重启震屏。

### 3.2 实现

- `FirePresentationImpact` 从 entry rhythm 复制 shake 参数。
- `UpdateBattlePresentationShake` 只使用该 entry 的 amplitude/duration。
- 新增 `UpdateBattlePresentationReadout`，只改变 `BattleCinematicReadout` 的 render scale/opacity。
- Complete/Reset 显式恢复 translation=(0,0)、scale=(1,1)、opacity=1。

### 3.3 GREEN 与提交

运行 Task 2 的三组过滤器，然后提交：

```powershell
git commit -m "feat: grade battle impact feedback"
git push origin main
```

---

## Task 4：主动牌提交动画

**文件：**

- 修改：`Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h`
- 修改：`Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`
- 修改：`Source/GameXXK/Private/Tests/GameXXKCardBattleBoardWidgetTest.cpp`
- 修改：`Source/GameXXK/Private/Tests/GameXXKBattleAnimationLayerWidgetTest.cpp`

### 4.1 RED

新增 Board seams：commit active、instance id、elapsed、source button transform/opacity、完成次数。覆盖：

1. 自动目标牌成功后立刻开始 0.18 秒提交；0.18 秒前伤害队列不启动。
2. 手动目标确认成功后使用同一卡实例。
3. 治疗/状态/资源牌即使没有 damage queue，也等提交结束才 continuation。
4. 结算失败、非法目标、预览失败不播放。
5. 自动重放没有手牌 commit。
6. visual session 取消和 widget teardown 恢复 transform/opacity，continuation 至多一次。
7. 原 hand slot、206×285 尺寸和数组顺序始终不变。

### 4.2 实现

- `QueueMutationPresentation` 接受可选 `PlayedCardInstanceId`。
- 成功的 `ResolveAutomaticCardPlay` 与手动目标路径传入 `Result.CardInstanceId`；结束回合、敌方意图、选择恢复不传。
- `BeginPlayedCardCommit` 从缓存的 `HandCardInstanceIds` 找按钮，仅保存弱引用和初始 render 状态。
- 0.18 秒内使用 ease-out：上提至 -72px、峰值 1.08、后半段淡出；只使用 render transform/opacity。
- commit 完成后：若有队列，开始第一 entry；无队列，执行 deferred continuation。
- interaction lock 和 `IsBattlePresentationPending` 必须包含 commit。

### 4.3 GREEN

冷 UBT 后：

```text
GameXXK.Integration.CardBattle.Board
GameXXK.Presentation.BattleAnimationLayer
GameXXK.Presentation.CardHoverMotion
```

确保 hover/selected transform 与 commit 不冲突。

### 4.4 提交

```powershell
git commit -m "feat: add played-card commit feedback"
git push origin main
```

---

## Task 5：状态 HUD 短脉冲

**文件：**

- 修改：`Source/GameXXK/Public/UI/GameXXKBattleUnitHudWidget.h`
- 修改：`Source/GameXXK/Private/UI/GameXXKBattleUnitHudWidget.cpp`
- 修改：`Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h`
- 修改：`Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`
- 修改：`Source/GameXXK/Private/Tests/GameXXKBattleStatusEffectsWidgetTest.cpp`

### 5.1 RED

断言净状态变化后只有受影响单位状态区进行 0.30 秒 pulse；多状态同一 transaction 合并为一次；DoT 高层变化不插入 full-screen queue；结束/重置归一。

### 5.2 实现

在 Unit HUD 暴露 render-only pulse，不改状态图标布局或 tooltip。`ApplyDisplayedStatusDelta` 聚合受影响 UnitId 并启动/刷新 pulse。Board tick 推进 pulse；视觉 session 不可用时直接显示最终状态，不阻塞 continuation。

### 5.3 GREEN 与提交

```text
GameXXK.Presentation.StatusEffectsWidget
GameXXK.Integration.CardBattle
```

提交：

```powershell
git commit -m "feat: pulse battle status changes"
git push origin main
```

---

## Task 6：真实 PIE 动效复核与迭代

**文件：**

- 新增/修改：`Content/Python/gamexxk_verify_battle_rhythm.py`
- 新增：`scripts/test_gamexxk_battle_rhythm_report.py`
- 证据：`Saved/Automation/BattleRhythm_*`、`Saved/RealPlayFlow/*`（不提交大媒体）
- 修改：`docs/production/2026-08-12-battle-rhythm-tuning-status.md`

### 6.1 自动场景

通过 UE 5.8 MCP/项目脚本构造并录制或定点截图：

- 单击。
- 五段攻击。
- 闪避。
- 格挡与反击。
- 流血/中毒/灼烧触发。
- 获得与消耗状态。
- 致死及双方同死玩家胜利。

报告记录事件数、实际 wall-clock、每个 impact 时间、最终 HP、continuation 次数、残留 interaction lock、根 transform 是否归零。

### 6.2 人工视觉门禁

逐场确认：

- 出牌反馈立即可见。
- 首击看得清，后续命中不糊成同时发生。
- 数字不重复、不遮住关键目标。
- 轻击不夸张，致命足够明确。
- 没有特写闪白、角色跳位、atlas 回退长时间占位。
- 卡牌、血条、状态图标、意图和布局与基线位置一致。

若时间轴不舒服，只在规格允许区间内微调 rhythm 常量，并先更新纯 timing 测试；不得用改布局解决节奏问题。

---

## Task 7：最终动效认证

1. 冷 UBT。
2. `GameXXK.Presentation` 全组。
3. `GameXXK.Integration.CardBattle` 全组。
4. `Automation RunTests GameXXK` 全前缀。
5. 运行真实 PIE 主流程并执行 Task 6 场景。
6. 对比基线截图和布局断言。
7. 更新状态文档，列出实测五段总时长、致死总时长、已知 atlas fallback 和任何未决视觉问题。
8. 最终提交：

```powershell
git add <exact code/test/docs only>
git commit -m "test: certify tuned balance and battle rhythm"
git push origin main
```
