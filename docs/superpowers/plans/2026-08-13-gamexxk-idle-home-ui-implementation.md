# GameXXK Idle Home UI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 新增沿用现有背包水墨语言的完整 2D 历练界面，准确展示章节、效率、离线结算、队伍刷怪、双宝箱和容量状态，同时保持现有背包/装备布局逐像素不变。

**Architecture:** `FGameXXKIdlePresentationModel` 只把已结算 `IdleState` 投影为只读 snapshot；`UGameXXKIdleHomeWidget` 只渲染 snapshot 并向 Subsystem 发命令，绝不在 Tick/Refresh 中结算。PlayerController 按现有 Widget 管理模式创建它，并复用现有 Inventory/Companion 窗口，而不是复制背包或队伍页面。

**Tech Stack:** UE 5.8 UMG/WidgetTree、CanvasPanel、现有 MasterV2 Approved 纹理与背包模板、MVPPlayerController、UE Automation、MCP/PIE screenshot probe。

---

## 0. 文件职责与布局合同

**Create:**

- `Public/Idle/GameXXKIdlePresentationModel.h`
- `Private/Idle/GameXXKIdlePresentationModel.cpp`
- `Public/UI/GameXXKIdleHomeWidget.h`
- `Private/UI/GameXXKIdleHomeWidget.cpp`
- `Private/Tests/GameXXKIdlePresentationModelTest.cpp`
- `Private/Tests/GameXXKIdleHomeWidgetTest.cpp`

**Modify:**

- `Public/MVP/GameXXKMVPPlayerController.h`、private cpp — Ensure/Open/Close/Refresh IdleHome。
- `Public/MVP/GameXXKMVPSubsystem.h`、private cpp — read-only snapshot 与 UI command facade。
- `Private/Tests/GameXXKPlayerFlowWidgetTest.cpp`、`GameXXKMVPUIWidgetTest.cpp` — 邻接。
- `Content/Python/gamexxk_probe_real_play_flow.py` — 只读 idle UI probe。
- `scripts/gamexxk_real_play_flow_mcp.py`、tests — 新增显式 `--idle-home` 验收模式；不得混入旧目标预演提交。

明确不修改：`GameXXKInventoryWindowWidget.cpp` 中 1450×849 纸窗、200容量、4×5可见格、装备六槽、页签和按钮 geometry。

Presentation 类型：

```cpp
USTRUCT(BlueprintType)
struct FGameXXKIdlePresentationSnapshot
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) int32 ActiveChapter = 0;
    UPROPERTY(BlueprintReadOnly) TArray<int32> UnlockedChapters;
    UPROPERTY(BlueprintReadOnly) int64 GoldPerHour = 0;
    UPROPERTY(BlueprintReadOnly) int64 ExperiencePerHour = 0;
    UPROPERTY(BlueprintReadOnly) int64 PendingGold = 0;
    UPROPERTY(BlueprintReadOnly) int64 PendingExperience = 0;
    UPROPERTY(BlueprintReadOnly) int32 PendingNormalChests = 0;
    UPROPERTY(BlueprintReadOnly) int32 PendingBossChests = 0;
    UPROPERTY(BlueprintReadOnly) int32 PendingChestCount = 0;
    UPROPERTY(BlueprintReadOnly) int32 ChestCapacity = 20;
    UPROPERTY(BlueprintReadOnly) bool bChestCapacityFull = false;
    UPROPERTY(BlueprintReadOnly) int64 NormalCooldownSeconds = 0;
    UPROPERTY(BlueprintReadOnly) int64 BossCooldownSeconds = 0;
    UPROPERTY(BlueprintReadOnly) int64 LastSettledUtcTicks = 0;
    UPROPERTY(BlueprintReadOnly) bool bHasOfflineSummary = false;
    UPROPERTY(BlueprintReadOnly) int64 OfflineDurationSeconds = 0;
    UPROPERTY(BlueprintReadOnly) int64 OfflineGold = 0;
    UPROPERTY(BlueprintReadOnly) int64 OfflineExperience = 0;
    UPROPERTY(BlueprintReadOnly) int32 OfflineNormalChests = 0;
    UPROPERTY(BlueprintReadOnly) int32 OfflineBossChests = 0;
    UPROPERTY(BlueprintReadOnly) bool bOfflineTruncatedByCapacity = false;
    UPROPERTY(BlueprintReadOnly) FText StatusText;
};
```

离线 summary 是当前会话 transient receipt，由成功 settlement result 生成；关闭提示不改账本、不重置游标。

