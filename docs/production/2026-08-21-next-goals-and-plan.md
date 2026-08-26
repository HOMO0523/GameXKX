---
status: plan
owner: agent
created_at: 2026-08-21T15:40:00+08:00
branch: main
base_commit: 498c08b
merged_from:
  - docs/production/2026-08-21-next-goals-roadmap.md
  - docs/production/2026-08-21-next-goals-and-plan.md
supersedes: docs/production/2026-08-21-next-goals-roadmap.md
source_of_truth: docs/production/current-goal-acceptance.md
---

# GameXXK 后续目标与执行计划（2026-08-21 · 合并权威版）

> 本文件合并了 `docs/production/2026-08-21-next-goals-roadmap.md`（P0–P5 分层 / WP-A / WP-B）与 `docs/production/2026-08-21-next-goals-and-plan.md`（G0–G8 / WP0–WP8）两份并发草案，自即日起是"接下来做什么、按什么顺序做"的**唯一权威后续计划**；roadmap 文件已标 `superseded`，仅作历史记录。
> 本文件不是"已完成"声明；最终状态仍以 `docs/production/current-goal-acceptance.md`（滚动指针）为唯一真源，每个目标收尾后必须回写滚动指针。

---

## 1. 当前进程快照（截至 2026-08-21）

### 1.1 已验收（按时间倒序）

1. **纯 2D 默认入口与目标箭头吸附**（`9178682`，验收 `docs/production/2026-08-21-desktop-2d-default-pointer-acceptance.md`）：
   - 编辑器/游戏默认地图与 `MapsToCook` 均为 `/Game/GameXXK/Maps/L_DesktopTrainingHUD`；
   - `GameXXKLevelFlow::MapForScreen(Town)` 权威解析为同一 2D 地图，3D 地图仅在显式专项验收时加载；
   - 工作台"挑战"不接青山镇任务、不改变 `quest_state`，同地图打开现有全屏 BattleBoard，退出恢复工作台；
   - 目标箭头修复"浮动窗口桌面原点混入 Board-local"整段偏移与"贴图中心代替可见箭尖"局部热点：只允许 viewport-client / SafeStage 本地坐标链，可见箭尖源图点 `(1082,608)/(1254,1254)` 为热点与旋转枢轴，禁止 `LocalToAbsolute -> AbsoluteToLocal` 跨 Geometry 往返；
   - 证据：60/60 Automation（`Desktop2DCanonicalFinal-20260821`）、冷 UBT GREEN、默认地图静态合同 2/2、三尺寸/三窗口原点真实 PIE（1280×720 / 1672×941 / 1920×1080）。
2. **BlockShield 状态图标与中央空问号修复**（验收 `docs/production/2026-08-21-blockshield-status-icon-acceptance.md`）：`BASE.UI.STATUS.BLOCK_SHIELD v08` 确认，退役中央状态图标不再复活为 `? ×1`；`GameXXK.UI.Battle.Status` 2/2。
3. **桌面 HUD 角色入口 / 编队 / 直接挑战**（验收 `docs/production/2026-08-20-desktop-training-hud-roster-flow-acceptance.md`）：主角/伙伴/NPC 真实头像入口固定中栏左下，查看角色不换队，只有独立编队页"编入队伍"写入权威队伍槽；星点长条/通用错误 Tab 底永久停用。

### 1.2 已落地但未闭合验收

- **Task 6 两层退出控制**：功能代码已提交（`7d2a9f2..69c5f4b`：v23 战斗入口检查点、Battle 原子退回、RouteMap 放弃结算预览/精确一次结算、BattleBoard 与 RouteMap 两个确认弹窗；`9178682` 亦含实现）。
- **但最终真实 PIE/三分辨率视觉验收仍未闭合**：`Saved/HarnessReports/battle-retreat-route-abandon-real-flow.json` 仍为 `ok=false`（2026-08-19 22:38；请求 1672×941 被系统 DPI 0.8 钳制成 1556×884，`logical_scale=[0.8,0.8]`），`Saved/VisualReview/20260819-battle-retreat-route-abandon/` 仅有一张 `_highres_smoke_1280x720.png`。→ 列为本计划**短期最高优先（G1/WP-A）**。

### 1.3 明确缺口（未完成事项）

