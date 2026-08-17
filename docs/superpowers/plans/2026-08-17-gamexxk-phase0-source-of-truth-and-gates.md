# GameXXK Phase 0 Source-of-Truth and Test-Gate Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 GameXXK 当前项目状态、历练规格、脚本测试分层和资产保护规则对齐到 `main@ba90810`，让 Phase 0 门禁可重复运行且不触碰用户已有 `.umap` 和未跟踪大资产。

**Architecture:** 生产指针只记录事实和证据路径；旧历练迁移文档通过 front matter 标为 `superseded`/`shelved`，新的桌面工作台规格作为唯一待执行设计真源；Python 门禁通过一个只读 manifest 将 `headless`、`asset-contract`、`mcp-live` 分层，`all` 默认只运行 headless。所有路径修复以参数/env 为入口，不把本机资源复制进仓库。

**Tech Stack:** Markdown production records, Python 3.12, `argparse`, `json`, `subprocess`, `pathlib`, PowerShell read-only process/Git checks, `harness_state_validator.py`, `ai_production_loop.py`.

---

## Scope and safety lock

本计划只执行优化方案 Phase 0 的文档与 Python 门禁工作。不得修改：

- `Content/GameXXK/Maps/L_Main.umap`（当前用户/编辑器已有 tracked 修改）；
- `SourceAssets/`、`SourceArt/` 下未跟踪美术；
- `.uasset`、`.umap`、角色像素图、PaperZD、相机和 HD2D 参数；
- 任何运行时 C++、存档字段或默认入口行为。

每个任务独立提交。任务完成前必须运行该任务列出的验证命令；未通过的任务不得继续下一个任务。

## File map

| 文件 | 职责 | 本计划动作 |
|---|---|---|
| `docs/production/current-goal-acceptance.md` | 当前事实滚动指针 | 修正 HEAD、日期、历练状态、统计口径和下一步 |
| `docs/production/2026-08-17-phase0-baseline.md` | 本轮状态证据 | 新建，记录命令、输出摘要、工作区保护清单 |
| `docs/superpowers/specs/2026-08-12-gamexxk-idle-desktop-migration-design.md` | 旧历练总规格 | 标记 `superseded`，指向 2026-08-17 新规格 |
| `docs/superpowers/plans/2026-08-13-gamexxk-idle-*.md` | 旧历练实施包 | 标记 `shelved`，冻结 v17 前置条件 |
| `docs/superpowers/plans/2026-08-13-gamexxk-desktop-migration-implementation-index.md` | 旧包索引 | 保留历史正文，增加新规格和 v17 边界说明 |
| `docs/superpowers/specs/2026-08-17-gamexxk-desktop-training-workbench-design.md` | 当前 UI/玩法设计真源 | 增加机器可读 front matter，保持书面复核状态 |
| `scripts/script-test-manifest.json` | Python 测试标签清单 | 新建，只列异常标签，默认其余为 headless |
| `scripts/ai_production_loop.py` | 生产循环 | 读取标签 manifest；`all` 只跑 headless；JSON 输出安全 |
| `scripts/test_gamexxk_ui_master_build.py` | UI Master 子进程测试 | 固定 UTF-8、替换非法输出 |
| `scripts/ue_paths.py` | UE 根路径解析 | 保持参数/env 优先，供后续路径审计使用 |
| `scripts/asian_village_migration.py`、`scripts/run_asian_village_migration.py` | 外部源迁移脚本 | 改为参数/env；缺源只报告可解释的 skip，不在默认门禁执行 |

## Task 1: Record the current Phase 0 baseline

**Files:**

- Create: `docs/production/2026-08-17-phase0-baseline.md`
- Modify: `docs/production/current-goal-acceptance.md`

- [ ] **Step 1: Capture immutable identifiers and workspace ownership**

Run from `D:\UE5 demo\GameXXK`:

