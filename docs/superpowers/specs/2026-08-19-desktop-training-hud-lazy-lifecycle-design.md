# GameXXK HUD-only 懒加载与折叠休眠设计

日期：2026-08-19  
状态：已确认
范围：`L_DesktopTrainingHUD` 的 UI 创建与折叠生命周期；不切换默认 3D 城镇入口

## 1. 问题与基线

当前 HUD-only 编辑器运行（`UnrealEditor -game /Game/GameXXK/Maps/L_DesktopTrainingHUD`，1672×941）在约 20 秒时为 `Working Set 1060.5 MiB / Private 1248.8 MiB`，但约 50 秒上升到 `3447.3 MiB / 5008.8 MiB`。增长主因不是第一章 1K 战斗图集；当前 32 张 1024×1024 BC7 atlas 每张 UE resource 为 1 MiB。主因是 `AGameXXKMVPPlayerController::EnsurePlayerFlowWidgets` 在 HUD 地图仍一次性创建主菜单、TownOverlay、路线图、BattleBoard、世界地图、旧背包、商店、伙伴、任务/事件/遗物等完整玩家界面。

折叠背包当前会重建工作台可见树，但没有一条明确的“延迟释放强引用、取消过期异步请求、执行一次安全 GC”的休眠边界。因此视觉折叠不等价于可验证的内存回收。

## 2. 冻结目标

1. `L_DesktopTrainingHUD` 启动时只创建并显示 `UGameXXKDesktopTrainingWorkbenchWidget`；顶部挂机条立即可用。
2. 其它玩家界面不在 HUD 启动阶段创建。只有明确交互请求对应界面时才单独创建；不得因为一次点击回退到“创建全部玩家界面”。
3. 普通游历下折叠背包后立即只显示顶部挂机条和 Tab；3 秒内重新展开取消回收，3 秒后释放折叠界面的重资源并请求一次安全 GC。
4. 折叠不停止、不暂停、不重置游历。`UGameXXKMVPSubsystem`、`FGameXXKTrainingTravelRuntime`、顶部条当前三人/敌人所需 1K atlas、Tab 与小型公共纸框继续保留。
5. 默认 3D 城镇、路线、局内战斗的原有启动行为保持不变；本设计不得改变 `MapForScreen(Town)`，不得保存或修改 `L_Main.umap`。

## 3. HUD-only 启动策略

在 PlayerController 增加纯策略判断：

- `DesktopTrainingOnly`：当前包名匹配 `L_DesktopTrainingHUD`。
- `FullPlayerFlow`：其它地图以及无 World 的既有自动化夹具。

`DesktopTrainingOnly` 的启动路径只执行：

- 创建/加入 viewport：桌面历练工作台；
- 初始化窗口规则、输入与顶部挂机条；
- 不创建 MainMenu、TownOverlay、TownHud、WorldMap、RouteMap、全局 BattleBoard、InventoryWindow、MetaShop、CompanionRoster、QuestDialog、RouteEncounter、RouteMerchant、RelicBar、TaskPanel。

现有 `EnsurePlayerFlowWidgets` 拆成可独立调用的 ensure 单元。完整地图仍调用全量协调入口；HUD-only 页面点击商店、邮件或以后新增页面时，只调用对应 ensure 单元。工作台自己的嵌入式背包、工具、历练地图和挑战 BattleBoard 仍由工作台按当前页面创建，不依赖全局旧窗口。

## 4. 折叠休眠边界

### 4.1 立即发生

点击 Tab 折叠时：

- 取消并回滚鼠标吸附道具；
- 把工具 3×3 中未执行的条目退回原容器；
- 隐藏中央背包、左仓库和右地图/工具页；
- 保留顶部挂机条、Tab 和连续游历 Tick；
- 启动 3.0 秒单次休眠倒计时。

### 4.2 三秒内重新展开

- 取消休眠倒计时；
- 不执行 GC；
- 恢复原页面状态，避免快速查看背包时反复卸载/重载。