1. Task 6 两层退出最终验收（见上）。
2. MVP 演示启动器：`scripts/launch_desktop_mvp_demo.ps1` 已存在但**未跟踪、无配套测试**，根目录 `Launch_2D_Desktop_MVP_Demo.cmd`（未跟踪）待确认是否一并交付；`docs/production/2026-08-19-mvp-demo-delivery-plan.md` 复选框未勾。→ G2/WP-B。
3. 产品 UI/权威数据：最终 PSD 可编辑交付、剩余未确认图标（顶部音量/邮件/商店/退出、Tab 双态、节点状态、挑战/游历/重试、工具五模式、宝箱与局内专用）、天赋权威 read model/最终概率、工具真实配方/掉率、宝箱 FIFO 箱批/容量/箱内物品、三难度节点实机闭环。→ G3。
4. 性能包络：仅 2026-08-19 HUD-only 编辑器内存数据（20 s Working 3248.6 / Private 4652.8 MiB，50 s 3387.1 / 4759.6 MiB，仍远超 TaskBarHero 参照）；空壳/静置/游历/局内四组同机 Shipping(-game) 与 PIE 数据未完成。→ G4。
5. 玩法序列：地形增益重设计（Phase 1，两口径待复核）与数值迭代（Phase 2，§4.8 基准）未实施。→ G5。
6. asset-contract：58/69 通过、11 个显式 blocker（外部源缺失、保护地图 hash 漂移、视觉合同漂移、未实现 golden-asset 合同），不改保护资产追求假绿。→ G6。
7. 代码健康/架构债务：Phase 1 快赢剩余项（A5 seed 溢出等）、Phase 2/3 高险重构（ForTest 收敛、旧战斗下线、存档收敛、巨型文件拆分、DataAsset 化等）未立项。→ G7。
8. 仓库与源美术治理：`SourceAssets/`、`SourceArt/` 约 5.4 GB 未跟踪；`.gitignore` 待梳理。→ G8。

### 1.4 工作区与保护边界（执行全程不变）

- Git：根目录 `main`，HEAD `498c08b`（`docs: record canonical desktop 2d acceptance`），上一功能提交 `9178682`；`origin/main` 停在 `1589f936`，本机无 GitHub 凭据，`remote_sync: pending`——不伪造同步状态，等用户提供凭据/方式。
- `Content/GameXXK/Maps/L_Main.umap` 当前实测 SHA256 `EE6E8394E40298321F2A57CC030018BDD1109EED36248597A7D7F414E387E46B`，与 08-21 验收记录 `20BC1575…` 不一致——用户在验收快照后又改过该地图。**无论哪一版，都不提交、不覆盖、不回滚**，仅记录观测值。
- `scripts/test_battle_camera_framing.py`（用户修改）保持不动、不提交。
- `Content/Python/_*.py` 历史探针、根目录 `Private/`、`Public/`、`SourceAssets/`、`SourceArt/` 保持未跟踪，禁止 `git add -A` / `git add .`。
- 不创建 worktree；不用 UnrealBridge；不用 Live Coding / Hot Reload；C++ 验证必须冷 UBT / `scripts/ue_tdd_pipeline.py`。
- 编辑器 dirty package：先经 MCP `save_dirty_packages` 保存再决定关闭；MCP 不可用不得强关编辑器。

---

## 2. 目标分层总表

> G0–G8 为后续目标，沿用优化计划 §7 的 P0–P5 分层；两者一一映射，工作包为执行单元。

| 目标 | 分层 | 一句话动机 | 优先级 | 依赖 |
|---|---|---|---|---|
| G0 | P0 | 2D 默认链路常绿回归门禁：新默认入口是唯一日常工作面 | 持续 | 无 |
| G1（WP-A） | — | 闭合 Task 6 两层退出最终验收（唯一"已落地未验收"欠账） | **短期最高优先** | G0 |
| G2（WP-B） | — | 交付可一键复现的 MVP 演示启动器 | 短期 | 无 |
| G3 | P1 | 关闭占位图标/占位数据欠账，产品完成度主体 | 中期 | G0 |
| G4 | P3 | 2D 性能包络采样与热点优化，为发布提供同机可比数据 | 中期 | 无 |
| G5 | P5 | 恢复玩法推进主线：地形增益 → 数值迭代 | 中期 | G0 常绿 |
| G6 | P4 | asset-contract 显式 blocker 逐项收敛，不追求假绿 | 按用户决策 | 无 |
| G7 | P4 | 代码健康与架构债务（Phase 1 快赢 + Phase 2/3 立项） | 长期 | 逐项批准 |
| G8 | P4 | 仓库与源美术治理，消除误提交风险 | 长期 | 用户拍板 |

