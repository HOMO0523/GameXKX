# PSD 权威城镇 UI 迁移设计

**状态：** 用户已确认方案 A 与首批范围；本文件替代此前“现有切图优先”的城镇视觉假设。

## 目标

把 GameXXK 的首批城镇玩家界面迁移到用户指定的“江湖行”分层 PSD 的真实版式与切图体系。成品必须保留现有可用的游戏状态、输入、存档和窗口行为；视觉组件不能用整张截图或单一通用按钮代替。

## 已锁定的首批范围

| 范围 | 首批处理方式 |
| --- | --- |
| 常驻城镇 HUD | 按 PSD 的头像区、顶部资源条、左侧导航和江湖入口的设计坐标重新排布。人物、数值和资源数量维持运行时数据。 |
| 角色页 | 使用 PSD 的角色页框体、属性块、装备槽、页签和详情按钮；属性与装备仍由现有状态来源显示。 |
| 伙伴页 | 使用 PSD 的伙伴分类栏、伙伴卡格、筛选与详情区；保留现有 12 人上限、随机个人 12 张卡组、属性和选中伙伴逻辑。 |
| 任务列表 | 使用 PSD 的任务标签、任务行、奖励图标、追踪与前往按钮；任务状态仍走既有任务系统。 |
| 背包 | 使用 PSD 的背包框、五类页签、格子、排序和分解按钮；物品、数量、筛选、排序和分解仍走既有背包逻辑。 |

## 明确不在首批修改的页面

主菜单、任务 NPC 对话、世界/路线地图、路线事件、战斗、奖励三选一、牌组编辑和全局 Tooltip 不用城镇素材硬套。它们在下一批各自获得“源图生成 → 透明化 → 裁切 → manifest → 新分层 PSD”的独立资产家族。

战斗状态图标已被用户确认正确并锁定；这一迁移不会替换、重色、重绘或更改其层数 Tooltip 行为。

## 权威参考与可移植工作流

外部参考目录仅作为只读来源：

```text
C:\Users\shxuw\Downloads\nw-studio-nwueball-https-github-com\nw-studio-nwueball-https-github-com
```

当前权威流程是 v2，而不是早期的 `prepare-assets.js` / `split_assets.py` 流程：

```text
generated_v2/*.png
  -> rebuild_clean_assets.py
  -> clean_assets_v2/*.png + manifest.json
  -> build-psd.js
  -> compose.jsx
  -> run-photoshop.ps1
  -> output PSD + validation.json
```

v2 的 `manifest.json` 采用 1024 逻辑画布、4 倍输出为 4096×4096；该参考已验证为 133 个图像层与 130 个可编辑文字层，文字层经 Photoshop 重开回读校验。GameXXK 必须复制并重定向该流程到项目内可写目录，绝不向 Downloads 目录写入，也不保留脚本中的历史绝对路径。

## 项目内目录契约

```text
SourceArt/UI/PSD/
  town-v2/
    generated/                 # 已审批的图集/缺失屏幕生成源，禁止动态文字
    clean_assets/              # 一组件一 PNG，透明边缘已验证
    manifest.json              # 图像层 + 可编辑文字层 + 逻辑坐标
    semantic-map.json          # 组件语义、UE 目标纹理名、九宫格信息
    previews/                  # 无文字审阅图与合成预览
  generated-screens/           # 后续非城镇屏幕的独立输入批次
scripts/ui_psd_pipeline/
  rebuild_clean_assets.py
  build-psd.js
  run-photoshop.ps1
outputs/UI_PSD/
  GameXXK_Town_4K.psd
  GameXXK_Town_4K.validation.json
```

所有可交付 PSD 放在项目内 `outputs/UI_PSD/`，这样脚本、manifest、原始图与交付物有稳定的相对关系。运行时 UE 纹理从已验证的 `clean_assets/` 导入；PSD 仅作为美术可编辑交付和审阅基线，运行时不加载整张 PSD。

## 素材分类与文字规则

### 直接 PSD 素材

城镇 HUD、角色、伙伴、任务和背包的框体、插画、图标、卡槽、页签和按钮以 v2 的 `clean_assets_v2` 为唯一视觉来源。现有 `docs/ui/town/source_art` 中的替代件不得在这些位置覆盖 PSD 切图。

### PSD 派生素材

仅当原始切图没有特定状态时，才能从同一 PSD 的无文字框体制作 hover、pressed、disabled、selected 或九宫格画刷。派生件必须保留原框体比例、纸张纹理和低饱和色阶，并在 `semantic-map.json` 中关联回原始素材名。

