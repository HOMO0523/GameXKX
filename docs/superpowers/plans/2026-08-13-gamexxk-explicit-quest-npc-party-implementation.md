# GameXXK Explicit Quest NPC Party Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把“接受任务”“NPC世界位置”“战斗队伍选择”彻底分离：接任务后 NPC 原地不跟随、不自动入队，只有另点“入队”才写 `PartySelection.QuestNpc`，并让 v15/v16 旧存档安全迁移到 v17。

**Architecture:** `QuestState` 继续表示剧情；`PartySelection.QuestNpc` 继续作为唯一战斗 NPC 选择；旧 `bFollowerJoined`/location 只作为读取兼容字段并在 v17 规范化为 false/非驱动。规则与 Subsystem 提供显式 Join/Remove 事务，Town Actor/Character 只发命令，GameMode 不再恢复 follower movement。

**Tech Stack:** UE 5.8 C++、SaveMigration v17、MVPSubsystem、CompanionPartySelection、Town NPC/QuestDialog、UMG、Automation、真实 PIE/MCP harness。

---

## 0. 当前冲突与文件范围

当前 v15 生产行为：

- `MigrateQuestFollowerContract` 把旧 Accepted/false 攡成 follower=true；
- `ValidateRuntimeState` 拒绝 Accepted&&!follower；
- `AcceptTownQuest` 设置 follower/location；
- Town Actor/Character 接受成功后 `ActivateFollower`；
- GameMode 读 follower/location恢复跟随；
- QuestDialog 已有“入队”按钮，Controller 调 `SelectTownQuestNpcForParty`。

本包必须一次性迁移这些互相依赖的断言，不能只把测试改成 false。

**Modify:**

- `Public/MVP/GameXXKSaveMigration.h`、private cpp — v17。
- `Public/GameXXKMVPRules.h`、private cpp — 接受任务新语义。
- `Public/MVP/GameXXKMVPSubsystem.h`、private cpp — Join/Remove 原子命令。
- `Private/MVP/GameXXKMVPGameMode.cpp` — 禁止 follower恢复。
- `Public/Private TownNpcActor`、`TownNpcCharacter` — 不 Activate/Tick follow。
- `Public/Private UI/GameXXKQuestDialogWidget` — 按状态启用“入队/移出”。
- `Private/MVP/GameXXKMVPPlayerController.cpp` — explicit commands。
- tests：SaveGame、TownShell、TownNpcInteractionRules、QingshanTaskNpcRoute、MVPFlow、PlayerFlow、CardBattleAdapter、QuestNpc selection。
- `Content/Python/gamexxk_probe_real_play_flow.py`、real play harness及 unittest。
- `AGENTS.md`、`docs/design/agent-operating-guide.md`、对应 production/master docs。

版本常量：

```cpp
static constexpr int32 IdleStateIntroducedSaveVersion = 16;
static constexpr int32 ExplicitQuestNpcPartyIntroducedSaveVersion = 17;
static constexpr int32 CurrentSaveVersion = 17;
```

不新增第二个 NPC party字段。`FGameXXKCompanionPartySelection::QuestNpc` 已是显式真源。

## Task 1: 用精确 RED 结束旧 follower 合同

- [ ] **Step 1: 写 v16→v17 migration RED**

至少四例：

1. Accepted + follower=true + QuestNpc empty → Accepted、follower=false、QuestNpc empty；
2. Accepted + follower=true + explicit QuestNpc=Tusi → Accepted、follower=false、Tusi保留；
3. Accepted + follower=false + explicit QuestNpc=YueBai → 保留YueBai；
4. Completed + stale follower/location → follower=false，party按现有完成清理规则处理。

location 可保留作旧世界位置兼容，但不能触发 movement；测试明确锁这个边界。

- [ ] **Step 2: current-v17验证 RED**

Accepted&&!follower 必须合法；Accepted+follower=true 必须非法或被 current-load明确拒绝（current version不静默修复）；QuestNpc若非空必须是 catalog有效 NPC 且恰3张固定卡、route/unlocked条件合法。

- [ ] **Step 3: 冷 UBT/Automation RED**

预期只因 Current=16、旧 follower normalize/validate失败。

- [ ] **Step 4: 实现迁移**

旧来源 `<17`：始终 `bFollowerJoined=false`；不从 follower推导 QuestNpc；显式 QuestNpc保留；位置字段原样保留但注释为 legacy placement。把旧 `<15` 先前 follower迁移的最终结果在 v17再退役，确保任意版本链一致。

- [ ] **Step 5: 迁移组 GREEN/commit**

```powershell
git commit -m "feat: migrate quest npc party semantics to version seventeen"
```

## Task 2: 接受任务只改剧情

- [ ] **Step 1: 写 Rules RED**

CreateNewGame→Town→Accept：Quest=Accepted、route seal=1、`bFollowerJoined=false`、PartySelection.QuestNpc empty、NPC location不被 Accept规则主动改、输入其他 party selection不丢。

- [ ] **Step 2: 实现 AcceptTownQuest**

删除 follower=true/location reset副作用；仍保持 Add route seal原子性。重复 accept失败且状态不变。

- [ ] **Step 3: Town Actor/Character RED**

Confirm Quest后 `IsFollowerActive=false`、actor location不变、follow target=null；Tick多次不移动；world location无需由接受动作记录。

- [ ] **Step 4: 删除运行 follower 行为**

接受分支不调用 `ActivateFollower`/`RecordQuestNpcLocation`。`ActivateFollower`/`DismissFollower` 可以保留 deprecated Blueprint兼容入口，但实现必须 no-op/false，或只供非任务旧角色且不能由QuestState驱动；选择一个并在测试锁定。推荐任务 NPC hard no-op，减少两套语义。

