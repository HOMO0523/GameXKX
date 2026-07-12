# Stylized Eastern Village Migration Compatibility Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Safely copy the complete UE5.4 `Asian_Village` package into GameXXK, upgrade and validate it in UE5.8, and produce a categorized asset-audit map and evidence set without modifying the source project or existing Qingshan maps.

**Architecture:** A pure-Python migration layer owns filesystem inventory, hashing, staging and copy verification. A focused Unreal Python audit layer owns Asset Registry dependency inspection, package loading, blueprint/material checks and UE5.8 resave. Licensed vendor binaries remain a local ignored dependency under `/Game/Asian_Village`; only tooling, evidence metadata and GameXXK-owned audit assets are committed.

**Tech Stack:** Python 3.11+, `unittest`, UE5.4/UE5.8 Python Editor APIs, GameXXK UE MCP, Git local ignore policy, JSON evidence.

---

## Scope boundary

This is phase 1 of three independently testable plans. It ends when the complete vendor library loads in UE5.8 and the audit map is reviewable. Phase 2 will author GameXXK materials, building assemblies and PCG collections from the approved audit. Phase 3 will create `L_Qingshan_AsianVillage_Integration`, rebuild QuickRoad presentation and run gameplay/performance acceptance. Phase 2 and 3 receive their exact asset paths from this phase's audit report and must be planned after that report exists.

The following files are protected throughout this plan and must not change:

- `Content/GameXXK/Maps/L_QingshanInn.umap`
- `Content/GameXXK/Maps/Dev/L_Qingshan_PCG_Whitebox_B0R.umap`
- `Content/GameXXK/Maps/Dev/L_Qingshan_PCG_Dress_B1.umap`
- character, PaperZD, camera and HD2D assets
- `D:/UE5 demo/zzz/我的项目/Content/Asian_Village/**`

## Task 1: Local vendor-content policy and migration contract

**Files:**
- Modify: `.gitignore`
- Create: `docs/production/asian-village-local-dependency.md`
- Create: `scripts/test_asian_village_migration.py`
- Create: `scripts/asian_village_migration.py`

- [ ] **Step 1: Write failing policy tests**

Add tests that create a temporary Git-like project root and assert the contract constants and path guards:

```python
class MigrationContractTests(unittest.TestCase):
    def test_contract_uses_exact_roots_and_counts(self):
        self.assertEqual(SOURCE_ASSET_DIR.name, "Asian_Village")
        self.assertEqual(TARGET_RELATIVE_DIR.as_posix(), "Content/Asian_Village")
        self.assertEqual(EXPECTED_COUNTS, {"files": 505, "uasset": 503, "umap": 2})

    def test_target_must_be_inside_project_content(self):
        with self.assertRaisesRegex(MigrationError, "Content/Asian_Village"):
            resolve_target(Path("C:/outside"), Path("D:/project"))
```

- [ ] **Step 2: Run the tests to verify red state**

Run:

```powershell
python -m unittest scripts.test_asian_village_migration.MigrationContractTests -v
```

Expected: import failure because `scripts/asian_village_migration.py` does not exist.

- [ ] **Step 3: Add the explicit local dependency ignore after the existing `!Content/**/*.uasset` rules**

Append exactly:

```gitignore
# Licensed Fab dependency. Install locally from Stylized Eastern Village.
Content/Asian_Village/**
```

The rule must occur after the existing Content negation rules so `git check-ignore Content/Asian_Village/example.uasset` succeeds. Do not add a Git LFS rule and do not stage the 505 vendor packages. The remote repository is publicly readable and the largest BuiltData package is about 228 MB; raw redistribution is outside this plan.

- [ ] **Step 4: Implement the minimal migration contract**