## Task 1: 只读 PresentationModel

- [ ] **Step 1: 写 RED**

构造 full、not-unlocked、offline summary、partial-claim 四种 RuntimeState；Build 前后用 `CompareScriptStruct` 断言输入完全不变，游标/余额/attempt ordinal 不变。

- [ ] **Step 2: 实现唯一 Build**

```cpp
class GAMEXXK_API FGameXXKIdlePresentationModel final
{
public:
    static bool Build(
        const FGameXXKRuntimeState& State,
        const TOptional<FGameXXKIdleSettlementResult>& SessionReceipt,
        FGameXXKIdlePresentationSnapshot& OutSnapshot,
        FString* OutError = nullptr);
};
```

不接收 Now，不调用 TimeSource；cooldown/elapsed 只显示已保存 snapshot。若 UI 需要实时刷新，Controller 定时调用明确的 `AdvanceOnlineIdleToNow` 事务，成功后再 Build。

- [ ] **Step 3: 精确文本**

容量满固定“宝箱已满，产出暂停”；Active=0固定“完成首次通关后解锁历练”；未知/损坏配置 Build 失败并清空 OutSnapshot，不显示高收益兜底。

- [ ] **Step 4: mutation 与 commit**

删除输入不可变保护（故意推进游标）必须 RED；恢复后提交 `feat: add idle presentation snapshot`。

## Task 2: 程序化全屏布局

**Files:** IdleHome widget/test。

- [ ] **Step 1: 先写缺 Widget RED**

测试直接 `NewObject<UGameXXKIdleHomeWidget>()`、`TakeWidget()`，断言名为 `IdleRootCanvas`、`IdleScenePanel`、`IdleTopBar`、`IdleChestPanel`、`IdleBottomDrawer`、`IdleDrawerToggle` 的树。

- [ ] **Step 2: 固定 1920×1080 设计布局**

```text
scene        x=0    y=0    w=1920 h=1080 z=0
top bar      x=64   y=28   w=1792 h=84   z=10
chapter card x=64   y=132  w=300  h=700  z=10
party stage  x=390  y=150  w=1050 h=650  z=10
task action  x=390  y=812  w=164  h=52   z=11
shop action  x=568  y=812  w=164  h=52   z=11
chest panel  x=1470 y=150  w=386  h=650  z=10
drawer btn   x=876  y=1000 w=168  h=56   z=20
drawer       x=300  y=880  w=1320 h=112  z=19 (collapsed initially)
```

Canvas 用现有 `T_MasterV2_PanelLarge`、`T_MasterV2_ButtonNeutral`、`T_MasterV2_ItemSlot` 和既有字体；无底图资源时使用低饱和墨色 Border，不新生成临时高饱和美术。

- [ ] **Step 3: 输入和可见性**

装饰层 `HitTestInvisible`，按钮 Visible；Widget 关闭/重建清 delegate。Drawer 默认 collapsed，点击仅改变本地 UI 状态，不触发 settlement。

- [ ] **Step 4: 真实树测试**

遍历实际 Canvas slots 锁 anchors/alignment/position/size/z；锁装饰输入穿透；SetSnapshot 两次只更新文本/visibility，不重建重复子树。

- [ ] **Step 5: 提交**

`git commit -m "feat: add full idle home layout"`。

## Task 3: 章节、队伍、箱和离线反馈

- [ ] **Step 1: 章节列表**

三章按钮：unlocked 显示“开始历练”，active 显示“历练中”，locked 显示“前往挑战”。点击 unlocked→Subsystem `SwitchIdleChapter`；locked→只发 `RequestRouteChallenge(chapter)`，不修改 unlock。

- [ ] **Step 2: 中央刷怪表现**

只订阅 scheduler 已提交的 transient presentation events；角色/怪物走循环位移动画与简单命中闪白。动画完成/加速/漏帧不得回调奖励规则。没有事件时显示“历练进行中”静态循环。

- [ ] **Step 3: 双箱面板**

两行严格为 Normal/Boss，只显示 `PendingLedger` 对应数量、cooldown ready/text和总历练容量；背包中的箱数另由背包显示，不混进右侧容量。满仓只显示一个明确警示，不弹阻塞 modal。

- [ ] **Step 4: 离线 summary**

成功载入/settle 后显示一次非阻塞纸条：离线时长、金币、经验、普通箱、首领箱、容量截断。Close 只清 session receipt；领取是另一个明确按钮。

- [ ] **Step 5: 测试**

