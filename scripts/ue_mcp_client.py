#!/usr/bin/env python3
"""UE 5.8 built-in MCP client helpers for GameXXK automation."""

from __future__ import annotations

import http.client
import json
import socket
import time
import uuid
from pathlib import Path
from typing import Any


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 18765
DEFAULT_PATH = "/mcp"
DEFAULT_PROTOCOL_VERSION = "2025-11-25"
EDITOR_TOOLSET = "EditorToolset.EditorAppToolset"
LOGS_TOOLSET = "EditorToolset.LogsToolset"
GAMEXXK_TDD_TOOLSET = "gamexxk_mcp_tdd_toolset.GameXXKTDDToolset"


def make_json_rpc(method: str, params: dict[str, Any] | None, request_id: int | None) -> bytes:
    payload: dict[str, Any] = {"jsonrpc": "2.0", "method": method}
    if params is not None:
        payload["params"] = params
    if request_id is not None:
        payload["id"] = request_id
    return json.dumps(payload, separators=(",", ":")).encode("utf-8")


def read_sse_event(response: http.client.HTTPResponse, max_wait: float = 30.0) -> bytes:
    chunks: list[bytes] = []
    deadline = time.monotonic() + max_wait
    while time.monotonic() < deadline:
        try:
            chunk = response.read(1)
        except socket.timeout:
            break
        if not chunk:
            break
        chunks.append(chunk)
        raw = b"".join(chunks)
        if b"\r\n\r\n" in raw or b"\n\n" in raw:
            break
        if len(raw) > 4_000_000:
            raise RuntimeError("SSE response exceeded 4MB")
    return b"".join(chunks)


def extract_sse_json(body: bytes) -> dict[str, Any] | None:
    if not body.startswith(b"event:") and b"\ndata:" not in body and b"\r\ndata:" not in body:
        return None
    text = body.decode("utf-8", "ignore")
    data_lines: list[str] = []
    for line in text.splitlines():
        if line.startswith("data:"):
            data_lines.append(line[len("data:") :].strip())
    if not data_lines:
        return None
    return json.loads("\n".join(data_lines))


def parse_json_response(method: str, status: int, body: bytes) -> dict[str, Any]:
    if status < 200 or status >= 300:
        raise RuntimeError(f"{method} failed with HTTP {status}: {body.decode('utf-8', 'ignore')}")
    if not body:
        return {}
    parsed = extract_sse_json(body) or json.loads(body.decode("utf-8"))
    if "error" in parsed:
        raise RuntimeError(f"{method} returned JSON-RPC error: {parsed['error']}")
    return parsed


def extract_text_content(response: dict[str, Any]) -> str:
    return "\n".join(
        item.get("text", "")
        for item in response.get("result", {}).get("content", [])
        if item.get("type") == "text"
    )


def parse_tool_return(response: dict[str, Any]) -> Any:
    text = extract_text_content(response).strip()
    if not text:
        return None
    try:
        parsed = json.loads(text)
    except json.JSONDecodeError as exc:
        preview = text[:1000]
        raise RuntimeError(f"Tool returned non-JSON text: {preview}") from exc
    if isinstance(parsed, dict) and "returnValue" in parsed:
        return parsed["returnValue"]
    return parsed


