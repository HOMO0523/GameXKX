# 2026-09-16 回归基线实测（只读，未改代码）

状态：新增证据记录。本轮没有修改任何 `Source/`、`Content/`、`Config/` 或运行时代码；只启动过一次隔离的无窗口 Automation 进程并保存报告。

## 目的

当前 `main` 工作区有 201 个未提交改动，而 `Saved/Automation` 里最后一份报告停在 **2026-09-12**（`FontRollout20260912`）。也就是说 09-13 ～ 09-16 的全部改动都没有任何全量回归基线。本轮补一次，用来区分“改动引入的失败”和“既有失败”。

## 命令

复用项目既有的一次性核对脚本模式（与 `Saved/EnemyIntentFix-20260916/run_checks.py` 同构），新增：

- 脚本：`Saved/Baseline-20260916/run_checks.py`
- 报告：`Saved/Baseline-20260916/baseline-full/`
- 隔离 UserDir：`Saved/Baseline-20260916/AutomationUser/`

```
UnrealEditor-Cmd.exe GameXXK.uproject /Game/GameXXK/Maps/L_DesktopTrainingHUD
  -Unattended -NoSound -NullRHI -NoSplash -NoPause -NoZenAutoLaunch -DDC-ForceMemoryCache
  -UserDir=<隔离目录> -ReportOutputPath=<报告目录> -abslog=<报告目录>/Editor.log
  -ExecCmds=Automation RunTests GameXXK; Quit
```

## 结果：跑不完

进程退出码 **3**，产生 minidump（`AutomationUser/Saved/Crashes/UECC-Windows-C577551C444E6FB056D70C92109AF151_0000/`）。

**在崩溃前只完成 22 项测试**，`index.json` 因此没有可信的 tests 数组，`parse_automation_index.py` 无法判定。全量基线**当前拿不到**。

已完成的 22 项：18 Success / 4 Fail（清单见 `completed-before-crash.tsv`）。

## 发现 1：运行时崩溃（阻断整条回归）

```
Unhandled Exception: EXCEPTION_ACCESS_VIOLATION writing address 0x00000000000000d0
[Callstack] UnrealEditor-GameXXK.dll!FGameXXKHighestManaSiphonTest::RunTest()
           [Source/GameXXK/Private/Tests/GameXXKEnemyManaSiphonTest.cpp:60]
```

对应代码：

```cpp
Find(State,Party[0])->Mana=3;   // GameXXKEnemyManaSiphonTest.cpp:60
```

`Find()` 是按 `UnitId` 线性查找并返回指针，没有判空：

```cpp
FGameXXKCardCombatUnit* Find(FGameXXKRuntimeState& State,FName Id)
{ return State.CardRun.ActiveBattle.Units.FindByPredicate([Id](const auto& U){return U.UnitId==Id;}); }
```

`Party[0]` 是字符串 `"Hero"`，而单位 ID 由 `BeginCardBattle` 生成，所以 `Find` 返回 `nullptr`，随后对 `->Mana` 赋值触发对地址 `0xd0` 的写越界。同一测试文件 `GameXXKEnemyManaSiphonTest.cpp:72`、`:73`、`:74` 也复用同一返回值。

**影响**：这不是“一条测试失败”，而是 `check()`/未捕获访问违例让整个 `UnrealEditor-Cmd` 进程终止。在它修掉之前，任何 `Automation RunTests GameXXK` 都跑不到后 80% 的测试。

## 发现 2：学院课程战斗全部构建失败（与用户“教程点击不了”同源）

4 项失败全部来自 `GameXXKAcademyRulesTest.cpp`：

```
Fail | GameXXK.Academy.CatalogAndLoadouts
Fail | GameXXK.Academy.GuidedObjectives
Fail | GameXXK.Academy.IndependentEncounter
Fail | GameXXK.Academy.PlayableObjectives
```

日志中的实际错误文本：

