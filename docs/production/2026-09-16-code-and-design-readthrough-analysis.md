# 项目代码与设计通读分析（2026-09-16）

范围：`main` 工作区，201 个未提交改动，`SaveVersion = 47`，HEAD `edf280cb`。
方法：读全部交接文档与 2026-09 设计与生产记录，加**代码级走查**（行号可复核）。本轮只读，未改任何运行时代码。

上一篇 `2026-09-16-next-work-feasibility-analysis.md` 回答“做什么”。本篇回答“**这个工程实际是怎么长的、哪里会坏、为什么坏**”。

---

## 1. 工程实体画像

| 维度 | 实测 |
|---|---|
| C++ 源文件 | 741 个（`.h/.cpp/.inl`），约 266,600 行 |
| 自动化测试文件 | 326 个（`Source/GameXXK/Private/Tests`） |
| Python 脚本 | `scripts/` 247 个 `.py`，其中 `test_*.py` 113 个 |
| 引擎 | UE 5.8（`D:\UE_5.8`），`Build.bat`／`UnrealEditor-Cmd.exe` 在位 |
| 增量冷 UBT | 约 47 秒（UBA 加速，实测） |
| 存档 schema | v47，每个版本一个 `*IntroducedSaveVersion` 常量 |
| 默认地图 | `/Game/GameXXK/Maps/L_DesktopTrainingHUD`（纯 2D 桌面） |
| 编辑器状态 | 未运行，无脏包 |

### 1.1 单文件体量分布（这是全部结构问题的根源）

| 文件 | 大小 | 说明 |
|---|---:|---|
| `GameXXKCardRules.cpp` | 702 KB | 卡牌结算规则，**最大单文件** |
| `UI/GameXXKDesktopTrainingWorkbenchWidget.cpp` | 482 KB / 11,594 行 | 桌面工作台，**程序化构建整个布局** |
| `UI/GameXXKBattleBoardWidget.cpp` | 456 KB | 全屏战斗板 |
| `Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp` | 370 KB | 单文件测试超过多数引擎模块 |
| `UI/GameXXKInventoryWindowWidget.cpp` | 203 KB / 4,867 行 | 背包/仓库/卡组编辑 |
| `MVP/GameXXKMVPSubsystem.cpp` | 225 KB / 6,301 行 | 应用层唯一权威状态持有者 |

交接文档 §1 自己承认过“不是已经彻底分层的框架模板”，实测确实如此：`MVPSubsystem`、`CardRules`、`DesktopTrainingWorkbenchWidget`、`BattleBoardWidget` 四个文件合计约 1.8 MB，承担了绝大部分职责。

---

## 2. 真实的系统结构（走查所得，非文档复述）

### 2.1 分层与依赖方向

```
玩家输入
  └─ UI 层：工作台(11.6k行) / 战斗板 / 背包窗口(4.9k行) / 路线图
       └─ 应用层：UGameXXKMVPSubsystem（唯一权威 FGameXXKRuntimeState）
            └─ 规则层：Card / Training / Equipment / Talent / PartyFormation / …
                 └─ 目录层：CardCatalog / EnemyCatalog / CompanionCatalog / AcademyCatalog
       └─ 存档层：SaveMigration(v47) / SaveStorage（完整性封装 + 3份轮换）
```

**关键结构事实（比文档更重要）：**

1. **规则层已经拆得很干净。** `GameXXKCardQualityRules`、`GameXXKPartyFormationRules`、`GameXXKCharacterStatRules`、`GameXXKTrainingRules` 等都是纯静态规则类，没有 UI 依赖，可被测试直接调用。这一层质量是好的。
2. **UI 层是程序化构建的，不是 UMG 蓝图。** 工作台和背包的所有控件都由 C++ 逐行 `ConstructWidget` + `AddCanvas` 拼出来。这是理解后面所有 UI 缺陷的前提。
3. **`FGameXXKRuntimeState` 是唯一权威状态**，`CardRun` 同时装永久与局内数据（官方文档自己标了这个陷阱）。

### 2.2 品质→效果的**唯一**换算入口

```
FGameXXKCardQualityRules::BuildEffectiveDefinition(BaseDefinition, CurrentQuality)
```
（`Public/GameXXKCardQualityRules.h:12`，实现 `Private/GameXXKCardQualityRules.cpp:293`）

