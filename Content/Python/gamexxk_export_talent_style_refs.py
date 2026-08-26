"""Export existing transparent navigation icons as flat-talent style references."""

from __future__ import annotations

import json
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / "Saved/Codex/TalentStyleRefs"
ASSETS = [
    "/Game/GameXXK/UI/ImageTruth/Training/T_TrainingNavTalents.T_TrainingNavTalents",
    "/Game/GameXXK/UI/ImageTruth/Training/T_TrainingNavWarehouse.T_TrainingNavWarehouse",
    "/Game/GameXXK/UI/ImageTruth/Training/T_TrainingNavTools.T_TrainingNavTools",
]


def main() -> None:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    exported: list[str] = []
    for asset_path in ASSETS:
        asset = unreal.EditorAssetLibrary.load_asset(asset_path)
        if not isinstance(asset, unreal.Texture2D):
            raise RuntimeError(f"missing talent style reference: {asset_path}")
        task = unreal.AssetExportTask()
        task.object = asset
        task.filename = str(OUTPUT / f"{asset.get_name()}.png")
        task.automated = True
        task.prompt = False
        task.replace_identical = True
        task.exporter = unreal.TextureExporterPNG()
        if not unreal.Exporter.run_asset_export_task(task):
            raise RuntimeError(f"failed to export talent style reference: {asset_path}")
        exported.append(task.filename)
    print(json.dumps({"ok": True, "exported": exported}, ensure_ascii=False))


if __name__ == "__main__":
    main()
