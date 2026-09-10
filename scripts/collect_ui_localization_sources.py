"""Collect runtime text identities for translation review without altering rule code."""
import hashlib
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STRING = r'"((?:\\.|[^"\\])*)"'
NS = re.compile(r'NSLOCTEXT\s*\(\s*' + STRING + r'\s*,\s*' + STRING + r'\s*,\s*' + STRING + r'\s*\)')
TEXT = re.compile(r'TEXT\s*\(\s*' + STRING + r'\s*\)')
CJK = re.compile(r"[\u3400-\u9fff]")
PRINTF = re.compile(r"%(?!%)(?:[-+ #0]*)(?:\d+|\*)?(?:\.(?:\d+|\*))?(?:I64|ll|l|h|z)?[diuoxXfFeEgGsc]")


def decode(source):
    return re.sub(r'\\([nrt"\\])', lambda m: {"n": "\n", "r": "\r", "t": "\t", '"': '"', "\\": "\\"}[m[1]], source)


def main():
    paths = subprocess.check_output(["rg", "--files", "Source/GameXXK", "-g", "*.cpp", "-g", "*.h"], cwd=ROOT, text=True, encoding="utf-8").splitlines()
    existing = json.loads((ROOT / "Content/Localization/GameXXK/strings.json").read_text(encoding="utf-8"))["entries"]
    identities = {(row.get("namespace", "GameXXK"), row["key"]) for row in existing}
    exact_sources = {row["zh-Hans"] for row in existing}
    rows = []
    seen = set()
    for relative in sorted(paths):
        if "/Tests/" in relative.replace("\\", "/"): continue
        source = (ROOT / relative).read_text(encoding="utf-8-sig")
        for match in NS.finditer(source):
            namespace, key, raw = match.groups()
            text = decode(raw)
            if not CJK.search(text): continue
            identity = (namespace, key, text)
            if identity in seen: continue
            seen.add(identity)
            rows.append({"kind": "NSLOCTEXT", "namespace": namespace, "key": key, "source": text,
                         "path": relative.replace("\\", "/"), "line": source.count("\n", 0, match.start()) + 1,
                         "translated": (namespace, key) in identities})
        for match in TEXT.finditer(source):
            text = decode(match[1])
            if not CJK.search(text) or ("TEXT", text) in seen: continue
            seen.add(("TEXT", text))
            prefix = source[max(0, match.start() - 170):match.start()]
            kind = "literal-review"
            if re.search(r"(?:StartsWith|Contains|Replace|EndsWith|RemoveFromStart)\(\s*$", prefix): kind = "semantic-parser"
            elif "UE_LOG" in prefix.split("\n")[-1]: kind = "diagnostic"
            elif PRINTF.search(text): kind = "printf-template"
            rows.append({"kind": kind, "key": "Legacy." + hashlib.sha1(text.encode("utf-8")).hexdigest()[:16],
                         "source": text, "path": relative.replace("\\", "/"), "line": source.count("\n", 0, match.start()) + 1,
                         "printfArguments": PRINTF.findall(text), "translated": text in exact_sources})
    out = ROOT / "docs/design/2026-09-10-ui-localization/collected-source.json"
    out.write_text(json.dumps({"schemaVersion": 1, "note": "Review inventory; does not claim player-text completeness or integration.", "entries": rows}, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    counts = {kind: sum(row["kind"] == kind for row in rows) for kind in sorted({row["kind"] for row in rows})}
    print(json.dumps({"count": len(rows), "kinds": counts, "path": str(out)}, ensure_ascii=False))


if __name__ == "__main__":
    main()
