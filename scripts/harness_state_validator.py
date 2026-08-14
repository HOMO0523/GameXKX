#!/usr/bin/env python3
"""Validate GameXXK production-unit state files and loose production reports.

Two kinds of artifacts live under ``docs/production``:

1. Production units (directories) follow the strict 8-file template
   (``00-raw-input`` ... ``07-review``) with required YAML front-matter.
2. Loose reports (``*.md`` files at the root) record recent goal states,
   ledgers, and acceptance records. They are validated with a lighter schema.

Severity rules: ``error`` findings fail the gate; ``warning`` findings are
reported but do not fail unless ``--fail-on-warnings`` is set, so existing
reports without front-matter surface in the output instead of blocking work.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from datetime import datetime
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_ROOT = PROJECT_ROOT / "docs" / "production"

REQUIRED_FILES = [
    "00-raw-input.md",
    "01-semantics.md",
    "02-infra-audit.md",
    "03-plan.md",
    "04-tests.md",
    "05-implementation-log.md",
    "06-test-results.md",
    "07-review.md",
]

REQUIRED_METADATA = [
    "unit_id",
    "status",
    "owner",
    "updated_at",
    "source_commit",
    "depends_on",
    "parallel_lock",
]

LOOSE_REPORT_METADATA = [
    "status",
    "owner",
    "updated_at",
    "source_commit",
]

TERMINAL_STATUSES = {"done", "complete", "closed", "ship", "shipped", "verified"}
NON_UNIT_DIRECTORIES = {"evidence"}
STALE_UNIT_DAYS = 14
STALE_REPORT_DAYS = 30
PLACEHOLDER_COMMITS = {"working-tree", "working tree", "worktree"}


def parse_metadata(path: Path) -> dict[str, str]:
    text = path.read_text(encoding="utf-8", errors="replace").splitlines()
    if not text or text[0].strip() != "---":
        return {}
    metadata: dict[str, str] = {}
    for raw in text[1:80]:
        line = raw.strip()
        if line == "---":
            break
        match = re.match(r"^([A-Za-z0-9_-]+)\s*:\s*(.*)$", line)
        if match:
            metadata[match.group(1).lower().replace("-", "_")] = match.group(2).strip()
    return metadata


def parse_timestamp(value: object) -> datetime | None:
    text = str(value or "").strip()
    if not text:
        return None
    try:
        return datetime.fromisoformat(text.replace("Z", "+00:00"))
    except ValueError:
        return None


def _check_freshness(
    findings: list[dict[str, str]],
    metadata: dict[str, str],
    stale_days: int,
    subject: str,
    kind: str,
) -> None:
    status = metadata.get("status", "").strip().lower()
    if status in TERMINAL_STATUSES or not status:
        return
    updated = parse_timestamp(metadata.get("updated_at"))
    if updated is None:
        findings.append(
            {
                "severity": "warning",
                "path": subject,
                "message": f"{kind} has status '{status}' but updated_at is missing or unparseable",
            }
        )
        return
    now = datetime.now(updated.tzinfo) if updated.tzinfo else datetime.now()
    age_days = (now - updated).days
    if age_days > stale_days:
        findings.append(
            {
                "severity": "warning",
                "path": subject,
                "message": f"{kind} stale: status '{status}' last updated {age_days} days ago (limit {stale_days})",
            }
        )


def _check_dependencies(
    findings: list[dict[str, str]],
    metadata: dict[str, str],
    unit_ids: set[str],
) -> None:
    depends = metadata.get("depends_on", "").strip()
    if not depends or depends == "[]":
        return
    for token in re.split(r"[,\s]+", depends.strip("[]")):
        if token and token not in unit_ids:
            findings.append(
                {
                    "severity": "warning",
                    "path": metadata.get("unit_id", "unit"),
                    "message": f"depends_on references unknown unit: {token}",
                }
            )


def _check_placeholder_commit(
    findings: list[dict[str, str]],
    metadata: dict[str, str],
    subject: str,
) -> None:
    commit = metadata.get("source_commit", "").strip()
    if commit and commit.lower() in PLACEHOLDER_COMMITS:
        findings.append(
            {
                "severity": "warning",
                "path": subject,
                "message": f"source_commit is a placeholder ('{commit}'); pin the real commit once merged",
            }
        )


def validate(
    root: Path,
    require_units: bool,
    *,
    fail_on_warnings: bool = False,
    stale_unit_days: int = STALE_UNIT_DAYS,
    stale_report_days: int = STALE_REPORT_DAYS,
) -> dict:
    findings: list[dict[str, str]] = []
    unit_paths = (
        [
            path
            for path in sorted(root.iterdir())
            if path.is_dir() and path.name not in NON_UNIT_DIRECTORIES
        ]
        if root.exists()
        else []
    )
    unit_ids = {path.name for path in unit_paths}
    if require_units and not unit_paths:
        findings.append({"severity": "error", "path": str(root), "message": "no production units found"})

    active_locks: dict[str, str] = {}
    for unit in unit_paths:
        semantics_metadata: dict[str, str] = {}
        for filename in REQUIRED_FILES:
            path = unit / filename
            if not path.exists():
                findings.append({"severity": "error", "path": str(path), "message": "required file missing"})
                continue
            metadata = parse_metadata(path)
            if filename == "01-semantics.md":
                semantics_metadata = metadata
            for key in REQUIRED_METADATA:
                if key not in metadata:
                    findings.append({"severity": "error", "path": str(path), "message": f"metadata missing: {key}"})
            lock = metadata.get("parallel_lock", "")
            status = metadata.get("status", "").lower()
            if filename == "01-semantics.md" and lock and status not in TERMINAL_STATUSES:
                previous = active_locks.get(lock)
                if previous and previous != str(unit):
                    findings.append({"severity": "error", "path": str(unit), "message": f"duplicate active parallel_lock: {lock}"})
                active_locks[lock] = str(unit)
        if semantics_metadata:
            _check_dependencies(findings, semantics_metadata, unit_ids)
            _check_freshness(findings, semantics_metadata, stale_unit_days, str(unit), "unit")
            _check_placeholder_commit(findings, semantics_metadata, str(unit))

    loose_reports = sorted(root.glob("*.md")) if root.exists() else []
    for path in loose_reports:
        metadata = parse_metadata(path)
        if not metadata:
            findings.append(
                {
                    "severity": "warning",
                    "path": str(path),
                    "message": "loose report missing YAML front-matter (status/owner/updated_at/source_commit)",
                }
            )
            continue
        for key in LOOSE_REPORT_METADATA:
            if key not in metadata:
                findings.append({"severity": "warning", "path": str(path), "message": f"metadata missing: {key}"})
        lock = metadata.get("parallel_lock", "")
        status = metadata.get("status", "").lower()
        if lock and status not in TERMINAL_STATUSES:
            previous = active_locks.get(lock)
            if previous and previous != str(path):
                findings.append({"severity": "error", "path": str(path), "message": f"duplicate active parallel_lock: {lock}"})
            active_locks[lock] = str(path)
        _check_freshness(findings, metadata, stale_report_days, str(path), "report")
        _check_placeholder_commit(findings, metadata, str(path))

    error_findings = [item for item in findings if item["severity"] == "error"]
    ok = not error_findings if not fail_on_warnings else not findings
    return {
        "ok": ok,
        "units": [str(path) for path in unit_paths],
        "loose_reports": [str(path) for path in loose_reports],
        "findings": findings,
    }


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=DEFAULT_ROOT)
    parser.add_argument("--require-units", action="store_true")
    parser.add_argument("--fail-on-warnings", action="store_true", help="Treat warning findings as failures")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args(argv)

    report = validate(args.root, args.require_units, fail_on_warnings=args.fail_on_warnings)
    if args.json:
        print(json.dumps(report, ensure_ascii=False, indent=2))
    else:
        for finding in report["findings"]:
            print(f"{finding['severity'].upper()}: {finding['path']}: {finding['message']}")
        print("OK" if report["ok"] else "FAILED")
    return 0 if report["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
