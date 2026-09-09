# GameXXK UI 美术替换说明——桌面工作台默认展开态

只修改 `02_Assets` 中的 PNG。文件名已经使用项目内 UE 资源名，请不要改名。

- 保持原画布宽高和 Alpha 通道。
- 不把文字、数值、数量角标或纯色进度条烘焙进底图；这些内容在清单中标记为 `CODE_DRAWN`。
- 回填时按 `asset_manifest.csv` 的 `ue_asset_path` 确认目标，避免替换同名旧资源。
- `01_Runtime_Screenshot` 是当前实机状态参考。
- `03_Annotation_Guide` 标出了截图区域与同名 PNG 的对应关系。

## 挂机条连续背景

`T_TrainingIdleStrip_Background.png` 是挂机条后方的 2D 连续背景。运行时使用同一张项目纹理横向平铺 3 次，并不是三张不同资源。为方便美术查找，`02_Assets/Continuous_Background` 另放了一份同名副本，`03_Annotation_Guide/Usage_T_TrainingIdleStrip_Background_Repeat3.png` 展示实际重复方式；回填时仍只替换同一个 UE 资源。

本包不自动导入 UE，也没有修改项目代码、地图或 `.uasset`。美术修改完成后的回填与实机复核另行执行。