它返回一份**缩放副本，不改目录对象**（符合 §3.1“目录层不能被玩家升级改写”）。全工程 **54 处**调用它：

| 消费方 | 调用数 |
|---|---:|
| `GameXXKCardRules.cpp`（战斗结算） | 20 |
| `GameXXKCardText.cpp`（卡面/Tooltip 文案） | 6 |
| `GameXXKBattleBoardWidget.cpp`（显示） | 3 |
| `GameXXKSaveMigration.cpp:2058`（存档迁移） | 1 |
| 其余测试与预览 | 24 |

**工程含义**：任何“卡牌成长”维度（品质、以及待做的技能点等级）只要并入这一个函数，战斗、文案、显示、存档会**同时**拿到正确值，不存在漏改某一条管线的问题。这是技能点系统风险远低于账面印象的直接原因。

---

## 3. 四条主流程走查

### 3.1 游历（挂机）

`StartTrainingTravel → AdvanceTrainingTravelStep/Encounter → 遭遇结算 → 记账 → TravelVisualRuntime 展示`

- 游历是**低成本的重复小战斗**，不走 BattleBoard 的玩家手牌回合。
- 开箱、工具教学、看背包**不暂停**游历；挑战**暂停**游历。
- 表现层 `GameXXKTravelLootWidget` + `GameXXKRewardPresentation` 负责掉落飞行。

### 3.2 挑战（主动卡牌战斗）

```
选关 → StartTrainingChallenge（暂停游历）→ 路线图 → 选怪物
  → BattleBoard（全屏）→ 胜利 → 固定奖励候选 → 回路线图 → … → Boss 结算 → 回工作台
```

链路横跨 `MVPSubsystem`（事务）→ `CardBattleAdapter`（提交结果）→ `CardRules`（规则）→ `BattleBoardWidget`（演出）。**动画队列播放期间必须保护会改业务状态的输入**——这是 §6.1 明确写下的约束，也是历史上“重复扣血/二次出牌”类缺陷的来源。

### 3.3 主线任务战

`StartTask → BeginTaskJourney/对白 → BeginTaskBattle → 真实战斗 → 战后对白 → Completed → ClaimReward → Rewarded`

三种结算凭据**身份不同、必须分别走原事务**：
1. 训练结算凭据（已支付，确认只做确认与清理展示）
2. 路线终结凭据（待应用/已应用）
3. 任务领取（任务自身状态）

### 3.4 桌面工作台的布局重建机制 ← **本篇最重要的发现**

这是理解 A02/A09 类缺陷的钥匙。工作台不是“构建一次就稳定”，而是：

```
RefreshLayout()                                  // :8455
  ├─ if (bInternalLayoutRebuild) → 标 pending，return      // :8457
  ├─ if (GetWorld() || bNativeTickActive || bInActionCallback)
  │     → bLayoutRefreshPending = true                     // :8462
  │     → 用 SetTimerForNextTick 延到下一帧                 // :8470
  └─ else RebuildLayoutNow()                               // :8490

RebuildLayoutNow()                               // :8493
  → BuildProgrammaticLayout()                    // :8500
      ├─ ClearRebuiltPanelChildren(RootCanvas)   // :4397
      ├─ StageButtons.Reset(); ActionButtons.Reset()
      └─ BuildWorkbenchShell() → 逐面板重建
```

`RefreshLayout()` 在全文件有 **114 个调用点**。

**每次 `BuildProgrammaticLayout()` 会连带重建挂载在里面的子窗口：**

```cpp
// :7424  BuildBackpackPanel 内
EmbeddedInventoryWidget = WidgetTree->ConstructWidget<UGameXXKInventoryWindowWidget>(
    UGameXXKInventoryWindowWidget::StaticClass(), TEXT("EmbeddedApprovedBackpack"));
...
// :4349  BuildProgrammaticLayout 顶部
EmbeddedInventoryWidget = nullptr;
```

也就是说：**工作台每次结构刷新，背包窗口被整体销毁重建，连带 36 张卡组卡牌按钮及其 Tooltip。**

工程自己也意识到了这点，并加了测试保证“一次点击只触发一次重建”：

