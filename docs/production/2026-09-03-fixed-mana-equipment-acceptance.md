---
status: record
owner: codex
updated_at: 2026-09-03T20:06:39+08:00
source_commit: 2e0db810df578590758f842ced2a36c72588a58e
working_tree: Fixed Mana prerequisite committed; remaining Sorcerer Formation cards pending
---

# 固定内力与装备排除：阶段验收

提交2e0db81落实用户确认的内力规则。冷UBT与装备、主角牌、伙伴牌、存档、模拟基础共316/316通过，0失败、0警告。

## 完成内容

- 主角基础内力上限固定30，法师伙伴固定34，等级和升星不再抬高这两个基础值。当前内力仍正常扣费和回复。
- 所有职业及NPC均不从装备获得内力：腰带/饰品基础、装备等级、强化、纳息百分比与旧墨砚固定20都不生效。
- 纳息停止生成及洗炼；旧ID和原始装备记录保持可读，结算忽略该词缀，背包、伙伴及商店说明显示已停用。没有替换其他属性或重掷已有装备。
- 装备说明、换装比较、角色属性和旧伙伴属性接口口径一致。
- 明确固定效果保留：玄冰本场每次+4、自身阵赏再+8；主角山泉养气本路线+6，月白玉璧每节点+1。六合8/16及寒序6只回复当前内力。
- 旧大容量重算保留当前资源点数并限制在新上限内。测试中的500/525、有路线+6时重算为36/36，重复重算稳定。
- 战斗额外容量和当前值保存在战斗单位上，永久面板按基础+路线同步。法师34再获本场+12、当前43，保存读取后仍为43/46。
- 旧实例装备先补齐允许缺失的空宝石孔，再校验；v25现有宝石保留，第二孔补空，超额孔仍被拒绝。
- 旧测试补齐合法三人队伍、工具天赋、200格旧档兼容背包和3页仓库前提；游戏天赋及容量限制不变。蚀骨与重放期望按四类DOT、一级向上取整、不消耗储量更新，交易原子性及重放断言保留。
- 候选词缀不足时在读取数组前明确拒绝；正常六套仍支持5条不同词缀，基础套的新游戏生产入口仍是普通品质。

## 验证证据

- Saved/Automation/InRun02_Task06_FixedMana_RED_v2/index.json：裸属性、装备和面板规则先失败，战斗样例的等级解锁前提随后修正。
- Saved/Automation/InRun02_Task06_FixedManaBattle_RED_v2/index.json：真实战斗投影及额外容量保存先失败。
- Saved/Automation/InRun02_Task06_NoEquipmentMana_RED/index.json：全职业装备排除、词缀池及说明断言先失败。
- Saved/Automation/InRun02_Task06_NoEquipmentManaContracts_GREEN/index.json：装备与存档80/80。
- Saved/Automation/InRun02_Task06_FixedManaFinalContracts_GREEN/index.json：综合316/316。
- Saved/HarnessReports/InRun02_Task06_FixedManaFinalContracts_GREEN_build.json：最终冷编译。
- Saved/HarnessReports/20260903-200630-ai-production-loop.md：综合报告。

## 剩余工作

本次只完成Plan 2 Task 6的资源前置。伙伴四冰10%、新标准冰爆及阵赏、法师气力与阵师卡仍待实装；雷走具体倍率尚未确认。其他职业的等级内力成长未在本次法师确认中改动，装备排除适用于所有角色。完整数值文案、真实PIE和胜率验收继续按后续任务执行。
