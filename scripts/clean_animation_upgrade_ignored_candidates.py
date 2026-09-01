#!/usr/bin/env python3
"""Remove only project-generated artifacts for explicitly ignored animation sources."""

from __future__ import annotations

import json
import shutil
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
STAGING_ROOT = PROJECT_ROOT / "SourceAssets/AnimationProduction/upgrade_20260827_corrected"
REVIEW_ROOT = PROJECT_ROOT / "Saved/HarnessReports/animation-upgrade-20260827-corrected"
IGNORED_CANDIDATE_IDS = (
    "enemy_18_deer_bow_candidate",
)


def inside(root: Path, path: Path) -> bool:
    try:
        path.resolve().relative_to(root.resolve())
        return True
    except ValueError:
        return False


def main() -> int:
    removed: list[str] = []
    for candidate_id in IGNORED_CANDIDATE_IDS:
        for root in (STAGING_ROOT, REVIEW_ROOT):
            target = root / candidate_id
            if not target.exists():
                continue
            if not inside(root, target):
                raise RuntimeError(f"refusing out-of-root cleanup: {target}")
            shutil.rmtree(target)
            removed.append(str(target.relative_to(PROJECT_ROOT)).replace("\\", "/"))
    remaining = [
        str((root / candidate_id).relative_to(PROJECT_ROOT)).replace("\\", "/")
        for candidate_id in IGNORED_CANDIDATE_IDS
        for root in (STAGING_ROOT, REVIEW_ROOT)
        if (root / candidate_id).exists()
    ]
    report = {"ok": not remaining, "removed": removed, "remaining": remaining}
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 0 if report["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
