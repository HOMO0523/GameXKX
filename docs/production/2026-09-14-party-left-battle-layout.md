# 我方在左、敌方在右：战斗与挂机显示改造

基点：`9fccc526ea123d499b7dc2a0f666c5519c3861b6`，根项目 `main`。

## 范围与约束

- 局内与挂机统一我方左、敌方右；只改变显示位置与方向，保留 UnitId、阵位编号、行动顺序、伤害与存档语义。
- 通过角色图像层翻转复用原图，保留用户校准的尺寸、透明边界和脚底锚点；文字、卡框、资源条正常阅读。
- 覆盖局内阵位/HUD/目标代理、敌意图与详情、结果预览、独立战斗特写、受击后退、雷系大招，以及挂机行走/交战/灰狼扑击/循环背景/波次进度。
- 只使用纯 2D 默认地图 `/Game/GameXXK/Maps/L_DesktopTrainingHUD` 验证，不操作其他任务正在调整的角色美术。

## 执行顺序

1. 添加或更新针对左右显示的自动化断言，冷编译后记录预期失败。
2. 调整共用受击方向及局内显示。敌意图移到右上；工具按钮移到左上空位，保留地势/战报区域。
3. 调整挂机角色和血条坐标、朝向、循环背景与波次展示。7/21 波使用相同顺序规则。
4. 冷编译，运行受影响的布局/动画/目标选择/挂机自动化；将旧失败与本轮回归区分。
5. 在隔离测试存档下验证纯 2D 挂机、战斗与返回，记录截图；保存验收结果并仅提交本任务文件。

## 验收

- 三人/三怪朝向相对，HUD 和点击区域对应正确 UnitId；特写和恢复阵位不改变朝向。
- 受击远离对手；挂机灰狼朝向对手扑击，死亡仍为原淡出效果。
- 挂机向右行走，背景向左循环且无空隙；波次从左向右推进，不改实际遭遇顺序。
- 指向箭头继续只用 viewport-client/SafeStage 本地坐标；真实点击与视觉目标一致。
- 卡片/文字无镜像，右上敌意图与工具按钮不重叠；不同窗口尺寸保持可用。
- 原美术、玩家存档和非本任务修改保留。

## 结果

已完成本次显示改造，原始美术与战斗规则未修改。

### 实现

- 局内六个阵位中心左右对调，HUD 与透明点击代理共用原来的阵位映射；只在角色子图像上使用负 X 缩放，保留所有纵向和体型校准。
- 敌方特写中心为 1330，我方为 590（1920 设计宽）；受击后退相应反向。群体预览移到敌方半场。
- 意图卡在右上按视觉 3P→2P→1P 排列，死亡后保留原编号；底层意图索引与执行顺序不变。按后续附图调整，“自动/关闭”最终在右侧 (1722,36) 竖排，卡牌层级低于立绘（详见末尾追加）。
- 挂机图像与血条按 953 逻辑宽反射坐标，血条自身不翻转。行走和战斗均翻转角色，灰狼程序扑击改向左。
- 三张背景从 X=0/750/1500 开始向左移动，覆盖整个循环而不露空；7/21 波按照实际遭遇顺序从左向右展示和填充。
- 雷系独立演出在图像层镜像整幅画面，保留材质内部裁切、偏移和非对称云层遮罩的原校准。当前正式队列未设置该演出触发标记，本轮没有新增触发语义，也不宣称实播了此独立演出。

### 验证证据

所有本地报告位于 `Saved/PartyLeft/`。

