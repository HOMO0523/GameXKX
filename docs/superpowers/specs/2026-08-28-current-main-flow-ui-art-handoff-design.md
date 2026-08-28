# GameXXK 当前主流程 UI 美术交付包设计

日期：2026-08-28  
状态：待用户书面复核  
范围：只读导出与整理，不修改代码、UE 资产或素材像素

## 目标

为美术人员整理当前主流程实际使用的 UI。每个界面形成一个自包含文件夹，包含一张实机状态截图、该界面使用的全部图片与图标、区域到素材的标注指引图，以及可直接用于同名替换的资产清单。

先制作一个样板界面。用户验收样板后，再按相同规范处理其余当前主流程界面。

## 范围

首个样板为默认纯 2D 主流程入口：`/Game/GameXXK/Maps/L_DesktopTrainingHUD` 中的桌面工作台默认展开态。

后续批量阶段只覆盖当前主流程实际可达的 UI，不纳入旧 3D Town、历史版本、未使用资产、候选稿或仅供参考的旧 PSD 页面。主流程范围以当前运行时可达关系和实际绘制引用为准，而不是简单复制 `Content/GameXXK/UI` 整个目录。

## 样板目录结构

```text
01_DesktopTrainingWorkbench_Default/
├─ 01_Runtime_Screenshot/
│  └─ DesktopTrainingWorkbench_Default_Runtime.png
├─ 02_Assets/
│  └─ T_ProjectAssetName.png
├─ 03_Annotation_Guide/
│  └─ Guide_DesktopTrainingWorkbench_Default.png
├─ asset_manifest.csv
└─ README_替换说明.md
```

每个界面目录必须自包含。多界面共用素材允许在不同界面目录中重复一份，以降低美术交接时漏图或跨目录误替换的风险；清单中标记其共用关系。

## 命名与替换规则

- 导出 PNG 的主文件名严格使用 UE 项目内资源名，例如 `T_MasterV2_PanelLarge.png`。
- 不给可替换文件追加 `_export`、`_copy`、`_optimized` 等后缀。
- `asset_manifest.csv` 记录完整 UE 对象路径或包路径，例如 `/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_PanelLarge`。
- 若源图原名与 UE 资源名不同，源图原名只记录在清单的溯源字段中，不改变交付文件名。
- 同名但来自不同 UE 路径的资源不得静默覆盖；必须在清单中标记冲突，并按完整路径拆分子目录。

## 截图规范

- 使用当前运行中的 UE 5.8 编辑器和项目 MCP 流程，从真实 PIE/Standalone 主流程状态取证。
- 默认地图固定为 `/Game/GameXXK/Maps/L_DesktopTrainingHUD`，不以 3D Town 作为截图回退面。
- 截图保留完整客户端 UI，不裁掉窗口内实际可见的边缘控件。
- 截图前排除鼠标悬停、Tooltip、临时调试层和非目标弹窗干扰。
- 记录截图分辨率、采集时间、地图、界面状态和文件哈希。

## 素材收集规则

- 以该实机状态实际绘制或该界面可见控件直接引用的纹理为准。
- 包含背景、面板、按钮底图、图标、页签、槽位、状态装饰和该状态可见的角色/物品图像。
- 程序绘制的纯色块、文字、数值、进度和几何边框不伪造为图片；在清单和指引图中标记为 `CODE_DRAWN`。
- 动画图集若在该界面实机可见，则按项目内纹理资源名导出，并在清单标明图集、帧信息和用途。
- 只复制或导出，不修改原图内容，不重新压缩回写 UE，不改变透明边缘或色彩。

## 标注指引图

- 在实机截图副本上使用高对比编号框和引线标注 UI 区域。
- 每个编号旁显示对应 PNG 文件名；共用素材加 `SHARED`，程序绘制区域加 `CODE_DRAWN`。
- 密集区域允许使用编号索引栏，避免文字遮挡界面本身。
- 标注图只用于说明，不作为替换素材。

## 清单字段

`asset_manifest.csv` 至少包含：

- `region_id`
- `screen_name`
- `asset_filename`
- `ue_asset_path`
- `source_art_path`
- `width`
- `height`
- `color_mode`
- `has_alpha`
- `shared`
- `usage`
- `notes`
- `sha256`

## 替换说明

`README_替换说明.md` 说明：

- 美术仅修改 `02_Assets` 中的同名 PNG。
- 保持文件名、画布尺寸和透明通道，除非清单明确允许调整。
- 同名 PNG 回填前必须按清单确认 UE 路径，避免替换历史或同名资源。
- 本交付阶段不自动导入 UE；美术修改完成后的回填与验证另行执行。

## 验证与验收

样板交付前执行以下检查：

1. 实机截图可打开，尺寸与记录一致。
2. 清单中的每个 PNG 均存在，PNG 文件名与 UE 资源名一致。
3. 每个 PNG 的尺寸、Alpha 信息和 SHA-256 已记录。
4. 标注图上的每个素材编号都能在清单和 `02_Assets` 中找到。
5. 未修改任何现有代码、地图或 `.uasset`。
6. 使用项目要求的 Luna 视觉代理对截图和标注对应关系做复核。
7. 生成压缩包，并验证压缩包可解压、文件计数和哈希清单一致。

## 批量阶段门禁

用户只先验收样板界面。样板未确认前，不开始其他界面的导出与整理。样板确认后，保持同一目录、命名、截图、标注和清单规范批量处理当前主流程其余界面。