---

## 3. 详细执行

### G0 · P0 —— 2D 默认链路回归地板（持续）

**目标**：把已验收语义固化为每次改动必跑的回归面，失败即阻断提交。

必须覆盖的五条语义（来自 2026-08-21 验收）：

1. 默认地图合同：编辑器/游戏默认 `/Game/GameXXK/Maps/L_DesktopTrainingHUD`，且在 `MapsToCook` 中。
2. `GameXXKLevelFlow::MapForScreen(Town)` 解析为同一 2D 地图；3D 地图仅在显式专项验收时加载。
3. `挑战` 不依赖青山镇任务：`quest_state=NOT_ACCEPTED` 下直接进入全屏 BattleBoard。
4. 同地图往返：退出战斗恢复工作台，`screen=TOWN`、`workbench_visible=true`、BattleBoard 隐藏；任务状态不变。
5. 浮动窗口目标箭头：仅 viewport-client / SafeStage 本地坐标链；可见箭尖 `(1082,608)/(1254,1254)` 是热点与旋转枢轴；禁止跨 Geometry 往返。

**执行口径**：

- 每次 C++ / 规则 / UI 改动后按序跑：`git diff --check` → 冷 UBT → 聚焦 Automation（至少 `DesktopTraining`、`Training`、`UI.Battle.Status` + 本轮改动桶）→ `scripts/test_default_2d_entry_config.py`。
- 每轮收尾跑一次真实 PIE：`scripts/gamexxk_real_play_flow_mcp.py` 的 2D 权威链路 + 窗口尺寸/原点矩阵（1280×720 / 1672×941 / 1920×1080）。
- 表现类取证优先委托 lunamax（`~/.claude/skills/codex-vision/scripts/codex_vision.ps1 -Effort max`）；本机该脚本不存在时，用确定性坐标合同 + 真实 OS 鼠标输入 + 三尺寸截图替代，不把 NullRHI/HighResShot 黑图当证据。

### G1 · WP-A —— Task 6 两层退出最终验收（短期最高优先，预计 2–4 天）

入口依据：`docs/production/2026-08-19-task6-two-level-exit-continuation-plan.md`（阶段 A–F 完整步骤）、`docs/superpowers/plans/2026-08-19-battle-retreat-route-abandon-controls.md` Task 6 复选框、`docs/production/2026-08-19-deepseek-handoff.md` §8。

**现状**：功能代码已提交，唯一缺口是最终真实 PIE/MCP 验收与三分辨率视觉证据（报告 `ok=false`，根因 1672×941 被系统 DPI 0.8 钳制成 1556×884）。

执行阶段（详见续作计划）：

1. **阶段 A · 修复/确认高分辨率取证**：检查 `scripts/gamexxk_real_play_flow_mcp.py::capture_resolution_matrix`、`PreviewWindowController.resize_preview_window_logical`、`Content/Python/gamexxk_probe_real_play_flow.py::_handle_high_res_screenshot`；最小 smoke 对三尺寸各截一张，记录 `requested / actual / dpi / logical_scale / transport / viewport_size`；优先 PIE 内 `r.SetRes` + viewport 轮询，失败则记录环境 blocker，不改 JSON 假绿。
2. **阶段 B · 宽回归**：冷 UBT（§5 命令）+ `run_mvp_test_suites.ps1`（Route.BattleRetreat、Route.Settlement、MVP.SaveGame、MVP.RouteMap、Integration.CardRoute、Integration.CardBattle、DesktopTraining.Workbench、Training）+ Python harness 115/115 口径 + `harness_state_validator.py --json`。
3. **阶段 C · 真实 PIE/MCP**：`python scripts/gamexxk_real_play_flow_mcp.py --two-level-exit-acceptance --timeout 600 --report Saved/HarnessReports/battle-retreat-route-abandon-real-flow.json`，核对全部 named checkpoints（取消 no-op → 确认恢复检查点 → 精英重试 → 奖励 → RouteMap 预览取消/确认 → 回 Town → fixture/存档清理）。
4. **阶段 D · 视觉证据与 Luna**：`Saved/VisualReview/20260819-battle-retreat-route-abandon/` 补齐两个弹窗三分辨率 PNG（Battle/RouteMap × 1280×720 / 1672×941 / 1920×1080），覆盖工具栏可读、End Turn/Party Qi 不重叠、RouteMap 关闭按钮固定、弹窗居中不变形、无拉伸/重复工作台战斗壳/新生成艺术。
5. **阶段 E · 文档**：更新滚动指针、`2026-08-19-goal-progress-evidence.md`、`2026-08-19-deepseek-handoff.md`（补 `source_commit` 与最新证据）、Task 6 复选框；`harness_state_validator.py --json` 目标 `findings=[]`。
6. **阶段 F · 安全提交**：只提交本轮意图文件（subsystem、harness、probe、tests、docs），暂存清单核对后 commit；绝不含 §1.4 保护清单。