```powershell
git branch --show-current
git rev-parse HEAD
git log -1 --format='%h %s'
git status --short --branch
git status --short -- Content/GameXXK/Maps/L_Main.umap SourceAssets SourceArt
```

Expected facts at plan creation time: branch `main`, HEAD `ba90810`, one tracked modification at `Content/GameXXK/Maps/L_Main.umap`, and untracked SourceAssets/SourceArt entries. Record the observed output and the rule that these paths are outside the Phase 0 write set.

- [ ] **Step 2: Capture the current validation evidence without claiming a fresh UE full run**

Run:

```powershell
python scripts/harness_state_validator.py --json
python scripts/ai_production_loop.py --run-script-tests --json
git diff --check
```

The baseline record must distinguish:

- fresh Phase 0 checks (`harness_state_validator`, default production loop, whitespace check);
- historical evidence (`598/598` Automation and cold UBT from the dated reports);
- known stale/partial evidence (`--script-tests all` currently recorded as `64/86`, with 22 failures awaiting tags or fixes).

- [ ] **Step 3: Add the baseline record**

Use this structure, replacing only values observed by the commands:

```markdown
---
status: record
updated_at: 2026-08-17
source_commit: ba90810
---
# GameXXK Phase 0 基线证据

## Fresh checks
- `harness_state_validator.py --json`: `ok=true`, `findings=[]`
- default `ai_production_loop.py --run-script-tests --json`: `ok=true`
- `git diff --check`: exit 0

## Historical checks
- Automation: 598/598, source report path and report timestamp
- Cold UBT: report path, `-NoHotReload`, result
- Full script discovery: 64/86 in the 2026-08-16 record; not a Phase 0 pass

## Protection lock
- Preserve `Content/GameXXK/Maps/L_Main.umap` tracked modification.
- Do not stage untracked `SourceAssets/` or `SourceArt/`.
- Do not alter `.uasset`, `.umap`, PaperZD, camera, or HD2D values.
```

- [ ] **Step 4: Correct the rolling pointer**

Update only factual sections of `current-goal-acceptance.md`:

- set `updated_at` and the baseline commit to `ba90810`;
- state that `ba90810` is documentation-only on top of the current code baseline;
- keep `598/598` labeled as the latest historical Automation evidence with its report path/date;
- replace “历练桌面迁移 7 包用户决定搁置” with “旧 7 包执行索引 shelved；新桌面工作台设计仅完成布局/UX 规格，运行时实现尚未开始；恢复执行必须从 Phase 0 和 v17 边界重新取基线”;
- add the new specification path and the Phase 0 baseline path to the next-step list;
- preserve the explicit follower semantic freeze and the known `--script-tests all` 64/86 record until the gate is actually rerun.

- [ ] **Step 5: Verify Task 1 and commit**

```powershell
python scripts/harness_state_validator.py --json
git diff --check
git diff -- docs/production/current-goal-acceptance.md docs/production/2026-08-17-phase0-baseline.md
git add -- docs/production/current-goal-acceptance.md docs/production/2026-08-17-phase0-baseline.md
git diff --cached --check
git commit -m "docs: record phase 0 project baseline"
```

Expected: only the two listed Markdown files are in the commit; `L_Main.umap` and all untracked asset entries remain unstaged.

## Task 2: Mark superseded and shelved migration documents

**Files:**

- Modify: `docs/superpowers/specs/2026-08-12-gamexxk-idle-desktop-migration-design.md`
- Modify: `docs/superpowers/plans/2026-08-13-gamexxk-desktop-mini-window-implementation.md`
- Modify: `docs/superpowers/plans/2026-08-13-gamexxk-idle-chest-ledger-implementation.md`
- Modify: `docs/superpowers/plans/2026-08-13-gamexxk-idle-core-save-implementation.md`
- Modify: `docs/superpowers/plans/2026-08-13-gamexxk-idle-default-entry-e2e-implementation.md`
- Modify: `docs/superpowers/plans/2026-08-13-gamexxk-idle-home-ui-implementation.md`
- Modify: `docs/superpowers/plans/2026-08-13-gamexxk-desktop-migration-implementation-index.md`
- Modify: `docs/superpowers/specs/2026-08-17-gamexxk-desktop-training-workbench-design.md`

