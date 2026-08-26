---
status: superseded
owner: agent
created_at: 2026-08-21
base_commit: 498c08b
superseded_by: docs/production/2026-08-21-next-goals-and-plan.md
source_of_truth: docs/production/current-goal-acceptance.md
---

# GameXXK 后续目标与执行计划(2026-08-21 起点)〔已合并 · 历史记录〕

> ⚠️ **本文件已于 2026-08-21 被合并进 `docs/production/2026-08-21-next-goals-and-plan.md`(G0–G8 / P0–P5 / WP0–WP8 合并权威版)。自即日起不再作为执行口径,仅保留为历史记录。**
>
> 原内容:本文件是"纯 2D 默认入口验收"之后的**后续路线图**,承接 `docs/production/current-goal-acceptance.md`(滚动指针)与 `docs/production/optimization-plan.md` §7。
> 它按优先级列出目标、验收口径、执行顺序与待用户拍板事项;每个目标收尾后仍以滚动指针为准,本文件只负责"接下来做什么、按什么顺序做"。

---

## 1. 当前进程快照(截至 2026-08-21)

- 分支 `main`,HEAD `498c08b`(`docs: record canonical desktop 2d acceptance`),上一功能提交 `9178682`(`feat: finalize canonical desktop 2d battle flow`)。
- **已验收(本轮)**:默认地图/`Town` 解析均为纯 2D `L_DesktopTrainingHUD`;`挑战` 不接任务、直接在同地图打开现有全屏 BattleBoard、退出恢复工作台且任务状态不变;目标箭头修复"浮动窗口桌面原点混入"整段偏移与"贴图中心代替可见箭尖"热点。证据:60/60 Automation(`Desktop2DCanonicalFinal-20260821`)、冷 UBT GREEN、三尺寸/三窗口原点真实 PIE、默认地图静态合同 2/2。权威记录:`docs/production/2026-08-21-desktop-2d-default-pointer-acceptance.md`。
- **已验收(同日)**:BlockShield 状态图标 v08 与中央空问号生命周期修复;`GameXXK.UI.Battle.Status` 2/2。记录:`docs/production/2026-08-21-blockshield-status-icon-acceptance.md`。
- **已落地(前序)**:两层退出确认(Task 6)功能代码已提交(`7d2a9f2..69c5f4b`:v23 检查点、Battle 原子退回、RouteMap 放弃结算、两个确认弹窗)。**但其最终真实 PIE/三分辨率视觉验收仍未闭合**:`Saved/HarnessReports/battle-retreat-route-abandon-real-flow.json` 仍为 `ok=false`(2026-08-19 22:38,1672×941 实际截得 1556×884,DPI 0.8),视觉目录只有一张 `_highres_smoke_1280x720.png`;见 §3.6,列为本计划短期最高优先。
- MVP 演示启动器 `scripts/launch_desktop_mvp_demo.ps1` 存在但未跟踪、无配套测试,`docs/production/2026-08-19-mvp-demo-delivery-plan.md` 未打勾;见 §4.7。
- **工作树保护对象**:`Content/GameXXK/Maps/L_Main.umap`(用户修改,已跟踪)、`scripts/test_battle_camera_framing.py`(用户修改)保持不动、不提交;`Content/Python/_*.py` 历史探针保持未跟踪。
  - `L_Main.umap` 当前实测 SHA256 为 `EE6E8394E40298321F2A57CC030018BDD1109EED36248597A7D7F414E387E46B`,与 2026-08-21 验收记录的 `20BC1575…` 不一致——说明用户在该验收快照之后又改动了该地图。**无论哪个版本,都不提交、不覆盖、不回滚**,仅记录观测值。
- **未完成交接**:`remote_sync: pending`——本机无 GitHub 凭据,`origin/main` 停在 `1589f936`;见 §11 待用户拍板。
- **文档治理提示**:工作树中同时存在并发计划 `docs/production/2026-08-21-next-goals-and-plan.md`(G0–G8/WP0–WP8 结构,owner: codex),与本文件内容重叠。两份都未提交;建议用户裁决后**合并为一份**,避免再次出现本项目 P2 式"文档双轨漂移"。本文件已吸收其全部关键事实(Task 6 未闭合、启动器未交付、性能四组缺口、asset-contract 分类、仓库治理),可独立作为权威版本。

