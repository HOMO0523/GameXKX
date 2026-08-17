---
status: record
updated_at: 2026-08-17
source_commit: ba90810
---
# GameXXK Phase 0 基线证据

本记录把优化 Phase 0 开始前的事实、证据和写入保护边界固定下来。它不代表 2D 历练运行时已经实现，也不把历史 Automation/UBT 报告冒充为当前提交的新验证。

## Fresh checks

| 检查 | 时间/证据 | 结果 |
|---|---|---|
| `python scripts/harness_state_validator.py --json` | 2026-08-17 当前工作区 | `ok=true`，`findings=[]` |
| `python scripts/ai_production_loop.py --run-script-tests --json` | `Saved/HarnessReports/20260817-223034-ai-production-loop.md` | `ok=true`，默认 3 个脚本测试通过 |
| `git diff --check` | 2026-08-17 当前工作区 | exit 0 |

默认生产循环报告中的 `test_ue_tdd_pipeline.py` 是脚本自测 fake pipeline，不等同于本轮实际冷 UBT；UE 全量结果只引用下方历史证据。

## Historical checks

- Automation：`Saved/Automation/ChargeFinishSubject/index.json`，生成时间 `2026.08.16-04.01.35`，`succeeded=557`、`succeededWithWarnings=41`、`failed=0`、`notRun=0`，合计 598/598。
- 冷 UBT：`Saved/HarnessReports/20260816-114544-ai-production-loop.md`，`GameXXKEditor Win64 Development`、`-NoHotReload`，结果 `PASS`。
- 全量脚本发现：优化跟进记录 `docs/production/2026-08-16-optimization-followup.md` §3 P16 记录为 64/86，22 项失败；这些失败尚未完成 `headless`/`asset-contract`/`mcp-live` 分层，不得作为 Phase 0 通过证据。

## Current repository state

- 分支：`main`。
- HEAD：`ba90810 docs: freeze desktop training workbench design`。
- `ba90810` 只新增桌面历练工作台设计规格；最近运行时代码基线仍为 `e78be7c`。
- 当前 `CurrentSaveVersion=17`；旧历练索引按 v16/v17/v18 的边界已经失效。
- 最新设计真源：`docs/superpowers/specs/2026-08-17-gamexxk-desktop-training-workbench-design.md`；运行时尚未实现，默认 3D 城镇入口未切换。

## Protection lock

- 保留 `Content/GameXXK/Maps/L_Main.umap` 的用户/编辑器 tracked 修改，不回滚、不格式化、不加入 Phase 0 提交。
- `SourceAssets/`、`SourceArt/` 以及 `Content/Python/`、`scripts/` 中已有未跟踪探针不做无差别 `git add`。
- 不修改 `.uasset`、`.umap`、角色像素图、PaperZD、相机变换、HD2D 平面参数。
- 任何新 PSD/图标生产先进入独立候选目录并登记 manifest/hash；概念图不作为切图源。

## Phase 0 exit criteria

以下条件全部满足后，才能进入 PSD reuse/derive/new 审计和 Workbench 运行时计划：

1. 当前指针和所有旧历练文档明确标出 `superseded`/`shelved` 与 v17 重排要求。
2. 默认生产循环、harness、`git diff --check` 全绿。
3. `--script-tests all` 只运行 headless；asset-contract 与 mcp-live 单独报告环境状态，不启动 UE 编辑器。
4. JSON 输出在当前 Windows 控制台编码下可解析，外部个人路径由参数/env 控制。
5. Phase 0 提交没有触碰保护锁路径。