- [ ] **Step 1: Add machine-readable status front matter**

Add exactly one YAML block at the beginning of each file, before the current title:

```yaml
---
status: superseded
updated_at: 2026-08-17
superseded_by: docs/superpowers/specs/2026-08-17-gamexxk-desktop-training-workbench-design.md
---
```

Use `status: shelved` and `shelved_reason: legacy migration package; do not execute` for the five old implementation plans and the old index. Use `status: superseded` for the old 2026-08-12 design. The new 2026-08-17 design uses `status: design-review` and keeps its current sentence that layout/UX is confirmed but written spec review is pending.

- [ ] **Step 2: Freeze old version boundaries**

Add this notice immediately after the status block of each shelved plan/index:

```markdown
> 执行冻结：当前 `CurrentSaveVersion=17`，本文的 v15/v16/v17/v18 迁移边界不能直接执行。恢复历练实现必须以 `2026-08-17-gamexxk-desktop-training-workbench-design.md` 和新的 Phase 0 基线重新编排迁移编号。
```

Do not rewrite the historical task body; the goal is to make an old plan visibly non-authoritative while retaining its audit trail.

- [ ] **Step 3: Verify Task 2 and commit**

```powershell
rg -n '^status:|superseded_by:|shelved_reason:|CurrentSaveVersion=17|执行冻结' docs/superpowers/specs/2026-08-12-gamexxk-idle-desktop-migration-design.md docs/superpowers/plans/2026-08-13-gamexxk-idle-*.md docs/superpowers/plans/2026-08-13-gamexxk-desktop-migration-implementation-index.md docs/superpowers/specs/2026-08-17-gamexxk-desktop-training-workbench-design.md
git diff --check
git add -- docs/superpowers/specs/2026-08-12-gamexxk-idle-desktop-migration-design.md docs/superpowers/plans/2026-08-13-gamexxk-idle-*.md docs/superpowers/plans/2026-08-13-gamexxk-desktop-migration-implementation-index.md docs/superpowers/specs/2026-08-17-gamexxk-desktop-training-workbench-design.md
git diff --cached --name-only
git commit -m "docs: mark legacy idle migration plans shelved"
```

Expected: eight named documents only; no `.umap`, `.uasset`, `SourceAssets`, or `SourceArt` path is staged.

## Task 3: Add explicit script-test tag manifest

**Files:**

- Create: `scripts/script-test-manifest.json`
- Modify: `scripts/ai_production_loop.py`
- Test: `scripts/test_ai_production_loop.py`

- [ ] **Step 1: Write the manifest contract test**

Create `scripts/test_ai_production_loop.py` with this exact initial contract:

