# Desktop Training UI 图片与素材清单

更新时间：2026-08-19
范围：纯 2D 桌面历练工作台、挂机条、背包/仓库、历练地图、工具页、局内放大画布
素材真源：`SourceArt/UI/ImageTruth/manifest.json`
候选队列：`SourceArt/UI/ImageReviewQueue/`

## 判定规则

- `CONFIRMED_TRUTH`：用户对具体文件明确确认，已登记 SHA256、尺寸、alpha 和 UE 文本层规则，可以进入 PSD/UE。
- `UNVERIFIED_EXISTING`：项目里已经存在或代码正在引用，但用户尚未重新确认；不能因为文件位于旧 `Approved`、`PSD` 或 `RuntimeAssets` 目录就自动视为正确。
- `NEW_IMAGEGEN`：当前没有可靠真源，需用 imagegen 单独生成透明、无文字、1:1 安全框图标，再逐张核对。
- `CODE_TEXT`：不做成图片，由 UE 文本层渲染，支持本地化和状态切换。
- `DERIVED_RUNTIME`：由已确认真源经过等比缩放、裁切或 atlas 打包得到；派生过程必须有输入 hash 和尺寸报告。

## A. 已确认真源（6 张）

| 语义 ID | 文件 | 尺寸/格式 | 用途 |
|---|---|---:|---|
| `training.idle_strip.background.seamless.v003` | `SourceArt/UI/ImageTruth/confirmed/training_idle_strip_background_seamless_v003.png` | 1983×793 RGBA | 顶部挂机条左右连续地面/远景背景 |
| `training.nav.warehouse.ink.monochrome.v002` | `SourceArt/UI/ImageTruth/confirmed/training_nav_warehouse_ink_monochrome_v002.png` | 1254×1254 RGBA | 底部仓库 glyph |
| `training.nav.formation.ink.v002` | `SourceArt/UI/ImageTruth/confirmed/training_nav_formation_ink_v002.png` | 1254×1254 RGBA | 底部编队 glyph，三个基础小人 |
| `training.nav.training.ink.v001` | `SourceArt/UI/ImageTruth/confirmed/training_nav_training_ink_v001.png` | 1254×1254 RGBA | 底部历练 glyph，卷轴/路线/旗帜 |
| `training.nav.talents.ink.knot.v004` | `SourceArt/UI/ImageTruth/confirmed/training_nav_talents_ink_knot_v004.png` | 1305×1205 RGBA | 底部天赋 glyph，聚集的简化水墨凯尔特结纹 |
| `training.nav.tools.ink.hammer.v005` | `SourceArt/UI/ImageTruth/confirmed/training_nav_tools_ink_hammer_v005.png` | 1273×1236 RGBA | 底部工具 glyph，大锤头、高填充率、UE 单独显示“工具” |

所有导航文字（仓库、编队、天赋、工具、历练）必须由 UE 文本层绘制，不能烧进图标。

## B. 工作台壳体与公共控件

| 素材 | 当前代码/来源 | 状态 | 结论 |
|---|---|---|---|
| 全高左仓库纸框 | `PanelLargeTexturePath` → `T_MasterV2_PanelLarge` | `UNVERIFIED_EXISTING` | 需要重新对照实机比例和透明边缘 |
| 中央背包纸框 | `PanelLargeTexturePath` / Inventory `WindowFrameTexturePath` | `UNVERIFIED_EXISTING` | 只能复用尺寸正确版本，不能自动继承旧 PSD |
| 右历练/工具纸框 | `PanelLargeTexturePath` | `UNVERIFIED_EXISTING` | 需要按右栏长比例重新核对 |
| 普通按钮底 | `ButtonNeutralTexturePath` | `UNVERIFIED_EXISTING` | 当前被大量滥用，先停用错误语义 |
| 主按钮底 | `ButtonPrimaryTexturePath` | `UNVERIFIED_EXISTING` | 仅保留正确状态，不能当所有导航按钮 |
| 危险/退出按钮底 | `ButtonDangerTexturePath` | `UNVERIFIED_EXISTING` | 退出/失败重试专用，需重新核对 |
| 关闭图标 | `CloseInkTexturePath` | `UNVERIFIED_EXISTING` | 与设置、退出严格分开 |
| 设置图标 | `SettingsTexturePath` → Town HUD | `UNVERIFIED_EXISTING` | 旧 Town 资产，未确认不得使用 |
| Tab 折叠箭头 | 当前是 UE 文本 ▲/▼ | `NEW_IMAGEGEN` | 生成简洁水墨上下箭头，文字仍由 UE 控制 |
| 选中/悬停/按下/禁用状态 | 当前多由颜色 Tint 模拟 | `UNVERIFIED_EXISTING` | 需要统一控件状态图或明确代码态，不可混用 |
| Tooltip 纸张 | Inventory `TooltipPaperTexturePath` | `UNVERIFIED_EXISTING` | 需与工作台纸框一并复核 |

