# GameXXK 双窗口呈现状态机设计

**日期：** 2026-08-29  
**状态：** 已由用户确认并要求继续实施

## 目标

GameXXK 保留两个互相独立的顶层窗口：透明置顶的 `GameXXKDesktopOverlay` 与承载路线、战斗和 3D 城镇的主 GameViewport。桌面挂机工作台可见时，Overlay 拥有呈现权，主 GameViewport 默认最小化到任务栏；进入任何局内路线界面、卡牌战斗或 3D 城镇时，Overlay 隐藏，主 GameViewport 自动恢复、切换到无边框全屏并置前。返回桌面挂机态时，必须先确认 Overlay 已成功显示，再最小化主窗口。

## 状态裁决

窗口策略由当前地图、`EGameXXKScreen`、工作台可见性和 Overlay composition 状态共同决定，不能只读取 `Screen`：

| 条件 | 目标呈现 |
|---|---|
| `L_DesktopTrainingHUD`、`Town`、工作台可见、Overlay 已附着 | `DesktopIdleOverlay` |
| `L_DesktopTrainingHUD` 上的 `DungeonMap`、`RouteEvent`、`RouteCamp`、`RouteMerchant` 或 `Battle` | `FullscreenGameplay` |
| 3D 青山镇、主菜单、世界地图或其他非桌面地图 | `FullscreenGameplay` |
| 桌面 Town 但 Overlay 未成功附着或工作台关闭 | `ViewportFallback` |

`DesktopIdleOverlay` 同时覆盖折叠挂机条、展开工作台和正在游历。局内范围必须包含路线图、事件、休息、商店和战斗；只在 `Battle` 恢复主窗口会让玩家无法操作战斗前路线。

## 原生窗口动作

### 进入桌面挂机态

1. 创建或复用 Overlay，并确认 DirectComposition 与原生命中区域均已附着。
2. 显示 Overlay。
3. 获取真正承载 GameViewport 的 `SWindow`，拒绝把 Overlay 当作主窗口。
4. 对主窗口调用 `Minimize()`，不用 `HideWindow()`。
5. 记录这是桌面策略触发的最小化；只允许一次短延迟重试处理 BeginPlay 后引擎重新显示主窗，禁止永久每帧抢夺用户窗口状态。

### 进入局内或 3D 城镇

1. 隐藏 Overlay；同地图局内切换保留其 composition 以便快速返回，跨地图时按现有安全顺序释放。
2. 重新获取主 GameViewport `SWindow`。
3. 如窗口隐藏则 `ShowWindow()`，如已最小化则 `Restore()`。
4. 调用 `SetWindowMode(EWindowMode::WindowedFullscreen)`；不使用普通最大化，也不使用独占全屏。
5. 调用 `BringToFront(true)`，随后把 Slate 用户焦点交还 GameViewport。

无边框窗口全屏可避免独占模式在双 swapchain、Alt-Tab 和 DirectComposition 之间产生模式切换、黑屏或闪烁。

### Overlay 失败

Overlay 创建、原生附着或 composition 任一步失败时，主窗口保持可见，桌面工作台回退到现有 viewport 呈现。任何失败路径都不得先最小化主窗口。

## 环境边界

- 打包版与 `UnrealEditor -game` 启用完整策略。
- 普通编辑器 PIE 不得最小化 Unreal Editor 主窗口。独立 Game Preview 的模拟策略以后可通过显式测试开关加入，本轮不扩大范围。
- Overlay 是无 Owner 的独立顶层窗口；最小化主窗口不会连带最小化 Overlay。

## 代码结构

- 在 `GameXXKMVPPlayerController` 中增加纯策略枚举与动作规划函数，Automation 可在没有原生窗口时覆盖完整矩阵和动作顺序。
- 生产路径使用同一动作规划结果驱动 `SWindow`，避免测试一套、运行时另一套。
- 将现有 `SetDesktopTrainingGameViewportVisible(bool)` 的隐藏语义替换为明确的“桌面最小化”和“全屏恢复”操作。
- `ShowDesktopTrainingOverlayWindow` 与 `HideDesktopTrainingOverlayWindow` 保留为生命周期入口，但内部委托统一窗口策略。
- `PlayerTick` 只执行有界的最小化重试，不再永久调用隐藏。

## 验收

1. `-game` 启动桌面地图后，Overlay 可见，主窗口 `IsIconic=true` 且不是 `SW_HIDE`。
2. 挂机条动画、TravelRunner、宝箱计时在主窗口最小化后持续推进至少 60 秒。
3. 点击挑战后，从路线图开始主窗口即恢复为显示器工作区无边框全屏，Overlay 隐藏并且输入焦点正确。
4. 路线事件、休息、商店、战斗全过程保持主窗口全屏。
5. 战斗或路线结算返回桌面时，先显示 Overlay，再最小化主窗口。
6. 进入与退出 3D 青山镇均满足相同恢复/最小化策略，工作台快照继续恢复。
7. Overlay 故障注入时主窗口保持可见，应用不会双窗消失。
8. 普通 PIE 不最小化 Unreal Editor 主窗口。
9. 冷 UBT、Workbench 聚焦 Automation 与真实 `-game` Win32/视觉证据全部通过。