```python
SOURCE_ASSET_DIR = Path(r"D:\UE5 demo\zzz\我的项目\Content\Asian_Village")
TARGET_RELATIVE_DIR = Path("Content") / "Asian_Village"
EXPECTED_COUNTS = {"files": 505, "uasset": 503, "umap": 2}

class MigrationError(RuntimeError):
    pass

def resolve_target(target: Path, project_root: Path) -> Path:
    root = project_root.resolve(strict=True)
    expected = (root / TARGET_RELATIVE_DIR).resolve(strict=False)
    candidate = target.resolve(strict=False)
    if candidate != expected or not candidate.is_relative_to(root / "Content"):
        raise MigrationError("target must be the exact project Content/Asian_Village directory")
    return candidate
```

- [ ] **Step 5: Write the local installation document**

The document must state:

- Fab listing URL and product name;
- source is a user-owned UE5.4 installation;
- vendor files are local-only and ignored by Git;
- collaborators must obtain the package from Fab and install it at `Content/Asian_Village`;
- vendor assets must not be uploaded to Tripo, GPT image generation or another external AI service;
- GameXXK-owned scripts, reports, maps and adapters remain tracked.

- [ ] **Step 6: Verify and commit**

Run:

```powershell
python -m unittest scripts.test_asian_village_migration.MigrationContractTests -v
git check-ignore -v Content/Asian_Village/example.uasset
git diff --check -- .gitignore docs/production/asian-village-local-dependency.md scripts/asian_village_migration.py scripts/test_asian_village_migration.py
```

Expected: tests pass; `git check-ignore` reports the final `Content/Asian_Village/**` rule.

Commit only these files:

```powershell
git add -- .gitignore docs/production/asian-village-local-dependency.md scripts/asian_village_migration.py scripts/test_asian_village_migration.py
git commit -m "build: define local eastern village dependency"
```

## Task 2: Deterministic inventory, staging and copy verification

**Files:**
- Modify: `scripts/asian_village_migration.py`
- Modify: `scripts/test_asian_village_migration.py`
- Create during execution: `docs/production/evidence/asian-village-migration/source-file-manifest.json`
- Create during execution: `docs/production/evidence/asian-village-migration/copied-file-manifest.json`

- [ ] **Step 1: Write failing inventory tests**

Use temporary directories with nested `.uasset`, `.umap` and non-Unreal files. Assert stable POSIX relative ordering, byte size, lowercase SHA-256, extension counts, rejection of symlinks/reparse points and rejection of files changing during hashing.

```python
def test_build_manifest_is_sorted_and_hashes_bytes(self):
    write(self.source / "meshes" / "B.uasset", b"b")
    write(self.source / "maps" / "A.umap", b"a")
    manifest = build_manifest(self.source)
    self.assertEqual([x["path"] for x in manifest["files"]], ["maps/A.umap", "meshes/B.uasset"])
    self.assertEqual(manifest["counts"], {"files": 2, "uasset": 1, "umap": 1})
```

- [ ] **Step 2: Run targeted tests and confirm failure**

```powershell
python -m unittest scripts.test_asian_village_migration.ManifestTests -v
```

Expected: failures name missing `build_manifest`, `write_manifest` and `verify_manifest`.

- [ ] **Step 3: Implement canonical manifests**

Define:

```python
def sha256_file(path: Path) -> str:
    with path.open("rb") as handle:
        before = os.fstat(handle.fileno())
        digest = hashlib.file_digest(handle, "sha256").hexdigest()
        after = os.fstat(handle.fileno())
    identity = (before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns)
    if identity != (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns):
        raise MigrationError(f"file changed while hashing: {path}")
    return digest

def build_manifest(source: Path) -> dict:
    root = source.resolve(strict=True)
    records = []
    for path in sorted(root.rglob("*"), key=lambda item: item.relative_to(root).as_posix()):
        if path.is_symlink():
            raise MigrationError(f"symlink is forbidden: {path}")
        if not path.is_file():
            continue
        relative = path.relative_to(root).as_posix()
        records.append({"path": relative, "bytes": path.stat().st_size, "sha256": sha256_file(path)})
    return {
        "schema_version": 1,
        "counts": {
            "files": len(records),
            "uasset": sum(item["path"].endswith(".uasset") for item in records),
            "umap": sum(item["path"].endswith(".umap") for item in records),
        },
        "files": records,
    }

def canonical_json_bytes(value: object) -> bytes:
    return (json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2) + "\n").encode("utf-8")

def write_manifest(path: Path, manifest: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_bytes(canonical_json_bytes(manifest))
    temporary.replace(path)

def verify_manifest(root: Path, manifest: dict) -> dict:
    actual = build_manifest(root)
    if actual["counts"] != manifest["counts"] or actual["files"] != manifest["files"]:
        raise MigrationError("manifest verification failed")
    return actual
```

