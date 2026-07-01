"""
UE TDD Bridge Client — TCP client for UnrealBridge in UE5 editor.

Handles port discovery, script execution, PIE lifecycle, and log capture.
"""

import socket
import json
import struct
import time
import subprocess
import sys
from pathlib import Path

# === Discovery ===

MULTICAST_GROUP = "239.255.42.99"
MULTICAST_PORT = 9876
DISCOVERY_TIMEOUT = 3.0
CONNECT_TIMEOUT = 5.0
EXEC_TIMEOUT = 30


def discover_bridge_port(timeout: float = DISCOVERY_TIMEOUT) -> int | None:
    """Discover UnrealBridge TCP port via UDP multicast.

    Returns the TCP port number, or None if no bridge found.
    """
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(timeout)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    try:
        sock.bind(("", MULTICAST_PORT))
        mreq = struct.pack("4s4s", socket.inet_aton(MULTICAST_GROUP), socket.inet_aton("0.0.0.0"))
        sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)

        data, addr = sock.recvfrom(4096)
        announcement = json.loads(data.decode("utf-8"))
        tcp_port = announcement.get("tcp_port", announcement.get("port", 0))
        if tcp_port:
            return int(tcp_port)
    except socket.timeout:
        pass
    finally:
        sock.close()
    return None


def find_bridge_port_via_netstat(editor_pid: int | None = None) -> int | None:
    """Fallback: scan for UnrealBridge TCP port by probing editor-owned listeners."""
    import subprocess

    editor_pids = []
    if editor_pid is None:
        for image_name in ("UnrealEditor.exe", "UnrealEditor-Cmd.exe"):
            result = subprocess.run(
                ["tasklist", "/fi", f"imagename eq {image_name}", "/fo", "csv", "/nh"],
                capture_output=True, text=True
            )
            for line in result.stdout.strip().split("\n"):
                parts = line.replace('"', "").split(",")
                if len(parts) >= 2 and "UnrealEditor" in parts[0]:
                    editor_pids.append(int(parts[1]))
    else:
        editor_pids.append(editor_pid)

    result = subprocess.run(
        ["netstat", "-ano"], capture_output=True, text=True
    )
    candidate_ports = []
    for line in result.stdout.split("\n"):
        parts = line.split()
        if len(parts) < 5 or "LISTENING" not in line:
            continue
        if not ("127.0.0.1" in line or "0.0.0.0" in line):
            continue
        if editor_pids and parts[-1] not in {str(pid) for pid in editor_pids}:
            continue
        addr = parts[1]
        port = int(addr.split(":")[-1])
        candidate_ports.append(port)

    if not editor_pids:
        candidate_ports.sort(key=lambda candidate: 0 if candidate >= 49152 else 1)

    # Probe each candidate with a ping
    for port in candidate_ports:
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(1.0)
            sock.connect(("127.0.0.1", port))
            payload = json.dumps({"id": "probe", "script": "print('UE_BRIDGE_PROBE')"}).encode()
            sock.sendall(struct.pack(">I", len(payload)) + payload)
            hdr = sock.recv(4)
            if len(hdr) == 4:
                n = struct.unpack(">I", hdr)[0]
                data = b""
                while len(data) < n:
                    chunk = sock.recv(n - len(data))
                    if not chunk:
                        break
                    data += chunk
                response = json.loads(data.decode("utf-8"))
                if response.get("success") and "UE_BRIDGE_PROBE" in response.get("output", ""):
                    sock.close()
                    return port
            sock.close()
        except Exception:
            pass

    return None


# === Bridge Client ===

