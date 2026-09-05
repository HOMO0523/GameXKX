# GameXXK 当前设计总表与配队分析

- `GameXXK_卡牌设计总表_2026-09-04.xlsx`：173张卡、419个品质版本、36条分支、1498条逐卡Pill说明。
- `GameXXK_装备设计总表_2026-09-04.xlsx`：新装备预算下的百级最终属性、49个模板、35条词缀、18条套装描述和30种宝石。
- `GameXXK_怪物与阶段数值设计总表_2026-09-04.xlsx`：批准的27关等级/189编制、21怪物、351个难度/阶段意图用例，并单列125级地狱3-1。
- `GameXXK_职业配队与伤害期望分析_2026-09-04.html`：100级新装备角色对125级地狱3-1的透明设计期望模型。

卡牌表保留两项未决：`Profession.Sorcerer.RanLingHuanYuan`倍率与`Npc.JinGui.HouXiangTuoShen`对象语义。装备套装、敌人多阶段、固定编制、5～135关卡等级与显式难度上下文均已进入运行时；配队分析必须使用这套已验证条件重新生成。

重新生成：

- `python scripts/export_game_design_tables.py`
- `python scripts/export_game_enemy_design_table.py`
- `python scripts/export_game_analysis_html.py`