`sha256_file` must open once, hash from the open handle, compare pre/post `fstat` identity, size, mtime and ctime, and raise `MigrationError` if the file changes. `build_manifest` must reject symlinks and store only canonical POSIX relative paths.

- [ ] **Step 4: Write failing atomic-copy tests**

Cover target already exists, staging directory already exists, copy failure, hash mismatch and successful promotion. A failed operation must leave no target directory and must not delete the source.

```python
def test_copy_failure_leaves_source_and_no_target(self):
    with self.assertRaises(MigrationError):
        stage_and_promote(self.source, self.target, self.staging, copier=failing_copier)
    self.assertTrue(self.source.exists())
    self.assertFalse(self.target.exists())
```

- [ ] **Step 5: Implement staging and promotion**

```python
def stage_and_promote(source: Path, target: Path, staging: Path, manifest: dict) -> dict:
    require_exact_target(target)
    require_empty_destination(target, staging)
    shutil.copytree(source, staging, copy_function=shutil.copy2)
    verify_manifest(staging, manifest)
    staging.replace(target)
    return build_manifest(target)
```

Before any recursive cleanup, resolve and verify `staging` is exactly `Saved/MigrationStaging/Asian_Village`; use `shutil.rmtree` only for that verified path after a failed copy. Never delete or move the source.

- [ ] **Step 6: Add CLI actions**

```powershell
python scripts/asian_village_migration.py inventory --source "D:\UE5 demo\zzz\我的项目\Content\Asian_Village" --output docs/production/evidence/asian-village-migration/source-file-manifest.json
python scripts/asian_village_migration.py copy --source "D:\UE5 demo\zzz\我的项目\Content\Asian_Village" --project-root "D:\UE5 demo\GameXXK" --manifest docs/production/evidence/asian-village-migration/source-file-manifest.json
python scripts/asian_village_migration.py verify --root "D:\UE5 demo\GameXXK\Content\Asian_Village" --manifest docs/production/evidence/asian-village-migration/source-file-manifest.json --output docs/production/evidence/asian-village-migration/copied-file-manifest.json
```

`inventory` must require the real source to match 505/503/2. `copy` must abort if the editor is running; the orchestration task will call it only after an MCP save and editor shutdown. `verify` runs before UE5.8 resaves packages because resave legitimately changes package hashes.

- [ ] **Step 7: Run tests and commit tooling**

```powershell
python -m unittest scripts.test_asian_village_migration -v
git diff --check -- scripts/asian_village_migration.py scripts/test_asian_village_migration.py
git add -- scripts/asian_village_migration.py scripts/test_asian_village_migration.py
git commit -m "feat: add deterministic eastern village migration"
```

Do not create or commit real evidence yet.

## Task 3: Unreal dependency and compatibility audit script

**Files:**
- Create: `Content/Python/gamexxk_audit_asian_village.py`
- Create: `scripts/test_asian_village_ue_audit.py`

- [ ] **Step 1: Write failing pure-helper and fake-Unreal tests**

Tests must cover asset-class counts, canonical package paths, dependency classification, external `/Game` dependency rejection, allowed `/Engine` dependencies, load failures, blueprint compile failures, material compile failures and deterministic report serialization.

