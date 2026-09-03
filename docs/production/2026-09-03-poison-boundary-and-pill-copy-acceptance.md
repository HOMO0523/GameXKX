---
status: accepted
owner: codex
updated_at: 2026-09-03T23:15:00+08:00
source_commit: 77250368873c92f56c6a71b0eedda754689f6e28
working_tree: Poison timing implemented; simplified pill copy prepared for review
---

# 中毒回合末结算与Pill信息整理

用户确认中毒说明：“任意一方回合结束时，失去等同中毒值的生命。”其余Pill按信息分工方案整理。

## 中毒实现

提交`7725036`包含中毒结算、对应状态说明和测试，共4个文件。

- 玩家回合末和敌方回合末均对双方存活的中毒单位结算一次，完整一轮通常触发两次。
- 每次直接损失当前中毒值对应的生命，不再乘品质、等级、攻击、防御，也不消耗中毒数值。
- 虚弱仍只在所属方回合末减层；护甲仍按所属方回合开始的规则处理。
- 同一回合末的毒伤全部执行后，再判断胜负和阶段转换；保持原有同时消灭敌人时的胜利优先规则。
- 每笔实际毒伤只向符合条件的药方报告一次。
- 游戏中毒状态说明已同步为用户确认的句子。

验证：冷UBT通过。新增双方触发、两边致死、药方事件及状态说明检查先RED，4/4复现旧行为；修正后状态、缩放、伙伴牌、主角牌、存档、基础模拟和状态说明共**252/252通过，0失败、0警告**。

证据：`Saved/Automation/InRun02_PoisonBothPhaseEnds_RED/index.json`、`Saved/Automation/InRun02_PoisonBothPhaseEnds_GREEN/index.json`；冷编译记录`Saved/HarnessReports/InRun02_PoisonBothPhaseEnds_GREEN_build.json`，综合报告`Saved/HarnessReports/20260903-230639-ai-production-loop.md`。未使用Live Coding或Hot Reload。

## 文案整理

- 气力、内力不再作为卡牌效果pill，基础介绍放资源栏。
- 同时出现的蓄力与重箭合并成一条释义，保留两个来源名称；持续伤害共性只在相关卡下说明一次。
- 检索、编序、阵赏、药方等保留核心含义和影响决策的限制，去除重复条件和执行细节。
- 四张通用法师的自动入手能力补进简述与详述；普通重箭的详述标明使用打出前的蓄力，NPC保留先获得再消耗的顺序。
- 满手等条件放在临场提示中；文案不再以统一字数上限作为验收目标。

文案覆盖仍为173张卡、419个合法品质版本与36条通用阵赏品质分支。生成器检查资源词条排除、合并词条来源、持续伤害共性、自动入手能力及重箭顺序；右键单击开关、松开保持、Shift恢复等原型状态检查与脚本语法检查通过。

入口：[完整文本表](../design/2026-09-03-all-card-text-review/README.md)、[逐卡Pill说明](../design/2026-09-03-all-card-text-review/11-card-pill-descriptions.md)。本轮除中毒的明确修订外，未将整套审阅文案或三模式交互接入正式游戏UI；剩余卡牌重平衡仍沿用主线计划。
