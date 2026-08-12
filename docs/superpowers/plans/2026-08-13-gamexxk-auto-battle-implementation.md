# GameXXK Auto Battle And 2x Presentation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在首次通关卡牌战斗中加入可随时开启、在完整结算链后安全关闭的自动出牌，并提供只缩短演出的 2× 速度，路线、事件、商店、奖励、重试和退出始终由玩家决定。

**Architecture:** 从现有 `GameXXKCombatSimulationRules.cpp` 提取共享的纯策略评分为 `FGameXXKAutoBattlePolicy`，候选行动全部在 RuntimeState 副本上通过 `FGameXXKCardBattleAdapter` 解析；Board 的 driver 只有在表现队列和选择边界空闲时提交一步。2× 只乘 Board presentation duration，不进入 CardRules/Adapter/随机输入，1×与2×最终权威状态必须逐字段相同。

**Tech Stack:** UE 5.8 C++、CardBattleAdapter、CardOutcomePreview、UMG BattleBoard、UE Automation、现有 CombatSimulation、真实 PIE。

---

## 0. 文件结构与共享决策协议

**Create:**

- `Public/AutoBattle/GameXXKAutoBattleTypes.h`
- `Public/AutoBattle/GameXXKAutoBattlePolicy.h`
- `Private/AutoBattle/GameXXKAutoBattlePolicy.cpp`
- `Public/AutoBattle/GameXXKAutoBattleDriver.h`
- `Private/AutoBattle/GameXXKAutoBattleDriver.cpp`
- `Private/Tests/GameXXKAutoBattlePolicyTest.cpp`
- `Private/Tests/GameXXKAutoBattleRuntimeTest.cpp`
- `Private/Tests/GameXXKAutoBattlePresentationTest.cpp`

**Modify:**

- `Public/GameXXKCombatSimulationRules.h`、private cpp — 复用 policy，删除私有重复 chooser/weights。
- `Public/UI/GameXXKCardBattleBoardWidget.h`、private cpp — toggle、driver、presentation multiplier。
- `MVPSubsystem` — 跨战斗记忆 `bAutoBattlePreferred`、`BattlePresentationSpeed`（玩家设置，不进入战斗随机）。
- `GameXXKCardBattleBoardWidgetTest.cpp`、`GameXXKCardBattleRuntimeTest.cpp`、`GameXXKRouteBalanceSimulationTest.cpp` — 邻接。
- Probe/harness — auto battle real PIE。

```cpp
UENUM(BlueprintType)
enum class EGameXXKAutoBattleAction : uint8
{
    None,
    PlayCard,
    SubmitInsight,
    SubmitHeroTaskSearch,
    SubmitForcedDiscard,
    CancelInsight,
    EndPlayerPhase
};

USTRUCT(BlueprintType)
struct FGameXXKAutoBattleDecision
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) EGameXXKAutoBattleAction Action = EGameXXKAutoBattleAction::None;
    UPROPERTY(BlueprintReadOnly) FName CardInstanceId = NAME_None;
    UPROPERTY(BlueprintReadOnly) FName TargetUnitId = NAME_None;
    UPROPERTY(BlueprintReadOnly) TArray<FName> ChoiceInstanceIds;
    UPROPERTY(BlueprintReadOnly) TArray<FName> ReorderedInstanceIds;
    UPROPERTY(BlueprintReadOnly) int64 Score = MIN_int64;
};
```

## Task 1: 提取共享合法策略并锁稳定决胜

- [ ] **Step 1: 写 policy 缺失 RED**

真实 CardBattle fixtures 覆盖：可击杀、濒死防御、职业机制、资源/过牌、无正收益动作。每个决定重新用 Adapter resolve，必须成功且结果与评分候选相同。

- [ ] **Step 2: 迁移现有 simulation weights**

把 `FGameXXKSimulationPolicyWeights`、`ScoreCandidateResolution`、`ChooseSkilledDecision` 的通用部分移到新 cpp。Simulation 调新 API，保持 2400 矩阵结果和 trace不变；新 policy 禁止 CardId 特例。

