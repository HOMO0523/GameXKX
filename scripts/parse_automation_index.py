#!/usr/bin/env python3
"""Parse a UE automation index.json and gate on its authoritative counts.

A report directory named ``*_GREEN`` is not evidence; the counts inside
``index.json`` are. This script is the single place that decides whether an
automation run passed:

- discovered / succeeded / succeededWithWarnings / failed / notRun / inProcess
- per-test warnings and errors
- top-level counter vs per-test count cross-check

Gate: 0 failed, 0 notRun, 0 inProcess, 0 errors, and no unexpected states.
Warnings are reported but pass by default (the project classifies warnings
separately); use ``--fail-on-warnings`` to gate them too.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


STATE_BUCKETS = {
    "Success": "succeeded",
    "SuccessWithWarnings": "succeeded_with_warnings",
    "Fail": "failed",
    "Failed": "failed",
    "NotRun": "not_run",
    "InProcess": "in_process",
}

TOP_LEVEL_COUNTERS = {
    "succeeded": "succeeded",
    "succeededWithWarnings": "succeeded_with_warnings",
    "failed": "failed",
    "notRun": "not_run",
    "inProcess": "in_process",
}


def parse_report(path: Path, fail_on_warnings: bool = False) -> dict:
    try:
        data = json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, json.JSONDecodeError) as exc:
        return {"ok": False, "index": str(path), "error": f"cannot read index.json: {exc}"}

    tests = data.get("tests")
    if not isinstance(tests, list):
        return {"ok": False, "index": str(path), "error": "index.json has no tests array"}

    counts = {
        "discovered": len(tests),
        "succeeded": 0,
        "succeeded_with_warnings": 0,
        "failed": 0,
        "not_run": 0,
        "in_process": 0,
        "other": 0,
        "warnings": 0,
        "errors": 0,
    }
    problems: list[str] = []
    for index, test in enumerate(tests):
        if not isinstance(test, dict):
            counts["other"] += 1
            problems.append(f"tests[{index}] is not an object")
            continue
        test_name = str(test.get("fullTestPath") or test.get("testDisplayName") or index)
        state = str(test.get("state", ""))
        bucket = STATE_BUCKETS.get(state)
        if bucket:
            counts[bucket] += 1
        else:
            counts["other"] += 1
            problems.append(f"{test_name}: unexpected state {state!r}")
        warnings = test.get("warnings")
        errors = test.get("errors")
        if isinstance(warnings, int) and warnings > 0:
            counts["warnings"] += warnings
        if isinstance(errors, int) and errors > 0:
            counts["errors"] += errors
        if state in ("Fail", "Failed") or (isinstance(errors, int) and errors > 0):
            problems.append(f"{test_name}: state={state} errors={errors}")

    # UE splits Success vs SuccessWithWarnings at the top level differently
    # than per-test states report them; compare the combined sum for those
    # two, and per-key equality for the rest.
    succeeded_top = data.get("succeeded")
    succeeded_with_warnings_top = data.get("succeededWithWarnings")
    if isinstance(succeeded_top, int) and isinstance(succeeded_with_warnings_top, int):
        top_sum = succeeded_top + succeeded_with_warnings_top
        per_test_sum = counts["succeeded"] + counts["succeeded_with_warnings"]
        if top_sum != per_test_sum:
            problems.append(f"top-level succeeded+succeededWithWarnings={top_sum} disagrees with per-test count {per_test_sum}")
    for key, bucket in TOP_LEVEL_COUNTERS.items():
        if bucket in ("succeeded", "succeeded_with_warnings"):
            continue
        top = data.get(key)
        if isinstance(top, int) and top != counts[bucket]:
            problems.append(f"top-level {key}={top} disagrees with per-test count {counts[bucket]}")

    ok = (
        counts["failed"] == 0
        and counts["not_run"] == 0
        and counts["in_process"] == 0
        and counts["errors"] == 0
        and counts["other"] == 0
        and not problems
    )
    if fail_on_warnings and counts["warnings"] > 0:
        ok = False

    return {
        "ok": ok,
        "index": str(path),
        "counts": counts,
        "problems": problems[:100],
        "problem_count": len(problems),
    }


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--index", type=Path, required=True, help="Path to index.json")
    parser.add_argument("--fail-on-warnings", action="store_true", help="Fail when warnings > 0")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args(argv)

    report = parse_report(args.index, fail_on_warnings=args.fail_on_warnings)
    if args.json:
        print(json.dumps(report, ensure_ascii=False))
    else:
        counts = report.get("counts", {})
        print(f"index: {report['index']}")
        if report.get("error"):
            print(f"ERROR: {report['error']}")
        else:
            print(
                "discovered={discovered} succeeded={succeeded} "
                "succeeded_with_warnings={succeeded_with_warnings} "
                "failed={failed} notRun={not_run} inProcess={in_process} "
                "other={other} warnings={warnings} errors={errors}".format(**counts)
            )
        for problem in report.get("problems", []):
            print(f"PROBLEM: {problem}")
        print("OK" if report["ok"] else "FAILED")
    return 0 if report["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
