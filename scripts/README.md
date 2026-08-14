# scripts/ 自动化脚本索引

本目录是 GameXXK 的 Python 自动化层(约 161 个 .py)。入口优先级:先看"核心脚本",其余按命名约定分组发现。

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

## 命名约定分组

- **`test_*.py`**(83 个):脚本单元测试,`python -m unittest` 风格;核心子集见 `ai_production_loop.py` 默认列表。
- **`gamexxk_*_check.py` / `gamexxk_*_apply.py`**(成对):资产流水线,check 只读校验、apply 构建/导入/装配(经 MCP `run_project_python_file` 调 `Content/Python/gamexxk_validate_*` / `assemble_*`)。
- **`run_*.py` / `build_*.py`**(15 个):一次性运行器,如 `run_qingshan_town_pcg_vertical_slice.py`、`run_qingshan_whitebox_b0r.py`、`run_asian_village_migration.py`、`build_qingshan_b1_heightmap.py`。
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
```

## 已知问题(见 docs/production/optimization-plan.md)

- `ue_mcp_smoke.py` 曾复刻 `ue_mcp_client.py` 传输逻辑(Phase 1.4 去重)。
- `battle_ui_assets_check` vs `battle_ui_asset_check` 命名漂移(Phase 2.4 收敛)。
- 一次性脚本见 `_archive/`。
