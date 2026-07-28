"""Validate and expand the immutable three-chapter route balance matrix.

This first layer deliberately performs no simulation and writes no game data.  It
is the shared contract consumed later by the no-render UE commandlet.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any


_NODE_KINDS = ("Battle", "Elite", "Boss")
_EXPECTED_COHORT_IDS = {
    "NakedBaseline",
    "PoJunSong",
    "XuanJiaYueBai",
    "QingNangZhou",
    "ZhuiFengJinGui",
    "ShiGuQiong",
    "ShanHeTusi",
    "MixedMaxRegression",
}
_EXPECTED_ROUTE_LEVELS = {"1": 5, "2": 10, "3": 15}


def load_matrix(path: Path) -> dict[str, Any]:
    """Load one immutable version-1 balance matrix and reject a wrong contract."""
    try:
        matrix = json.loads(path.read_text(encoding="utf-8"))
    except OSError as exc:
        raise ValueError(f"cannot read matrix {path}: {exc}") from exc
    except json.JSONDecodeError as exc:
        raise ValueError(f"matrix is not valid JSON: {exc}") from exc

    if matrix.get("schema_version") != 1:
        raise ValueError("matrix schema_version must be 1")
    full = matrix.get("profiles", {}).get("Full")
    if not isinstance(full, dict) or full.get("seed_count") != 100:
        raise ValueError("Full profile must contain exactly 100 seeds")
    if tuple(full.get("node_kinds", ())) != _NODE_KINDS:
        raise ValueError("Full profile must contain Battle, Elite, Boss in that order")
    if matrix.get("route_levels") != _EXPECTED_ROUTE_LEVELS:
        raise ValueError("route levels must be chapter 1/2/3 = 5/10/15")
    cohorts = matrix.get("cohorts")
    if not isinstance(cohorts, list) or {item.get("id") for item in cohorts} != _EXPECTED_COHORT_IDS:
        raise ValueError("matrix must contain the eight locked balance cohorts")
    if len(cohorts) != len(_EXPECTED_COHORT_IDS):
        raise ValueError("matrix cohort identifiers must be unique")
    if matrix.get("gates") != {
        "Battle": [0.55, 0.7],
        "Elite": [0.35, 0.5],
        "Boss": [0.15, 0.35],
    }:
        raise ValueError("matrix gate ranges do not match the locked balance targets")
    return matrix


def expand_full_cases(matrix: dict[str, Any]) -> list[dict[str, Any]]:
    """Expand the Full profile into its exact, deterministic 2,400 combat cases."""
    full = matrix["profiles"]["Full"]
    route_levels = matrix["route_levels"]
    cases: list[dict[str, Any]] = []
    for cohort_index, cohort in enumerate(matrix["cohorts"]):
        for node_index, node_kind in enumerate(full["node_kinds"]):
            for seed_ordinal in range(full["seed_count"]):
                chapter = 1 + ((seed_ordinal + cohort_index) % 3)
                cases.append(
                    {
                        "cohort_id": cohort["id"],
                        "quest_npc": cohort["quest_npc"],
                        "set": cohort["set"],
                        "quality": cohort["quality"],
                        "enhancement": cohort["enhancement"],
                        "node_kind": node_kind,
                        "chapter": chapter,
                        "route_level": route_levels[str(chapter)],
                        "seed_ordinal": seed_ordinal,
                        "seed": 900000 + cohort_index * 10000 + node_index * 1000 + seed_ordinal,
                    }
                )
    return cases


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--matrix", type=Path, required=True)
    args = parser.parse_args()
    matrix = load_matrix(args.matrix)
    cases = expand_full_cases(matrix)
    print(json.dumps({"ok": True, "profile": "Full", "case_count": len(cases)}, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