锁：未通关不能切；点击显示状态与 disk commit一致；开/关 summary 不改 state；capacity full 文案；两类数不混；UI animation 10×仍不增奖励。

- [ ] **Step 6: 提交**

`git commit -m "feat: present idle progress and rewards"`。

## Task 4: 底部折叠工具栏与现有页面复用

- [ ] **Step 1: 工具栏六入口**

固定顺序：历练、队伍、背包、装备、天赋、设置。默认只显示展开按钮；展开不自动打开页面。任务/商店不是第七、第八个页签，而是中央场景底部两个明确 action；它们复用既有任务面板和 MetaShop，保证以后切掉3D默认入口仍可完成局外主循环。

- [ ] **Step 2: 复用 Controller 入口**

- 队伍→`OpenCompanionRoster()`；
- 背包→`OpenFreeInventoryWindow()`；装备→新增 `OpenFreeInventoryWindow(EGameXXKInventoryFilter InitialFilter)` 重载并传 Equipment，内部仍复用同一窗口/geometry；
- 天赋→仅打开容量升级小面板，显示 `level/12`、`capacity/80`、下一金币成本；天赋页的完整树、名称与美术继续留在后续规格，但这个容量入口必须能真实升级而非未开放提示；
- 设置→本包只提供“返回主菜单”和“关闭”；包4接入迷你窗/置顶，包5接入自动战斗偏好与1×/2×，不得放不可用占位按钮；
- 任务 action→复用 `OpenTaskPanel()`；商店 action→复用 `OpenMetaShopWindow()`。这两个现有 Widget/Controller 目前都硬检查 `Screen==Town`，因此在各自公开 Open/Refresh 和 `FGameXXKMetaShopRules::PreviewPurchase` 中把合法局外 screen 明确扩为 `{Town, IdleHome}`，购买/接任务仍走原规则与原子保存，不能伪造临时 Town。包3保留当时 Accept 规则，包6再把 Accept/入队分离。

- [ ] **Step 3: 页面互斥与后台继续**

打开背包/队伍不能关闭在线 scheduler；测试用 fake time 在页面打开前后推进相同时间，事件序列相等。关闭子页面回 IdleHome，不新建另一份 state。

- [ ] **Step 4: 背包布局快照邻测**

运行现有 Inventory/PartnerBackpack 快照，所有 geometry/resource path 相等；若因打开来源不同而移动任何布局，视为失败。

- [ ] **Step 5: 提交**

`git commit -m "feat: connect idle home to existing meta pages"`。

## Task 5: PlayerController 生命周期与显式入口

- [ ] **Step 1: 新增 Ensure/Show，但不改默认入口**

```cpp
UGameXXKIdleHomeWidget* EnsureIdleHomeWidget();
bool OpenIdleHome();
bool CloseIdleHome();
```

包3阶段 MainMenu/StartNewGame 仍按旧路径；开发命令和测试 seam 可显式打开 IdleHome。

- [ ] **Step 2: Refresh 生命周期**

Controller 在已显示时按已提交 state Build snapshot；NativeTick 不能直接 settle。World travel/battle 可隐藏 IdleHome，但 Subsystem online activity保持。

- [ ] **Step 3: Widget 创建测试**

无 viewport 时 NewObject fallback 也可建树；真实 viewport 只存在一个 IdleHome；关闭/重开不重复 delegate/settle。

- [ ] **Step 4: 提交**

`git commit -m "feat: host idle home in player flow"`。

## Task 6: 真实 PIE 与视觉验收

- [ ] **Step 1: Python probe**

输出：screen、widget visible、active chapter、效率、两箱/容量、drawer state、关键控件 absolute rect、idle state hash before/after refresh。

- [ ] **Step 2: harness**

使用独立非玩家 slot 或现有 sidecar 备份机制；流程：启动→显式开 IdleHome→drawer→背包→返回→队伍→返回→切已解锁章→领取→截图→cleanup/restore。任何 stop/delete/restore 失败顶层 `ok:false`。

- [ ] **Step 3: 截图审查**

1920×1080 与窗口缩放两档；确认低饱和水墨、文字不截断、两箱有区分但不是高饱和、底栏默认收起、现有背包布局不变。

- [ ] **Step 4: 最终测试**

```text
GameXXK.UI.IdleHome
GameXXK.Idle.Presentation
GameXXK.MVP.UI
GameXXK.MVP.Inventory
GameXXK.MVP.Companion
```

- [ ] **Step 5: production status + push**

创建 `docs/production/2026-08-13-idle-home-ui-status.md`，只声明“显式入口可用”；默认入口仍待包7。
