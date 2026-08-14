# Content/Python/ 项目内 Python 脚本索引

这些脚本在 **UE 编辑器/PIE 进程内**用 `unreal` API 运行,不直接命令行执行。运行时由 MCP 工具 `GameXXKTDDToolset.run_project_python_file`(见 `scripts/ue_mcp_client.py`)加载,白名单限死 `scripts/` 与 `Content/Python/` 两个根。

## 机制

- `init_unreal.py`:启动时把 `GameXXKTDDToolset`(ToolsetDefinition + `@toolset_registry.tool_call`)注册进 UE。
- `gamexxk_mcp_tdd_toolset.py`:编辑器内工具实现(`execute_console_command` / `save_dirty_packages` / `get_pie_world_time` / `run_project_python_file` 等)。
- 探针统一模式:进程内读运行时对象 → 转 JSON → `print` 到 stdout,由 MCP 捕获后交回 Python harness。

## 分组

- **`gamexxk_probe_*`**(65+):运行时探针。主探针 `gamexxk_probe_real_play_flow.py` 一次返回 `map_name / runtime_state / save_state / player_controller / hud / pawn / actors / battle_board`,同时也是命令入口(驱动接任务、进路线、选节点、存档等)。
- **`_probe_*`**:一次性调试探针(如 `_probe_battle_deck_dump.py`、`_probe_reward_box_geometry.py`)。
- **`gamexxk_validate_*`**:资产校验(check 侧),如 `gamexxk_validate_character_visuals.py`(尺寸/SHA256/alpha/UV/pivot/关键帧/父类),产出 `ok/…/config_errors` JSON。
- **`gamexxk_assemble_*` / `ensure_*` / `import_*` / `execute_*` / `migrate_*` / `integrate_*` / `create_*` / `build_*`**:资产落地(apply 侧)。前缀尚未收敛(见优化计划 Phase 2.4)。

## 关键文件

| 文件 | 作用 |
|---|---|
| `gamexxk_probe_real_play_flow.py` | 主探针 + 命令入口(真机流程 harness 的数据源) |
| `gamexxk_probe_active_widgets.py` | 枚举可见 UserWidget(最小探针示例) |
| `gamexxk_validate_character_visuals.py` | 角色视觉资产校验最完整样例 |
| `gamexxk_validate_testmap_compat.py` | 校验 TestMap 兼容函数库可加载 |
| `gamexxk_mcp_tdd_toolset.py` | MCP 编辑器内工具实现 |

## 已知问题

- 前缀漂移:`assemble/ensure/author/create/build/integrate/migrate/clone/execute/import` 并存(Phase 2.4 收敛)。
- 探针两套命名:`gamexxk_probe_*` 与 `_probe_*`。