---

## 2. 目标分层与优先级

> 分层沿袭 `optimization-plan.md` §7 的 P0–P3,并按现状补充 P4(工程健康/债务)与 P5(玩法序列)。

| 层 | 目标 | 一句话动机 | 依赖 |
|---|---|---|---|
| P0 | 2D 默认链路常绿回归门禁 | 新默认入口是唯一日常工作面,任何改动不能破坏它 | 无 |
| P1 | 剩余产品 UI 与权威数据补完 | 关闭"临时图标/占位数据"欠账,是产品完成度的主体 | P0 |
| P2 | 战斗可用性(BattleBoard 共享局内) | 局内是玩家核心体验,继续修读性/反馈/状态表现 | P0 |
| P3 | 2D 性能包络采样 | 为发布与后续优化提供同机可比数据;不再作为切默认入口的前置门禁(入口已由用户批准切换) | 无 |
| P4 | 工程健康与债务处置 | 远程同步、测试收敛、asset-contract 分类、Phase 3 债务立项 | 各项目独立 |
| P5 | 玩法序列:地形增益重设计 → 数值迭代 | Phase 0 已收尾,恢复玩法推进主线 | P0 常绿 |

---

## 3. P0 —— 2D 默认链路常绿回归门禁

**目标**:把已验收语义固化为每次改动必跑的回归面,失败即阻断提交。

必须覆盖的语义(来自 2026-08-21 验收):

1. 默认地图合同:编辑器/游戏默认 `/Game/GameXXK/Maps/L_DesktopTrainingHUD`,且该地图在 `MapsToCook` 中。
2. `GameXXKLevelFlow::MapForScreen(Town)` 解析为同一 2D 地图;3D 地图仅在显式专项验收时加载。
3. `挑战` 不依赖青山镇任务:`quest_state=NOT_ACCEPTED` 下直接进入全屏 BattleBoard。
4. 同地图往返:退出战斗恢复工作台,`screen=TOWN`、`workbench_visible=true`、BattleBoard 隐藏。
5. 浮动窗口目标箭头:仅使用 viewport-client / SafeStage 本地坐标链,可见源图箭尖 `(1082,608)/(1254,1254)` 是热点与旋转枢轴;禁止 `LocalToAbsolute -> AbsoluteToLocal` 跨 Geometry 往返。

**执行口径**:

- 每次 C++ / 规则 / UI 改动后按序跑:`git diff --check` → 冷 UBT → 聚焦 Automation(至少 `DesktopTraining`、`Training`、`UI.Battle.Status`、本轮改动桶)→ `scripts/test_default_2d_entry_config.py`。
- 每周或每轮收尾跑一次真实 PIE:`scripts/gamexxk_real_play_flow_mcp.py` 的 2D 权威链路 + 窗口尺寸/原点矩阵(1280×720 / 1672×941 / 1920×1080)。
- 表现类取证优先委托 lunamax(`~/.claude/skills/codex-vision/scripts/codex_vision.ps1 -Effort max`);本机该脚本不存在时,用确定性坐标合同 + 真实 OS 鼠标输入 + 三尺寸截图替代(2026-08-21 已采用),不把 NullRHI/HighResShot 黑图当证据。

### 3.6 WP-A —— Task 6 两层退出最终验收(短期最高优先,预计 2–4 天)

**现状**:功能代码已提交,唯一缺口是最终真实 PIE/MCP 验收与三分辨率视觉证据(报告 `ok=false`,根因为 1672×941 请求被系统 DPI 0.8 钳制成 1556×884)。

执行依据:`docs/production/2026-08-19-task6-two-level-exit-continuation-plan.md`(阶段 A–F 完整步骤)、`docs/superpowers/plans/2026-08-19-battle-retreat-route-abandon-controls.md` Task 6 复选框、`docs/production/2026-08-19-deepseek-handoff.md` §8。

