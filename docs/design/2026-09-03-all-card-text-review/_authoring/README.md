# 文案生成来源

这些脚本仅生成同目录上一级的审阅文档、JSON和离线HTML，不写运行时卡牌或UI文件。

在项目根目录运行：

```powershell
python docs/design/2026-09-03-all-card-text-review/_authoring/build_card_text_review.py
```

卡牌语义人工逐项对照总规格与定义编写；旧目录只提供稳定ID、名称和元数据。脚本校验173个ID、419个合法品质文本、36条通用阵赏、pill定义覆盖及关键数值。倍率显示按本次用户确认的“基础点数＋增幅倍率”，治疗系数无原生品质除法。
