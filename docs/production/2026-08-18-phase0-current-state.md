---
status: record
owner: codex
updated_at: 2026-08-18T11:14:48+08:00
source_commit: f0e37ac25af7df4880935bda1075b848b530d6f6
design_baseline: ba90810a56e06a3b70ed0e3125c4ef67a59a0685
working_tree: dirty
---
# GameXXK Phase 0 当前状态证据

本记录是 2026-08-18 11:14（Asia/Shanghai）对当前工作区的取证，不替代滚动指针，也不把历史报告冒充为本轮全量验证。

## Git 与基线

| 项目 | 实际值 | 说明 |
|---|---|---|
| 设计/规则冻结基线 | `ba90810a56e06a3b70ed0e3125c4ef67a59a0685` | `docs: freeze desktop training workbench design`；历史 v17 前置边界 |
| 当前仓库 `main` | `628c46a23af9` | `checkpoint: desktop training visual mvp and hud-only map` |
| 活动工作区分支 | `codex/desktop-training-2d-hud-migration` | 未擅自切换到 `main` |
| 活动工作区 HEAD | `f0e37ac25af7df4880935bda1075b848b530d6f6` | `fix: normalize direct hud map startup` |
| 工作区状态 | dirty，239 条 `git status --porcelain=v1 --untracked-files=normal` 记录 | 用户改动与本轮/历史探针混合存在，不能整体收纳 |

保护边界：`Content/GameXXK/Maps/L_Main.umap` 当前为 tracked modified，保持原样；`SourceAssets/`、`SourceArt/`、`Content/Python` 未跟踪探针和生成源文件均未纳入本轮 Phase 0 写入集。当前取证没有执行 reset、checkout、clean、删除或批量移动。

## SaveVersion 事实

当前代码真源为 `Source/GameXXK/Public/MVP/GameXXKSaveMigration.h`：

- `CurrentSaveVersion = 20`；
- `DesktopTrainingWorkbenchIntroducedSaveVersion = 18`；
- `TrainingRewardCooldownsIntroducedSaveVersion = 19`；
- `TrainingOfflineCollectionIntroducedSaveVersion = 20`。

`ba90810` 规格冻结时记录的 v17 是历史前置基线，不是当前运行时版本。旧 v16/v17/v18 历练索引保持 `shelved`，恢复时必须从 v20 后分配新的迁移编号。

## 门禁与运行状态

| 检查 | 日期证据 | 结果 | 当前解释 |
|---|---|---|---|
| `harness_state_validator.py --json` | 2026-08-18 11:14 | `ok=true`，0 finding | 结构化生产单元与 loose report schema 通过 |
| 当前默认 production loop | `Saved/HarnessReports/20260818-111819-ai-production-loop.md` | PASS | 当前 HEAD，包含 validator、diff check 和 focused script tests |
| 当前 headless/all | `20260818-111803`、`20260818-111810` | PASS；all 未启动 UnrealEditor | walkloop NumPy/源美术依赖已归入 asset-contract |
| system-Python asset-contract | `Saved/HarnessReports/20260818-112620-ai-production-loop.md` | 69 文件中 58 通过/内部跳过，11 failures | Pillow 11.3/NumPy 2.5 已可用；剩余为外部源缺失、保护 hash 漂移、旧 manifest/未实现合同，保持 blocker |
| mcp-live | `Saved/HarnessReports/20260818-055050-ai-production-loop.md` | 非绿 | 真实 flow、HP HUD、旧 party-deck catalog 仍有失败 |
| 工作台聚焦冷 UBT | `Saved/HarnessReports/20260818-065557-ai-production-loop.md` | `20/20`，0 failed | 仅覆盖 Workbench Slate/Layout/Training 目标，不等于全量 598 |
| 历史全量 Automation | `Saved/Automation/ChargeFinishSubject/index.json` | `598/598` | 2026-08-16 历史证据，不冒充当前 HEAD |
| 当前可见编辑器 | 2026-08-18 11:14 进程检查 | 0 UnrealEditor、0 dotnet | 交互启动器已落盘，等待用户桌面双击后再做可见 PIE/MCP 复核 |

## Phase 0 判定

当前 Phase 0 **未闭环**：harness 结构门禁、默认/headless/all 已在当前 HEAD 通过，但 asset-contract 仍有 11 个 blocker，mcp-live 未绿；PSD/manifest、1920/2560 视觉证据、四组性能采样和完整挑战/游历 PIE 仍是后续工作包。3D 城镇入口与 `L_Main.umap` 回退基线保持不变。

### 当前 asset-contract blocker 分类

- 外部源未提供：battle terrain manifest、PSD 057/036 等本机 Downloads 路径、部分 PartyDeck PSD cutout。
- 保护资产不一致：`Content/GameXXK/Maps/L_QingshanInn.umap` 观测 SHA 与旧合同期望值不同；不能覆盖或回滚用户文件，只能重新取明确批准的基线后更新合同。
- 现有合同漂移/未实现：UI Master 页面 text layer、PartyDeck portrait/atlas manifest、Qingshan golden-asset contract 等；需独立任务和独立回归，不在本轮启动器/Phase 0 文档修正中伪造通过。
