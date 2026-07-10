from __future__ import annotations

import json

import unreal


SHOP = "/Game/GameXXK/Environment/TownPCG/Prototype/QingshanShopA/SM_Qingshan_Shop_A_HQ_Retop50K"


def main() -> dict:
    mesh = unreal.load_asset(SHOP)
    if mesh is None:
        raise RuntimeError(f"missing mesh: {SHOP}")
    bounds = mesh.get_bounds()
    extent = bounds.box_extent
    size_m = [
        float(extent.x) * 0.02,
        float(extent.y) * 0.02,
        float(extent.z) * 0.02,
    ]
    return {
        "ok": all(value > 0 for value in size_m),
        "asset_path": SHOP,
        "bounds_size_m": size_m,
        "material_slot_count": len(mesh.get_editor_property("static_materials")),
    }


if __name__ == "__main__":
    print(json.dumps(main(), ensure_ascii=False, sort_keys=True))
