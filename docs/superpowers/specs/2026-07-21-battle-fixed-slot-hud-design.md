# 战斗固定槽位 HUD 设计（取代投影跟随方案）

## 决策

战斗角色已经使用稳定的六个编队显示槽位：敌方 1P/2P/3P 与我方 1P/2P/3P。单位资源 HUD 不再从 Actor 世界坐标投影，也不再因 Controller 与 Slate 的 Tick 时序、相机或窗口 DPI 变化而重新取点。

HUD 改为由 `UGameXXKBattleBoardWidget` 依据权威的 `FGameXXKBattleUnitHudView.Side` 与 `SlotNumber` 绑定到固定 UMG Canvas 槽位；这与底部手牌区、右下共享气力和结束回合使用的屏幕锚点布局相同。

## 槽位契约

- 仅接受有效、存活的固定战斗槽位（Side 为 Party/Enemy，SlotNumber 为 1 至 3）。我方的权威角色顺序为：招募伙伴 = 我 1P、主角 = 我 2P（中间）、任务 NPC = 我 3P；不能再由数组下标或临时 HUD 代码反推。
- 同一 Side + SlotNumber 在一场战斗内代表同一个固定视觉位置；单位刷新、出牌、状态变化只更新数据，不重新计算屏幕投影。
- 每个 HUD 使用固定尺寸和归一化 Canvas Anchor，中心对齐。窗口拉伸时由 UMG Anchor 与 DPI 统一重排，HUD 与所属编队槽位保持同一相对位置，不使用像素缓存或手动 DPI 系数。
- 固定槽位必须处于一个居中的 16:9 战斗安全舞台中；战斗相机使用相同 16:9 AspectRatio 约束（不改 transform/FOV）。只固定 UI 或只锁相机都会在超宽/窄窗口使 UI 与角色脱节。
- 固定槽位要避开顶部意图牌和底部手牌/共享气力/结束回合的安全区；HUD 不能在 Tick 中临时上移、绕障或跨槽。
- 单位死亡、退出当前战斗或失去有效固定槽位时，移除其 HUD；复活或新增时由同一稳定槽位重建。

## 交互边界

- 卡牌目标箭头仍可使用单位中心的世界投影，因为箭头必须指向实际角色；它与资源 HUD 布局完全分离。
- 不改角色 Actor，不添加 WidgetComponent，不改相机、战斗规则、出牌、敌方意图或状态结算。
- `RegisterBattleUnitHudScreenPosition`、HUD 脚底投影缓存和每 Tick `RefreshProjectedUnitHudPositions` 不再参与生产 HUD 布局。过渡 API 若保留，仅可作为弃用的测试兼容层，不能决定可见性或位置。

## 验收

- 在同一场真实 PIE 战斗内，所有存活单位有可见 HUD，且 `Side + SlotNumber` 对应稳定的 Canvas Anchor。
- 以至少两种不同 viewport 尺寸/纵横比验证：每个 HUD 的 Anchor、Alignment、尺寸和相对槽位不受世界投影数据影响；不出现零锚点、折叠或横向漂移。
- 角色 HP/内力/状态的真实视觉重组是后续独立任务；本任务只稳定固定槽位定位，不能用平面色块或新美术替代。
