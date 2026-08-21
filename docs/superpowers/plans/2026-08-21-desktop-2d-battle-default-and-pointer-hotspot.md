# 纯 2D 默认流程与箭头热点修复执行计划

## 目标

把已验收的“2D 桌面工作台 → 直接挑战 → 全屏 BattleBoard → 返回工作台”设为项目默认流程，修复目标箭头可见箭尖没有吸附真实鼠标的问题，并把当前进程、剩余计划和 Git 远程同步到 `origin/main`。

## 执行步骤

- [x] 增加默认地图、`Town` 解析和箭尖热点的失败回归测试。
- [x] 将 `DefaultEngine.ini` 与 Cook 地图切到/纳入 `L_DesktopTrainingHUD`。
- [x] 将 `MapForScreen(Town)` 切为 2D 地图，同时保留 3D 地图的显式识别能力。
- [x] 以源图 `(1082,608) / (1254,1254)` 定义可见箭尖热点，围绕热点旋转。
- [x] 更新 `AGENTS.md`、操作指南、滚动状态与本轮验收记录。
- [x] 冷 UBT、聚焦 Automation、默认 2D 真实 PIE、三分辨率箭头视觉验收。
- [x] 复核保护地图哈希，精确暂存本轮与此前已验收但未提交的 2D HUD/BlockShield 范围。
- [ ] `git fetch` 后普通提交并推送根项目 `main` 到 `origin`；本地提交已完成，远程认证恢复后只剩普通 push。

实现提交：`91786825ee8040cbb4683fbf7798998e1f26210f`。本计划与验收元数据由紧随其后的本地文档收尾提交承载。

## 不做

- 不保存或重调任何 `.umap`。
- 不修改 3D 场景、角色 Sprite、PaperZD、相机或 HD2D Plane。
- 不把旧工作台内嵌 ChallengeViewport 恢复为玩家路径。
- 不让挑战隐式接受青山镇任务。
