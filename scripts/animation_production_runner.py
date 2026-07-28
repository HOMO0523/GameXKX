#!/usr/bin/env python3
"""Single-attempt Dreamina submission runner for production_v1.

The runner deliberately marks an asset attempted before invoking the paid CLI.
An interrupted or failed invocation is never retried automatically.
"""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
import urllib.request
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
PRODUCTION_ROOT = ROOT / "SourceAssets/AnimationProduction/production_v1"
MANIFEST_PATH = PRODUCTION_ROOT / "manifest.json"
LEDGER_PATH = PRODUCTION_ROOT / "ledger.json"
DREAMINA_EXE = Path(
    r"C:\Users\shxuw\AppData\Local\Temp\gamexxk-dreamina-cli-review\dreamina.exe"
)


class PreflightError(RuntimeError):
    pass


def _now() -> str:
    return datetime.now(timezone.utc).isoformat()


def _read_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def _write_json(path: Path, data: dict[str, Any]) -> None:
    temp = path.with_suffix(path.suffix + ".tmp")
    temp.write_text(
        json.dumps(data, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    temp.replace(path)


def _parse_json_output(stdout: str) -> dict[str, Any]:
    start = stdout.find("{")
    end = stdout.rfind("}")
    if start < 0 or end < start:
        raise RuntimeError(f"CLI did not return JSON: {stdout[-500:]}")
    return json.loads(stdout[start : end + 1])


def _run_cli(arguments: list[str], cwd: Path | None = None) -> dict[str, Any]:
    result = subprocess.run(
        [str(DREAMINA_EXE), *arguments],
        cwd=str(cwd or ROOT),
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=60,
        check=False,
    )
    if result.returncode != 0:
        raise RuntimeError(
            f"dreamina exit {result.returncode}: {(result.stderr or result.stdout)[-1000:]}"
        )
    return _parse_json_output(result.stdout)


def get_live_credit() -> int:
    result = _run_cli(["user_credit"])
    return int(result["total_credit"])


def preflight_entry(
    entry: dict[str, Any], state: dict[str, Any], live_credit: int
) -> None:
    if entry.get("automatic_retry") is not False:
        raise PreflightError("automatic retry must be disabled")
    if int(entry.get("max_submissions", 0)) != 1:
        raise PreflightError("max_submissions must equal one")
    if int(state.get("submission_count", 0)) != 0:
        raise PreflightError(f"asset already attempted: {entry['asset_id']}")
    if state.get("status") != "not_submitted":
        raise PreflightError(f"asset is not submit-ready: {entry['asset_id']}")
    if live_credit < int(entry["expected_credits"]):
        raise PreflightError(
            f"insufficient credit for {entry['asset_id']}: "
            f"need {entry['expected_credits']}, have {live_credit}"
        )


def _remaining_reserved_credit(
    manifest: dict[str, Any], ledger: dict[str, Any]
) -> int:
    return sum(
        int(entry["expected_credits"])
        for entry in manifest["entries"]
        if ledger["assets"][entry["asset_id"]]["status"] == "not_submitted"
    )


def apply_reuse_state(
    ledger: dict[str, Any], asset_id: str, source_video: str, reason: str
) -> None:
    state = ledger["assets"][asset_id]
    if state.get("status") != "not_submitted" or int(
        state.get("submission_count", 0)
    ) != 0:
        raise PreflightError(f"asset cannot be reused from current state: {asset_id}")
    state.update(
        {
            "status": "reused_existing",
            "existing_video": source_video,
            "reuse_reason": reason,
            "reused_at": _now(),
        }
    )
    ledger["reused_count"] = int(ledger.get("reused_count", 0)) + 1
    ledger["successful_count"] = int(ledger.get("successful_count", 0)) + 1


def apply_query_result_state(
    ledger: dict[str, Any],
    asset_id: str,
    result: dict[str, Any],
    downloaded_video: str | None,
) -> None:
    state = ledger["assets"][asset_id]
    if state.get("status") != "pending":
        raise PreflightError(f"asset is not pending: {asset_id}")
    generation_status = str(result.get("gen_status", "")).lower()
    if generation_status == "success":
        videos = result.get("result_json", {}).get("videos", [])
        if not videos or not downloaded_video:
            raise RuntimeError(f"successful result has no downloaded video: {asset_id}")
        video = videos[0]
        state.update(
            {
                "status": "success",
                "downloaded_video": downloaded_video,
                "video_metadata": {
                    key: video.get(key)
                    for key in ("fps", "width", "height", "format", "duration")
                },
                "completed_at": _now(),
            }
        )
        ledger["pending_count"] = max(0, int(ledger.get("pending_count", 0)) - 1)
        ledger["successful_count"] = int(ledger.get("successful_count", 0)) + 1
    elif generation_status in {"fail", "failed", "failure", "error"}:
        state.update(
            {
                "status": "failed_no_retry",
                "failure": result.get(
                    "fail_reason", result.get("message", generation_status)
                ),
                "failed_at": _now(),
            }
        )
        ledger["pending_count"] = max(0, int(ledger.get("pending_count", 0)) - 1)
        ledger["failed_count"] = int(ledger.get("failed_count", 0)) + 1


def _download_file(url: str, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    temp = destination.with_suffix(destination.suffix + ".tmp")
    request = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
    try:
        with urllib.request.urlopen(request, timeout=60) as response, temp.open("wb") as out:
            shutil.copyfileobj(response, out)
        temp.replace(destination)
    finally:
        if temp.exists():
            temp.unlink()


def _query_submit_ids(
    pending: list[tuple[str, str]], query_fn: Any, max_workers: int = 6
) -> list[tuple[str, dict[str, Any]]]:
    def query(item: tuple[str, str]) -> tuple[str, dict[str, Any]]:
        asset_id, submit_id = item
        return asset_id, query_fn(["query_result", f"--submit_id={submit_id}"])

    with ThreadPoolExecutor(max_workers=min(max_workers, len(pending) or 1)) as pool:
        return list(pool.map(query, pending))


def _select_entry(
    manifest: dict[str, Any], ledger: dict[str, Any], asset_id: str | None
) -> dict[str, Any]:
    if asset_id:
        for entry in manifest["entries"]:
            if entry["asset_id"] == asset_id:
                return entry
        raise PreflightError(f"unknown asset: {asset_id}")
    for entry in manifest["entries"]:
        state = ledger["assets"][entry["asset_id"]]
        if state["status"] == "not_submitted" and state["submission_count"] == 0:
            return entry
    raise PreflightError("no unsubmitted assets remain")


def _select_entries(
    manifest: dict[str, Any], ledger: dict[str, Any], limit: int
) -> list[dict[str, Any]]:
    if limit < 1:
        raise PreflightError("batch limit must be positive")
    selected = [
        entry
        for entry in manifest["entries"]
        if ledger["assets"][entry["asset_id"]]["status"] == "not_submitted"
        and int(ledger["assets"][entry["asset_id"]]["submission_count"]) == 0
    ][:limit]
    if not selected:
        raise PreflightError("no unsubmitted assets remain")
    return selected


def _entry_inputs(entry: dict[str, Any]) -> tuple[Path, Path, str]:
    first = ROOT / entry["first_frame"]
    last = ROOT / entry["last_frame"]
    prompt_path = ROOT / entry["prompt_file"]
    for path in (first, last, prompt_path):
        if not path.is_file():
            raise PreflightError(f"missing input: {path}")
    prompt = prompt_path.read_text(encoding="utf-8")
    if entry["kind"] == "unit_action" and not prompt.startswith(
        "\u6700\u9ad8\u4f18\u5148\u7ea7\u786c\u6027\u6784\u56fe\u9650\u5236\uff1a"
    ):
        raise PreflightError(f"hard frame limit is not first: {entry['asset_id']}")
    return first, last, prompt


def _submit_prechecked_entry(
    entry: dict[str, Any], ledger: dict[str, Any], live_before: int
) -> dict[str, Any]:
    state = ledger["assets"][entry["asset_id"]]
    preflight_entry(entry, state, live_before)
    first, last, prompt = _entry_inputs(entry)
    state.update(
        {
            "status": "attempting",
            "submission_count": 1,
            "attempt_started_at": _now(),
            "live_credit_before": live_before,
        }
    )
    ledger["submitted_count"] = int(ledger["submitted_count"]) + 1
    _write_json(LEDGER_PATH, ledger)
    raw_dir = PRODUCTION_ROOT / "raw" / entry["asset_id"]
    raw_dir.mkdir(parents=True, exist_ok=True)
    arguments = [
        "frames2video",
        f"--first={first}",
        f"--last={last}",
        f"--prompt={prompt}",
        f"--video_resolution={entry['resolution']}",
        f"--duration={entry['duration_seconds']}",
        f"--model_version={entry['model']}",
        "--poll=0",
    ]
    try:
        result = _run_cli(arguments, cwd=raw_dir)
        credit_count = int(result.get("credit_count", entry["expected_credits"]))
        state.update(
            {
                "status": "pending",
                "submit_id": result["submit_id"],
                "credit_count": credit_count,
                "submitted_at": _now(),
                "raw_dir": raw_dir.relative_to(ROOT).as_posix(),
            }
        )
        ledger["spent_credit_total"] = int(ledger["spent_credit_total"]) + credit_count
        ledger["pending_count"] = int(ledger["pending_count"]) + 1
        _write_json(LEDGER_PATH, ledger)
        return {
            "asset_id": entry["asset_id"],
            "submit_id": result["submit_id"],
            "credit_count": credit_count,
            "status": "pending",
        }
    except Exception as exc:
        state.update(
            {"status": "failed_no_retry", "failure": str(exc), "failed_at": _now()}
        )
        ledger["failed_count"] = int(ledger["failed_count"]) + 1
        _write_json(LEDGER_PATH, ledger)
        raise


def submit_one(asset_id: str | None = None) -> dict[str, Any]:
    if not DREAMINA_EXE.is_file():
        raise PreflightError(f"Dreamina CLI missing: {DREAMINA_EXE}")
    manifest = _read_json(MANIFEST_PATH)
    ledger = _read_json(LEDGER_PATH)
    entry = _select_entry(manifest, ledger, asset_id)
    state = ledger["assets"][entry["asset_id"]]
    live_before = get_live_credit()
    reserved = _remaining_reserved_credit(manifest, ledger)
    if live_before < reserved:
        raise PreflightError(
            f"production reserve violated: need {reserved}, have {live_before}"
        )
    preflight_entry(entry, state, live_before)

    first = ROOT / entry["first_frame"]
    last = ROOT / entry["last_frame"]
    prompt_path = ROOT / entry["prompt_file"]
    for path in (first, last, prompt_path):
        if not path.is_file():
            raise PreflightError(f"missing input: {path}")
    prompt = prompt_path.read_text(encoding="utf-8")
    if entry["kind"] == "unit_action" and not prompt.startswith(
        "\u6700\u9ad8\u4f18\u5148\u7ea7\u786c\u6027\u6784\u56fe\u9650\u5236\uff1a"
    ):
        raise PreflightError(f"hard frame limit is not first: {entry['asset_id']}")

    state.update(
        {
            "status": "attempting",
            "submission_count": 1,
            "attempt_started_at": _now(),
            "live_credit_before": live_before,
        }
    )
    ledger["submitted_count"] = int(ledger["submitted_count"]) + 1
    _write_json(LEDGER_PATH, ledger)

    raw_dir = PRODUCTION_ROOT / "raw" / entry["asset_id"]
    raw_dir.mkdir(parents=True, exist_ok=True)
    arguments = [
        "frames2video",
        f"--first={first}",
        f"--last={last}",
        f"--prompt={prompt}",
        f"--video_resolution={entry['resolution']}",
        f"--duration={entry['duration_seconds']}",
        f"--model_version={entry['model']}",
        "--poll=0",
    ]
    try:
        result = _run_cli(arguments, cwd=raw_dir)
        live_after = get_live_credit()
        state.update(
            {
                "status": "pending",
                "submit_id": result["submit_id"],
                "credit_count": int(result.get("credit_count", entry["expected_credits"])),
                "live_credit_after": live_after,
                "submitted_at": _now(),
                "raw_dir": raw_dir.relative_to(ROOT).as_posix(),
            }
        )
        ledger["spent_credit_total"] = int(ledger["spent_credit_total"]) + int(
            result.get("credit_count", entry["expected_credits"])
        )
        ledger["pending_count"] = int(ledger["pending_count"]) + 1
        _write_json(LEDGER_PATH, ledger)
        return {
            "asset_id": entry["asset_id"],
            "submit_id": result["submit_id"],
            "credit_count": result.get("credit_count", entry["expected_credits"]),
            "live_credit_after": live_after,
            "status": "pending",
        }
    except Exception as exc:
        state.update(
            {
                "status": "failed_no_retry",
                "failure": str(exc),
                "failed_at": _now(),
            }
        )
        ledger["failed_count"] = int(ledger["failed_count"]) + 1
        _write_json(LEDGER_PATH, ledger)
        raise


def submit_batch(limit: int = 3) -> dict[str, Any]:
    if not DREAMINA_EXE.is_file():
        raise PreflightError(f"Dreamina CLI missing: {DREAMINA_EXE}")
    manifest = _read_json(MANIFEST_PATH)
    ledger = _read_json(LEDGER_PATH)
    live_before = get_live_credit()
    reserved = _remaining_reserved_credit(manifest, ledger)
    if live_before < reserved:
        raise PreflightError(
            f"production reserve violated: need {reserved}, have {live_before}"
        )
    entries = _select_entries(manifest, ledger, limit)
    projected_live = live_before
    results: list[dict[str, Any]] = []
    for entry in entries:
        result = _submit_prechecked_entry(entry, ledger, projected_live)
        results.append(result)
        projected_live -= int(result["credit_count"])
    live_after = get_live_credit()
    for result in results:
        ledger["assets"][result["asset_id"]]["live_credit_after_batch"] = live_after
    _write_json(LEDGER_PATH, ledger)
    return {
        "submitted_count": len(results),
        "live_credit_before": live_before,
        "live_credit_after": live_after,
        "results": results,
    }


def reuse_existing(asset_id: str, source_video: str, reason: str) -> dict[str, Any]:
    manifest = _read_json(MANIFEST_PATH)
    ledger = _read_json(LEDGER_PATH)
    if not any(entry["asset_id"] == asset_id for entry in manifest["entries"]):
        raise PreflightError(f"unknown asset: {asset_id}")
    source = Path(source_video)
    if not source.is_absolute():
        source = ROOT / source
    if not source.is_file():
        raise PreflightError(f"existing video missing: {source}")
    source_relative = source.relative_to(ROOT).as_posix()
    apply_reuse_state(ledger, asset_id, source_relative, reason)
    remaining = _remaining_reserved_credit(manifest, ledger)
    ledger["reserved_credit_total"] = remaining
    ledger["projected_credit_after_single_pass"] = (
        int(ledger["starting_credit_snapshot"])
        - int(ledger["spent_credit_total"])
        - remaining
    )
    _write_json(LEDGER_PATH, ledger)
    return {
        "asset_id": asset_id,
        "status": "reused_existing",
        "existing_video": source_relative,
        "remaining_reserved_credit": remaining,
    }


def query_pending(limit: int = 10) -> dict[str, Any]:
    ledger = _read_json(LEDGER_PATH)
    pending_ids = [
        asset_id
        for asset_id, state in ledger["assets"].items()
        if state.get("status") == "pending"
    ][:limit]
    queried = _query_submit_ids(
        [(asset_id, ledger["assets"][asset_id]["submit_id"]) for asset_id in pending_ids],
        _run_cli,
    )
    results: list[dict[str, Any]] = []
    for asset_id, result in queried:
        state = ledger["assets"][asset_id]
        generation_status = str(result.get("gen_status", "")).lower()
        downloaded_relative: str | None = None
        if generation_status == "success":
            videos = result.get("result_json", {}).get("videos", [])
            if not videos:
                raise RuntimeError(f"successful result has no video: {asset_id}")
            raw_dir = PRODUCTION_ROOT / "raw" / asset_id
            destination = raw_dir / f"{state['submit_id']}_video_1.mp4"
            _download_file(videos[0]["video_url"], destination)
            downloaded_relative = destination.relative_to(ROOT).as_posix()
        apply_query_result_state(ledger, asset_id, result, downloaded_relative)
        results.append({"asset_id": asset_id, "status": ledger["assets"][asset_id]["status"]})
        _write_json(LEDGER_PATH, ledger)
    return {"queried_count": len(results), "results": results}


def status() -> dict[str, Any]:
    manifest = _read_json(MANIFEST_PATH)
    ledger = _read_json(LEDGER_PATH)
    counts: dict[str, int] = {}
    for state in ledger["assets"].values():
        counts[state["status"]] = counts.get(state["status"], 0) + 1
    return {
        "live_credit": get_live_credit(),
        "remaining_reserved_credit": _remaining_reserved_credit(manifest, ledger),
        "spent_credit_total": ledger["spent_credit_total"],
        "counts": counts,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)
    submit_parser = subparsers.add_parser("submit-one")
    submit_parser.add_argument("--asset-id")
    batch_parser = subparsers.add_parser("submit-batch")
    batch_parser.add_argument("--limit", type=int, default=3)
    reuse_parser = subparsers.add_parser("reuse-existing")
    reuse_parser.add_argument("--asset-id", required=True)
    reuse_parser.add_argument("--source-video", required=True)
    reuse_parser.add_argument("--reason", required=True)
    query_parser = subparsers.add_parser("query-pending")
    query_parser.add_argument("--limit", type=int, default=10)
    subparsers.add_parser("status")
    args = parser.parse_args()
    try:
        if args.command == "submit-one":
            result = submit_one(args.asset_id)
        elif args.command == "submit-batch":
            result = submit_batch(args.limit)
        elif args.command == "reuse-existing":
            result = reuse_existing(args.asset_id, args.source_video, args.reason)
        elif args.command == "query-pending":
            result = query_pending(args.limit)
        else:
            result = status()
        print(json.dumps(result, ensure_ascii=False, indent=2))
        return 0
    except (PreflightError, RuntimeError, subprocess.TimeoutExpired) as exc:
        print(str(exc), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
