# 后续可制作项：可行性分析

日期：2026-09-16。范围：`main` 工作区（含 201 个未提交改动）。
依据：`docs/handoff/*`、`docs/design/2026-09-1*`、`docs/production/current-goal-acceptance.md`，加本轮**新取的两条代码级证据**（见 §2、§3）。

本文只回答一个问题：**按当前代码、资源与验证能力，哪些后续项是我可以真正做完并证明的。**
不把“有底层类”“有文档”“有静态稿”算成可交付。待办编号沿用 `docs/handoff/03-backlog-and-acceptance.md`。

---

## 1. 结论摘要

| 判断 | 项目 |
|---|---|
| **本轮就能闭环，且能自证** | A09 学院课程（真实根因已定位）、A02 卡组 Shift 卡死、A10 背包/仓库排序 |
| **代码可做，但卡在用户数值/规则定稿** | A03–A08 技能点升级闭环（接入点已定位，目录表待填） |
| **只缺“参数定稿”，功能可做完** | A14 音效/BGM 独立音量（但工程当前**没有 BGM**，见 §6） |
| **必须先修基础设施，否则无法回归** | `GameXXKEnemyManaSiphonTest` 崩溃（见 §3） |
| **我给不了** | A01 逐卡文案、B01–B10 全部美术/图鉴、A11/A12/A13/A15 引导的**资源**部分、B05–B07 女主、C01–C04 |

验证能力已实测可用：`D:\UE_5.8` 下 `Build.bat`／`UnrealEditor-Cmd.exe` 均在位；增量冷 UBT 约 47 秒（UBA），可支撑“先 RED → 改 → GREEN”的紧凑迭代；工程现有 326 个 C++ 自动化测试文件、113 个 `scripts/test_*.py` 脚本测试。编辑器当前未运行，无脏包风险。

一句话：**当前最该做的不是加新功能，而是先把“课程点不动”和“回归跑不完”这两件事解决**——它们一个堵玩家，一个堵我。

---

## 2. 新证据一：学院 13 门课的战斗构建 100% 失败

### 2.1 实测

本轮跑了一次隔离的 `Automation RunTests GameXXK`。学院相关 4 项全部 Fail，日志原文：

```
Academy.Basic: Selected NPC is not one of the six owned definitions.
Expected 'Academy.Basic lesson 0 builds: ...' to be true.
```

失败清单：

```
Fail | GameXXK.Academy.CatalogAndLoadouts
Fail | GameXXK.Academy.GuidedObjectives
Fail | GameXXK.Academy.IndependentEncounter
Fail | GameXXK.Academy.PlayableObjectives
```

### 2.2 根因链（已逐跳到行）

1. `GameXXKAcademyStateBuilder::BuildLoadout` 第 14 行：`State = UGameXXKMVPRules::CreateNewGame();`
   ——借用一个**全新档**状态，没有任何天赋购买记录。
2. 第 31 行直接指定 NPC：`Npc.TusiChief` 兜底或课程自己的 `NpcId`。
3. 第 40 行调 `SetQuestNpcForCurrentRun(State, Npc, NpcCards, &Error)`。
4. 走到 `GameXXKPartyFormationRules::SetQuestNpc` 第 318 行的前置校验，不通过即第 321 行报错：
   ```cpp
   if (!QuestNpcId.IsNone() && (!IsSlotUnlocked(InOutState, EGameXXKPartyMemberKind::QuestNpc)
       || !FGameXXKCompanionCatalog::FindQuestNpcDefinition(QuestNpcId)))
   ```
5. `IsSlotUnlocked`（同文件第 217 行）在 `bProgressivePartySlots` 为真时要求槽位天赋已购买：
   ```cpp
   return IsSlotEligible(State, Kind)
       && (Kind == EGameXXKPartyMemberKind::Hero || !State.Training.bProgressivePartySlots
           || State.Talents.NodeRanks.FindRef(SlotTalentId(Kind)) > 0);
   ```
   新档 `bProgressivePartySlots == true`，NPC 槽天赋 rank 为 0 → 未解锁 → 失败。

不是 `FindQuestNpcDefinition` 找不到——`Npc.TusiChief` 等 6 个定义在 `GameXXKCompanionCatalog.cpp:82-87` 都齐全。**是槽位没解锁。**

### 2.3 为什么这等于用户在说的“点击不了”

