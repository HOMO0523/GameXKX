# GameXXK 当前设计总表与配队分析

- `GameXXK_卡牌设计总表_2026-09-04.xlsx`：173张卡、419个品质版本、36条分支、1498条逐卡Pill说明。
- `GameXXK_装备设计总表_2026-09-04.xlsx`：49个装备模板、35条词缀、18条套装描述、30种宝石。
- `GameXXK_怪物与阶段数值设计总表_2026-09-04.xlsx`：21种怪物、78个意图、90条意图效果与3个首领第二阶段。
- `GameXXK_职业配队与伤害期望分析_2026-09-04.html`：3240场队伍组合模拟与2520场正交模拟的交互分析。

卡牌表保留两项未决：`Profession.Sorcerer.RanLingHuanYuan`倍率与`Npc.JinGui.HouXiangTuoShen`对象语义。装备表将玄甲、山河未完成的战斗消费者显式标为待评审。

重新生成：

- `python scripts/export_game_design_tables.py`
- `python scripts/export_game_enemy_design_table.py`
- `python scripts/export_game_analysis_html.py`
