# Desktop Workbench UI Art Pilot Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce one reviewable, self-contained art handoff package for the current pure-2D desktop workbench backpack-expanded screen without modifying project code or Unreal assets.

**Architecture:** Capture the real PIE Slate window, prepare a deterministic backpack-expanded runtime state, inspect the live programmatic widget tree for visible texture brushes, and export each referenced `UTexture2D` as a same-named PNG. Build the annotation guide as a deterministic overlay on a copy of the screenshot, verify it with the required Luna visual agent, then archive and hash the package.

**Tech Stack:** Unreal Engine 5.8 MCP, existing `scripts/ue_mcp_client.py` and `scripts/gamexxk_vision_pie.py`, transient Unreal Python under `Saved/Codex`, host Python 3 with Pillow 11.3, PowerShell 7, and Luna visual review through `codex_vision.ps1`.

---

## Scope and file map

This is pure art/export work. Per `AGENTS.md`, do not use TDD and do not compile. Verification is by dimensions, alpha, hashes, manifests, archive extraction, and visual review.

Read only:

- `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`
- `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h`
- `SourceArt/UI/PSD/gamexxk-v4/ui-master/final-approved-runtime-assets-manifest.json`
- `docs/production/current-goal-acceptance.md`
- `scripts/ue_mcp_client.py`
- `scripts/gamexxk_vision_pie.py`

Create as transient evidence only:

- `Saved/Codex/ui-art-handoff-20260828/export_live_workbench_textures.py`
- `Saved/Codex/ui-art-handoff-20260828/render_pilot_guide.py`
- `Saved/Codex/ui-art-handoff-20260828/DesktopTrainingWorkbench_Default_Runtime.png`
- `Saved/HarnessReports/ui-art-handoff-20260828/runtime-texture-report.json`
- `Saved/HarnessReports/ui-art-handoff-20260828/capture-report.json`
- `Saved/HarnessReports/ui-art-handoff-20260828/luna-screen-review.md`
- `Saved/HarnessReports/ui-art-handoff-20260828/luna-guide-review.md`

Create as the user-facing delivery:

- `Deliverables/GameXXK_UI_MainFlow_Art_Handoff_20260828/01_DesktopTrainingWorkbench_Default/01_Runtime_Screenshot/DesktopTrainingWorkbench_Default_Runtime.png`
- `Deliverables/GameXXK_UI_MainFlow_Art_Handoff_20260828/01_DesktopTrainingWorkbench_Default/02_Assets/*.png`
- `Deliverables/GameXXK_UI_MainFlow_Art_Handoff_20260828/01_DesktopTrainingWorkbench_Default/03_Annotation_Guide/Guide_DesktopTrainingWorkbench_Default.png`
- `Deliverables/GameXXK_UI_MainFlow_Art_Handoff_20260828/01_DesktopTrainingWorkbench_Default/asset_manifest.csv`
- `Deliverables/GameXXK_UI_MainFlow_Art_Handoff_20260828/01_DesktopTrainingWorkbench_Default/README_替换说明.md`
- `Deliverables/GameXXK_UI_MainFlow_Art_Handoff_20260828/SHA256SUMS.csv`
- `Deliverables/GameXXK_UI_MainFlow_Art_Handoff_20260828.zip`

Never save dirty packages, stop/restart the editor, alter a map, import an image, or write under `Content`, `Source`, `Config`, or `SourceArt` during execution.

### Task 1: Preflight the running pure-2D UI and create isolated output directories

**Files:**

- Create: `Saved/Codex/ui-art-handoff-20260828/`
- Create: `Saved/HarnessReports/ui-art-handoff-20260828/`
- Create: `Deliverables/GameXXK_UI_MainFlow_Art_Handoff_20260828/01_DesktopTrainingWorkbench_Default/`

- [ ] **Step 1: Record the current repository and editor state without changing it**

Run:

