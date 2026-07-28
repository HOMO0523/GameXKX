# 青山镇地形战斗场迁移 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将一块经审计的青山镇 Landscape 地形，以自包含资源的形式应用到 `L_BattleScene` 的既有战斗地面，同时证明城镇和战斗场其余手调内容未受影响。

**Architecture:** 使用一个只读审计/快照脚本确定源 Landscape 切片与基线；使用一个显式执行模式的 UE 编辑器脚本创建战斗专用静态网格、烘焙纹理和材质；再以精确 Actor Label 锁定的方式仅更新 `GameXXK_Encounter_Floor` 的网格和材质引用。独立验证脚本比较前后快照，并把资源清单和哈希写入版本化 manifest。

**Tech Stack:** Unreal Engine 5.8、项目 UE MCP (`scripts/ue_mcp_client.py` / `scripts/ue_tdd_pipeline.py`)、Unreal Python、C++/UBT、Python unittest、项目受控 Content 路径。

---

## 固定前提与禁止项

- 源地图：`/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo`；源 Landscape：`Landscape` 或 `Landscape_0`，由审计结果唯一确定。
- 目标地图：`/Game/GameXXK/Maps/L_BattleScene`；唯一可写 Actor：精确 Actor Label `GameXXK_Encounter_Floor` 的既有 StaticMeshActor 地面组件。
- 输出路径固定为 `/Game/GameXXK/Environment/Battle/TownTerrain`；不得在 `/Game/Asian_Village/*`、城镇图或用户手调角色资产中创建/修改资源。
- 不可运行 `Content/Python/gamexxk_ensure_route_encounter_maps.py`，它会重设战斗地图的 WorldGrid、相机和演示器。
- 不使用 Live Coding 或 Hot Reload；不得在未保存脏包时关闭编辑器。
- 只读审计只在 PIE 已停止且编辑器干净时临时切换地图；成功或普通失败后恢复原地图。若过程中出现意外脏包，必须停止恢复并保留当前地图、报告脏包，绝不通过强制切图丢弃用户工作。
- 本项目不建立 worktree，也不自动提交或暂存；若用户明确要求再处理 Git。

## 文件结构

| 文件 | 责任 |
| --- | --- |
| `Content/Python/gamexxk_audit_battle_town_terrain.py` | 只读枚举源/目标地图、受保护 Actor、Landscape 分块及候选切片，输出 JSON 快照。 |
| `Content/Python/gamexxk_bake_battle_town_terrain.py` | 只在 `--execute` 下创建输出资产并替换精确 Actor Label 锁定的地面引用；默认拒绝写入。 |
| `Content/Python/gamexxk_validate_battle_town_terrain.py` | 对比基线与完成快照、检查资源路径/尺寸/地面引用和地图保护规则。 |
| `SourceAssets/PartyDeck/battle-town-terrain/battle-town-terrain-manifest-v1.json` | 记录输入世界坐标、源素材、输出资产、文件哈希和验证结果。 |
| `scripts/test_battle_town_terrain_pipeline.py` | 纯静态单元测试，禁止不安全路径、检查脚本模式、资源命名和验证契约。 |
| `docs/superpowers/specs/2026-07-17-battle-town-terrain-design.md` | 已批准设计与不可变范围。 |

### Task 1: 写入前只读审计与静态保护测试

**Files:**
- Create: `Content/Python/gamexxk_audit_battle_town_terrain.py`
- Create: `scripts/test_battle_town_terrain_pipeline.py`
- Create: `SourceAssets/PartyDeck/battle-town-terrain/battle-town-terrain-manifest-v1.json`

- [ ] **Step 1: 写出失败的静态测试**

```python
def test_audit_is_read_only_and_declares_both_maps():
    text = read("Content/Python/gamexxk_audit_battle_town_terrain.py")
    assert "L_Qingshan_AsianVillage_Demo" in text
    assert "L_BattleScene" in text
    assert "save_current_level" not in text.lower()
    assert "save_loaded_asset" not in text.lower()

def test_no_legacy_map_reset_script_is_called():
    text = read("Content/Python/gamexxk_bake_battle_town_terrain.py")
    assert "gamexxk_ensure_route_encounter_maps" not in text
```

- [ ] **Step 2: 运行失败测试**

Run:
```powershell
& 'C:\Users\shxuw\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' scripts\test_battle_town_terrain_pipeline.py
```

Expected: `FAIL`，指出审计脚本和执行脚本尚不存在。

- [ ] **Step 3: 实现最小只读审计脚本**

实现 `main()`，接受 `--output <absolute-json-path>`；在 UE Python 中加载两个地图并输出如下稳定结构，禁止调用任何保存 API：

```python
snapshot = {
    "schema": 1,
    "town_map": "/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo",
    "battle_map": "/Game/GameXXK/Maps/L_BattleScene",
    "town_package_sha256": sha256_file(town_umap_filename),
    "battle_package_sha256": sha256_file(battle_umap_filename),
    "landscapes": [{"name": actor.get_name(), "transform": transform_dict(actor), "bounds": bounds_dict(actor)}],
    "protected_battle_actors": protected_actor_snapshot(world),
    "encounter_floor": encounter_floor_snapshot(world),
}
```