class UnrealMCPClient:
    def __init__(
        self,
        host: str = DEFAULT_HOST,
        port: int = DEFAULT_PORT,
        path: str = DEFAULT_PATH,
        protocol: str = DEFAULT_PROTOCOL_VERSION,
        timeout: float = 30.0,
    ) -> None:
        self.host = host
        self.port = port
        self.path = path
        self.protocol = protocol
        self.timeout = timeout
        self.session_id: str | None = None
        self._request_id = 0
        self._log_marker: str | None = None

    @property
    def endpoint(self) -> str:
        return f"http://{self.host}:{self.port}{self.path}"

    def _next_id(self) -> int:
        self._request_id += 1
        return self._request_id

    def post(
        self,
        method: str,
        params: dict[str, Any] | None = None,
        request_id: int | None = None,
        timeout: float | None = None,
        session_id: str | None | object = ...,
    ) -> tuple[int, dict[str, str], bytes]:
        body = make_json_rpc(method, params, request_id)
        headers = {
            "Content-Type": "application/json",
            "Accept": "application/json, text/event-stream",
            "Mcp-Protocol-Version": self.protocol,
            "Content-Length": str(len(body)),
        }
        effective_session = self.session_id if session_id is ... else session_id
        if effective_session:
            headers["Mcp-Session-Id"] = str(effective_session)

        connection = http.client.HTTPConnection(self.host, self.port, timeout=timeout or self.timeout)
        try:
            connection.request("POST", self.path, body=body, headers=headers)
            response = connection.getresponse()
            response_headers = {key.lower(): value for key, value in response.getheaders()}
            content_type = response_headers.get("content-type", "")
            if "text/event-stream" in content_type:
                response_body = read_sse_event(response, max_wait=timeout or self.timeout)
            else:
                response_body = response.read()
            return response.status, response_headers, response_body
        finally:
            connection.close()

    def connect(self) -> bool:
        try:
            status, headers, body = self.post(
                "initialize",
                {
                    "protocolVersion": self.protocol,
                    "capabilities": {},
                    "clientInfo": {"name": "GameXXKMCPClient", "version": "1.0"},
                },
                request_id=self._next_id(),
                session_id=None,
                timeout=self.timeout,
            )
            response = parse_json_response("initialize", status, body)
            self.session_id = headers.get("mcp-session-id") or headers.get("Mcp-Session-Id")
            if not self.session_id:
                raise RuntimeError("initialize response did not include Mcp-Session-Id")
            self.protocol = response.get("result", {}).get("protocolVersion") or self.protocol
            self.post("notifications/initialized", request_id=None, timeout=5.0)
            return True
        except Exception:
            self.session_id = None
            return False

    def require_connected(self) -> None:
        if self.session_id is None and not self.connect():
            raise RuntimeError(f"Cannot connect to UE MCP server at {self.endpoint}")

    def call_meta_tool(self, name: str, arguments: dict[str, Any] | None = None, timeout: float | None = None) -> dict[str, Any]:
        self.require_connected()
        status, _, body = self.post(
            "tools/call",
            {"name": name, "arguments": arguments or {}},
            request_id=self._next_id(),
            timeout=timeout or self.timeout,
        )
        return parse_json_response(f"tools/call:{name}", status, body)

    def call_tool(
        self,
        tool_name: str,
        arguments: dict[str, Any] | None = None,
        toolset_name: str | None = None,
        timeout: float | None = None,
    ) -> Any:
        call_arguments: dict[str, Any] = {"tool_name": tool_name, "arguments": arguments or {}}
        if toolset_name:
            call_arguments["toolset_name"] = toolset_name
        response = self.call_meta_tool("call_tool", call_arguments, timeout=timeout)
        return parse_tool_return(response)

    def list_toolsets(self) -> list[str]:
        response = self.call_meta_tool("list_toolsets", timeout=self.timeout)
        text = extract_text_content(response)
        names: list[str] = []
        for line in text.splitlines():
            if not line.startswith("- "):
                continue
            name = line[2:].split(":", 1)[0].strip()
            if name:
                names.append(name)
        return names

    def is_in_pie(self) -> bool:
        return bool(self.call_tool("IsPIERunning", toolset_name=EDITOR_TOOLSET))

    def start_pie(self, warmup_seconds: float = 1.0) -> bool:
        self.call_tool(
            "StartPIE",
            {"options": {"warmupSeconds": max(0.0, float(warmup_seconds))}},
            toolset_name=EDITOR_TOOLSET,
            timeout=max(self.timeout, 45.0),
        )
        return True

    def stop_pie(self) -> bool:
        if not self.is_in_pie():
            return True
        self.call_tool("StopPIE", toolset_name=EDITOR_TOOLSET, timeout=max(self.timeout, 45.0))
        return True

    def clear_log_buffer(self) -> None:
        self._log_marker = f"[TDD] MCP log marker {uuid.uuid4().hex}"
        self.write_log(self._log_marker)

    def get_recent_log_lines(self, num_lines: int = 200, pattern: str = "") -> list[str]:
        max_entries = max(0, min(int(num_lines), 50000))
        return_value = self.call_tool(
            "GetLogEntries",
            {"category": "", "pattern": pattern, "maxEntries": max_entries},
            toolset_name=LOGS_TOOLSET,
            timeout=max(self.timeout, 60.0),
        )
        lines = [str(line).strip() for line in (return_value or []) if str(line).strip()]
        if self._log_marker and self._log_marker in "\n".join(lines):
            marker_index = 0
            for index, line in enumerate(lines):
                if self._log_marker in line:
                    marker_index = index + 1
            lines = lines[marker_index:]
        return lines

    def filter_tdd_lines(self, num_lines: int = 200) -> list[str]:
        return [line for line in self.get_recent_log_lines(num_lines=num_lines, pattern=r"\[TDD\]") if "[TDD]" in line]

    def execute_console_command(self, command: str) -> str:
        result = self.call_tool(
            "execute_console_command",
            {"command": command},
            toolset_name=GAMEXXK_TDD_TOOLSET,
            timeout=max(self.timeout, 60.0),
        )
        return str(result or "")

    def save_dirty_packages(self) -> dict[str, Any]:
        result = self.call_tool("save_dirty_packages", toolset_name=GAMEXXK_TDD_TOOLSET, timeout=max(self.timeout, 120.0))
        if isinstance(result, str):
            return dict(json.loads(result))
        return dict(result or {})

    def get_pie_world_time(self) -> float:
        result = self.call_tool("get_pie_world_time", toolset_name=GAMEXXK_TDD_TOOLSET, timeout=self.timeout)
        try:
            return float(result)
        except (TypeError, ValueError):
            return -1.0

    def get_editor_memory_mb(self) -> float:
        result = self.call_tool("get_editor_memory_mb", toolset_name=GAMEXXK_TDD_TOOLSET, timeout=self.timeout)
        try:
            return float(result)
        except (TypeError, ValueError):
            return 0.0

    def collect_garbage(self, full_purge: bool = True) -> dict[str, Any]:
        result = self.call_tool(
            "collect_garbage",
            {"full_purge": bool(full_purge)},
            toolset_name=GAMEXXK_TDD_TOOLSET,
            timeout=max(self.timeout, 60.0),
        )
        if isinstance(result, str):
            return dict(json.loads(result))
        return dict(result or {})

    def write_log(self, message: str, severity: str = "Log") -> None:
        self.call_tool(
            "write_log",
            {"message": message, "severity": severity},
            toolset_name=GAMEXXK_TDD_TOOLSET,
            timeout=self.timeout,
        )

    def run_project_python_file(
        self,
        relative_path: str,
        argv: list[str] | None = None,
        run_as_main: bool = True,
    ) -> dict[str, Any]:
        result = self.call_tool(
            "run_project_python_file",
            {
                "relative_path": relative_path,
                "argv_json": json.dumps(argv or [], ensure_ascii=False),
                "run_as_main": bool(run_as_main),
            },
            toolset_name=GAMEXXK_TDD_TOOLSET,
            timeout=max(self.timeout, 180.0),
        )
        if isinstance(result, str):
            return dict(json.loads(result))
        return dict(result or {})


if __name__ == "__main__":
    client = UnrealMCPClient()
    if not client.connect():
        raise SystemExit(f"FAIL: Cannot connect to {client.endpoint}")
    print(json.dumps({"endpoint": client.endpoint, "toolsets": client.list_toolsets()}, indent=2, ensure_ascii=False))
