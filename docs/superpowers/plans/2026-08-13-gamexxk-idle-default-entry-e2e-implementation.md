---
status: shelved
updated_at: 2026-08-17
shelved_reason: legacy migration package; do not execute
superseded_by: docs/superpowers/specs/2026-08-17-gamexxk-desktop-training-workbench-design.md
---
# GameXXK Idle Default Entry And E2E Certification Implementation Plan

> 执行冻结：本文以 `CurrentSaveVersion=17` 为历史前置基线；当前工作区已进入 `CurrentSaveVersion=18`，本文的 v15/v16/v17/v18 迁移边界不能直接执行。恢复历练实现必须以 `2026-08-17-gamexxk-desktop-training-workbench-design.md` 和新的 Phase 0 基线从 v19 重新编排迁移编号。

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在前六包全部 GREEN 后，把新建/继续游戏默认入口迁到 2D 历练主界面，保留 3D 城镇为明确的兼容/开发入口，并完成全流程、数值、存档、UI、迷你窗和自动战斗认证。

**Architecture:** 新 `EGameXXKScreen::IdleHome` 映射到 `L_Main` 纯 HUD，Start/Load只改变 screen与显式 settlement orchestration，不加载3D城镇；挑战未通关章节时仍进入现有 route map。3D地图、NPC、相机与手调资产保留，开发入口调用现有 `EnterWorldRegion`，最终认证用当前快照的全组和真实 PIE，不沿用旧报告名称。

**Tech Stack:** UE 5.8 MVP LevelFlow/PlayerController、IdleHome、Route/CardBattle、Save v18、Automation、UBT、MCP PIE harness、Python JSON分析。

---

## 0. 前置门禁与范围

只有以下条件都满足才执行本包：

- v17 migration、explicit NPC party全绿；
- Idle core/chest/home/mini/auto各自production status存在；
- `main==origin/main`、tracked/index clean；
- 当前没有未收尾目标预演/tooltip/美术任务重叠；
- 3D资产 hash/Transform baseline已保存。

**Modify:**

- `Public/GameXXKMVPRules.h`、private cpp — 新 screen与新局默认状态。
- `Public/MVP/GameXXKSaveMigration.h`、private cpp — `IdleHomeDefaultEntrySaveVersion=18` 与 v17→v18 screen 规范化。
- `Public/Private MVP/GameXXKLevelFlow` — IdleHome→L_Main。
- `Public/Private MVP/GameXXKMVPSubsystem` — Start/Continue settlement sequence。
- `Public/Private MVP/GameXXKMVPPlayerController` — IdleHome visibility/default focus。
- `Public/Private UI/GameXXKMainMenuWidget`、PlayableRoot/CommandRouter — Start/Continue UI语义。
- MVP/flow/level/UI tests。
- real play probe/harness/tests。
- `AGENTS.md`、agent guide、project plans/production ledger。

**Create:**

- `Private/Tests/GameXXKIdleDefaultEntryTest.cpp`
- `Private/Tests/GameXXKIdleMigrationEndToEndTest.cpp`
- `scripts/gamexxk_idle_balance_report.py`
- `scripts/test_gamexxk_idle_balance_report.py`
- `docs/production/2026-08-13-idle-migration-final-status.md`

不删除任何 `Content/GameXXK/Maps`、Town、PaperZD、sprite、camera、PCG、HD2D asset。

## Task 1: 新 screen 与 LevelFlow

- [ ] **Step 1: 写映射 RED**

`MapForScreen(IdleHome)==/Game/GameXXK/Maps/L_Main`；从L_Main显示IdleHome不触发load；IdleHome→DungeonMap进入L_RouteMap；开发Town仍映射现有 AsianVillage map。

- [ ] **Step 2: enum 兼容**

在 `EGameXXKScreen` 末尾新增 `IdleHome`，不要插入中间改变旧序列化数值。把 `CurrentSaveVersion` 提升到18，并让 v17→v18 只在明确 `MigrateDefaultEntryScreen` helper 中把非active route的 Town/WorldMap规范到 IdleHome；MainMenu存档仍为MainMenu，active Battle/Dungeon/Event/Merchant保持原screen。current-v18不修复非法screen，只拒绝。这样已经写出的合法v17存档不会在同版本内悄悄改变含义。

