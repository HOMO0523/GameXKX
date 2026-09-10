# 强化石与洗炼砂 · 宝石同系列

用户于2026-09-10追加要求重新制作强化石和洗炼砂。延续同轮装备新设计，只依据名称与用途设计，不引用旧图造型。

- 强化石：厚实锻造矿石与橙金向上矿纹，表达装备提升。
- 洗炼砂：短宽布袋盛金色细砂，以袋口和砂堆轮廓区别宝石与普通药囊。
- 内置 image_gen 绘制，用户已允许本批脚本去底、边缘清理与尺寸统一。独立512×512透明PNG，保留原图、完整提示词、哈希、审阅图。
- 从项目根执行 `python scripts/process_equipment_redesign.py --materials --complete` 可重建。
- 已获用户确认并导入替换UE原材料纹理。44张整体导入与回读报告：Saved/Diagnostics/EquipmentRedesign-20260910/。

## 最终接入状态

用户已确认本批道具。42张装备与2张材料均已导入原路径并保存，44张UE解码像素和Alpha与成品完全一致，回读验证通过。原uasset备份于 Saved/Diagnostics/EquipmentRedesign-20260910/asset-baseline/。基础护甲按用户要求水平镜像；玄甲护甲领口已返修。