**验收标准**：新鲜 `ok=true` 报告（或明确环境 blocker）+ 三分辨率截图 + 冷 UBT/聚焦/Python/validator 全绿 + `L_Main.umap` 与 `L_DesktopTrainingHUD.umap` 观测 hash 未因本轮改动变化。

**中止条件**：MCP 不可用且编辑器可能 dirty → 停止报告，不强关；分辨率确达不到 → 保留显式 blocker 继续语义验收；新回归失败 → 回退本轮最小改动定位，不回滚用户资产。

### G2 · WP-B —— MVP 演示启动器交付（短期，预计 1–2 天，可与 G1 并行但不得同时占用同一 UE 编辑器进程）

入口：`docs/production/2026-08-19-mvp-demo-delivery-plan.md`。

1. 审阅现有 `scripts/launch_desktop_mvp_demo.ps1`（未跟踪）与 `Launch_2D_Desktop_MVP_Demo.cmd`（未跟踪，确认是否一并交付）。
2. 补 `scripts/test_launch_desktop_mvp_demo.py`（或扩展 `test_interactive_editor_launcher.py`）：`-DescribeOnly` JSON 合同、三种 `-Profile` 参数组合、编辑器路径缺失报错、不启动 UE 的纯参数测试。
3. `scripts/README.md` 加入启动器条目；勾选 MVP 计划复选框；给出用户可见演示路径（workbench / travel / challenge）。
4. 真实 smoke：在用户许可下运行三种 Profile，确认窗口打开、地图为 `L_DesktopTrainingHUD`、工作台/游历/Battle 可见。
5. 提交：只提交启动器、测试、README、计划文档；不含用户资产。

**验收标准**：`-DescribeOnly` 合同通过；三模式至少各一次真实 smoke；未触碰保护资产。

### G3 · P1 —— 产品 UI 与权威数据补完（中期，按依赖顺序分 6 小包，各包独立验收）

1. **最终 PSD 可编辑交付（视觉底座）**：约 `1200×108` 顶部挂机条 + 工作台主页面，按冻结分层 `BG_CharcoalInk / BG_MountainSilhouette / BG_Path / BG_Decor / FX_GroundShadow / Actors_ExistingSprites / HUD_RuntimeOnly` 交付。实机截图与 `Content/GameXXK/UI` 批准资源是唯一视觉基准；GPT 生图只允许无文字/无 UI/无角色怪物的背景板；复用/拆分现有资源、等比、nearest-neighbor、禁止非等比拉伸。候选 `TrainingIdleStrip_Background_GPT_v003_Seamless_RGBA.png` 仍为 draft，不得当作完成证据。验收：PSD 分层清单 + UE 资产合同 + 1920×1080 与 2560×1440 整体截图 + Luna/用户复核。
2. **ImageTruth 逐张确认队列**：当前 8/8 `ok=true`；剩余顶部按钮、Tab 双态、节点状态、挑战/游历/重试、工具五模式、宝箱与局内专用图标；候选图只有用户确认后才晋升导入，每张记录源图与 UE 资产双 SHA-256。验收：逐张合同 `ok=true` + 对应 DesktopTraining 聚焦测试。
3. **天赋权威数据（read model）**：真实天赋 read model、最终概率/掉率，替换占位；挑战/游历读同一权威数据。验收：规则层确定性单测 + 工作台天赋页真实数据 PIE 取证。
4. **工具真实规则（五模式 3×3）**：配方数据权威化（确定性与单测）+ 真实 PIE 工具替换右栏流程；未接配方不消耗语义保持。
5. **宝箱 FIFO / 容量 / 箱内物品**：规则单测（批处理顺序、容量边界、物品抽取）+ 顶部挂机条开箱表现 PIE；普通/精英箱 240/360 秒冷却规则保持。
6. **三难度状态与失败重试实机闭环**：真实 PIE 下三难度节点进入、失败重试、1-1 与挑战同生命/编制口径。

