# GameXXK 桌面 2D 剧情层与任务抽屉设计

**日期：** 2026-08-29  
**状态：** 用户逐节确认  
**默认入口：** `L_DesktopTrainingHUD` 展开 Workbench → `剧情任务`  
**方向：** 默认剧情流程不依赖 3D 城镇；3D 青山镇仅保留为显式 Legacy 回归面，后续可以整体取消

## 1. 目标与裁决

剧情任务、任务选择、正式对话、2D 角色演出、暂停与历史记录统一承载在 `GameXXKDesktopOverlay` 的 HUD 2D 层。点击 `剧情任务` 不再默认切换到 3D 城镇，而是在现有 Workbench 中打开任务抽屉；玩家选择任务后进入屏幕级 2D Narrative Layer。

本规格覆盖：

- 任务抽屉 UX 与状态筛选。
- `I/Q/C/Tab` 的统一输入路由。
- 屏幕级 2D Narrative Layer。
- 对话推进、暂停、重播、正常完成与手动领取报酬。
- 关闭游戏、崩溃恢复和输入软锁恢复。
- 与双窗口呈现状态机的关系。

本规格明确废止以下默认玩家路径：

- `剧情任务`按钮直接发起 3D 城镇地图旅行。
- 剧情 Dialogue 只通过主 GameViewport 的 `AddToViewport`呈现。
- `I`打开独立 FreeInventory、`Q`打开旧 TaskPanel、`C`打开独立 CompanionRoster。
- NPC 的“拥有、解锁、加入角色库”语义。NPC 始终属于 CharacterCatalog；剧情只改变故事标记与任务进度。

## 2. Overlay 层级与所有权

`UGameXXKDesktopTrainingWorkbenchWidget` 的根 Canvas 扩展为三个明确层：

```text
DesktopOverlayRootCanvas
├─ WorkbenchLayer
│  └─ 挂机条、Tab、背包、仓库、工具、编队等现有内容
├─ StoryTaskDrawerLayer
│  └─ 复用仓库竖向区域的任务抽屉
└─ NarrativeLayer
   ├─ 屏幕中央 2D 语义舞台
   ├─ 屏幕下方正式对话纸框
   ├─ 气泡、立绘、道具、VFX 与选择
   ├─ 自动播放、历史与跳过已读
   └─ 屏幕右上安全区的暂停按钮
```

DialogueRunner、DialogueCoordinator、NarrativeSequence 与任务规则继续作为逻辑核心，不读取窗口、地图坐标或 Workbench 几何。Desktop Narrative Host 创建并持有实际 Presenter，将现有 DialoguePanel、History 和新的 2D Bubble/Stage Presenter 绑定给 Coordinator。

默认剧情期间主 GameViewport 保持最小化。Narrative Layer 属于同一个 Desktop Overlay，不创建第三个原生窗口。

## 3. 左侧面板统一状态

现有 `bWarehousePanelOpen` 扩展为显式互斥状态：

```cpp
enum class EGameXXKDesktopTrainingLeftPanel : uint8
{
    None,
    Warehouse,
    StoryTasks
};
```

`Warehouse` 与 `StoryTasks` 使用相同竖向长面板区域、纸框尺寸和右上角关闭按钮位置，并保留各自独立 UI 状态。

只要左侧长面板不是 `None`：

- `剧情任务`与`进入城镇`纵向按钮组一起平移到面板外侧。
- 平移后的任务按钮仍是任务抽屉开关。
- 任务按钮再次点击、右上角关闭按钮和 `Esc`执行同一个关闭事务。
- 关闭后按钮组返回原位置，Tab 保持展开。

打开任务抽屉前必须终止物品拖拽/携带事务。关闭任务抽屉不改变仓库页码、排序、筛选，也不清空任务页签、滚动和选择状态。

## 4. 任务抽屉 UX

任务抽屉顶部只有两个页签：

```text
[可进行]  [待领取 ●]
```

只要存在至少一个 `Completed`且尚未领取报酬的任务，`待领取`页签右上角显示红点；红点不显示数量。

### 4.1 列表与详情

列表条目只显示：

```text
状态小标记  任务标题
            一行剧情摘要
```

列表条目不包含独立操作按钮。选中条目后，面板底部固定详情区显示：

- 任务标题。
- 两至三行剧情简介。
- 当前状态。
- 报酬预览。
- 一个固定主操作按钮。

主按钮根据选中任务显示：

| 任务状态 | 页签 | 主按钮 |
|---|---|---|
| `Available` | 可进行 | 接取任务 |
| `Active`且未播放 | 可进行 | 继续剧情 |
| `Completed`且未领奖 | 待领取 | 领取奖励 |
| `Locked` | 不显示 | 无 |
| `Rewarded` | 不显示 | 无 |

“继续剧情”表示继续该任务，但按本规格从当前剧情段入口重新播放，不从暂停节点恢复。

