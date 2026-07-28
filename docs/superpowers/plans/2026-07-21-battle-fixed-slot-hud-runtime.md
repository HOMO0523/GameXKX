# 战斗固定槽位 HUD 实施计划

> 直接在 `main` 执行；不创建 worktree；保留无关脏改动和人工调整的关卡/角色资产。

## 任务 1：固定布局契约与 RED 测试

文件：

- `Source/GameXXK/Private/Tests/GameXXKBattleProjectedUnitHudTest.cpp`
- `Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h`

先添加失败测试，证明：

1. Party 1P/2P/3P 和 Enemy 1P/2P/3P 使用六个稳定、不同的 Canvas Anchors；我方权威顺序固定为伙伴 1P、主角 2P（中间）、任务 NPC 3P。
2. 插入任意世界投影值、清空中心投影或连续 Tick 都不会改动 HUD 的固定 Anchor / Alignment / 尺寸。
3. 无效 Slot、死亡和移除会隐藏/删除 HUD。
4. 测试在两种 Canvas 大小下只允许 UMG 锚点带来的正常重排，不能依赖手工 DPI 换算或 `SetOffsets` 的世界坐标。

## 任务 2：分离 HUD 与投影桥

文件：

- `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`
- `Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h`
- `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`（仅移除 HUD 脚底注册；保留箭头中心投影）

实现：

1. 为 `FGameXXKBattleUnitHudView` 解析 Side + SlotNumber 到固定 Anchor、Alignment、Size，并让 Battle Board 的 UI 与战斗相机共享明确的居中 16:9 安全舞台。
2. 创建或刷新 HUD 时应用该固定 Canvas Slot；每帧不再调用 HUD 位置投影刷新，也不根据障碍物修改位置。
3. `RefreshBattleCardTargetingBridge` 仅登记角色中心用于目标箭头/高亮，停止登记脚底 HUD 坐标。
4. 删除或明确弃用只为脚底 HUD 服务的缓存和测试 seam；不能把其继续作为显示前提。

## 任务 3：冷验证和真实 PIE

1. 通过 MCP 保存，再关闭编辑器；执行 `Build.bat GameXXKEditor Win64 Development -Project="D:\UE5 demo\GameXXK\GameXXK.uproject" -NoHotReload`。
2. 用 `UnrealEditor-Cmd.exe` 运行焦点自动化测试。
3. 直接打开 `.uproject`，走 `L_Main → Town → Route → Battle`，采集真实 HUD 状态和截图。
4. 使用两个 viewport 尺寸/纵横比复验 HUD Anchor、Align、Size 和可见性；不得声称做过无法获得的人工 DPI 操作。

## 后续任务（不与本任务混改）

- HP/内力改为真实的独立 Track/Fill PSD 组件，删除复合底图 + 纯色 Fill。
- 状态图标改为单一纸墨视觉层、紧裁剪 V4 资源、保留层数和 Tooltip。
