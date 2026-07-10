# Qingshan Shop Toon Import Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Import the approved Tripo high-detail, 51,110-quad shop into UE 5.8 and validate it with an isolated Substrate Toon BSDF material.

**Architecture:** Track the exact approved source archive under `SourceAssets/TownPCG/QingshanShopA`, extract working files under `Saved/ImportStaging`, and create all Unreal assets below a new prototype-only content folder so no tuned level or existing asset is touched. The tracked archive was copied byte-for-byte from the original external export at `C:/Users/shxuw/Downloads/SM_Qingshan_Shop_A_HQ_Retop50K_Quad_Textured.zip`; `source_manifest.json` records its SHA256 and import facts. A single idempotent editor Python script imports the FBX and albedo, creates a Toon Profile and Substrate Toon BSDF material, assigns the material, enables Nanite, saves the assets, and opens the mesh editor for visual inspection.

**Tech Stack:** Unreal Engine 5.8 Python API, UE 5.8 built-in MCP via `scripts/ue_mcp_client.py`, Substrate Toon BSDF, Toon Profile, FBX static-mesh import.

---

### Task 1: Stage the exported source files

**Files:**
- Read: `SourceAssets/TownPCG/QingshanShopA/SM_Qingshan_Shop_A_HQ_Retop50K_Quad_Textured.zip`
- Create: `Saved/ImportStaging/QingshanShopHQ/tripo_convert_cb771793-4261-40ff-8d41-55d783292210.fbx`
- Create: `Saved/ImportStaging/QingshanShopHQ/tripo_convert_cb771793-4261-40ff-8d41-55d783292210.fbm/tripo_rgb_007e7cb5-1303-4f32-a196-10ba100da0ce.jpg`

- [ ] **Step 1: Extract the archive to the isolated staging folder**

Run:

```powershell
Expand-Archive -LiteralPath 'D:\UE5 demo\GameXXK\SourceAssets\TownPCG\QingshanShopA\SM_Qingshan_Shop_A_HQ_Retop50K_Quad_Textured.zip' -DestinationPath 'D:\UE5 demo\GameXXK\Saved\ImportStaging\QingshanShopHQ' -Force
```

Expected: one FBX and one JPG exist under `Saved/ImportStaging/QingshanShopHQ`.

- [ ] **Step 2: Verify the staged source files**

Run:

```powershell
Get-ChildItem -LiteralPath 'D:\UE5 demo\GameXXK\Saved\ImportStaging\QingshanShopHQ' -Recurse -File | Select-Object FullName,Length
```

Expected: the FBX is about 3 MB and the JPG is about 3.8 MB.

### Task 2: Add the idempotent UE import script

**Files:**
- Create: `Content/Python/gamexxk_import_qingshan_shop_toon.py`

- [ ] **Step 1: Add argument and source validation**

The script must accept `--fbx` and `--albedo`, resolve them to existing files, and raise `RuntimeError` before changing assets when either path is invalid.

- [ ] **Step 2: Add deterministic asset import**

Use `unreal.AssetImportTask` with `unreal.FbxImportUI`: static mesh, combine meshes, no imported materials, no imported textures, automated, replace existing, and save disabled until configuration finishes. Import the JPG separately as `T_Qingshan_Shop_A_Albedo` with sRGB enabled.

- [ ] **Step 3: Create Toon Profile and Substrate material**

Create or load these assets under `/Game/GameXXK/Environment/TownPCG/Prototype/QingshanShopA`:

```text
TP_Qingshan_Building_Toon
M_Qingshan_Building_Toon
SM_Qingshan_Shop_A_HQ_Retop50K
T_Qingshan_Shop_A_Albedo
```

Create `unreal.MaterialExpressionTextureSampleParameter2D` and `unreal.MaterialExpressionSubstrateToonBSDF`, set the texture parameter to the imported albedo, set the Toon BSDF's `toon_profile`, connect RGB to Base Color, and connect the Toon BSDF output to `unreal.MaterialProperty.MP_FRONT_MATERIAL`. Set matte building defaults: metallic `0.0`, specular `0.15`, roughness `0.85`.

- [ ] **Step 4: Configure and save the static mesh**

Assign the material to slot zero, enable Nanite, keep the imported bottom-center pivot, rebuild the mesh, save all four packages, and open the static-mesh editor. Print a JSON result containing asset paths, material class, topology source label `Quad`, source face count `51110`, bounds, material-slot count, and Nanite state.

### Task 3: Execute through UE 5.8 MCP

**Files:**
- Execute: `Content/Python/gamexxk_import_qingshan_shop_toon.py`

- [ ] **Step 1: Verify MCP and stop PIE if necessary**

Run a short Python command that instantiates `UnrealMCPClient`, connects to port `18765`, prints `is_in_pie()`, and calls `stop_pie()` only when PIE is active.

- [ ] **Step 2: Run the project script through the approved MCP toolset**

Run `run_project_python_file` with:

```json
[
  "--fbx",
  "D:/UE5 demo/GameXXK/Saved/ImportStaging/QingshanShopHQ/tripo_convert_cb771793-4261-40ff-8d41-55d783292210.fbx",
  "--albedo",
  "D:/UE5 demo/GameXXK/Saved/ImportStaging/QingshanShopHQ/tripo_convert_cb771793-4261-40ff-8d41-55d783292210.fbm/tripo_rgb_007e7cb5-1303-4f32-a196-10ba100da0ce.jpg"
]
```

Expected: the returned JSON reports success and all four assets exist.

- [ ] **Step 3: Save dirty packages through MCP**

Call `save_dirty_packages()` and require `dirty_after` to be empty for the prototype packages.

### Task 4: Verify the prototype

**Files:**
- Verify: `/Game/GameXXK/Environment/TownPCG/Prototype/QingshanShopA/*`

- [ ] **Step 1: Run the import script a second time**

Expected: the operation remains successful and updates the same four assets without creating duplicates.

- [ ] **Step 2: Check editor logs**

Query recent UE log lines for `Error`, `Substrate`, and `QingshanShop`. Expected: no material compile error, missing texture, or failed import message.

- [ ] **Step 3: Visually inspect the static-mesh editor**

Expected: blue-grey roof, warm wood framing, cream walls, preserved curved eaves, no magenta missing texture, and stepped toon lighting rather than realistic glossy PBR.
