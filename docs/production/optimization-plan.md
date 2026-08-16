---
status: record
owner: codex
updated_at: 2026-08-14
source_commit: c463e1d7fccaa4de5f7afc913901bc874c0d35e2
---
# GameXXK 项目优化计划

> 本文档是项目自身(工具链、文档、进程、代码健康度)的优化计划,不是玩法功能计划。
> 原则:**按风险分级、可独立回滚、符合 `AGENTS.md` 硬约束**(不用 UnrealBridge / Live Coding / Hot Reload;保护手调资产;C++ 改动需冷 UBT 验证;仓库扫描保持定向)。

---

## 0. 问题清单(来自 2026-08-14 全景审查)

| # | 问题 | 位置 | 风险等级 |
|---|---|---|---|
| P1 | 脚本无索引,约 370 个 Python 文件靠命名约定发现 | `scripts/` + `Content/Python/` | 零(只加文档) |
| P2 | 文档双轨漂移:生产单元 vs 散落报告;AGENTS.md 验收语义与放置迁移设计冲突 | `docs/production/`、`AGENTS.md` | 零 |
| P3 | 一次性脚本堆积(18 个 `_patch_*`/`repro_*`/`grind_*`/`*_part2`/`*_capture`) | `scripts/` | 零 |
| P4 | `ue_mcp_smoke.py` 复刻 `ue_mcp_client.py` 传输逻辑(SSE/JSON-RPC) | `scripts/` | 低(需重跑 smoke) |
| P5 | 生产循环 self-test 只默认跑 3 个,其余 ~80 个 test_* 未纳入门禁 | `scripts/ai_production_loop.py` | 低 |
| P6 | 命名漂移:`battle_ui_assets_check` vs `battle_ui_asset_check`;apply 侧 `assemble/ensure/author/create/build/...` 多前缀 | `scripts/` + `Content/Python/` | 低 |
| P7 | 新旧两代状态/UI 并存,`FGameXXKRuntimeState`/`FGameXXKSaveState` 大量 legacy 重复字段与 Deprecated 入口 | `Source/GameXXK/` | 高(需编译) |
| P8 | 巨型头文件/Widget(`GameXXKCardTypes.h` ~2400 行、`GameXXKBattleBoardWidget.h` ~1200 行) | `Source/GameXXK/` | 高 |
| P9 | 数据硬编码在 .cpp 静态 catalog,不可视化编辑 | `Source/GameXXK/` | 高(架构决策) |
| P10 | `WITH_DEV_AUTOMATION_TESTS` 侵入生产类(`*ForTest` 探针) | `Source/GameXXK/` | 中 |
| P11 | 样板化 slot handler(24 节点按钮、5 手牌槽手写 UFUNCTION) | `Source/GameXXK/UI` | 中 |
| P12 | 脆弱的反射桥接 `OneGameIslandRouteMapBridge` | `Source/GameXXK/MVP` | 中 |
| P13 | 测试盲区:全编辑器内自动化、无整局路线通关率建模、Widget 无像素级验收 | `Source/GameXXK/Private/Tests` | 中(长期) |

---

## 1. Phase 1 —— 零编译风险(本轮执行)

不碰 C++,只动文档/脚本/进程工具。每项独立、可回滚。

### 1.1 脚本索引(修 P1)
- 新建 `scripts/README.md`:核心脚本逐一说明 + 命名约定分组 + 入口导航。
- 新建 `Content/Python/README.md`:探针机制说明 + `gamexxk_probe_*` / `gamexxk_validate_*` / `gamexxk_assemble_*` 分组。
- 验证:`git diff --check`;人工核对无错别路径。

### 1.2 文档指针 + 语义对齐(修 P2)
- 新建 `docs/production/current-goal-acceptance.md` 作为**滚动"当前目标"指针**,内容指向 `2026-08-13-current-goal-final-acceptance.md` 并冻结关键语义(含 v15 follower vs 放置迁移的差异声明)。
- 在 `AGENTS.md` 的 Navigation 表加一行指向该指针,消除"当前验收"硬编码漂移。
- 验证:重跑 `scripts/harness_state_validator.py`(已能校验散落报告)无 error。

