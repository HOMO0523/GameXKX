# 2026-09-16 A0+学院+Shift 批次：实现与验证

状态：本轮已实现并通过定向验证；回归基线已建立。工作区 `main`，未提交前基线为 `02917d62`。

## 本轮范围（用户已确认：先 A0 → 再 A；学院权限取 B 置灰）

1. A0-1：修 `GameXXKEnemyManaSiphonTest` 空指针崩溃，恢复全量回归能力。
2. A0-2：重跑全量 `GameXXK` 自动化，建立 09-16 基线。
3. A-1：学院课程权限改为 **B（未满足前置的课置灰并给出原因）**。
4. A-2：Shift 从 Windows 物理键轮询改为 Slate 修饰键，并补「物理键 × 重建」测试。
5. 文档纠偏：更正 `2026-09-16-onboarding-coverage-audit.md` 中两条对当前代码已失效的 blocker。

---

## 1. A0-1 崩溃修复

### 现象

全量自动化在 `GameXXKEnemyManaSiphonTest.cpp:60` 触发
`EXCEPTION_ACCESS_VIOLATION writing address 0x00000000000000d0`，
**直接终止 UnrealEditor-Cmd 进程**，全量只能跑完 22 项，回归能力事实上缺失。

### 三个叠加缺陷

| # | 缺陷 | 证据 |
|---|---|---|
| 1 | 测试把英雄的**卡牌归属键** `"Hero"` 当成**战斗单位 ID** 传入 `Find()` | 战斗单位 ID 实为 `FGameXXKEquipmentRules::HeroCharacterId()` = `"Player"`（`GameXXKEquipmentRules.cpp:538`；适配器 `GameXXKCardBattleAdapter.cpp:35` 亦为 `TEXT("Player")`） |
| 2 | `Find()` 返回指针且**调用点不判空** | `GameXXKEnemyManaSiphonTest.cpp:18-21`，`:60/:72/:73/:74` 直接解引用 |
| 3 | 编队放了**两个 `PermanentCompanion`**，而编队契约最多一位 | `FGameXXKPartyFormationRules` 的 `ProjectCompatibility` 只激活第一位，第二位不会被实例化 |

缺陷 1 被修掉后，缺陷 3 才显形——新增的守卫把它报成测试错误而不是崩溃：

```
siphon fixture is missing party unit CompanionInstance.Companion_Healer_01.0000411F
```

### 修复

- 英雄用 `FGameXXKEquipmentRules::HeroCharacterId()`，不再硬编码。
- 第三名成员改用 **QuestNpc 槽**（`Npc.YueBai`），符合“至多一位常驻伙伴”的契约；显式传入三张**有内力消耗**的卡，避免固定种子抽牌抽到唯一的 0 费牌。
- `Prepare()` 增加“三名成员都必须实际实例化”的守卫：再出现同类错配会**报测试失败而不是崩溃进程**。
- `SaveResumeAndCardLegality` 原本只翻弃牌堆找可出牌，抽牌运气不好就找不到；改为通过生产入口 `GameXXKCardRules::DrawCards` 发真实手牌再查找，并补一个**对照断言**（回复内力时该牌合法），证明失败确实来自内力而非能量/目标/阶段。
- 顺带修正该用例原本 `Preview.FailureReason` 读法错误：卡不合法时 `BuildCardPlayPreview` **返回 false** 而非填充 preview。

### 验证

`Saved/Baseline-20260916/siphon-fix7/index.json` → **3/3 通过，0 错误**，进程退出码 0（不再崩溃）。

---

## 2. A0-2 全量基线（09-16）

`Saved/Baseline-20260916/baseline-full2/index.json`：

| 指标 | 数值 |
|---|---:|
| discovered | 1258 |
| succeeded | 1088 |
| **failed** | **170** |
| warnings | 606 |

对照：`Saved/Automation` 中最后一份完整报告停在 **2026-09-12**（`FontRollout20260912`）。09-13～09-16 的全部改动此前没有任何全量基线。

这 170 项为**既有失败**，不是本批引入。已另开专项 triage（`docs/production/2026-09-16-baseline-failure-triage.md`）。

### 分诊修正（勿用本文早先的口径）

专项分诊对 170 项的结论，修正了本节早先的两处判断：