```python
import json
import unittest
from pathlib import Path

from ai_production_loop import SCRIPT_TEST_MANIFEST, discover_script_tests, load_script_test_manifest


class ScriptTestManifestTests(unittest.TestCase):
    def test_manifest_has_three_known_tags(self):
        manifest = load_script_test_manifest()
        self.assertEqual(1, manifest["schema"])
        self.assertEqual({"headless", "asset-contract", "mcp-live"}, set(manifest["tags"]))

    def test_all_mode_excludes_asset_and_live_tags(self):
        manifest = load_script_test_manifest()
        headless = set(discover_script_tests("headless"))
        self.assertTrue(headless)
        self.assertTrue(set(manifest["tags"]["asset-contract"]).isdisjoint(headless))
        self.assertTrue(set(manifest["tags"]["mcp-live"]).isdisjoint(headless))

    def test_tagged_files_exist_under_scripts(self):
        manifest = load_script_test_manifest()
        for tag, names in manifest["tags"].items():
            for name in names:
                self.assertTrue((Path(__file__).parent / name).is_file(), f"{tag}: {name}")


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Run the new contract test and observe the red state**

Run:

```powershell
python scripts/test_ai_production_loop.py
```

Expected before implementation: import failure for `SCRIPT_TEST_MANIFEST` or `load_script_test_manifest`. Record the failure, then implement the minimum API.

- [ ] **Step 3: Add the manifest**

Create `scripts/script-test-manifest.json`:

```json
{
  "schema": 1,
  "tags": {
    "headless": [],
    "asset-contract": [
      "test_animation_production_manifest.py",
      "test_animation_prompt_catalog.py",
      "test_animation_unit_profiles.py",
      "test_asian_village_demo_town_migration.py",
      "test_asian_village_migration.py",
      "test_asian_village_occlusion_material_pipeline.py",
      "test_asian_village_ue_audit.py",
      "test_battle_animation_pilot_pipeline.py",
      "test_battle_animation_production_import.py",
      "test_battle_animation_texture_memory_validator.py",
      "test_battle_backdrop_pipeline.py",
      "test_battle_camera_framing.py",
      "test_battle_party_qi_icon.py",
      "test_battle_render_budget_config.py",
      "test_battle_resource_psd_cuts.py",
      "test_battle_status_icon_imports.py",
      "test_battle_terrain_art.py",
      "test_battle_town_backdrop_pipeline.py",
      "test_battle_town_terrain_pipeline.py",
      "test_extract_party_deck_ppt_references.py",
      "test_fullscreen_battle_hud_asset_import.py",
      "test_gamexxk_map_ui_import_scripts.py",
      "test_gamexxk_prepare_animation_2k.py",
      "test_gamexxk_ui_calibration_v2.py",
      "test_gamexxk_ui_master_assets.py",
      "test_gamexxk_ui_master_build.py",
      "test_gamexxk_ui_master_contract.py",
      "test_gamexxk_ui_master_pages.py",
      "test_gamexxk_ui_master_validation.py",
      "test_hero_backpack_psd_package.py",
      "test_occlusion_cutout_pilot_assets.py",
      "test_party_deck_card_portrait_pipeline.py",
      "test_party_deck_character_assembly.py",
      "test_party_deck_sprite_atlas_packer.py",
      "test_party_deck_sprite_import_pipeline.py",
      "test_party_deck_sprite_manifest.py",
      "test_player_occlusion_reveal_assets.py",
      "test_prepare_animation_safe_frames.py",
      "test_psd_card_frame_pipeline.py",
      "test_psd_paper_ink_scrollbar_pipeline.py",
      "test_qingshan_b1_heightmap.py",
      "test_qingshan_b1_proxy_kit.py",
      "test_qingshan_building_concepts.py",
      "test_qingshan_dress_b1_assets.py",
      "test_qingshan_dress_b1_config.py",
      "test_qingshan_dress_b1_scripts.py",
      "test_qingshan_environment_assets.py",
      "test_qingshan_environment_source_probe.py",
      "test_qingshan_golden_inn.py",
      "test_qingshan_shop_import.py",
      "test_qingshan_town_pcg_config.py",
      "test_qingshan_town_pcg_scripts.py",
      "test_qingshan_whitebox_config.py",
      "test_qingshan_whitebox_scripts.py",
      "test_reference_faithful_task_ui_icons.py",
      "test_run_asian_village_migration.py",
      "test_run_qingshan_town_pcg_vertical_slice.py",
      "test_run_qingshan_whitebox_b0r.py",
      "test_seedance_seven_asset_pilot.py",
      "test_town_hud_psd_visual_contract.py",
      "test_town_psd_image_ops.py",
      "test_town_psd_import_manifest.py",
      "test_town_psd_package.py",
      "test_trigger_battle_animation_sample.py",
      "test_validate_map_ui_source_art.py",
      "test_validate_meta_shop_ui_v2.py"
    ],
    "mcp-live": [
      "test_gamexxk_real_play_flow_mcp.py",
      "test_gamexxk_real_play_flow_probe.py",
      "test_hp_hud_updates.py",
      "test_party_deck_real_play_acceptance.py",
      "test_qingshan_town_acceptance.py",
      "test_ue_pie_lifecycle.py"
    ]
  }
}
```

The empty `headless` array means “all discovered `test_*.py` not listed in the other two arrays”. It must not be used as an explicit allow-list, otherwise newly added tests silently leave the gate.

- [ ] **Step 4: Implement the tag loader and all-mode behavior**

Add to `scripts/ai_production_loop.py`:

```python
SCRIPT_TEST_MANIFEST = SCRIPT_DIR / "script-test-manifest.json"
SCRIPT_TEST_TAGS = ("headless", "asset-contract", "mcp-live")


