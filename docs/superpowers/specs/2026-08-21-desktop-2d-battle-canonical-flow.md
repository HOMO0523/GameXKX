---
status: approved
owner: user
updated_at: 2026-08-21T13:00:00+08:00
supersedes:
  - default-3d-town-entry
  - desktop-challenge-route-prerequisite
---
# 纯 2D 桌面主界面到局内战斗权威流程

## 用户裁决

没有新的明确指示时，项目默认停留在纯 2D 流程，不自动打开或切换到 3D 城镇、3D 放置关卡或其他 3D 验收面。3D 青山镇继续作为可显式加载的回退/专项验收内容，但不再是编辑器、游戏启动或 `Town` 状态的默认目标。

## 权威玩家流程

1. 编辑器启动与游戏启动均进入 `/Game/GameXXK/Maps/L_DesktopTrainingHUD`。
2. `Town` 在当前产品语义中表示 2D 桌面挂机主界面；进入或返回 `Town` 不加载青山镇。
3. 桌面工作台的“挑战”不要求接受青山镇任务、不修改任务状态，也不依赖城镇 NPC。
4. 点击可挑战关卡后，在同一个 `L_DesktopTrainingHUD` 世界内关闭工作台并显示现有全屏 `UGameXXKBattleBoardWidget`；不得生成工作台内嵌战斗画布或切入 3D 地图。
5. 退出、胜利或失败后的明确返回动作恢复 `Town` 状态和 2D 工作台；地图仍是 `L_DesktopTrainingHUD`。
6. 背包、卡组、属性以及角色/伙伴/NPC、编队等桌面 HUD 规则继续服从 2026-08-20 已验收记录。

## 地图与资产边界

- `GameDefaultMap`、`EditorStartupMap`：`/Game/GameXXK/Maps/L_DesktopTrainingHUD`。
- `MapsToCook` 必须包含 `L_DesktopTrainingHUD`。
- `GameXXKLevelFlow::MapForScreen(Town)` 返回 `L_DesktopTrainingHUD`。
- `L_Main`、青山镇与其他 3D 地图保持可回退，不在本次修改中保存、覆盖或重调。
- 不修改角色 Sprite、PaperZD、相机、放置 Actor 或 HD2D Plane。

## 目标箭头热点契约

目标箭头偏移有两层独立根因，必须同时满足下面的本地坐标与贴图热点契约。

### 浮动窗口本地坐标

- PlayerController 优先读取 viewport-client 的 DPI 坐标；该坐标不含浮动 PIE 窗口相对桌面的左上角。
- viewport-client 坐标通过 `BattleHudSafeStage` 的 `Offset/Scale` 转为 1920×1080 stage 坐标。
- `NativePaint` 通过 `SafeStage.Offset + StagePosition × SafeStage.Scale` 回到 Board-local。
- 禁止 `StageGeometry.LocalToAbsolute -> AllottedGeometry.AbsoluteToLocal`。用户截图中的整段蓝色偏移约等于浮动窗口原点 `(124,109)`，正是这次跨 Geometry 往返混用了桌面绝对原点与客户区原点。

### 可见箭尖热点

- 源图尺寸：`1254 × 1254`。
- 透明边界实测：`(285,205) - (1082,1031)`。
- 可见右侧箭尖热点：`(1082,608)`；右缘非透明像素的 Y 范围为 `605..610`。
- 运行时箭尖热点按 `ArrowSize` 分别缩放。
- 未旋转框体左上角等于 `PointerEnd - ScaledTipHotspot`。
- `MakeRotatedBox` 必须显式围绕同一个本地热点旋转，因此方向只改变旋转，不改变箭尖与鼠标的重合关系。
- 黑色墨点曲线、卡牌命中判断、合法目标和坐标换算不在本次重做范围内。

## 验收

- 静态合同测试锁定两个默认地图配置、Cook 地图与 `Town` 解析。
- 箭头几何测试覆盖水平、垂直、对角方向，均要求 `TopLeft + TipHotspot == PointerEnd`。
- 冷 UBT 与相关 Automation 全绿。
- 真实浮动 PIE 从默认 2D 地图进入挑战，地图不变、任务不变、BattleBoard 可见，退出后工作台恢复。
- 至少在三个窗口尺寸和三个不同桌面窗口原点下检查可见箭尖与真实鼠标热点重合。
