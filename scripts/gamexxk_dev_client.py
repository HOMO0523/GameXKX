#!/usr/bin/env python3
"""Call the shared GameXXK Dev service from a local AI or terminal.

Development builds consume atomic JSON requests in their Saved/DevTools/inbox.
Editor sessions can use the same protocol or --via mcp. This client never edits
the player's save file or supplies an alternate combat rule implementation.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import sys
import time
import uuid

PROJECT = Path(__file__).resolve().parent.parent
REQUEST_ID = re.compile(r"^[A-Za-z0-9_-]{1,64}$")


def default_directory() -> Path:
    candidates = [PROJECT / "Saved/InteractiveEditorUser/Saved/DevTools",
                  PROJECT / "Saved/DevTools"]
    return next((p for p in candidates if (p / "inbox").is_dir()), candidates[-1])


def request_payload(command: str, arguments: dict, request_id: str | None = None) -> dict:
    identifier = request_id or uuid.uuid4().hex
    if not REQUEST_ID.fullmatch(identifier):
        raise ValueError("request_id must be 1-64 letters, digits, '_' or '-'")
    if not isinstance(arguments, dict):
        raise ValueError("command arguments must be a JSON object")
    return {"schema": 1, "request_id": identifier, "command": command, "args": arguments}


def submit_file(directory: Path, request: dict, timeout: float = 30.0) -> dict:
    identifier = request["request_id"]
    if not REQUEST_ID.fullmatch(identifier):
        raise ValueError("invalid request_id")
    inbox, outbox = directory / "inbox", directory / "outbox"
    if not inbox.is_dir() or not outbox.is_dir():
        raise RuntimeError("Dev service directory is not ready; enter the game or specify --dev-dir")
    destination, response = inbox / f"{identifier}.json", outbox / f"{identifier}.json"
    if response.is_file():
        return json.loads(response.read_text(encoding="utf-8-sig"))
    if destination.exists():
        if json.loads(destination.read_text(encoding="utf-8-sig")) != request:
            raise ValueError("request_id is already used by a different pending command")
    else:
        temporary = inbox / f"{identifier}.{uuid.uuid4().hex}.tmp"
        temporary.write_text(json.dumps(request, ensure_ascii=False, allow_nan=False), encoding="utf-8")
        os.replace(temporary, destination)
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if response.is_file():
            try:
                result = json.loads(response.read_text(encoding="utf-8-sig"))
            except (OSError, json.JSONDecodeError):
                time.sleep(0.1)
                continue
            if result.get("request_id") != identifier:
                raise RuntimeError("response request_id mismatch")
            return result
        time.sleep(0.2)
    raise TimeoutError(f"Dev request pending: {identifier}; retry with this request_id to collect its result")


def submit_mcp(request: dict, timeout: float = 30.0) -> dict:
    from ue_mcp_client import UnrealMCPClient
    client = UnrealMCPClient(timeout=timeout)
    client.require_connected()
    result = client.run_project_python_file("Content/Python/gamexxk_dev_workbench_probe.py",
                                          ["execute", json.dumps(request, ensure_ascii=False)])
    if not result.get("success"):
        raise RuntimeError(result)
    return json.loads(result["stdout"].strip().splitlines()[-1])


class Client:
    def __init__(self, via: str, directory: Path, timeout: float):
        self.via, self.directory, self.timeout = via, directory, timeout

    def call(self, command: str, arguments: dict | None = None, request_id: str | None = None) -> dict:
        request = request_payload(command, arguments or {}, request_id)
        if self.via == "mcp":
            return submit_mcp(request, self.timeout)
        return submit_file(self.directory, request, self.timeout)

    def simulate(self, arguments: dict, wait_timeout: float) -> dict:
        started = self.call("simulate.start", arguments)
        if not started.get("ok"):
            return started
        expected_id = started.get("data", {}).get("id")
        if not expected_id:
            raise RuntimeError("Dev service did not return a batch id")
        deadline = time.monotonic() + wait_timeout
        last_done = -1
        while time.monotonic() < deadline:
            status = self.call("simulate.status")
            if not status.get("ok"):
                return status
            data = status.get("data", {})
            if data.get("id") != expected_id:
                raise RuntimeError("The batch session changed or the game restarted; the previous result is not complete")
            done = data.get("done", 0)
            if done != last_done:
                print(f"batch {data.get('id')}: {done}/{data.get('total', 0)}", file=sys.stderr, flush=True)
                last_done = done
            if not data.get("running"):
                report = data.get("report", {})
                if data.get("cancelled") or report.get("errors", 0):
                    status["ok"] = False
                    status["message"] = "Batch was cancelled or contains execution errors; inspect the report"
                return status
            time.sleep(1)
        raise TimeoutError("Batch is still running; use simulate.status or simulate.cancel")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--via", choices=["files", "mcp"], default="files")
    parser.add_argument("--dev-dir", type=Path, default=default_directory())
    parser.add_argument("--timeout", type=float, default=30)
    sub = parser.add_subparsers(dest="mode", required=True)
    call = sub.add_parser("call")
    call.add_argument("command")
    call.add_argument("--args", default="{}", help="JSON command arguments")
    call.add_argument("--args-file", type=Path)
    call.add_argument("--request-id")
    export = sub.add_parser("export")
    export.add_argument("path", type=Path)
    load = sub.add_parser("import")
    load.add_argument("path", type=Path)
    sim = sub.add_parser("simulate")
    sim.add_argument("--scene", type=Path)
    sim.add_argument("--stage", default="Training.Normal.1-1")
    sim.add_argument("--encounter", type=int, default=7)
    sim.add_argument("--seed", type=int, default=20260906)
    sim.add_argument("--runs", type=int, default=100)
    sim.add_argument("--max-rounds", type=int, default=100)
    sim.add_argument("--wait-timeout", type=float, default=1800)
    sim.add_argument("--continue-current", action="store_true")
    compare = sub.add_parser("compare", help="Run two saved builds against the same seed group")
    compare.add_argument("--a", type=Path, required=True)
    compare.add_argument("--b", type=Path, required=True)
    compare.add_argument("--stage", default="Training.Hell.3-1")
    compare.add_argument("--encounter", type=int, default=7)
    compare.add_argument("--seed", type=int, default=20260906)
    compare.add_argument("--runs", type=int, default=100)
    compare.add_argument("--max-rounds", type=int, default=100)
    compare.add_argument("--wait-timeout", type=float, default=1800)
    args = parser.parse_args(argv)
    client = Client(args.via, args.dev_dir, args.timeout)
    if args.mode == "call":
        arguments = json.loads(args.args_file.read_text(encoding="utf-8-sig") if args.args_file else args.args)
        result = client.call(args.command, arguments, args.request_id)
    elif args.mode == "export":
        result = client.call("snapshot.export")
        if result.get("ok"):
            args.path.parent.mkdir(parents=True, exist_ok=True)
            args.path.write_text(json.dumps(result["data"], ensure_ascii=False, indent=2), encoding="utf-8")
            result = {"ok": True, "exported": str(args.path.resolve())}
    elif args.mode == "import":
        result = client.call("snapshot.import", {"scene": json.loads(args.path.read_text(encoding="utf-8-sig"))})
    elif args.mode == "simulate":
        parameters = {"stage": args.stage, "encounter": args.encounter, "seed": args.seed,
                      "runs": args.runs, "max_rounds": args.max_rounds, "continue_current": args.continue_current}
        if args.scene:
            parameters["scene"] = json.loads(args.scene.read_text(encoding="utf-8-sig"))
        result = client.simulate(parameters, args.wait_timeout)
    else:
        reports = []
        for scene_path in (args.a, args.b):
            parameters = {"stage": args.stage, "encounter": args.encounter, "seed": args.seed,
                          "runs": args.runs, "max_rounds": args.max_rounds,
                          "scene": json.loads(scene_path.read_text(encoding="utf-8-sig"))}
            response = client.simulate(parameters, args.wait_timeout)
            if not response.get("ok"):
                print(json.dumps(response, ensure_ascii=False, indent=2))
                return 1
            reports.append(response["data"]["report"])
        a, b = reports
        same_seeds = a["seeds"] == b["seeds"]
        result = {"ok": same_seeds, "same_seeds": same_seeds,
                  "a": a, "b": b,
                  "b_minus_a": {key: b[key] - a[key]
                                for key in ("win_rate", "mean_rounds", "mean_remaining_health")}}
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result.get("ok") else 1


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    try:
        raise SystemExit(main())
    except (ValueError, OSError, RuntimeError, TimeoutError) as error:
        print(json.dumps({"ok": False, "error": str(error)}, ensure_ascii=False), file=sys.stderr)
        raise SystemExit(2)