### G4 · P3 —— 2D 性能包络采样与热点优化（中期，预计 3–5 天）

1. **固定测量方法**：同机、同窗口尺寸、同启动参数；采集 CPU、GPU、帧时间、Working Set / Private Bytes / GPU Dedicated；场景四组——空壳（仅启动无工作台）、工作台静置、游历挂机（顶部条动画中）、局内 BattleBoard；分别记录 PIE 与 Shipping(-game)；3D 青山镇只在显式对照任务中采集。沿用 `61c9281` 四档性能 harness，补齐数据而非新造工具。
2. **报告**：产出 `docs/production/2026-08-21-performance-baseline.md`，数据带时间戳、进程、模式、分辨率、地图。基线参照：2026-08-19 HUD-only 编辑器 20 s Working 3248.6 / Private 4652.8 MiB，50 s 3387.1 / 4759.6 MiB。
3. **热点优化（按优化方案优先级）**：A6 每帧全量刷新 → 脏标记/事件驱动；A7 手牌 preview 每帧重算 → 缓存（费用/目标/牌变化才重算）；A8 同步 LoadObject → 软引用预载 + Brush 缓存（复用 AtlasCache）；编辑器固定开销/插件资产单独分析，给出可回滚减负项逐项单独提交。
4. **验证**：每项 C++ 改动冷 UBT + 聚焦 Automation + 性能前后对比；任何减负改动必须 P0 回归常绿。

**验收标准**：四组同机数据可复现 + 热点优化前后对比 + 不破坏 2D 默认链路。目标包络数值需用户拍板（§6 第 2 项）。

### G5 · P5 —— 玩法序列：地形增益重设计 → 数值迭代（中期）

1. **Phase 1 地形增益**：先复核 `docs/design/2026-08-13-terrain-benefit-redesign.md` §5 山河三档与 `TriggerTerrainBenefit` 卡牌口径两个待定项（结论需用户确认，§6 第 6 项）；确认后按 TDD 落地新表（每回合全员 1 次 + 对应职业再 1 次 + 山河套再 1 次，敌方统一 1 易伤 + 1 燃烧）。
2. **Phase 2 数值迭代**：以 `docs/production/2026-08-12-balance-tuning-ledger.md` §4.8 为最新基准；先跑 `9598072`/`e78be7c` 后的 2400/2520 矩阵新基线并写回台账（带 SHA 与日期），再继续单变量审计（月白/周光祖、弓手上限、装备/套装预算、成长档）。
3. 继续维护非阻塞测试工具：保证 `gamexxk_real_play_flow_mcp.py` 等与冻结语义同步，不把旧断言当门禁。

### G6 · P4 —— asset-contract 显式 blocker 收敛（按用户决策推进）

当前 58/69 通过、11 个显式 blocker，分类处置：

- **外部源缺失**（`063/064/065.png`、`057.png`、`036.png` 等在个人 Downloads 路径）→ 请用户提供或批准改源，未提供前保持 blocker；
- **保护地图基线漂移**（`L_QingshanInn.umap` 7856… vs 合同 a363…）→ 需用户批准重定基线，禁止覆盖/回滚地图；
- **未实现 golden-asset 合同**（`qingshan_building_concepts` 的 `NotImplementedError`）→ 独立功能立项；
- **视觉合同漂移**（`reference_faithful_task_ui_icons`、`reward_coin/exp/token` 等）→ 委托 Luna/用户复核后再更新合同。

验收：每个 blocker 有明确状态（resolved / waiting-user / waiting-assets / accepted-known-failure），无隐藏假绿；asset-contract 保持与默认 headless 门禁分离，单独维护 blocker 清单。

### G7 · P4 —— 代码健康与架构债务（长期，逐项立项、先批准后 TDD）

按 `docs/production/2026-08-16-full-project-optimization-proposal.md`：

