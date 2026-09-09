# 竖向六帧落雷接入

## 范围

- 使用已确认的 `yellow-thunder-keyframes-v3` 六帧，保留笔触和固定落地点；去纸底、顶部渐隐。
- 复用战斗受击队列及异步纹理加载，不使用 Python 预览循环驱动正式效果。
- `LightningPerTargetStatusSnapshot` 的实际伤害包携带仅用于表现的 `bLightningStrike`。不按角色名称或玩家可见卡名猜测。
- 适用于相同效果操作的主动出牌、任务重放及 NPC 阵赏；闪避不播放命中效果，火焰与普通攻击维持原效果。
- 每次实际落雷播放六帧，0.30 秒；队列时长覆盖完整尾帧。数值与目标解析不变。

## 素材

- `SourceArt/UI/Battle/VFX/VerticalLightning/manifest.json`
- `/Game/GameXXK/UI/Battle/VFX/VerticalLightning/T_VerticalLightning`
- 为兼容现有材质使用 8×8 格，前六格有内容，单格 256×280，其余格透明。

## 验证状态

- 六张透明 PNG 与图集已生成；已目视检查纸底和落点。
- 冷编译通过：`Saved/Codex/vertical-lightning-build.log` 与图集规格收尾编译 `vertical-lightning-grid-build.log`。
- 自动化报告 `Saved/Automation/VerticalLightning_20260908/index.json`：19 通过，2 项旧动画测试失败。失败断言涉及旧主角攻击路径、帧率、死亡片段及旧节奏常量；本次未改这些行为，未重写其断言。新增落雷/火焰区分、事件传递及尾帧检查通过。
- 真实 PIE 通过“雷序引霆”主动出牌触发，`Saved/Codex/vertical-lightning-live.json` 记录到 0–5 全部帧，末尾恢复隐藏。截图 `Saved/Codex/vertical-hit-0.png`。
- 仅主动出牌经过本次目视验证；重放和 NPC 走同一效果分支，尚未单独录制。闪避排除由队列条件保留，未额外制造闪避场景。
- 后续接入：`GameXXKBattleLightningUltimate.cpp` 在实际 TaskReward 落雷伤害包前播放 057，同一批同一角色仅一次；其余重放伤害先播完。无落雷伤害的阵赏不触发此大招。
- 放大：六帧显示尺寸 640×700（+25%），以原图 (256,510) 命中点锚定目标展示中心。
- 最新冷编译 `lightning-task-integration-build-fixed.log` 通过；`Saved/Automation/PartnerLightningTask_20260908/index.json` 60 项全部通过。
- 法师伙伴实际主动完成四张雷牌加“万法归一”，`Saved/Codex/partner-task-lightning-live.json` 记录约 7.16 秒开始全屏大招、七张图集均采样、之后继续六帧落雷（0–5 均观察到）。验证使用临时 50000 气血敌人和 5 气力，正式数值未改。
- PIE 留在 4/5 的任务验证快照，用户点击“万法归一”即可重复验收；测试结束可 `session.restore` 返回原进度。
