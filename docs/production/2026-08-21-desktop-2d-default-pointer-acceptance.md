---
status: implemented
owner: codex
updated_at: 2026-08-21T13:09:02+08:00
base_commit: 1589f936b3f3a1491be5aa0a3b48385d01be270d
source_commit: 91786825ee8040cbb4683fbf7798998e1f26210f
working_tree: clean for this scope after documentation follow-up; unrelated user files remain untracked
remote_sync: pending; origin/main remains 1589f936 because this session has no GitHub credential and the authentication API is unreachable
---
# 纯 2D 默认入口与战斗目标箭头吸附验收

## 本轮结论

- UE 编辑器与游戏默认地图已改为 `/Game/GameXXK/Maps/L_DesktopTrainingHUD`，并显式加入 `MapsToCook`。
- `GameXXKLevelFlow::MapForScreen(Town)` 已改为同一 2D 地图；旧 `L_Main` 与青山镇 3D 地图只在用户明确要求 3D/旧流程验收时加载。
- 2D 工作台“挑战”继续不接任务、不依赖青山镇，直接在同一地图显示现有全屏 BattleBoard；退出后回到同一 2D 工作台。
- 目标箭头的整段蓝色偏移已修复；箭头与墨点不再向右下多出浮动窗口原点距离。
- 可见箭尖热点使用源图 `(1082,608) / (1254,1254)`，不再把贴图中心当鼠标热点。

## 箭头根因与修复

用户标注截图中，真实鼠标与箭头相差约一段 `(124,109)` 的蓝色距离；这与现场浮动 PIE 窗口左上角完全一致。

旧绘制路径在 `NativePaint` 中执行：

```text
BattleDesignStage local
  -> StageGeometry.LocalToAbsolute
  -> AllottedGeometry.AbsoluteToLocal
```

浮动 PIE 下两个 Geometry 对“Absolute”的原点口径不一致，结果把窗口桌面原点再次加入 Board-local。修复后输入与绘制都只走本地坐标：

```text
viewport-client(DPI)
  -> (position - SafeStage.Offset) / SafeStage.Scale
  -> 1920x1080 stage
  -> SafeStage.Offset + stage * SafeStage.Scale
  -> Board-local paint
```

箭头框体再以源图实际箭尖为显式旋转枢轴，保证换方向只旋转、不让可见尖端离开鼠标。

## RED / GREEN

- 默认配置 RED：`scripts/test_default_2d_entry_config.py` 在旧配置下 2/2 失败（启动地图与 Cook 地图）。
- 旧箭尖中心合同 RED：`Desktop2DPointerRed-20260821` 发现 2、失败 2。
- 浮动窗口坐标 RED：`TargetPointerViewportRed-20260821` 发现 1、失败 1，2 个几何断言按预期失败。
- 最终冷 UBT：`GameXXKEditor Win64 Development -NoHotReload -NoHotReloadFromIDE -NoUBA -MaxParallelActions=2`，`Result: Succeeded`。
- 最终聚焦 Automation：`Saved/Automation/Desktop2DCanonicalFinal-20260821/index.json`，60/60 succeeded、0 failed、0 errors。
- 最终生产循环：`Saved/HarnessReports/20260821-125859-ai-production-loop.md`。
- 默认地图静态合同：2/2 passed；BlockShield PIE probe `py_compile` passed；`git diff --check` passed。

## 真实 2D PIE

权威链路结果：

- `map=L_DesktopTrainingHUD`；
- 挑战后 `screen=BATTLE`、`quest_state=NOT_ACCEPTED`、BattleBoard 可见；
- 返回后 `screen=TOWN`、`workbench_visible=true`、BattleBoard 隐藏；
- 全程未加载 3D 地图。

窗口移动/尺寸矩阵：

| 窗口尺寸 | 桌面原点 | 鼠标（窗口本地） | 结果 | 证据 |
|---:|---:|---:|---:|---|
| 1280×720 | (40,40) | (182,463) | PASS | `Saved/Codex/desktop2d_pointer_tip_fixed_1280x720_at_40_40.png` |
| 1672×941 | (420,200) | (201,550) | PASS | `Saved/Codex/desktop2d_pointer_tip_fixed_1672x941_at_420_200.png` |
| 1920×1080 | (80,80) | (213,604) | PASS | `Saved/Codex/desktop2d_pointer_tip_fixed_1920x1080_at_80_80.png` |

1672×941 的鼠标热点叠加图为 `Saved/Codex/desktop2d_pointer_tip_paint_fixed_1672x941_cursor_marked.png`；青色十字压在可见箭尖上。

项目要求的 Luna 脚本 `~/.claude/skills/codex-vision/scripts/codex_vision.ps1` 在本机不存在；本轮用用户标注图、确定性坐标合同、真实 OS 鼠标输入、三尺寸/三窗口原点截图完成替代验收，没有把 NullRHI 或 HighResShot 黑图当视觉证据。

## 保护检查

- `Content/GameXXK/Maps/L_Main.umap` SHA256：`20BC157561D1BBA58D57E0B679DBD66BD0BD2515DDBC3EEF9B5AC40948A0827E`。
- `Content/GameXXK/Maps/L_DesktopTrainingHUD.umap` SHA256：`7C141D525BD0FB8C63BA45FF32017899D05AC12316FDFFE6891C76808D14A5C9`。
- 两张地图均未修改；角色 Sprite、PaperZD、相机、放置 Actor 与 HD2D Plane 均未写入。

## 当前项目进程与下一步

已完成：2D 默认入口、工作台真实角色入口/独立编队、未接任务直接挑战、同地图 BattleBoard 往返、BlockShield 图标与中央空问号修复、目标箭头窗口原点与可见尖端吸附。

尚未宣称整个产品完成：最终 PSD/剩余未确认图标、天赋与工具的完整权威数据、箱批/FIFO 与容量规则、2D 静置/游历/局内的 Shipping 性能包络仍需后续独立工作包。3D 城镇仅保留回退与显式专项验收，不进入默认后续流程。
