# 战斗投影 HUD 运行时修复设计（方案 A，已被固定槽位设计取代）

> **历史记录：** 用户随后明确表示战斗角色本身是固定编队位置，资源 HUD 也必须和手牌区一样使用固定屏幕槽位，不能继续跟随世界投影。当前有效设计见 [battle-fixed-slot-hud-design.md](2026-07-21-battle-fixed-slot-hud-design.md)。本文件保留仅供诊断历史参考，不再作为实现依据。

## 已确认目标

战斗中的角色血量、内力和状态 HUD 使用**普通 UMG**，由 Battle Board 统一管理；不回退到角色 Actor 的 WidgetComponent / TextRender。

用户已选方案 A：每名存活角色的 HUD 跟随其在屏幕中的角色位置，窗口尺寸、DPI 缩放、相机和角色位置变化后仍与角色对应。手牌、敌方意图、共享气力和结束回合仍属于固定屏幕区域。

真实 PIE 截图已证明当前缺口：卡牌、气力、结束回合和敌方意图已渲染，但主角与怪物脚下没有 HP/MP/状态 HUD。

## 视觉与素材规则

- 已核实 `clean_assets_v2/049.png`（角色气血）与 `050.png`（角色内力）是 420×32 的完整合成条；源 PSD 的对应层也没有独立轨道或填充。不能再错误地把它们当作“空框”并叠一层纯色，需按源图制作并导入命名明确的独立 Track/Fill 切图，再供 ProgressBar 使用。
- 角色气血的原始色相是低饱和墨绿，角色内力是赭红/朱砂色；不能把内力替换成惯例蓝色。现有 `T_TownPsd_CharacterHealthFrame` 与 `T_TownPsd_CharacterManaFrame` 都是上述合成图，而非真正 Frame。
- HP/MP 都以对应 PSD 条的原有色相、纸纹和描边为准；不因常规游戏约定擅自改成纯红或纯蓝，也不再使用 `FSlateColorBrush` 作为最终填充。
- 角色行显示 `当前气血 / 最大气血` 与 `当前内力 / 最大内力`；敌人不显示内力行。
- 状态图标保持独立、紧随资源条下方，层数数字与 Tooltip 行为不变。
- HUD 以角色下方为默认位置；若会碰到手牌区、共享气力、结束回合或屏幕边缘，则按优先级向上、向内偏移，不覆盖这些固定交互区。
- 所有材料来自项目已存在的 PSD 切图；只有确认缺少独立 Track/Fill 时才补制同源切图，不能用临时纯色代替最终样式。

## 运行时数据与坐标链路

1. `AGameXXKBattleSceneUnitActor` 只提供角色视觉、可点击区域、受击反馈和稳定 `UnitId`。
2. `AGameXXKMVPPlayerController` 在**战斗期间**每帧为每个存活单位投影一个可用的世界位置到当前 viewport。优先使用可验证的角色视觉中心 / 底部来源；若视觉 Bounds 的底点不可投影，使用可投影的 Actor 基点加屏幕 Y 偏移。
3. `UGameXXKBattleBoardWidget` 接收 `UnitId → 屏幕锚点`，把资源/状态复合 Widget 放入 `BattleProjectedUnitHudLayer`。它不重新创建子 Widget，也不重载贴图。
4. Board 将本帧已成功应用的锚点与 viewport/DPI 版本绑定。Controller 与 Board 的 Tick 顺序偶发错位时，可保留同一 viewport 版本的最后一个已验证锚点一帧；发生 viewport/DPI/父层无效、单位死亡或连续投影失败时立即折叠，不能跨窗口尺寸保留旧坐标。
5. Board 每帧只更新存活单位的位置与必要的避让偏移。窗口变化由当前 Board 几何和当前 viewport 尺寸自动触发新的投影/布局；静态时不会重建 UI 或创建临时对象。

## 当前真实问题的诊断先行要求

当前 `screen_rect_missing` 不能直接当作 HUD 不存在的根因：探针连已可见的手牌、气力和结束回合也读不到 Slate geometry。修复分两步：

1. 在 Controller 记录每个单位中心点/底点的投影是否成功、世界位置和结果坐标；在 Board 记录本帧输入锚点、实际 Slot 偏移、可见性、Canvas 尺寸与 viewport 版本。
2. 修复 `gamexxk_probe_real_play_flow.py` 的几何读取诊断：保留具体异常、cached/local/absolute geometry 数据，而不是吞掉异常后统一返回 `None`。

只有诊断确认是投影点失败或 Tick 顺序缺口后，才对相应源头做最小修复。

## 验收与测试

- C++ 自动化：真实 Controller → Board 投影桥在正常相机下为 Party 与 Enemy 产生非零、可见且对应 `UnitId` 的锚点；延迟一帧的输入不会错误折叠 HUD；viewport 版本变化会使旧锚点失效并重投影。
- 资源 Widget 自动化：HP/MP 样式的 frame 与 fill 都绑定项目 Texture2D；敌人内力行折叠，角色内力行可见；数值、百分比、护甲和状态保持正确。
- 探针自动化：可见 HandCard、Qi、EndTurn 与 Unit HUD 均返回 geometry 或明确的读取错误，不允许无声 `None`。
- 真实 PIE：主菜单 → 城镇 → 任务 → 路线 → 战斗后，HUD 的最后已应用锚点、可见性和截图均通过；再用不同窗口宽高/DPI 条件验证角色与 HUD 不漂移、也不压住手牌/气力/结束回合。
- 战斗行为不变：目标箭头、敌方意图、出牌、敌方回合与状态结算不被本次 HUD 改动改变。

## 非目标

- 不把单位 HUD 改回 Actor 组件。
- 不更改现有卡牌、伤害、内力、气力或状态规则。
- 不使用新生成的美术替代项目已有 PSD 切图。
