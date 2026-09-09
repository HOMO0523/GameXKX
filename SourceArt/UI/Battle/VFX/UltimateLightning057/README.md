# 057 雷电大招：透明素材

已按实际图集分格重新切片并移除纸底。56张独立RGBA帧、7张带4像素透明间隔的图集；亮芯保留，透明边缘带隐藏RGB扩边以减轻双线性采样边线。

- `Frames/`：56张640×360独立透明PNG，按源方向排列。
- `Atlases/`：7张2592×736 RGBA图集，每张4列×2行，8帧。
- [sequence.json](sequence.json)：源帧号、每帧时长、图集索引、像素矩形与安全UV。
- [asset-check.json](asset-check.json)：56帧RGBA及图集间隔验证。
- [ue-import.json](ue-import.json)、[ue-verified.json](ue-verified.json)：引擎导入与复核。

## 已导入UE

目录：`/Game/GameXXK/UI/Battle/VFX/UltimateLightning057`

- 七张 `T_UltimateLightning057_Atlas_01` 至 `07`。
- `M_UltimateLightning057`：UI透明材质，使用真实Texture Alpha。
- `MI_UltimateLightning057_Friendly`：我方预置，`FlipX=1`。
- `MI_UltimateLightning057_Canonical`：源方向，`FlipX=0`。

播放时按清单切换 `AtlasTexture`、`U0/V0/U1/V1`。镜像只作用于当前帧内部UV，不能翻转整张图集或倒置帧序。按`duration_seconds`推进，默认源时间基准为预览假定的30FPS；加速应统一缩放各帧时长。开始时重置不透明度，结束时隐藏播放控件。

贴图使用UI纹理组、无Mip、Clamp、Bilinear及保留Alpha压缩设置。素材导入已完成；战斗事件触发与播放调度尚未挂接，本单元未改变战斗数值。