```cpp
GameXXKDesktopChildRefreshTest.cpp:38   TestEqual(TEXT("the next game tick applies one structural refresh"), …, Before+1);
GameXXKDesktopTrainingWorkbenchWidgetTest.cpp:5093  TestFalse(TEXT("travel NativeTick schedules no layout rebuild"), …);
GameXXKDesktopTrainingWorkbenchWidgetTest.cpp:2798  TestEqual(TEXT("numeric refresh does not rebuild the layout"), …);
```

**结论**：重建频率**不是**无界循环（我核实过 `bLayoutRebuildScheduled` 有正确的置位/清位配对，不会每帧转）。但**每次重建的代价是整棵子树**，且**重建会摧毁正在交互的控件**。这正好解释了两个玩家可见症状。

---

## 4. A02 的真实机制：Shift 不是多选，是“详述模式”

待办表把 A02 写成「卡组页 Shift 点击多次后卡住」，此前一直按“Shift 多选修饰键”方向排查。**代码里根本没有 Shift 多选。**

全工程 Shift 只有一种语义：**按住 Shift 把卡牌 Tooltip 从简版切成详述版**。

```
GameXXKCardTooltipInteraction.cpp:17
  Mode = !bHovered            ? Compact
       : bShiftDown           ? Detail
       : bPillsOpen           ? Pills
       :                        Compact;
```

Shift 的实现方式是 **Windows 物理键轮询，不走 Slate 事件**：

```cpp
// GameXXKCardTooltipWidget.cpp:58
bool UGameXXKCardTooltipWidget::IsPhysicalShiftDown()
{
    return (::GetAsyncKeyState(VK_LSHIFT) & 0x8000) != 0
        || (::GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0;
}
```

背包页每个 tick 由宿主统一同步（**注释本身就说明了它会漏 key-up**）：

```cpp
// GameXXKInventoryWindowWidget.cpp:234
/** Host heartbeat that enforces physical Shift even if a nested Tooltip misses key-up. */
// :949
void UGameXXKInventoryWindowWidget::NativeTick(...)
{ Super::NativeTick(...); SynchronizeCardTooltipShiftState(); }
// :955
{ const bool bShiftExpanded = ...IsPhysicalShiftDown();
  bCardTooltipShiftExpanded = bShiftExpanded;
  for (每个 HeroDeckTooltipWidgets[i])
      Tooltip->UpdateInspectionFromOwner(bWindowActive && Button->IsHovered(),
                                        bShiftExpanded, bControlDown, bEscapeDown); }
```

有一个正确的去抖护栏：

```cpp
// GameXXKCardTooltipWidget.cpp:366
if (!bForce && PresentedLanguageRevision==LanguageRevision
    && bExpanded==bResolvedExpanded && bPillHelpDisplayed==bResolvedPills) return;
```

所以 Shift 反复按**不会**把 Tooltip 重建刷爆。

### 4.1 组合起来就是“卡住”

把三条事实叠起来：

1. 背包窗口在**每次工作台结构刷新时被销毁重建**（§3.4）。
2. 卡组页的 Tooltip 状态是**逐 tick 轮询物理 Shift**，且与 `Button->IsHovered()` 绑定。
3. 卡牌点击 `HandleHeroDeckCardClicked → ToggleHeroDeckCardForTest → RefreshHeroDeckCards()`，而切换会改变携带/候选集合，**足以触发父级结构刷新**。

于是：**按住 Shift 连点卡牌 → 每次点击都可能触发一次父级重建 → 重建把正在悬停/点击的按钮和它的 Tooltip 一起销毁 → 物理 Shift 仍按着，但新的控件数组与旧的悬停索引对不上 → 交互状态丢失，表现为“卡住 / 点不动”。**

### 4.2 测试盲区（这是它能存活到用户手上的原因）

- 覆盖卡组页 Shift 行为的测试：**0 个**。全工程 Shift 相关测试只在 `GameXXKCardTooltipReadingTest`（纯状态机）和 `GameXXKCardBattleBoardWidgetTest:4038`（注释里提了一句物理 Shift）。
- `GameXXKFormationDeckInteractionTest.cpp` 覆盖了编队入口的卡组交互，但**不覆盖 Shift**。
- 也就是说：**状态机有测试，物理键轮询与 UI 重建的交互没有测试。**

### 4.3 修复方向（按风险从低到高）

