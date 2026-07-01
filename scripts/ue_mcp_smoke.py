#!/usr/bin/env python3
"""Smoke-test the UE 5.8 built-in MCP server for GameXXK."""

from __future__ import annotations

import argparse
import datetime as dt
import http.client
import json
import socket
import sys
import time
from pathlib import Path
from typing import Any


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_PROTOCOL_VERSION = "2025-11-25"
REQUIRED_META_TOOLS = {"list_toolsets", "describe_toolset", "call_tool"}
REQUIRED_TOOLSETS = {
    "EditorToolset.EditorAppToolset",
    "EditorToolset.LogsToolset",
    "GameFeaturesToolset.GameFeaturesToolset",
    "gamexxk_mcp_tdd_toolset.GameXXKTDDToolset",
    "PluginToolset.PluginToolset",
}
GAMEFEATURE_ASSET_MANAGER_ERROR = (
    "Asset manager settings do not include a rule for assets of type GameFeatureData"
)


def make_json_rpc(method: str, params: dict[str, Any] | None, request_id: int | None) -> bytes:
    payload: dict[str, Any] = {"jsonrpc": "2.0", "method": method}
    if params is not None:
        payload["params"] = params
    if request_id is not None:
        payload["id"] = request_id
    return json.dumps(payload, separators=(",", ":")).encode("utf-8")


def post_json_rpc(
    host: str,
    port: int,
    path: str,
    protocol_version: str,
    session_id: str | None,
    method: str,
    params: dict[str, Any] | None = None,
    request_id: int | None = None,
    timeout: float = 10.0,
) -> tuple[int, dict[str, str], bytes]:
    body = make_json_rpc(method, params, request_id)
    headers = {
        "Content-Type": "application/json",
        "Accept": "application/json, text/event-stream",
        "Mcp-Protocol-Version": protocol_version,
        "Content-Length": str(len(body)),
    }
    if session_id:
        headers["Mcp-Session-Id"] = session_id

    connection = http.client.HTTPConnection(host, port, timeout=timeout)
    try:
        connection.request("POST", path, body=body, headers=headers)
        response = connection.getresponse()
        response_headers = {key.lower(): value for key, value in response.getheaders()}
        content_type = response_headers.get("content-type", "")
        if "text/event-stream" in content_type:
            response_body = read_sse_event(response, max_wait=timeout)
        else:
            response_body = response.read()
        return response.status, response_headers, response_body
    finally:
        connection.close()


def read_sse_event(response: http.client.HTTPResponse, max_wait: float = 10.0) -> bytes:
    chunks: list[bytes] = []
    deadline = time.monotonic() + max_wait
    while time.monotonic() < deadline:
        try:
            chunk = response.read1(65536)
        except socket.timeout:
            break
        if not chunk:
            break
        chunks.append(chunk)
        raw = b"".join(chunks)
        if try_extract_sse_json(raw) is not None:
            break
        if len(raw) > 2_000_000:
            raise RuntimeError("SSE response exceeded 2MB")
    return b"".join(chunks)


def iter_sse_payloads(body: bytes) -> list[str]:
    if not body.startswith(b"event:") and b"\ndata:" not in body and b"\r\ndata:" not in body:
        return []
    text = body.decode("utf-8", "ignore")
    events = text.replace("\r\n", "\n").split("\n\n")
    payloads: list[str] = []
    for event in events:
        data_lines: list[str] = []
        for line in event.splitlines():
            if line.startswith("data:"):
                data_lines.append(line[len("data:") :].strip())
        if data_lines:
            payloads.append("\n".join(data_lines))
    return payloads


def try_extract_sse_json(body: bytes) -> dict[str, Any] | None:
    for payload in iter_sse_payloads(body):
        try:
            parsed = json.loads(payload)
        except json.JSONDecodeError:
            continue
        if isinstance(parsed, dict):
            return parsed
    return None


