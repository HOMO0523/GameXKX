# 历练导航图标：墨绿色路线版

按用户要求保留卷轴地图的造型，增强深色外轮廓，将内部路线、起点与旗帜统一为墨绿色。工具先缩小约 15%，再按后续反馈将圆底与全部图标整体放大约 25%：当前圆底 100×100，普通图标 82×82，工具图标 70×70。整排上移 12 个设计像素，文字仍在圆底下方，点击区域同步覆盖放大后的内容。

- 最终素材：[training_nav_training_ink_green_v002.png](training_nav_training_ink_green_v002.png)，1254×1254 RGBA。
- 生成方式：内置 `image_gen` 编辑原始地图图标；[完整提示词](prompt.txt)。
- `generated-map.png` 为生成原图。后处理仅清理图标内外的棋盘背景与透明像素底色，保留墨线颜色与结构。
- [manifest.json](manifest.json) 记录源文件、哈希、透明边界及导入状态。此次为用户要求的修改；未将新版本标为用户已视觉验收。
- 原已确认图保留于 `SourceArt/UI/ImageTruth/confirmed/training_nav_training_ink_v001.png`。
- 专用导入脚本：`Content/Python/gamexxk_import_training_nav_green_revision.py`。原导航导入脚本通过 `runtimeRevision` 保留此次修改。
- 实机、编译与纹理导出证据：`Saved/Diagnostics/BottomNavDiscs/`。
