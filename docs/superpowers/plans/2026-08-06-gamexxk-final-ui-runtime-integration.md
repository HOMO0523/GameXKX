# GameXXK 最终确认 UI 运行时接入实施计划

> **执行方式：** 直接在项目根目录 `main` 工作，不建 worktree。纯美术拆分/导入不做 TDD，用清单和视觉验证；所有 C++ 行为修改严格先红后绿。禁用 Live Coding/Hot Reload。

**目标：** 把当前保存的最终 Master UI 组件化接入 UE；主角和伙伴背包均支持六槽装备、右键装备/替换/卸下，卡组页签显示对应角色的卡组背包与编入牌组，所有城镇导航和确认页面可点击、有状态反馈，并完成冷编译与真实 PIE 验收。

**架构：** 保留现有 `UGameXXKInventoryWindowWidget`、`UGameXXKCompanionRosterWidget`、`UGameXXKTownHudWidget` 等 C++ UMG 协调器。新增一个只负责“当前角色 + 六槽 + 仓库装备实例 + 快速事务”的共享背包交互模型，主角背包和伙伴背包共同调用；卡组继续使用现有 Subsystem 的主角 12/8、伙伴 12/5 数据。PSD 只提供无文字组件贴图。

**技术栈：** Photoshop/JSX、PowerShell、Python、UE 5.8 MCP、C++ UMG、Automation Tests、UBT、PIE 探针。

---

## 任务 1：锁定最终 PSD 与生成完整组件清单（美术，无 TDD）

**文件：**

- 修改：`scripts/ui_psd_pipeline/prepare-approved-runtime-assets.ps1`
- 新建：`scripts/ui_psd_pipeline/validate-final-master-psd.ps1`
- 新建：`SourceArt/UI/PSD/gamexxk-v4/ui-master/final-approved-runtime-assets-manifest.json`
- 新建：`SourceArt/UI/PSD/gamexxk-v4/ui-master/RuntimeApproved/*.png`
- 新建：`SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/FinalRuntimeIntegration/*.png`

### 步骤

1. 只读检查 Master PSD 的当前 SHA256、顶层页面、关键组名和可见状态；若 SHA256 与设计基线不同，先重新备份并更新清单，不覆盖用户的新保存。
2. 扩充已有资源映射，至少包含：共用壳体、统一关闭按钮、短元宝条、主角/伙伴标题区、六装备槽、4 x 5 物品格、`inventory_scrollbar_Button`、分解/遣散按钮底、五页签、卡底、锁图标、卡组左右切换、伙伴三槽和分页箭头、Tooltip 纸片。
3. 为每个 PNG 记录：`sourcePsdSha256`、`sourceLayer`、宽高、RGBA、Alpha bounds、SHA256、`ueDestination`、是否含文字。
4. 生成四张组合预览：主角装备基础态、主角物品选中态、主角卡组态、伙伴卡组态。
5. 运行：

```powershell
pwsh -File scripts/ui_psd_pipeline/validate-final-master-psd.ps1
```

该脚本包含中文 PSD 图层名，统一由 PowerShell 7 以 UTF-8 解析；不使用 Windows PowerShell 5.1 直接读取无 BOM 的 UTF-8 源文件。

预期：退出码 0，清单状态 `PASS`，所有必需组件非空、Alpha 边缘有效、运行时文字未烘焙。

## 任务 2：为共享六槽背包交互建立失败测试（运行时，RED）

**文件：**

- 新建：`Source/GameXXK/Public/UI/GameXXKCharacterBackpackModel.h`
- 新建：`Source/GameXXK/Private/Tests/GameXXKCharacterBackpackModelTest.cpp`
- 修改：`Source/GameXXK/GameXXK.Build.cs`（仅在缺少所需输入模块时修改）

### 先写测试契约

共享模型公开最小接口：

