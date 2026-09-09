# 057重绘：关键帧校正版

用户已选定B02/057用于雷电任务大招，要求我方施放时左右反转。首版12格概念稿的选帧不准确，保留在`superseded-rough-layout-v1.png`供追溯，未作为成品。

本次逐帧检查原296帧后，按动作节点选择56个定位帧：30个主关键帧与26个过渡帧。重点补回35–40预放电、97–164远近镜头变化、223首次末段触地、265–274第二次冲击。差分仅用于找候选变化位置，白闪并未直接替代造型主帧。

## 预览

- [互动对照页](index.html)
- [我方2倍节奏GIF](057-friendly-preview.gif)
- [我方保留原时间GIF](057-friendly-source-time.gif)
- [源方向保留原时间GIF](057-canonical-source-time.gif)

每张按对应原帧的时间间隔排列，不是等时翻图。原片未标FPS，原时间版本仍按此前30FPS基准：有效动作约9.87秒；2倍试看约4.93秒。另加约0.85秒程序淡出和空场用于循环。GIF解码包含56个重绘图及收尾帧，不把收尾编码帧冒充额外手绘图。

## 素材与方法

- 原始定位帧与阶段说明：[keyframe-plan.json](keyframe-plan.json)
- 内置ImageGen绘制的7张分段图集：[painted-atlases](painted-atlases/)
- 完整提示词：[painting-prompts.json](painting-prompts.json)
- 尺寸、哈希、切片、时间、镜像检查：[painted-manifest.json](painted-manifest.json)

使用内置ImageGen逐段重绘，FFmpeg负责图集切片、等比缩放、时间组装、我方水平镜像及GIF/MP4编码。原向图集保留，我方镜像在切片后的整段播放层执行，未翻转帧顺序，也不翻转UI文字。

当前已按实际分格边界重切，修复均分切片带入相邻帧的条带；并整理56张640×360独立透明RGBA帧、7张带4像素安全间隔的图集。游戏素材位于 `SourceArt/UI/Battle/VFX/UltimateLightning057`，已导入UE纹理与UI材质，我方镜像预置为1。透明检查入口为 [game-ready.html](game-ready.html)。

生成图集原文件保留，未重新绘画。未宣称逐像素复原原片、未宣称已绘完296个源帧；尚未挂接战斗事件触发，没有更改阵赏数值。
