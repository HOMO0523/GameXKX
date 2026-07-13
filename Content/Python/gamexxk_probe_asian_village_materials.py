"""Read-only inventory of Asian Village material parentage for occlusion-cutout design."""

from __future__ import annotations

import collections
import json
import unreal


ROOT = "/Game/Asian_Village"


def main():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    assets = registry.get_assets_by_path(ROOT, recursive=True)
    material_assets = [asset for asset in assets if str(asset.asset_class_path.asset_name) in {"Material", "MaterialInstanceConstant"}]
    parent_counts = collections.Counter()
    class_counts = collections.Counter()
    blend_counts = collections.Counter()
    samples = []
    for asset_data in material_assets:
        asset = asset_data.get_asset()
        class_name = asset.get_class().get_name()
        class_counts[class_name] += 1
        parent_path = ""
        if isinstance(asset, unreal.MaterialInstanceConstant):
            parent = asset.get_editor_property("parent")
            parent_path = "" if parent is None else str(parent.get_path_name())
            parent_counts[parent_path] += 1
        try:
            blend_counts[str(asset.get_blend_mode())] += 1
        except Exception:
            pass
        if len(samples) < 25:
            samples.append({"asset": str(asset.get_path_name()), "class": class_name, "parent": parent_path})
    report = {
        "root": ROOT,
        "material_asset_count": len(material_assets),
        "class_counts": dict(class_counts),
        "blend_counts": dict(blend_counts),
        "top_instance_parents": parent_counts.most_common(20),
        "samples": samples,
    }
    print(json.dumps(report, ensure_ascii=False))
    return report


if __name__ == "__main__":
    main()