`GameXXKAcademyCatalog.cpp` 里 13 门课**每一门都带 NPC**（各自 `NpcId` 或 `Npc.TusiChief` 兜底），全部走同一条 `BuildLoadout`。所以：

- 不是“某位伙伴的课坏了”，是**13 门课全坏**，含主角课。
- `UGameXXKAcademySubsystem::BeginCourse` 第 40 行构建失败即 `return false`；
- 工作台 `GameXXKDesktopTrainingWorkbenchWidget.cpp:10640` 收到 false 后只把错误文本塞进 `SetNotice`。

玩家侧看到的就是“点了没反应 / 点不动”。**它与鼠标命中、层级遮挡、UI 重建都无关**，先前的排查方向（输入捕获、ZOrder）是错的。

### 2.4 时序归因

`2026-09-15` 引入渐进出战槽（存档 v43，`docs/production/2026-09-15-party-slot-onboarding.md`）后，NPC 槽从“默认开放”变成“200 金币天赋购买”。第 318 行这个前置校验随之加入，而学院的借用状态构造器仍按“NPC 槽可用”的旧前提写。**这是一次典型的回归，不是新需求。**

### 2.5 修复面（需要用户拍一个权限口径）

技术上很窄，两种方向：

- **A：学院借用态绕过槽位门槛。** 在 `BuildLoadout` 里显式解锁/旁路槽位校验，让课程不依赖玩家是否买了 NPC 槽。最小改动，且符合“课程是教学场景、不该要求玩家先付费”的直觉。
- **B：学院按玩家真实解锁状态开课。** 未解锁 NPC 槽时该课置灰并给出原因。改动更大，且和用户“主角与各伙伴课程都要能点”的期望冲突。

**建议 A**，但这属于规则决定，需要你确认（§8 已列为提问）。

---

## 3. 新证据二：一条测试崩溃，导致全量回归跑不完

```
Unhandled Exception: EXCEPTION_ACCESS_VIOLATION writing address 0x00000000000000d0
[Callstack] UnrealEditor-GameXXK.dll!FGameXXKHighestManaSiphonTest::RunTest()
           [Source/GameXXK/Private/Tests/GameXXKEnemyManaSiphonTest.cpp:60]
```

第 60 行：

```cpp
Find(State,Party[0])->Mana=3;
```

而 `Find` 不判空：

```cpp
FGameXXKCardCombatUnit* Find(FGameXXKRuntimeState& State,FName Id)
{ return State.CardRun.ActiveBattle.Units.FindByPredicate([Id](const auto& U){return U.UnitId==Id;}); }
```

`Party[0]` 是字符串 `"Hero"`，单位 ID 由 `BeginCardBattle` 生成，所以必然找不到。同一文件第 72/73/74 行复用同一返回值，是同一个坑。

**影响**：这不是“一条测试红”，是**整个 `UnrealEditor-Cmd` 进程被终止**。本轮全量只跑完 22 项（18 通过 / 4 失败）就崩了，`index.json` 不可用。

**后果**：`Saved/Automation` 里最后一份完整报告停在 **2026-09-12**。09-13 ～ 09-16 的全部改动（左右布局、箭头、灼烧、宝箱、奖励飞行、引导……）**没有任何全量回归基线**。在这个修掉之前，我做的任何改动都无法证明“没有新增失败”。

**这是所有后续工作的前置条件，优先级高于任何功能。**

---

## 4. 已经比待办表写的更完整的两项

### 4.1 A10 排序：不是“未做”，是“做了但不对版”

| 位置 | 现状 |
|---|---|
| 背包排序按钮 | **已存在**。`GameXXKDesktopTrainingWorkbenchWidget.cpp:7445-7454`，ActionId 61 |
| 背包排序实现 | **已存在**。`case 61`（`:11137`）排序 `BackpackSlots` |
| 仓库排序实现 | **已存在**。`SortWarehouseForTest()`（`:3405`）排序 `WarehouseSlots` |
| 仓库排序按钮 | **不存在**。全局搜 `WarehouseSortButton` 只有测试里出现过一次，`ActionId 5` 也没有对应按钮 |

两个实现都只按 `bEquipmentInstance → EntryId.LexicalLess` 排，**没有实现待办表第 137 行建议的“类别→品质→等级”**。而且 `SortWarehouseForTest` 第 3435 行的提示写的是：

```cpp
SetNotice(GameXXKLocalization::Source(TEXT("仓库已排序：槽位 → 品质 → 等级")));
```

