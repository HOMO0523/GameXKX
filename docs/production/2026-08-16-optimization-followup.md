---
status: record
owner: codex
updated_at: 2026-08-16
source_commit: b6763a0d7e8f7a27b3b6c6a8cfb39b9c08f5ac89
---
# GameXXK 优化建议跟进（2026-08-16）

> 接续 2026-08-14 的“阅读项目进程 + 优化建议”任务，按 `AGENTS.md` 硬约束做定向复查。
> 本文档只提建议与记录零风险收尾；不改玩法数值、不碰手调资产、不替代 `current-goal-acceptance.md` 的滚动指针地位。

---

## 1. 方法

- 阅读顺序：`AGENTS.md` → `current-goal-acceptance.md` → `optimization-plan.md` → 未完成项路线图 → 数值台账 → 最近生产循环报告与 Automation index。
- 定向检查：`git log/status/diff --check`、脚本自测（默认核心子集 + `--script-tests all`）、`harness_state_validator`、关键 C++ 文件行数、`.git` 对象仓库体积与未跟踪资产体积。
- 未启动 UE 编辑器、未改任何 `.uasset/.umap`。工作区里的 `Content/GameXXK/Maps/L_Main.umap` 修改为编辑器/用户输出，本轮不纳入、不回滚。

---

## 2. 项目进程快照（2026-08-16）

- 分支：`main`；分析基线 `b6763a0`（`fix: add card subject to Charge/Finish trigger sentences`），本轮收尾提交见 `git log -1`（尚未推送 `origin/main`）。
- 最近一次全量自动化：`Saved/Automation/ChargeFinishSubject/index.json`，**598/598 通过、0 error**（warning 408，均为既有 NullRHI/Atlas/world-context 类，已在最终验收记录分类过）。`HudReconcileFull` 同基线。
- 最近一次冷 UBT：`Saved/HarnessReports/20260816-114544-ai-production-loop.md`，`GameXXKEditor` Development，`Result: Succeeded`，`-NoHotReload`。
- 生产循环默认核心自测：**PASS**（状态校验 + `git diff --check` + 3 个核心脚本测试）。
- `--script-tests all`：**64/86 绿，22 个测试红**（详见 §4 P16）。该模式目前不能作为门禁。
- 自 `a490235`（箭头错位验收）以来又合入 **13 个提交**，集中于战斗 tooltip/卡牌文本/HUD 生命值一致性，全部未经新的 spec/plan/生产记录，只有 HarnessReport 与 Automation index 留痕。

---

## 3. 新问题清单

| # | 问题 | 证据 | 建议 | 风险 |
|---|---|---|---|---|
| P14 | 滚动指针与台账又落后一个 UI 热修簇 | 复查前 `current-goal-acceptance.md` 停在 `a490235`；`optimization-plan.md` 未记录 `8dc5ee6..b6763a0` 的 12 个 UI/文本提交；`2026-08-14-unfinished-inventory-optimization-roadmap.md` A2 的月白/周光祖胜率仍写 33.3%/16.7%，而台账 4.8 已是首领 11/30 与 9/30。**本轮已更新滚动指针与 optimization-plan**；旧路线图 A2 数值待下次改该文件时一并修正 | 每次功能簇收尾后立即更新滚动指针；台账胜率以 `2026-08-12-balance-tuning-ledger.md` 最新节为准 | 零 |
| P15 | 测试工具欠账已实害 | ①`gamexxk_real_play_flow_mcp.py` 仍在 `_vector2_near(pointer, 固定锚点, 1.0)`（L2595/L2680），与 `6668146` 自由鼠标语义冲突；②`scripts/test_hp_hud_updates.py` 仍引用已归档的 `Content/Python/_probe_battle_action.py`，在 MCP 在线时直接报 “Project Python file does not exist” | ①改比较 stage-space 实时指针与 OS 鼠标换算，或校验指针落在目标代理区域；②把测试指向 `Content/Python/_archive/_probe_battle_action.py`（本轮已修），长期应把该探针提升为 `gamexxk_probe_battle_action.py` | 低 |
| P16 | `--script-tests all` 门禁不可用 | 22 个失败可归因：a) 依赖本机 Downloads/Photoshop 路径的外部素材合同测试（town 036/057、terrain art、party deck/psd 等）8 个；b) 生产代码演进后未同步的旧测试（路线牌视觉映射、卡组审计计数、相机 FOV、TownHud ActionBlank、HP MCP 探针路径）5 个；c) harness fake 与生产 API 漂移（`test_ue_pie_lifecycle` 两个 TypeError/AttributeError）；d) GBK 编解码崩溃（`ai_production_loop.py` 打印 summary 与 `test_gamexxk_ui_master_build.py` 子进程解码）；e) 依赖 Pillow/PSD 工具链 API 的环境型失败若干 | 给 `test_*.py` 打标签：`headless`（默认全量）、`asset-contract`（需本机素材时跳过并报 SKIP）、`mcp-live`（仅 UE MCP 在线时跑）；`all` 只跑 headless；修复 GBK 边界 | 低（只动脚本） |
| P17 | 仓库双膨胀：`.git` 复查前 6.5 GB（已清理至 2.2 GB）+ 未跟踪源美术 5.4 GB | `git count-objects -vH`：4.16 GiB 松散对象 + 1.79 GiB pack；`git fsck --unreachable --no-reflogs` 找到 **929 个不可达 blob 共 3.93 GiB**（含两个 1.89/1.76 GB 的 blob），另有 152 个 `tmp_obj_*` 垃圾文件 487.8 MiB。未跟踪：SourceAssets 9401 文件 4.1 GB + SourceArt 775 文件 1.3 GB（9559 张 png 为主）。当前 `.gitignore` 没有 SourceAssets/SourceArt 策略，直接 `git add .` 会再给仓库加 ~5 GB | `git gc --prune=now` **已执行**：`.git` 6.5 GB → 2.2 GB，回收约 4.3 GB，工作区与 `git log` 均不受影响；SourceAssets/SourceArt 走 LFS 或独立资产库 + 只读 manifest（SHA256+路径），禁止无差别提交 | 低（gc）/中（资产策略需用户拍板） |
| P18 | 战斗 UI 巨型文件继续增长 | `GameXXKBattleBoardWidget.cpp` 在 `a490235` 时 8310 行，当前 **8956 行**（+646），头部 1237 行；最近 13 个提交里 10 个直接改它。tooltip、待选奖励面板、目标预览、生命值同步全部挤在同一 widget | 下一轮 UI 特性前先拆 `BattleTooltipPresenter` / `PendingChoicePanel` / `OutcomePreviewLayer` 子部件，或至少把 tooltip 纯函数迁到独立 presenter；否则每次小修都扩大回归面 | 中（需冷 UBT + PIE） |
| P19 | UI 热修簇绕过 spec/plan 流程 | `8dc5ee6..b6763a0` 没有 `docs/superpowers/specs|plans/2026-08-16-*` 对应记录，只有提交与 HarnessReport | 恢复“先 spec/plan → TDD → 生产记录”的最小流程；表现类问题按证据选择复核方式；热修可简化，但收尾必须回填记录 | 零 |
| P20 | 冷 UBT 出现内存击杀抖动 | `20260816-114544` 报告中 UBA 因 `Low on memory (33.6gb/34.7gb)` 杀掉 `GameXXKCardTextTest.cpp` 10 次后重试成功；最终 Succeeded 但有抖动 | 冷编译前关闭编辑器/浏览器等占内存进程；必要时在构建脚本加 `-MaxParallelActions=8`；记录为可观察指标而非失败 | 低（稳定性） |