def load_script_test_manifest() -> dict:
    manifest = json.loads(SCRIPT_TEST_MANIFEST.read_text(encoding="utf-8"))
    if manifest.get("schema") != 1 or set(manifest.get("tags", {})) != set(SCRIPT_TEST_TAGS):
        raise RuntimeError(f"invalid script test manifest: {SCRIPT_TEST_MANIFEST}")
    names = [name for tag in ("asset-contract", "mcp-live") for name in manifest["tags"][tag]]
    if len(names) != len(set(names)):
        raise RuntimeError("script test tags overlap")
    return manifest


def discover_script_tests(tag: str = "headless") -> list[str]:
    manifest = load_script_test_manifest()
    discovered = {
        path.name
        for path in SCRIPT_DIR.glob("test_*.py")
        if path.is_file() and "_archive" not in path.parts
    }
    tagged = set(manifest["tags"]["asset-contract"]) | set(manifest["tags"]["mcp-live"])
    if tag == "headless":
        return sorted(discovered - tagged)
    if tag not in SCRIPT_TEST_TAGS:
        raise ValueError(f"unknown script test tag: {tag}")
    return sorted(set(manifest["tags"][tag]) & discovered)
```

Change the existing `discover_script_tests()` call so `--script-tests all` calls `discover_script_tests("headless")`. Add `--script-test-tag` with choices `headless`, `asset-contract`, `mcp-live` for explicit tagged runs. Preserve comma-separated filename mode for focused tests. Include the selected tag in the report step name.

- [ ] **Step 5: Make JSON output safe on GBK consoles**

Replace the two final `ensure_ascii=False` JSON prints in `ai_production_loop.py` with `ensure_ascii=True`. Keep the report file UTF-8 and keep human-readable console output unchanged for non-JSON mode. In `test_gamexxk_ui_master_build.py`, add `encoding="utf-8", errors="replace"` to the `subprocess.run` call that invokes `build_gamexxk_ui_master.py`.

- [ ] **Step 6: Run the red-green gate**

```powershell
python scripts/test_ai_production_loop.py
python scripts/ai_production_loop.py --run-script-tests --script-tests all --json
python scripts/ai_production_loop.py --run-script-tests --script-test-tag asset-contract --json
python scripts/ai_production_loop.py --run-script-tests --script-test-tag mcp-live --json
```

Expected: manifest contract passes; `all` executes only unlisted headless tests and never launches UE; asset-contract/mcp-live commands are explicit and may report environment skips/failures without contaminating the default headless gate; JSON commands exit with valid JSON under the current console code page.

- [ ] **Step 7: Commit only the test-gate files**

```powershell
git diff --check
git add -- scripts/script-test-manifest.json scripts/ai_production_loop.py scripts/test_ai_production_loop.py scripts/test_gamexxk_ui_master_build.py
git diff --cached --name-only
git commit -m "test: separate headless and environment script gates"
```

## Task 4: Parameterize non-archive personal source paths

**Files:**

- Modify: `scripts/asian_village_migration.py`
- Modify: `scripts/run_asian_village_migration.py`
- Modify: `scripts/test_asian_village_migration.py`
- Modify: `scripts/test_run_asian_village_migration.py`

- [ ] **Step 1: Add explicit source and engine arguments**

Replace module-level personal source constants with functions that resolve in this order:

```python
def resolve_source_asset_dir(cli_value: str | None = None) -> Path:
    value = cli_value or os.environ.get("GAMEXXK_ASIAN_VILLAGE_SOURCE")
    if not value:
        raise RuntimeError(
            "Asian Village source is not configured; pass --source or set GAMEXXK_ASIAN_VILLAGE_SOURCE"
        )
    return Path(value).expanduser().resolve()