——**文案宣称的排序键和代码实际排序键不一致**，且这条提示在 UI 上没有入口能触发。

所以 A10 的真实工作量是：定排序键 → 改两处比较器 → 给仓库补按钮 → 修提示文案 → 补验收（排序前后集合等价、页范围、拖拽/选择中被排序不串物品）。

### 4.2 技能点系统：接入点是**唯一**的，不是散落的

待办表说 A03 “未做”，这是对的；但它没说清接入成本。实测：

- 品质→效果的唯一换算入口是 `FGameXXKCardQualityRules::BuildEffectiveDefinition(BaseDefinition, CurrentQuality)`（`Public/GameXXKCardQualityRules.h:12`），返回一份**缩放副本，不改目录**。
- 全工程 **54 处**调用它，覆盖战斗结算（`GameXXKCardRules.cpp` 20 处）、卡面与 Tooltip 文案（`GameXXKCardText.cpp` 6 处）、Board 显示（`GameXXKBattleBoardWidget.cpp` 3 处）、存档迁移（`GameXXKSaveMigration.cpp:2058`）和大量测试。

**含义**：技能点等级不需要另建一条数值管线，只要把 level 作为**第二个维度**并进 `BuildEffectiveDefinition`，战斗、Tooltip、预览、存档会**同时**拿到正确值。这正是玩法案 §9.1 建议的“品质定义功能门槛、等级定义该功能下的成长，再生成一份实际有效卡定义供战斗和 Tooltip 共用”。

**所以 A03 的代码部分风险比看上去低得多；真正的阻塞是 A04 的逐卡目录表。**

---

## 5. 需要你定稿才能做（我做不了决定）

| 项 | 缺什么 | 谁能给 |
|---|---|---|
| A04 | 品质功能表：CardId × 品质门槛 × 1/2/3 级效果 × 升级成本 × 前置 × 与战后升品质的关系 | 你 |
| A03/A05/A06 | 每级发几点、升级花几点、是否可洗点、旧档补点公式、NPC 是否同等参与 | 你 |
| A14 | 音量范围、默认值、是否另有总开关、切换档案是否共享 | 你 |
| B03/B04 | 横幅停留时长、中英同屏还是切语言、音效、折叠态表现 | 你（**且缺音效资源**） |

这些不是“技术难”，是**没有权威数值我就只能编**。项目自己的规则（待办表 §8.1）也写明“不把临时代码常量当作用户批准值”。所以这几项我会**做到“数据层+事务+展示就绪、只等填表”为止**，不擅自填数。

---

## 6. 一个必须提醒的资源事实

待办表 A14 写的是“音效／BGM 独立音量设置”。实测：

- 音频层只有 `Source/GameXXK/Public/Audio/GameXXKSfx.h`（Cue 服务）和 `GameXXKSfxPolicy.h`（并发/静音策略）。
- 全工程搜 `BGM|BackgroundMusic|AmbientMusic|MusicCue|PlayMusic`：**0 命中**。
- 现有静音实现也偏弱：`GameXXKDesktopTrainingWorkbenchWidget.cpp:10341` 的 `ToggleMuted()` 只动全局 `FApp::SetVolumeMultiplier(0.0f)`，而 `bMuted` 是 **Widget 成员**（头文件第 1090 行），不是存档字段，重开即丢。

**含义**：A14 里的“音效音量”现在就可以做完整；“BGM 音量”没有 BGM 可调，做了就是个死滑块。**要么先确定有没有 BGM 资源计划，要么把 A14 拆成“音效音量（本轮可做）+ BGM 音量（待资源）”。** A13 音量开关引导依赖 A14，同理顺延。

---

## 7. 我做不了的（资源类，不是我努力就能出）

- **A01 逐卡文案简化**：173 张活动卡的中英双语文案重写，属内容创作，且需你逐批审核（项目规则本来就要求先给文本表再改 UI）。
- **B01/B02/B09**：全角色动画、卡面、idle、魅力立绘——资源未配齐。
- **B03/B04**：横幅需要美术字与音效。
- **B05/B06/B07**：女主 30 张差异牌设计、61 张插图、切主角剧情映射。其中 B06 是 61 张图的生成+审图，B05 需要你先定男女主机制对照。
- **B08/B10 图鉴**：伙伴/怪物两页签，依赖 B09 立绘与怪物意图图资源。
- **A11/A12/A13/A15**：引导的**逻辑**我可以写（引导框架 `FGameXXKGuideProgress` / `GuideCoordinator` / `GuideTargetRegistry` 都在），但它们各自都还有前置依赖（A11 依赖容量权威、A13 依赖 A14、A15 依赖桌面窗口置顶的真实行为）。建议排在 §8 的 A 批之后。
- **C01–C04**：联机/异步匹配/UGC/DLC，交接文档自己就写的是“大后期独立立项，先出规则与协议案”。

