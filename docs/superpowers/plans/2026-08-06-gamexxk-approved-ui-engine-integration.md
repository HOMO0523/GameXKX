# GameXXK 已确认 UI 引擎接入实施计划

> **执行要求：** 使用 `superpowers:executing-plans` 逐项实施；项目规则要求直接在 `main`，不建 worktree。美术步骤不做 TDD，运行时代码变更必须先红后绿。

**目标：** 把确认的主菜单、城镇 HUD、背包（含选中态）和新商店接入 UE 运行时，并在真实 PIE 流程中验证。

**架构：** Master PSD 保持美术权威；导出透明组件到 `RuntimeApproved`，由 UE Python 导入到独立的 `/Game/GameXXK/UI/MasterV2/Approved`。四个原生 UMG Widget 继续承载动态文本与真实交互，只替换资源和 1920 x 1080 坐标。新商店继续使用既有 Meta Shop 规则，不恢复旧商店。

**技术栈：** Photoshop JSX/COM、PowerShell、Python、UE 5.8 MCP、C++ UMG、Automation Tests、UBT。

---

### 任务 1：修正并锁定背包选中态

**文件：**
- 新建：`scripts/ui_psd_pipeline/build-approved-backpack-selection-ink-jsx.js`
- 新建：`scripts/ui_psd_pipeline/run-approved-backpack-selection-ink.ps1`
- 修改：`outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.psd`
- 新建：`SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/ApprovedEngineIntegration/PSD/*.png`

- [ ] 从 page 07 精确复制 `032_selected_selection_ink` 到 page 13。
- [ ] 将其按 110/170 比例缩放至约 126 x 43，放到首格后方。
- [ ] 隐藏旧 `44_SelectedInventorySlot`，不复制首格物品。
- [ ] 隐藏 `46_SelectedItemRuntimeText` 中的小布袋/说明/分解/使用占位文案。
- [ ] 导出 page 03/page 13 并验证尺寸、层级、哈希和基础态差异仅限选中/详情/动作。

### 任务 2：导出批准组件与生成清单

**文件：**
- 新建：`scripts/ui_psd_pipeline/build-approved-runtime-assets-jsx.js`
- 新建：`scripts/ui_psd_pipeline/run-approved-runtime-assets.ps1`
- 新建：`SourceArt/UI/PSD/gamexxk-v4/ui-master/RuntimeApproved/*`
- 新建：`SourceArt/UI/PSD/gamexxk-v4/ui-master/approved-runtime-assets-manifest.json`

- [ ] 从确认层组导出主菜单、共用壳体、背包、商店组件，文字不烘焙。
- [ ] 对每个 PNG 检查尺寸、Alpha 边界和非空像素。
- [ ] 清单记录源层、用途、目标 UE 路径和 SHA256。

### 任务 3：先建立运行时失败契约

**文件：**
- 修改：`Source/GameXXK/Private/Tests/GameXXKMainMenuPlayerFlowTest.cpp`
- 修改：`Source/GameXXK/Private/Tests/GameXXKPlayerFlowWidgetTest.cpp`
- 修改：`Source/GameXXK/Private/Tests/GameXXKInventoryEnhancementTest.cpp`
- 修改：`Source/GameXXK/Private/Tests/GameXXKMetaShopWidgetTest.cpp`

- [ ] 断言主菜单使用 Approved 背景/按钮且控件坐标匹配 Master。
- [ ] 断言城镇 HUD 使用 Approved 身份/短元宝条/五导航资源，且不自动打开商店。
- [ ] 断言背包为 4 列 x 5 行、带右滚动条，并使用 Approved 选择墨迹而不是旧状态图。
- [ ] 断言新商店使用 Approved 纸框/选择墨迹/元宝资源，不创建旧库存网格。
- [ ] 运行目标测试并确认因新契约尚未实现而失败。

### 任务 4：导入资源并最小修改四个 Widget

**文件：**
- 新建：`Content/Python/gamexxk_import_approved_master_ui.py`
- 修改：`Source/GameXXK/Private/UI/GameXXKMainMenuWidget.cpp`
- 修改：`Source/GameXXK/Private/UI/GameXXKTownHudWidget.cpp`
- 修改：`Source/GameXXK/Private/UI/GameXXKInventoryWindowWidget.cpp`
- 修改：`Source/GameXXK/Public/UI/GameXXKInventoryWindowWidget.h`
- 修改：`Source/GameXXK/Private/UI/GameXXKMetaShopWidget.cpp`
- 修改：`Source/GameXXK/Public/UI/GameXXKMetaShopWidget.h`

- [ ] 通过 UE MCP 导入并保存 `/Game/GameXXK/UI/MasterV2/Approved`。
- [ ] 主菜单切换到确认背景和 Master 坐标，保持四个按钮行为。
- [ ] 城镇 HUD 切换到共用壳体和短元宝条，保持动态身份/导航。
- [ ] 背包改为 Master 排版、4 列 x 5 行和右侧滚动条；选中墨迹在槽位底图后显示。
- [ ] 新商店改为 Master 排版，商品选中与背包共用墨迹，所有局外价格使用元宝。
- [ ] 保持旧商店不可达、城镇进场不自动开商店。

### 任务 5：编译与 PIE 验收

**文件：**
- 新建：`Content/Python/gamexxk_probe_approved_master_ui.py`
- 新建：`Saved/GameXXK/ApprovedMasterUI/*`（运行产物）

- [ ] 运行目标 Automation tests，确认全部转绿。
- [ ] 通过 UBT 或 `scripts/ue_tdd_pipeline.py` 完成冷编译。
- [ ] 启动编辑器并通过 UE MCP 保存脏包、运行 PIE。
- [ ] 验证主菜单进入城镇；城镇只显示 HUD、不自动开商店。
- [ ] 打开背包，滚动并选择首件物品，确认墨迹在格子后且无重复图标。
- [ ] 打开新商店，切换商品、发起/取消购买，确认元宝显示和七个商品功能。
- [ ] 保存五个批准界面与关键状态截图，并生成探针 JSON。

### 任务 6：最终证据与交付

**文件：**
- 修改：`docs/superpowers/plans/2026-08-06-gamexxk-approved-ui-engine-integration.md`

- [ ] 重新运行资源验证、测试、编译和 PIE 探针。
- [ ] 记录 PSD、PNG、UE 资源、测试和截图路径。
- [ ] 逐项对照设计文档后再宣布完成。