### 1.3 归档一次性脚本(修 P3)
- 新建 `scripts/_archive/`,把 18 个一次性脚本移入(不删除,保留可查)。
- 验证:确认无源码/测试 import 这些文件名(grep),`git diff --check`。

### 1.4 去重 smoke 传输层(修 P4)
- `scripts/ue_mcp_smoke.py` 改为 `import ue_mcp_client` 复用 `make_json_rpc`/`read_sse_event`/`iter_sse_payloads`/`parse_json_response`,删除本地重复实现。
- 验证:与主流程不冲突;若 UE MCP 在线则跑 `ue_mcp_smoke.py`。

### 1.5 生产循环 self-test 覆盖面(修 P5)
- `ai_production_loop.py` 增加 `--script-tests all` 模式:discover `scripts/test_*.py` 全量;默认仍跑稳定的核心子集,避免沙箱临时目录写限制导致的误报。
- 验证:`python scripts/ai_production_loop.py --run-script-tests --json` 退出码 0。

---

## 2. Phase 2 —— 低/中风险 C++ 卫生(需冷 UBT 验证,逐项单独提交)

> 每项遵循:先 `ue_tdd_pipeline.py` 或冷 UBT 确认基线 GREEN → 改动 → 冷 UBT + 聚焦自动化 GREEN → mutation RED → 恢复 → 独立提交。

### 2.1 样板化 slot handler 去重(修 P11)
- `UGameXXKOneGameRouteMapWidget::HandleNodeButton0..23` 改为循环/映射生成;`UGameXXKBattleBoardWidget::HandleHandCardSlot0..4` 同理。
- 验证:对应 `*WidgetTest` + 真机 PIE 路线图/手牌流程。

### 2.2 `WITH_DEV_AUTOMATION_TESTS` 收敛(修 P10)
- 把 `*ForTest` 探针从生产类迁到 `#if WITH_DEV_AUTOMATION_TESTS` 更窄作用域或独立测试工具类。
- 验证:全量 Automation + 生产构建(Shipping/Development 无测试符号)。

### 2.3 反射桥接加固(修 P12)
- `OneGameIslandRouteMapBridge` 的 `FIntProperty/FBoolProperty` 反射改为显式契约接口或缓存 + 失败即报错(不静默)。
- 验证:路线图滚动/战斗入口真机流程。

### 2.4 命名漂移收敛(修 P6)
- 统一 `battle_ui_assets_check.py`/`battle_ui_asset_check.py` 为单一入口(保留薄包装向后兼容)。
- apply 侧前缀收敛为一个约定并写入 README。

---

## 3. Phase 3 —— 高风险架构债务(仅规划,需单独评审后执行)

### 3.1 新旧状态合并(修 P7)
- 删除 `FGameXXKRuntimeState` 旧 MVP 镜像字段与 `FGameXXKSaveState` 顶层 legacy 重复字段;清理 Deprecated/"Compatibility-only"入口;必要时 bump SaveVersion 并写迁移。
- 风险:存档兼容、旧蓝图/测试引用;需全量回归 + 真机 PIE + 迁移矩阵。

### 3.2 巨型文件拆分(修 P8)
- `GameXXKCardTypes.h` 按 类型/效果/运行时 拆分;`GameXXKBattleBoardWidget.h` 拆子 widget。
- 风险:编译耦合重,`bUseUnity=false` 下改 include 图需逐 TU 验证。

### 3.3 数据 DataAsset 化(修 P9)
- catalog 从静态 .cpp 迁移到 DataAsset + 只读校验,保留确定性。
- 风险:改变数据工作流,需与"确定性 + 单测"原则平衡,单独立项。

### 3.4 测试盲区补齐(修 P13)
- 增加整局路线通关率模拟(累计掉血/休息/三章连乘);补关键 Widget 的像素级/PIE 验收。

---

## 4. 执行顺序与回滚

- Phase 1 → 本轮直接执行,`git` 单独提交(文档/脚本类,不涉 `.uasset`)。
- Phase 2 → 逐项、每次一个归因簇、冷 UBT + 聚焦自动化验证后独立提交。
- Phase 3 → 每项需用户明确批准后单独立项,不与其他改动混入。

