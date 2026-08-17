---
status: record
owner: codex
updated_at: 2026-08-18T04:28:00+08:00
source_commit: fbd7e7f
---
# GameXXK Phase 0 基线证据

本记录把优化 Phase 0 的事实、证据和写入保护边界固定下来。它不把程序化工作台壳冒充为生产 PSD/真实战斗，也不把历史 Automation/UBT 报告冒充为当前提交的新验证。

## Fresh checks

| 检查 | 时间/证据 | 结果 |
|---|---|---|
| `python scripts/harness_state_validator.py --json` | 2026-08-18 当前工作区 | `ok=true`，`findings=[]` |
| `python scripts/ai_production_loop.py --run-script-tests --script-test-tag headless --json` | `Saved/HarnessReports/20260818-011435-ai-production-loop.md` | `ok=true`，headless `13/13`；不启动 UE 编辑器 |
| `git diff --check` | 2026-08-18 当前工作区 | exit 0 |

默认生产循环报告中的 `test_ue_tdd_pipeline.py` 是脚本自测 fake pipeline，不等同于本轮实际冷 UBT。asset-contract 最新独立运行于 `Saved/HarnessReports/20260818-012130-ai-production-loop.md`，66 项中 51 通过、15 个测试文件失败；这些失败涉及本机外部素材、旧 hash/manifest、Pillow API、受保护的 L_Main 资产合同等，不能记作 Phase 0 全绿。mcp-live 未运行。

## Historical checks

- Automation：`Saved/Automation/ChargeFinishSubject/index.json`，生成时间 `2026.08.16-04.01.35`，`succeeded=557`、`succeededWithWarnings=41`、`failed=0`、`notRun=0`，合计 598/598。
- 冷 UBT：`Saved/HarnessReports/20260816-114544-ai-production-loop.md`，`GameXXKEditor Win64 Development`、`-NoHotReload`，结果 `PASS`。
- 全量脚本历史发现：优化跟进记录 `docs/production/2026-08-16-optimization-followup.md` §3 P16 记录为 64/86，22 项失败；本轮已分层，当前 headless 13/13，通过的 asset-contract 51/66，mcp-live 未运行。历史 598/598 与历史冷 UBT 仍只作回归参考。

## Current repository state

- 分支：`main`。
- HEAD：`fbd7e7f feat: add warehouse sorting and backpack unequip`；前置仓库分页/quick-equip 为 `a70b192`，背包/伙伴导航为 `48b7212`，工作台 RuntimeState read model 为 `dfb5230`，宝箱 Inventory bridge 为 `a650527`、seeded Resolver/cooldown 为 `1a17019`、TravelRunner 为 `23aee95`、桥接为 `7881927`。Training 规则、v19 存档、程序化工作台、PlayerController opt-in、真实 CardBattle 桥接、确定性 TravelRunner、seeded reward/cooldown Resolver、最小宝箱 Inventory 镜像、背包角色/伙伴切换、工具/天赋容器基础导航、仓库 20 格分页、visible-slot quick-equip、确定性排序和装备槽卸下回仓已提交。`Content/GameXXK/Maps/L_Main.umap`、未跟踪探针与源美术仍受保护且不在提交内。
- 当前 `CurrentSaveVersion=19`；`DesktopTrainingWorkbenchIntroducedSaveVersion=18`、`TrainingRewardCooldownsIntroducedSaveVersion=19`。旧历练索引按 v16/v17/v18 的边界已经失效，不得复用。
- 最新设计真源：`docs/superpowers/specs/2026-08-17-gamexxk-desktop-training-workbench-design.md`；运行时已有 opt-in 规则/壳和真实 CardBattle 单步桥接，但默认 3D 城镇入口未切换，PSD/游历执行器/完整战斗结算/奖励 RNG/性能和 PIE 验收未完成。
- 当前新增规则/工作台证据：`Saved/HarnessReports/20260818-025938-ai-production-loop.md`（Training 14/14，含 1-1 全 encounter 无箱、非 1-1 游历复用局内概率、普通箱 240 秒/精英与首领高级箱 360 秒冷却实际重置/递减、Inventory bridge 与 v19 round-trip）、`Saved/HarnessReports/20260818-031445-ai-production-loop.md`（最新冷 UBT `-NoHotReload` 成功）、`Saved/HarnessReports/20260818-022528-ai-production-loop.md`（SaveGame 12/12）和 `Saved/HarnessReports/20260818-031507-ai-production-loop.md`（DesktopTraining 1/1，断言分页、quick-equip、排序幂等与装备槽卸下回仓；角色/伙伴与工具/历练导航前置证据见 `20260818-025005-ai-production-loop.md`）；`Saved/HarnessReports/20260818-011435-ai-production-loop.md`（headless 13/13）。完整目标复核见 `docs/production/2026-08-18-desktop-training-goal-review.md`。

