---
status: record
owner: codex
updated_at: 2026-09-03T09:20:00+08:00
source_commit: 8a78a66499cd2c37f048384c060384aa0f7f7987
working_tree: Plan 2 Task 3 restored for review; fresh Hero gate passes; legacy Hero task migration regression is RED
---

# 整体局内优化中断恢复审查

## 结论

现有纯 2D 桌面挂机、路线、BattleBoard、编队与物品系统已经形成可玩的主流程。本轮七阶段改造目前完成 Plan 1，Plan 2 的前两项已提交，第 3 项主角卡牌处于未提交状态。后续怪物阶段、成长、结算和最终平衡认证仍有完整实施工作，不能把主角测试通过视为本轮重平衡完成。

中断没有丢失已提交工作或最后的代码编辑，但审查确认了一个提交前必须修复的兼容缺口：旧存档中的八牌主角术士任务无法被新的四牌任务校验接受。恢复执行必须先关闭这个缺口，再推进后续卡牌家族。

## 中断原因与现场

- 原执行任务：`01a05cf6-2e92-7c93-b241-50d973256c41`；最后执行轮次状态为 `failed`，错误明确为 `You've hit your usage limit`。这是任务记录提供的直接原因，不能仅凭更新时间认定是项目崩溃。
- 当前用量工具查询没有返回额度阻断。没有消耗任何重置信用或购买额度。
- 原用户曾明确要求建立“整体局内优化”分支，并选择在同一任务内按 Plan 1→7 顺序执行。继续使用根目录现有 `codex/overall-in-run-optimization`，不创建 worktree。
- 审查起点 HEAD 为 `8a78a66`，暂存区为空；14 个已跟踪源码/测试修改，3 个原有资源删除，360 个未跟踪条目。未跟踪条目包含目录，不能把这个数当成精确文件数。
- 最后旧报告 `InRun02_Task03_HeroCards_PreFinal` 写于 08:17；`GameXXKCardRules.cpp` 和批准卡表测试分别在 08:18、08:19 继续修改。故旧 116/116 报告早于最终编辑，不能直接用于当前代码验收。
- 当前环境没有 PATH 中的 `python` 命令；应用配置的 Python 为 `C:/Users/shxuw/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe`，用该路径已成功运行项目构建/验证脚本。

## 七阶段实际进度

| 阶段 | 实际状态 | 完成边界 / 剩余范围 |
|---|---|---|
| Plan 1 战斗缩放基础 | 已提交、已有完整阶段门禁 | `4754be7`→`7eb8002`；品质 1/1.2/1.4、等级与难度快照、护甲去上限、DOT 水库、药效、下回合气力惩罚、v33 迁移 |
| Plan 2 173 张卡牌 | 进行中 | Task 1 数值策略 `a2d4c87`、Task 2 退役 25 张路线卡及 v34 迁移 `8a78a66` 已提交；Task 3 的 36 张主角牌等待旧档兼容修复与最终提交；108 张伙伴牌、24 张 NPC 牌、5 张 Boss 牌、显示投影及精确目录认证仍在后续任务中 |
| Plan 3 任务 / 药方 / 地势 / 状态 UI | 尚未阶段验收 | Task 3 已涉及部分主角运行时语义，但不等于完成 Plan 3 的全角色生命周期、状态显示和存档门禁 |
| Plan 4 怪物阶段与 27 关编队 | 待实施 | 21 种敌人、多阶段、351 个意图难度案例、189 个遭遇编队 |
| Plan 5 装备 / 宝石 / 挂机成长 | 待实施 | 装备等级 135、装备贡献和宝石曲线、分关卡掉落等级带 |
| Plan 6 首领结算 | 待实施 | 唯一待领取结算凭据、幂等奖励、恢复和返回桌面 |
| Plan 7 平衡遥测与认证 | 待实施且存在计划口径冲突 | 策略验证、全卡遥测、合法阵容矩阵、Hell 3-3 胜率与回合数认证 |

计划源：`docs/superpowers/plans/2026-09-03-overall-in-run-optimization-plan-suite.md`。七份计划的复选框尚未随提交同步，不能仅靠复选框判断完成情况。

## 本次实际验证

