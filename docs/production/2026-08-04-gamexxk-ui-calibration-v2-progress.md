---
status: record
owner: codex
updated_at: 2026-08-05
source_commit: 526f63393014847364c7eece5387eeaba71f84cf
---
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

## Master UI V2 迭代里程碑

2026-08-05 已将 `00_公共组件`、`03_主角背包`、`07_商店交易` 三张重点页面切换为批准的 V2 来源，并重建 18 张 `1920 × 1080` Master 预览及联系表。

- 背包视窗扩为 `4 × 5`、共 20 个可见格；总容量显示为 200；竖向滑条位于格子区域右侧。
- 背包只展示强化石、洗炼砂、青山讨伐令三件核心道具，并使用 6 件普通初始装备。
- 新商店固定展示 6 个套装装备包与 1 个伙伴包，价格为 100/500 永久金币；品质文案为普通 70%、稀有 25%、珍稀 5%。
- 旧草药货架、出售入口及旧三槽商店装备没有进入新商店页面。

生成命令：

```powershell
python -m py_compile scripts/gamexxk_ui_master_pages.py scripts/build_gamexxk_ui_master.py
python scripts/build_gamexxk_ui_master.py
```

结果：`ok: true`，18 页，3 张 `v2_master`，背包格 20，右侧滑条 1，新商店产品图层 7；清单资源路径、重点预览尺寸及旧商店文字排除检查全部通过。原分辨率视觉复核确认纸张/墨线风格一致、图标无裁切，背包和商店层级可读。

| Master 输出 | SHA-256 |
|---|---|
| `Previews/00_公共组件.png` | `f80c6e663dc1c2c702680526d0dc8400eb85b52650922f3fc4b6cf6845a51e06` |
| `Previews/03_主角背包.png` | `580d1a886711d278e273436dd39e0b63de1b0a69d81b98858fdd6a7ab8250983` |
| `Previews/07_商店交易.png` | `0898b8e57c7dc8fdc45da4e042335b77c4103f74d87d581a5f0a38e2e27a1719` |
| `GameXXK_UI_Master_ContactSheet.png` | `98cb2e4fca182811d89a589b0869a4b9c3f824ba97cd7da4e235f5eb0cb34f55` |
| `master-manifest.json` | `f00aaa3b9eae19ae46bf74d71f66e5b39623e7b60d4c9ceb7c518de624cbbbaf` |

### Master UI 对齐微调

2026-08-05 后续对齐轮次将 `03_主角背包` 的六个装备槽规范为两列三行：左列 `x=420`、右列 `x=930`，三行 `y=340/515/690`；全部框体保持 `118 × 124`，图标保持 `88 × 88` 和 15 像素内缩。

两张重点页的控件型短文本改为按 Microsoft YaHei 实际字形边界居中：背包页 5 个页签、5 个分类和 4 个属性；商店页 7 个商品名、7 个价格、详情标题与购买按钮。页面标题、说明、概率、永久金币和容量文字继续左对齐。

后生成结构检查结果：装备框 6、装备图标 6、居中短文本 30、背包格 20、右侧滑条 alpha 范围 `(1640, 332, 1663, 931)`、商店商品 7；两张重点预览均为 `1920 × 1080`。原分辨率复核确认两列共线、三行等距、短文本无碰撞，说明文本层级未改变。

| 对齐后输出 | SHA-256 |
|---|---|
| `Previews/03_主角背包.png` | `ea2d5aa285ea895337e1ba6a20a2785d5a127c9ec4db637964bc30bea451d4b8` |
| `Previews/07_商店交易.png` | `a220e6b900bc567f134e026cf505af12b639eda87c60d55867e2c1d8e9f6a911` |
| `GameXXK_UI_Master_ContactSheet.png` | `9204b9cab9c11b3df5f92ca56e76df881807e3b8d72dee601499641f5d14f612` |
| `master-manifest.json` | `23ffd661b94901b5fe9422e3b10ad2b9b7b4cc21f8961d5a87eb74cc93dc1b29` |