步骤:

1. **修复/确认高分辨率取证**:检查 `scripts/gamexxk_real_play_flow_mcp.py::capture_resolution_matrix`、`PreviewWindowController.resize_preview_window_logical`、`Content/Python/gamexxk_probe_real_play_flow.py::_handle_high_res_screenshot`;最小 smoke 对三尺寸各截一张,记录 `requested / actual / dpi / logical_scale / transport / viewport_size`;优先尝试 PIE 内 `r.SetRes` + viewport 轮询,失败则记录环境 blocker,不改 JSON 假绿。
2. **宽回归**:冷 UBT(§10)+ `run_mvp_test_suites.ps1`(Route.BattleRetreat、Route.Settlement、MVP.SaveGame、MVP.RouteMap、Integration.CardRoute、Integration.CardBattle、DesktopTraining.Workbench、Training)+ Python harness 115/115 口径 + `harness_state_validator.py --json`。
3. **真实 PIE/MCP**:`python scripts/gamexxk_real_play_flow_mcp.py --two-level-exit-acceptance --timeout 600 --report Saved/HarnessReports/battle-retreat-route-abandon-real-flow.json`,核对全部 named checkpoints(取消 no-op → 确认恢复检查点 → 精英重试 → 奖励 → RouteMap 预览取消/确认 → 回 Town → fixture/存档清理)。
4. **视觉证据与 Luna**:`Saved/VisualReview/20260819-battle-retreat-route-abandon/` 补齐两个弹窗三分辨率 PNG,覆盖工具栏可读、End Turn/Party Qi 不重叠、RouteMap 关闭按钮固定、弹窗居中不变形。
5. **文档与提交**:更新滚动指针、`2026-08-19-goal-progress-evidence.md`、Task 6 复选框;只提交本轮意图文件,绝不含 §12 保护清单。

**验收**:新鲜 `ok=true` 报告(或明确环境 blocker)、三分辨率截图、冷 UBT/聚焦/Python/validator 全绿、`L_Main.umap` 与 `L_DesktopTrainingHUD.umap` 观测 hash 未因本轮改动变化。

---

## 4. P1 —— 剩余产品 UI 与权威数据补完

按依赖顺序拆为可独立验收的工作包:

### 4.1 最终 PSD 可编辑交付(视觉底座)

- 范围:约 `1200×108` 顶部挂机条 + 工作台主页面,按冻结分层 `BG_CharcoalInk / BG_MountainSilhouette / BG_Path / BG_Decor / FX_GroundShadow / Actors_ExistingSprites / HUD_RuntimeOnly` 交付。
- 规则(用户已冻结):实机截图与 `Content/GameXXK/UI` 批准资源是唯一视觉基准;GPT 生图只允许无文字、无 UI、无角色/怪物的背景板;框体、按钮、图标、文字、角色、敌人必须复用/拆分现有资源,保持等比与 nearest-neighbor,禁止非等比拉伸。
- 已登记候选:`SourceArt/UI/PSD/desktop-training-v1/generated/TrainingIdleStrip_Background_GPT_v003_Seamless_RGBA.png` 仍为 draft,不得当作 PSD 完成证据。
- 验收:PSD 分层清单、导入后 UE 资产合同、1920×1080 与 2560×1440 整体工作台截图、Luna/用户视觉复核。

### 4.2 ImageTruth 逐张确认队列(图标真源)

- 现状:`scripts/gamexxk_ui_image_truth_check.py --json` 为 `ok=true`,确认数 8/8(两态图钉等)。
- 剩余队列:顶部音量/邮件/商店/退出按钮、Tab、历练节点状态(可达/锁定/完成)、挑战/游历/重试、工具五模式、宝箱与局内专用图标。
- 规则:候选图只有用户确认后才晋升并导入 PSD/UE;每张记录源图与 UE 资产双 SHA-256。
- 验收:逐张 ImageTruth 合同 `ok=true` + 对应 DesktopTraining 聚焦测试。

