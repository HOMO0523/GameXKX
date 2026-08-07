# 卡组页 PSD 图层命名

## 页面与页签

- 新页面组：`18_主角背包_卡组页`
- 03 未选中卡组页签：`tab_03_deck`
- 18 选中卡组页签：`tab_03_deck_selected`
- 页签显示文字：`卡组`

## 卡组视图切换

- 控件组：`45_DeckSwitchControls`
- 左切换按钮：`deck_switch_left_Button`
- 右切换按钮：`deck_switch_right_Button`
- 中间动态标题：`deck_view_title_Text`
- 卡组背包内容组：`50_DeckBackpackView`
- 角色卡组内容组：`51_CharacterDeckView`

`deck_view_title_Text` 运行时显示 `卡组背包` 或 `角色卡组`。左右按钮只切换这两个内容组；同一时间只显示一个。

## 卡牌共用层

- 共用卡框：`card_frame_base_PSD057`
- 卡面角色图：`card_portrait`
- 卡名：`card_name_Text`
- 费用：`card_cost_Text`
- 状态文字：`card_state_Text`
- 选中墨迹：`card_state_selected_ink`
- 未解锁暗色覆盖：`card_state_locked_overlay`
- 未解锁锁图标：`card_state_locked_icon`

## 职业信息色条

卡框 `card_frame_base_PSD057` 保持原始宣纸色，不整体染色。职业色只放在底部信息条：

| 显示名称 | 图层名 | 色值 |
| --- | --- | --- |
| 主角 | `card_info_strip_hero` | `#F1E4CC` |
| 刀客 | `card_info_strip_blade` | `#B6483F` |
| 护卫 | `card_info_strip_guard` | `#254D4D` |
| 医师 | `card_info_strip_healer` | `#5A936D` |
| 猎手 | `card_info_strip_hunter` | `#9A6833` |
| 术士 | `card_info_strip_sorcerer` | `#40518D` |
| 阵师 | `card_info_strip_formation_master` | `#806279` |
| 任务 NPC | `card_info_strip_quest_npc` | `#252321` |
| 路线/通用 | `card_info_strip_route` | `#E1D3B8` |

## 未解锁状态

- `card_state_locked_overlay`：建议 `#4E4B45`，混合模式“正片叠底”，不透明度 `42%`。
- 整张卡可再降到 `62%` 显示强度，与当前引擎逻辑一致。
- `card_state_locked_icon` 放在卡面中心，下面显示 `未解锁`。
- 未解锁仍保留卡框轮廓，不把卡牌处理成纯灰色矩形。

## 两种卡组内容

- `50_DeckBackpackView`：展示角色拥有的 12 张个人牌，包含已解锁和未解锁状态。
- `51_CharacterDeckView`：展示当前编入牌组；主角固定 8 张，永久伙伴固定 5 张。

