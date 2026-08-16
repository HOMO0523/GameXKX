---
status: proposal
owner: codex
updated_at: 2026-08-16
source_commit: 3ae0561a28fa18208d957650451aea1a34878d19
---
# GameXXK 全量代码与文档审查及优化方案（2026-08-16）

> 任务：先阅读全量项目代码和文档，再提出优化方案。本文档是全量审查后的方案真源，取代 `2026-08-16-optimization-followup.md` 的“下一步待办”部分，与其发现兼容。
> 范围：Source C++、docs 全部、scripts 与 Content/Python 全部、配置与构建文件。本轮只读审查；除本方案文档外不改生产代码。

---

## 1. 审查范围与证据

| 范围 | 数量 | 行数 | 审查方式 |
|---|---:|---:|---|
| Source C++（GameXXK/GameXXKEditor/TestMap，含 .h/.cpp/.cs） | 385 | 170,193 | 6 个并行只读审查 + 全量 grep 交叉扫描 |
| 其中 GameXXK Tests | 176 cpp + 3 helper | 74,616 | 全量统计 + 分域精读 |
| docs 全部（production/design/verification/superpowers/ui/defense） | 251 | 73,628 | 活跃文档逐篇全文；superpowers 全量结构 + 08 月全文 |
| scripts/*.py（含子目录与 _archive） | 198 | 59,873 | AST import/函数清单 + 核心精读 + 84 个测试实测 |
| Content/Python/*.py（含 _archive） | 222 | 40,196 | 同上 |
| 配置/Build/uproject/插件 | 12 | — | 全读 |

实测基线：`HEAD=3ae0561`，全量 Automation `598/598`、冷 UBT GREEN、默认生产循环 PASS、`--script-tests all` **84 个脚本测试中 22 个失败**。

---

## 2. 总体结论

项目功能成熟度较高（198 卡、v17 存档、600 个 C++ 自动化测试），但**结构性债务已开始反向拖累每个新功能**：

1. 运行时表现层存在**每帧全量刷新 + 同步纹理加载**的热路径浪费；
2. 战斗存在**新旧两套系统并存**，状态存在**运行/存档双写**；
3. 5 个巨型文件（CardRules 16,258 行、BattleBoard 8,956 行、CardTypes 2,407 行等）持续膨胀；
4. 数据全部硬编码在 C++，测试 seam 深度侵入生产类（75 个生产文件、约 1,155 处 `*ForTest`）；
5. 文档三轨漂移（AGENTS/设计 v15 follower 语义 vs 代码已切换）与计划状态不可机读；
6. Python 工具链 10 万行、命名漂移、个人路径硬编码、脚本测试门禁实际不可用；
7. 5.4 GB 源美术未跟踪且无资产策略。

---

## 3. 关键发现清单（全量审查合并结果）

### A. C++ 架构与正确性

| ID | 位置 | 严重度 | 发现 | 建议 |
|---|---|---|---|---|
| A1 | `MVPRules.cpp:1423,2342` | 高 | 旧 MVP 战斗（RunEnemyAI、ExecuteBattleBasicAttack/CraneWing/Guiyuan/Defend/HealingPowder、旧 BuildTurnOrder）与新 CardRules/CardBattleAdapter 并存，公式重复 | 确认无 UI/蓝图引用后下线旧入口，统一战斗规则 |
| A2 | `MVPRules.h:353,505` | 高 | `FGameXXKSaveState` 顶层重复保存 PlayerLocation/QuestState/follower/Gold/UnlockedRegions；`MakeSaveState` 双写，易漂移 | SaveState 只保留 SaveVersion+RuntimeState+派生缓存；补 v1→v17 全版本迁移矩阵 |
| A3 | `CardRules.cpp` 16,258 行；`BattleBoardWidget.cpp` 8,956 行 | 高 | 单文件 god object；ResolveCardPlay 约 660 行，BattleBoard 同时管布局/演出/手牌/奖励/HUD/NativePaint | 按阶段拆 CardRules；BattleBoard 拆 BattleLayout/Hand/IntentRail/Reward/UnitHud 子部件 |
| A4 | `CardCatalog.cpp:1834` 等 8 个 catalog | 高 | 198 卡/21 敌/30 遗物/49 装备/35 词缀全部编译期静态数组；坏数据直接 `UE_LOG(Fatal)` 崩溃 | DataAsset/DataTable 化；校验降级为可恢复错误+回退，保留确定性单测 |
| A5 | `MVPRules.cpp:1315`、`CardBattleAdapter.cpp:2172` | 高 | `NodeId * 486187739` 使用有符号 int32 乘法溢出，属未定义行为 | 改 uint32/int64 混合后截断，抽统一 seed 工具并加测试 |
| A6 | `MVPHUD.cpp:396` → `BattleBoard.cpp:3383`、`RouteMap.cpp:254` | 高 | DrawHUD 每帧无条件 `RefreshAuxiliaryWidgets`，触发 BattleBoard/RouteMap 全量重建与规则预览重算 | 状态脏标记/事件驱动；只刷新可见且变更的部件 |
| A7 | `BattleBoard.cpp:6511,3756,7435` | 高 | 每帧/每次刷新对手牌逐张 `BuildCardPlayPreview`（完整规则预演） | 缓存 preview，费用/目标/牌变化才重算 |
| A8 | `BattleBoard.cpp:7870-7902,8156`、`InventoryWindow.cpp:188`、`CompanionRoster.cpp:206` | 中 | 48 处同步 `LoadObject`；背包/卡面/路线图节点每刷新建 Brush 或 LoadSynchronous | 推广 AtlasCache 式软引用预载 + Brush 缓存 |
| A9 | `OneGameIslandRouteMapBridge.cpp:61-87,330-370` | 中 | 用 Timer 轮询、TObjectIterator 全扫、按“第一个 bool/int 属性”反射读取旧 1Game UI | 旧 UI 提供显式接口或事件，桥接去反射 |
| A10 | `MVPHUD.cpp:451`、`OneGameRouteMapWidget` | 中 | 旧 Canvas 路线图绘制与新 Widget 并存，双轨切换 | 删除旧 DrawRouteMap，统一 Widget |
| A11 | `RelicRules.cpp:232`、`CardRules.cpp` 33 处 FindByPredicate | 中 | 热路径线性查找遗物/单位/状态 | catalog 与战斗单位改 TMap 索引 |
| A12 | `MVPRules.cpp:2094-2163`、`RelicRules.cpp:324-337` | 中 | 奖励结算忽略关键返回值，遗物触发错误被吞 | 失败即回滚；错误向外传播 |
| A13 | `CompanionTypes.h:81`、`MVPRules.h:821`、EquipmentEconomyRules | 中 | Legacy 180 处、compat 47 处、多个兼容买卖/强化/分解入口 | 按 SaveVersion 闸门分批删除 |
| A14 | `SaveMigration.cpp:1220` | 低 | 路线种子归一化公式在迁移层重复实现 | 迁移只依赖规则层函数 |
| A15 | `GameXXK.Build.cs:10` | 中 | `bUseUnity=false`，385 个 TU 冷编译慢；UBT 报告还出现 UBA 低内存杀进程抖动 | 基准测试后启用 adaptive unity；冷编译前关编辑器，必要时 `-MaxParallelActions=8` |

### B. 测试体系

| ID | 位置 | 严重度 | 发现 | 建议 |
|---|---|---|---|---|
| B1 | 全部 600 个测试 | 中 | 全部 `IMPLEMENT_SIMPLE_AUTOMATION_TEST`，单旗标，无 Latent/超时/Disabled | 分 Smoke/Critical/Stress/Disabled + 超时守护 |
| B2 | 75 个生产文件约 1,155 处 `*ForTest` | 高 | 测试 seam 进入生产 API 与渲染热路径（BattleBoard 287 处、MVPPlayerController 97 处） | 迁到独立 TestAPI 或至少 `#if WITH_DEV_AUTOMATION_TESTS` |
| B3 | Tests 目录 | 高 | `MakeUnit` 39 文件、`MakeCard` 35、`BuildRuntime` 30 份复制粘贴 | 建公共 fixture/序列化库 |
| B4 | UI 测试 | 高 | 无 NullRHI/Slate 真实渲染/像素级验收；关键 Widget 直接测试稀疏 | 补 NullRHI/PIE 截图验收（表现类委托 lunamax） |
| B5 | `CardBattleBoardWidgetTest.cpp:2302,2359` 等 | 中 | 大量脆弱硬编码坐标/中文文案逐字断言 | 改为几何公式/容差/ID 断言 |
| B6 | 迁移/路线测试 | 中 | 无 v1→v17 全矩阵，无三章连乘整局路线通关率模型 | 补迁移矩阵与路线 Monte Carlo |
| B7 | Python 测试 | 高 | `--script-tests all` 22/84 失败：外部素材缺失、旧测试未随生产变更、GBK 编解码、MCP 测试混入 headless | 建 headless/asset-contract/mcp-live 三级标签，修复或降级 |

### C. 文档与流程

| ID | 位置 | 严重度 | 发现 | 建议 |
|---|---|---|---|---|
| C1 | `AGENTS.md:36-37`、`master-plan.md:35`、`all-in-one.md:47,1705,1866` | 高 | 文档仍写“接任务即 follower=true、Town 按钮进路线图”；代码已切为“接任务不跟随、显式入队、北门 F 进路线图” | 统一 follower 语义；给旧验收加 superseded 注记 |
| C2 | `current-goal-acceptance.md:19,26`；`balance-ledger.md §6`；`roadmap.md:24` | 中 | 提交区间写成 `8dc5ee6..b6763a0`（实为 `a490235..b6763a0`）；596/598 并存未标时间；§6/路线图 A2 仍写月白 33.3%、周 16.7%（最新为 36.7%、30%） | 修正数字并给每条证据带日期/报告路径 |
| C3 | `superpowers/` 186 篇 | 中 | 171 篇无 front-matter 状态；历练 7 包仍按 v16/v17/v18 规划，而 `CurrentSaveVersion=17` 已被首领槽占用；多代 UI/卡池方案互相覆盖未标记 | 补 `status: implemented/superseded/shelved/historical`；历练索引整组标 shelved；归档清单见下 |
| C4 | `8dc5ee6..b6763a0` | 中 | 8 个 UI/文本热修提交无 spec/plan/生产记录 | 补一份收尾记录；恢复 spec→TDD→record 纪律 |
| C5 | `all-in-one.md` 与分拆专题 | 中 | A 版全集与 B 版专题重复，生成后未随 08-13~16 刷新 | 重建或冻结版本头 |
| C6 | 建议归档 | 低 | 见 §7，约 40-50 篇已被取代 | 移入 `docs/superpowers/_archive/`，保留 git 历史 |

### D. Python 工具链与资产

| ID | 位置 | 严重度 | 发现 | 建议 |
|---|---|---|---|---|
| D1 | scripts 198 文件 59,873 行；Content/Python 222 文件 40,196 行 | 高 | 36 个根 `_*.py` 一次性脚本无引用；前缀 assemble/ensure/author/create/build/integrate/migrate/clone/execute/import 并存；README 计数过期 | 删/归档一次性脚本，收敛命名，更新两个 README |
| D2 | `run_asian_village_migration.py:34-37`、`import_town_ui_assets.py:26`、`import_psd_card_frame.py:25` 等 | 高 | 硬编码 `C:\Users\shxuw\...`、`D:\UE5 demo\zzz\...`、UE5.4/5.8 个人路径；换机/缺源即失败 | 改为 env/参数/仓库内源，缺失源资产显式 SKIP |
| D3 | `ai_production_loop.py:290` | 中 | `--json` 在 GBK 控制台打印 summary 触发 `UnicodeEncodeError` | 输出统一 UTF-8 或 errors=replace |
| D4 | `test_hp_hud_updates.py` | 高 | headless 全量测试会拉起 UE 编辑器并连 MCP | 标记 mcp-live；`all` 模式绝不启动编辑器 |
| D5 | 9 check / 10 apply | 中 | 3 个 apply 无配对 check，2 个 check 无 apply | 补齐 check/apply 对 |
| D6 | 所有 `gamexxk_*_apply` | 中 | 多数无 dry-run/预检，可直接写 Content/删资产 | 统一 plan/execute 分离与 dry-run |
| D7 | `.gitignore` + SourceAssets/SourceArt | 高 | 5.4 GB（10,257 文件）源美术未跟踪且无策略；`.git` 已完成一次 gc（6.5→2.2 GB） | 用户拍板 LFS/独立资产库；先落地 hash manifest，禁止 `git add .` |
| D8 | `Config/DefaultEngine.ini:38-39` | 低 | `DefaultGraphicsRHI` 重复两行；打包 AlwaysCook 同时有整目录与分目录重复条目 | 去重并审计打包配置 |

---

## 4. 分阶段优化方案

### Phase 0 —— 真源修正与工具链收尾（0.5–1 天，零 C++ 风险，立即执行）

1. 修正 C1/C2：AGENTS、master-plan、all-in-one 的 follower 语义；滚动指针/台账/路线图数字；optimization-plan 的 archive 计数。
2. Superpowers 治理：全部文档补 front-matter 状态；历练 7 包索引标注 `shelved` 并写清版本边界（v17→v18 / v18→v19 / v19→v20）；执行 §7 归档清单。
3. Python 收尾：修 D3；测试打标签并修 `all` 不启动编辑器；36 个根 `_*.py` 归档；修 `gamexxk_real_play_flow_mcp.py` 的 `pointer_matches_target` 旧吸附断言；更新 README。
4. 配置：删 `DefaultGraphicsRHI` 重复行；打包 AlwaysCook 去重（保留 `/Game/GameXXK` 一条）。
5. 验证：`git diff --check` + `harness_state_validator` + 默认生产循环 + headless 脚本测试全绿。

### Phase 1 —— 正确性与性能快赢（2–4 天，C++ 逐项 TDD 独立提交）

1. **P1-1 修 A5 seed 溢出**（最高优先 bug）：uint32/int64 混合 + 固定种子回归。
2. **P1-2 修 A6/A7 每帧全量刷新**：DrawHUD 脏标记化；BattleBoard/RouteMap 事件驱动；preview 缓存。验证：帧刷新计数测试 + 真机 PIE。
3. **P1-3 修 A8 同步加载**：统一软引用+Brush 缓存，复用 AtlasCache；背包/路线图/卡面预载。验证：加载计数器测试 + PIE 无卡顿。
4. **P1-4 修 A12/A11**：错误传播与 TMap 索引；失败回滚测试。
5. **P1-5 抽公共工具**：确定性 RNG/seed、错误助手、UI 共享 slot/tooltip/brush 库，删除 7 份 MakeTextureBrush 复制。
6. 每项验证：冷 UBT + 聚焦 Automation + mutation RED + 真机 PIE；每项单独提交。

### Phase 2 —— 结构重构（高价值高风险，逐项立项评审后执行，每个 2–5 天）

1. **P2-1 合并战斗系统（A1）**：确认旧入口零引用 → 下线 ExecuteBattle* 与旧 RunEnemyAI/BuildTurnOrder → 删除重复公式。
2. **P2-2 存档收敛（A2）**：SaveState 去重 + SaveGame 标记统一 + v1→v17 全矩阵。
3. **P2-3 巨型文件拆分（A3）**：CardRules 按阶段；BattleBoard 拆子部件；CardTypes 按域拆分。
4. **P2-4 数据 DataAsset 化（A4）**：先 Enemy/Relic/Equipment 试点，再 CardCatalog；保留确定性校验与回退。
5. **P2-5 ForTest 收敛（B2）**：独立 TestAPI 模块或宏隔离，分 3 批迁移，全量 Automation 每批护航。
6. **P2-6 命令路由/桥接去反射（A9/A10）**：删除旧 HUD Canvas 路线图，桥接改显式契约。
7. **P2-7 编译性能（A15）**：adaptive unity 基准测试；若全量编译时间下降 ≥30% 且无编译错误，正式切换。

### Phase 3 —— 测试体系升级（1–2 周，可与 Phase 1 穿插）

1. 测试旗标 + 超时/Disabled（B1）；观测类 2400/2520 场移入 Stress 桶。
2. 公共 fixture 库（B3）；合并 39+35+30 份复制。
3. 补覆盖缺口：CommandRouter、MVPWidgetBase、DungeonMap/WorldMap/Trade/CharacterPanel/QuestDialog/TownHud、InteractionComponent。
4. 补迁移矩阵与整局路线通关率模型（B6，与台账 §6.3 合并）。
5. 关键 Widget NullRHI/PIE 像素验收（表现类委托 lunamax）。
6. Python 门禁：headless 全绿；asset-contract/mcp-live 按环境选择性跑；默认门禁从 3 个扩到 headless 子集。

### Phase 4 —— 资产与仓库治理（与玩法并行，需用户决策）

1. **源美术策略**（D7）：三选一：Git LFS 入库 / 独立资产库 + hash manifest / 网盘 + manifest；在决策前为 SourceAssets/SourceArt 生成只读 manifest 并冻结改动。
2. **Git 历史**：评估 `git lfs migrate` 或 BFG 清理历史大 blob；当前 2.2 GB 中仍可能有可迁移二进制；执行前完整备份。
3. `r.Streaming.PoolSize=768` 待用户城镇/PIE 视觉验收后写入。
4. Python 资产流水线补 dry-run/check 对、去个人路径、补源图资产。

### Phase 5 —— 玩法主线（沿用既有裁决，不因本方案插队）

1. 地形增益重设计：先复核 §5 山河三档 + `TriggerTerrainBenefit` 口径，再 TDD。
2. 数值迭代：按台账 4.8 继续月白/周光祖单变量审计 → 弓手上限 → 装备/套装预算 → 成长档。
3. 历练桌面迁移 7 包：维持搁置；恢复前先修版本边界并重新取 clean baseline。
4. 补 `8dc5ee6..b6763a0` UI 热修簇的收尾生产记录。

---

## 5. 建议执行顺序（90 天视角）

```text
第 1 周：Phase 0 + P1-1 seed 修复 + P1-2 每帧刷新 + P1-3 资源预载
第 2 周：P1-4/P1-5 + Phase 3 Python 门禁 + 文档归档
第 3-6 周：Phase 3 测试升级 + P2-1 旧战斗下线 + P2-2 存档收敛
第 7-10 周：P2-3 BattleBoard 拆分 + P2-6 桥接/旧 HUD + P2-7 编译
第 11-13 周：P2-4 DataAsset 试点 + P2-5 ForTest 迁移 + Phase 4 资产治理
玩法 Phase 5 按用户裁决插入，不与其他高险重构并行。
```

---

## 6. 待用户决策（阻塞项）

1. **SourceAssets/SourceArt 归属**：LFS / 独立资产库 / 仅 manifest。
2. **DataAsset 化范围**：是否接受策划工作流从改 C++ 转为改 DataAsset（推荐试点 Enemy/Relic）。
3. **旧战斗下线授权**：删除旧 MVP 战斗入口属破坏性重构，需要明确授权。
4. **历练桌面 7 包**：继续搁置还是恢复；恢复前版本边界修订。
5. **`r.Streaming.PoolSize=768`**：需用户城镇/PIE 视觉验收。
6. **Git 历史重写**：是否接受 LFS migrate/BFG（会重写历史，影响所有 clone）。

---

## 7. 建议归档文档清单（Phase 0 执行）

1. 历练桌面迁移 8 篇（index+7 包，标 shelved 后归档）
2. UI PSD 旧候选链 5 篇（hero-backpack-psd-workflow、ui-calibration-v2、ui-master-phase-a、ui-master-v2-assembly、approved-ui-engine-integration）
3. 战斗 HUD 投影方案 6 篇（central-projection、projected-hud-runtime、battle-actor-resource-hud）
4. 174 卡伙伴索引组 5 篇（index+4 计划）
5. 路线临时卡旧链 3 篇（route-merchant、battle-reward-tiering、route-relics-events）
6. MVP 起点 2 篇（2026-07-01 design/implementation）
7. 青山环境旧链 6 篇
8. 遮挡试错链 4 篇
9. 动画试产 4 篇
10. 2026-08-15 箭头修复 2 篇（验收已闭环）

---

## 8. 验收标准

- **Phase 0**：`harness_state_validator` 0 finding；headless 脚本测试全绿；`all` 模式不会启动编辑器；follower 语义在 AGENTS/设计/代码三处一致。
- **Phase 1**：每项冷 UBT GREEN + 聚焦 Automation 0 failed + mutation RED 记录 + PIE 截图/日志；每帧刷新测试显示状态不变时 0 重建。
- **Phase 2**：全量 598 Automation GREEN；存档迁移矩阵通过；Shipping/Development 无测试符号（ForTest 项）；编译时间指标改善记录。
- **Phase 3**：默认门禁含 Smoke 全量；Stress 桶稳定；脚本 headless 全绿；路线通关率模型可复现且不回写单场目标。
- **Phase 4**：资产 manifest SHA256 完整；LFS/外部库决策落地；个人路径 grep 为 0（经允许的 env 解析除外）。
- **Phase 5**：按既有 balance ledger / terrain design 的 TDD 门禁执行，不回退当前 598 基线。

---

## 9. 关联记录

- `docs/production/current-goal-acceptance.md`（滚动指针）
- `docs/production/2026-08-16-optimization-followup.md`（上一轮定向优化建议）
- `docs/production/optimization-plan.md`
- `docs/production/2026-08-12-balance-tuning-ledger.md`
- 六份并行审查报告：核心规则、UI/MVP、C++ 测试、活跃文档、superpowers、Python 工具链（本轮会话产物，未落盘为独立文件）。
