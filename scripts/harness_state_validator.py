#!/usr/bin/env python3
"""Validate GameXXK production-unit state files."""

from __future__ import annotations

import argparse
import json
import re
import sys
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

NON_UNIT_DIRECTORIES = {"evidence"}


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


def validate(root: Path, require_units: bool) -> dict:
    findings: list[dict[str, str]] = []
    units = [
        path
        for path in sorted(root.iterdir())
        if path.is_dir() and path.name not in NON_UNIT_DIRECTORIES
    ] if root.exists() else []
    if require_units and not units:
        findings.append({"severity": "error", "path": str(root), "message": "no production units found"})

    active_locks: dict[str, str] = {}
    for unit in units:
        for filename in REQUIRED_FILES:
            path = unit / filename
            if not path.exists():
                findings.append({"severity": "error", "path": str(path), "message": "required file missing"})
                continue
            metadata = parse_metadata(path)
            for key in REQUIRED_METADATA:
                if key not in metadata:
                    findings.append({"severity": "error", "path": str(path), "message": f"metadata missing: {key}"})
            lock = metadata.get("parallel_lock", "")
            status = metadata.get("status", "").lower()
            if filename == "01-semantics.md" and lock and status not in {"done", "complete", "closed", "ship", "shipped"}:
                previous = active_locks.get(lock)
                if previous and previous != str(unit):
                    findings.append({"severity": "error", "path": str(unit), "message": f"duplicate active parallel_lock: {lock}"})
                active_locks[lock] = str(unit)
    return {"ok": not any(item["severity"] == "error" for item in findings), "units": [str(path) for path in units], "findings": findings}


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=DEFAULT_ROOT)
    parser.add_argument("--require-units", action="store_true")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args(argv)

    report = validate(args.root, args.require_units)
    if args.json:
        print(json.dumps(report, ensure_ascii=False, indent=2))
    else:
        for finding in report["findings"]:
            print(f"{finding['severity'].upper()}: {finding['path']}: {finding['message']}")
        print("OK" if report["ok"] else "FAILED")
    return 0 if report["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