```python
def test_private_game_dependency_is_blocking(self):
    result = classify_dependency("/Game/Characters/PrivateMesh")
    self.assertEqual(result, {"kind": "external_game", "blocking": True})

def test_engine_dependency_is_allowed(self):
    result = classify_dependency("/Engine/BasicShapes/Cube")
    self.assertEqual(result, {"kind": "engine", "blocking": False})
```

- [ ] **Step 2: Run tests and confirm missing module**

```powershell
python -m unittest scripts.test_asian_village_ue_audit -v
```

Expected: import failure for `Content.Python.gamexxk_audit_asian_village`.

- [ ] **Step 3: Implement mode-independent helpers**

Define:

```python
ASSET_ROOT = "/Game/Asian_Village"
MODES = ("source-readonly", "target-upgrade", "target-verify")

def classify_dependency(package_name: str) -> dict:
    if package_name.startswith(ASSET_ROOT + "/"):
        return {"kind": "internal", "blocking": False}
    if package_name.startswith("/Engine/") or package_name.startswith("/Script/"):
        return {"kind": "engine", "blocking": False}
    if package_name.startswith("/Game/"):
        return {"kind": "external_game", "blocking": True}
    return {"kind": "plugin_or_unknown", "blocking": False}

def summarize_assets(records: list[dict]) -> dict:
    class_counts: dict[str, int] = {}
    for record in records:
        name = record["class"]
        class_counts[name] = class_counts.get(name, 0) + 1
    return {"asset_count": len(records), "class_counts": dict(sorted(class_counts.items()))}

def canonical_report_bytes(report: dict) -> bytes:
    return (json.dumps(report, ensure_ascii=False, sort_keys=True, indent=2) + "\n").encode("utf-8")

def validate_report(report: dict, mode: str) -> None:
    if mode not in MODES or report.get("mode") != mode:
        raise AuditError("report mode is invalid")
    if report.get("asset_root") != ASSET_ROOT:
        raise AuditError("report asset_root is invalid")
    if type(report.get("asset_count")) is not int:
        raise AuditError("report asset_count must be an integer")
    if type(report.get("ok")) is not bool:
        raise AuditError("report ok must be a boolean")
```

The report schema must contain engine version, project path, mode, asset root, asset count, class counts, package records, external dependencies, load failures, blueprint failures, material failures, map failures, save failures and terminal `ok`.

- [ ] **Step 4: Implement Unreal Asset Registry audit**

Use:

```python
registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
assets = sorted(
    registry.get_assets_by_path(ASSET_ROOT, recursive=True),
    key=lambda item: str(item.package_name),
)
options = unreal.AssetRegistryDependencyOptions(
    include_soft_package_references=True,
    include_hard_package_references=True,
    include_searchable_names=False,
    include_soft_management_references=True,
    include_hard_management_references=True,
)
```

For each package, record class, object path, dependencies and whether `unreal.EditorAssetLibrary.load_asset` succeeds. In `source-readonly`, do not compile, save, rename, modify or call `save_dirty_packages`.

- [ ] **Step 5: Implement target upgrade checks**

In `target-upgrade` only:

- compile the two Blueprints with `unreal.BlueprintEditorLibrary.compile_blueprint` or `unreal.KismetEditorUtilities.compile_blueprint`;
- recompile material bases with `unreal.MaterialEditingLibrary.recompile_material`;
- load the two maps and execute Map Check;
- save `/Game/Asian_Village` recursively with `only_if_is_dirty=False`;
- record every failure and keep `ok=False` if any package cannot load or save.

In `target-verify`, load and validate without saving.

- [ ] **Step 6: Make report output explicit and safe**

Read `GAMEXXK_AV_AUDIT_MODE` and `GAMEXXK_AV_AUDIT_OUTPUT` from the environment. Resolve the output path and require it to live under either the source project's `Saved/AsianVillageAudit` or GameXXK `docs/production/evidence/asian-village-migration`. Write transactionally to `.tmp`, `flush`, `fsync` and replace.

