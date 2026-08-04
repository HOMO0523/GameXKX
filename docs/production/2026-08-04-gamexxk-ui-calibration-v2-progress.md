# GameXXK UI 校准 V2 进度记录

日期：2026-08-05

## 当前边界

- V2 已切换到用户批准的新装备与核心道具，源包可进入 UE 导入阶段。
- 批准范围固定为：六套装备 36 件、普通初始装备 6 件、核心道具 3 件。
- 背包核心道具只保留强化石、洗炼砂、青山讨伐令；旧 7 件道具表与兼容表不进入 V2。
- UE 导入与 WBP 接线尚未执行。
- 旧 V1 程序化线框稿继续保留为失败回归样本，不得晋升。
- 本轮没有修改 WBP、地图、相机或运行时 UI。

## 锁定输入

- 批准参考：`SourceArt/UI/PSD/gamexxk-v4/ui-master/Reference/approved_town_hero_backpack.png`
- 最终 Hero Idle：`SourceAssets/AnimationProcessing/Production/character_00_hero_idle/frames/frame_0000.png`
- Hero 使用完整 `512 × 512` 透明画布等比缩放，横纵缩放比为 `1.0`。

## V2 生成资产

| 用途 | 文件 | SHA-256 |
|---|---|---|
| 带彩色物件、无人物无动态文字的高保真校准底图 | `SourceArt/UI/PSD/gamexxk-v4/calibration-v2/Generated/hero_backpack_textless_base_clean.png` | `8f3dbc61c01e83ec5f1584139ccd5b0aa0447492d396e5618b7c4acbcb642116` |
| 保留 UI 结构、移除标记区域图标的空壳底图 | `SourceArt/UI/PSD/gamexxk-v4/calibration-v2/Generated/hero_backpack_ui_shell_no_icons.png` | `3a627ddcf3a023548b00af3329f89def84aad71bc3f48335d8a734dd8405abdc` |
| 大宣纸内部完全清空、仅保留外轮廓的纯面板底图 | `SourceArt/UI/PSD/gamexxk-v4/calibration-v2/Generated/hero_backpack_large_panel_clean.png` | `b767119eb28d1d1ad63843f289ee2b0e0b3ce59c909896d7f9c58d0b18f611b6` |
| 完全移除背包格子、页签、面板和 HUD 的纯城镇底图 | `SourceArt/UI/PSD/gamexxk-v4/calibration-v2/Generated/town_background_clean_no_ui.png` | `e1ed6de9caa0c7827e80efbe01aa36ccd5cc14e3a3c5539cadba50c41304b4b5` |
| 组合后的 Hero/Backpack V2 校准预览 | `SourceArt/UI/PSD/gamexxk-v4/calibration-v2/Previews/GameXXK_HeroBackpack_V2.png` | `a8bd3adcbf8c749194bb3916f27e4100de1e392bc216882be70c24afb11a8477` |
| 28 个独立控件拆层总览 | `SourceArt/UI/PSD/gamexxk-v4/calibration-v2/Review/GameXXK_HeroBackpack_V2_components.png` | `f80c6e663dc1c2c702680526d0dc8400eb85b52650922f3fc4b6cf6845a51e06` |
| 45 个批准装备与道具总览 | `SourceArt/UI/PSD/gamexxk-v4/calibration-v2/Review/GameXXK_UI_V2_ApprovedContent_45.png` | `d88d870eeac20da952d2655c6b3484ac117fec9afbf7cf8ff460fc9841df9ec9` |

`Components/` 下保存 28 个带透明边缘的独立 PNG：5 个页签框、6 个装备槽、16 个背包格、1 个详情物品槽。它们不再烘焙进大宣纸底图。

`Content/Equipment/` 保存 36 件套装装备，`Content/StarterEquipment/` 保存 6 件普通初始装备，`Content/Items/` 只保存 3 件核心道具。45 张成品均为 `512 × 512` 透明 PNG，边缘透明且无可见洋红底色。校准预览为 `1920 × 1080`。

## 生成提示词边界

1. UI 空壳版：黑白图作为移除蒙版；只清除白色区域内的入口、资源、装备、物品图标，重建宣纸纹理；保留圆形纸片、资源条、主面板、页签框、装备槽、背包格、分隔线、撕边和城镇环境。
2. 纯城镇底图：移除所有 UI 纸片、圆形入口、HUD、页签、格子、图标、文字和分隔线；连续重建相同机位、光照、屋顶、石板路、树木与建筑。
3. 纯大面板底图：只清除大宣纸内部的页签框、装备槽、背包格、详情槽、标题色块与分隔线；保留城镇、HUD 空纸片、左侧圆底和大宣纸外轮廓。
4. 独立控件：从高保真 UI 空壳按原始像素区域逐个裁切，并给控件框外区域加透明通道；构建脚本同时生成 28 格总览供视觉核对。

## 自动验证

运行：

```powershell
python -m unittest scripts.test_gamexxk_ui_calibration_v2 -v
python scripts/build_gamexxk_ui_calibration_v2.py
```

结果：12/12 项成品后检查通过；构建报告 `ok: true`；批准内容数 `45`（36/6/3）；画布 `1920 × 1080`；Hero 源画布 `512 × 512`；Hero 横纵缩放比 `1.0`；独立控件数 `28`。正式预览与 45 件总览已完成视觉复核。

纯美术任务按用户要求不采用 TDD；本轮检查均在资产生成后执行。运行时逻辑与 UE 接线仍需后续在编辑器中验证。