1. **`CurrentMapId` 不是独立缺陷。** 没有任何代码覆盖它。29 条 map-identity 断言失败**纯粹是级联**：
   `GameXXKMVPSubsystem.cpp:6297` 在 `BuildLoadout` 失败后直接 return，
   `GameXXKAcademyStateBuilder.cpp:65` 根本没执行，状态保留 `CreateNewGame` 的 `MainMenu`。
   `index.json` 显示 109/110/112 三行各 29 次失败，与构建失败 **1:1 完全对应**。
2. **170 项并非全部来自 09-13～16 窗口。** 分诊恢复了 09-12 基线
   （`Saved/Automation/FontRollout20260912`，覆盖 682/1258）并回查 3369 份历史报告：
   **16 项在 09-12 就已是 Fail**、**74 项历史上从未有过成功记录**、**93 项上一次为绿**。
   且 09-12 基线只有 9 个前缀，**未覆盖** `MVP.Battle.*`、`Presentation.*`、`UI.*`、`Interaction.*`，
   因此动画节奏、锚点、本地化那几组**不能**用“09-12 是绿的”推断。

最大单项收益：**A1+A2（渐进出战槽门禁与编队塌缩）共 74 项（44%）**，
根因是**夹具过时而非设计缺陷**（门禁是有意设计，已有两项通过测试证明），
正确夹具已存在于 `GameXXKPermanentPartyTestFixtures.h:36`。

---

## 3. A-1 学院课程权限（方案 B）

### 根因（已在上一轮定位，本轮修复）

`GameXXKAcademyStateBuilder::BuildLoadout` 借用 `UGameXXKMVPRules::CreateNewGame()` 的全新档状态，
而新档 `bProgressivePartySlots == true` 且槽位天赋 rank 为 0 →
`FGameXXKPartyFormationRules::SetQuestNpc`（`:318`）判定槽位未解锁 →
`AcademySubsystem.cpp:40` 直接 `return false` → 工作台只 `SetNotice`。
13 门课**每一门都带 NPC**，因此**全部构建失败**，表现就是玩家说的“点击不了”。

`CurrentMapId == "AcademyBattle"` 的 29 条断言失败是**级联**：`BuildLoadout` 失败后
`BuildEncounter`（唯一设置该字段的地方，`GameXXKAcademyStateBuilder.cpp:59`）根本没执行。

### 实现

**新增资格规则**（`FGameXXKAcademyRules::EvaluateEligibility`，`GameXXKAcademyRules.h/.cpp`）：
对玩家**真实状态**逐个课程判定，返回可用性与首个阻塞原因：

| 阻塞 | 原因文案 |
|---|---|
| `ActiveBattle` | 请先结束当前战斗。 |
| `CompanionSlotLocked` | 需要先解锁伙伴出战槽。 |
| `CompanionNotRecruited` | 需要先招募该职业的伙伴。 |
| `NpcSlotLocked` | 需要先解锁任务伙伴出战槽。 |
| `NpcNotOwned` | 尚未认识这位任务伙伴。 |
| `CourseMissing` | 教学任务不存在。 |

**界面（工作台学院抽屉）**：
- 不可用课程的整行**置灰且不可点**（`SetIsEnabled(false)`，头像与文字降至 45% 不透明度）。
- 悬停提示改为**缺失的具体前置**，不再是课程简介。
- 选中课程不可用时，「开始教程」按钮**禁用**，并在其下方显示同一条原因文本。

**防御**：`UGameXXKAcademySubsystem::BeginCourse` 在构建前调用同一资格函数，
无法从任何其它入口绕过界面门。

**借用态修复**：`BuildLoadout` 明确把借用状态的 `bProgressivePartySlots` 置为 `false`，
并注明理由——课程是隔离教学战斗，玩家的真实资格**已由界面门校验**；
把 onboarding 门再套到一次性借用状态上，只会让每门课都构建失败。

### 验证

`Saved/Baseline-20260916/academy-b2/index.json`：`GameXXK.Academy` **6 项中 4 项通过**。

- 修复前失败、现通过：`GameXXK.Academy.CatalogAndLoadouts`、`GameXXK.Academy.IndependentEncounter`。
- 仍失败 2 项：`GameXXK.Academy.GuidedObjectives`、`GameXXK.Academy.PlayableObjectives`。

### 剩余 2 项的性质（未宣称完成）

两项失败原因相同，且**在修复前被 “Selected NPC” 报错掩盖**，属新暴露问题：

```
Academy.Sorcerer/2 won=1 missing=任务重放;
```

