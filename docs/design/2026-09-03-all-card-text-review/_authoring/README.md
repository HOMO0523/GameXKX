# 文案生成来源

这些脚本仅生成同目录上一级的审阅文档、JSON和离线HTML，不写运行时卡牌或UI文件。

在项目根目录运行：

```powershell
python docs/design/2026-09-03-all-card-text-review/_authoring/build_card_text_review.py
node docs/design/2026-09-03-all-card-text-review/_authoring/check_interaction.cjs
```

卡牌语义人工逐项对照总规格与定义编写；旧目录只提供稳定ID、名称和元数据。脚本校验173个ID、419个合法品质文本、36条通用阵赏、pill定义覆盖及关键数值。倍率显示按本次用户确认的“基础点数＋增幅倍率”，治疗系数无原生品质除法。

对象标题独立于正文与pill说明。右键说明按当前卡牌/品质收集；基础资源由资源栏介绍，蓄力与重箭合并解释，持续伤害共性只补一遍；单击右键开关，不因松开鼠标关闭。交互检查执行原型的真实事件处理函数，验证单击、松开、Shift返回及退出状态；它不等于浏览器视觉验证或UE运行验收。
