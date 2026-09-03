---
status: accepted
owner: codex
updated_at: 2026-09-04T00:22:00+08:00
source_commit: 780275dba6e041d6950a1e4e040c82102fe31e67
working_tree: Tooltip card names use white blue purple; Sorcerer rules are being implemented separately
---

# 卡名按稀有度着色

用户确认用白、蓝、紫表达卡牌稀有度。悬停卡名现按当前展示品质使用普通白、稀有蓝、史诗紫；保留22号加重字与深色描边，对象行14号加粗、正文13号。商店强化预览用正在展示的强化后品质，牌组和战斗悬停用各自传入的当前品质。非卡牌奖励标题仍使用原有文字颜色。

审阅页的卡名和每个品质的交互预览同步颜色。筛选单一品质时标题跟随该品质；同时展示所有品质时，总标题使用原生品质，各交互预览分别使用自己的品质。

提交：`780275d`。冷UBT与5/5说明/手牌悬停回归通过，0非预期警告：`Saved/Automation/InRun02_TooltipRarity_GREEN/index.json`；构建记录：`Saved/HarnessReports/InRun02_TooltipRarity_GREEN_build.json`。HTML生成检查、Ctrl/Shift原型状态检查与脚本语法检查通过。没有将这些检查称为实机视觉验收。

该改动不改变卡牌数值或全卡实装进度；法师重平衡与数值文案继续按Plan 2推进。