即：自动化试玩能**打赢**该节，但无法满足法师第 3 节的「完成五牌任务并触发重放」目标
（`Goal(G::SpellTask, …)`）。这更像**测试内自动出牌策略过于朴素**，不足以完成需要连续五张不同法师牌的任务链，
而不是运行时缺陷；但**尚未证实**。本轮不把它算作通过，也不改测试去掩盖。

---

## 4. A-2 Shift 机制

### 根因

全工程 Shift 只有一种语义：**按住 Shift 把卡牌 Tooltip 从简版切成详述版**（`GameXXKCardTooltipInteraction.cpp:17`），
并非多选修饰键。而它此前是**Windows 物理键轮询**：

```cpp
// 旧实现
return (::GetAsyncKeyState(VK_LSHIFT) & 0x8000) != 0 || ...;
```

宿主的同步注释本身就承认会漏 key-up：

```cpp
/** Host heartbeat that enforces physical Shift even if a nested Tooltip misses key-up. */
```

叠加“工作台每次结构刷新会整体销毁重建背包窗口（含 36 张卡牌按钮与其 Tooltip）”，
按住 Shift 连点卡牌时，重建会摧毁正在交互的控件；物理键仍按着，
但新控件与旧悬停索引错位，交互状态丢失 → 表现为“卡住”。

### 实现

- `IsPhysicalShiftDown()` / `IsPhysicalControlDown()` 改为读取 **Slate 自身**的修饰键状态
  （`FSlateApplication::Get().GetModifierKeys()`），使按下与松开都与投递 hover/key 事件的**同一事件系统**一致；
  未初始化 Slate 时返回未按下，不再访问物理键盘。
- `IsPhysicalEscapeDown()` **保持原样**（Escape 非修饰键，不在本次范围，避免夹带无关行为变更）。
- 新增测试 `GameXXK.UI.CardTooltip.RebuildRecovery`，锁定两条使其可恢复的性质：
  1. **丢失悬停必须清除模式**，即使 Shift 仍被报告为按下——这正是重建摧毁悬停按钮、key-up 未被观察到的情形；
  2. **重建后的 Tooltip 是全新对象**，不得继承旧实例的 mode，且仍能正确读取当时按住的 Shift。
- 另加“修饰键读取器不闭锁”断言（连续两次读取一致），可在无键盘的 headless 环境安全运行。

### 验证

- `Saved/Baseline-20260916/shift-test/index.json`：`GameXXK.UI.CardTooltip` **12/12 通过**。
- `Saved/Baseline-20260916/shift-wide/index.json`（覆盖 UI / Academy / DesktopTraining /
  CardBattle / Battle / MetaShop，共 234 项）：
  **新增失败 0**；且有 3 项由失败转通过
  （`GameXXK.Academy.CatalogAndLoadouts`、`GameXXK.Academy.IndependentEncounter`、
  `GameXXK.UI.CompanionRoster.DuplicateRecruitmentFeedback`）。

即：Shift 改动**没有引入任何新失败**，学院修复**净修好 3 项**。

---

## 5. 文档纠偏

`docs/production/2026-09-16-onboarding-coverage-audit.md` 顶部新增复核更正块：
第 1、3 条 blocker 对当前代码已失效（`OfferPartyProgressionGuide` 只在 `ExperiencedPlayer` 时阻断；
`OfferedPartyProgressionGuides` 已在 `ResetPresentationForNewGame` 中清理），
并说明照原文施工会主动引入缺陷。第 2、4、5 条仍然有效。

---

## 6. 验证方法与环境

- 冷 UBT（`Build.bat … -NoHotReload -NoUBTMakefiles -gather`），每步均 `BUILD_EXIT 0`；
  未使用 Live Coding / Hot Reload。
- 自动化用隔离 `-UserDir` 与 `-NullRHI` 无窗口运行，未触碰玩家存档：
  `Saved/Baseline-20260916/run_checks.py`，构建 `Saved/A0-20260916/build.py`。
- 本轮**未启动可见编辑器、未做真实 PIE**。学院置灰与 Shift 的**实机视觉/手感验收仍未进行**，
  因此不宣称这两项的玩家侧验收完成。

## 7. 本轮未完成 / 明确的边界

- 学院 2 项目标断言仍红（见 §3 末），性质待定，未掩盖。
- 全量 170 项既有失败中，除学院外均未修（专项 triage 另出）。
- 学院置灰、Shift 改动的**真实鼠标 PIE 复核**待做。
- A14/A13（音量）尚未开始，取决于用户是否给 BGM 资源结论。