### 新生成素材

只有缺失屏幕和缺失组件允许生成。生成源图必须无文字、无样例数值、无水印、无整页 UI 截图；采用低饱和暖纸、水墨细线、青绿主操作、赭金危险操作的同一视觉语言。生成后必须先完成清底、透明边缘检查、单组件裁切、manifest 和可编辑文字层合成，才可导入 UE。

### 文字

玩家名、等级、战力、HP、经验、货币数量、任务状态、物品数量、伙伴属性、卡组内容、按钮文案和 Tooltip 均为运行时 UMG 文字。PSD 内固定的文字仅作为可编辑设计层；不得烤进运行时按钮或窗口底图。

## 按钮与互动语义

`T_TownBackpack_ActionBlank` 只可作为它自身的中性纸色动作底，不能再承担全部 UI 操作。首批至少映射三类视觉语义：

| 语义 | PSD 来源 | 使用位置 |
| --- | --- | --- |
| 中性纸色 | 任务/背包的中性操作底 | 关闭、返回、筛选、查看、取消、非破坏性辅助操作 |
| 青绿主操作 | PSD 任务“前往/追踪”及背包“整理”类底图 | 前往、追踪、确认、装备、选择、主要可推进操作 |
| 赭金危险操作 | PSD 背包“分解”类底图 | 分解、移除、出售确认等有损或不可逆操作 |

每个按钮的 `Normal/Hovered/Pressed/Disabled` 必须保持同一语义色族；Hover 不能仅改变文字，必须有可读的亮度或描边反馈。任何没有现有功能目标的参考按钮不进入运行时界面。

## 运行时集成边界

1. `UGameXXKMVPPlayerController` 继续拥有 Widget 创建、显示层级与输入模式；视觉迁移不得平行创建第二条状态或导航链路。
2. `UGameXXKTownHudWidget` 是常驻 HUD 的新版式入口；`UGameXXKTownOverlayWidget` 的旧测试 API 必须保持可用，但不再展示与 HUD 重复的旧式菜单。
3. `UGameXXKInventoryWindowWidget`、`UGameXXKCompanionRosterWidget`、`UGameXXKTaskPanelWidget` 与角色面板仅更换展示层与语义按钮，继续调用既有 RuntimeState、背包、伙伴和任务 API。
4. 现有角色原画、伙伴/任务 NPC 原画、PaperZD、地图与已调好的场景参数保持原样。
5. 所有新增或替换的 Texture2D 采用 UI 纹理设置：无 mipmap、UI 纹理组、双线性过滤、Clamp、sRGB、保留 alpha、不可流送。

## 首批验收标准

1. 在 16:9 的 1920×1080 与 2560×1440 下，城镇 HUD、角色、伙伴、任务、背包均遵循 PSD 的相对栅格，无元素越界、重叠或因为分辨率而失真。
2. 角色、任务、伙伴、背包显示的数据与迁移前同一存档一致；按钮仍执行原有真实功能。
3. 中性、主操作、危险操作不会共享一张通用空白按钮图；每个实际可点击控件具备 hover/pressed/disabled 反馈。
4. 伙伴页保留最多 12 名伙伴、伙伴背包、每人不同的随机 12 张个人卡组和属性展示；没有把伙伴页退化成纯展示图。
5. 所有新纹理均能追溯到 `semantic-map.json` 中的原始切图或生成源；所有新 PSD 图层都有对应 manifest 记录。
6. `GameXXK_Town_4K.psd` 能在 Photoshop 正常打开，包含完整背景、独立非文字层和可编辑文字层；`validation.json` 的图层数与 manifest 一致且文字回读匹配。
7. UE MCP 保存改动包后关闭编辑器，以用户的 `Build.bat GameXXKEditor Win64 Development -Project=... -NoHotReload` 命令完成冷编译；随后执行相关自动化测试与 PIE 城镇交互截图检查。

## 不变量与风险控制

- 不重置、不覆盖用户已调的资源；使用增量导入和新纹理名，验证后才切换消费端引用。
- 不修改外部 Downloads 工作目录，也不依赖其写权限。
- 不在 PowerShell 参数、管道或脚本字符串中直接传递中文；manifest、SVG、JSX 与 JSON 使用 UTF-8，中文由文件内容承载。
- Photoshop 未运行或 COM 不可用时，可完成图集、裁切、manifest 与 JSX 生成，但不能声称 PSD 或文字回读已验证。
