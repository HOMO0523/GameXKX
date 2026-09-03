---
status: active
owner: codex
updated_at: 2026-09-04T01:53:00+08:00
source_commit: 4d21696bc37d1aa611e3738e6634cb6556c76fe2
working_tree: Twenty-three confirmed NPC cards and all five Boss cards complete; HouXiang target remains pending
---

# 任务NPC与Boss牌进度

提交`4d21696`接入23张已确认任务NPC牌和全部5张Boss牌；后巷脱身保持旧实现，等待对象归属确认。

- 土司护甲改用防御系数；原生稀有寨主号令40%×品质=48%，盟寨誓约原生史诗50%×品质=70%。石门80%、冲锋40%，军令40%，收招20%。
- 耳目密报用全体敌方虚弱1、标记2替代重复意图显示；三张无目标地势牌不再要求选择敌人。
- 月白灼烧使用DOT基础系数与等级/品质；山河残图用40%防御护甲、5内、地势1次，阵赏为100%×品质加每甲1个百分点。
- 周广祖与琼梅儿治疗统一为基础系数15/15/30/25，药效只快照并消耗一次；指定三类净化保留蚀伤，毒爆四类储量均不消耗。
- 市井耳目、藤桥飞渡重箭使用当前牌品质；巧言周旋80%防御护甲。
- 五张Boss牌均保持史诗：196%/252%/196%/196%/322%确认值，护甲读取主角防御，虎魄清四类持续伤害。

RED `InRun02_TaskNpcBoss_Final_RED`为0/2，复现旧NPC/Boss定义。初次GREEN `InRun02_TaskNpcBoss_GREEN`为2/2；完整目录、三牌任务、治疗、毒爆、重箭及Boss奖励门禁最终为`Saved/Automation/InRun02_TaskNpcBoss_Final_GREEN/index.json`，21/21通过、0非预期警告，均经冷UBT。

当前171/173张最新主要规则完成。剩余：雷走八方倍率与后巷脱身对象；两项都已向用户列出具体选项。完整数值卡面/详述和共享地势新值仍按后续任务推进。
