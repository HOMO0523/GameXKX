from __future__ import annotations

import json

import unreal


def main() -> None:
    library = getattr(unreal, "GameXXKPaperZDAutomationLibrary", None)
    if library is None:
        raise RuntimeError("GameXXKPaperZDAutomationLibrary is not loaded; rebuild GameXXKEditor")

    runner = getattr(library, "ensure_hero_paper_zd_assets", None)
    if runner is None:
        runner = getattr(library, "ensure_hero_paper_zdassets", None)
    if runner is None:
        raise RuntimeError("Could not find EnsureHeroPaperZDAssets Python binding")

    raw_report = runner()
    try:
        parsed = json.loads(str(raw_report))
    except json.JSONDecodeError as exc:
        raise RuntimeError(f"EnsureHeroPaperZDAssets returned non-JSON text: {raw_report}") from exc
    print(json.dumps(parsed, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