```

Apply the same precedence to `--ue54-root`/`GAMEXXK_UE54_ROOT` and `--ue58-root`/`GAMEXXK_UE_ROOT`. No default may point at `D:\UE5 demo\zzz`, `C:\Users\shxuw\Downloads`, or a user-specific temporary folder.

- [ ] **Step 2: Keep tests deterministic with temporary roots**

Update migration tests to pass temporary source and engine paths through the new resolver arguments. Add tests for: CLI value wins over env; env value works; missing source raises the exact configuration error; existing safe-copy behavior remains unchanged.

- [ ] **Step 3: Run and commit**

```powershell
python scripts/test_asian_village_migration.py
python scripts/test_run_asian_village_migration.py
rg -n --glob '*.py' 'D:\\UE5 demo\\zzz|C:\\Users\\shxuw\\Downloads|C:\\Users\\shxuw\\AppData\\Local\\Temp\\gamexxk' scripts --glob '!scripts/_archive/**'
git diff --check
git add -- scripts/asian_village_migration.py scripts/run_asian_village_migration.py scripts/test_asian_village_migration.py scripts/test_run_asian_village_migration.py
git diff --cached --name-only
git commit -m "chore: parameterize external asset migration paths"
```

Expected: the grep returns only intentional documentation/help examples or no matches; no external source is copied during tests.

## Task 5: Phase 0 final gate and production record

**Files:**

- Modify: `docs/production/2026-08-17-phase0-baseline.md`
- Modify: `docs/production/current-goal-acceptance.md`

- [ ] **Step 1: Run the complete Phase 0 gate**

```powershell
python scripts/harness_state_validator.py --json
python scripts/ai_production_loop.py --run-script-tests --script-tests all --json
python scripts/test_ai_production_loop.py
git diff --check
git status --short --branch
git diff --name-only -- Content/GameXXK/Maps/L_Main.umap
git diff --cached --name-only
```

The default gate is green only when validator, manifest contract, headless tests and whitespace check exit 0. `asset-contract` and `mcp-live` results are reported separately with explicit `SKIP`/environment status; they are not silently counted as headless passes.

- [ ] **Step 2: Update the evidence record**

Append exact command timestamps, report paths, headless pass/skip counts, tagged environment results, and a “not modified” list for `L_Main.umap`, `SourceAssets`, and `SourceArt`. Do not update the historical `598/598` number unless a new full Automation run was actually executed and its `index.json` is recorded.

- [ ] **Step 3: Commit and review the Phase 0 diff**

```powershell
git diff --check
git status --short --branch
git log -5 --oneline --decorate
git diff HEAD~1 --stat
```

The review must confirm that every Phase 0 commit contains only its listed files, the user’s tracked `.umap` remains unchanged, and no source-art directory was staged.

## Plan self-review

- **Spec coverage:** current source-of-truth, shelved old migration, script gates, GBK safety, personal paths, asset protection, and Phase 0 acceptance are each mapped to a task.
- **Completeness:** every task names exact files, commands, expected outputs, and commit boundaries; no unspecified action is required to pass Phase 0.
- **Scope:** this plan does not implement C++, PSD, Workbench Shell, idle rules, ChallengeViewport, or default-entry migration; those remain separate goal work packages.
- **Safety:** all write sets are explicit and never include `L_Main.umap`, `.uasset`, `SourceAssets`, or `SourceArt`.