---

## 4. 建议执行顺序

```text
A. 零风险收尾（本次已做/建议立即）
   A1 更新滚动指针 + 本跟进报告
   A2 完成 Content/Python 探针归档的提交（已执行：`git log -1`）
   A3 修复 test_hp_hud_updates.py 的探针路径（已做）
   A4 git gc --prune=now（已执行：`.git` 6.5 GB → 2.2 GB）
   A5 修 ai_production_loop.py 的 GBK 输出崩溃 + 给 all 模式加测试标签/SKIP

B. 测试工具债（1 个工作日，零 C++ 风险）
   B1 更新 gamexxk_real_play_flow_mcp.py 的 pointer_matches_target 断言
   B2 清理/更新 P16 列出的旧测试；headless 全量恢复绿色
   B3 scripts/README.md 记录 all 模式语义与标签约定

C. 玩法主线（沿用既有顺序，不因本报告插队）
   C1 地形增益重设计：先与用户复核设计文档 §5 山河三档 + TriggerTerrainBenefit 口径
   C2 数值迭代：以台账 4.8 为准继续月白/周光祖单变量审计，再弓手上限与路线通关率建模
   C3 历练桌面迁移 7 包：仍搁置；执行前先修订索引中的 SaveVersion 边界

D. 架构债（逐项评审后单独立项）
   D1 BattleBoardWidget 拆分（P18）→ D2 新旧状态合并 → D3 巨型头文件 → D4 DataAsset 化
   D5 SourceAssets/SourceArt 资产策略（用户拍板后先做 PoC 再全量）
```

---

## 5. 待用户决策（不阻塞 A/B）

1. **源美术资产归属**：SourceAssets/SourceArt 是继续留在本仓库（需要 LFS/分批提交策略），还是移到独立资产库/网盘，仓库只放 hash manifest？
2. **地形增益两个口径**：山河三档细节与 `TriggerTerrainBenefit` 结算口径（设计文档 §5/§2）。
3. **`r.Streaming.PoolSize` 提案 768**：等用户在城镇/PIE 视觉验收后写入 `DefaultEngine.ini`。
4. **历练桌面迁移 7 包**：维持搁置还是重新启动；无论哪种，都应先把索引的存档版本边界改对。

---

## 6. 验证记录

- `python scripts/harness_state_validator.py --json`：`ok=true`，findings 空。
- `python scripts/ai_production_loop.py --run-script-tests --json`（默认核心）：`ok=true`。
- `git diff --check`：通过（不含未跟踪资产）。
- `git log --oneline a490235..HEAD`：13 个提交，逐条审阅无 C++ 规则层越界。
- 本轮未运行 UE 全量 Automation（分析基线已有 12:01 的 598/598 报告，引用即可）。
- 本地 Git 仓库清理：`git gc --prune=now` 退出码 0，`.git` 6.5 GB → 2.2 GB；`git count-objects` 松散对象归零、pack 2.13 GiB。

## 关联记录

- `docs/production/current-goal-acceptance.md`
- `docs/production/optimization-plan.md`
- `docs/production/2026-08-14-unfinished-inventory-optimization-roadmap.md`
- `docs/production/2026-08-12-balance-tuning-ledger.md`
- `docs/production/2026-08-15-battle-target-arrow-alignment-incident.md`
