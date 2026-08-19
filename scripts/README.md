# scripts/ 自动化脚本索引

本目录是 GameXXK 的 Python 自动化层(根目录 175 个 `.py`，2026-08-18 实测)。入口优先级:先看"核心脚本",其余按命名约定分组发现。

## 核心脚本(手动维护的流程入口)

| 脚本 | 作用 |
|---|---|
| `ue_mcp_client.py` | UE 5.8 内置 MCP 客户端(HTTP JSON-RPC 2.0 + SSE),封装 connect/call_tool/PIE 生命周期/日志采集 |
| `ue_mcp_smoke.py` | MCP 服务冒烟测试(校验必需 toolset) |
| `ue_paths.py` | 解析 UE 安装根目录(GAMEXXK_UE_ROOT → 候选 → 注册表) |
| `ue_tdd_pipeline.py` | TDD 冷编译→启动 MCP 编辑器→PIE→[TDD] 日志断言全流程(禁 Live Coding) |
| `ai_production_loop.py` | 生产循环:状态校验 → diff --check → 脚本自测 → 冷 UBT → Automation → 真机流程 |
| `parse_automation_index.py` | Automation 报告判官:只认 index.json 计数 |
| `harness_state_validator.py` | 生产单元/散落报告状态校验(8 文件模板 + 散落报告轻量 schema + 新鲜度) |
| `gamexxk_real_play_flow_mcp.py` | 真机 PIE 玩家流程 harness(主菜单→城镇→接任务→存档→路线图→战斗) |
| `run_training_visual_pie_probe.py` | 直接加载纯 HUD 验收地图并以两次 MCP 调用验证历练条 NativeTick、无缝滚动与 walk atlas；等待发生在 UE 进程外 |
| `measure_desktop_training_hud_memory.ps1` | 隔离启动 empty / Travel / 现有全屏 BattleBoard / 3D town 四档进程，在 20/50 秒记录 CPU、GPU 与内存并写 JSON 证据；不执行构建或打包 |
| `gamexxk_ui_image_truth_check.py` | 只读校验用户确认图片真源库：manifest 覆盖、SHA256、尺寸、透明通道和确认依据必须一致 |

## 交互编辑器启动

需要在 Windows 桌面看到 UE 编辑器时，双击项目根目录的 `Launch_GameXXK_Editor.cmd`。该入口固定打开 `D:\UE_5.8` 对应的 `GameXXK.uproject`，把 Turnkey/.NET、DDC、Shader 临时写入隔离到项目 `Saved`，避免用户 AppData 权限异常；它不使用 `-Unattended`，并同时启动 18765 MCP。`ue_tdd_pipeline.py` 仍只用于无窗口自动化验证，不能替代这个玩家可见入口。

## 桌面历练 HUD 编辑器内存

`measure_desktop_training_hud_memory.ps1` 固定以 1672×941、一次一个独立进程采集 `empty`、`travel`、`challenge`、`town3d`。其中 `challenge` 仅保留历史报告键名，实际 `surface` 为 `existing-fullscreen-battle`：它通过显式性能夹具进入现有全屏 `UGameXXKBattleBoardWidget`，不再创建工作台内嵌战斗。20/50 秒样本包含 CPU、GPU Engine、Dedicated/Shared GPU Memory、Working Set 和 Private Memory，并把命令、PID、HEAD、表面语义和清理结果写入 `Saved/HarnessReports`。脚本只关闭自己启动的进程，不会终止已有编辑器；若检测到同项目编辑器已运行会拒绝启动，除非显式传入 `-AllowConcurrentEditor`。

```powershell
# 查看四档采样合同，不启动 UE
pwsh -File scripts/measure_desktop_training_hud_memory.ps1 -DescribeOnly -Profile all

# 执行完整四档 20/50 秒实测
pwsh -File scripts/measure_desktop_training_hud_memory.ps1 -Profile all

# 只测现有全屏战斗表面
pwsh -File scripts/measure_desktop_training_hud_memory.ps1 -Profile challenge
```

## 命名约定分组

- **`test_*.py`**(91 个):脚本单元测试,`python -m unittest` 风格;核心子集见 `ai_production_loop.py` 默认列表。
- **`gamexxk_*_check.py` / `gamexxk_*_apply.py`**(成对):资产流水线,check 只读校验、apply 构建/导入/装配(经 MCP `run_project_python_file` 调 `Content/Python/gamexxk_validate_*` / `assemble_*`)。
  - `gamexxk_battle_ui_assets_check.py` / `gamexxk_battle_ui_assets_apply.py`:校验/应用**已导入 UE** 的战斗 UI 纹理(MCP);
  - `gamexxk_battle_target_art_check.py`:本地 PIL 校验**源图**(目标箭头/墨点图集,无需 UE)。
- **`run_*.py` / `build_*.py`**(16 个):受控运行器,如 `run_training_visual_pie_probe.py`、`run_qingshan_town_pcg_vertical_slice.py`、`run_qingshan_whitebox_b0r.py`、`run_asian_village_migration.py`、`build_qingshan_b1_heightmap.py`。
- **`qingshan*` / `asian_village*`**:青山镇 PCG 与亚洲村庄迁移专项。
- **`_archive/`**:一次性/调试脚本归档区(不参与日常流程)。

## 常用验证命令

```powershell
# 生产状态校验
python scripts/harness_state_validator.py --json

# 脚本自测(核心子集)
python scripts/ai_production_loop.py --run-script-tests --json

# 冷 UBT 编译 + 自动化(需先确保编辑器已关闭/已保存)
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK

# 真机 PIE 流程
python scripts/gamexxk_real_play_flow_mcp.py

# 纯 HUD 历练滚动验收；未启动编辑器时精确启动当前 GameXXK.uproject
python scripts/run_training_visual_pie_probe.py --launch-editor --capture
```

## 已知问题(见 docs/production/optimization-plan.md)

- `ue_mcp_smoke.py` 曾复刻 `ue_mcp_client.py` 传输逻辑(已去重)。
- `battle_ui_assets_check` vs `battle_ui_asset_check` 命名漂移(已重命名为 `gamexxk_battle_target_art_check.py`)。
- 一次性脚本见 `_archive/`。
