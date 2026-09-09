# 三系抗性宝石图标

火焰抗性、冰霜抗性、雷击抗性各一张；17种宝石方案中的三类新增项，所有品质共用，不加百分号。源图由内置imagegen生成，参考同项目防御宝石体块及对应元素伤害宝石配色。

- `icons/`：3张512×512透明PNG。
- `resistance-gems-review-v1.png`：大图、48px彩色与灰度对照。
- `GameXXK_三抗性宝石图标_v1.zip`：图标、审阅图和清单。
- `manifest-v1.json`：源路径/提示词/哈希、原RGB保留、透明边界检查。
- `generation-records-v1.json`：生成任务原记录。

背景清理和统一排图沿用用户已授权流程；不改变宝石本体配色，不重画笔触。旧14枚不覆盖。当前为图标审阅阶段，尚未导入UE或改变运行时类型枚举。

维护：`python scripts/prepare_resistance_gem_icons.py`。
