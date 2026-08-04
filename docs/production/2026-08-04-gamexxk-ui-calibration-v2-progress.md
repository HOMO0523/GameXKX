# GameXXK UI 校准 V2 进度记录

日期：2026-08-04

## 当前边界

- V2 视觉校准稿已生成，等待用户逐图确认。
- UE 导入：阻塞，等待用户视觉批准。
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
| 完全移除背包格子、页签、面板和 HUD 的纯城镇底图 | `SourceArt/UI/PSD/gamexxk-v4/calibration-v2/Generated/town_background_clean_no_ui.png` | `e1ed6de9caa0c7827e80efbe01aa36ccd5cc14e3a3c5539cadba50c41304b4b5` |
| 组合后的 Hero/Backpack V2 校准预览 | `SourceArt/UI/PSD/gamexxk-v4/calibration-v2/Previews/GameXXK_HeroBackpack_V2.png` | `f6ac2a64e9003444382891bcbaf03e6acc5b98aa0603560de26b67578e0f533e` |

所有生成图均为 `1672 × 941` 的 16:9 源图；校准预览由构建脚本无损适配为 `1920 × 1080`。

## 生成提示词边界

1. UI 空壳版：黑白图作为移除蒙版；只清除白色区域内的入口、资源、装备、物品图标，重建宣纸纹理；保留圆形纸片、资源条、主面板、页签框、装备槽、背包格、分隔线、撕边和城镇环境。
2. 纯城镇底图：移除所有 UI 纸片、圆形入口、HUD、页签、格子、图标、文字和分隔线；连续重建相同机位、光照、屋顶、石板路、树木与建筑。

## 自动验证

运行：

```powershell
python -m unittest scripts.test_gamexxk_ui_calibration_v2 -v
python scripts/build_gamexxk_ui_calibration_v2.py
```

结果：4 个 V2 回归测试通过；构建报告 `ok: true`；画布 `1920 × 1080`；Hero 源画布 `512 × 512`；Hero 横纵缩放比 `1.0`。

自动验证仅证明源锁、尺寸、路径和等比缩放正确，不代表视觉批准。
