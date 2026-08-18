---
status: in-progress
owner: codex
updated_at: 2026-08-18
source_commit: 628c46a
decision: not-complete
goal_status: active
---

# Desktop Training 2D HUD Migration — branch record

日期：2026-08-18
分支：`codex/desktop-training-2d-hud-migration`
基线：`628c46a` (`checkpoint: desktop training visual mvp and hud-only map`)

## 目标

把“进入青山镇后再按 Tab 打开”的历练工作台迁移为桌面端的纯 2D HUD 入口，同时保留原 3D 城镇作为 Git/资产回退路径。迁移不覆盖用户修改过的 `L_Main.umap`，也不把 3D 场景 Actor 搬进 HUD 地图。

## 运行时接线

1. `GameXXKLevelFlow::MapForScreen(Town)` 在本分支返回 `/Game/GameXXK/Maps/L_DesktopTrainingHUD`。
2. `L_DesktopTrainingHUD.umap` 是 `L_QingshanInn` 的隔离副本，只保留 `PlayerStart`；场景、灯光、雾、出口和放置角色不在副本中。
3. `AGameXXKMVPGameMode::SpawnDefaultPawnAtTransform_Implementation` 在 HUD 地图返回空 Pawn，避免默认城镇 GameMode 注入 3D 主角。
4. `AGameXXKMVPPlayerController::BeginPlay` 识别 HUD 地图后只在该地图自动启用并打开 `UGameXXKDesktopTrainingWorkbenchWidget`；原 3D 城镇仍保持显式 opt-in。
5. 工作台默认仍以历练页为主入口；`Tab`、仓库/编队/天赋/工具和挑战/游历交互继续由现有工作台统一承载。

## 回退边界

- `L_QingshanInn`、`/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo` 和用户修改的 `L_Main.umap` 未被迁移代码覆盖。
- 回退到 3D 入口只需要切回 `main` 或恢复 `GameXXKLevelFlow::MapForScreen(Town)` 的目标，不需要删除 HUD 资产。
- `IsTownGameplayMapPackage` 不把 HUD 地图当作 3D 城镇 Actor 生成地图；`IsDesktopTrainingHUDMapPackage` 是独立识别口。

## 验收证据

| 检查 | 结果 |
| --- | --- |
| 冷 UBT：`GameXXKEditor Win64 Development -NoHotReload -NoHotReloadFromIDE -MaxParallelActions=1` | PASS，`Result: Succeeded` |
| `GameXXK.MVP.LevelFlow` | PASS |
| `GameXXK.MVP.UI.MainMenuPlayerFlow.SaveMigration` | PASS |
| `GameXXK.MVP.PlayableShell.GameModeDefaults` | PASS |
| `scripts/test_desktop_training_hud_migration.py` | 3/3 PASS |
| 既有 walkloop/import 合同 | 6/6 PASS |
| HUD 地图清理报告 | `Saved/HarnessReports/desktop-training-hud-map.json`；仅保留 `PlayerStart` |

自动化报告：`Saved/Automation/DesktopTrainingHUDMigration/index.json`。
冷编译报告：`Saved/HarnessReports/20260818-100806-ai-production-loop.md`。

## 尚未宣称完成的部分

这次提交是入口迁移，不等同于完整目标完成。真实 PIE 中的 `NativeTick`/滚动视觉、六角色/三敌合成、后台 Timer、最终 PSD/图标、1920/2560 截图和 TaskBarHero 四组性能采样仍需在本分支继续复核；工作台现有规则/战斗/游历 MVP 也不能替代最终发行实现。

## 下一步

- 在干净 HUD 地图中做一次真实 PIE：确认窗口启动后只出现 HUD，确认顶部滚动条实际位移、战斗暂停和失败重试。
- 将 HUD 入口的窗口尺寸、字体可读性、左右仓库/中间背包/右历练地图比例锁成 1920 与 2560 两档合同。
- 继续补齐 HUD 迁移所需的真实角色/伙伴、敌人和最终 PSD 图标资源，但不重绘已有像素基准。