| 验证 | 结果 |
|---|---|
| 修改前定向运行 | `Red/index.json` 记录原朝向、后退、背景与波次断言失败；局内夹具随后补齐显示会话初始化 |
| 冷 UBT | `build-green.log`、`build-final.log`、`build-final-confirm.log`、`build-fixture-confirm.log` 均成功；没有 Live Coding / Hot Reload |
| 最终定向 | `FocusedFinal/index.json`：3/3 通过，覆盖角色子图像朝向、特写恢复、后退、HUD/代理/卡牌起点、循环接缝及 7/21 波。21 波夹具使用正常讨伐令授权入口 |
| 同范围基线 | `Baseline/index.json`：157 项，140 通过、17 失败，使用未改运行时代码的二进制实跑 |
| 回归对账 | `Final/index.json` 加 `FocusedFinal/index.json` 的对应定向重跑，合计 160 项，143 通过、17 既有失败；`comparison.json` 逐项比较错误内容及次数，新增错误为 0；原始报告均保留 |
| 箭头专项 | `pointer-mcp-results.json`：TargetArrowAlignment、TargetPointerViewportCoordinates 2/2，无错误或警告 |
| 真实鼠标三阵位 | 浮动 PIE 中点击攻击卡后点公鸡、狸猫、山羊；对应 HP 分别 74→61、76→60、90→79，其他敌人不被误选，见 `Live/native-*-hit-observe.json` |
| 特写与恢复 | `Live/cinematic-samples.json` 记录 6 个特写采样，双方锚点 X 为 .3072917/.6927083，敌方向右后退采样最大 127.61；实机观察攻击后恢复新阵位 |
| 返回挂机 | 实际点击关闭及退出按钮，同一 `UEDPIE_0_L_DesktopTrainingHUD` 返回 Walking/EncounterIdle，战斗控件隐藏；随后还原 Dev 会话，见 `Live/native-return-observe.json`、`Live/restored-restore.json` |
| 玩家保护 | `player-save-hashes-after.json`：原 42 份玩家存档 SHA256 全部未变；测试使用独立 UserDir |
| 清理 | 停止 PIE，通过 MCP 保存，脏包 0；关闭本轮创建的隔离编辑器 |

### 图像证据

- `Saved/PartyLeft/Live/battle-1920.png`：同一个真实 PIE 战斗控件的 1920×1080 导出。
- `Saved/PartyLeft/Live/battle-cinematic.png`：真实主角攻击特写。
- `Saved/PartyLeft/Live/idle-1920.png`、`idle-return-1920.png`：真实挂机交战与返回后行走的控件导出，透明区域衬中性底。
- Computer Use 另观察实际浮动窗口并执行点击，未将控件导出冒充操作系统截图。

### 保留边界

- 17 项既有失败涉及通知/关闭栈、旧卡牌/奖励/动画/HUD 断言；没有削弱它们来制造全绿，完整名单见 `comparison.json`。
- 未打 Shipping 包、未逐套实播全部 34 套外观、未进行 3D/旧城镇回归。
- 其他任务新增的 `SourceArt/Characters/cast-with-npcs-20260914/` 内容保留在原工作区，不纳入本任务提交。

## 追加：卡面插图左右翻转

按用户追加要求，共用 `UGameXXKCardPortraitImage` 的 `T_CardPortrait_*` 插图在自己的绘制矩形中心水平翻转；覆盖手牌、敌意图/展示牌、奖励/待选牌及复用控件的卡组、商店等卡面。卡框、名称、费用和其他文字不翻转，不修改纹理资产。

圆角遮罩仍读取插图到原卡面的相对绘制变换。首轮视觉复核发现多补了一次图宽平移，使插图被裁掉；移除额外平移后，使用 `MakeChild` 自带的中心枢轴完成镜像。已保留失败几何记录，并在探针中增加两端均位于卡面水平范围内的断言。

- `Saved/CardArtMirror/build-fixed.log`：修正版冷 UBT 成功，无 Live Coding / Hot Reload。
- `painted-card-mask-check.json` 与 `selected-card-mask-check.json`：实际 PIE 的 5 张手牌、3 张敌意图均为负水平绘制轴，使用真实卡面几何且两端不越界；选中放大状态同样通过。
- 已检查实际浮动窗口与 1920×1080 控件导出；卡图、文字及圆角正常显示。
- 最终截图：`Saved/PartyLeft/Live/card-art-mirrored-final.png`，放大截图：`card-art-mirrored-selected.png`。
- 当前 43 份现有玩家存档哈希保持，见 `Saved/CardArtMirror/player-saves-after.json`。隔离 Dev 会话还原、PIE 停止、MCP 保存脏包 0。
- 本次是显示追加，未重复整套数值/战斗回归，也未更新 Shipping 包。