class BridgeClient:
    """Thin wrapper around UnrealBridge TCP protocol."""

    def __init__(self, host: str = "127.0.0.1", port: int | None = None):
        self.host = host
        self.port = port
        self._counter = 0

    def connect(self) -> bool:
        """Discover and connect. Returns True if bridge is ready."""
        if self.port is None:
            self.port = discover_bridge_port()
            if self.port is None:
                print("[BRIDGE] Discovery failed, trying netstat fallback...")
                self.port = find_bridge_port_via_netstat()
        return self.port is not None

    def send(self, script: str, timeout: int = EXEC_TIMEOUT) -> dict:
        """Send a Python script to the bridge and return the response dict."""
        if self.port is None:
            return {"success": False, "error": "Bridge not connected", "output": ""}

        self._counter += 1
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(timeout)
        try:
            sock.connect((self.host, self.port))
            payload = json.dumps({
                "id": f"tdd_{self._counter}",
                "script": script,
                "timeout": timeout
            }).encode("utf-8")
            sock.sendall(struct.pack(">I", len(payload)) + payload)

            hdr = sock.recv(4)
            if len(hdr) < 4:
                return {"success": False, "error": "No response header", "output": ""}
            n = struct.unpack(">I", hdr)[0]
            data = b""
            while len(data) < n:
                chunk = sock.recv(n - len(data))
                if not chunk:
                    break
                data += chunk
            return json.loads(data.decode("utf-8"))
        except Exception as e:
            return {"success": False, "error": str(e), "output": ""}
        finally:
            sock.close()

    # === PIE Lifecycle ===

    def is_in_pie(self) -> bool:
        r = self.send("import unreal_bridge; print(unreal_bridge.Editor.is_in_pie())")
        return r.get("output", "").strip() == "True"

    def start_pie(self) -> bool:
        r = self.send("import unreal_bridge; unreal_bridge.Editor.start_pie()")
        return r.get("success", False)

    def stop_pie(self) -> bool:
        r = self.send("import unreal_bridge; unreal_bridge.Editor.stop_pie()")
        return r.get("success", False)

    def get_pie_world_time(self) -> float:
        r = self.send("import unreal_bridge; print(unreal_bridge.Editor.get_pie_world_time())")
        try:
            return float(r.get("output", "-1").strip())
        except ValueError:
            return -1.0

    # === Log Capture ===

    def clear_log_buffer(self):
        self.send("import unreal_bridge; unreal_bridge.Editor.clear_log_buffer()")

    def get_recent_log_lines(
        self,
        num_lines: int = 200,
        min_severity: str = "Log",
        max_chars_per_line: int = 8192,
    ) -> list[str]:
        safe_num_lines = max(0, min(int(num_lines), 500))
        safe_chars = max(256, min(int(max_chars_per_line), 16384))
        s = (
            "import unreal_bridge\n"
            f"for line in unreal_bridge.Editor.get_recent_log_lines(num_lines={safe_num_lines}, min_severity=\"{min_severity}\"):\n"
            "    line = str(line).strip()\n"
            "    if '__UB_B64__' in line or '__UB_END__' in line:\n"
            "        continue\n"
            f"    if len(line) > {safe_chars}:\n"
            f"        line = line[:{safe_chars}] + '...[BridgeClientTruncated]'\n"
            "    print(line)\n"
        )
        r = self.send(s)
        output = r.get("output", "")
        return [
            line.strip()
            for line in output.split("\n")
            if line.strip() and "__UB_B64__" not in line and "__UB_END__" not in line
        ]

    def filter_tdd_lines(self, num_lines: int = 200) -> list[str]:
        """Return only [TDD] prefixed log lines."""
        all_lines = self.get_recent_log_lines(num_lines=num_lines)
        return [line for line in all_lines if "[TDD]" in line]

    def write_log(self, message: str, severity: str = "Log"):
        msg_escaped = message.replace("'", "\\'")
        self.send(f"import unreal_bridge; unreal_bridge.Editor.write_log_message(message='{msg_escaped}', severity='{severity}')")

    # === Console ===

    def execute_console_command(self, command: str) -> str:
        cmd_escaped = command.replace("'", "\\'")
        r = self.send(f"import unreal_bridge; print(unreal_bridge.Editor.execute_console_command(command='{cmd_escaped}'))")
        return r.get("output", "").strip()

    # === Editor State ===

    def get_editor_state(self) -> dict | None:
        r = self.send("import unreal_bridge; print(unreal_bridge.Editor.get_editor_state())")
        return r.get("output", "").strip()

    def is_editor_ready(self) -> bool:
        r = self.send("print('ready')")
        return r.get("success", False)


# === CLI ===

if __name__ == "__main__":
    print("=== ue_tdd_bridge self-test ===")
    client = BridgeClient()
    if not client.connect():
        print("FAIL: Could not connect to bridge. Is the editor running?")
        sys.exit(1)

    print(f"Connected to bridge on port {client.port}")
    state = client.get_editor_state()
    print(f"Editor state: {state}")

    pie_status = client.is_in_pie()
    print(f"PIE running: {pie_status}")

    print("\n--- Log capture test ---")
    client.clear_log_buffer()
    client.write_log("[TDD_TEST] bridge self-test message", "Log")
    time.sleep(0.3)
    tdd_lines = client.filter_tdd_lines(50)
    for line in tdd_lines:
        print(f"  {line}")

    print(f"\nFound {len(tdd_lines)} [TDD] lines")
    print("=== Self-test complete ===")