- [ ] **Step 7: Run tests and static source guards**

```powershell
python -m unittest scripts.test_asian_village_ue_audit -v
python -m py_compile Content/Python/gamexxk_audit_asian_village.py
rg -n "delete_asset|rename_asset|migrate_packages" Content/Python/gamexxk_audit_asian_village.py
```

Expected: tests pass; forbidden mutation scan returns no matches. The source-readonly branch must not call save APIs.

- [ ] **Step 8: Commit**

```powershell
git add -- Content/Python/gamexxk_audit_asian_village.py scripts/test_asian_village_ue_audit.py
git commit -m "feat: audit eastern village unreal assets"
```

## Task 4: Safe editor lifecycle and migration orchestrator

**Files:**
- Create: `scripts/run_asian_village_migration.py`
- Create: `scripts/test_run_asian_village_migration.py`
- Reuse: `scripts/ue_mcp_client.py`
- Reuse: `scripts/ue_tdd_pipeline.py`

- [ ] **Step 1: Write failing phase-order tests**

Use injected callables and assert this exact order:

```python
EXPECTED_PHASES = [
    "preflight",
    "source_inventory",
    "save_target_editor",
    "close_target_editor",
    "source_ue54_audit",
    "copy_and_verify",
    "target_ue58_upgrade",
    "target_ue58_verify",
    "relaunch_target_editor",
]
```

Add failures for: editor running but MCP unavailable, dirty packages remain after save, source audit `ok=false`, target directory already exists, pre-resave hash mismatch, upgrade failure and verify failure. Later phases must not run after failure.

- [ ] **Step 2: Run tests and confirm red state**

```powershell
python -m unittest scripts.test_run_asian_village_migration -v
```

Expected: missing module or missing `run_migration`.

- [ ] **Step 3: Implement preflight**

Require these exact paths and fail with actionable JSON if missing:

```python
SOURCE_UPROJECT = Path(r"D:\UE5 demo\zzz\我的项目\我的项目.uproject")
UE54_CMD = Path(r"D:\UE_5.4\Engine\Binaries\Win64\UnrealEditor-Cmd.exe")
TARGET_UPROJECT = Path(r"D:\UE5 demo\GameXXK\GameXXK.uproject")
UE58_CMD = Path(r"D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe")
AUDIT_SCRIPT = Path(r"D:\UE5 demo\GameXXK\Content\Python\gamexxk_audit_asian_village.py")
```

Also hash protected maps/config before migration and store their digests in the run result.

- [ ] **Step 4: Implement safe editor save and close**

Reuse `UnrealMCPClient.save_dirty_packages()`. If the UE5.8 editor is running, connect through the project MCP, require `save_result=True` and `dirty_after=[]`, then request graceful exit with `execute_console_command("QUIT_EDITOR")`. Wait for the exact GameXXK UnrealEditor process to exit. Only after a confirmed save may the existing `kill_editor()` fallback be used; never terminate an editor whose dirty state could not be saved.

- [ ] **Step 5: Implement commandlet execution**

Run without shell string composition:

```python
cmd = [
    str(editor_cmd), str(uproject),
    f'-ExecutePythonScript={AUDIT_SCRIPT}',
    '-unattended', '-nop4', '-nosplash', '-NoSound',
    '-EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities',
]
subprocess.run(cmd, env={**os.environ, **audit_env}, check=False, text=True)
```

Set `GAMEXXK_AV_AUDIT_MODE` and `GAMEXXK_AV_AUDIT_OUTPUT` explicitly. Capture stdout/stderr to phase-specific logs under `Saved/AsianVillageMigration` and require both process exit code 0 and report `ok=true`.

- [ ] **Step 6: Implement the orchestration CLI**

```powershell
python scripts/run_asian_village_migration.py --preflight
python scripts/run_asian_village_migration.py --execute
python scripts/run_asian_village_migration.py --verify-only
```