### 4.3 三秒到期

- 取消中央/侧栏已发出的异步资源请求并使旧回调 token 失效；
- 释放嵌入式背包、仓库、工具、历练地图、Tooltip、头像/道具图标和挑战画布的强引用；
- 释放非顶部挂机条需要的纹理 cache pin；
- 保留顶部条当前阶段所需的 walk、Idle/Attack/Hit/Death 1K atlas；
- 在帧尾请求一次非强制全清的安全 GC，不在点击当帧同步 `CollectGarbage`；
- 设置 `bCollapsedResourcesReleased`，防止每帧重复请求回收。

## 5. 状态保存与恢复

折叠前把以下轻量状态保存在工作台，而不是保存在即将销毁的子 Widget：

- 当前主角/伙伴/NPC owner；
- 主角/伙伴/NPC 入口页；
- 属性/装备/卡组子页；
- 仓库页码、背包/仓库滚动位置；
- 当前右侧页类型、工具模式；
- 尚未点击“应用卡组”的临时卡组选择。

重新展开时先恢复纸框与交互占位，再异步载入头像、道具图标和非当前 atlas。游历数值状态只从 subsystem 读取，不从旧 Widget 恢复。若异步资源未完成，按钮暂时禁用并显示已有项目纸框占位，禁止出现空引用点击。

主动挑战期间不进入普通背包休眠流程：挑战画布是当前主动玩法。若未来允许挑战最小化，必须另立独立状态机，不在本任务内把挑战伪装成游历后台。

## 6. 资源策略

- 顶部挂机：保留当前编队、当前敌人编制和走路背景所需 1K 资源。
- 常用小资源：Panel/Tab/Slot/关闭/排序等 MasterV2 小纹理可保留暖缓存。
- 重资源：角色头像池、物品图标页、Tooltip 内容、BattleBoard、非当前角色/敌人动作 atlas 在休眠后释放。
- 新代码不得继续在 HUD-only 启动路径同步 `LoadObject` 全套页面资源；使用 soft path、按页异步加载与 generation token。

## 7. 失败与竞态处理

- 折叠后到达的旧异步回调必须因 token 不匹配而丢弃，不能重新把已释放纹理挂回 Widget。
- 展开与三秒到期同帧竞争时，展开优先：取消回收并创建新的 generation。
- GC 前确认没有携带事务、工具保留项或挑战视觉 session。
- 页面载入失败时保留可交互外壳、禁用依赖资源的动作并显示简短错误，不关闭游历。

## 8. 验收与重新测量

自动化必须证明：

1. HUD-only 启动只存在工作台；其它全局窗口均未创建。
2. 其它地图的完整玩家界面回归不变。
3. 折叠 2.9 秒时尚未释放；3.0 秒到期后重资源引用为空，顶部条仍存在且 Tick/走路帧/战斗步骤继续前进。
4. 三秒内展开不会 GC；三秒后展开只创建一份页面，不重复绑定 delegate 或异步请求。
5. 折叠时携带物和工具条目正确回滚，临时卡组/页签/滚动状态恢复。
6. 冷 UBT、Training、Workbench、FinalInventory、PlayerController flow 与 SaveGame 聚焦回归通过。

实测使用与当前基线完全相同的命令、分辨率与等待时间：记录约 20 秒启动值以及至少 50 秒稳定值的 Working Set/Private Memory。报告必须同时给出修改前 `1060.5/1248.8 MiB` 启动值与 `3447.3/5008.8 MiB` 稳定值，不能只选择最低瞬时值。若稳定内存仍明显上升，继续追踪剩余强引用，不以“界面已隐藏”判定完成。

## 9. 非目标

- 不切换默认 3D 城镇入口；
- 不删除 3D 城镇、旧 UI 资产或用户地图调整；
- 不在本任务中重做视觉稿、PSD 或战斗数值；
- 不用 Shipping 包数据替代本次明确要求的编辑器 HUD-only 内存测试。
