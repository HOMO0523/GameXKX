# GameXXK UI 图片真源库

本目录是 UI 位图素材的唯一“用户确认真源”。它不是候选图目录，也不继承旧 PSD、概念稿或当前 UE 资产的正确性。

## 硬规则

1. `confirmed/` 只能放用户在对话中按具体文件名明确确认的图片。
2. 生图结果、PSD 导出、截图、临时切图和自动修复图必须先进入 `SourceArt/UI/ImageReviewQueue/`，不得直接写入 `confirmed/`。
3. “布局可用”“方向还行”“作为参考”不等于图片素材确认，不能据此晋升真源。
4. 每张确认图都必须在 `manifest.json` 中记录语义 ID、相对路径、SHA256、像素尺寸、透明通道、确认时间和确认依据。
5. 真源图禁止带文字、数字、水印、概念稿噪点或非等比拉伸；需要文字时由 UE/PSD 独立文本层渲染。
6. UE、PSD 和代码只能从已确认条目派生正式资产。未列入 manifest 的图片一律视为候选或历史材料。

## 晋升流程

1. 把候选图放到 `SourceArt/UI/ImageReviewQueue/<批次>/`。
2. 向用户展示原图或联系表，使用稳定文件名逐张核对。
3. 用户明确回复“确认 `<文件名>`”后，才复制到 `confirmed/`。
4. 记录 manifest 元数据并运行：

```powershell
python scripts/gamexxk_ui_image_truth_check.py --json
```

5. 校验通过后，才能进入 PSD 拆图、UE 导入和运行时绑定。

当前状态：已收入 1 张用户逐图确认素材；没有自动继承任何旧素材。准确数量与哈希以 `manifest.json` 和校验器输出为准。
