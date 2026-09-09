"""Inventory Chinese source literals for review; this is not a translation-coverage claim.

Only the runtime module is scanned. Rule fragments, persisted identity strings and
diagnostics remain review items; they must never be globally replaced with English.
"""

import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
LITERAL = re.compile(r'TEXT\s*\(\s*"((?:\\.|[^"\\])*)"\s*\)')
CJK = re.compile(r"[\u3400-\u9fff]")


def inventory():
    paths = subprocess.check_output(
        ["rg", "--files", "Source/GameXXK", "-g", "*.cpp", "-g", "*.h", "-g", "!Private/Tests/**"],
        cwd=ROOT, text=True, encoding="utf-8"
    ).splitlines()
    catalog_path = ROOT / "Content/Localization/GameXXK/strings.json"
    catalog = json.loads(catalog_path.read_text(encoding="utf-8-sig"))
    translated = {e["zh-Hans"] for e in catalog["entries"]}
    files = []
    counts = {"literalOccurrences": 0, "uniqueLiterals": 0, "exactCatalogSources": 0}
    unique = set()
    for name in sorted(paths):
        if "/Tests/" in name.replace("\\", "/"):
            continue
        path = ROOT / name
        raw = path.read_bytes()
        text = raw.decode("utf-8-sig")
        matches = []
        for match in LITERAL.finditer(text):
            source = match[1]
            if not CJK.search(source):
                continue
            prefix = text[max(0, match.start() - 150):match.start()]
            classification = "needs-context-review"
            if re.search(r"(?:Contains|StartsWith|EndsWith|Replace|RemoveFromStart)\(\s*$", prefix):
                classification = "semantic-parser-do-not-translate-before-parsing"
            elif "UE_LOG" in prefix.split("\n")[-1]:
                classification = "diagnostic"
            matches.append({"line": text.count("\n", 0, match.start()) + 1,
                            "source": source, "classification": classification,
                            "hasExactCatalogueEntry": source in translated})
            unique.add(source)
        if matches:
            files.append({"path": name.replace("\\", "/"), "sha256": hashlib.sha256(raw).hexdigest(), "literals": matches})
            counts["literalOccurrences"] += len(matches)
    counts["uniqueLiterals"] = len(unique)
    counts["exactCatalogSources"] = len(unique & translated)
    return {"schemaVersion": 1, "scope": "Source/GameXXK runtime C++/headers only",
            "note": "Occurrences include semantic fragments and diagnostics. Exact catalogue matches do not prove runtime integration.",
            "counts": counts, "files": files}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    data = inventory()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"output": str(args.output.resolve()), **data["counts"]}, ensure_ascii=False))


if __name__ == "__main__":
    main()