```cpp
struct FGameXXKCharacterBackpackSlotView
{
    EGameXXKEquipmentSlot Slot;
    FName EquippedInstanceId;
};

class FGameXXKCharacterBackpackModel
{
public:
    void Bind(UGameXXKMVPSubsystem* InSubsystem, FName InCharacterId);
    TArray<FGameXXKCharacterBackpackSlotView> GetSixSlotSnapshot() const;
    bool QuickEquip(FName WarehouseInstanceId, FGameXXKEquipmentTransactionResult& OutResult);
    bool QuickUnequip(EGameXXKEquipmentSlot Slot, FGameXXKEquipmentTransactionResult& OutResult);
};
```

测试必须先覆盖并失败：

- 稳定顺序为 Weapon/Head/Armor/Belt/Shoes/Accessory。
- 主角与永久伙伴均能读取自己的六槽快照。
- 右键装备第二把同槽武器时，新武器入槽、旧武器回仓库。
- 六种槽位都能卸下，不再只支持旧三槽。
- 无效角色、非仓库实例、槽位不匹配、仓库满和路线锁定不改变运行时状态。

运行目标测试：

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'D:\UE5 demo\GameXXK\GameXXK.uproject' `
  -Unattended -NoSplash -NoSound -NullRHI `
  '-ExecCmds=Automation RunTests GameXXK.MVP.UI.CharacterBackpackModel;Quit' `
  '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

预期：因模型尚未实现或断言未满足而失败，保存 RED 证据。

## 任务 3：实现共享六槽背包模型（GREEN）

**文件：**

- 新建：`Source/GameXXK/Private/UI/GameXXKCharacterBackpackModel.cpp`
- 修改：`Source/GameXXK/Public/UI/GameXXKCharacterBackpackModel.h`

### 实现规则

1. `Bind` 只保存弱 Subsystem 和 CharacterId，不缓存可失效的运行时状态。
2. `GetSixSlotSnapshot` 每次调用 `GetEquipmentLoadoutSnapshot`。
3. `QuickEquip` 从 `GetEquipmentWarehouseSnapshot` 查实例和定义槽，调用 `EquipEquipmentInstance`；不手写替换逻辑。
4. `QuickUnequip` 直接调用 `UnequipEquipmentSlot`。
5. 所有失败原样返回 `FGameXXKEquipmentTransactionResult`，UI 不猜测错误原因。
6. 重新运行任务 2 的测试，预期全绿。

## 任务 4：主角背包六槽、右键和最终布局（RED → GREEN）

**文件：**

- 修改：`Source/GameXXK/Public/UI/GameXXKInventoryWindowWidget.h`
- 修改：`Source/GameXXK/Private/UI/GameXXKInventoryWindowWidget.cpp`
- 修改：`Source/GameXXK/Private/Tests/GameXXKInventoryEnhancementTest.cpp`
- 新建或修改：`Source/GameXXK/Private/Tests/GameXXKFinalInventoryWidgetTest.cpp`

### RED 契约

- `GetEquipmentSlotCountForTest() == 6`。
- `GetBackpackSlotCountForTest() == 20`，网格列数为 4。
- 关闭按钮、右滚动条、分解按钮和选中墨迹都使用 Approved 资源。
- 当前装备读取来自实例制 loadout，而不是旧三字段。
- 为测试提供明确入口：

```cpp
bool QuickEquipBackpackInstanceForTest(FName InstanceId);
bool QuickUnequipSlotForTest(EGameXXKEquipmentSlot Slot);
FName GetEquippedInstanceForSlotForTest(EGameXXKEquipmentSlot Slot) const;
int32 GetBackpackColumnCountForTest() const;
```

### GREEN 实现

1. `UGameXXKInventorySlotButton::NativeOnMouseButtonDown` 识别 `EKeys::RightMouseButton`，调用 Owner 的快速动作并返回 `FReply::Handled()`；左键仍通过 `OnClicked` 选择并显示 Tooltip。
2. 主角目标固定为 `FGameXXKEquipmentRules::HeroCharacterId()`。
3. 生成六个方形装备槽，按 PSD 左三右三围绕主角立绘；不使用旧竖排三块大槽。
4. 背包变为 4 x 5，并把 Approved 右滚动条作为真实 `UScrollBox` 表现。
5. `RefreshEquipmentSlots` 使用共享模型快照，显示真实实例图标与 Tooltip。
6. 快速事务成功后刷新六槽、仓库、属性和选中态；失败显示 `OutResult.Message`。
7. `分解` 只对允许分解的仓库装备启用，保持现有确认流程。
8. 运行目标测试并确认全绿。

## 任务 5：主角属性/装备/卡组/天赋/称号页签（RED → GREEN）

**文件：**

- 修改：`Source/GameXXK/Public/UI/GameXXKInventoryWindowWidget.h`
- 修改：`Source/GameXXK/Private/UI/GameXXKInventoryWindowWidget.cpp`
- 修改：`Source/GameXXK/Private/UI/GameXXKCharacterPanelWidget.cpp`
- 修改：`Source/GameXXK/Private/Tests/GameXXKFinalInventoryWidgetTest.cpp`

### 契约

新增显示模式：

```cpp
UENUM()
enum class EGameXXKCharacterBackpackTab : uint8
{
    Attributes,
    Equipment,
    Deck,
    Talents,
    Titles
};
```

测试：

- 五个页签均为真实按钮，选中贴图唯一。
- 属性页显示真实主角快照。
- 装备页显示六槽 + 20 格。
- 卡组页显示 `卡组背包`/`角色卡组` 两视图。
- 天赋/称号显示 `尚未开放` 且不会改运行时数据。
- 切页清除不适用的 Tooltip/确认框，不丢失已装备状态。

### 实现

复用 `UGameXXKCharacterPanelWidget` 的属性格式化逻辑，避免复制公式；天赋和称号只做明确空状态。完成后运行 `GameXXK.MVP.UI.FinalInventory`。

## 任务 6：主角卡组背包与角色卡组（RED → GREEN）

**文件：**

- 修改：`Source/GameXXK/Public/UI/GameXXKInventoryWindowWidget.h`
- 修改：`Source/GameXXK/Private/UI/GameXXKInventoryWindowWidget.cpp`
- 新建：`Source/GameXXK/Private/Tests/GameXXKCharacterDeckTabWidgetTest.cpp`

### 契约

- 主角卡组背包始终展示 12 张定义。
- 未解锁卡存在灰暗覆盖和锁图标，可悬停，不可编入。
- 角色卡组精确 8 张。
- 左右按钮只在 `DeckBackpack` 与 `CharacterDeck` 之间切换。
- 选卡使用现有 `SetHeroCardLoadout`；非 8 张不能提交。
- 路线锁定后仍可查看、不可修改。

### 实现

把 `UGameXXKCompanionRosterWidget` 中成熟的英雄卡池读取和 Tooltip 组合抽成共享函数或轻量 Presenter；不要在两个 Widget 中复制卡牌定义遍历、锁状态和 Tooltip 格式化。

## 任务 7：把伙伴名册重构为最终伙伴背包（RED → GREEN）

**文件：**

- 修改：`Source/GameXXK/Public/UI/GameXXKCompanionRosterWidget.h`
- 修改：`Source/GameXXK/Private/UI/GameXXKCompanionRosterWidget.cpp`
- 修改：`Source/GameXXK/Private/Tests/GameXXKCompanionRosterWidgetTest.cpp`
- 新建：`Source/GameXXK/Private/Tests/GameXXKPartnerBackpackWidgetTest.cpp`

### RED 契约

- 布局标题为 `伙伴`，不是旧的 `伙伴行囊`/名册三栏。
- 伙伴背包使用与主角相同的六槽、20 格、筛选、Tooltip、滚动条和关闭按钮资源。
- 底部恰好 3 个伙伴头像槽，4 页覆盖 12 个位置。
- 首次打开定位到当前出战伙伴页。
- 空位无头像且不可点击；未出战用 `_inactive`，出战用正常头像。
- 悬停返回伙伴名称。
- 点击拥有的伙伴立即调用 `SetActivePermanentCompanion`，唯一出战状态更新，不存在单独出战按钮。
- 箭头只翻页。
- 伙伴右键装备/卸下修改当前伙伴 CharacterId，不修改主角。

### GREEN 实现

1. 删除旧布局中的单独 `设置出战`、`暂不编入`、12 格文字名册和三栏框，但不删除底层公开能力。
2. 使用共享背包模型绑定 `SelectedCompanionId`。
3. `SelectCompanion` 在城镇可编辑状态下先调用 `SetActivePermanentCompanion`，成功后再刷新当前选择；路线锁定时只允许查看并显示原因。
4. 伙伴分页状态独立保存；翻页不改变 Selected/Active。
5. 使用角色到头像资源的稳定映射，按 `bIsActive` 选择 normal/inactive。
6. 无伙伴时保持空页面可关闭，不崩溃。

## 任务 8：伙伴卡组和满员遣散/替换（RED → GREEN）

**文件：**

- 修改：`Source/GameXXK/Public/UI/GameXXKCompanionRosterWidget.h`
- 修改：`Source/GameXXK/Private/UI/GameXXKCompanionRosterWidget.cpp`
- 修改：`Source/GameXXK/Private/Tests/GameXXKCompanionRosterWidgetTest.cpp`

### 契约

- 点击伙伴 `卡组` 展示当前伙伴 12 张个人牌与 5 张编入牌。
- 切换伙伴后卡池/编入牌立即更换，不残留上一角色选择。
- 锁牌样式和主角一致。
- `遣散` 在 `< 12` 时禁用；`12/12` 但无待入队候选时仍禁用并显示原因。
- `12/12` 且存在候选时启用，点击当前伙伴调用 `ResolvePendingPermanentCompanionReplacement`。
- 被替换者出战时，新伙伴继承出战位；失败不改变完整运行时状态。

实现时复用现有 pending recruitment 原子事务，不增加任意删除伙伴的新存档路径。

## 任务 9：城镇壳体、互斥导航和图鉴不可达（RED → GREEN）

**文件：**

- 修改：`Source/GameXXK/Public/UI/GameXXKTownHudWidget.h`
- 修改：`Source/GameXXK/Private/UI/GameXXKTownHudWidget.cpp`
- 修改：`Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- 修改：`Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- 修改：`Source/GameXXK/Private/Tests/GameXXKPlayerFlowWidgetTest.cpp`
- 修改：`Source/GameXXK/Private/Tests/GameXXKTownHudWidgetTest.cpp`

### 契约

- 伙伴导航直接打开伙伴背包，`IsCompanionCodexOpenForTest()` 保持 false。
- 图鉴入口不创建，但旧存档的图鉴数据不被删除。
- 背包、伙伴、任务、商店、系统互斥。
- 进入城镇时没有大窗口和商店自动打开。
- 每个 `X` 和 Escape 正确关闭顶层窗口并恢复城镇输入。
- 局外壳体使用短元宝条；战斗/路线继续使用铜钱。

完成后运行玩家流程与城镇 HUD 目标测试。

## 任务 10：其余确认页面视觉接入与资源契约（逐页 RED → GREEN）

**文件：**

- 修改：`Source/GameXXK/Private/UI/GameXXKMainMenuWidget.cpp`
- 修改：`Source/GameXXK/Private/UI/GameXXKMetaShopWidget.cpp`
- 修改：`Source/GameXXK/Private/UI/GameXXKTaskLogWidget.cpp`（以实际文件名为准，先用 `rg --files` 定位）
- 修改：`Source/GameXXK/Private/UI/GameXXKSystemMenuWidget.cpp`（以实际文件名为准）
- 修改：`Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`
- 修改：`Source/GameXXK/Private/UI/GameXXKBattleRewardWidget.cpp`（以实际文件名为准）
- 修改：对应 `Source/GameXXK/Private/Tests/*WidgetTest.cpp`

### 顺序

1. 主菜单：终版老虎图、`霞客行`、四按钮、开始进入青山客栈。
2. 新商店：唯一入口、七商品、元宝、选择/确认/生成事务。
3. 任务日志：确认纸框、筛选/选择/追踪行为。
4. 系统菜单：确认按钮与保存/加载/设置/返回行为。
5. 战斗 HUD 与 17 选中目标态：只改表现，不改卡牌目标规则。
6. 奖励结算：铜钱、兑换比例、元宝到账和继续按钮。
7. 路线图继续使用此前已确认资源，不从当前缺失的 PSD 顶层组重建。

每一页先增加资源路径和控件存在性失败契约，再最小替换资源；禁止一次性改完整个战斗状态机。

## 任务 11：UE 资源导入和确定性验证（美术，无 TDD）

**文件：**

- 新建：`Content/Python/gamexxk_import_final_approved_ui.py`
- 新建：`Content/Python/gamexxk_probe_final_approved_ui_assets.py`
- 新建：`Saved/GameXXK/FinalApprovedUI/asset_probe.json`（运行产物）

### 步骤

1. 通过 `scripts/ue_mcp_client.py` 调用编辑器 Python，导入 `/Game/GameXXK/UI/MasterV2/Approved`。
2. 应用统一 TextureGroup、Alpha、过滤与 UI LOD 设置。
3. 保存所有新包；不覆盖用户手调角色、PaperZD、关卡或摄像机包。
4. 探针核对每个 UE 资源的源 SHA256、尺寸和目标路径。
5. UE MCP 不可用且编辑器可能有脏包时，不强关编辑器。

## 任务 12：冷编译、自动化与真实 PIE 验收

**文件：**

- 新建：`Content/Python/gamexxk_probe_final_ui_flow.py`
- 新建：`Saved/GameXXK/FinalApprovedUI/flow_probe.json`（运行产物）
- 修改：本计划的验收记录区

### 自动化

先用 `scripts/ue_tdd_pipeline.py` 完成保存、关闭编辑器和冷编译；本机 UBA 若再次出现共享内存提交失败，则使用同一 UBT 命令加 `-NoUBA`，不能把 UBA 崩溃算作测试结果。随后用 `UnrealEditor-Cmd.exe` 的 `Automation RunTests` 依次运行：

- `GameXXK.MVP.UI.CharacterBackpackModel`
- `GameXXK.MVP.UI.FinalInventory`
- `GameXXK.MVP.UI.CharacterDeckTab`
- `GameXXK.MVP.UI.PartnerBackpack`
- `GameXXK.MVP.UI.CompanionRoster`
- `GameXXK.MVP.UI.PlayerFlow`

随后执行一次完整 Editor 冷编译；不能用 `--check-only` 代替编译。

### PIE 点击链

1. `L_Main` 显示主菜单，点击开始进入 `L_QingshanInn`。
2. 城镇 HUD 可见且商店未自动打开。
3. 主角背包：切五页签、滚动、左键 Tooltip、右键替换六槽、右键卸下、分解取消/确认。
4. 伙伴背包：4 页翻动、空槽、灰/亮头像、点击即出战、伙伴六槽装备/卸下。
5. 主角卡组 12/8、伙伴卡组 12/5、锁牌和只读状态。
6. 新商店：七商品选择、购买取消、购买确认、元宝扣除和新装备/伙伴生成。
7. 任务、系统、路线、战斗选中目标、奖励结算均可点击并产生对应结果。
8. 每个窗口使用 `X` 与 Escape 关闭；任何时刻无重叠大窗口。

为每个关键状态保存截图和探针字段，并逐项与设计文档对照。只有资源检查、目标测试、冷编译和 PIE 链全部通过后，才能宣布“全部接入且功能正常”。

---

## 执行批次与检查点

- **批次 A（任务 1–4）：** 资源基线 + 主角六槽右键装备。完成后先看主角背包实机。
- **批次 B（任务 5–8）：** 页签、主角卡组、伙伴背包/卡组。完成后看两张背包实机。
- **批次 C（任务 9–10）：** 城镇导航与其余页面。
- **批次 D（任务 11–12）：** 全资源导入、冷编译和完整 PIE 验收。

每个批次只修改明确列出的文件；若发现用户正在编辑同一 PSD/UE 包，先保存或绕开，不覆盖其手调内容。
