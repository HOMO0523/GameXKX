"""Merge reviewed JSON text sources only; never modifies C++ or generated inl files."""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    path = ROOT / "Content/Localization/GameXXK/strings.json"
    data = json.loads(path.read_text(encoding="utf-8"))
    entries = {(row.get("namespace", "GameXXK"), row["key"]): row for row in data["entries"]}
    common = json.loads((ROOT / "docs/design/2026-09-10-relic-redesign/common-relic-translations.json").read_text(encoding="utf-8"))
    for relic in common["relics"]:
        for field, suffix in (("name", "Name"), ("description", "Description")):
            row = {"key": relic["id"] + "." + suffix, **relic[field]}
            entries[("GameXXK", row["key"])] = row
    for source in sorted((ROOT / "docs/design/2026-09-10-ui-localization").glob("*-translations.json")):
        for row in json.loads(source.read_text(encoding="utf-8"))["entries"]:
            entries[(row.get("namespace", "GameXXK"), row["key"])] = row
    # Card/relic quality wording; equipment's ten-rank vocabulary uses separate contextual keys.
    for key, zh, en in (("Quality.Common", "普通", "Common"), ("Quality.Rare", "稀有", "Rare"), ("Quality.Epic", "史诗", "Epic")):
        entries[("GameXXK", key)] = {"key": key, "zh-Hans": zh, "en": en}
    entries.pop(("GameXXK", "Quality.Uncommon"), None)
    data["entries"] = list(entries.values())
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Merged {len(entries)} bilingual text identities. Runtime UI coverage is a separate check.")


if __name__ == "__main__":
    main()