---

## 8. 我建议的批次（按“能不能自证”排，不按价值排）

### 批 A：恢复可用 + 恢复回归能力（建议立刻做，风险最低）

1. **修学院 NPC 槽根因**（§2）→ 13 门课恢复可开。验收：`GameXXK.Academy.*` 4 项转绿 + 真实 PIE 点开主角课与一门 NPC 课。
2. **修 `GameXXKEnemyManaSiphonTest` 崩溃**（§3）→ 全量回归可跑完。验收：`index.json` 出现完整 tests 数组。
3. **重跑全量基线** → 拿到 09-16 的通过/失败清单，作为后续一切改动的地板。
4. **A10 排序对版**（§4.1）→ 排序键、仓库按钮、提示文案三处对齐 + 集合等价验收。

这 4 项互不依赖、每项都能用冷 UBT + 定向自动化 + 真实 PIE 自证，且做完之后**我才具备做后面任何事的前提**。

### 批 B：设置类（可做完整，等你给参数）

5. **A14 音效音量**（BGM 部分视 §6 决定）→ 真实 SoundClass/SoundMix + 存档持久化 + 与静音协同。
6. **A13 音量开关引导** → 依赖 5。

### 批 C：技能点闭环（你给表，我做码）

7. 先做**数据层与事务**：技能点字段、v48 迁移与旧档补点策略、`BuildEffectiveDefinition` 增维、1→2/2→3 原子事务（失败不扣点）。
8. 再做**展示层**：1/2/3 级详情、当前等级标记、下一级变化强调。
9. 再做 **A07 红点 + A08 升级引导**（依赖前面权威点数状态）。
10. 最后 **A09 的课程内容扩充**（每个伙伴第一次实际用牌）——注意 A09 的“点不动”已由批 A 第 1 项解决，这里是内容延伸，不是同一个 bug。

### 批 D：其余引导

11. A11 满包→仓库放入、A12 商店引导、A15 置顶引导、D01 属性短链、D02 卡组编辑教学。

---

## 9. 风险与边界（我做事时会守的）

- **不碰手调资产**：角色 Sprite、PaperZD、已放置关卡、相机、HD2D 平面值。
- **不动已验收范围**：奖励飞行动效、教学箱、1-1/1-2 资格规则、挑战暂停游历、空槽编队。
- **不用 Live Coding / Hot Reload 当验证**；改 C++ 走 MCP 存盘 → 关编辑器 → 冷 UBT（实测增量约 47 秒，可迭代）→ 定向自动化 → 真实 PIE。
- **不删失败断言凑绿**。D04 的 3 条扩展 UI 静态断言目前不符，我会单独查原因，不掩盖。
- **不擅自填数**：A03/A04/A14 的未定参数一律留待定并标注，不写进代码当常量。
- **不动玩家存档**：破坏性夹具只跑隔离 `-UserDir`。本轮基线就是这么跑的。

## 10. 需要你回的三个问题

1. **学院课程权限**：按 §2.5 的 **A（课程借用态绕过 NPC 槽门槛，13 门课立刻可开）** 还是 **B（未解锁 NPC 槽的课置灰）**？
2. **BGM**：有没有 BGM 资源计划？没有的话 A14 是否只做音效音量、BGM 音量挂起？
3. **批次顺序**：是否同意“批 A（修课 + 修崩溃 + 基线 + 排序）先做”，还是你手上有更想优先的功能项（例如技能点）要我直接进？

---

## 附：本条分析新增的证据文件

- `docs/production/2026-09-16-regression-baseline-and-academy-blocker.md`（完整实测记录）
- `Saved/Baseline-20260916/run_checks.py`（可复现的隔离运行脚本）
- `Saved/Baseline-20260916/baseline-full/`（崩溃报告、Editor.log、index.html）
- `Saved/Baseline-20260916/completed-before-crash.tsv`（崩溃前 22 项逐条状态）
