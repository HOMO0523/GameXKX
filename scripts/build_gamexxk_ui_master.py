"""Build the deterministic GameXXK UI Master Phase A review package."""

from __future__ import annotations

import argparse
import copy
import json
from pathlib import Path
import shutil
import sys

from PIL import Image, ImageDraw

PROJECT_ROOT = Path(__file__).resolve().parents[1]
if str(PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT))

from scripts.gamexxk_ui_master_contract import load_contract, validate_source_lock
from scripts.gamexxk_ui_master_pages import PACKAGE, ROOT, _font, build_page_previews


def _write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(value, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )


def _build_runtime_assets(
    package_root: Path, component_records: dict[str, dict]
) -> dict:
    assets_root = package_root / "Assets"
    runtime_root = package_root / "RuntimeAssets"
    runtime_root.mkdir(parents=True, exist_ok=True)
    runtime_records: dict[str, dict] = {}
    for key, source_record in sorted(component_records.items()):
        source = assets_root / source_record["file"]
        destination = runtime_root / source_record["file"]
        shutil.copy2(source, destination)
        runtime_records[key] = {
            "file": destination.relative_to(package_root).as_posix(),
            "source": source.relative_to(package_root).as_posix(),
            "size": source_record["size"],
            "textBaked": False,
            "status": "draft_not_imported",
        }
    manifest = {
        "version": 1,
        "phase": "A",
        "textBaked": False,
        "assets": runtime_records,
    }
    _write_json(package_root / "runtime-assets-manifest.json", manifest)
    return manifest


def _build_contact_sheet(package_root: Path, pages: list[dict]) -> Path:
    sheet = Image.new("RGB", (2400, 1080), (38, 37, 34))
    draw = ImageDraw.Draw(sheet)
    for index, record in enumerate(pages):
        column = index % 5
        row = index // 5
        x = column * 480
        y = row * 270
        with Image.open(package_root / "Previews" / record["file"]) as opened:
            preview = opened.convert("RGB").resize((480, 270), Image.Resampling.LANCZOS)
        sheet.paste(preview, (x, y))
        draw.rectangle((x, y + 232, x + 480, y + 270), fill=(27, 27, 25))
        draw.text(
            (x + 12, y + 237),
            record["group"],
            font=_font(18, bold=True),
            fill=(232, 215, 179),
        )
        draw.rectangle((x, y, x + 479, y + 269), outline=(135, 123, 101), width=2)
    path = package_root / "GameXXK_UI_Master_ContactSheet.png"
    sheet.save(path)
    return path


def _build_master_manifest(
    package_root: Path,
    pages: list[dict],
    contract,
) -> dict:
    image_layers: list[dict] = []
    text_layers: list[dict] = []
    page_summaries: list[dict] = []
    for index, page in enumerate(pages):
        origin_x, origin_y = contract.page_origin(index)
        for source_layer in page["imageLayers"]:
            layer = copy.deepcopy(source_layer)
            layer["x"] = int(layer.get("x", 0)) + origin_x
            layer["y"] = int(layer.get("y", 0)) + origin_y
            image_layers.append(layer)
        for source_layer in page["textLayers"]:
            layer = copy.deepcopy(source_layer)
            layer["x"] = int(layer.get("x", 0)) + origin_x
            layer["y"] = int(layer.get("y", 0)) + origin_y
            text_layers.append(layer)
        page_summaries.append(
            {
                "index": index,
                "group": page["group"],
                "origin": [origin_x, origin_y],
                "size": page["size"],
                "preview": f"Previews/{page['file']}",
                "status": page["status"],
                "sourceFamily": page.get("sourceFamily", "phase_a_draft"),
                "imageLayerCount": len(page["imageLayers"]),
                "textLayerCount": len(page["textLayers"]),
            }
        )
    manifest = {
        "document": {
            "name": "GameXXK_UI_Master_V1",
            "width": contract.master_size[0],
            "height": contract.master_size[1],
            "scale": 1,
            "resolution": 72,
            "outputPsd": contract.output_psd.as_posix(),
            "overviewScale": contract.overview_scale,
        },
        "imageLayers": image_layers,
        "textLayers": text_layers,
        "phase": contract.phase,
        "pages": page_summaries,
        "sourceLock": "source-lock.json",
        "componentVariants": "component-variants.json",
        "contactSheet": "GameXXK_UI_Master_ContactSheet.png",
        "runtimeAssetsManifest": "runtime-assets-manifest.json",
    }
    _write_json(package_root / "master-manifest.json", manifest)
    return manifest


def build_package(output_root: Path) -> dict:
    output_root = output_root.resolve()
    output_root.mkdir(parents=True, exist_ok=True)
    contract = load_contract(PACKAGE / "ui-master-spec.json")
    source_report = validate_source_lock(PACKAGE / "source-lock.json", ROOT)
    if not source_report["ok"]:
        raise RuntimeError(f"source lock validation failed: {source_report['errors']}")
    for contract_name in (
        "ui-master-spec.json",
        "source-lock.json",
        "component-variants.json",
    ):
        source = PACKAGE / contract_name
        destination = output_root / contract_name
        if source.resolve() != destination.resolve():
            shutil.copy2(source, destination)

    previews_root = output_root / "Previews"
    assets_root = output_root / "Assets"
    pages = build_page_previews(
        previews_root,
        asset_root=assets_root,
        package_root=output_root,
    )
    component_records = json.loads(
        (assets_root / "component-assets.json").read_text(encoding="utf-8")
    )
    runtime_manifest = _build_runtime_assets(output_root, component_records)
    _build_contact_sheet(output_root, pages)
    manifest = _build_master_manifest(output_root, pages, contract)
    return {
        "ok": True,
        "phase": contract.phase,
        "outputRoot": output_root.as_posix(),
        "masterCanvas": list(contract.master_size),
        "pageGroups": len(pages),
        "imageLayers": len(manifest["imageLayers"]),
        "textLayers": len(manifest["textLayers"]),
        "runtimeAssets": len(runtime_manifest["assets"]),
        "v2MasterPages": [page["group"] for page in pages if page.get("status") == "v2_master"],
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--output-root",
        type=Path,
        default=PACKAGE,
    )
    args = parser.parse_args()
    try:
        report = build_package(args.output_root)
    except Exception as exc:  # pragma: no cover - CLI failure path
        print(json.dumps({"ok": False, "error": str(exc)}, ensure_ascii=False))
        return 1
    print(json.dumps(report, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
