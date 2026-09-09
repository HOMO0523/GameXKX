# 底栏粗线与墨黑修订

仓库和编队保留原造型，加粗外轮廓和内部线条；通过内置 `image_gen` 编辑，完整提示词保存在 [prompts.json](prompts.json)。

- [仓库最终图](T_TrainingNavWarehouse.png)
- [编队最终图](T_TrainingNavFormation.png)

两张均为 1254×1254 RGBA。后处理仅清除棋盘背景及透明像素底色，保留不透明墨线。原确认图与替换前的 UE 资产均保留备份。

天赋、工具沿用原图，通过界面线性颜色 `(0.01, 0.01, 0.01, 1)` 将白色高光压成墨黑，不改变原图轮廓和透明孔洞。地图保持墨绿色路线，显示尺寸由 82 增至 88；其他图标 82、工具 70，圆底 100，文字在圆底下方。

专用导入脚本：`Content/Python/gamexxk_import_navigation_bold_revision.py`。源文件、哈希和接入状态见 [manifest.json](manifest.json)。实机和构建证据在 `Saved/Diagnostics/BottomNavDiscs/`。