```cpp
static bool ChooseDecision(
    const FGameXXKRuntimeState& State,
    FGameXXKAutoBattleDecision& OutDecision,
    FString* OutError = nullptr);
```

- [ ] **Step 3: 稳定 tie-break**

按 Score desc、Card AcquisitionOrdinal asc、CardInstanceId lexical、target `BattleSlotNumber` asc、UnitId lexical。不能只按 FName target 导致1P/3P乱序。

- [ ] **Step 4: 预见死亡优先**

评分用候选 committed copy；不读取隐藏 future intents/RNG。已 reveal intent 可参与，未 reveal 不参与。确定击杀奖励低于避免主角本回合确定死亡的高优先级；具体权重放一个 config struct并测试关系，不锁单卡。

- [ ] **Step 5: mutation/GREEN/commit**

反转 target slot tie-break，确定性测试 RED；恢复提交 `feat: share deterministic auto battle policy`。

## Task 2: 自动处理三种选择

- [ ] **Step 1: 写 PendingChoice RED**

真实覆盖：ForcedDiscard、InsightChooseToHand、HeroTaskSearchChooseToHand。选择必须来自当前 CandidateViews/Pending arrays；数量精确；Insight remaining order稳定；不可取消时不发 Cancel。

- [ ] **Step 2: 评分选择结果**

Insight/TaskSearch 对每个候选创建 copy，调用对应 Adapter submit，比较后状态；ForcedDiscard 对所需数量用有界组合搜索：手牌上限20，若组合数超128则用稳定逐张损失评分 greedy，测试锁边界和稳定性。

- [ ] **Step 3: unsupported choice 停止**

未知 enum 返回 error并关闭自动 driver，不自动猜选择、不结束回合吞状态。

- [ ] **Step 4: GREEN/commit**

`git commit -m "feat: automate legal card battle choices"`。

## Task 3: 单步 Driver 与安全开关

- [ ] **Step 1: 写 driver 状态机 RED**

状态：Disabled、WaitingForBoundary、Ready、AwaitingPresentation、Terminal、Faulted。开启只在 Ready提交；关闭设置 `bDisableRequested`，若表现/反应/DOT/automatic queue仍在运行，等待 drain 后 Disabled。

- [ ] **Step 2: 定义 Board boundary**

只有同时满足以下条件才能动作：Board没有 active/queued presentation、没有 deferred continuation、没有 card target selection、没有 unresolved automatic resolution queue、没有正在播放 enemy intent、battle phase允许当前动作。

- [ ] **Step 3: 提交一步**

PlayCard→Adapter ResolveCardPlay并走现有 Board queue；Choice→现有 Submit UI handler抽出公共 commit helper；End→现有 EndPlayerCardPhase/敌方 intent链。Driver不得直接跳过演出或批量调用 `ResolveEnemyPhase`。

- [ ] **Step 4: 终局**

Victory/Defeat立即 Terminal；战败只停自动并显示现有 retry/exit UI，不调用 retry；胜利只完成当前 battle/route node，不选 reward/next node。

- [ ] **Step 5: 关闭 mutation**

临时在 queue未空时强制 Disabled，测试必须捕获半结算控制权交回；恢复 commit `feat: drive auto battle at presentation boundaries`。

## Task 4: Board UI 控件且不移动已确认布局

- [ ] **Step 1: 先锁全 Canvas layout snapshot**

沿用 Board LayoutInvariant，对所有既有 sibling保存 anchors/alignment/offset/size/z。新增控件只能进入现有 action area空位，不移动任何 sibling。

- [ ] **Step 2: 新增两个小控件**

“自动”toggle 与“1×/2×”toggle，沿用 `InkButtonTexturePath` 和现有字体/颜色。明确显示 `自动：开/关`、`速度：1×/2×`；不能用仅图标导致不理解。

- [ ] **Step 3: 输入锁**

自动开时普通手牌/目标/end turn不可点击，但 Hover tooltip仍可读；关闭请求等待期间显示“结算后停止”。Terminal/choice error 恢复可控 UI。

