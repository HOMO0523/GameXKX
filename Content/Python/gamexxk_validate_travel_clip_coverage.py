"""Read-only asset-load audit for all compact-strip enemy Idle/Attack pairs.

The C++ automation covers resolver behavior and cache re-entry. This companion
probe loads the actual project textures, including intentional 2K fallbacks.
"""
import argparse
import json
import re
from pathlib import Path

import unreal
import gamexxk_validate_battle_animation_texture_memory as texture_info


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--preload", action="store_true", help="Warm textures, then run again after the editor has finished texture compilation")
    args = parser.parse_args()
    root = Path(unreal.Paths.project_dir()).resolve()
    source = (root / "Source/GameXXK/Private/UI/GameXXKBattleAnimationPresentation.cpp").read_text(encoding="utf-8-sig")
    pairs = re.findall(r'\{TEXT\("([^\"]+)"\), TEXT\("(enemy_[^\"]+)"\)\}', source)
    records = []
    for enemy, base in pairs:
        for action in ("idle", "attack"):
            # GrayWolf's retired attack is intentionally the live Idle + lunge.
            clip = "idle" if base == "enemy_07_graywolf" else action
            candidates = [f"/Game/GameXXK/BattleAnimations/Atlases/T_{base}_{size}_{clip}_atlas" for size in ("1k", "2k")]
            selected = next((path for path in candidates if unreal.EditorAssetLibrary.does_asset_exist(path)), None)
            texture = unreal.load_asset(selected) if selected else None
            dimensions = list(texture_info._texture_size(texture)) if texture else [0, 0]
            expected = 1024 if selected == candidates[0] else 2048
            records.append({"enemy": enemy, "action": action, "display_clip": clip,
                            "preferred_available": selected == candidates[0],
                            "resource": texture.get_path_name() if texture else None,
                            "dimensions": dimensions,
                            "ok": isinstance(texture, unreal.Texture2D) and (args.preload or dimensions == [expected, expected])})
    report = {"ok": len(pairs) == 21 and all(row["ok"] for row in records),
              "enemy_count": len(pairs), "clip_count": len(records),
              "fallback_count": sum(not row["preferred_available"] for row in records),
              "preload_only": args.preload, "records": records}
    filename = "travel-clip-preload-20260907.json" if args.preload else "travel-clip-coverage-20260907.json"
    output = root / "Saved/Diagnostics" / filename
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps({key: value for key, value in report.items() if key != "records"}, ensure_ascii=False))
    if not report["ok"]:
        raise RuntimeError(f"Travel clip coverage failed; see {output}")


if __name__ == "__main__":
    main()