- [ ] **Step 3: visibility**

PlayerController IdleHome visible iff screen=IdleHome；MainMenu visible iff MainMenu；Route/Battle不叠 IdleHome。Mini可独立存在但首次路线时不覆盖选择UI。

- [ ] **Step 4: GREEN/commit**

`git commit -m "feat: add idle home screen routing"`。

## Task 2: 新建与继续默认入口

- [ ] **Step 1: StartNewGame RED**

新局仍创建两永久伙伴、8主角牌、6件starter设备、材料等既有内容；Screen改IdleHome；IdleState无解锁章、ActiveChapter=0；不加载Town，不接受任务、不制造奖励。

- [ ] **Step 2: Continue RED**

加载后先 migration，再用 TimeSource计算 settlement Candidate并保存；成功才提交并显示IdleHome/offline receipt。保存失败保持加载前RuntimeState/当前screen并显示明确错误，不能进UI展示假奖励。

- [ ] **Step 3: active route恢复**

若存档有 active battle/route/pending choice，Continue恢复对应 route/battle screen，不强制IdleHome、不做覆盖 active route临时状态的效率刷新；离线历练仍可按独立局外snapshot结算一次。

- [ ] **Step 4: MainMenu**

Start/New Game按钮仍可点；Continue有效slot可点；标签无需写Town。主菜单背景/现布局不在本包重设计。

- [ ] **Step 5: mutation/commit**

临时在Load失败也提交Candidate，原子测试RED；恢复 `feat: make idle home the default entry`。

## Task 3: 从2D进入首次通关路线和返回

- [ ] **Step 1: 未通关章挑战**

IdleHome chapter卡点“前往挑战”调用显式 StartChapterChallenge；玩家选route node/event/shop/reward。挑战按钮不直接解锁Idle。

- [ ] **Step 2: 胜败返回**

战败/放弃回IdleHome，章仍locked；章Boss胜利写unlock后回IdleHome并可StartIdle；最终章同样。原 route settlement和permanent reward只执行一次。

- [ ] **Step 3: 3D兼容入口**

设置/开发命令“进入旧城镇”调用 existing `EnterWorldRegion(Qingshan)`；退出Town回IdleHome。生产普通玩家是否显示该按钮由config `bExposeLegacyTownEntry` 默认false控制，开发/test可true。

- [ ] **Step 4: Town等价入口清单**

在切默认前证明2D已有：任务、NPC入队、商店、队伍、背包、装备、章节挑战、保存/设置；任何缺项阻止默认切换，不用隐藏Town来绕过。

- [ ] **Step 5: GREEN/commit**

`git commit -m "feat: route idle home into first clear progression"`。

## Task 4: 3D资产零改动门禁

- [ ] **Step 1: 记录受保护路径**

```powershell
git status --short -- Content/GameXXK/Maps Content/GameXXK/Characters Content/GameXXK/PaperZD
git diff --name-only $BaselineSha..HEAD -- Content/GameXXK/Maps Content/GameXXK/Characters Content/GameXXK/PaperZD
```

预期0 tracked变更。本轮不执行全资产hash扫描造成噪音；使用spec前manifest或只对已知Town map/角色asset做hash。

- [ ] **Step 2: 旧Town真实加载**

开发入口进入、NPC/相机/transform读取，离开后IdleState不重复settle。若资产已有用户dirty package，通过MCP保存/协调，不能覆盖。

- [ ] **Step 3: test**

LevelFlow仍识别 current/legacy Qingshan Town map；default-entry tests不能删除旧断言，而是将其移到“legacy dev entry”叶。

## Task 5: 数值评估与调优报告（只调配置）

- [ ] **Step 1: Python report RED**

读取Automation JSON/CSV，输出每章×队伍档：normal/elite kills/h、gold/xp/h、normal/boss chest/h、CD约束率、20/40/60/80容量填满分布、boss占比、7天回流值、online-vs-offline偏差。

- [ ] **Step 2: 三队伍档**

NakedBaseline、MidBuild、OptimizedBuild均由真实局外装备/队伍snapshot导出；不读取临时route HP/DOT/hand。每章至少100,000 online kills及离线长算。

- [ ] **Step 3: 调优纪律**

