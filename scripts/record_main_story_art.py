"""Copy a generated node illustration into the project and record exact provenance."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
from PIL import Image
from main_story_authoring import ROOT


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("node_id")
    parser.add_argument("source")
    parser.add_argument("--expected-contract-sha", default="")
    args = parser.parse_args()
    if not re.fullmatch(r"S0[0-5]-\d\d", args.node_id):
        raise SystemExit("Invalid node ID")
    campaign = json.loads((ROOT / "SourceAssets/Narrative/MainStory/campaign.json").read_text(encoding="utf-8"))
    node = next(n for c in campaign["chapters"] for n in c["nodes"] if n["id"] == args.node_id)
    contract = json.dumps({"art": node["art"], "result": node["result"]}, ensure_ascii=False, sort_keys=True)
    contract_sha = hashlib.sha256(contract.encode("utf-8")).hexdigest()
    source = Path(args.source).resolve(strict=True)
    digest = hashlib.sha256(source.read_bytes()).hexdigest()
    with Image.open(source) as image:
        size, mode = image.size, image.mode
        if size[0] < 1500 or not 2.8 <= size[0] / size[1] <= 3.2 or mode not in ("RGB", "RGBA"):
            raise SystemExit(f"Unexpected illustration format: {size}, {mode}")
    folder = (ROOT / "SourceArt/UI/StoryNodes").resolve()
    folder.mkdir(parents=True, exist_ok=True)
    stem = args.node_id.lower().replace("-", "_")
    destination = folder / (stem + "_v1.png")
    revision = 1
    while destination.exists() and hashlib.sha256(destination.read_bytes()).hexdigest() != digest:
        revision += 1
        destination = folder / f"{stem}_v{revision}.png"
    if not destination.resolve().is_relative_to(folder):
        raise SystemExit("Destination escaped story-art folder")
    if not destination.exists():
        shutil.copy2(source, destination)
    manifest_file = folder / "manifest.json"
    manifest = json.loads(manifest_file.read_text(encoding="utf-8")) if manifest_file.is_file() else {"schema_version": 1, "expected_count": 61, "style_reference": "SourceArt/UI/RouteCamp/campfire_rest_banner_v3.png", "nodes": {}}
    manifest["nodes"][args.node_id] = {
        "file": destination.relative_to(ROOT).as_posix(), "generated_source": str(source),
        "sha256": digest, "size": list(size), "mode": mode, "revision": revision,
        "texture": node["art"]["texture"], "cast": node["art"]["cast"],
        "result": node["result"], "display_phase": node['art']['display_phase'], "style_version": node['art'].get('style_version', 'grain_v1'),
        "contract_sha256": args.expected_contract_sha or contract_sha,
        "visual_review": "stale_contract" if args.expected_contract_sha and args.expected_contract_sha != contract_sha else "pending",
    }
    manifest_file.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    response = dict(manifest["nodes"][args.node_id])
    response["pause_requested"] = (ROOT / "Saved/StorySystem/art.pause").is_file()
    print(json.dumps(response, ensure_ascii=False))


if __name__ == "__main__":
    main()
