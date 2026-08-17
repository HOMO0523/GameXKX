---
status: shelved
updated_at: 2026-08-17
shelved_reason: legacy migration package; do not execute
superseded_by: docs/superpowers/specs/2026-08-17-gamexxk-desktop-training-workbench-design.md
---
# GameXXK Desktop Mini Window Implementation Plan

> 执行冻结：当前 `CurrentSaveVersion=17`，本文的 v15/v16/v17/v18 迁移边界不能直接执行。恢复历练实现必须以 `2026-08-17-gamexxk-desktop-training-workbench-design.md` 和新的 Phase 0 基线重新编排迁移编号。

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 提供默认 `640×180`、可拖动、默认置顶且可关闭置顶的独立桌面迷你窗，并让向上抽屉、领取和章节切换与完整历练界面共享同一展示模型和权威事务。

**Architecture:** 使用 `SWindow` 创建真正的小原生窗口，内容由专用 `SGameXXKIdleMiniWindow` 渲染；窗口设置持久化在本地 `UGameUserSettings`/SaveConfig，而奖励状态仍只在 v16 游戏存档。mini controller 仅订阅 `FGameXXKIdlePresentationSnapshot` 并发 Subsystem 命令，绝不自己结算或复制状态。

**Tech Stack:** UE 5.8 Slate/SWindow/FSlateApplication、FDisplayMetrics、多显示器/DPI、GameUserSettings config、UMG/Slate bridge、Automation、Windows 真实运行探针。

---

## 0. 文件结构与窗口状态

**Create:**

- `Public/Idle/GameXXKIdleMiniWindowSettings.h`、private cpp — 本地 UI 偏好。
- `Public/Idle/GameXXKIdleMiniWindowController.h`、private cpp — SWindow 生命周期。
- `Private/UI/SGameXXKIdleMiniWindow.h`、cpp — 640×180 内容。
- `Private/UI/SGameXXKIdleMiniDrawer.h`、cpp — 向上展开抽屉。
- `Private/Tests/GameXXKIdleMiniWindowSettingsTest.cpp`
- `Private/Tests/GameXXKIdleMiniWindowTest.cpp`

**Modify:**

- `GameXXK.Build.cs` — Slate/SlateCore 已为 public dependency，无需新增 OnlineSubsystem；只有编译证明需要时才改。
- `MVPPlayerController` — 打开/关闭/返回完整界面。
- `IdleHomeWidget` — “打开迷你窗”按钮。
- `IdlePresentationModel` — 不新增结算，只复用 snapshot。
- Python probe/harness — Win32 window rect、topmost、click-through outside、多显示器恢复。

本地偏好：

```cpp
UCLASS(Config=Game, DefaultConfig)
class UGameXXKIdleMiniWindowSettings : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY(Config) bool bAlwaysOnTop = true;
    UPROPERTY(Config) FString MonitorId;
    UPROPERTY(Config) FVector2D LogicalPosition = FVector2D::ZeroVector;
    UPROPERTY(Config) float SavedDPIScale = 1.0f;
    UPROPERTY(Config) bool bHasSavedPosition = false;
};
```

偏好不包含奖励、时间、章节或 cooldown。

## Task 1: 位置/DPI/显示器纯规则

- [ ] **Step 1: 写 RED**

测试三个显示器工作区、125%/150% DPI、移除已保存显示器、负坐标左屏、分辨率缩小。无保存时放当前主显示器右下角；有保存时用逻辑坐标恢复；任何情况下整窗至少保留在一个 work area 内。

- [ ] **Step 2: 定义纯计算 API**

```cpp
struct FGameXXKMiniWindowPlacement
{
    FVector2D PixelPosition;
    FVector2D PixelSize;
    FString MonitorId;
    float DPIScale = 1.0f;
};

static FGameXXKMiniWindowPlacement ResolvePlacement(
    const UGameXXKIdleMiniWindowSettings& Settings,
    const TArray<FMonitorInfo>& Monitors,
    const FString& PreferredMonitorId);
```

LogicalPosition 存相对 work-area 左上、已除 DPI 的坐标；保存时由实际 window rect 反算。窗口大小逻辑固定640×180，物理像素由 monitor DPI 决定。

- [ ] **Step 3: mutation/GREEN/commit**

临时取消 clamp，显示器移除用例 RED；恢复提交 `feat: add mini window placement rules`。

## Task 2: 原生窗口与输入边界

- [ ] **Step 1: 写缺 controller RED**

在可初始化 Slate 的 Automation 中 Open→单一 SWindow、ClientSize逻辑640×180、SizingRule fixed、SupportsMaximize/Minimize false、初始 topmost true。

- [ ] **Step 2: 实现 Controller**

```cpp
class FGameXXKIdleMiniWindowController : public TSharedFromThis<FGameXXKIdleMiniWindowController>
{
public:
    bool Open(UGameXXKMVPSubsystem* Subsystem, FString* OutError = nullptr);
    void Close();
    bool SetAlwaysOnTop(bool bEnabled, FString* OutError = nullptr);
    void Refresh(const FGameXXKIdlePresentationSnapshot& Snapshot);
    bool IsOpen() const;
private:
    TSharedPtr<SWindow> Window;
    TSharedPtr<SGameXXKIdleMiniWindow> Content;
};
```

