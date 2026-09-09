import json
from pathlib import Path

import unreal


def main():
    out_dir = Path(r"D:\UE5 demo\GameXXK\Saved\Codex")
    out_dir.mkdir(parents=True, exist_ok=True)
    pairs = [
        ("/Game/GameXXK/UI/Town/Textures/Backpack/T_TownBackpack_WindowFrame.T_TownBackpack_WindowFrame",
         "tex_old_town_window_frame.png"),
        ("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_PanelLarge.T_MasterV2_PanelLarge",
         "tex_masterv2_panel_large.png"),
        ("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_ItemSlot.T_MasterV2_ItemSlot",
         "tex_masterv2_item_slot.png"),
    ]
    results = {}
    for asset_path, out_name in pairs:
        tex = unreal.load_asset(asset_path)
        if tex is None:
            results[out_name] = {"error": "asset not found"}
            continue
        out_path = str(out_dir / out_name)
        try:
            ok = unreal.AssetToolsHelpers.get_asset_tools().export_assets([asset_path], out_dir.as_posix())
            results[out_name] = {"export_ok": bool(ok), "expected_file": out_path,
                                 "exists_after": (out_dir / out_name).exists()}
        except Exception as exc:  # noqa: BLE001
            results[out_name] = {"error": str(exc)}
    print(json.dumps(results, ensure_ascii=False, default=str))


if __name__ == "__main__":
    main()