### 4.3 天赋权威数据(read model)

- 目标:真实天赋 read model、最终概率/掉率配置,替换现有占位;挑战/游历读取同一权威数据。
- 验收:规则层确定性单测 + 工作台天赋页真实数据展示的 PIE 取证。

### 4.4 工具真实规则(五模式 3×3)

- 现状:五模式固定 3×3 非破坏性输入与确定按钮已落地,未接配方不消耗;真实配方/产出规则仍缺。
- 验收:配方数据权威化(确定性与单测)+ 真实 PIE 工具替换右栏流程。

### 4.5 宝箱 FIFO / 容量 / 箱内物品

- 现状:普通/精英箱冷却规则(240/360 秒)已有;FIFO 箱批、容量上限、箱内物品定义未完成。
- 验收:规则单测(批处理顺序、容量边界、物品抽取)+ 顶部挂机条开箱表现 PIE。

### 4.6 三难度状态与失败重试实机闭环

- 现状:普通/困难/地狱规则与聚焦测试已存在;最终路线图节点交互、失败重试的实际表现链未完成。
- 验收:真实 PIE 下三难度节点进入、失败重试、1-1 与挑战同生命/编制口径。

### 4.7 WP-B —— MVP 演示启动器交付(短期,预计 1–2 天)

- 现状:`scripts/launch_desktop_mvp_demo.ps1` 已存在但未跟踪、无配套测试;`docs/production/2026-08-19-mvp-demo-delivery-plan.md` 未打勾;根目录 `Launch_2D_Desktop_MVP_Demo.cmd`(未跟踪)需确认是否一并交付。
- 工作:① 补 `-DescribeOnly` JSON 合同、三种 `-Profile` 组合、编辑器路径缺失报错、不启动 UE 的纯参数测试;② `scripts/README.md` 加入启动器条目;③ 在用户许可下真实 smoke `workbench / travel / challenge` 三模式(窗口打开、地图正确、工作台/游历/Battle 可见);④ 只提交启动器、测试、README、计划文档。
- 验收:`-DescribeOnly` 合同通过、三模式至少各一次真实 smoke、未触碰保护资产。

---

## 5. P2 —— 战斗可用性(共享 BattleBoard)

原则:继续复用现有全屏 BattleBoard,不重建内嵌 ChallengeViewport。

1. **状态图标收尾**:BlockShield 已验收;梳理其余状态图标的运行时渲染与层数显示,逐张 ImageTruth 确认。
2. **卡牌反馈与 tooltip**:延续 8-16 tooltip 热修口径,修读性/换行/药丸排序/跟随定位保持确定性;发现表现问题时先取证、再改,不盲改绘制公式(8-15 箭头教训)。
3. **自动战斗可读性**:自动出牌、意图标记、敌方反击的节奏与高亮反馈;`自动战斗`/`关闭`/`回合结束` 三控件位置语义已冻结,不得重排。
4. **目标选择**:箭头吸附已修复;继续验证治疗/敌方双方向、拖拽/悬停边界与 DPI 缩放。
- 验收:每项聚焦 Automation + 真实 PIE 截图 + Luna max(或等价替代取证)。

---

## 6. P3 —— 2D 性能包络采样

- **四组同机数据**(2D 为主):空壳(仅启动无工作台)、工作台静置、游历挂机(顶部条动画中)、局内 BattleBoard 战斗;3D 青山镇只在显式对照任务中采集。
- **指标**:CPU、GPU、帧时间、Working Set / Private Bytes / GPU Dedicated,分别记录 PIE 与 Shipping(-game)环境;沿用 `61c9281` 已加入的四档性能 harness,补齐数据而非新造工具。
- **现状基线**:2026-08-19 HUD-only 编辑器测量 20 秒 Working 3248.6 / Private 4652.8 MiB,50 秒 3387.1 / 4759.6 MiB,仍远超 TaskBarHero 参照与目标包络;编辑器固定开销/插件资产是主因。
- **后续动作**:① 固定同机四组采样并保存 JSON 证据;② 定位编辑器固定开销与插件资产大头,给出可回滚的减负项(如启动参数、插件禁用清单),逐项单独提交;③ 与用户对齐目标包络数值(见 §7)。
- **验收**:四组采样报告 + 每组 CPU/GPU/内存/帧率可比表;任何减负改动必须 P0 回归常绿。

