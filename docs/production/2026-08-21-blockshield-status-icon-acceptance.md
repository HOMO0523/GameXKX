---
status: implemented
owner: codex
updated_at: 2026-08-21T13:09:02+08:00
source_commit: 91786825ee8040cbb4683fbf7798998e1f26210f
working_tree: clean for this scope after documentation follow-up; unrelated user files remain untracked
---
# 2D 局内战斗格挡图标与中央问号验收

本记录只验收桌面挂机主界面进入局内 BattleBoard 后的格挡状态显示，不涉及 3D 城镇、场景、角色 Sprite、PaperZD、镜头或 HD2D Plane。

## 用户裁决

- 最终图采用 `BASE.UI.STATUS.BLOCK_SHIELD v08`；用户原话：`这个不错`。
- 攻击轨迹必须从左侧进入，在盾面接触点以连续弧线向上折射；禁止直角、折角和 L 形箭头。
- 盾面采用低饱和蓝灰色侧盾，保持项目既有宣纸、墨线与状态图标裁切规范。

## 资产落位

- 源图：`SourceArt/UI/Battle/StatusIcons/battle_status_block_shield_inkflat_v4.png`
- 候选记录：`SourceArt/Generated/Candidates/BASE.UI.STATUS.BLOCK_SHIELD/v08/README.md`
- UE 资产：`/Game/GameXXK/UI/Battle/StatusIcons/T_BattleStatus_BlockShield`
- 源图 SHA-256：`FD3A1FA3D56DFB9C78800B1D20AE96804F2A7CE97F3A61C212B63E0D15BAFA56`
- UE 资产 SHA-256：`5E911D6ED7E99B71E5CE0B445DCE6B58FFF358513AB1F7276C5F7B5A758E5CA3`
- 导入属性：1254×1254、RGBA、UI LOD、NoMipmaps、EditorIcon、Clamp、sRGB、NeverStream、保留 Alpha。

## 中央问号根因与修复

中央 `? ×1` 不是 BlockShield 导入失败，而是已经退役的 `BattleCinematicStatusIcon` 生命周期错误：Board 先把它设为 `Hidden`，随后子控件 `NativeConstruct()` 再次进入 `EnsureWidgetTree()`，无条件把外层恢复为 `SelfHitTestInvisible`。空 BadgeModel 因此用未知状态 fallback 绘出问号和默认层数 1。

`UGameXXKBattleStatusIconWidget::EnsureWidgetTree()` 现在只会把普通可见状态规范为 input-transparent；对调用者明确设置的 `Hidden` / `Collapsed` 保持不变。普通角色脚下状态栏行为不变，退役中央控件不会再复活。

## 纯 2D 实机链路

通过 UE 5.8 MCP 在浮动 PIE 中执行：

1. 加载 `/Game/GameXXK/Maps/L_DesktopTrainingHUD`。
2. 直接点击挑战，保持 `QuestState=NOT_ACCEPTED`。
3. 验证 `Screen=Battle`、BattleBoard 可见，地图仍为 `L_DesktopTrainingHUD`。
4. 瞬态打出 `Profession.Guard.BuDongRuShan`，主角获得 `Block 3`。
5. 角色脚下渲染模型返回 `IconId=BlockShield`、`displayed_stack=3`，目标纹理可加载。

修复前后证据：

- 修复前中央空问号：`Saved/Codex/blockshield_2d_training_battle_20260821.png`
- 修复后完整 2D 战斗：`Saved/Codex/blockshield_2d_training_battle_fixed_20260821.png`
- 修复后 BlockShield 运行时放大裁切：`Saved/Codex/blockshield_2d_training_battle_fixed_crop_20260821.png`

修复后画面中央不再有 `? ×1`；主角脚下第三枚状态图标显示用户认可的弧形折射箭头与侧盾，层数为 3。

## 验证结果

- 冷 UBT：`GameXXKEditor Win64 Development -NoHotReload -NoHotReloadFromIDE -NoUBA -MaxParallelActions=2`，`Result: Succeeded`。
- Automation：`GameXXK.UI.Battle.Status`，2/2 succeeded、0 failed、0 errors；`Saved/Automation/BlockShieldCentralVisibility-20260821/index.json`。
- Automation 报告：`Saved/HarnessReports/20260821-120841-ai-production-loop.md`。
- BlockShield 源图/安全单资产导入聚焦测试：1/1 passed。
- UE MCP smoke：PASS；修复后日志无 `BlockShield` 缺失、状态图标 fallback 或目标纹理加载 warning。
- `git diff --check`：PASS。

## 保护检查

- 根项目 `main`，未创建 worktree，未使用 UnrealBridge、Live Coding 或 Hot Reload。
- `Content/GameXXK/Maps/L_Main.umap` SHA-256：`20BC157561D1BBA58D57E0B679DBD66BD0BD2515DDBC3EEF9B5AC40948A0827E`，保持不变。
- `Content/GameXXK/Maps/L_DesktopTrainingHUD.umap` 未修改；验收时 SHA-256：`7C141D525BD0FB8C63BA45FF32017899D05AC12316FDFFE6891C76808D14A5C9`。
