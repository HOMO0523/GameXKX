# 底栏轮廓二次加强与工具炭灰回调

按最新反馈继续加粗仓库、历练的轮廓。历练保留墨绿色路线、起点和旗帜；工具从过深的墨黑回调至炭灰。

- [仓库最终图](T_TrainingNavWarehouse.png)
- [历练最终图](T_TrainingNavTraining.png)
- 内置 `image_gen` 编辑；[完整提示词](prompts.json)。两张源图均为 1254×1254 RGBA，后处理仅清理棋盘背景和透明像素底色。

当前显示尺寸：圆底 100，仓库／编队／天赋 82，工具 70，历练 88。工具线性颜色乘数从 `0.01` 调回 `0.10`，中间调接近其他图标的炭灰，亮部保留为暗灰；天赋仍为 `0.01`。名称独立放在圆底下方，底栏位于 Y=788，点击区域为 151×136。

专用导入脚本：`Content/Python/gamexxk_import_navigation_contour_v2.py`。旧图和替换前的 UE 包均保留。源文件哈希、导入状态及验证记录见 [manifest.json](manifest.json)。