用 `FSlateApplication::AddWindow` 添加独立 window；不要创建全桌面透明 overlay。窗口外天然不命中此 HWND；窗口内部只让明确按钮/drag area处理输入，装饰 `HitTestInvisible`。

- [ ] **Step 3: 关闭与退出**

OnWindowClosed 保存 placement、解绑 delegate、清 shared ptr；游戏退出不创建驻留进程。关闭 mini 不切 Offline；进程退出/休眠空档由包1下一次 settlement 接管。

- [ ] **Step 4: 输入测试**

锁：content 外没有额外 transparent window；装饰不吃输入；drag area返回 BeginWindowMove；button 不触发 drag；关闭后 weak window失效。

- [ ] **Step 5: 提交**

`git commit -m "feat: host idle view in native mini window"`。

## Task 3: 迷你内容与共享 snapshot

- [ ] **Step 1: 构造固定布局**

```text
background 0,0,640,180
chapter    16,12,132,28
party      156,12,250,116
rates      16,50,132,66
chests     420,12,138,92
claim      566,104,62,36
drawer     598,12,30,30
```

只显示当前章、队伍循环、金币/经验效率、两类箱/容量、领取和 drawer。禁止路线、手牌、商店或奖励选择。

- [ ] **Step 2: 同 snapshot parity test**

给 Full Widget 与 Mini 同一 snapshot，断言 active chapter、两效率、两箱数量、容量满状态完全相等；两者 Refresh 前后 RuntimeState hash不变。

- [ ] **Step 3: 动画非权威**

Mini animation 使用 presentation event queue 的拷贝；窗口隐藏/拖动/2×不得改变 scheduler cursor。停止渲染后重新显示只刷新快照，不补播生成奖励。

- [ ] **Step 4: 提交**

`git commit -m "feat: render shared idle snapshot in mini window"`。

## Task 4: 向上抽屉与命令

- [ ] **Step 1: 抽屉方向 RED**

打开前 window逻辑180高；打开后 drawer window/扩展区域只向上增加高度，原 bottom-right 锚点不变。若上方空间不足，将整体夹入 work area，但 drawer 不向下盖过原主内容。

- [ ] **Step 2: 内容**

已通关章节切换、领取、置顶开关、返回完整界面。Locked chapter只显示“挑战”且返回完整界面后进入 route；不得在 mini 内做路线决策。

- [ ] **Step 3: topmost**

默认 on；创建时用 `SWindow::IsTopmostWindow(bEnabled)`。UE 5.8 的 `SWindow` 没有运行时 topmost setter，因此 Windows 运行时切换必须通过 `Window->GetNativeWindow()->GetOSWindowHandle()` 获取 HWND，再调用 `SetWindowPos(HWND_TOPMOST/HWND_NOTOPMOST, SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE)`；随后用 `GetWindowLongPtr(GWL_EXSTYLE)&WS_EX_TOPMOST` 读回成功才 `SaveConfig`。非 Windows 构建返回明确 unsupported 且不改变偏好。置顶不是全局单例锁，不改变主窗口 z-order。

- [ ] **Step 4: 返回完整界面**

关闭 mini 或隐藏它，再由 PlayerController `OpenIdleHome`；同一 Subsystem/RuntimeState，不 Load/Settle 第二次。

- [ ] **Step 5: 测试/commit**

`git commit -m "feat: add upward idle mini drawer"`。

## Task 5: 真实 Windows 验收

- [ ] **Step 1: 探针输出**

输出 HWND/title、client rect、window rect、monitor id/work area/DPI、topmost extended style、drawer rect、main state hash、各按钮 screen rect。

- [ ] **Step 2: 真实流程**

打开 mini→确认640×180逻辑→拖到另一屏→关置顶→抽屉向上→切章→领取→返回full→再开mini→验证位置/topmost恢复。

- [ ] **Step 3: 外部点击不拦截**

在窗口 rect 外对安全测试窗口发送点击，确认 GameXXK HWND不收到；不得点击真实桌面文件或用户应用。这个验收使用专用测试宿主区域。

- [ ] **Step 4: 显示器/DPI恢复**

自动测试 mock metrics，真实测试至少当前显示器缩放；无法物理移除显示器时不伪称已真机验证，保留 mock证据。

- [ ] **Step 5: 冷 UBT/组测试**

```text
GameXXK.Idle.MiniWindow
GameXXK.Idle.Presentation
GameXXK.UI.IdleHome
GameXXK.MVP
```

- [ ] **Step 6: 状态文档与 push**

创建 `docs/production/2026-08-13-idle-mini-window-status.md`，附截图、rect、DPI、topmost与cleanup。不得声称支持程序关闭后常驻；设计明确不常驻。