1. 当前未提交代码经过冷 UBT，成功生成新 `UnrealEditor-GameXXK.dll`；没有使用 Live Coding / Hot Reload。
2. 当前主角桶 `GameXXK.Data.HeroCards` 为 **116/116，0 failed、0 warnings、0 errors**。证据：`Saved/Automation/Recovery_InRun02_Task03_HeroCards/index.json`；冷编译及脚本汇总：`Saved/HarnessReports/20260903-085500-ai-production-loop.md`。
3. 新增 `Source/GameXXK/Private/Tests/GameXXKHeroTaskResumeMigrationTest.cpp`，通过真实 `FGameXXKSaveMigration::MigrateToCurrent` 验证旧任务恢复。该测试的普通存档对照组通过，v33 和 v34 的八牌任务均失败，错误均为 `An active protagonist spell task has invalid locked IDs, progress, or starter metadata.`。
4. 加入该测试后的迁移桶为 **2 passed / 1 failed**，两个错误分别对应上述两个源版本；原有 CombatScalingFoundationV33 和 RetiredRouteCardsV34 仍通过。证据：`Saved/Automation/Recovery_InRun02_SaveMigration/index.json`；本轮冷编译与 RED 汇总：`Saved/HarnessReports/20260903-091925-ai-production-loop.md`。本次 RED 是定位到真实兼容缺口，不是编译失败或夹具初始化错误。
5. `git diff --check` 通过。原有生产文档元数据警告继续按历史分类报告。

没有重新宣称全量 GameXXK 自动化全绿，没有运行真实鼠标 PIE 或正式打包，也没有把 NullRHI 测试当作视觉验收。

## 提交前缺口：八牌旧任务迁移

新 `ValidateHeroSpellTaskRuntime` 要求 `LockedHeroCardIds` 等于四张已装备 Mage 卡；此前运行时允许锁定完整八张 Hero 卡。现有 `MigrateHeroCardBattle` 只在旧 Hero 卡池迁移阶段调用，且只处理 ID 映射。v33→v34 只新增路线卡退役，同版本 v34 分支也没有八牌任务转换，因此它们最终均被新校验拒绝。

Plan 3 的 v35 迁移目前还写着“仅初始化轮次字段，保留已有 task/formula arrays”，不能假定它已计划解决此缺口。

恢复任务的要求：

- 先让新增回归转绿，保留源存档不变、已完成的 Mage 进度、战斗 HP、卡区与奖励状态。
- 不得在加载时自动发放任务奖励、治疗或结算；重复加载必须幂等。
- 补核旧任务已完成、正在自动回放、卡牌选择暂停等边界，不能只覆盖一张已完成牌。
- 调整迁移时审查后续 v35→v38 的组合，避免无说明地挤占既定版本号。
- 重新执行受影响的主角、迁移和战斗基础门禁，再将 Task 3 的代码、测试按明确路径一起提交。

## 后续必须校正的计划问题

### 平衡认证阵容与当前产品规则不一致

`GameXXKPartyFormationRules.cpp` 的权威校验要求恰好一个主角、一个永久伙伴、一个固定 NPC。批准规格 §15.3 及 Plan 7 Task 4 却写成“主角职业 × 两个永久伙伴”，以 `6 × C(6,2) = 90` 计算。

这会使模拟覆盖玩家无法组成的队伍，并遗漏固定 NPC 的实际影响。Plan 7 开始前应统一为当前产品可用的合法阵容，保留 NPC 规则；不要为了凑满旧 90 个样本改变玩家编队或绕过合法性校验。按六主角职业 × 六伙伴职业 × 六 NPC 的直接上限是 216，实际矩阵数量仍须由合法性规则输出，模拟预算随之重新计算。

### 工程状态与交付状态分开验收

- 当前目标指针只记录 Plan 1 完成、Plan 2 待开始，落后于提交和工作区；本次恢复条目补上实际断点。
- 默认地图配置确实为 `L_DesktopTrainingHUD`，但 `Config/DefaultGame.ini` 的 `MapsToCook` 仍显式含 `L_Main`、青山 3D Demo 和其他旧地图。3D cook 隔离仍属待设计工作，不能声称已去除包体依赖。
- 商店/事件/奖励 UI、遗物设计、原创路线美术、结算 UX、跨机器 DPI 白边以及纯 2D 教程仍是规格 §18 的独立后续范围。七计划完成不自动关闭这些产品欠账。
- 历史广泛回归有失败和 tooltip 崩溃记录；新阶段的局部绿色结果不能替代最终宽回归和真实 2D 流程证据。

## 恢复交接

优先级为：旧任务迁移 RED→GREEN → Task 3 聚焦复验与提交 → Plan 2 Tasks 4–9 → Plans 3–6 → 修正合法阵容口径后的 Plan 7。继续原用户选择的同一执行任务内顺序实施。

本次新增迁移回归仍随未提交 Task 3 工作区交接。用户原有 3 项素材删除及未跟踪美术/探针保持保护，后续提交必须列出确切源码/文档路径并检查暂存清单。
