# 宝石风格指向箭头与向上弧线

基点：`eb6e1c72`，根项目 `main`。按用户截图修正换边后的下弧，并重绘、放大箭头头部。

## 最终行为

- 曲线控制点沿屏幕上方偏移，不再沿左右变化的法线偏移；交换起终点仍是同一条上拱曲线。
- 弧度随距离增长，控制点最大上移 180 个局部单位；短距离限制偏移，防止近乎竖直的指向越过终点再折返。
- 内置 imagegen 参考项目宝石与强化石图标，生成蓝青宝石切面、金边、深色粗轮廓的箭头。绘制框由 74×56 改为等比例 132×132。
- 箭头沿二次曲线末端切线旋转，位置始终为指针减去实际尖端热点；旋转也围绕同一热点。保留 viewport-client/SafeStage 本地坐标链，不使用跨 Geometry 桌面坐标往返。
- 热点从最终源图测得为 (1148.5,625)/1254。原始箭头及十二张水墨点资产保留，水墨轨迹沿新弧线绘制。

## 美术与透明显示

源文件和完整提示词位于 `SourceArt/UI/Battle/Targeting/gem-arrow-20260914/`。

生成器前两次“透明底”实际返回 RGB 棋盘格，未将其冒充 Alpha PNG。最终使用内置工具生成纯品红底版本，保留原始文件字节，由 UI 材质去底并抑制边缘品红溢色。未用脚本改写生成图像像素。

- `T_BattleTargetArrowHead_GemV1_Keyed.png`：1254×1254 源图。
- `T_BattleTargetArrowHead_GemV1.uasset`：512×512 BGRA8 构建资源，实际资源 1 MiB。
- `M_BattleTargetArrowHead_GemV1.uasset`：透明 UI 材质。
- `manifest.json`：SHA256、源尺寸、可见轮廓、热点与去底参数。四角和外围 90 像素边框的计算 Alpha 全为零；实际 PIE 未见背景色块。
- 新资产位于已有 `/Game/GameXXK/UI` AlwaysCook 目录，未改旧贴图或导入玩家其他美术。

## 验证

报告根目录：`Saved/GemTargetArrow/`。

- `Red/index.json`：新 TargetArcUpward 测试在原公式上失败，11 条断言明确覆盖下弧、左右不一致和水平控制点偏移。
- `build-red.log`、`build-green.log`：冷 UBT 成功；未使用 Live Coding / Hot Reload。
- `green-tests.json`：TargetArcUpward、TargetArrowAlignment、TargetPointerViewportCoordinates 3/3，通过且无警告/错误。
- `import.json`：新纹理与 UI 材质导入保存成功。
- `Live/right.png`、`upper-right.png`、`left.png`：同一真实 PIE BattleBoard 的可复现指向截图；探针只临时设置舞台本地指针，不结算卡牌。向右、右上、反向均已目视复核，弧线向上、箭头清晰、透明显示正常。
- `Live/upper-right.json` 与 `left.json` 的 texture_audit 确认源图 1254²、实际构建 512²、资源 1 MiB。
- 现有 43 份玩家存档 SHA256 未变；Dev 会话还原、PIE 停止，MCP 保存脏包 0。

本轮未重跑无关的整套战斗数值回归，也未更新 Shipping 包。上次同范围回归的 17 项既有失败仍按原记录保留。