`protected_actor_snapshot()` 必须记录精确 Actor Label `GameXXK_Encounter_Floor`、相机、Presenter、PlayerStart、主要灯光和单位 Actor 的名称、标签、Transform、组件类与资源引用；找不到唯一地面 Actor Label 则抛出错误。

- [ ] **Step 4: 写入空 manifest 模板**

```json
{
  "schema": 1,
  "status": "pending-audit",
  "source_map": "/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo",
  "target_map": "/Game/GameXXK/Maps/L_BattleScene",
  "source_landscape": "",
  "slice_world_bounds_cm": {},
  "output_assets": [],
  "source_hashes": {},
  "validation": {}
}
```

- [ ] **Step 5: 运行静态测试并提交审计快照**

Run:
```powershell
& 'C:\Users\shxuw\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' scripts\test_battle_town_terrain_pipeline.py
```

Expected: `OK`。随后通过 UE MCP 执行审计脚本，将 `before.json` 写入项目 `Saved/` 目录；确认没有保存城镇图、没有变更任何 `.umap` 哈希。审计只在 PIE 已停止且干净会话中切换地图；成功或普通失败后恢复原地图，若发现意外脏包则保留当前地图并报告，不能强制恢复。

### Task 2: 锁定可安全复制的城镇切片

**Files:**
- Modify: `Content/Python/gamexxk_audit_battle_town_terrain.py`
- Modify: `SourceAssets/PartyDeck/battle-town-terrain/battle-town-terrain-manifest-v1.json`
- Test: `scripts/test_battle_town_terrain_pipeline.py`

- [ ] **Step 1: 增加失败测试，强制切片大小与排除区**

```python
def test_candidate_requires_24_by_14_meter_bounds_and_no_worldgrid():
    text = read("Content/Python/gamexxk_audit_battle_town_terrain.py")
    assert "2400.0" in text
    assert "1400.0" in text
    assert "candidate" in text.lower()
    assert "WorldGrid" not in read("Content/Python/gamexxk_bake_battle_town_terrain.py")
```

- [ ] **Step 2: 运行测试确认失败**

Run the command from Task 1. Expected: `FAIL` until candidate selection is explicit.

- [ ] **Step 3: 实现候选区选择**

在审计脚本中把 `2400.0 × 1400.0 cm` 的候选 Bounds 记录为 `candidate_slice`。选择规则必须全部通过：

```python
assert is_inside_landscape(candidate)
assert max_height_delta_cm(candidate) <= 160.0
assert not overlaps_tagged_actor(candidate, forbidden_tags)
assert not overlaps_water_or_stream(candidate)
```

将实际选定中心点、Bounds、最大高度差、源 Landscape 名称和源材质路径写入 manifest；如果没有合格切片，脚本只报告失败，绝不能选择整张 Landscape 或改动地图。

- [ ] **Step 4: 再跑静态测试与只读审计**

Expected: `OK`。在编辑器中人工查看候选区截图，确认它是城镇地表，不含房屋、河流、NPC 或地图边界。

### Task 3: 以明确执行开关生成战斗专用资源

**Files:**
- Create: `Content/Python/gamexxk_bake_battle_town_terrain.py`
- Modify: `scripts/test_battle_town_terrain_pipeline.py`
- Modify: `SourceAssets/PartyDeck/battle-town-terrain/battle-town-terrain-manifest-v1.json`

- [ ] **Step 1: 写出默认拒绝写入的失败测试**

```python
def test_baker_requires_execute_flag_and_owned_output_path():
    text = read("Content/Python/gamexxk_bake_battle_town_terrain.py")
    assert "--execute" in text
    assert "/Game/GameXXK/Environment/Battle/TownTerrain" in text
    assert "/Game/Asian_Village/" not in text.replace("source_material", "")
```

- [ ] **Step 2: 运行测试确认失败**

Run the command from Task 1. Expected: `FAIL` because the baker has not yet been created.

- [ ] **Step 3: 实现安全的默认模式**

```python
if "--execute" not in sys.argv:
    raise RuntimeError("Refusing to write: pass --execute after a reviewed before snapshot.")
```

执行模式必须先读取 `before.json` 和 manifest，验证所有源路径和 `source_hashes` 完全匹配；不匹配时失败。

- [ ] **Step 4: 实现本地网格与材质产物**

通过已在 UE 5.8 运行时实际探测到的 Geometry/StaticMesh API，生成 `SM_Battle_QingshanGround_01`。网格顶点只能来自 manifest 的切片高度采样；输出材质只引用以下项目自有资产：

```python
OUTPUT_ROOT = "/Game/GameXXK/Environment/Battle/TownTerrain"
MESH = OUTPUT_ROOT + "/SM_Battle_QingshanGround_01"
MATERIAL = OUTPUT_ROOT + "/M_Battle_QingshanGround_01"
```

若可安全使用引擎材质烘焙 API，创建 `T_Battle_QingshanGround_Albedo_01` 等纹理；若该 API 不可用，停止并报告技术缺口，不得退回为纯色/WorldGrid/生成图替代。