def extract_sse_json(body: bytes) -> dict[str, Any] | None:
    payloads = iter_sse_payloads(body)
    if not payloads:
        return None
    last_error: json.JSONDecodeError | None = None
    for payload in payloads:
        try:
            parsed = json.loads(payload)
        except json.JSONDecodeError as exc:
            last_error = exc
            continue
        if isinstance(parsed, dict):
            return parsed
    if last_error:
        raise last_error
    return None


def parse_json_response(method: str, status: int, body: bytes) -> dict[str, Any]:
    if status < 200 or status >= 300:
        raise RuntimeError(f"{method} failed with HTTP {status}: {body.decode('utf-8', 'ignore')}")
    if not body:
        return {}
    parsed = extract_sse_json(body) or json.loads(body.decode("utf-8"))
    if "error" in parsed:
        raise RuntimeError(f"{method} returned JSON-RPC error: {parsed['error']}")
    return parsed


def parse_toolset_names(toolsets_response: dict[str, Any]) -> list[str]:
    content = toolsets_response.get("result", {}).get("content", [])
    text = "\n".join(item.get("text", "") for item in content if item.get("type") == "text")
    names: list[str] = []
    for line in text.splitlines():
        if not line.startswith("- "):
            continue
        name = line[2:].split(":", 1)[0].strip()
        if name:
            names.append(name)
    return names


def read_log_tail(log_path: Path, offset: int = 0) -> str:
    if not log_path.exists():
        return ""
    with log_path.open("rb") as handle:
        handle.seek(min(offset, log_path.stat().st_size))
        return handle.read().decode("utf-8", "ignore")


def wait_for_log_pattern(log_path: Path, offset: int, pattern: str, timeout: float) -> bool:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if pattern in read_log_tail(log_path, offset):
            return True
        time.sleep(0.25)
    return False