## C. 顶部挂机条

| 素材 | 状态 | 说明 |
|---|---|---|
| 连续背景 | `CONFIRMED_TRUTH` | 使用 A 中 seamless background；不得加黑色纸板底 |
| 主角 Walk/Idle/Attack/Hit/Death 1K atlas | `DERIVED_RUNTIME` / 需审计 | 当前从 `Content/GameXXK/BattleAnimations/Atlases` 和 Training 1K 资源取；必须保留 1K 尺寸与脚底锚点报告 |
| 伙伴 1K 动作 atlas | `DERIVED_RUNTIME` / 需审计 | 当前编队真实三人需要伙伴动作资源 |
| NPC 1K 动作 atlas | `DERIVED_RUNTIME` / 需审计 | 当前编队真实 NPC 需要 NPC 动作资源 |
| 敌方公鸡/山羊/黄鼬/狸猫/青角羊王 1K atlas | `DERIVED_RUNTIME` / 需审计 | Idle/Attack/Hit/Death；敌人朝向和脚底高度必须统一 |
| 六个敌我站位槽纸框 | `UNVERIFIED_EXISTING` | 当前由 `ItemSlotTexturePath`/程序化边框组成，需重新核对 |
| 我方/敌方 HP track/fill | `UNVERIFIED_EXISTING` | 可参考 `UI/Battle/ResourceBars`，但需确认顶部缩小后可读 |
| 金币/经验/掉箱冷却小图标 | `NEW_IMAGEGEN` 或 `UNVERIFIED_EXISTING` | 只保留真正使用的资源，文字和秒数由 UE 绘制 |

## D. 底部五导航

| 导航 | glyph | 文字 | 状态 |
|---|---|---|---|
| 仓库 | `training.nav.warehouse.ink.monochrome.v002` | `仓库` | `CONFIRMED_TRUTH` + `CODE_TEXT` |
| 编队 | `training.nav.formation.ink.v002` | `编队` | `CONFIRMED_TRUTH` + `CODE_TEXT` |
| 天赋 | `training.nav.talents.ink.knot.v004` | `天赋` | `CONFIRMED_TRUTH` + `CODE_TEXT` |
| 工具 | `training.nav.tools.ink.hammer.v005` | `工具` | `CONFIRMED_TRUTH` + `CODE_TEXT` |
| 历练 | `training.nav.training.ink.v001` | `历练` | `CONFIRMED_TRUTH` + `CODE_TEXT` |

导航圆底、选中圈、按下/禁用态目前仍是 `UNVERIFIED_EXISTING`；不能直接把旧 `NavDisc*` 当作最终按钮底。接入前要确认是沿用空底+真源 glyph，还是另行生成一套水墨底。

## E. 背包与角色页面

| 素材组 | 所需内容 | 状态 |
|---|---|---|
| 角色纸框 | 中央背包纸、左右六装备槽、金币位置、关闭/设置分离 | `UNVERIFIED_EXISTING` |
| 角色页签 | 属性、装备、卡组三个运行页；天赋/称号不放顶栏 | `UNVERIFIED_EXISTING`，文字 `CODE_TEXT` |
| 角色/伙伴/NPC 头像与全身 | 主角、6 职业伙伴、6 NPC；装备和卡组切换 | `DERIVED_RUNTIME` / 需逐类核对 |
| 装备槽 | 武器、头部、衣甲、腰带、鞋、饰品六槽空态/选中/悬停 | `UNVERIFIED_EXISTING` |
| 背包格 | 4×5 可见格、滚动条、选中 ink、空/占用/拖拽态 | `UNVERIFIED_EXISTING` |
| 排序 | 单独排序图标/按钮 | `UNVERIFIED_EXISTING`；筛选五按钮应删除 |
| 卡组 | 卡框、锁定态、选中态、应用按钮 | `UNVERIFIED_EXISTING` / 现有 PartyDeck 资产需重新确认 |
| 道具/装备图标 | 物品、材料、装备、任务道具、卡牌 portrait | `UNVERIFIED_EXISTING`；不自动把旧背包物品图当真源 |