```
Academy.Basic: Selected NPC is not one of the six owned definitions.
Expected 'Academy.Basic lesson 0 builds: ...' to be true.
```

### 根因（已定位到具体分支）

`AcademyStateBuilder::BuildLoadout`（`Source/GameXXK/Private/MVP/GameXXKAcademyStateBuilder.cpp`）：

1. `State = UGameXXKMVPRules::CreateNewGame();` —— 全新档，**没有任何天赋购买记录**。
2. 直接往编队里塞 NPC：
   ```cpp
   const FName Npc = Course.NpcId.IsNone() ? FName(TEXT("Npc.TusiChief")) : Course.NpcId;
   ```
3. 调 `FGameXXKCardBattleAdapter::SetQuestNpcForCurrentRun(...)`（第 40 行）。

而 `SetQuestNpcForCurrentRun` 最终走到 `FGameXXKPartyFormationRules::SetQuestNpc`（判断在 `GameXXKPartyFormationRules.cpp:318`，报错在 `:321`）：

```cpp
if (!QuestNpcId.IsNone() && (!IsSlotUnlocked(InOutState, EGameXXKPartyMemberKind::QuestNpc)
    || !FGameXXKCompanionCatalog::FindQuestNpcDefinition(QuestNpcId)))
{
    SetError(OutError, TEXT("Selected NPC is not one of the six owned definitions."));
    return false;
}
```

`IsSlotUnlocked`（`GameXXKPartyFormationRules.cpp:217`）在 `bProgressivePartySlots` 为真时要求对应天赋已购买：

```cpp
return IsSlotEligible(State, Kind)
    && (Kind == EGameXXKPartyMemberKind::Hero || !State.Training.bProgressivePartySlots
        || State.Talents.NodeRanks.FindRef(SlotTalentId(Kind)) > 0);
```

新档 `bProgressivePartySlots == true` 且 NPC 槽天赋 rank 为 0 → 槽位未解锁 → 报错。

### 时序证据

`2026-09-15` 引入渐进出战槽（存档 v43，`2026-09-15-party-slot-onboarding.md`）后，NPC 槽变成需要 200 金币天赋购买。`GameXXKPartyFormationRules.cpp:318` 这个前置校验是随该批加入的。学院借用的全新档状态因此必然撞上它。

**这不是 UI 点击问题。** `Source/GameXXK/Private/Guide/GameXXKAcademySubsystem.cpp:40`：

```cpp
if(!UGameXXKMVPSubsystem::BuildAcademyBattleState(*C,Index,Borrowed,Focus,Error))
{ Message=GameXXKLocalization::Source(Error); return false; }
```

课程战斗构建失败 → `BeginCourse` 直接返回 false → 工作台 `HandleActionClicked`（`GameXXKDesktopTrainingWorkbenchWidget.cpp:10640`）只把错误文本丢进 `SetNotice`。玩家看到的就是“点不动 / 点了没反应”。

### 与卡死的区别

`GameXXKAcademyCatalog.cpp` 里 13 门课全部走同一条 `BuildLoadout`，且**每一门都带 NPC**（`Npc.TusiChief` 兜底或各自 `NpcId`）。所以这不是“某个伙伴的课坏了”，是**13 门课全坏**。

## 尚未覆盖

- 全量跑不完，因此 09-13 ～ 09-16 其余改动仍然没有回归基线。
- 崩溃是否只在 `GameXXKEnemyManaSiphonTest` 出现、还是同类夹具通病，未逐文件核对。
- 本轮未启动可见编辑器，未做 PIE，未复核玩家存档；使用的 `-UserDir` 是隔离目录。
- 学院失败在真实新档上的实机表现（提示文本是否可见、偏好弹窗是否遮挡）未实拍。

## 复现方式

```powershell
python -X utf8 "Saved\Baseline-20260916\run_checks.py" baseline-full --tests GameXXK
```