def default_report_path() -> Path:
    timestamp = dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    return PROJECT_ROOT / "Saved" / "HarnessReports" / f"ue-mcp-smoke-{timestamp}.json"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=18765)
    parser.add_argument("--path", default="/mcp")
    parser.add_argument("--protocol", default=DEFAULT_PROTOCOL_VERSION)
    parser.add_argument("--project-log", type=Path, default=PROJECT_ROOT / "Saved" / "Logs" / "GameXXK.log")
    parser.add_argument("--report", type=Path, default=default_report_path())
    parser.add_argument("--tool-timeout", type=float, default=10.0)
    parser.add_argument("--allow-gamefeature-error", action="store_true")
    args = parser.parse_args()

    report: dict[str, Any] = {
        "ok": False,
        "endpoint": f"http://{args.host}:{args.port}{args.path}",
        "protocol": args.protocol,
        "project_log": str(args.project_log),
        "checks": {},
    }

    try:
        initialize_status, initialize_headers, initialize_body = post_json_rpc(
            args.host,
            args.port,
            args.path,
            args.protocol,
            None,
            "initialize",
            {
                "protocolVersion": args.protocol,
                "capabilities": {},
                "clientInfo": {"name": "GameXXKMcpSmoke", "version": "1.0"},
            },
            request_id=1,
        )
        initialize = parse_json_response("initialize", initialize_status, initialize_body)
        session_id = initialize_headers.get("mcp-session-id") or initialize_headers.get("Mcp-Session-Id")
        if not session_id:
            raise RuntimeError("initialize response did not include Mcp-Session-Id")
        negotiated_protocol = (
            initialize.get("result", {}).get("protocolVersion")
            or initialize_headers.get("mcp-protocol-version")
            or args.protocol
        )
        report["session_id"] = session_id
        report["protocol"] = negotiated_protocol
        report["checks"]["initialize"] = {"status": initialize_status, "protocol": negotiated_protocol}

        initialized_status, _, initialized_body = post_json_rpc(
            args.host,
            args.port,
            args.path,
            negotiated_protocol,
            session_id,
            "notifications/initialized",
            request_id=None,
            timeout=5.0,
        )
        if initialized_status not in (200, 202, 204):
            raise RuntimeError(
                "notifications/initialized failed with "
                f"HTTP {initialized_status}: {initialized_body.decode('utf-8', 'ignore')}"
            )
        report["checks"]["initialized_notification"] = {"status": initialized_status}

        tools_status, _, tools_body = post_json_rpc(
            args.host,
            args.port,
            args.path,
            negotiated_protocol,
            session_id,
            "tools/list",
            request_id=2,
        )
        tools_response = parse_json_response("tools/list", tools_status, tools_body)
        tool_names = [tool.get("name", "") for tool in tools_response.get("result", {}).get("tools", [])]
        missing_tools = sorted(REQUIRED_META_TOOLS.difference(tool_names))
        if missing_tools:
            raise RuntimeError(f"tools/list missing required meta tools: {missing_tools}; got {tool_names}")
        report["checks"]["tools_list"] = {"status": tools_status, "tool_names": tool_names}

        log_offset = args.project_log.stat().st_size if args.project_log.exists() else 0
        tool_status, _, tool_body = post_json_rpc(
            args.host,
            args.port,
            args.path,
            negotiated_protocol,
            session_id,
            "tools/call",
            {"name": "list_toolsets", "arguments": {}},
            request_id=3,
            timeout=20.0,
        )
        if tool_status < 200 or tool_status >= 300:
            raise RuntimeError(f"tools/call failed with HTTP {tool_status}: {tool_body.decode('utf-8', 'ignore')}")
        toolsets_response = parse_json_response("tools/call:list_toolsets", tool_status, tool_body)
        toolset_names = parse_toolset_names(toolsets_response)
        missing_toolsets = sorted(REQUIRED_TOOLSETS.difference(toolset_names))
        if missing_toolsets:
            raise RuntimeError(f"list_toolsets missing required toolsets: {missing_toolsets}")
        tool_log_seen = wait_for_log_pattern(
            args.project_log,
            log_offset,
            "Running tool: 'list_toolsets'",
            args.tool_timeout,
        )
        report["checks"]["toolsets"] = {
            "status": tool_status,
            "count": len(toolset_names),
            "required_present": sorted(REQUIRED_TOOLSETS),
        }
        report["checks"]["tool_dispatch"] = {
            "status": tool_status,
            "sse_result_parsed": bool(toolset_names),
            "fresh_log_seen": tool_log_seen,
        }

        describe_status, _, describe_body = post_json_rpc(
            args.host,
            args.port,
            args.path,
            negotiated_protocol,
            session_id,
            "tools/call",
            {
                "name": "describe_toolset",
                "arguments": {"toolset_name": "EditorToolset.EditorAppToolset"},
            },
            request_id=4,
            timeout=20.0,
        )
        describe_response = parse_json_response("tools/call:describe_toolset", describe_status, describe_body)
        describe_text = "\n".join(
            item.get("text", "")
            for item in describe_response.get("result", {}).get("content", [])
            if item.get("type") == "text"
        )
        if "Play" not in describe_text and "Editor" not in describe_text:
            raise RuntimeError("describe_toolset did not return recognizable EditorAppToolset content")
        report["checks"]["describe_editor_toolset"] = {
            "status": describe_status,
            "bytes": len(describe_body),
        }

        log_text = read_log_tail(args.project_log, 0)
        gamefeature_error_seen = GAMEFEATURE_ASSET_MANAGER_ERROR in log_text
        report["checks"]["gamefeature_asset_manager_error"] = {"seen": gamefeature_error_seen}
        if gamefeature_error_seen and not args.allow_gamefeature_error:
            raise RuntimeError(GAMEFEATURE_ASSET_MANAGER_ERROR)

        report["ok"] = True
        return 0
    except (ConnectionRefusedError, TimeoutError, socket.timeout, OSError, RuntimeError, json.JSONDecodeError) as error:
        report["error"] = str(error)
        return 1
    finally:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
        print(json.dumps(report, indent=2, ensure_ascii=False))


if __name__ == "__main__":
    sys.exit(main())