### 4.2 排序与恢复

`可进行`排序：

1. `Active`任务优先。
2. `Available`任务随后。
3. 同状态内按 authored story order 排列。

打开时优先选中最近暂停的 Active 任务，否则选择第一项 Available。存在待领取红点时不自动切页。

`待领取`按完成时间倒序；进入页签默认选择第一项。领取成功后移除当前条目并选择下一项；最后一项领取后红点立即消失。

两个页签分别保存：

- 滚动位置。
- 最后选中的 TaskId。

如果保存的任务已离开该页签，按默认选择规则恢复。空列表显示明确空状态，底部主按钮禁用。

## 5. 输入统一路由

不再让物理按键直接创建旧独立窗口。保留快捷键习惯，但全部重定向到 Workbench：

| 输入或旧按钮 | 新语义 |
|---|---|
| `I` / TownHud 背包按钮 | 打开 Workbench、展开 Tab、切换到嵌入背包页 |
| `Q` / TownHud 任务按钮 | 打开 Workbench、展开 Tab、打开 StoryTasks |
| `C` / TownHud 伙伴按钮 | 打开 Workbench、展开 Tab、切换到编队页 |
| `Tab` | 普通状态下展开/折叠 Workbench |
| `F` | 仅显式 Legacy 3D 的 NPC/场景交互 |
| `Space`、`Enter`、左键 | NarrativeActive 时推进对白 |
| `1–4` | NarrativeActive 时选择对话选项 |
| `Ctrl` | 跳过已读 |
| `Esc` / 暂停按钮 | NarrativeActive 时暂停剧情；其他状态按模态优先级返回 |

路由优先级：

```text
NarrativeActive
→ StoryTaskDrawer
→ Workbench 页面快捷键
→ LegacyTown 交互
→ Route/Battle 输入
→ Super::InputKey
```

NarrativeActive 必须消费 `Tab/I/Q/C/F`，不能让它们穿透。暂停或正常结束后这些输入恢复。StoryTaskDrawer 打开时，`Q`保持抽屉聚焦；`I/C`关闭抽屉并切换对应 Workbench 页面。

旧 Widget 类暂时保留给嵌入复用、测试和显式 Legacy 回退，但玩家入口不再调用 `OpenFreeInventoryWindow`、旧 `OpenTaskPanel`或独立 `OpenCompanionRoster`。

## 6. Narrative Layer 布局

Narrative Layer 使用屏幕/显示器工作区坐标，不依附挂机条位置、尺寸、拖动锚点或展开方向。

进入 NarrativeActive：

1. 关闭任务抽屉。
2. WorkbenchLayer 整体隐藏；挂机条和 Tab 均不可见。
3. 保留桌面 HUD 位置和用户设置，不重置它们。
4. Desktop Overlay 原生窗口临时扩展到当前显示器工作区。
5. 显示 Narrative Layer。

布局：

- 屏幕中央为 2D 语义舞台。
- 正式 Dialogue 纸框锚定屏幕下方 Safe Area。
- 暂停按钮锚定屏幕右上 Safe Area，并拥有最高输入 ZOrder。
- 自动、历史、跳过已读位于正式对话纸框内。
- 角色、道具、气泡与特效只引用语义槽，例如 `Left`、`Center`、`Right`、`Prop`、`Vfx`，不引用世界坐标。

暂停或正常完成后：

1. 隐藏 Narrative Layer。
2. Overlay 恢复桌面 HUD 原窗口范围、透明命中区域和位置。
3. WorkbenchLayer 重新显示。
4. 强制进入普通折叠挂机条状态。
5. Tab 解锁，但不恢复剧情开始前的展开状态。

## 7. 任务、重播与报酬状态机

```text
Locked → Available → Active → Completed → Rewarded
```

### 7.1 开始与重播

`Available`点击“接取任务”后：

- Task 进入 Active。
- 任务抽屉关闭。
- WorkbenchLayer 隐藏。
- 从该剧情段入口开始 Sequence。

Active 任务暂停后，在任务抽屉中显示“继续剧情”。点击后从当前剧情段入口重播。

表现命令允许重播；带玩法副作用的命令必须有稳定幂等键。剧情设计应把正式故事结果集中在整段正常完成事务中，避免中途留下半完成状态。

### 7.2 正常完成

最后一个 Dialogue/Sequence 节点正常完成后，原子提交剧情结果：

- 写入故事标记，例如 `StoryFlag.Met.YueBai`。
- 推进当前主线 Step。
- 开放后续任务。
- 当前任务进入 Completed。

NPC 没有解锁或拥有状态。剧情不得修改 NPC 列表、伙伴列表或编队状态。

金币、经验和普通物品等奖励不在剧情完成事务中发放。它们保留为待领取报酬。

### 7.3 手动领取