## Protection lock

- 保留 `Content/GameXXK/Maps/L_Main.umap` 的用户/编辑器 tracked 修改，不回滚、不格式化、不加入 Phase 0 提交。
- `SourceAssets/`、`SourceArt/` 以及 `Content/Python/`、`scripts/` 中已有未跟踪探针不做无差别 `git add`。
- 不修改 `.uasset`、`.umap`、角色像素图、PaperZD、相机变换、HD2D 平面参数。
- 任何新 PSD/图标生产先进入独立候选目录并登记 manifest/hash；概念图不作为切图源。

## Phase 0 exit criteria

以下条件全部满足后，才能把 Phase 0 标为通过并进入 PSD reuse/derive/new 生产验收：

1. 当前指针和所有旧历练文档明确标出 `superseded`/`shelved` 与 v17 重排要求。
2. 默认生产循环、harness、`git diff --check` 全绿；当前 headless 已绿，但 asset-contract 仍为 51/66，故本项未闭环。
3. `--script-tests all` 只运行 headless；asset-contract 与 mcp-live 单独报告环境状态，不启动 UE 编辑器。
4. JSON 输出在当前 Windows 控制台编码下可解析，外部个人路径由参数/env 控制。
5. Phase 0 提交没有触碰保护锁路径；当前用户 `L_Main.umap` 仍为唯一已知 tracked 修改，未跟踪大资产与探针均未加入本轮提交。

## Runtime scope boundary

- `GameXXKTrainingRules.*` 已提供 27 个稳定关卡 ID、挑战/游历状态、1-1 默认通关、失败策略、确定性 TravelRunner、seeded challenge/travel reward Resolver 和普通/高级 Travel 240/360 秒冷却；非 1-1 游历沿用局内 encounter 概率，普通箱 CD 为 4 分钟、精英/首领高级箱 CD 为 6 分钟；1-1 游历普通/精英/首领都不掉箱，只保留阶段金币/经验；第一章映射已冻结为公鸡/狸猫普通、山羊/黄鼬次级精英、1-1 山羊、1-2 黄鼬、1-3 青角羊王。
- `GameXXKDesktopTrainingWorkbenchWidget.*` 仍是程序化几何合同壳，不是 PSD 生产稿；本轮接入最小 RuntimeState read model（金币、仓库装备实例/等级、背包六槽和物品数量/tooltip），并通过共享背包模型支持主角/永久伙伴选择、仓库 20 格分页、visible-slot quick-equip、确定性排序与装备槽卸下回仓、天赋中栏和工具右栏基础容器；没有 MasterV2 纹理绑定、透明图标 manifest/hash、真实字体校准、真实地图节点美术或局内卡牌演出。
- `StartTrainingChallenge` 已接真实 CardBattle 创建和单步推进；`StartTrainingTravel` 已接走动/自动攻击/掉血/击杀/结算/失败重试的确定性 runtime runner；挑战/游历结算已写入金币/经验与两个 canonical 宝箱 Inventory item，并通过 v19 save round-trip。尚未完成 RouteMap→全路线战斗→胜负→奖励→下一遭遇的 PIE 闭环、Travel Actor/动画、真实/离线计时、最终概率/天赋 read model、FIFO 箱批/容量、真实收菜和完整战斗中断恢复。
- `bEnableDesktopTrainingWorkbench` 默认 `false`，因此 3D 城镇可回退且当前未切换默认入口。