## F. 仓库页面

- 4 列仓库格：空、占用、悬停、拖拽目标、锁定/不可用。
- 多页页签：1、2、3、扩展页；分页上一页/下一页。
- 仓库↔背包转移按钮、排序按钮、容量文本。
- 不显示“小侠客 LV1”身份卡；身份与仓库内容分离。

上述框体、页签、分页箭头、排序图标、转移图标目前均为 `UNVERIFIED_EXISTING`；如果用户确认旧图不对，逐项进入 `NEW_IMAGEGEN`，不能用通用按钮底代替。

## G. 历练地图

- 地图纸张与章节页签：第一章、第二章、第三章。
- 难度切换：普通、困难、地狱。
- 节点状态：锁定、可挑战、已通关、当前游历、普通怪、次级精英、首领。
- 节点内容：1-1 至 3-3，共 27 个逻辑节点；图标可以复用状态模板，数据由代码提供。
- 地图底部：挑战、游历两个按钮；当前游历关卡文本；失败重试仅在游历侧显示。
- Tooltip 图标和内容：精英是哪一只、首领配置、锁定原因、期待新内容。

节点圆底、精英/首领徽记、挑战/游历/重试按钮、难度/章节页签目前全部 `NEW_IMAGEGEN` 或 `UNVERIFIED_EXISTING`，尚无用户确认真源。

## H. 工具页

- 五个模式 glyph：分解/炼金、合成、制作、强化、洗炼/镶嵌预留。
- 3×3 工具输入槽：空、占用、悬停、非法、结果预览。
- 自动填充、确定执行、包含仓库物品开关。
- 结果态：成功、材料不足、未配置配方、不可执行。

工具锤子 glyph 已确认，但它只代表底部“工具”入口；工具页内五个操作 glyph 仍未确认，不能重复使用锤子或通用按钮底充数。

## I. 局内 ChallengeViewport

- 路线节点/当前节点/精英/首领状态图。
- 3 敌 + 3 我战斗站位、统一 HP 条、受击/攻击/死亡表现 atlas。
- 卡牌框、手牌、能量/气力、敌方意图、目标箭头、状态图标。
- 自动战斗开关、暂停/继续、战斗结算、普通箱/高级箱。
- 左仓库与右历练地图只读外壳，不新增交互图标。

战斗卡牌与状态图标已有独立资源链，但不属于本次 UI ImageTruth；接入桌面工作台前需要引用各自 manifest/hash，不能把概念图截图裁入。

## J. 系统与奖励

- 顶部工具条：置顶、静音、邮件、商店、退出；均需独立语义图标，状态由代码切换。
- 退出确认：取消、退出、关闭；不能与背包关闭共用同一语义。
- 普通箱、高级箱、金币、经验、4 分钟/6 分钟冷却提示。
- 失败/重试、奖励结算、离线奖励领取提示。

以上系统小图标和宝箱图标目前没有用户确认真源；文字、数值、秒数、错误提示全部 `CODE_TEXT`。

## 接入前门禁

1. `python scripts/gamexxk_ui_image_truth_check.py --json` 必须 `ok=true`，且所有生产图都在 manifest 中。
2. 新图必须真实 alpha、无文字/数字/水印、1:1 安全框；棋盘格烘焙 RGB 视为失败。
3. 任何 `UNVERIFIED_EXISTING` 图先拿出来与用户核对，不能因为路径叫 `Approved` 就自动接入。
4. 每次晋升记录源文件 hash、目标文件 hash、尺寸、透明通道和用户确认依据。
5. 只有用户确认的图才导入 UE；导入后再记录 `.uasset` 路径和导入报告。
