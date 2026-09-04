# Selected Fonts UE Trial Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Import the approved all-language JiangHu display font plus the required glyph fallback into UE 5.8 as isolated trial Font assets, open it in the Font Editor, and collect deterministic import evidence without changing existing UI or maps.

**Architecture:** A focused project Python script under the MCP-approved `Content/Python` root drives `UFontFileImportFactory` through `AssetImportTask`. Each source produces one `UFontFace` and one Runtime `UFont` in `/Game/GameXXK/UI/Fonts/Trial`; the script validates class, source filename, package existence, and save results before opening both Runtime Font assets in the editor.

**Tech Stack:** Unreal Engine 5.8, Unreal Python, UE 5.8 MCP via `scripts/ue_mcp_client.py`, Slate Font Editor, PowerShell hash checks.

---

### Task 1: Preserve the approved source identity

**Files:**
- Read: `Saved/FontPreview/20260904/fonts/ZiKuJiangHuGuFeng.ttf`
- Read: `Saved/FontPreview/20260904/fonts/SourceHanSansCN-Regular.otf`
- Read: `Saved/FontPreview/20260904/font-manifest.json`
- Update: `Saved/FontPreview/20260904/verification.json`

- [x] **Step 1: Verify the two selected source files against the approved hashes**

Run:

```powershell
Get-FileHash -Algorithm SHA256 -LiteralPath 'Saved/FontPreview/20260904/fonts/ZiKuJiangHuGuFeng.ttf','Saved/FontPreview/20260904/fonts/Rancho-Regular.ttf'
```

Expected:

```text
ZiKuJiangHuGuFeng.ttf  8d1cead1150f42e5ac4d5a454a7897ea34e7ab801e6ea12035acb73e54cf06d4
SourceHanSansCN-Regular.otf  e2bc8a2e7f37474b774fff8db758681ece40bb6947a90d571bce9dd60671a8e4
```

- [x] **Step 2: Confirm the trial asset paths do not overwrite an existing production font**

The importer must use only these asset paths:

```text
/Game/GameXXK/UI/Fonts/Trial/FF_Trial_ZhHans_JiangHuGuFeng
/Game/GameXXK/UI/Fonts/Trial/FF_Trial_ZhHans_JiangHuGuFeng_Font
/Game/GameXXK/UI/Fonts/Trial/FF_Trial_Fallback_SourceHanSansCN_Regular
```

If a path already exists, accept it only when the asset class matches the corresponding Font Face or Runtime Font class; otherwise stop without deleting or renaming the existing asset.

### Task 2: Author the idempotent UE import driver

**Files:**
- Create: `Content/Python/gamexxk_import_selected_font_trial.py`
- Create during execution: `Saved/FontPreview/20260904/ue-import-result.json`

- [x] **Step 1: Write the importer**

Create a Python entry point with two fixed records: all display languages → `ZiKuJiangHuGuFeng.ttf`, and fallback → `SourceHanSansCN-Regular.otf`. The display record creates a companion Runtime Font; the fallback record creates only a Font Face. It also removes the rejected Rancho trial pair when present. For each selected record it must:

1. verify the exact SHA-256 above;
2. instantiate `unreal.FontFileImportFactory`;
3. set `batch_create_font_asset` to `CREATE_IF_NO_FONT_EXISTS`;
4. import with `automated=True`, `replace_existing=True`, and `save=False`;
5. require the face asset class to be `FontFace` and the generated companion asset class to be `Font`;
6. save both exact packages;
7. emit one JSON record containing culture, source, source hash, face path, optional Font path, class names, source filename, and save results;
8. configure the Runtime Font's fallback typeface to the imported Source Han Sans face through `ToolsetLibrary`, verify its resolved object path, and save it;
9. close any stale editor instance and reopen the companion Runtime Font asset through `AssetEditorSubsystem` after all validations pass.

Failures must be printed as a JSON object with `ok=false` and must raise after writing `ue-import-result.json`; partial success must never be reported as completion.

- [x] **Step 2: Check the Python file without importing**

Run:

```powershell
python -m py_compile Content/Python/gamexxk_import_selected_font_trial.py
```

Expected: exit code `0` and no output.

### Task 3: Import and save through UE 5.8 MCP

**Files:**
- Create: `Content/GameXXK/UI/Fonts/Trial/FF_Trial_ZhHans_JiangHuGuFeng.uasset`
- Create: `Content/GameXXK/UI/Fonts/Trial/FF_Trial_ZhHans_JiangHuGuFeng_Font.uasset`
- Create: `Content/GameXXK/UI/Fonts/Trial/FF_Trial_Fallback_SourceHanSansCN_Regular.uasset`

- [x] **Step 1: Launch the canonical editor and MCP server**

Run `Launch_GameXXK_Editor.cmd`. Wait for `python scripts/ue_mcp_client.py` to connect to `http://127.0.0.1:18765/mcp`.

Expected: editor opens `/Game/GameXXK/Maps/L_DesktopTrainingHUD`; MCP lists the GameXXK TDD toolset.

- [x] **Step 2: Run the importer through MCP**

Call `run_project_python_file` with:

```text
Content/Python/gamexxk_import_selected_font_trial.py
```

Expected: `ue-import-result.json` records `ok=true`, exactly two Font Face assets, exactly one Runtime Font asset, all three package saves as successful, one fallback entry points to the Source Han Sans face, and rejected Rancho paths are absent.

- [x] **Step 3: Save dirty packages through MCP**

Call `save_dirty_packages` after the importer. Expected: no trial font package remains dirty.

### Task 4: Verify the UE-native trial

**Files:**
- Update: `Saved/FontPreview/20260904/verification.json`
- Create: `Saved/HarnessReports/20260904-selected-fonts-ue-trial.md`

- [x] **Step 1: Verify the generated packages on disk and in Asset Registry**

Require all three `.uasset` files to exist and have non-zero size. The importer result must report matching expected classes and source filenames. Recompute the two selected source hashes and compare them with the manifest.

- [x] **Step 2: Inspect both Runtime Font assets in the UE Font Editor**

Leave the trial Runtime Font asset open in UE. Confirm the Font Editor renders the JiangHu face without a load failure or missing-resource error. Verify that `−`, `×`, and `→` resolve through the Source Han Sans fallback instead of rendering missing-glyph boxes. This is a Font Editor trial; it does not claim that existing UMG widgets have been changed.

- [x] **Step 3: Record the result and stop at the trial boundary**

Write an acceptance note containing exact asset paths, source hashes, class names, package sizes, editor/MCP status, and the remaining boundary: no UI assignment, no localization resource, no map edit, no C++ change, and no packaged-build claim.

- [x] **Step 4: Inspect repository scope**

Run:

```powershell
git status --short -- Content/GameXXK/UI/Fonts/Trial Content/Python/gamexxk_import_selected_font_trial.py Saved/FontPreview/20260904 docs/superpowers/plans/2026-09-04-selected-fonts-ue-trial.md
```

Expected: only the importer and three new trial `.uasset` files are visible under `Content`; `Saved` evidence remains Git-ignored; no existing UI asset appears as modified by this trial.
