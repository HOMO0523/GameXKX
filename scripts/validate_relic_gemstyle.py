"""Deterministic asset checks after the user-authorized background/size processing."""
import hashlib
import argparse
import json
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / "SourceArt/UI/Relics/gemstyle-20260910"


def main():
    global ART
    parser = argparse.ArgumentParser()
    parser.add_argument("--group", choices=("relic", "hunt"), default="relic")
    args = parser.parse_args()
    if args.group == "hunt": ART = ROOT / "SourceArt/UI/Hunt/gemstyle-20260910"
    manifest = json.loads((ART / "manifest.json").read_text(encoding="utf-8"))
    jobs = json.loads((ART / "art-jobs.json").read_text(encoding="utf-8"))["jobs"]
    expected = {job["slug"] for job in jobs}
    records = {record["slug"]: record for record in manifest["icons"]}
    issues = []
    count = 45 if args.group == "relic" else 5
    if len(records) != count or set(records) != expected: issues.append(f"{count} unique {args.group} icons are required")
    for slug, record in records.items():
        path = ROOT / record["icon"]
        image = Image.open(path)
        if image.mode != "RGBA" or image.size != (512, 512): issues.append(slug + ": expected 512 RGBA")
        alpha = image.getchannel("A")
        bounds = alpha.getbbox()
        if not bounds or max(bounds[2] - bounds[0], bounds[3] - bounds[1]) not in range(447, 457): issues.append(slug + ": invalid content fill")
        edges = [alpha.crop((0, 0, 512, 1)), alpha.crop((0, 511, 512, 512)), alpha.crop((0, 0, 1, 512)), alpha.crop((511, 0, 512, 512))]
        if any(edge.getextrema()[1] for edge in edges): issues.append(slug + ": opaque image edge")
        if hashlib.sha256(path.read_bytes()).hexdigest() != record["sha256"]: issues.append(slug + ": output hash drift")
        raw = ROOT / record["raw"]
        if hashlib.sha256(raw.read_bytes()).hexdigest() != record["rawSha256"]: issues.append(slug + ": raw hash drift")
    report = {"count": len(records), "expected": count, "passed": not issues, "issues": issues, "visualReviewRequired": True}
    (ART / "validation.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, ensure_ascii=False))
    if issues: raise SystemExit(1)


if __name__ == "__main__": main()