1. **把 Shift 从 `GetAsyncKeyState` 轮询改成 Slate 修饰键事件**（`FSlateApplication::Get().GetModifierKeys()` 加 key-up 处理），消除“漏 key-up”这个已被注释承认的缺陷。改动局部，且有现成状态机测试护航。
2. **让卡组卡牌的 Tooltip 不被父级重建摧毁**：把 36 张卡片的 Tooltip 提到不随内容页重建的外层（这也是奖励飞行 `RewardPresentation` 已经用过的做法——“飞行层放在不随内容页重建而删除的外层”）。
3. **点击处理期间禁止结构重建**：复用已有的 `bInActionCallback` / `FScopedActionCallbackGuard`（`GameXXKDesktopTrainingWorkbenchWidget.cpp:1556`），让卡牌点击走同一个保护。

建议 **1 先做**（可独立验证），2 作为结构性收敛。

---

## 5. A09 的真实机制：不是点击问题，是业务构建失败

已在 `2026-09-16-regression-baseline-and-academy-blocker.md` 记录，此处只给结论链：

```
BuildLoadout(:14) 借全新档 → (:31) 指定 Npc.TusiChief/课程NpcId
  → (:40) SetQuestNpcForCurrentRun
    → PartyFormationRules.cpp:318 要求 IsSlotUnlocked(QuestNpc)
      → PartyFormationRules.cpp:217 要求 SlotTalentId 天赋 rank > 0
        → 全新档 rank = 0 → 失败
      → :321 报错 "Selected NPC is not one of the six owned definitions."
  → AcademySubsystem.cpp:40 return false
  → WorkbenchWidget.cpp:10640 只 SetNotice，无其它反馈
```

`GameXXKAcademyCatalog.cpp` 里 13 门课**每一门都带 NPC**，全部走这条路径 → **13 门课全坏，含主角课**。

时序归因：09-15 引入渐进出战槽（v43）把 NPC 槽改成 200 金币天赋购买，`PartyFormationRules.cpp:318` 的校验随之加入；学院的状态构造器仍按旧前提（NPC 槽默认可用）写。**回归，不是新需求。**

---

## 6. 安全网的真实状态：覆盖不均

| 层 | 覆盖情况 |
|---|---|
| 规则层（品质/阵型/训练/属性） | **好**。大量精确断言，逐卡逐机制 |
| 存档迁移 | **好**。每版一个迁移测试 |
| 战斗运行时 | **好**。意图矩阵 351 组、逐职业逐机制 |
| 工作台布局重建 | **中**。有 build-count 测试保证“一次点击一次重建”，但**不覆盖重建期间的输入** |
| 物理修饰键 × UI 重建 | **空白**（§4.2） |
| 学院课程战斗 | **有测试且正在红**（4 项失败） |
| 全量回归 | **当前跑不完**：`GameXXKEnemyManaSiphonTest.cpp:60` 空指针崩溃终止进程 |

崩溃细节：`Find(State,Party[0])->Mana=3`，`Find` 按 `UnitId` 查找且**不判空**，而 `Party[0]` 是字符串 `"Hero"`，单位 ID 由 `BeginCardBattle` 生成 → 必然 `nullptr` → 写 `0xd0` 访问违例。同文件 `:72/:73/:74` 复用同一返回值。

**影响**：`Saved/Automation` 最后一份完整报告停在 **09-12**。09-13～09-16 的全部改动（左右布局、宝石箭头、敌方灼烧、教学箱、奖励飞行、首战引导）**没有全量基线**。这不是“一条测试红”，是**回归能力本身缺失**。

---

## 7. 文档 vs 代码：三处口径已经过期

我逐条核对了 `2026-09-16-onboarding-coverage-audit.md` 的五个 blocker，发现其中两条**对当前代码已经失效**：

| audit 记载 | 代码实际 | 判定 |
|---|---|---|
| 「`OfferPartyProgressionGuide` 要求 `Preference == NewPlayer`，新档默认 `Unset` 被阻断，测试直接设成 NewPlayer 因此不能证明默认新档能触发」 | `GameXXKDesktopTrainingWorkbenchWidget.cpp:1882` 只在 `== ExperiencedPlayer` 时 return。**默认 `Unset` 是放行的**。要求 `NewPlayer` 的是 `HandleGuideEvent`（`:5518`，旧叙事引导，另一条链） | **已过期** |
| 「`OfferedPartyProgressionGuides` 未找到清空调用，F10 重置后不再自动提示」 | `ResetPresentationForNewGame()` 在 `:2643` 调用 `OfferedPartyProgressionGuides.Reset();` | **已过期** |
| 「`HandleGuideEvent` 额外要求 `Step.Main.XuXiake.CombatTutorial`」 | `:5541` 属实 | 仍有效 |
| 「学习证据模型未统一」 | 属实 | 仍有效 |
| 「计划含旧开场段落」 | 属实（纯文档问题） | 仍有效 |

