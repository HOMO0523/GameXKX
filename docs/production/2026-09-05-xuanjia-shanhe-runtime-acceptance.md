---
status: record
owner: codex
updated_at: 2026-09-05T03:41:00+08:00
source_commit: 67cc19b
working_tree: Xuanjia/Shanhe runtime complete; card tooltip and monster production validation continue
---

# 玄甲／山河套装运行时验收

玄甲、山河的2／4／6件效果已从旧描述符升级为完整战斗消费者，并与装备表、地势规则和v35存档迁移一致。

## 已实现语义

- 玄甲2：只放大穿戴者新产生的护甲10%；复制、翻倍、返还和保留不会重复放大。
- 玄甲4：我方回合开始保留穿戴者50%护甲，向下取整；明确的完整保留优先。每个敌方回合第一次实际格挡后追加80%攻击伤害。
- 玄甲6：敌方攻击首次令任一友方损失气血后，在整张敌方牌及普通反应结算完毕后触发；全体获得穿戴者40%防御的护甲，穿戴者为其他存活友方各追加1次援护。同名团队效果只生效一份。
- 山河2：穿戴者每回合首次主动打出地势牌并完成原牌选择／重放后抽1张。
- 山河4：同一张首张地势牌预览和支付均少耗1气，原牌完整结算后其他友方各回复2内力；失败出牌、自动地势及自动重放不消耗次数。
- 山河6：每个我方回合先结算普通阵师的当前地势，再由套装穿戴者追加1次；两次分别读取各自防御，阵亡来源不触发。
- 上阵NPC装备已进入同一战斗快照链，属性、个人套装和团队唯一套装均参与结算。

## 地势基础口径

- 平原：全体敌人获得基础系数2的灼烧，按品质100%与队伍最高等级倍率结算；100级为10点。
- 山崖：全体敌人破绽2、标记1。
- 林地：全体友方按治疗系数10结算；100级为50点治疗。
- 水岸／渡口：全体友方回复3内力。
- 村寨：抽1张；全体友方获得触发者20%防御的护甲。
- 洞窟：全体友方获得触发者40%防御的护甲，并各登记1次格挡。

## 存档与文档

- 当前存档版本为v35；v34及更早的活跃战斗只规范化玄甲／山河描述符并清零旧触发计数，不补执行回合事件，也不改破军、青囊、追风、蚀骨的进行中状态。
- 装备总表含18条套装阈值和“地势牌／全队唯一”术语表；玄甲／山河六条均标记为“已批准；消费者已实装”。
- 运行时成功触发写入统一`[EquipmentSet]` Verbose审计，记录效果ID、来源、回合及已结算主／次数据；纯预览不记成功日志。

## 验证证据

- 冷UBT：`Saved/HarnessReports/20260905-033541-ai-production-loop.md`，`GameXXKEditor Win64 Development`通过，使用`-NoHotReload -NoHotReloadFromIDE`。
- 最终聚焦：`Saved/Automation/XuanShan_FinalFocused/index.json`，21/21通过。
- 完整装备组：`Saved/Automation/Equipment_All_GREEN/index.json`，85/85通过。
- 阵师、守卫、敌方意图和装备相邻回归：`Saved/Automation/XuanShan_AdjacentFloor_GREEN/index.json`，108/108通过。
- v35及相邻迁移：`Saved/Automation/SetMigration_AllAdjacent_GREEN/index.json`，15/15通过。
- 描述与运行日志聚焦：`Saved/Automation/SetDocsAndLogs_GREEN/index.json`，12/12通过。
- Excel使用Codex工作区自带Python/openpyxl重建并复读：18条套装阈值、6条批准文本、2条术语均通过断言。

本验收完成装备套装与其直接依赖；卡牌tooltip的实际来源数值、全卡旧测试期望、怪物阶段与关卡等级仍按当前总任务继续处理。