玩家在`待领取`页选择任务并点击“领取奖励”。领取必须是原子事务：全部成功后 Task 进入 Rewarded；任何验证或落位失败时保持 Completed，不发放部分奖励、不移除条目、不清除红点。

## 8. 暂停与故障安全恢复

暂停是硬性安全路径，不依赖当前 Dialogue 节点、Sequence 命令、动画、资源或任一 Presenter 正常工作。

右上角暂停按钮与 `Esc`统一调用 `AbortNarrativeToDesktop()`：

1. 设置退出重入保护。
2. 取消当前异步演出、移动、等待和 UI Pending。
3. 关闭 Dialogue、气泡和历史。
4. 将当前剧情段重置到入口。
5. Task 保持 Active。
6. 释放 Narrative 拥有的移动、视角、鼠标和键盘锁。
7. 隐藏 Narrative Layer。
8. 恢复 Overlay 原窗口范围和命中区域。
9. 显示折叠挂机条并解锁 Tab。
10. 保存“下次从本段入口播放”。

要求：

- 清理不得因为空 Coordinator、空 Widget、资源丢失或单步错误提前 return。
- 重复暂停幂等。
- 必需演出命令失败或超时自动走同一恢复路径，并显示可重试提示。
- Sequence 执行器禁止阻塞循环，只允许异步、有界等待。
- 正常完成事务与暂停互斥，不允许半提交。

## 9. 关闭游戏与读档

关闭游戏重启绝不自动进入剧情。

- 剧情中正常退出：先走故障安全暂停，再保存。
- 强退或崩溃留下活动 NarrativeSession：读档归一化为 Task Active、剧情段入口待重播，清除 Dialogue 显示、Pending 与输入锁。
- 启动后只显示普通折叠挂机条；Narrative Layer 和任务抽屉均关闭，Tab 可用。
- 玩家主动打开任务抽屉并点击“继续剧情”后才重播。
- 已正常完成但未领奖的任务启动后只显示待领取红点，不重播剧情。
- 完成事务必须保证读档只看到 Active 待重播或 Completed 待领奖，不存在半完成状态。

## 10. 与双窗口状态机的关系

双窗口呈现 Resolver 必须增加 Narrative Overlay 所有权输入。桌面 NarrativeActive 时，即使 WorkbenchLayer 整体隐藏，仍属于 Overlay presentation：

```text
Desktop map + NarrativeLayerActive + Overlay attached
→ Overlay visible/full-work-area
→ Primary GameViewport minimized
```

不能沿用“Workbench visible 才最小化主窗口”的旧条件，否则一隐藏挂机条就会错误恢复主 GameViewport。

路线图、路线事件、休息、商店、战斗继续使用主 GameViewport 无边框全屏。显式 Legacy 3D 城镇同样使用主窗口，但不再是剧情任务默认入口。

## 11. 验收

### 任务抽屉

- 任务按钮仅在 Tab 展开后可见。
- 点击任务按钮在仓库区域打开 StoryTasks，并将任务/进城按钮组平移到外侧。
- 平移后的任务按钮、右上关闭按钮和 Esc 均可关闭；Tab 保持展开。
- 可进行/待领取筛选、选择、滚动、固定底部按钮和红点规则符合本规格。
- 仓库事务、页码、排序和筛选不被任务抽屉破坏。

### 输入

- `I/Q/C`只打开 Workbench 对应页面，不创建旧独立窗口。
- TownHud 三个旧按钮走相同语义路由。
- NarrativeActive 期间 `Tab/I/Q/C/F`均被消费。
- 暂停或正常结束后输入恢复，Workbench 以折叠状态出现。

### Narrative

- 主窗口保持最小化，Overlay 扩展到显示器工作区。
- 中央演出区、下方对话纸框和右上暂停按钮不依赖挂机条几何。
- 剧情期间挂机条与 Tab 整体隐藏。
- 空格、点击、自动、历史、跳过已读和选择均可用。
- 暂停在 Dialogue、选择、等待、移动、资源失败、空 Coordinator、Pending Executor 和重复调用下都能恢复桌面，不软锁。

### 任务与存档

- 暂停后从剧情段入口重播。
- 正常完成立即提交故事标记、主线推进和后续任务开放，不修改 NPC/伙伴/编队列表。
- 物质报酬只在任务抽屉手动领取，且原子提交。
- 正常退出、强退恢复、启动、Completed 待领取均不自动进入剧情。

### 回归

- 默认表面仍为 `L_DesktopTrainingHUD`。
- 挂机 TravelRunner、路线图、商店、战斗与返回桌面保持原语义。
- 显式 Legacy 3D 城镇仍可进入和退出，但不再拥有默认剧情任务入口。
- 冷 Editor/Game UBT、聚焦 Automation、真实 `-game` 输入/窗口取证和 luna 视觉检查通过。