```powershell
git status --short --branch
Get-Process UnrealEditor -ErrorAction Stop | Select-Object Id,StartTime,MainWindowTitle,Path
Test-NetConnection -ComputerName 127.0.0.1 -Port 18765 -InformationLevel Quiet
```

Expected: branch is `main`; the existing user changes remain visible; one `GameXXK Preview` process is present; port `18765` returns `True`.

- [ ] **Step 2: Confirm the UE MCP is usable without saving or restarting**

Run:

```powershell
python scripts/ue_mcp_smoke.py --report Saved/HarnessReports/ui-art-handoff-20260828/mcp-smoke.json
```

Expected: the smoke report is written and the required editor, Slate, and GameXXK toolsets are available.

- [ ] **Step 3: Create only the new delivery and evidence directories**

Run:

```powershell
$paths = @(
  'Saved/Codex/ui-art-handoff-20260828',
  'Saved/HarnessReports/ui-art-handoff-20260828',
  'Deliverables/GameXXK_UI_MainFlow_Art_Handoff_20260828/01_DesktopTrainingWorkbench_Default/01_Runtime_Screenshot',
  'Deliverables/GameXXK_UI_MainFlow_Art_Handoff_20260828/01_DesktopTrainingWorkbench_Default/02_Assets',
  'Deliverables/GameXXK_UI_MainFlow_Art_Handoff_20260828/01_DesktopTrainingWorkbench_Default/03_Annotation_Guide'
)
$paths | ForEach-Object { New-Item -ItemType Directory -Path $_ -Force | Out-Null }
```

Expected: all five directories exist and no existing project file changes.

### Task 2: Prepare and capture the exact pilot state

**Files:**

- Create: `Saved/Codex/ui-art-handoff-20260828/DesktopTrainingWorkbench_Default_Runtime.png`
- Create: `Saved/HarnessReports/ui-art-handoff-20260828/capture-report.json`
- Create: `Deliverables/GameXXK_UI_MainFlow_Art_Handoff_20260828/01_DesktopTrainingWorkbench_Default/01_Runtime_Screenshot/DesktopTrainingWorkbench_Default_Runtime.png`

- [ ] **Step 1: Reset only the runtime workbench shell and open its backpack**

Run this one-off command through the existing MCP client:

```powershell
python -c "import sys; sys.path.insert(0, r'scripts'); from ue_mcp_client import UnrealMCPClient; c=UnrealMCPClient(timeout=60); c.require_connected(); code=\"import unreal; w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world(); pc=unreal.GameplayStatics.get_player_controller(w,0); ui=pc.get_desktop_training_workbench_widget_for_test(); ui.close_workbench(); ui.open_workbench(); ui.open_backpack(); print({'expanded':ui.is_backpack_expanded_for_test(),'warehouse':ui.is_warehouse_panel_open_for_test(),'right':ui.is_right_panel_open_for_test(),'settings':ui.is_settings_panel_open_for_test(),'exit':ui.is_exit_confirmation_open_for_test()})\"; print(c.execute_console_command('py '+code))"
```

Expected runtime-only state: `expanded=True`, `warehouse=False`, `right=False`, `settings=False`, `exit=False`. Do not call `save_dirty_packages`.

The PIE world reported by the later runtime inventory must end in `L_DesktopTrainingHUD`; reject a capture from any 3D Town or legacy map.

- [ ] **Step 2: Capture the real PIE Slate window**

Run:

```powershell
python scripts/gamexxk_vision_pie.py --capture-only --name ui-art-handoff-20260828/DesktopTrainingWorkbench_Default_Runtime.png --output Saved/HarnessReports/ui-art-handoff-20260828/capture-report.json --json
Copy-Item -LiteralPath 'Saved/Codex/ui-art-handoff-20260828/DesktopTrainingWorkbench_Default_Runtime.png' -Destination 'Deliverables/GameXXK_UI_MainFlow_Art_Handoff_20260828/01_DesktopTrainingWorkbench_Default/01_Runtime_Screenshot/DesktopTrainingWorkbench_Default_Runtime.png' -Force
```