- [ ] **Step 5: 保存并更新 manifest**

manifest 完成后必须包含：`status: "baked"`、源切片、输出资源完整路径、每个输出包或源文件的 SHA-256、引擎版本、时间戳和 `before_snapshot` 路径。

- [ ] **Step 6: 运行静态测试**

Expected: `OK`，且默认不带 `--execute` 的脚本调用退出非零、没有生成或改写资源。

### Task 4: 仅替换精确 Actor Label 锁定的战斗地面引用

**Files:**
- Modify: `Content/Python/gamexxk_bake_battle_town_terrain.py`
- Modify: `Content/Python/gamexxk_validate_battle_town_terrain.py`
- Test: `scripts/test_battle_town_terrain_pipeline.py`

- [ ] **Step 1: 写出失败测试，禁止改动受保护对象**

```python
def test_apply_targets_only_encounter_floor_and_preserves_transform():
    text = read("Content/Python/gamexxk_bake_battle_town_terrain.py")
    assert "GameXXK_Encounter_Floor" in text
    assert "set_static_mesh" in text.lower()
    assert "relative_transform" in text.lower() or "world_transform" in text.lower()
    assert "set_actor_transform" not in text.lower()
```

- [ ] **Step 2: 运行测试确认失败**

Expected: `FAIL` until the apply contract is present.

- [ ] **Step 3: 实现唯一对象查找和引用替换**

执行前要求 exactly one Actor Label 匹配：

```python
floor = find_single_actor_by_label(world, "GameXXK_Encounter_Floor")
component = find_single_static_mesh_component(floor)
old_transform = component.get_component_transform()
component.set_static_mesh(load_asset(MESH))
component.set_material(0, load_asset(MATERIAL))
assert transforms_equal(old_transform, component.get_component_transform())
```

不得写入任何其他 Actor。只在上述断言通过后保存 `L_BattleScene` 与输出资源，并立即生成 `after.json`。

- [ ] **Step 4: 实现验证器**

验证器必须比对：

```python
assert before["town_package_sha256"] == after["town_package_sha256"]
assert before["landscapes"] == after["landscapes"]
assert protected_actors_equal_except_floor_resources(before, after)
assert after["encounter_floor"]["mesh"] == "/Game/GameXXK/Environment/Battle/TownTerrain/SM_Battle_QingshanGround_01"
assert after["encounter_floor"]["material"] == "/Game/GameXXK/Environment/Battle/TownTerrain/M_Battle_QingshanGround_01"
```

- [ ] **Step 5: 用 UE MCP 执行并保存**

先在编辑器中保存脏包；再执行 `--execute`。如果脚本找不到唯一地面、资源烘焙失败或任一保护断言失败，停止且不继续 PIE。成功后保存目标资源和 `L_BattleScene`，将最终验证写入 manifest。

### Task 5: 冷编译、静态与真实流程验证

**Files:**
- Modify: `SourceAssets/PartyDeck/battle-town-terrain/battle-town-terrain-manifest-v1.json`
- Test: `scripts/test_battle_town_terrain_pipeline.py`

- [ ] **Step 1: 运行纯静态验证**

Run:
```powershell
& 'C:\Users\shxuw\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' scripts\test_battle_town_terrain_pipeline.py
```

Expected: `OK` with no failure and no references to `WorldGrid` or `gamexxk_ensure_route_encounter_maps.py`.

- [ ] **Step 2: 冷编译项目**

Run:
```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -NoHotReload
```

Expected: `Result: Succeeded` / exit code `0`; 不能以 `--check-only`、Live Coding 或 Hot Reload 代替。

- [ ] **Step 3: 运行战斗 UI 与卡牌目标回归**

Run the project’s focused automation command for `GameXXK.Integration.CardBattle.BoardTargeting` through `scripts/ue_tdd_pipeline.py`. Expected: green pass with no `WorldGrid`/裸地面回归。

- [ ] **Step 4: 运行真实 PIE 主流程**

从主菜单开始：世界地图 → 青山镇 → 接任务 → 路线节点 → 战斗。截图确认战斗地面为城镇视觉，手动目标牌仍显示高亮和箭头并能选中单位；结束战斗后奖励和返回路线仍可用。

- [ ] **Step 5: 完成记录**

把所有命令、时间、前后快照路径、资源路径和结果填入 manifest 的 `validation`。只有上述全部绿色时才能报告本任务完成；不自动暂存、提交或删除用户已有的生成背景源文件。

## 自审

- [ ] 设计中的所有限制均映射到 Task 1–5：只读源审计（Task 1/2）、局部自包含资源（Task 3）、唯一地面替换（Task 4）、冷编译和 PIE（Task 5）。
- [ ] 未使用 `TODO`、`TBD` 或“以后补充”；不可用的烘焙 API 明确规定为停止并报告，而非使用替代视觉。
- [ ] 未引入对城镇材质、整张 Landscape、WorldGrid、旧地图重置脚本、Live Coding 或自动 Git 操作的写入。