1. **Phase 1 剩余正确性/性能快赢**：A5 seed 溢出（最高优先 bug：`NodeId * 486187739` int32 乘法 UB → uint32/int64 + 统一 seed 工具 + 固定种子回归）；A6/A7 每帧刷新；A8 同步加载；A12 错误传播（奖励结算忽略返回值 → 失败回滚）；A11 TMap 索引；公共工具抽取。
2. **Phase 2 高险重构（每项先用户批准/评审，再 TDD 独立提交）**：A1 旧 MVP 战斗下线（确认无 UI/蓝图引用后）；A2 存档收敛 + v1→v17 全迁移矩阵；A3 巨型文件拆分（`CardRules.cpp` 16,258 行、`BattleBoardWidget.cpp` 8,956 行）；A4 catalog DataAsset/DataTable 化试点（保留确定性单测）；B2 `WITH_DEV_AUTOMATION_TESTS` 收敛（75 文件约 1,155 处 `*ForTest`，分 3 批迁移 + Shipping 构建回归）；A9/A10 桥接/旧 HUD 去反射；A15 adaptive unity 基准（全量编译时间下降 ≥30% 才切换）。
3. **Phase 3 测试体系升级**：测试旗标/超时/Disabled、公共 fixture、整局路线通关率模型、关键 Widget 像素级验收（委托 lunamax）。

验收：每个重构包冷 UBT GREEN + 聚焦 Automation 0 failed + mutation RED 记录 + 真机 PIE；全量回归基线不下降；提交不含用户资产。

### G8 · P4 —— 仓库与源美术治理（长期，需用户拍板）

1. `SourceAssets/`、`SourceArt/` 约 5.4 GB 未跟踪：决策前只生成只读 manifest（路径/SHA256/大小），不做批量 `git add`；候选路线 Git LFS / 独立资产库 / 网盘 + manifest（§6 第 3 项相关）。
2. `Content/Python/_*.py`、`scripts/_*.py` 历史探针按 Phase 1.3 模式归档到 `_archive/`（仅移动、不删除，先确认无生产脚本 import）。
3. 梳理 `.gitignore`（源美术、根 `Private/` `Public/`、探针、生成物），使 `git status` 可读、误提交风险消除。
4. 提交纪律：提交前必查 `git diff --cached --name-only`；禁 `git add -A` / `git add .`。

---

## 4. 统一执行顺序

```text
第一批（立即，顺序执行）:
  WP0 基线快照（记录 hash/门禁现状，产出 docs/production/2026-08-21-baseline-snapshot.md）
  → G1/WP-A Task 6 两层退出最终验收（短期最高优先）
  → G2/WP-B MVP 演示启动器交付（可与 WP-A 并行，但不占用同一编辑器进程）

第二批（中期，可并行）:
  G3 各小包（PSD → ImageTruth → 天赋/工具/箱批/三难度）
  + G2 之后战斗可用性打磨（与 ImageTruth 队列共享流程，见 G0 语义 5 与 P2 打磨项）
  + G4 性能采样（独立同机环境即可开跑）

第三批（玩法/长期）:
  G5 地形增益复核与数值迭代 → G6/G7/G8 按用户批准顺序逐项立项
```

- 任何改动先过 G0/P0 回归；WP-A/WP-B 涉及编辑器/PIE 的命令不得同时占用同一 UE 编辑器进程。
- G7 高险重构必须单独批准、单独立项，不与玩法混入。
- G6/G8 等用户决策的事项不阻塞本地开发。

---

## 5. 验证命令速查（全部通过才算一轮收尾）

```powershell
git diff --check
# 冷 UBT（禁 Live Coding / Hot Reload）
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -WaitMutex -NoHotReload -NoHotReloadFromIDE -NoUBA -MaxParallelActions=2
# 聚焦自动化（按本轮改动范围选桶）
& scripts/run_mvp_test_suites.ps1 -Suites @('GameXXK.DesktopTraining.Workbench','GameXXK.Training','GameXXK.MVP.SaveGame','GameXXK.UI.Battle.Status') -TimeoutSeconds 1500
# Task 6 宽回归桶
& scripts/run_mvp_test_suites.ps1 -Suites @('GameXXK.Route.BattleRetreat','GameXXK.Route.Settlement','GameXXK.MVP.SaveGame','GameXXK.MVP.RouteMap','GameXXK.Integration.CardRoute','GameXXK.Integration.CardBattle','GameXXK.DesktopTraining.Workbench','GameXXK.Training') -TimeoutSeconds 1500
# 脚本门禁
python scripts/ai_production_loop.py --run-script-tests --script-tests all --json
python scripts/harness_state_validator.py --json
python scripts/test_default_2d_entry_config.py
python scripts/gamexxk_ui_image_truth_check.py --json
# 真实 PIE（UE MCP 可用时）
python scripts/gamexxk_real_play_flow_mcp.py --two-level-exit-acceptance --timeout 600 --report Saved/HarnessReports/<本轮名>-real-flow.json
```