Expected: the capture report has `ok: true`; both PNG paths exist and have the same SHA-256.

- [ ] **Step 3: Ask Luna to validate the target state before asset work**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File 'C:\Users\shxuw\.claude\skills\codex-vision\scripts\codex_vision.ps1' -Prompt '这是 GameXXK 当前纯2D主流程的样板截图。请以美术交付取证标准检查：1) 是否为桌面工作台背包展开态；2) 是否没有旧3D Town、路线图、战斗板、Tooltip、鼠标悬停或调试层干扰；3) 列出画面中需要单独交付的主要图片/图标区域。只报告可见证据，不推测代码。' -Images 'Deliverables/GameXXK_UI_MainFlow_Art_Handoff_20260828/01_DesktopTrainingWorkbench_Default/01_Runtime_Screenshot/DesktopTrainingWorkbench_Default_Runtime.png' -Effort max -MaxDim 1920 -Out 'Saved/HarnessReports/ui-art-handoff-20260828/luna-screen-review.md' -Workspace 'D:\UE5 demo\GameXXK'
```

Expected: Luna explicitly confirms the screen state or identifies a concrete visible mismatch. If it identifies a mismatch, repeat Steps 1–3 after correcting only the runtime UI state.

### Task 3: Export the textures actually used by the visible workbench

**Files:**

- Create: `Saved/Codex/ui-art-handoff-20260828/export_live_workbench_textures.py`
- Create: `Saved/HarnessReports/ui-art-handoff-20260828/runtime-texture-report.json`
- Create: `Deliverables/GameXXK_UI_MainFlow_Art_Handoff_20260828/01_DesktopTrainingWorkbench_Default/02_Assets/*.png`

- [ ] **Step 1: Create the transient live-widget exporter under `Saved`**

Create `Saved/Codex/ui-art-handoff-20260828/export_live_workbench_textures.py` with this exact content:

```python
from __future__ import annotations

import json
from pathlib import Path

import unreal

PROJECT = Path(r"D:\UE5 demo\GameXXK")
ASSET_DIR = PROJECT / "Deliverables/GameXXK_UI_MainFlow_Art_Handoff_20260828/01_DesktopTrainingWorkbench_Default/02_Assets"
REPORT = PROJECT / "Saved/HarnessReports/ui-art-handoff-20260828/runtime-texture-report.json"

BRUSH_FIELDS = (
    "normal", "hovered", "pressed", "disabled",
    "background_image", "fill_image", "marquee_image",
    "normal_bar_image", "hovered_bar_image",
    "normal_thumb_image", "hovered_thumb_image", "disabled_thumb_image",
    "unchecked_image", "unchecked_hovered_image", "unchecked_pressed_image",
    "checked_image", "checked_hovered_image", "checked_pressed_image",
    "undetermined_image", "undetermined_hovered_image", "undetermined_pressed_image",
)
WIDGET_FIELDS = ("brush", "background", "widget_style", "widget_bar_style")


def prop(obj, name):
    try:
        return obj.get_editor_property(name)
    except Exception:
        return None


def object_path(obj):
    try:
        return obj.get_path_name()
    except Exception:
        return ""


def xy(value):
    if value is None:
        return [0.0, 0.0]
    return [float(value.x), float(value.y)]


def geometry(widget):
    try:
        value = widget.get_cached_geometry()
        position = getattr(value, "absolute_position", None)
        size = getattr(value, "absolute_size", None)
        if position is None and hasattr(value, "get_absolute_position"):
            position = value.get_absolute_position()
        if size is None and hasattr(value, "get_absolute_size"):
            size = value.get_absolute_size()
        return {"position": xy(position), "size": xy(size)}
    except Exception:
        return {"position": [0.0, 0.0], "size": [0.0, 0.0]}


def visible(widget):
    try:
        return bool(widget.is_visible())
    except Exception:
        return "COLLAPSED" not in str(widget.get_visibility()).upper()


def add_texture(value, field, widget, found):
    if isinstance(value, unreal.Texture2D):
        path = object_path(value)
        found.setdefault(path, {"asset": value, "uses": []})["uses"].append({
            "widget_name": str(widget.get_name()),
            "widget_class": str(widget.get_class().get_name()),
            "field": field,
            "geometry": geometry(widget),
        })
        return
    resource = prop(value, "resource_object")
    if isinstance(resource, unreal.Texture2D):
        add_texture(resource, field + ".resource_object", widget, found)
    for brush_field in BRUSH_FIELDS:
        child = prop(value, brush_field)
        if child is not None:
            add_texture(child, field + "." + brush_field, widget, found)


def main():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if world is None:
        raise RuntimeError("PIE game world is unavailable")
    controller = unreal.GameplayStatics.get_player_controller(world, 0)
    workbench = controller.get_desktop_training_workbench_widget_for_test()
    if workbench is None:
        raise RuntimeError("desktop training workbench is unavailable")
    workbench.force_layout_prepass()
    tree = prop(workbench, "widget_tree")
    if tree is None:
        tree = workbench.get_widget_tree()
    root = prop(tree, "root_widget")
    widgets = list(tree.get_all_widgets())
    found = {}
    for widget in widgets:
        if not visible(widget):
            continue
        for field in WIDGET_FIELDS:
            value = prop(widget, field)
            if value is not None:
                add_texture(value, field, widget, found)

    ASSET_DIR.mkdir(parents=True, exist_ok=True)
    rows = []
    names = {}
    for path in sorted(found):
        texture = found[path]["asset"]
        name = str(texture.get_name())
        destination_dir = ASSET_DIR
        previous = names.get(name)
        if previous and previous != path:
            destination_dir = ASSET_DIR / path.split(".", 1)[0].strip("/").replace("/", "__")
            destination_dir.mkdir(parents=True, exist_ok=True)
        names[name] = path
        destination = destination_dir / f"{name}.png"
        task = unreal.AssetExportTask()
        task.object = texture
        task.filename = str(destination)
        task.automated = True
        task.prompt = False
        task.replace_identical = True
        task.exporter = unreal.TextureExporterPNG()
        if not unreal.Exporter.run_asset_export_task(task):
            raise RuntimeError(f"texture export failed: {path}")
        rows.append({
            "ue_asset_path": path,
            "asset_name": name,
            "asset_file": str(destination),
            "width": int(texture.blueprint_get_size_x()),
            "height": int(texture.blueprint_get_size_y()),
            "uses": found[path]["uses"],
        })

    payload = {
        "ok": True,
        "map": object_path(world),
        "screen": "DesktopTrainingWorkbench_Default",
        "state": {
            "expanded": bool(workbench.is_backpack_expanded_for_test()),
            "warehouse": bool(workbench.is_warehouse_panel_open_for_test()),
            "right_panel": bool(workbench.is_right_panel_open_for_test()),
            "settings": bool(workbench.is_settings_panel_open_for_test()),
            "exit_confirmation": bool(workbench.is_exit_confirmation_open_for_test()),
        },
        "root_geometry": geometry(root),
        "asset_count": len(rows),
        "assets": rows,
    }
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps({"ok": True, "asset_count": len(rows), "report": str(REPORT)}, ensure_ascii=False))


main()
```

- [ ] **Step 2: Execute the transient exporter inside the running editor**

Run:

```powershell
python -c "import sys; sys.path.insert(0, r'scripts'); from ue_mcp_client import UnrealMCPClient; c=UnrealMCPClient(timeout=180); c.require_connected(); p=r'D:\UE5 demo\GameXXK\Saved\Codex\ui-art-handoff-20260828\export_live_workbench_textures.py'; command=\"py exec(compile(open(r'\"+p+\"', encoding='utf-8').read(), r'\"+p+\"', 'exec'))\"; print(c.execute_console_command(command))"
```

Expected: `runtime-texture-report.json` has `ok: true`, its state matches Task 2, and every reported `asset_file` exists. If an Unreal method name differs in UE 5.8, adjust only the transient `Saved` script and rerun; do not edit project Python or C++.

- [ ] **Step 3: Cross-check runtime output against the workbench resource constants**

Run:

```powershell
rg -n "static constexpr const TCHAR\* .*TexturePath" Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp
Get-Content -Raw -LiteralPath 'Saved/HarnessReports/ui-art-handoff-20260828/runtime-texture-report.json' | ConvertFrom-Json | Select-Object -ExpandProperty assets | Select-Object asset_name,ue_asset_path,width,height
```

Expected: all textures visible in the screenshot have a runtime report entry. Constants belonging only to collapsed, hidden, right-panel, route, battle, Town, fallback, or confirmation states are not copied into this screen folder.

The runtime rows must retain full `/Game/...` paths. Generic shell assets such as `T_MasterV2_PanelLarge` are exported only when a visible widget brush proves that this pilot state uses them.

### Task 4: Build the manifest, replacement notes, and deterministic annotation guide

**Files:**

- Create: `Saved/Codex/ui-art-handoff-20260828/render_pilot_guide.py`
- Create: `Deliverables/GameXXK_UI_MainFlow_Art_Handoff_20260828/01_DesktopTrainingWorkbench_Default/asset_manifest.csv`
- Create: `Deliverables/GameXXK_UI_MainFlow_Art_Handoff_20260828/01_DesktopTrainingWorkbench_Default/README_替换说明.md`
- Create: `Deliverables/GameXXK_UI_MainFlow_Art_Handoff_20260828/01_DesktopTrainingWorkbench_Default/03_Annotation_Guide/Guide_DesktopTrainingWorkbench_Default.png`

- [ ] **Step 1: Create a deterministic host-side guide renderer under `Saved`**

Create `Saved/Codex/ui-art-handoff-20260828/render_pilot_guide.py` with these rules encoded directly:

```python
from __future__ import annotations

import csv
import hashlib
import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

PROJECT = Path(r"D:\UE5 demo\GameXXK")
SCREEN_DIR = PROJECT / "Deliverables/GameXXK_UI_MainFlow_Art_Handoff_20260828/01_DesktopTrainingWorkbench_Default"
SCREENSHOT = SCREEN_DIR / "01_Runtime_Screenshot/DesktopTrainingWorkbench_Default_Runtime.png"
ASSET_DIR = SCREEN_DIR / "02_Assets"
GUIDE = SCREEN_DIR / "03_Annotation_Guide/Guide_DesktopTrainingWorkbench_Default.png"
MANIFEST = SCREEN_DIR / "asset_manifest.csv"
REPORT = PROJECT / "Saved/HarnessReports/ui-art-handoff-20260828/runtime-texture-report.json"
FONT_PATH = Path(r"C:\Windows\Fonts\NotoSansSC-VF.ttf")
COLORS = ((255, 73, 73), (52, 211, 153), (59, 130, 246), (250, 204, 21), (192, 132, 252), (251, 146, 60))


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def transform(rect, root, shot_size):
    root_x, root_y = root["position"]
    root_w, root_h = root["size"]
    x, y = rect["position"]
    w, h = rect["size"]
    sx = shot_size[0] / root_w
    sy = shot_size[1] / root_h
    return (
        int(round((x - root_x) * sx)),
        int(round((y - root_y) * sy)),
        int(round((x - root_x + w) * sx)),
        int(round((y - root_y + h) * sy)),
    )


payload = json.loads(REPORT.read_text(encoding="utf-8"))
shot = Image.open(SCREENSHOT).convert("RGB")
canvas = Image.new("RGB", (shot.width + 760, shot.height), (20, 20, 24))
canvas.paste(shot, (0, 0))
draw = ImageDraw.Draw(canvas)
font = ImageFont.truetype(str(FONT_PATH), 20)
small = ImageFont.truetype(str(FONT_PATH), 16)
title = ImageFont.truetype(str(FONT_PATH), 28)
draw.text((shot.width + 28, 24), "Desktop Workbench 素材指引", font=title, fill=(255, 255, 255))
draw.text((shot.width + 28, 64), "编号 → 项目内同名 PNG", font=font, fill=(180, 180, 190))

rows = []
legend_y = 108
for index, asset in enumerate(payload["assets"], 1):
    region_id = f"A{index:02d}"
    color = COLORS[(index - 1) % len(COLORS)]
    uses = asset["uses"]
    rects = []
    for use in uses:
        rect = transform(use["geometry"], payload["root_geometry"], shot.size)
        if rect[2] > rect[0] and rect[3] > rect[1]:
            rects.append(rect)
            draw.rectangle(rect, outline=color, width=3)
    if rects:
        x1, y1, _, _ = rects[0]
        draw.rectangle((x1, max(0, y1 - 25), x1 + 48, y1), fill=color)
        draw.text((x1 + 4, max(0, y1 - 24)), region_id, font=small, fill=(0, 0, 0))
    filename = Path(asset["asset_file"]).name
    draw.rectangle((shot.width + 28, legend_y + 4, shot.width + 44, legend_y + 20), fill=color)
    draw.text((shot.width + 54, legend_y), f"{region_id}  {filename}", font=small, fill=(245, 245, 245))
    legend_y += 27
    if legend_y > shot.height - 34:
        raise RuntimeError("legend exceeds guide height; reduce font or split the guide before delivery")
    image = Image.open(asset["asset_file"])
    has_alpha = image.mode in ("RGBA", "LA") or "transparency" in image.info
    widget_names = sorted({use["widget_name"] for use in uses})
    usage = sorted({f"{use['widget_name']}:{use['field']}" for use in uses})
    rows.append({
        "region_id": region_id,
        "screen_name": "DesktopTrainingWorkbench_Default",
        "asset_filename": filename,
        "ue_asset_path": asset["ue_asset_path"],
        "source_art_path": "",
        "width": image.width,
        "height": image.height,
        "color_mode": image.mode,
        "has_alpha": str(bool(has_alpha)).lower(),
        "shared": str(len(uses) > 1).lower(),
        "usage": "; ".join(usage),
        "notes": f"visible_instances={len(uses)}; widgets={'; '.join(widget_names)}",
        "sha256": sha256(Path(asset["asset_file"])),
    })

draw.text((shot.width + 28, shot.height - 64), "C01  CODE_DRAWN：文字、数值、纯色进度和程序边框", font=small, fill=(180, 180, 190))
rows.append({
    "region_id": "C01",
    "screen_name": "DesktopTrainingWorkbench_Default",
    "asset_filename": "CODE_DRAWN",
    "ue_asset_path": "",
    "source_art_path": "",
    "width": "",
    "height": "",
    "color_mode": "",
    "has_alpha": "",
    "shared": "true",
    "usage": "runtime text; numbers; solid fills; progress bars; programmatic borders",
    "notes": "No replacement PNG. Do not bake these elements into texture assets.",
    "sha256": "",
})

GUIDE.parent.mkdir(parents=True, exist_ok=True)
canvas.save(GUIDE, format="PNG", optimize=True)
with MANIFEST.open("w", encoding="utf-8-sig", newline="") as stream:
    writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
    writer.writeheader()
    writer.writerows(rows)
print(json.dumps({"ok": True, "asset_count": len(rows), "guide": str(GUIDE), "manifest": str(MANIFEST)}, ensure_ascii=False))
```

- [ ] **Step 2: Run the guide renderer**

Run:

```powershell
python Saved/Codex/ui-art-handoff-20260828/render_pilot_guide.py
```

Expected: JSON prints `ok: true`; its count is the runtime texture count plus the single `CODE_DRAWN` explanatory row; the PNG guide and UTF-8-BOM CSV exist.

- [ ] **Step 3: Write the replacement instructions**

Create `README_替换说明.md` with the following content:

```markdown
# GameXXK UI 美术替换说明——桌面工作台默认展开态

## 修改位置

只修改 `02_Assets` 中的 PNG。每个 PNG 已使用项目内 UE 资源名；不要改文件名。

## 必须保持

- 保持画布宽高，除非 `asset_manifest.csv` 明确另行说明。
- 保持 Alpha 通道和透明边缘。
- 不把界面文字烘焙进底图；文字、数值和纯程序色块不在素材文件中。
- 回填前按 `asset_manifest.csv` 的 `ue_asset_path` 确认目标，避免替换同名旧资源。

## 文件用途

- `01_Runtime_Screenshot`：当前实机状态参考。
- `02_Assets`：可修改、可同名回填的 PNG。
- `03_Annotation_Guide`：画面区域与 PNG 文件的对应关系。
- `asset_manifest.csv`：尺寸、Alpha、UE 路径、用途和原始 SHA-256。

本交付包不会自动导入 UE。美术修改完成后的回填与实机复核另行执行。
```

Expected: the instructions do not claim that program-drawn text or geometry is a texture.

### Task 5: Visually verify and correct the guide

**Files:**

- Create: `Saved/HarnessReports/ui-art-handoff-20260828/luna-guide-review.md`
- Modify only if evidence requires it: `Saved/Codex/ui-art-handoff-20260828/render_pilot_guide.py`
- Regenerate only if evidence requires it: `Deliverables/GameXXK_UI_MainFlow_Art_Handoff_20260828/01_DesktopTrainingWorkbench_Default/03_Annotation_Guide/Guide_DesktopTrainingWorkbench_Default.png`

- [ ] **Step 1: Run the required Luna max-effort comparison**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File 'C:\Users\shxuw\.claude\skills\codex-vision\scripts\codex_vision.ps1' -Prompt '对比两张图：第一张是原始实机截图，第二张是素材标注图。请逐项检查：标注框是否落在对应可见控件上；编号与右侧 PNG 文件名是否清楚且没有遮住关键内容；是否漏掉明显的图片或图标；是否把纯文字、纯色或程序绘制区域误当成贴图。输出 PASS 或按严重程度列出必须修正项。不要建议修改游戏代码。' -Images 'Deliverables/GameXXK_UI_MainFlow_Art_Handoff_20260828/01_DesktopTrainingWorkbench_Default/01_Runtime_Screenshot/DesktopTrainingWorkbench_Default_Runtime.png','Deliverables/GameXXK_UI_MainFlow_Art_Handoff_20260828/01_DesktopTrainingWorkbench_Default/03_Annotation_Guide/Guide_DesktopTrainingWorkbench_Default.png' -Effort max -MaxDim 1920 -Out 'Saved/HarnessReports/ui-art-handoff-20260828/luna-guide-review.md' -Workspace 'D:\UE5 demo\GameXXK'
```

Expected: `PASS`, or a bounded list of guide-only corrections.

- [ ] **Step 2: Apply only evidence-backed guide corrections**

If Luna reports misaligned boxes, adjust the screenshot-to-root transform or split the legend in the transient renderer, rerun Task 4 Step 2, and repeat Task 5 Step 1. Do not edit runtime UI source, layout constants, assets, maps, or the original screenshot.

Expected: the final Luna report is `PASS` with no missing visible image/icon category.

### Task 6: Deterministic package verification and archive

**Files:**

- Create: `Deliverables/GameXXK_UI_MainFlow_Art_Handoff_20260828/SHA256SUMS.csv`
- Create: `Deliverables/GameXXK_UI_MainFlow_Art_Handoff_20260828.zip`
- Create: `Saved/Codex/ui-art-handoff-20260828/archive-verification/`

- [ ] **Step 1: Validate PNGs, names, dimensions, alpha, and manifest coverage**

Run:

```powershell
python -c "import csv; from pathlib import Path; from PIL import Image; root=Path(r'Deliverables/GameXXK_UI_MainFlow_Art_Handoff_20260828/01_DesktopTrainingWorkbench_Default'); rows=list(csv.DictReader((root/'asset_manifest.csv').open(encoding='utf-8-sig'))); texture_rows=[r for r in rows if r['asset_filename']!='CODE_DRAWN']; assets=list((root/'02_Assets').rglob('*.png')); assert len(texture_rows)==len(assets)>0; by_file={r['asset_filename']:r for r in texture_rows}; assert len(by_file)==len(texture_rows); [(lambda im,r,p: (im.verify(), (_ for _ in ()).throw(AssertionError(p)) if (str(Image.open(p).width)!=r['width'] or str(Image.open(p).height)!=r['height']) else None))(Image.open(p),by_file[p.name],p) for p in assets]; assert all(p.stem==by_file[p.name]['ue_asset_path'].rsplit('/',1)[-1].split('.',1)[-1] for p in assets); assert any(r['asset_filename']=='CODE_DRAWN' for r in rows); print({'ok':True,'asset_count':len(assets),'code_drawn_rows':1})"
```

Expected: `{'ok': True, 'asset_count': N}` with `N > 0`.

- [ ] **Step 2: Generate package hashes**

Run:

```powershell
$root = (Resolve-Path -LiteralPath 'Deliverables/GameXXK_UI_MainFlow_Art_Handoff_20260828').Path
$rows = Get-ChildItem -LiteralPath $root -Recurse -File | Where-Object { $_.Name -ne 'SHA256SUMS.csv' } | ForEach-Object {
  [pscustomobject]@{
    relative_path = $_.FullName.Substring($root.Length + 1).Replace('\','/')
    sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash.ToLowerInvariant()
    bytes = $_.Length
  }
}
$rows | Sort-Object relative_path | Export-Csv -LiteralPath (Join-Path $root 'SHA256SUMS.csv') -NoTypeInformation -Encoding utf8BOM
```

Expected: every package file except `SHA256SUMS.csv` has one hash row.

- [ ] **Step 3: Create the ZIP without changing the source directory**

Run:

```powershell
Compress-Archive -LiteralPath 'Deliverables/GameXXK_UI_MainFlow_Art_Handoff_20260828' -DestinationPath 'Deliverables/GameXXK_UI_MainFlow_Art_Handoff_20260828.zip' -CompressionLevel Optimal -Force
```

Expected: the ZIP exists and is non-empty.

- [ ] **Step 4: Extract to a new verification directory and compare counts**

Run:

```powershell
$verify = 'Saved/Codex/ui-art-handoff-20260828/archive-verification'
New-Item -ItemType Directory -Path $verify -Force | Out-Null
Expand-Archive -LiteralPath 'Deliverables/GameXXK_UI_MainFlow_Art_Handoff_20260828.zip' -DestinationPath $verify -Force
$sourceCount = (Get-ChildItem -LiteralPath 'Deliverables/GameXXK_UI_MainFlow_Art_Handoff_20260828' -Recurse -File).Count
$archiveCount = (Get-ChildItem -LiteralPath "$verify/GameXXK_UI_MainFlow_Art_Handoff_20260828" -Recurse -File).Count
if ($sourceCount -ne $archiveCount) { throw "Archive count mismatch: source=$sourceCount archive=$archiveCount" }
Write-Output "archive_verified files=$sourceCount"
```

Expected: `archive_verified files=N`.

- [ ] **Step 5: Confirm project files were not modified by the export**

Run:

```powershell
git status --short -- Content Source Config SourceArt
```

Expected: the output is identical to the preflight state for these roots; no new change was introduced by this plan. Existing user changes remain untouched.

Do not commit or stage the exported media. Deliver the folder, ZIP, guide preview, asset count, Luna result, and any explicitly recorded limitation to the user. Do not start any other UI screen until the user accepts this pilot.
