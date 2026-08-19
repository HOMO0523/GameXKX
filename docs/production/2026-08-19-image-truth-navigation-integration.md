# 2026-08-19 ImageTruth 导航与挂机背景接入证据

## 范围

本次只接入用户逐图确认的 6 张 ImageTruth 图片：挂机连续背景、仓库、编队、天赋、工具、历练。未导入任何未确认 PSD、旧 MasterV2 导航圆底或候选图片。

## 真源与 UE 路径

真源 manifest：`SourceArt/UI/ImageTruth/manifest.json`
校验命令：`python scripts/gamexxk_ui_image_truth_check.py --json`
结果：`ok=true`、`confirmedCount=6`、`manifestCount=6`、`findings=[]`

| ImageTruth 语义 | UE 资产 |
|---|---|
| `training.idle_strip.background.seamless.v003` | `/Game/GameXXK/UI/ImageTruth/Training/T_TrainingIdleStrip_Background.T_TrainingIdleStrip_Background` |
| `training.nav.warehouse.ink.monochrome.v002` | `/Game/GameXXK/UI/ImageTruth/Training/T_TrainingNavWarehouse.T_TrainingNavWarehouse` |
| `training.nav.formation.ink.v002` | `/Game/GameXXK/UI/ImageTruth/Training/T_TrainingNavFormation.T_TrainingNavFormation` |
| `training.nav.talents.ink.knot.v004` | `/Game/GameXXK/UI/ImageTruth/Training/T_TrainingNavTalents.T_TrainingNavTalents` |
| `training.nav.tools.ink.hammer.v005` | `/Game/GameXXK/UI/ImageTruth/Training/T_TrainingNavTools.T_TrainingNavTools` |
| `training.nav.training.ink.v001` | `/Game/GameXXK/UI/ImageTruth/Training/T_TrainingNavTraining.T_TrainingNavTraining` |

MCP 导入脚本：`Content/Python/gamexxk_import_image_truth_nav.py`。脚本逐条拒绝 confirmed 目录之外的路径，并在导入前比较 manifest SHA256；6 条导入结果均返回 `ok=true`，尺寸与 manifest 一致。

## 运行时变更

- `UGameXXKDesktopTrainingWorkbenchWidget` 的底部五导航改用 `/Game/GameXXK/UI/ImageTruth/Training/` 下的 glyph。
- 导航 glyph 放入 `UScaleBox(ScaleToFit)`，保持天赋/工具等非正方形 alpha 边界不被压扁；标签继续由 UE 文本层绘制。
- 挂机连续背景切换到确认真源 UE 资产。
- 旧 `T_MasterV2_NavDisc*` 不再列入工作台 MasterV2 资源合同，也不再作为底部五导航运行时图标。

## 自动化与构建证据

- 冷 UBT：`GameXXKEditor Win64 Development -NoHotReload -NoHotReloadFromIDE -NoUBA -MaxParallelActions=2`，`Result: Succeeded`。
- `GameXXK.DesktopTraining.Workbench.ImageTruthNavigationBinding`：1/1。
- `GameXXK.DesktopTraining.Workbench.MasterV2ResourceContract`：1/1；旧导航圆底计数为 0。
- `GameXXK.DesktopTraining.Workbench.TravelVisualStrip`：1/1；背景路径指向 ImageTruth。
- 完整 `GameXXK.DesktopTraining.Workbench`：19/19，0 failed。

受保护地图 `Content/GameXXK/Maps/L_Main.umap` SHA256 仍为：
`EE6E8394E40298321F2A57CC030018BDD1109EED36248597A7D7F414E387E46B`

## 尚未接入

顶部置顶/音量/邮件/商店/退出、Tab 箭头、历练节点状态、挑战/游历/重试、工具五模式、宝箱和局内战斗专用图标仍处于 `UNVERIFIED_EXISTING` 或 `NEW_IMAGEGEN`，必须逐张用户确认后再导入。
