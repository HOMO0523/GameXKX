---
status: accepted
owner: codex
updated_at: 2026-09-03T23:47:00+08:00
source_commit: f31ca578ccc7b57424f2ec4421b6b7b21634156f
working_tree: Ctrl pill help and title hierarchy implemented; full numeric card-copy integration remains pending
---

# Ctrl卡牌说明与字体层级

用户最终选择Ctrl，替代先前讨论的右键、中键。用户另要求卡名比对象行更大、更粗。

## 本次完成

- 默认悬停显示简述；按一下Ctrl开关本卡Pill说明，松开保持，长按不重复切换。
- 按住Shift临时显示详述，松开回到此前的简述或Pill说明；移开、换牌、窗口失焦后清掉旧的Pill状态。Escape关闭Pill说明。
- 游戏手牌、候选牌、卡牌奖励以及背包、伙伴/NPC牌组、商店使用同一套阅读状态。鼠标右键与中键未增加新的绑定。
- 游戏卡名22号Bold，并用同色1单位描边加重；对象名14号Bold，独立一行；正文13号。预览页同步层级，并在交互预览内补上卡名。
- 对象行只突出名称。仅自身、不可选自身、状态要求、生命区间等限制保留在正文；从友方消耗护甲再攻击全体敌方的牌显示两类对象。
- Ctrl说明从当前品质的卡牌规则收集关键词，去重；已确定的通用法术分支隐藏其他分支的关键词。主角/伙伴/NPC任务分别说明4/5/3种牌。
- 气力、内力不生成卡内Pill说明；蓄力与重箭合并解释；持续伤害共性只补一次。说明正文不再生成一圈额外Pill。
- 中毒采用“任意一方回合结束时，失去等同中毒值的生命。”Shift不再追加通用状态释义，重箭仍保留本牌的消耗时机与具体效果。

运行时代码提交：`f31ca57`。本次没有改变战斗数值、伤害结算、牌堆或存档。

## 验证

- 初始RED：`Saved/Automation/InRun02_TooltipCtrl_RED/index.json`，3/3复现缺少Ctrl模式、独立说明及对象行。
- 对象限制RED：`Saved/Automation/InRun02_TooltipConstraints_RED/index.json`，确认拆出对象行时必须保留限制，并覆盖借用友方护甲的群攻对象。
- 最终冷UBT：`Saved/HarnessReports/InRun02_TooltipCtrl_Final_GREEN_build.json`，成功；没有使用Live Coding或Hot Reload。
- 最终Automation：`Saved/Automation/InRun02_TooltipCtrl_Final_GREEN/index.json`，**13/13通过，0失败、0非预期警告**。覆盖4项共享说明测试、真实手牌悬停控件、7项商店控件回归、中毒等状态说明。
- 手牌悬停测试原来注入已退役的路线牌，现让本用例显式使用现行的青锋一式。该无场景UI用例的4条“无视觉会话，跳过角色绘制”日志按完整内容及固定次数声明为预期；其他警告仍会失败。
- HTML生成检查保持173张牌、419个品质版本、36条通用阵赏。Node检查覆盖Ctrl按下/长按/松开、Shift优先与返回、Escape、离开、失焦，以及右键/中键不切换；生成页脚本语法检查通过。
- 汇总：`Saved/HarnessReports/20260903-234422-ai-production-loop.md`。

## 边界与后续

这是Plan 2 Task 8的交互、对象行、Pill分离及排版部分，不是173张卡最新数值文案的全部接入。

卡牌正文仍从当前运行时定义生成；审阅页的百级试算数字没有写死进游戏。冰爆的Ctrl通用说明本次只说明“消耗全部护甲攻击全体敌方”，具体倍率留在当前牌的阵赏正文，避免把尚待Task 6实装的公式提前显示成生效规则。剩余65张卡的实装、共享数值预览、各条件分支简述以及Task 9全卡认证继续按原计划推进；雷走八方数值方案和后巷脱身对象歧义仍待确认。

本次完成UE控件自动化与预览原型的状态检查，没有执行真实PIE键鼠/视觉验收。此前浏览器工具读取本地file页面受URL策略限制；没有改用其他通道绕过，也没有把脚本检查称为浏览器视觉通过。
