#!/usr/bin/env python3
"""Run and aggregate read-only UE card-balance observations."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import re
import statistics
import subprocess
import sys
import time
from collections import Counter, defaultdict
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Iterable, TextIO


AUTOMATION_TEST = "GameXXK.Diagnostics.CardBalanceObservation"
EXPECTED_CASE_COUNT = 2400
SAFE_RUN_ID = re.compile(r"^[A-Za-z0-9_-]+$")

INTEGER_FIELDS = (
    "enhancement",
    "chapter",
    "seed_ordinal",
    "seed",
    "rounds",
    "remaining_party_health",
    "first_round_deaths",
)

METRIC_FIELDS = (
    "damage_by_source",
    "healing_by_source",
    "armor_by_source",
    "status_produced",
    "status_consumed",
)


@dataclass(frozen=True)
class ObservationConfig:
    project_root: Path
    ue_editor: Path
    timeout_seconds: int = 600

    @property
    def project_file(self) -> Path:
        return self.project_root / "GameXXK.uproject"

    @property
    def evidence_root(self) -> Path:
        return self.project_root / "Saved" / "BalanceObservation"


def parse_metric_map(value: str) -> dict[str, int]:
    result: dict[str, int] = {}
    for item in filter(None, (part.strip() for part in value.split(";"))):
        key, separator, raw_value = item.partition("=")
        if not separator or not key or key in result:
            raise ValueError(f"invalid metric item: {item!r}")
        try:
            result[key] = int(raw_value)
        except ValueError as error:
            raise ValueError(f"invalid metric item: {item!r}") from error
    return result


def read_case_rows(source: TextIO) -> list[dict[str, object]]:
    reader = csv.DictReader(source)
    rows: list[dict[str, object]] = []
    for line_number, row in enumerate(reader, start=2):
        if None in row:
            raise ValueError(f"case CSV row {line_number} has extra columns")
        parsed: dict[str, object] = dict(row)
        try:
            for key in INTEGER_FIELDS:
                parsed[key] = int(row[key])
            for key in METRIC_FIELDS:
                parsed[key] = parse_metric_map(row[key])
        except (KeyError, TypeError, ValueError) as error:
            raise ValueError(f"case CSV row {line_number} is malformed") from error
        rows.append(parsed)
    return rows


def _percentile_nearest_rank(values: Iterable[int], percentile: float) -> int:
    ordered = sorted(values)
    if not ordered:
        raise ValueError("cannot calculate percentile of an empty sequence")
    return ordered[max(0, math.ceil(percentile * len(ordered)) - 1)]


def _outcome_summary(rows: list[dict[str, object]]) -> dict[str, object]:
    outcomes = Counter(str(row["outcome"]) for row in rows)
    resolved = outcomes["Victory"] + outcomes["Defeat"]
    resolved_rounds = [
        int(row["rounds"])
        for row in rows
        if row["outcome"] in {"Victory", "Defeat"}
    ]
    return {
        "case_count": len(rows),
        "outcomes": dict(sorted(outcomes.items())),
        "win_rate_all": round(outcomes["Victory"] / len(rows), 6)
        if rows
        else 0.0,
        "win_rate_resolved": round(outcomes["Victory"] / resolved, 6)
        if resolved
        else 0.0,
        "resolved_rounds": {
            "median": statistics.median(resolved_rounds),
            "p90": _percentile_nearest_rank(resolved_rounds, 0.9),
        }
        if resolved_rounds
        else None,
    }


def aggregate_case_rows(
    rows: list[dict[str, object]],
    expected_case_count: int = EXPECTED_CASE_COUNT,
) -> dict[str, object]:
    if len(rows) != expected_case_count:
        raise ValueError(
            f"expected {expected_case_count} cases, found {len(rows)}"
        )
    identities = {
        (row["cohort"], row["chapter"], row["node"], row["seed"])
        for row in rows
    }
    if len(identities) != len(rows):
        raise ValueError("case identities are not unique")

    produced: Counter[str] = Counter()
    consumed: Counter[str] = Counter()
    source_totals: dict[str, Counter[str]] = {
        field: Counter() for field in METRIC_FIELDS[:3]
    }
    buckets: dict[tuple[str, int, str], list[dict[str, object]]] = defaultdict(list)
    cohort_buckets: dict[str, list[dict[str, object]]] = defaultdict(list)
    chapter_node_buckets: dict[tuple[int, str], list[dict[str, object]]] = defaultdict(list)
    for row in rows:
        produced.update(row["status_produced"])
        consumed.update(row["status_consumed"])
        for field in source_totals:
            source_totals[field].update(row[field])
        buckets[(str(row["cohort"]), int(row["chapter"]), str(row["node"]))].append(row)
        cohort_buckets[str(row["cohort"])].append(row)
        chapter_node_buckets[(int(row["chapter"]), str(row["node"]))].append(row)

    status_utilization = {
        key: {
            "produced": produced[key],
            "consumed": consumed[key],
            "ratio": round(consumed[key] / produced[key], 6)
            if produced[key]
            else 0.0,
        }
        for key in sorted(produced.keys() | consumed.keys())
    }
    top_sources = {
        field: [
            {"source": source, "value": value}
            for source, value in totals.most_common(20)
        ]
        for field, totals in source_totals.items()
    }
    bucket_summaries = {
        f"{cohort}|{chapter}|{node}": _outcome_summary(bucket_rows)
        for (cohort, chapter, node), bucket_rows in sorted(buckets.items())
    }
    cohort_summaries = {
        cohort: _outcome_summary(bucket_rows)
        for cohort, bucket_rows in sorted(cohort_buckets.items())
    }
    chapter_node_summaries = {
        f"{chapter}|{node}": _outcome_summary(bucket_rows)
        for (chapter, node), bucket_rows in sorted(chapter_node_buckets.items())
    }
    stalemates = sorted(
        f"{row['cohort']}|{row['chapter']}|{row['node']}|{row['seed']}"
        for row in rows
        if row["outcome"] == "Stalemate"
    )

    overall = _outcome_summary(rows)
    return {
        **overall,
        "status_utilization": status_utilization,
        "top_sources": top_sources,
        "buckets": bucket_summaries,
        "cohorts": cohort_summaries,
        "chapter_nodes": chapter_node_summaries,
        "recurring_stalemates": stalemates,
    }


def _extract_add_card_blocks(source: str) -> list[str]:
    blocks: list[str] = []
    marker = "AddCard("
    cursor = 0
    while True:
        start = source.find(marker, cursor)
        if start < 0:
            break
        # Ignore the helper function declaration; calls start with Cards.
        if not source.startswith("Cards", start + len(marker)):
            cursor = start + len(marker)
            continue
        depth = 0
        quote = False
        escaped = False
        end = None
        for index in range(start + len("AddCard"), len(source)):
            character = source[index]
            if quote:
                if escaped:
                    escaped = False
                elif character == "\\":
                    escaped = True
                elif character == '"':
                    quote = False
                continue
            if character == '"':
                quote = True
            elif character == "(":
                depth += 1
            elif character == ")":
                depth -= 1
                if depth == 0:
                    end = index + 1
                    break
        if end is None:
            raise ValueError("unterminated AddCard call")
        blocks.append(source[start:end])
        cursor = end
    return blocks


def _quality_ids(source: str, function: str, next_function: str) -> set[str]:
    start = source.index(f"{function}()")
    end = source.index(f"{next_function}()", start)
    return set(re.findall(r'TEXT\("([^"]+)"\)', source[start:end]))


def audit_card_catalog(project_root: Path) -> dict[str, object]:
    catalog_path = project_root / "Source/GameXXK/Private/GameXXKCardCatalog.cpp"
    quality_path = project_root / "Source/GameXXK/Private/GameXXKCardQualityRules.cpp"
    catalog_source = catalog_path.read_text(encoding="utf-8-sig")
    quality_source = quality_path.read_text(encoding="utf-8-sig")
    epic_ids = _quality_ids(quality_source, "GetEpicCardIds", "GetRareCardIds")
    rare_ids = _quality_ids(quality_source, "GetRareCardIds", "GetEpicRelicIds")

    metadata_pattern = re.compile(
        r'TEXT\("([^"]+)"\)\s*,\s*TEXT\("([^"]+)"\)\s*,\s*'
        r'(\d+)\s*,\s*(\d+)\s*,\s*EGameXXKCardTargetMode::(\w+)',
        re.DOTALL,
    )
    cards: list[dict[str, object]] = []
    for block in _extract_add_card_blocks(catalog_source):
        match = metadata_pattern.search(block)
        if not match:
            raise ValueError("could not parse AddCard metadata")
        card_id, display_name, energy, mana, target_mode = match.groups()
        owner_match = re.search(r"EGameXXKCardOwner::(\w+)", block)
        if not owner_match:
            raise ValueError(f"could not parse owner for {card_id}")
        effect_start = block.find("{", match.end())
        if effect_start < 0:
            raise ValueError(f"could not parse effects for {card_id}")
        effect_depth = 0
        effect_end = None
        for effect_index in range(effect_start, len(block)):
            if block[effect_index] == "{":
                effect_depth += 1
            elif block[effect_index] == "}":
                effect_depth -= 1
                if effect_depth == 0:
                    effect_end = effect_index + 1
                    break
        if effect_end is None:
            raise ValueError(f"unterminated effects for {card_id}")
        effect_expression = re.sub(r"\s+", "", block[effect_start:effect_end])
        cards.append(
            {
                "id": card_id,
                "display_name": display_name,
                "energy": int(energy),
                "mana": int(mana),
                "target_mode": target_mode,
                "owner": owner_match.group(1),
                "quality": "Epic"
                if card_id in epic_ids
                else "Rare"
                if card_id in rare_ids
                else "Common",
                "effect_expression": effect_expression,
                "block": block,
            }
        )

    ids = [str(card["id"]) for card in cards]
    if len(set(ids)) != len(ids):
        raise ValueError("card catalog contains duplicate ids")
    quality_counts = Counter(str(card["quality"]) for card in cards)
    energy_counts = Counter(str(card["energy"]) for card in cards)
    owner_counts = Counter(str(card["owner"]) for card in cards)
    draw_token = "EGameXXKCardEffectType::DrawCards"
    resource_tokens = (
        draw_token,
        "EGameXXKCardEffectType::GainMana",
        "EGameXXKCardEffectType::GainManaPerConsumedStatus",
        "EGameXXKCardEffectType::Insight",
    )
    immediate_score_tokens = (
        "Attack(",
        "EGameXXKCardEffectType::DamagePercentAttack",
        "EGameXXKCardEffectType::DamageFlat",
        "EGameXXKCardEffectType::EachLivingAllyAttackSelectedTarget",
        "EGameXXKCardEffectType::Heal",
        "EGameXXKCardEffectType::AddArmor",
    )
    duplicate_signatures: dict[tuple[object, ...], list[str]] = defaultdict(list)
    energy_dominance_signatures: dict[
        tuple[object, ...], list[dict[str, object]]
    ] = defaultdict(list)
    for card in cards:
        duplicate_signatures[
            (
                card["energy"],
                card["mana"],
                card["target_mode"],
                card["effect_expression"],
            )
        ].append(str(card["id"]))
        card_id = str(card["id"])
        energy_dominance_signatures[
            (
                card_id.rsplit(".", 1)[0],
                card["quality"],
                card["mana"],
                card["target_mode"],
                card["effect_expression"],
            )
        ].append({"id": card_id, "energy": card["energy"]})
    setup_only_cards = sorted(
        str(card["id"])
        for card in cards
        if not any(token in str(card["block"]) for token in immediate_score_tokens)
    )
    return {
        "card_count": len(cards),
        "quality_counts": dict(sorted(quality_counts.items())),
        "energy_counts": dict(sorted(energy_counts.items(), key=lambda item: int(item[0]))),
        "owner_counts": dict(sorted(owner_counts.items())),
        "setup_only_for_greedy_policy_count": len(setup_only_cards),
        "setup_only_for_greedy_policy_cards": setup_only_cards,
        "exact_duplicate_effect_groups": sorted(
            sorted(card_ids)
            for card_ids in duplicate_signatures.values()
            if len(card_ids) > 1
        ),
        "strict_energy_dominance_groups": sorted(
            (
                {
                    "cards": sorted(
                        group,
                        key=lambda card: (int(card["energy"]), str(card["id"])),
                    )
                }
                for group in energy_dominance_signatures.values()
                if len(group) > 1
                and len({int(card["energy"]) for card in group}) > 1
            ),
            key=lambda group: tuple(
                str(card["id"]) for card in group["cards"]
            ),
        ),
        "zero_cost_draw_cards": sorted(
            str(card["id"])
            for card in cards
            if card["energy"] == 0 and draw_token in str(card["block"])
        ),
        "zero_cost_resource_cards": sorted(
            str(card["id"])
            for card in cards
            if card["energy"] == 0
            and any(token in str(card["block"]) for token in resource_tokens)
        ),
    }


def validate_automation_report(index_path: Path) -> dict[str, object]:
    if not index_path.is_file():
        raise RuntimeError(f"automation report was not written: {index_path}")
    report = json.loads(index_path.read_text(encoding="utf-8-sig"))
    succeeded = int(report.get("succeeded", 0))
    warned = int(report.get("succeededWithWarnings", 0))
    failed = int(report.get("failed", 0))
    not_run = int(report.get("notRun", 0))
    if failed or not_run or succeeded + warned != 1:
        raise RuntimeError(
            "diagnostic automation failed: "
            f"succeeded={succeeded}, warnings={warned}, failed={failed}, "
            f"notRun={not_run}"
        )
    return report


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(value, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )


def run_observation_once(config: ObservationConfig, run_id: str) -> dict[str, object]:
    if not SAFE_RUN_ID.fullmatch(run_id):
        raise ValueError(f"unsafe observation run id: {run_id!r}")
    if not config.ue_editor.is_file():
        raise FileNotFoundError(config.ue_editor)
    if not config.project_file.is_file():
        raise FileNotFoundError(config.project_file)

    evidence_dir = config.evidence_root / run_id
    report_dir = (
        config.project_root
        / "Saved/Automation/CardBalanceObservation"
        / run_id
    )
    log_path = config.project_root / "Saved/Logs" / f"CardBalanceObservation-{run_id}.log"
    evidence_dir.mkdir(parents=True, exist_ok=True)
    report_dir.mkdir(parents=True, exist_ok=True)
    log_path.parent.mkdir(parents=True, exist_ok=True)
    started = datetime.now().astimezone()
    command = [
        str(config.ue_editor),
        str(config.project_file),
        "-unattended",
        "-nop4",
        "-nosplash",
        "-nullrhi",
        "-nosound",
        "-NoPause",
        f"-GameXXKBalanceObservationId={run_id}",
        f"-AbsLog={log_path}",
        f"-ReportOutputPath={report_dir}",
        "-LogCmds=LogTemp Error",
        f"-ExecCmds=Automation RunTests {AUTOMATION_TEST}; Quit",
    ]
    monotonic_start = time.monotonic()
    process = subprocess.run(
        command,
        cwd=config.project_root,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=config.timeout_seconds,
        check=False,
    )
    duration_seconds = round(time.monotonic() - monotonic_start, 3)
    if process.returncode != 0:
        raise RuntimeError(
            f"UE diagnostic exited {process.returncode}; log={log_path}"
        )
    report = validate_automation_report(report_dir / "index.json")
    csv_path = evidence_dir / "cases.csv"
    if not csv_path.is_file():
        raise RuntimeError(f"diagnostic CSV was not written: {csv_path}")
    with csv_path.open("r", encoding="utf-8-sig", newline="") as source:
        rows = read_case_rows(source)
    summary = aggregate_case_rows(rows)
    record: dict[str, object] = {
        "run_id": run_id,
        "started_at": started.isoformat(),
        "completed_at": datetime.now().astimezone().isoformat(),
        "duration_seconds": duration_seconds,
        "csv_sha256": _sha256(csv_path),
        "csv_path": str(csv_path),
        "report_path": str(report_dir / "index.json"),
        "log_path": str(log_path),
        "automation": {
            key: report.get(key, 0)
            for key in ("succeeded", "succeededWithWarnings", "failed", "notRun")
        },
        "summary": summary,
    }
    _write_json(evidence_dir / "run_summary.json", record)
    return record


def load_existing_runs(
    evidence_root: Path,
    expected_case_count: int = EXPECTED_CASE_COUNT,
) -> list[dict[str, object]]:
    records: list[dict[str, object]] = []
    if not evidence_root.is_dir():
        return records
    for path in sorted(evidence_root.glob("*/run_summary.json")):
        try:
            record = json.loads(path.read_text(encoding="utf-8-sig"))
        except (OSError, json.JSONDecodeError, TypeError, ValueError):
            continue
        csv_path = Path(str(record.get("csv_path", "")))
        if csv_path.is_file():
            with csv_path.open("r", encoding="utf-8-sig", newline="") as source:
                rows = read_case_rows(source)
            record["summary"] = aggregate_case_rows(
                rows,
                expected_case_count=expected_case_count,
            )
            record["csv_sha256"] = _sha256(csv_path)
        records.append(record)
    return records


def write_final_summary(
    records: list[dict[str, object]],
    catalog_audit: dict[str, object],
    evidence_root: Path,
) -> dict[str, object]:
    evidence_root.mkdir(parents=True, exist_ok=True)
    (evidence_root / "observation_runs.jsonl").write_text(
        "".join(
            json.dumps(record, ensure_ascii=False, separators=(",", ":")) + "\n"
            for record in records
        ),
        encoding="utf-8",
    )
    hashes = Counter(str(record["csv_sha256"]) for record in records)
    stalls: Counter[str] = Counter()
    durations: list[float] = []
    for record in records:
        durations.append(float(record["duration_seconds"]))
        summary = record["summary"]
        if isinstance(summary, dict):
            stalls.update(summary.get("recurring_stalemates", []))
    representative = records[-1]["summary"] if records else None
    result: dict[str, object] = {
        "generated_at": datetime.now().astimezone().isoformat(),
        "run_count": len(records),
        "run_ids": [record["run_id"] for record in records],
        "deterministic": bool(records) and len(hashes) == 1,
        "hash_counts": dict(hashes),
        "duration_seconds": {
            "total": round(sum(durations), 3),
            "median": round(statistics.median(durations), 3) if durations else None,
        },
        "recurring_stalemates": dict(stalls.most_common()),
        "catalog_audit": catalog_audit,
        "representative_run": representative,
    }
    _write_json(evidence_root / "final_summary.json", result)

    lines = [
        "# Card balance observation summary",
        "",
        f"- Completed runs: {len(records)}",
        f"- Deterministic CSV: {result['deterministic']}",
        f"- Median UE run time: {result['duration_seconds']['median']} seconds",
    ]
    if isinstance(representative, dict):
        rounds = representative.get("resolved_rounds") or {}
        lines.extend(
            [
                f"- Representative outcomes: {representative.get('outcomes')}",
                f"- Resolved win rate: {representative.get('win_rate_resolved')}",
                f"- Resolved round median / p90: {rounds.get('median')} / {rounds.get('p90')}",
            ]
        )
    lines.extend(
        [
            "",
            "## Catalog audit",
            "",
            f"- Cards: {catalog_audit.get('card_count')}",
            f"- Quality: {catalog_audit.get('quality_counts')}",
            f"- Energy: {catalog_audit.get('energy_counts')}",
            f"- Zero-cost draw: {catalog_audit.get('zero_cost_draw_cards')}",
            "",
            "## Recurring stalemates",
            "",
        ]
    )
    if stalls:
        lines.extend(f"- {identity}: {count}/{len(records)} runs" for identity, count in stalls.most_common())
    else:
        lines.append("- None")
    (evidence_root / "final_summary.md").write_text(
        "\n".join(lines) + "\n", encoding="utf-8"
    )
    return result


def _parse_deadline(value: str) -> datetime:
    parsed = datetime.fromisoformat(value)
    if parsed.tzinfo is None:
        parsed = parsed.replace(tzinfo=datetime.now().astimezone().tzinfo)
    return parsed


def _new_run_id(sequence: int) -> str:
    return datetime.now().strftime("run_%Y%m%d_%H%M%S_") + f"{sequence:03d}"


def main(argv: list[str] | None = None) -> int:
    project_root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--deadline",
        type=_parse_deadline,
        help="local ISO deadline; launch full observations while current time is earlier",
    )
    parser.add_argument("--once", action="store_true")
    parser.add_argument("--summarize-existing", action="store_true")
    parser.add_argument(
        "--ue-editor",
        type=Path,
        default=Path("D:/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"),
    )
    parser.add_argument("--timeout-seconds", type=int, default=600)
    args = parser.parse_args(argv)
    config = ObservationConfig(project_root, args.ue_editor, args.timeout_seconds)

    if not args.summarize_existing and not args.once and args.deadline is None:
        parser.error("choose --once, --deadline, or --summarize-existing")

    sequence = 0
    if not args.summarize_existing:
        while args.once or datetime.now().astimezone() < args.deadline:
            sequence += 1
            run_id = _new_run_id(sequence)
            print(f"[{datetime.now().astimezone().isoformat()}] starting {run_id}", flush=True)
            try:
                record = run_observation_once(config, run_id)
            except Exception as error:
                print(f"{run_id} failed: {error}", file=sys.stderr, flush=True)
                return 1
            outcomes = record["summary"]["outcomes"]
            print(
                f"[{record['completed_at']}] completed {run_id} "
                f"in {record['duration_seconds']}s: {outcomes}",
                flush=True,
            )
            if args.once:
                break

    records = load_existing_runs(config.evidence_root)
    result = write_final_summary(
        records,
        audit_card_catalog(project_root),
        config.evidence_root,
    )
    print(
        f"summary: {result['run_count']} runs, deterministic={result['deterministic']}",
        flush=True,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