- [ ] **Step 5: GameMode**

删除 `bQuestFollowerShouldRestore` 分支；可按 saved location放置旧NPC，但随后总是 `DismissFollower`，并以实际 actor位置回填只供兼容显示时也不得自动保存推进。

- [ ] **Step 6: GREEN/commit**

`git commit -m "feat: stop quest acceptance from following or joining"`。

## Task 3: 显式入队/移出权威事务

- [ ] **Step 1: 写 Subsystem RED**

`JoinAcceptedQuestNpcParty(NpcId)` 前置：Quest=Accepted、Town/IdleHome可配置态、NpcId catalog有效、无 active route/battle。成功写 `SetQuestNpcForCurrentRun`；换NPC替换；重复相同幂等成功或明确 no-change；移出只清 QuestNpc，不改 QuestState/route seal/location。

- [ ] **Step 2: 原子保存**

使用包1 `CommitRuntimeCandidate`。Save false时 join/remove全部回滚；因为这是局外持久队伍配置，不能只内存变更。

- [ ] **Step 3: API**

```cpp
UFUNCTION(BlueprintCallable) bool JoinAcceptedQuestNpcParty(FName QuestNpcId);
UFUNCTION(BlueprintCallable) bool RemoveQuestNpcFromParty();
UFUNCTION(BlueprintPure) bool IsQuestNpcInParty(FName QuestNpcId) const;
```

原 `SelectTownQuestNpcForParty` 标 Deprecated 并仅转发 Join，避免旧 Blueprint立即断链；新代码/测试用新名。

- [ ] **Step 4: Battle投影**

BeginCardBattle 只读 PartySelection.QuestNpc；Accepted但空 party时必须得到 Hero+永久伙伴，不投影任务NPC。显式入队才投影第三人和三张NPC卡；移出后下一路线不投影。

- [ ] **Step 5: GREEN/commit**

`git commit -m "feat: add explicit quest npc party transactions"`。

## Task 4: QuestDialog 与队伍 UI

- [ ] **Step 1: 状态文案 RED**

未接受：主动作“剧情”，入队不可用；已接受未入队：“入队”；已入队：“移出队伍”；route/battle locked：按钮禁用并短提示。不要显示“跟随”。

- [ ] **Step 2: Controller handler**

`RecruitPendingTownNpc` 改名/新增 `TogglePendingTownNpcParty`，按当前 selection发 Join/Remove；成功关闭dialog并刷新，失败保留dialog/error。

- [ ] **Step 3: CompanionRoster**

队伍页显示临时任务NPC独立槽位；移出按钮不遣散/改任务；永久伙伴槽不变。禁止把任务NPC写入 permanent roster。

- [ ] **Step 4: 现布局快照**

QuestDialog Frame/按钮 geometry和Companion backpack布局不移动；只改label/enable state/新增已存在空位内的临时槽展示。

- [ ] **Step 5: GREEN/commit**

`git commit -m "feat: expose explicit quest npc party controls"`。

## Task 5: 更新所有旧 follower 测试与文档

- [ ] **Step 1: 全仓定向扫描**

```powershell
rg -n "follower|Follower|跟随|接受即|bFollowerJoined|QuestNpcLocation" AGENTS.md docs Source/GameXXK/Private/Tests Content/Python scripts
```

逐项分类：生产迁移兼容字段可保留；验收/现状描述必须改成显式入队；历史报告标注过期而不篡改证据。

- [ ] **Step 2: 根验收**

`AGENTS.md` 改为：F 接受任务；NPC仍原地；另点“入队”才加入战斗队伍；手存/加载保留 Accepted和显式 party selection。删除“follower active and NPC location”硬验收。

- [ ] **Step 3: 测试不能只改 false**

每个旧 leaf都走完整 flow：accept断言 no party→click join→save/load→battle projection→remove→quest still accepted。保留 location roundtrip仅作为兼容数据 test，不叫 follower恢复。

- [ ] **Step 4: 文档同步**

更新 agent guide、project master/all-in-one、save compatibility、production ledger；扫描“固定不跟随待裁决”等已过期表述为0。

- [ ] **Step 5: commit**

`git commit -m "docs: align acceptance with explicit quest npc entry"`。

## Task 6: 真实 PIE 端到端

- [ ] **Step 1: harness**

真实流程：Town F→任务offer→Accept→观察NPC位置3秒不变→重新交互→点入队→打开队伍确认→手存→加载→确认Accepted+party保留、NPC仍不跟随→进入battle确认第三人→返Town移出→任务仍Accepted→再存读。

- [ ] **Step 2: probe字段**

QuestState、legacy follower、actor location/follow target、PartySelection QuestNpc、party人数/UnitIds、NPC三张卡、save version/slot。禁止用 legacy follower当“入队成功”判断。

- [ ] **Step 3: migration真实slot**

复制 v15 fixture到专用user index，加载生成backup→v17→不自动party；显式QuestNpc fixture保留。测试必须备份/恢复用户槽，异常/强杀有 sidecar恢复。

- [ ] **Step 4: 广组**

```text
GameXXK.MVP
GameXXK.MVP.SaveGame
GameXXK.MVP.Town
GameXXK.Integration.CardBattleAdapter
GameXXK.Route
```

- [ ] **Step 5: 状态文档与push**

创建 `docs/production/2026-08-13-explicit-quest-npc-party-status.md`，记录v15/v16/v17矩阵。确认无任何生产路径调用任务NPC follower movement 后 push。