- [ ] **Step 4: 跨战斗偏好**

新增 `UGameXXKBattleAutomationSettings`（`UCLASS(Config=Game, DefaultConfig)`），只保存 `bAutoBattlePreferred` 和 `PresentationSpeed`；它们不是战斗/奖励权威，不为此改变 SaveGame 版本。新 battle按偏好启动，但路线 map绝不自动点击。`Faulted` 只写当前会话的 `bAutoBattleFaultedThisSession`，新battle不自动开启直到玩家手动再开；手动恢复成功后才清该标志。

- [ ] **Step 5: UI test/commit**

`git commit -m "feat: expose auto battle and speed controls"`。

## Task 5: 2× 只改变表现时长

- [ ] **Step 1: 识别所有 presentation duration**

`BattlePresentationShakeDurationSeconds`、readout、card travel、impact wait、status/death/intent wait 等统一通过：

```cpp
double UGameXXKBattleBoardWidget::ScalePresentationSeconds(double Base) const
{
    return Base / FMath::Clamp(BattlePresentationSpeed, 1.0, 2.0);
}
```

不要修改 `InDeltaTime` 传给规则、WorldSettings time dilation、timer global rate或 scheduler效率。

- [ ] **Step 2: 1×/2×等价 RED**

同一 battle/state/random，分别驱动到 terminal；对最终 RuntimeState、Damage/Armor/Healing结果序列、RandomState、route outcome做 struct等价；只允许演出 elapsed不同。

- [ ] **Step 3: 中途切速**

active entry只缩放剩余 presentation wall-clock，不重新 fire impact/continuation；测试切换前后 impact/continuation各恰一次。

- [ ] **Step 4: mutation/commit**

临时把2×应用到 CardRules两次tick，等价测试RED；恢复 `feat: add presentation-only battle speed`。

## Task 6: 机制全覆盖认证

- [ ] **Step 1: 真实卡表矩阵**

至少覆盖并命名：DOT、毒爆、重箭、药效/药方、六地形、反击、格挡、Sorcerer任务、Blade冲锋/收招、Mark/Momentum、群体/手动目标、forced discard。

- [ ] **Step 2: 所有 198 卡合法候选**

复用 AllCard runtime夹具，policy不得选出 Adapter拒绝动作；不是要求每张卡一定被选，而是每个被评分的 candidate能真实resolve且输入state不变。

- [ ] **Step 3: 禁止治疗粉**

对新 AutoBattle 文件执行 `rg -n "HealingPowder|治疗粉"`，预期0命中。现有旧系统命中不在本包顺手清除，但自动战斗不得消费它。

- [ ] **Step 4: Simulation邻测**

`GameXXK.RouteBalance.FullMatrixExecution` 与 policy确定性必须继续 green；如强度结果变化，先证明是提取差异bug，不能借机调卡数值。

## Task 7: 真实首次通关 PIE

- [ ] **Step 1: harness 新模式**

玩家手选 route node→进入battle→打开自动→中途切2×→请求关闭并等drain→手动再开→打到terminal→玩家手选reward/next route。harness禁止自动点击 route/event/shop/reward。

- [ ] **Step 2: probe**

记录每步 Decision、合法 preview、实际 Card/Target、queue counts、phase、speed、impact/continuation counts、state hash。截图控件与关键机制。

- [ ] **Step 3: Defeat flow**

固定弱队 seed失败后停在 defeat UI，确认没有自动retry/exit。

- [ ] **Step 4: 最终组**

```text
GameXXK.AutoBattle
GameXXK.Data.CardBattleRuntime
GameXXK.Data.CardOutcomePreview
GameXXK.Integration.CardBattle
GameXXK.UI.Battle
GameXXK.RouteBalance
```

- [ ] **Step 5: 状态文档/push**

创建 `docs/production/2026-08-13-auto-battle-status.md`，报告合法率、稳定 replay、1×/2×等价、真实 PIE；不宣称自动路线或离线完整战斗。