`--preflight` is read-only. `--execute` requires target `Content/Asian_Village` absent and performs the exact phase order. `--verify-only` requires the migrated directory present and runs target verification without copying or saving source content.

- [ ] **Step 7: Implement relaunch and readiness check**

Reuse `launch_editor()` and `wait_for_mcp()` from `scripts/ue_tdd_pipeline.py` with `build=False`. Launch GameXXK UE5.8 with `-ModelContextProtocolStartServer`, wait for MCP, then call `save_dirty_packages` only if the target-upgrade phase left target packages dirty.

- [ ] **Step 8: Verify tests and commit**

```powershell
python -m unittest scripts.test_run_asian_village_migration scripts.test_asian_village_migration scripts.test_asian_village_ue_audit -v
git diff --check -- scripts/run_asian_village_migration.py scripts/test_run_asian_village_migration.py
git add -- scripts/run_asian_village_migration.py scripts/test_run_asian_village_migration.py
git commit -m "feat: orchestrate eastern village upgrade"
```

## Task 5: Execute the real migration and lock evidence

**Files:**
- Create: `Content/Asian_Village/**` (local ignored vendor files; never stage)
- Create: `docs/production/evidence/asian-village-migration/source-file-manifest.json`
- Create: `docs/production/evidence/asian-village-migration/source-ue54-audit.json`
- Create: `docs/production/evidence/asian-village-migration/copied-file-manifest.json`
- Create: `docs/production/evidence/asian-village-migration/target-ue58-upgrade.json`
- Create: `docs/production/evidence/asian-village-migration/target-ue58-verify.json`
- Create: `docs/production/evidence/asian-village-migration/upgraded-file-manifest.json`

- [ ] **Step 1: Run preflight and inspect the result**

```powershell
python scripts/run_asian_village_migration.py --preflight
```

Expected: source count 505/503/2; target absent; UE5.4/UE5.8 commandlets found; GameXXK editor state and MCP state reported; protected digests recorded.

- [ ] **Step 2: Execute migration**

```powershell
python scripts/run_asian_village_migration.py --execute
```

Expected terminal phase is `relaunch_target_editor`; all prior phases report `ok=true`. Before UE5.8 resave, copied manifest must byte-match the source manifest records. After resave, `upgraded-file-manifest.json` becomes the new target-byte baseline and may differ from UE5.4 hashes.

- [ ] **Step 3: Verify ignored vendor content**

```powershell
git status --short -- Content/Asian_Village
git check-ignore -v Content/Asian_Village/maps/Asian_Village_Demo.umap
```

Expected: no vendor files in `git status`; `git check-ignore` reports the explicit local dependency rule.

- [ ] **Step 4: Verify reports and protected files**

Run:

```powershell
python scripts/run_asian_village_migration.py --verify-only
python scripts/asian_village_migration.py verify --root "D:\UE5 demo\GameXXK\Content\Asian_Village" --manifest docs/production/evidence/asian-village-migration/upgraded-file-manifest.json
```

Expected: 505 target files; zero missing packages; zero blocking external `/Game` dependencies; zero load, blueprint, material, map and save failures; protected digests unchanged.

- [ ] **Step 5: Commit evidence only**

Review every report for machine-specific secrets; absolute local paths are allowed only for the documented source/target roots and must not include account tokens or browser data.

```powershell
git add -- docs/production/evidence/asian-village-migration/*.json
git diff --cached --name-only
git commit -m "docs: record eastern village migration evidence"
```

Expected staged files are JSON evidence only; no `Content/Asian_Village` package is staged.

## Task 6: UE5.8 asset-audit map

**Files:**
- Create: `Content/Python/gamexxk_build_asian_village_asset_audit.py`
- Create: `Content/Python/gamexxk_validate_asian_village_asset_audit.py`
- Create: `scripts/run_asian_village_asset_audit.py`
- Create: `scripts/test_asian_village_asset_audit.py`
- Create: `Content/GameXXK/Maps/Dev/L_AsianVillage_AssetAudit.umap` (generated)
- Create: `docs/production/evidence/asian-village-migration/asset-audit-map.json`