只改 `GameXXKIdleCatalog.cpp` 配置；冷却固定4/6分钟、容量20..80和7天上限不改。目标不是固定10箱/h或精确8h填满，而是无零收益/爆表、档位差异可感知、20→80确有价值、Boss箱稀有且非不可见。

- [ ] **Step 4: 每次变更三次seed观测**

保存原配置、候选配置和对比；若修改后损伤章节/队伍体验，回滚该配置patch，不改solver特例。

- [ ] **Step 5: 提交**

数值若确需改：`balance: tune initial idle reward profiles`；若默认配置已合格，只提交报告，不为制造diff改数值。

## Task 6: 全自动化矩阵

- [ ] **Step 1: 冷 UBT**

禁止 Hot Reload/Live Coding，退出0、Result Succeeded。

- [ ] **Step 2: 新功能组**

```text
GameXXK.Idle
GameXXK.AutoBattle
GameXXK.UI.IdleHome
GameXXK.MVP.Idle
```

- [ ] **Step 3: 邻接全组**

```text
GameXXK.MVP
GameXXK.Route
GameXXK.RouteBalance
GameXXK.Equipment
GameXXK.Data.CardBattleRuntime
GameXXK.Data.CardOutcomePreview
GameXXK.Integration.CardBattle
GameXXK.UI.Battle
GameXXK.Data.AllCards.Playability
```

不得沿用旧 report；每组当前SHA fresh run。warning按测试leaf分类，目录名不算green。

- [ ] **Step 4: 存档矩阵**

v0、v8、v9、v11、v13、v14、v15、v16、v17、v18；关注reward防重、IdleState首次基线、QuestNpc explicit preserve、accepted no follower、v17 screen规范化与current-v18 invalid拒绝。

- [ ] **Step 5: 7天/分段/时钟/容量**

一次7天 vs 10080分钟；clock rollback；19/20/79/80；满仓释放；反复UI/mini打开；process切换边界。

## Task 7: 真实 PIE 最终旅程

- [ ] **Step 1: 新局主旅程**

MainMenu→Start→IdleHome locked→挑战章1→玩家选route→battle开启auto/2×→玩家选reward/route→Boss→Idle unlock→online推进→退出→fake/offline reload→summary→claim→inventory chest→open。

- [ ] **Step 2: NPC旅程**

从2D任务入口或legacyTown：Accept no follow/no party→点入队→save/load→battle第三人→移出仍Accepted。

- [ ] **Step 3: Mini旅程**

打开640×180→topmost off→drag→drawer up→claim/switch→return full；outside safe click不被拦截。

- [ ] **Step 4: 失败旅程**

save write false、inventory/warehouse full、clock rollback、invalid chapter/profile、battle defeat、mini invalid monitor；所有失败不部分提交、不自动retry、不丢箱。

- [ ] **Step 5: cleanup**

每次 harness在finally停止PIE并等待停止、clear fixture、恢复slot/sidecar；任何cleanup false顶层失败。不要关闭有dirty package的Editor。

## Task 8: 更新真源与交付

- [ ] **Step 1: 文档同步**

更新 `AGENTS.md` MVP acceptance、agent guide、project master/all-in-one、save compatibility、production ledger。保留3D资产但标“compat/dev entry”；删除旧default Town/follower硬验收。

- [ ] **Step 2: stale扫描**

```powershell
rg -n "Start.*Town|默认.*城镇|accepted.*follower|接受.*跟随|治疗粉|固定每小时 10|第三种宝箱" AGENTS.md docs/production docs/design Content/Python scripts
```

每个命中必须是明确历史/非目标引用，否则修正。

- [ ] **Step 3: final status**

`docs/production/2026-08-13-idle-migration-final-status.md` 写当前SHA、全部reports、数值、PIE、asset zero-diff、known warnings与未实现边界（十档装备/珠子/Steam/服务器/完整天赋）。

- [ ] **Step 4: 独立复审**

规格、质量、保存安全、UI/UX、测试假绿五类审查全部无Critical/Important；Minor记录或补测试。

- [ ] **Step 5: scoped commit/push**

```powershell
git diff --cached --check
git commit -m "test: certify idle desktop migration end to end"
git push origin main
git fetch origin main
```

最终成功标准是单机离线完整可玩且后续可替换 TimeSource/Repository；不是接入Steam、市场或服务器。