## 追加：右侧竖排按钮与卡牌后置

按用户标注图将“自动”置于“关闭”上方，容器改为 VerticalBox，设计位置 (1722,36)、尺寸 186×168，每个按钮保持 186×60，垂直间隔 48。右边距 12，与最后一张意图卡不相交。

普通敌方意图卡及放大展示卡改为 `BattleDesignStage` 的直接子节点，Z=9；角色立绘同父级 Z=10，攻击特写 Z=40。避免卡牌仍在 Z=20 的 Controls 父容器内、仅降低局部 Z 却继续盖住怪物。悬停详情与操作控件继续使用原控制层。

- `Saved/BattleRightToolbar/build.log`：冷 UBT 成功。
- `automation-results.json`：7/7 通过，覆盖竖排工具栏几何、退出/取消、演出锁、阵位/卡牌层级、死亡编号及输入桥接；保留原有诊断 warning。
- `live-layout.json`：真实 PIE 的容器为 VerticalBox，顺序 Auto/Close；普通/放大敌方卡 Z=9，六个立绘同父级 Z=10，检查通过。
- 实际鼠标点右侧“关闭”打开确认框，“继续战斗”取消，点右侧“自动”显示“自动战斗：开”并开始出牌。实机及导出画面确认羊角等立绘遮挡卡面下沿。
- 截图：`Saved/PartyLeft/Live/right-toolbar-behind-cards.png`。
- 同一编辑器先执行 ControllerInputBridge 再运行 PIE 时，停止阶段出现 `/Engine/Transient.Texture2DArray_0` 引用断言。该旧测试在 `GetTransientPackage()` 中 `CreateWorld(EWorldType::PIE)`，引擎会标记该包为 PIE；记录保留在 `mixed-automation-pie-stop.log`。新进程不预跑自动化，重复战斗、自动、还原、停止后通过，见 `clean-process-lifecycle.json`，未改动这项既有夹具。
- 现有 43 份玩家存档哈希不变；新进程 PIE 停止、MCP 保存脏包 0。未更新 Shipping 包。

## 追加：遗物栏移到左上并避开战报

按照用户红框，遗物栏在统一的 1920×1080 安全舞台内放到 (352,36)，尺寸维持 368×116，六列两行，超过十二件仍可滚动。独立遗物层增加与战斗相同的居中 ScaleToFit 布局，随分辨率、DPI 和留黑一起缩放；满屏容器均为 SelfHitTestInvisible，不拦截外侧战报或战斗控件。

遗物区域为 (352,36)～(720,152)。收起战报最右端 X=246，水平留空至少 106；展开战报从 Y=334 开始，垂直留空 182。因此战报原有位置、尺寸、展开方式和点击功能无需改变。

- `Saved/RelicBarPlacement/build.log`：冷 UBT 成功。
- `automation-results.json`：SixColumnWrapAndTooltip、FullscreenRootDoesNotBlockBattleClicks 2/2，通过位置/两行布局/战报避让/全屏输入透明断言，无错误或警告。
- 隔离 Dev 会话实际取得 13 件遗物，并真实结算攻击生成战报；普通与展开态可见，布局探针 `compact.json`、`expanded.json` 均不重叠。比较使用共用设计空间；非 Tick 遗物控件的缓存几何可能为零，没有拿零矩形充当屏幕测量。
- 完整 Slate 窗口截图：`Saved/RelicBarPlacement/compact.png`、`expanded.png`，包含独立遗物层和战报，已做视觉检查。
- 现有 43 份玩家存档哈希不变；Dev 会话还原、PIE 停止、脏包 0，并正常关闭隔离编辑器。未更改遗物规则、原始美术或 Shipping 包。