## 5. 本轮执行状态

| 项 | 状态 |
|---|---|
| 1.1 scripts/README.md + Content/Python/README.md | ✅ 已完成(两份索引落地) |
| 1.2 current-goal 指针 + AGENTS.md 对齐 | ✅ 已完成(`docs/production/current-goal-acceptance.md` 落地,`AGENTS.md` Navigation 引用) |
| 1.3 归档一次性脚本 | ✅ 已完成并补提交(20 个已跟踪 `scripts/_archive/`;`Content/Python/_archive/` 21 个探针副本落地,对应 5 个根目录删除;`battle_target_reward_capture.py`/`battle_reward_recapture.py` 成对移入 `scripts/_archive/`) |
| 1.4 ue_mcp_smoke.py 去重 | ✅ 已完成(6 个传输函数改为 import `ue_mcp_client`,import 冒烟通过) |
| 1.5 生产循环 self-test 覆盖 | ✅ 已完成(新增 `--script-tests all` 全量发现模式;另修复 subprocess GBK 解码与 stdout=None 崩溃)。⚠ 2026-08-16 复查:`all` 模式 64/86 绿,仍需按 P16 打测试标签,详见 `2026-08-16-optimization-followup.md` |
| 2.1 样板化 slot handler 去重 | ✅ 已完成(路线图 24 节点 + 手牌 5 槽的 24/5 分支 switch 改为 `FDelegate::BindUFunction` 按名绑定,UFUNCTION 保留) |
| 2.3 反射桥接加固 | ✅ 已完成(1Game 路线图桥:按类缓存属性解析 + 缺失/超时一次性告警,选择语义不变;MVP 桶 77/77) |
| 2.4 命名漂移收敛 | ✅ 已完成(`gamexxk_battle_ui_asset_check.py` 重命名为 `gamexxk_battle_target_art_check.py` 并实测运行;README 更新) |
| 2.2 `WITH_DEV_AUTOMATION_TESTS` 收敛 | ⏸ 评估后暂缓:503 处 `*ForTest` 遍布 30+ 生产头文件,整体迁出属"接口重设计"(friend 测试类/独立 accessor),且需 Shipping 构建回归保障;建议单独立项设计后再执行 |
| Phase 3 | 仅计划(待逐项批准后执行) |

验证证据:生产循环 `--run-script-tests` 5 步全 PASS;冷 UBT GREEN;`GameXXK.MVP` 自动化桶 77/77 通过;`GameXXK.Integration.CardBattle` 桶与未改动基线失败集**完全一致**(2 个失败为奖励分层提交 7e1513a 引入的既有问题,与本重构无关:TargetOutcomePreview.LayoutInvariant、BoardHandCardHoverStyle)。

### 已知遗留(均已处理)

- ~~`GameXXK.Integration.CardBattle.TargetOutcomePreview.LayoutInvariant` 与 `BoardHandCardHoverStyle` 在奖励分层提交(7e1513a)后失败~~ → 已按"简洁 tooltip 新设计"更新测试并收紧空 tooltip 布局写入(`728079f`),CardBattle 桶 27/27 通过。
- ~~地形切换代码引用的 7 张 `T_BattleArena_*_GeneratedV2` 贴图未入库~~ → 已补提交(`2909200`)。


---

## 6. 2026-08-16 复查

- 项目进程已推进到 `b6763a0`;新增 13 个 tooltip/卡牌文本/HP HUD 提交,`GameXXKBattleBoardWidget.cpp` 增至 8956 行。
- 新问题 P14–P20 与全量审查后的分阶段方案见 `docs/production/2026-08-16-full-project-optimization-proposal.md`(旧定向建议见 `2026-08-16-optimization-followup.md`)。
- 立即项:探针归档提交与 `test_hp_hud_updates.py` 指向 `_archive` 已完成;`git gc --prune=now` 已执行(`.git` 6.5 GB → 2.2 GB)。剩余:`gamexxk_real_play_flow_mcp.py` 旧 pointer 断言、`--script-tests all` 标签化。
- SourceAssets/SourceArt 约 5.4 GB 未跟踪资产的归属策略需用户拍板,禁止无差别 `git add`。
