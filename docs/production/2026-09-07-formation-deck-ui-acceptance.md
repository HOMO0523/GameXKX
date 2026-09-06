# 编队与背包共用卡组交互

状态：实现、冷编译双目标、相关回归与最终实机复核完成。继续当前UI任务、根工作区 `codex/ui-visual-optimization`；未提交。

## 结果

- 编队三名出战角色改成同排大卡，名称、等级、右下卡图及出战标记统一；删除重复长说明。每张卡下有编辑卡组入口，伙伴与NPC下方提供更换按钮。
- 更换时使用与背包一致的整张纸面变暗、单层半透明背景和六张横排卡。候选高亮不写队伍，编入才生效，红色返回在右下；多于六名伙伴时分页。
- 编队内复用 `UGameXXKInventoryWindowWidget` 编辑真实卡组，提供返回编队；背包与编队按角色共享同一份未提交草稿。
- 两个入口的卡组顶部都有2列/4列切换与展开选牌。展开按卡池数量适配，一页显示全部卡；保留名称、气/内、选中墨迹及tooltip。主角36张、伙伴个人卡池、NPC四张分别排版，确认与计数固定在页底。
- 空草稿不会自动填回原卡组；布局、展开/收起、角色入口以及HUD设置/缩放保留草稿。缺牌时禁用确认；未确认不写实际牌组。
- 纸底只使用已有背包纸面，背景内容降低不透明度，透明输入遮挡层不再画第二张纸。卡组层始终保持相同父控件，修复实机“展开后返回，卡牌暂时不重绘”。

## 验证

- RED：`Saved/Automation/FormationDeckRed_20260907`，缺少密度按钮与编队卡组入口两项按预期失败。
- `Saved/Automation/FormationDeckRelease_20260907/index.json`：85/86。新交互、卡组及其余Workbench检查通过；唯一失败仍为之前已有的`Workbench.InnerGeometry`武器槽位置/尺寸旧断言，本轮未改武器槽。
- 最终Editor与Development游戏目标均经冷UBT通过。游戏目标日志保留在`Saved/Codex/FormationDeck-20260907/development-build-final.log`。
- 实机证据目录：`Saved/Codex/FormationDeck-20260907`。`formation-before-confirmed.png`、`backpack-deck-before.png`是修改前截图；`formation-final-50/75/100.png`与`deck-overview-final-100.png`为三档复核。
- `native-selected-seven.png`、`native-return-seven.png`、`formation-deck-final-seven.png`：真实鼠标选成7/8，展开返回后仍7/8，再从编队打开仍7/8。`npc-candidate-final.png`、`npc-cancel-final.png`确认预览周光祖后取消，出战仍为土司首领。
- 最终小卡池优化：`npc-four-cards-release.png`确认四张NPC牌放大并排；`hero-overview-release.png`显示全部36张。`deck-scale-50-release-seven.png`、`deck-scale-75-release-seven.png`确认设置开关及倍率切换后仍是卡组页和7/8草稿。
- `card-tooltip-release.png`为实际鼠标悬停青锋一式的Tooltip窗口实拍，标题江湖体、放大正文、费用/对象/后续效果/手选目标提示完整显示。
- 操作通过项目UE MCP与确切UE窗口进行。输入探针使用当前线程DPI感知并校验屏幕点击点属于UE HWND，避免125% DPI下坐标被二次缩放。
- 不更改人物、PaperZD、地图或其他手调美术资产。数值矩阵不因本单元纯界面变化重复运行；见[数值报告验收](2026-09-07-dev-balance-ui-acceptance.md)。

## 交接

已退出Dev临时会话、结束PIE并经MCP保存（无脏包）。原存档、75%HUD设置、GameUserSettings和默认地图与起始基线全部一致，见同目录`final-baseline-check.json`。后续如改变卡池数量或卡牌宽高，复核整页可见范围、选中墨迹和返回后的绘制；不可重新引入运行时跨父控件搬移卡组层。
