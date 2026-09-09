# 宝石类型定型稿

当前采用 **v5，14类×1张512×512透明PNG**，用户已经确认并接入UE。包括固定攻防生命、三种属性百分比、直伤/产甲/治疗/反击/火焰/持续/冰霜/雷击。其余9档品质不另出图，名称、道具底、tooltip及镶嵌文本复用装备品质规则。

- `type-icons-v5/`：正式输入，14张PNG。
- `type-icon-manifest-v5.json`：源图/最终图哈希、透明边界、共用百分号及UE导入报告。
- `gem-types-review-v5.png`、`gem-types-48px-review-v5.png`：大图及小尺寸对照。
- `Content/Python/gamexxk_import_gem_type_icons.py`：固定类型纹理导入器，保留旧30张品质图。

只有三个属性百分比图标包含中心%，采用同一182×204像素浅浮雕层和同一坐标，分别为浅红、浅蓝、浅绿。其它11张不加百分号。

图像由内置imagegen生成，背景清理和统一符号合成均经用户授权；v4及以前稿保留作历史参考。

运行时数值和显示验收见 `docs/production/2026-09-09-gem-attributes-progress.md`，导入通过不等于全部战斗验证通过。
