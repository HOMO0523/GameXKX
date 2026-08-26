---
status: plan
owner: agent
created_at: 2026-08-19T23:20:00+08:00
branch: main
head_at_plan: 643ac9b
source_of_truth: docs/production/current-goal-acceptance.md
---

# MVP 演示交付计划：2D 桌面挂机 + 挑战局内

> 目标不是宣称整个桌面历练已经完成，而是让用户能**一键启动当前可玩的 MVP**：
> 打开 2D 桌面工作台，看到顶部挂机/游历表现；挑战动作关闭工作台并进入现有路线图，玩家自己点节点进入全屏 BattleBoard 的真实 CardBattle。
> 本计划与 Task 6 两层退出收尾并行；演示不依赖 Task 6 完全验收。

## 1. 演示目标（Definition of Done）

- [ ] 提供一个可重复运行的启动器：`scripts/launch_desktop_mvp_demo.ps1`。
- [ ] 启动后出现独立的 `GameXXK` 游戏窗口，默认加载 `L_DesktopTrainingHUD`。
- [ ] 窗口内自动打开 2D 桌面工作台（左仓库、中背包、右历练地图、顶部挂机条）。
- [ ] 支持三种演示模式：
  - `workbench`：只打开工作台，用户自己点“游历”；点“挑战”会关闭工作台并进入现有路线图；
  - `travel`：自动进入 1-1 游历，演示顶部挂机条走动/遇敌/攻击/受击/死亡表现；
  - `challenge`：直接进入现有全屏 BattleBoard 的 CardBattle（测量/演示快捷方式，不替代玩家选路线）。
- [ ] 启动脚本默认不触碰用户资产，不覆盖 `L_Main.umap`、`SourceAssets/`、`SourceArt/`。
- [ ] 提供清晰的演示路径文字：从哪里点“游历”、从哪里点“挑战”、如何退出。
- [ ] 若启动前检测到已有同项目编辑器，默认提示；加 `-CloseRunningEditor` 才会安全保存并关闭。

## 2. 当前可演示能力（已核实）

- `L_DesktopTrainingHUD.umap` 存在，是纯 2D HUD 隔离验收面；直接 `-game` 加载会自动打开工作台。
- `GameXXKDesktopTrainingWorkbenchWidget` 已实现：顶部 3 敌+3 我挂机条、左仓库、中背包、右历练地图、挑战/游历按钮。
- `StartTrainingTravel` / `AdvanceTrainingTravelStep` 已实现，顶部条有 Walking / EncounterIdle / HeroAttack / EnemyHit / EnemyDeath 等表现状态。
- 按 2026-08-19 用户纠偏，工作台“挑战”不再创建内嵌 ChallengeViewport/3+3 顶栏/只读侧壳；它只委托现有传送门路线入口，关闭工作台后进入现有路线图，玩家自己点节点进入全屏 BattleBoard。
- `-GameXXKPerfProfile=travel` 和 `-GameXXKPerfProfile=challenge` 已有 C++ 支持；`challenge` 是测量/演示快捷方式，直接进入现有全屏 BattleBoard，不代表替代玩家选路线。
- 已知限制：完整 PSD、最终图标、四组性能采样、默认入口切换、Task 6 三分辨率视觉仍未完成；演示只代表当前 opt-in MVP。

## 3. 详细执行步骤

### 3.1 交付启动器

- 新建 `scripts/launch_desktop_mvp_demo.ps1`。
- 参数：
  - `-Profile`：`workbench`（默认）、`travel`、`challenge`；
  - `-Resolution`：默认 `1672x941`，可选 `1920x1080`、`1280x720`；
  - `-CloseRunningEditor`：安全保存并关闭当前同项目编辑器后再启动；
  - `-DescribeOnly`：只打印启动参数不启动。
- 行为：
  - 若 `-CloseRunningEditor`，调用 `ue_tdd_pipeline.save_running_editor_before_close` + `kill_editor`，失败则中止；
  - 启动 `D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe`，参数：
    `GameXXK.uproject /Game/GameXXK/Maps/L_DesktopTrainingHUD -game -windowed -ResX=<w> -ResY=<h> -NoSplash -NoZenAutoLaunch -DDC-ForceMemoryCache`
  - 根据 `-Profile` 追加 `-GameXXKPerfProfile=travel|challenge`；
  - 输出 PID 和演示说明。

### 3.2 验证脚本与运行路径

- 运行 `powershell -File scripts/launch_desktop_mvp_demo.ps1 -DescribeOnly`，确认参数合同正确。
- 运行 `python -m unittest scripts.test_measure_desktop_training_hud_memory scripts.test_desktop_training_hud_migration` 等已有脚本测试，确认不破坏现有门禁。
- 可选：冷 UBT 确认当前工作区可编译（Task 6 已要求，但演示可先用现有已编译二进制）。

### 3.3 启动演示窗口

- 使用 `-Profile workbench` 启动一个可见窗口，供用户手动点击“游历/挑战”；
- 或使用 `-Profile travel` 直接演示挂机条，再让用户在窗口内尝试挑战；
- 或使用 `-Profile challenge` 直接进入现有全屏 BattleBoard 的 CardBattle（快捷演示，不模拟玩家选路线）。

### 3.4 取证与用户指引

- 若需要我截图，使用 MCP/Slate 截取 `L_DesktopTrainingHUD` 的 travel/challenge 状态；
- 给用户提供最短演示路径：
  1. 启动 `scripts/launch_desktop_mvp_demo.ps1 -Profile workbench`
  2. 在右侧历练地图选择一个关卡，点击“游历”看挂机条；
  3. 点击“挑战”会关闭工作台并进入现有路线图；玩家自己点节点进入全屏 BattleBoard；
  4. 打开自动战斗观察出牌，或手动点牌/结束回合。

## 4. 与 Task 6 的关系

- 演示可以现在进行；Task 6 的“两层退出确认”是额外收尾，不影响 2D 桌面挂机/挑战的观看。
- 若同时要演示“路线图关闭挑战”和“Battle 关闭确认”，需要先完成 Task 6 真实流；本计划不把两者混为一谈。
- 演示窗口使用 `L_DesktopTrainingHUD`，不切换默认 3D 城镇入口。

## 5. 交付后的验证标准

- [ ] 启动器 `-DescribeOnly` 输出合法参数且不启动 UE。
- [ ] `-Profile workbench` 能打开窗口且工作台可见。
- [ ] `-Profile travel` 能自动进入游历并出现顶部挂机条动作。
- [ ] `-Profile challenge` 能自动进入现有全屏 BattleBoard（不替代玩家选路线）。
- [ ] 用户能在窗口内手动切换游历/挑战（若互斥规则允许）。
- [ ] 未改动受保护地图/资产；`git status` 只新增本计划文档与启动器。
