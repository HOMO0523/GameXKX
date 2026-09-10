# 装备全新设计 · 宝石同系列

2026-09-10 用户要求：现用42件装备按当前宝石画风重出，旧版兼容7件不用；随后明确旧设计不好看且有结构问题，只依据名称与主题重新设计。

- 范围：破军、玄甲、青囊、追风、蚀骨、山河各6件，共36件；基础6件。部位依次为武器、头冠、护甲、腰带、鞋履、饰品。
- 参考只使用当前宝石画风，不向生成工具提供旧装备图。初轮继承旧轮廓的稿件在 ../gemstyle-20260910/，已撤回。
- 工具：内置 image_gen，每件独立生成。用户已明确允许本批使用本地脚本去底、清理边缘、统一尺寸。脚本不重画装备，不改非边缘主体配色。
- 交付：icons/ 中512×512透明PNG；raw/ 保留生成原图；prompts/ 保存逐件提示词、参考与来源；review/ 提供全览和64/48/32像素检查图；manifest.json 记录哈希、透明边缘与覆盖检查。
- 重建：从工程根运行 `python scripts/process_equipment_redesign.py --complete`。
- 美术源文件与游戏资产分开记录；本目录生成完成不自动代表已替换UE装备纹理。

## 同轮三抗性宝石空图修复

根因：代码引用 FireResistance、FrostResistance、LightningResistance 的独立图标路径，但这3个Texture2D资产未导入。编辑器探测确认3者均不存在。

已通过UE 5.8 MCP导入原批准PNG，保存为同名Texture2D，使用512×512、BC7、UI组、无Mip、保留Alpha。17类宝石路径均可加载。将UE解码源像素缓冲的SHA-1与原PNG的BGRA像素比较，3张完全相同，含Alpha；不是空白或全透明图。保存脏包后短暂停止并恢复PIE，没有C++修改。

修复入口：Content/Python/gamexxk_repair_resistance_gem_icons.py。取证：Saved/Diagnostics/ResistanceIconRepair-20260910/repair.json；复查：probe.json。

## 最终接入状态

用户已确认本批道具。42张装备与2张材料均已导入原路径并保存，44张UE解码像素和Alpha与成品完全一致，回读验证通过。原uasset备份于 Saved/Diagnostics/EquipmentRedesign-20260910/asset-baseline/。基础护甲按用户要求水平镜像；玄甲护甲领口已返修。
