#!/usr/bin/env python3
"""Generate bounded, read-only balance proposals from two deterministic observations."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import statistics
import sys
from collections import defaultdict
from pathlib import Path
from typing import Iterable

PROJECT_ROOT = Path(__file__).resolve().parents[1]
if str(PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT))

from scripts.run_card_balance_observation import read_case_rows


TARGET_INTERVALS = {
    "Early": {
        "Battle": (0.70, 0.85),
        "Elite": (0.35, 0.55),
        "Boss": (0.15, 0.35),
    },
    "Mid": {
        "Battle": (0.80, 0.92),
        "Elite": (0.55, 0.70),
        "Boss": (0.35, 0.55),
    },
    "Late": {
        "Battle": (0.95, 1.00),
        "Elite": (0.80, 0.95),
        "Boss": (0.60, 0.80),
    },
}


def wilson_interval(successes: int, total: int, z: float = 1.96) -> tuple[float, float]:
    if total <= 0 or successes < 0 or successes > total:
        raise ValueError("Wilson interval requires 0 <= successes <= total and total > 0")
    proportion = successes / total
    denominator = 1 + (z * z / total)
    centre = proportion + (z * z / (2 * total))
    margin = z * math.sqrt(
        (proportion * (1 - proportion) / total) + (z * z / (4 * total * total))
    )
    return (
        max(0.0, (centre - margin) / denominator),
        min(1.0, (centre + margin) / denominator),
    )


def get_target_interval(growth_tier: str, node: str) -> tuple[float, float]:
    try:
        return TARGET_INTERVALS[growth_tier][node]
    except KeyError as error:
        raise ValueError(f"unknown growth target: {growth_tier}/{node}") from error


def bounded_change(kind: str, current_value: int, direction: int) -> int:
    sign = 1 if direction > 0 else -1 if direction < 0 else 0
    if sign == 0:
        return 0
    if kind == "attack_multiplier_pp":
        magnitude = 15
    elif kind == "fixed_value":
        magnitude = min(2, max(1, math.floor(abs(current_value) * 0.15)))
    elif kind == "energy":
        magnitude = 1
    elif kind == "mana":
        magnitude = 3
    elif kind == "enemy_percent":
        magnitude = 10
    else:
        raise ValueError(f"unknown bounded change kind: {kind}")
    return sign * magnitude


def validate_observation_pair(
    first: dict[str, object],
    second: dict[str, object],
) -> None:
    first_summary = first.get("summary")
    second_summary = second.get("summary")
    first_count = first_summary.get("case_count") if isinstance(first_summary, dict) else None
    second_count = second_summary.get("case_count") if isinstance(second_summary, dict) else None
    checks = (
        first.get("schema_version") == second.get("schema_version") == 3,
        first.get("matrix") == second.get("matrix"),
        first.get("matrix") in {"locked", "orthogonal"},
        first_count == second_count and isinstance(first_count, int) and first_count > 0,
        bool(first.get("csv_sha256")),
        first.get("csv_sha256") == second.get("csv_sha256"),
    )
    if not all(checks):
        raise ValueError("observation pair must share schema, matrix, case count, and CSV SHA")


def classify_card_usage(
    card_usage: dict[str, dict[str, object]],
) -> dict[str, list[str]]:
    classified: dict[str, list[str]] = {
        "unseen": [],
        "never_played": [],
        "low_play_low_direct_contribution": [],
        "low_play_high_direct_contribution": [],
        "high_direct_contribution": [],
    }
    for card_id, metrics in sorted(card_usage.items()):
        seen = int(metrics.get("seen", 0))
        played = int(metrics.get("played", 0))
        contribution = sum(
            int(metrics.get(field, 0)) for field in ("damage", "healing", "armor")
        )
        if seen == 0:
            classified["unseen"].append(card_id)
            continue
        if played == 0:
            classified["never_played"].append(card_id)
            continue
        play_rate = played / seen
        contribution_per_play = contribution / played
        if play_rate < 0.15:
            bucket = (
                "low_play_high_direct_contribution"
                if contribution_per_play >= 50
                else "low_play_low_direct_contribution"
            )
            classified[bucket].append(card_id)
        if contribution_per_play >= 50 and play_rate >= 0.15:
            classified["high_direct_contribution"].append(card_id)
    return classified


def _group_rows(
    rows: Iterable[dict[str, object]],
) -> dict[tuple[str, str, str], list[dict[str, object]]]:
    groups: dict[tuple[str, str, str], list[dict[str, object]]] = defaultdict(list)
    for row in rows:
        dimension = str(row.get("dimension") or "")
        variant = str(row.get("variant") or "")
        node = str(row.get("node") or "")
        if not dimension or not variant or node not in {"Battle", "Elite", "Boss"}:
            raise ValueError("proposal rows require orthogonal dimension, variant, and combat node")
        groups[(dimension, variant, node)].append(row)
    return groups


def _slice_summary(rows: list[dict[str, object]]) -> dict[str, object]:
    victories = sum(row.get("outcome") == "Victory" for row in rows)
    lower, upper = wilson_interval(victories, len(rows))
    rounds = [int(row["rounds"]) for row in rows]
    remaining = [int(row["remaining_party_health"]) for row in rows]
    defeats = sorted(int(row["seed"]) for row in rows if row.get("outcome") == "Defeat")
    wins = sorted(int(row["seed"]) for row in rows if row.get("outcome") == "Victory")
    return {
        "cases": len(rows),
        "victories": victories,
        "win_rate": round(victories / len(rows), 6),
        "wilson_95": [round(lower, 6), round(upper, 6)],
        "median_rounds": statistics.median(rounds),
        "mean_remaining_party_health": round(statistics.mean(remaining), 3),
        "extreme_defeat_seeds": defeats[:5],
        "extreme_victory_seeds": wins[:5],
    }


def _candidate_kind(dimension: str) -> str:
    return {
        "Profession": "card_budget",
        "EquipmentSet": "set_budget",
        "QuestNpc": "npc_card_budget",
        "Terrain": "terrain_benefit",
        "Progression": "enemy_profile_last_resort",
    }.get(dimension, "diagnostic_only")


def _allowed_change(dimension: str, direction: str) -> dict[str, int]:
    player_direction = 1 if direction == "under_target" else -1
    if dimension == "Progression":
        enemy_direction = -player_direction
        return {
            "enemy_hp_attack_defense_percent": bounded_change(
                "enemy_percent", 100, enemy_direction
            )
        }
    return {
        "attack_multiplier_pp": bounded_change(
            "attack_multiplier_pp", 100, player_direction
        ),
        "fixed_value": bounded_change("fixed_value", 20, player_direction),
        "energy": bounded_change("energy", 1, -player_direction),
        "mana": bounded_change("mana", 12, -player_direction),
    }


def build_proposal(
    first: dict[str, object],
    second: dict[str, object],
    rows: list[dict[str, object]],
) -> dict[str, object]:
    validate_observation_pair(first, second)
    if str(first.get("matrix")) != "orthogonal":
        raise ValueError("automatic proposals require the orthogonal matrix")
    groups = _group_rows(rows)
    slices = {
        f"{dimension}|{variant}|{node}": _slice_summary(group_rows)
        for (dimension, variant, node), group_rows in sorted(groups.items())
    }

    candidates: list[dict[str, object]] = []
    for (dimension, variant, node), group_rows in sorted(groups.items()):
        evidence = _slice_summary(group_rows)
        tier = variant if dimension == "Progression" else "Mid"
        target_low, target_high = get_target_interval(tier, node)
        rate = float(evidence["win_rate"])
        if target_low <= rate <= target_high:
            continue
        direction = "under_target" if rate < target_low else "over_target"
        distance = target_low - rate if direction == "under_target" else rate - target_high
        seeds = (
            evidence["extreme_defeat_seeds"]
            if direction == "under_target"
            else evidence["extreme_victory_seeds"]
        )
        candidates.append(
            {
                "candidate_id": f"{dimension}.{variant}.{node}",
                "priority_score": round(distance, 6),
                "dimension": dimension,
                "variant": variant,
                "node": node,
                "direction": direction,
                "target_interval": [target_low, target_high],
                "evidence": evidence,
                "recommended_cluster": _candidate_kind(dimension),
                "bounded_change": _allowed_change(dimension, direction),
                "required_ab_test": {
                    "seed": seeds[0] if seeds else None,
                    "rule": "same seed and fixture; change exactly one proposed parameter",
                },
                "forbidden_parallel_changes": sorted(
                    {
                        "card_budget",
                        "set_budget",
                        "npc_card_budget",
                        "terrain_benefit",
                        "enemy_profile",
                    }
                    - {_candidate_kind(dimension)}
                ),
            }
        )
    candidates.sort(
        key=lambda item: (-float(item["priority_score"]), str(item["candidate_id"]))
    )

    first_summary = first.get("summary")
    if not isinstance(first_summary, dict):
        raise ValueError("observation pair has no aggregate summary")
    return {
        "schema_version": 1,
        "source_observation_schema": 3,
        "matrix": first.get("matrix"),
        "source_csv_sha256": first.get("csv_sha256"),
        "source_run_ids": [first.get("run_id"), second.get("run_id")],
        "write_authority": "none",
        "policy": "read-only proposals; production changes require a fixed-seed RED and one attribution cluster per commit",
        "slices": slices,
        "card_classification": classify_card_usage(
            first_summary.get("card_usage", {})
            if isinstance(first_summary.get("card_usage"), dict)
            else {}
        ),
        "runtime_totals": first_summary.get("runtime_totals", {}),
        "candidates": candidates,
    }


def write_proposal(proposal: dict[str, object], output_directory: Path) -> None:
    output_directory.mkdir(parents=True, exist_ok=True)
    (output_directory / "proposal.json").write_text(
        json.dumps(proposal, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    lines = [
        "# GameXXK bounded balance proposal",
        "",
        f"- Source SHA: `{proposal.get('source_csv_sha256')}`",
        f"- Matrix: `{proposal.get('matrix')}`",
        f"- Write authority: **{proposal.get('write_authority')}**",
        f"- Candidate count: {len(proposal.get('candidates', []))}",
        "",
        "| Priority | Slice | Direction | Win rate | Target | Suggested cluster |",
        "|---:|---|---|---:|---|---|",
    ]
    for candidate in proposal.get("candidates", []):
        evidence = candidate["evidence"]
        lines.append(
            "| {priority:.3f} | `{candidate}` | {direction} | {rate:.1%} | "
            "{low:.0%}–{high:.0%} | `{cluster}` |".format(
                priority=float(candidate["priority_score"]),
                candidate=candidate["candidate_id"],
                direction=candidate["direction"],
                rate=float(evidence["win_rate"]),
                low=float(candidate["target_interval"][0]),
                high=float(candidate["target_interval"][1]),
                cluster=candidate["recommended_cluster"],
            )
        )
    lines.extend(
        [
            "",
            "This file is diagnostic only. It does not authorize or perform production writes.",
        ]
    )
    (output_directory / "proposal.md").write_text(
        "\n".join(lines) + "\n", encoding="utf-8"
    )


def _load_record(path: Path) -> dict[str, object]:
    record_path = path / "run_summary.json" if path.is_dir() else path
    return json.loads(record_path.read_text(encoding="utf-8-sig"))


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--first", type=Path, required=True)
    parser.add_argument("--second", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)

    first = _load_record(args.first)
    second = _load_record(args.second)
    validate_observation_pair(first, second)
    first_csv = Path(str(first.get("csv_path", "")))
    second_csv = Path(str(second.get("csv_path", "")))
    if not first_csv.is_file() or not second_csv.is_file():
        raise ValueError("observation pair references a missing CSV")
    if _sha256(first_csv) != first.get("csv_sha256") or _sha256(second_csv) != second.get("csv_sha256"):
        raise ValueError("observation pair CSV bytes do not match their recorded SHA")
    with first_csv.open("r", encoding="utf-8-sig", newline="") as source:
        rows = read_case_rows(source)
    proposal = build_proposal(first, second, rows)
    write_proposal(proposal, args.output)
    print(
        f"proposal: {len(proposal['candidates'])} candidates from {len(rows)} cases; "
        f"write_authority={proposal['write_authority']}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