- [ ] **Step 1: Write failing layout tests**

The pure layout function must classify static meshes by package path and produce deterministic rows:

```python
CATEGORY_ORDER = ("building", "props", "trees", "plants", "cliff", "water", "sky")

def test_layout_has_one_record_per_static_mesh(self):
    layout = build_layout(self.asset_records)
    self.assertEqual(len(layout), 243)
    self.assertEqual([row["category"] for row in layout[:2]], ["building", "building"])
```

Add tests for 200/23/9/5/3/2/1 mesh category counts, stable 1000 cm grid spacing, collision disabled for audit-only placement and asset path uniqueness.

- [ ] **Step 2: Implement deterministic audit layout**

Create one row per static mesh from `target-ue58-verify.json`. Each category starts at a separate Y band; each row contains package path, category, transform and material/load status. The audit map must not contain gameplay actors, PCG, QuickRoad or imported Demo landscape.

- [ ] **Step 3: Author the map through UE MCP**

The Unreal script must:

- create an empty editor map at `/Game/GameXXK/Maps/Dev/L_AsianVillage_AssetAudit` only if absent;
- spawn labeled `StaticMeshActor` instances for all 243 meshes;
- add category text labels and a neutral light/sky setup owned by GameXXK;
- save only the new audit map;
- write `asset-audit-map.json` with actor count, category counts, missing meshes and map package.

Run with the project MCP script executor, not UnrealBridge.

- [ ] **Step 4: Validate the map**

The validator must load the audit map, require exactly 243 vendor mesh actors, compare every actor mesh path and transform with the deterministic layout, confirm no gameplay actors and confirm all referenced vendor assets load. `run_asian_village_asset_audit.py` must call the existing MCP project-script tool in three explicit phases: `assemble`, `validate`, and `capture`. Tests inject a fake MCP client and assert capture cannot run before a successful validation payload. The exact project-script call is:

```python
payload = client.run_project_python_file(
    relative_script,
    argv=argv,
    run_as_main=True,
)
```

Run:

```powershell
python -m unittest scripts.test_asian_village_asset_audit -v
python scripts/run_asian_village_asset_audit.py --assemble
python scripts/run_asian_village_asset_audit.py --validate
```

The runner imports `UnrealMCPClient` and calls its existing `run_project_python_file` method exactly as shown above; it must not assume `ue_mcp_client.py` exposes an unsupported `--script` CLI.

- [ ] **Step 5: Capture review evidence**

Capture exactly seven 1920×1080 overviews in `CATEGORY_ORDER` and store them under `docs/production/evidence/asian-village-migration/audit-captures/` as `building.png`, `props.png`, `trees.png`, `plants.png`, `cliff.png`, `water.png`, and `sky.png`. The runner must reuse the PNG integrity and foreground-window capture approach from `scripts/run_qingshan_dress_b1.py`, reject pre-existing capture files, and write `audit-captures.json` containing category, map, camera transform, dimensions, byte size and SHA-256. Run:

```powershell
python scripts/run_asian_village_asset_audit.py --capture
```

A capture is evidence, not automatic visual approval; record missing materials, wrong scale, wrong pivot, collision or shading problems in `asset-audit-map.json`.

- [ ] **Step 6: Commit GameXXK-owned audit artifacts**

```powershell
git add -- Content/Python/gamexxk_build_asian_village_asset_audit.py Content/Python/gamexxk_validate_asian_village_asset_audit.py scripts/run_asian_village_asset_audit.py scripts/test_asian_village_asset_audit.py Content/GameXXK/Maps/Dev/L_AsianVillage_AssetAudit.umap docs/production/evidence/asian-village-migration/asset-audit-map.json docs/production/evidence/asian-village-migration/audit-captures docs/production/evidence/asian-village-migration/audit-captures.json
git commit -m "feat: add eastern village asset audit map"
```