---

## 6. 待用户拍板清单（阻塞对应目标）

| # | 事项 | 阻塞目标 |
|---|---|---|
| 1 | GitHub 凭据 / 远程同步方式（`origin/main` 停在 `1589f936`，本地领先） | G8/P4-1 |
| 2 | 目标性能包络数值（内存/CPU/GPU 上限） | G4 |
| 3 | 外部 PSD 源文件提供方式（Downloads 路径 063/064/065、057、036） | G3、G6 |
| 4 | `L_QingshanInn.umap` 保护 hash 重定基线批准 | G6 |
| 5 | Phase 3 架构债务逐项立项批准（旧战斗下线/存档收敛/巨型文件拆分/DataAsset 化） | G7 |
| 6 | 地形增益重设计两个口径复核结论（§5 山河三档、`TriggerTerrainBenefit`） | G5 |
| 7 | `WITH_DEV_AUTOMATION_TESTS` 收敛立项（迁移方案 + Shipping 回归） | G7 |
| 8 | `L_Main.umap` 当前实测 `EE6E…` 与 08-21 验收记录 `20BC…` 不一致：确认哪一版是用户最新意图；无论结论如何，工作树版本保持不动、不提交 | 全部工作包 hash 核对口径 |
| 9 | ✅ 两份后续计划合并裁决 → 已由本文件完成；原 roadmap 标 `superseded` | 文档单轨治理 |

---

## 7. 风险与中止条件

1. **环境分辨率限制**：若 1672×941 / 1920×1080 无法在 PIE 中真实达到，必须记录实际尺寸与根因，保留显式 blocker，不伪造截图/JSON。
2. **MCP 不可用且编辑器可能 dirty**：不强关编辑器；先保存 dirty packages，失败则中止并报告。
3. **用户资产改动**：`L_Main.umap`、`scripts/test_battle_camera_framing.py`、`SourceAssets/`、`SourceArt/`、根 `Private/` `Public/` 均不得纳入本计划任何工作包的提交。
4. **回归新失败**：停止当前包，先定位最小差异；不回滚用户资产。
5. **旧报告冒充新证据**：所有验收以本轮新鲜报告为准；历史 598/598、旧 JSON 只作对照。

---

## 8. 完成定义（本轮"后续计划"的收尾条件）

- WP0 基线快照完成（`docs/production/2026-08-21-baseline-snapshot.md`）；
- G1/WP-A 产出新鲜 Task 6 验收结论（通过或明确 blocker）；
- G2/WP-B 启动器可复现运行并有测试；
- G3/G4/G5/G6/G7/G8 至少完成第一批并形成可继续执行的下一轮状态；
- 滚动指针 `current-goal-acceptance.md` 已更新为最新事实，不包含未经验证的完成声明。

---

## 9. 保护边界（每轮执行前默念）

- 默认入口保持 2D `L_DesktopTrainingHUD`；未明确要求 3D 时，开发、PIE、截图、验收全部留在 2D 桌面 → BattleBoard 链路。
- 不覆盖/回滚/重调：`L_Main.umap`、角色 Sprite、PaperZD、关卡放置、相机、HD2D Plane；工作树中用户的 `L_Main.umap` 与 `scripts/test_battle_camera_framing.py` 修改保持不动、不提交。
- 不用 UnrealBridge / Live Coding / Hot Reload；C++ 验证走冷 UBT / `scripts/ue_tdd_pipeline.py`。
- 提交用精确文件清单，禁 `git add -A` / `git add .`；`SourceAssets/`、`SourceArt/`、历史探针不批量入库。
- 表现类问题先取证、优先委托 lunamax，不自行盲改绘制层公式；方向向量只参与旋转/法线，不参与位置平移。