---

## 7. P4 —— 工程健康与债务处置

1. **远程同步(立即)**:本机无 GitHub 凭据、认证 API 不可达,`origin/main` 停在 `1589f936`,本地领先 4 个提交。需要用户提供凭据或指定同步方式后才执行 push;不伪造 `remote_sync` 状态。
2. **`WITH_DEV_AUTOMATION_TESTS` 收敛(已评估暂缓)**:503 处 `*ForTest` 遍布 30+ 生产头文件,属接口重设计;建议单独立项(迁移方案 + Shipping 构建回归)后再执行。
3. **Phase 3 架构债务(仅规划,逐项需用户明确批准)**:
   - 3.1 新旧状态合并(删 `FGameXXKRuntimeState`/`FGameXXKSaveState` legacy 重复字段,必要时 bump SaveVersion 写迁移);
   - 3.2 巨型文件拆分(`GameXXKCardTypes.h` ~2400 行、`GameXXKBattleBoardWidget.cpp` 8956+ 行);
   - 3.3 数据 DataAsset 化(静态 catalog 迁移,保留确定性);
   - 3.4 测试盲区补齐(整局路线通关率建模、关键 Widget 像素级/PIE 验收)。
4. **asset-contract 分类处置(不追求假绿)**:
   - 外部源缺失(`063/064/065.png`、`057.png`、`036.png` 等在个人 Downloads 路径)→ 请用户提供或批准改源;
   - 保护地图 hash 漂移(`L_QingshanInn.umap` 7856… vs 合同 a363…)→ 需用户批准重定基线,禁止覆盖/回滚地图;
   - 未实现 golden-asset 合同(`qingshan_building_concepts` 的 `NotImplementedError`)→ 独立功能立项;
   - 视觉合同漂移(`reference_faithful_task_ui_icons` 等)→ 委托 Luna/用户复核后再改图。
5. **一次性探针归档**:`Content/Python/_*.py` 历史探针沿用 Phase 1.3 模式移入 `_archive/`(仅移动,不删除),确认无生产脚本 import 后再归档。
6. **仓库与源美术治理(需用户拍板)**:`SourceAssets/`、`SourceArt/` 约 5.4 GB 未跟踪——决策前只生成只读 manifest(路径/SHA256/大小),不做批量 `git add`;候选路线:Git LFS / 独立资产库 / 网盘+manifest。同时梳理 `.gitignore`(源美术、根 `Private/`/`Public/`、探针、生成物),使 `git status` 可读、误提交风险消除。

---

## 8. P5 —— 玩法序列(Phase 0 之后的推进主线)

1. **Phase 1 地形增益重设计**:先复核 `docs/design/2026-08-13-terrain-benefit-redesign.md` §5 山河三档与 `TriggerTerrainBenefit` 卡牌口径两个待定项,确认后按 TDD 落地新表(每回合全员 1 次 + 对应职业再 1 次 + 山河套再 1 次,敌方统一 1 易伤 + 1 燃烧)。
2. **Phase 2 数值迭代**:以 `docs/production/2026-08-12-balance-tuning-ledger.md` §4.8 为最新基准。
3. 继续维护非阻塞测试工具:保证 `gamexxk_real_play_flow_mcp.py` 等与冻结语义同步,不把旧断言当门禁。

---

## 9. 统一执行顺序(推荐)