**这很重要**：audit 的“优先顺序建议 1”是“先修默认触发条件与 F10 去重”。按当前代码，这两条**已经不需要修了**。如果照着过期清单施工，会重做已经做完的事，还可能把正确的 `!= ExperiencedPlayer` 逻辑改成 `== NewPlayer`，反而**制造**一个“默认新档被阻断”的真 bug。

另外确认：`docs/superpowers/plans/2026-09-15-full-onboarding-rollout.md` 建议新建的 `Guide/GameXXKOnboardingCatalog.*` 与 `Guide/GameXXKOnboardingRules.*` **尚不存在**（全库搜 `Onboarding` 无源码命中）。统一派发层确实还没做。

---

## 8. 结构风险排序（按“会不会再咬人”）

| # | 风险 | 证据 | 影响 |
|---|---|---|---|
| R1 | 回归能力缺失 | `GameXXKEnemyManaSiphonTest.cpp:60` 崩溃终止全量 | **所有后续改动都无法自证** |
| R2 | 学院课程全坏 | §5 根因链 | 玩家可见功能整块失效 |
| R3 | 布局重建摧毁交互中的控件 | §3.4 + §4 | 表现为随机“点不动/卡住”，难复现难归因 |
| R4 | 物理修饰键轮询脱离 Slate 事件 | `IsPhysicalShiftDown` + 注释承认漏 key-up | Shift 类交互天生不可靠 |
| R5 | 巨型 UI 文件 | 工作台 11.6k 行 / 114 个 RefreshLayout 调用点 | 每次改动都容易碰到重建路径 |
| R6 | 文档口径漂移 | §7 两条已过期 | 照过期清单施工会主动引入缺陷 |

R3/R4 是同一类问题的两个面：**输入状态活在 UI 控件的生命周期里，而 UI 控件会被整棵树重建。** 这是这个工程最值得优先收敛的结构性问题。

---

## 9. 我的建议（修正版，覆盖上一篇的批次）

上一篇的批 A 方向不变，但**优先级和内容按本次通读调整**：

**批 A0（最高，先做）**
1. 修 `GameXXKEnemyManaSiphonTest` 崩溃 → 恢复全量回归能力。
2. 重跑全量 → 拿到 09-16 基线。
   > 先做 1 才能做 2。没有基线，后面做的任何事都无法证明没引入失败。

**批 A（玩家可用性）**
3. 修学院 NPC 槽根因（§5）→ 13 门课恢复。
4. 修 Shift 机制：改 Slate 事件（§4.3-1）+ 补“物理键 × 重建”测试。
5. 视情况做 Tooltip 提层（§4.3-2）。

**批 B（设置）**：A14 音效音量（BGM 待确认）。
**批 C（成长）**：技能点——先做数据层与 `BuildEffectiveDefinition` 增维，等你给 A04 表。
**批 D（引导）**：A11/A12/A13/A15、D01/D02。
**批 E（文档纠偏）**：把 §7 两条过期口径更新，避免后续照错清单施工。

---

## 10. 仍然需要你定的三件事

1. **学院课程权限**：A 借用态绕过 NPC 槽门槛（13 门课立刻可开）／B 未解锁槽的课置灰？
2. **BGM**：有没有资源计划？（没有则 A14 只做音效音量）
3. **优先级**：同意“A0（修崩溃+建基线）→ A（学院+Shift）”，还是先做别的？

---

## 附：本轮新增/引用的证据

- 本篇：`docs/production/2026-09-16-code-and-design-readthrough-analysis.md`
- 实测基线：`docs/production/2026-09-16-regression-baseline-and-academy-blocker.md`
- 可复现脚本：`Saved/Baseline-20260916/run_checks.py`
- 崩溃前逐条状态：`Saved/Baseline-20260916/completed-before-crash.tsv`

**本轮未修改任何运行时代码、未动资源与存档、未启动可见编辑器。** 全部结论来自源码走查与一次隔离的无窗口 Automation 运行。