## Task 7: Phase-1 acceptance and handoff inputs

**Files:**
- Create: `scripts/validate_asian_village_phase1.py`
- Create: `scripts/test_validate_asian_village_phase1.py`
- Create: `docs/production/evidence/asian-village-migration/phase1-acceptance.json`
- Modify: `docs/production/asian-village-local-dependency.md`

- [ ] **Step 1: Write failing acceptance tests**

Acceptance must fail on any of the following: wrong source count, copy hash mismatch, target upgraded count not 505, blocking external dependency, load/compile/map/save failure, missing audit actor, protected digest drift, vendor file staged in Git or audit map missing.

```python
def test_vendor_file_staged_is_terminal_failure(self):
    result = validate_phase1(valid_evidence(), staged_paths=["Content/Asian_Village/meshes/X.uasset"])
    self.assertFalse(result["ok"])
    self.assertIn("licensed vendor package staged", result["errors"])
```

- [ ] **Step 2: Implement evidence-only validator**

The validator reads existing JSON and Git index state; it must not open Unreal or modify files. Output schema:

```json
{
  "schema_version": 1,
  "phase": "asian_village_migration_compatibility",
  "ok": true,
  "source_counts": {"files": 505, "uasset": 503, "umap": 2},
  "target_counts": {"files": 505, "uasset": 503, "umap": 2},
  "blocking_external_dependencies": [],
  "unreal_failures": [],
  "audit_map": "/Game/GameXXK/Maps/Dev/L_AsianVillage_AssetAudit",
  "protected_files_unchanged": true,
  "vendor_files_staged": []
}
```

- [ ] **Step 3: Run full phase verification**

```powershell
python -m unittest scripts.test_asian_village_migration scripts.test_asian_village_ue_audit scripts.test_run_asian_village_migration scripts.test_asian_village_asset_audit scripts.test_validate_asian_village_phase1 -v
python scripts/validate_asian_village_phase1.py --evidence-root docs/production/evidence/asian-village-migration --output docs/production/evidence/asian-village-migration/phase1-acceptance.json
git diff --check
```

Expected: all tests pass; acceptance has `ok=true`; any unrelated pre-existing global `git diff --check` failure must be reported with its exact file and must not be silently attributed to this phase.

- [ ] **Step 4: Record exact Phase-2 inputs**

Append to the local dependency document:

- paths of every compatible building, prop, tree, plant, cliff, water and spline Blueprint;
- per-asset material slots, bounds, triangle/LOD/Nanite state and collision state from the audit;
- visual-review disposition: approved, needs GameXXK material override, needs collision fix, needs scale/pivot wrapper or rejected;
- source Demo map reference only; no Demo landscape adoption.

These records are the concrete input for the separate GameXXK adaptation plan; do not author building assemblies during phase 1.

- [ ] **Step 5: Commit acceptance tooling and report**

```powershell
git add -- scripts/validate_asian_village_phase1.py scripts/test_validate_asian_village_phase1.py docs/production/evidence/asian-village-migration/phase1-acceptance.json docs/production/asian-village-local-dependency.md
git commit -m "test: accept eastern village migration phase"
```

## Final verification checklist

- [ ] `git status --short -- Content/Asian_Village` prints nothing.
- [ ] `git check-ignore` proves vendor packages are ignored.
- [ ] Source project Content hashes still match the source manifest.
- [ ] Target pre-resave copy evidence matches source bytes.
- [ ] Target upgraded manifest contains exactly 505 files.
- [ ] Source UE5.4 audit and target UE5.8 verification have no blocking failures.
- [ ] `L_AsianVillage_AssetAudit` contains exactly 243 categorized static mesh actors.
- [ ] Existing Qingshan and gameplay protected file hashes are unchanged.
- [ ] Phase-1 acceptance JSON has `ok=true`.
- [ ] No vendor binary or unrelated dirty file is staged.
- [ ] Phase-2 planning waits for user review of the audit captures and disposition report.