```text
第一批(立即,顺序执行):
  WP0 基线快照(记录 hash/门禁现状,产出 docs/production/2026-08-21-baseline-snapshot.md)
  → WP-A Task 6 两层退出最终验收(§3.6,短期最高优先)
  → WP-B MVP 演示启动器交付(§4.7,可与 WP-A 并行,但不占用同一编辑器进程)

第二批(中期,可并行):
  P1 各工作包(4.1 PSD → 4.2 ImageTruth → 4.3/4.4/4.5 数据)
  + P2 战斗可用性(与 ImageTruth 队列共享流程)
  + P3 性能采样(独立同机环境即可开跑)

第三批(玩法/长期):
  P5 地形增益复核与数值迭代 → P4 债务项按用户批准顺序立项
```

- 任何改动先过 §3 的 P0 回归;WP-A/WP-B 涉及编辑器/PIE 的命令不得同时占用同一 UE 编辑器进程。
- P4-1(远程同步)等用户凭据,不阻塞本地开发。
- 高险重构(P4-3)必须单独批准、单独立项,不与玩法混入。

---

## 10. 验证命令速查(全部通过才算一轮收尾)

```powershell
git diff --check
# 冷 UBT(禁 Live Coding / Hot Reload)
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -WaitMutex -NoHotReload -NoHotReloadFromIDE -NoUBA -MaxParallelActions=2
# 聚焦自动化(按本轮改动范围选桶)
& scripts/run_mvp_test_suites.ps1 -Suites @('GameXXK.DesktopTraining.Workbench','GameXXK.Training','GameXXK.MVP.SaveGame','GameXXK.UI.Battle.Status') -TimeoutSeconds 1500
# 脚本门禁
python scripts/ai_production_loop.py --run-script-tests --script-tests all --json
python scripts/harness_state_validator.py --json
python scripts/test_default_2d_entry_config.py
python scripts/gamexxk_ui_image_truth_check.py --json
# 真实 PIE(UE MCP 可用时)
python scripts/gamexxk_real_play_flow_mcp.py --two-level-exit-acceptance --timeout 600 --report Saved/HarnessReports/<本轮名>-real-flow.json
```

---

## 11. 待用户拍板事项(阻塞对应目标)

| # | 事项 | 阻塞目标 |
|---|---|---|
| 1 | GitHub 凭据 / 远程同步方式 | P4-1 |
| 2 | 目标性能包络数值(内存/CPU/GPU 上限) | P3 |
| 3 | 外部 PSD 源文件提供方式(Downloads 路径 063/064/065、057、036) | P1、asset-contract |
| 4 | `L_QingshanInn.umap` 保护 hash 重定基线批准 | asset-contract |
| 5 | Phase 3 架构债务逐项立项批准(3.1–3.4) | P4-3 |
| 6 | 地形增益重设计两个口径复核结论(§5 山河三档、TriggerTerrainBenefit) | P5-1 |
| 7 | `WITH_DEV_AUTOMATION_TESTS` 收敛立项(迁移方案 + Shipping 回归) | P4-2 |
| 8 | `L_Main.umap` 当前实测 `EE6E…` 与 08-21 验收记录 `20BC…` 不一致:确认哪一版是用户最新意图;无论结论如何,工作树版本保持不动、不提交 | 全部工作包的 hash 核对口径 |
| 9 | 两份后续计划文档(`2026-08-21-next-goals-and-plan.md` 与 `2026-08-21-next-goals-roadmap.md`)合并为一份的裁决 | 文档单轨治理 |

---

## 12. 保护边界(每轮执行前默念)

- 默认入口保持 2D `L_DesktopTrainingHUD`;未明确要求 3D 时,开发、PIE、截图、验收全部留在 2D 桌面→BattleBoard 链路。
- 不覆盖/回滚/重调:`L_Main.umap`、角色 Sprite、PaperZD、关卡放置、相机、HD2D Plane;工作树中用户的 `L_Main.umap` 与 `scripts/test_battle_camera_framing.py` 修改保持不动、不提交。
- 不用 UnrealBridge / Live Coding / Hot Reload;C++ 验证走冷 UBT / `scripts/ue_tdd_pipeline.py`。
- 提交用精确文件清单,禁 `git add -A` / `git add .`;`SourceAssets/`、`SourceArt/`、历史探针不批量入库。
- 表现类问题先取证、优先委托 lunamax,不自行盲改绘制层公式。
