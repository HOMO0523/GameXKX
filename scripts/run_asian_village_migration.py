"""Safely orchestrate the local UE5.4 -> UE5.8 Asian Village migration."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import subprocess
import sys
import time
from pathlib import Path
from typing import Any, Callable

from scripts.asian_village_migration import (
    EXPECTED_COUNTS,
    SOURCE_ASSET_DIR,
    MigrationError,
    build_manifest,
    canonical_json_bytes,
    _load_manifest,
    stage_and_promote,
    verify_manifest,
    write_manifest,
)
from scripts.ue_mcp_client import DEFAULT_HOST, DEFAULT_PATH, DEFAULT_PORT, UnrealMCPClient
from scripts.ue_tdd_pipeline import is_editor_running, kill_editor, launch_editor, wait_for_mcp


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SOURCE_UPROJECT = Path(r"D:\UE5 demo\zzz\我的项目\我的项目.uproject")
UE54_CMD = Path(r"D:\UE_5.4\Engine\Binaries\Win64\UnrealEditor-Cmd.exe")
TARGET_UPROJECT = PROJECT_ROOT / "GameXXK.uproject"
UE58_CMD = Path(r"D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe")
AUDIT_SCRIPT = PROJECT_ROOT / "Content" / "Python" / "gamexxk_audit_asian_village.py"
TARGET_ASSET_DIR = PROJECT_ROOT / "Content" / "Asian_Village"
STAGING_DIR = PROJECT_ROOT / "Saved" / "MigrationStaging" / "Asian_Village"
EVIDENCE_ROOT = PROJECT_ROOT / "docs" / "production" / "evidence" / "asian-village-migration"
LOG_ROOT = PROJECT_ROOT / "Saved" / "AsianVillageMigration"

PROTECTED_FILES = (
    PROJECT_ROOT / "Content/GameXXK/Maps/L_QingshanInn.umap",
    PROJECT_ROOT / "Content/GameXXK/Maps/Dev/L_Qingshan_PCG_Whitebox_B0R.umap",
    PROJECT_ROOT / "Content/GameXXK/Maps/Dev/L_Qingshan_PCG_Dress_B1.umap",
)

PHASES = (
    "preflight",
    "source_inventory",
    "save_target_editor",
    "close_target_editor",
    "source_ue54_audit",
    "copy_and_verify",
    "target_ue58_upgrade",
    "target_ue58_verify",
    "relaunch_target_editor",
)


class OrchestrationError(RuntimeError):
    pass


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def protected_hashes() -> dict[str, str]:
    missing = [str(path) for path in PROTECTED_FILES if not path.is_file()]
    if missing:
        raise OrchestrationError(f"protected files are missing: {missing}")
    return {
        path.relative_to(PROJECT_ROOT).as_posix(): _sha256(path)
        for path in PROTECTED_FILES
    }


def preflight(*, require_target_absent: bool = True) -> dict[str, Any]:
    required = (SOURCE_ASSET_DIR, SOURCE_UPROJECT, UE54_CMD, TARGET_UPROJECT, UE58_CMD, AUDIT_SCRIPT)
    missing = [str(path) for path in required if not path.exists()]
    if missing:
        raise OrchestrationError(f"migration preflight paths are missing: {missing}")
    if require_target_absent and TARGET_ASSET_DIR.exists():
        raise OrchestrationError(f"target already exists: {TARGET_ASSET_DIR}")
    manifest = build_manifest(SOURCE_ASSET_DIR)
    if manifest["counts"] != EXPECTED_COUNTS:
        raise OrchestrationError(
            f"source counts drifted: expected {EXPECTED_COUNTS}, got {manifest['counts']}"
        )
    return {
        "ok": True,
        "source_counts": manifest["counts"],
        "target_exists": TARGET_ASSET_DIR.exists(),
        "editor_running": is_editor_running(),
        "protected_hashes": protected_hashes(),
    }


def save_and_close_editor(timeout_seconds: float = 60.0) -> None:
    running = is_editor_running()
    if running is False:
        return
    client = UnrealMCPClient(timeout=30.0)
    if not client.connect():
        raise OrchestrationError("GameXXK editor is running but UE MCP is unavailable; refusing to close")
    result = client.save_dirty_packages()
    if not result.get("save_result") or result.get("dirty_after"):
        raise OrchestrationError(f"dirty packages were not safely saved: {result}")
    client.execute_console_command("QUIT_EDITOR")
    deadline = time.time() + timeout_seconds
    while time.time() < deadline:
        if is_editor_running() is False:
            return
        time.sleep(1.0)
    # A forced close is permitted only after the MCP save gate succeeded.
    if not kill_editor():
        raise OrchestrationError("saved editor did not close")


def _load_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise OrchestrationError(f"could not read audit report {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise OrchestrationError(f"audit report is not an object: {path}")
    return value


def run_commandlet(
    editor_cmd: Path,
    uproject: Path,
    mode: str,
    output: Path,
    *,
    runner: Callable[..., subprocess.CompletedProcess[str]] = subprocess.run,
) -> dict[str, Any]:
    output.parent.mkdir(parents=True, exist_ok=True)
    LOG_ROOT.mkdir(parents=True, exist_ok=True)
    if output.exists():
        raise OrchestrationError(f"audit output already exists: {output}")
    command = [
        str(editor_cmd),
        str(uproject),
        f"-ExecutePythonScript={AUDIT_SCRIPT}",
        "-unattended",
        "-nop4",
        "-nosplash",
        "-NoSound",
        "-EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities",
    ]
    environment = dict(os.environ)
    environment.update(
        {
            "GAMEXXK_AV_AUDIT_MODE": mode,
            "GAMEXXK_AV_AUDIT_OUTPUT": str(output),
        }
    )
    completed = runner(command, env=environment, capture_output=True, text=True)
    log_path = LOG_ROOT / f"{mode}.log"
    log_path.write_text(
        (completed.stdout or "") + "\n--- STDERR ---\n" + (completed.stderr or ""),
        encoding="utf-8",
    )
    if completed.returncode != 0:
        raise OrchestrationError(
            f"{mode} commandlet failed with exit {completed.returncode}; see {log_path}"
        )
    report = _load_json(output)
    if report.get("mode") != mode or report.get("ok") is not True:
        raise OrchestrationError(f"{mode} audit reported failure: {report}")
    return report


def _assert_protected(expected: dict[str, str]) -> None:
    actual = protected_hashes()
    if actual != expected:
        raise OrchestrationError("protected Qingshan files changed during migration")


def execute_migration() -> dict[str, Any]:
    state: dict[str, Any] = {"ok": False, "phases": []}
    initial = preflight(require_target_absent=True)
    protected = dict(initial["protected_hashes"])
    state["phases"].append("preflight")

    EVIDENCE_ROOT.mkdir(parents=True, exist_ok=True)
    source_manifest = build_manifest(SOURCE_ASSET_DIR)
    write_manifest(EVIDENCE_ROOT / "source-file-manifest.json", source_manifest)
    state["phases"].append("source_inventory")

    save_and_close_editor()
    state["phases"].extend(("save_target_editor", "close_target_editor"))

    run_commandlet(
        UE54_CMD,
        SOURCE_UPROJECT,
        "source-readonly",
        EVIDENCE_ROOT / "source-ue54-audit.json",
    )
    state["phases"].append("source_ue54_audit")

    copied = stage_and_promote(
        SOURCE_ASSET_DIR,
        TARGET_ASSET_DIR,
        STAGING_DIR,
        source_manifest,
    )
    write_manifest(EVIDENCE_ROOT / "copied-file-manifest.json", copied)
    state["phases"].append("copy_and_verify")

    run_commandlet(
        UE58_CMD,
        TARGET_UPROJECT,
        "target-upgrade",
        EVIDENCE_ROOT / "target-ue58-upgrade.json",
    )
    state["phases"].append("target_ue58_upgrade")

    upgraded = build_manifest(TARGET_ASSET_DIR)
    write_manifest(EVIDENCE_ROOT / "upgraded-file-manifest.json", upgraded)
    run_commandlet(
        UE58_CMD,
        TARGET_UPROJECT,
        "target-verify",
        EVIDENCE_ROOT / "target-ue58-verify.json",
    )
    state["phases"].append("target_ue58_verify")
    _assert_protected(protected)

    if launch_editor() is None or wait_for_mcp(timeout=180.0) is None:
        raise OrchestrationError("GameXXK editor did not relaunch with MCP")
    state["phases"].append("relaunch_target_editor")
    state.update({"ok": True, "protected_hashes": protected, "upgraded_counts": upgraded["counts"]})
    return state


def verify_only() -> dict[str, Any]:
    initial = preflight(require_target_absent=False)
    if not TARGET_ASSET_DIR.is_dir():
        raise OrchestrationError(f"target is missing: {TARGET_ASSET_DIR}")
    manifest = _load_manifest(EVIDENCE_ROOT / "upgraded-file-manifest.json")
    verified = verify_manifest(TARGET_ASSET_DIR, manifest)
    temporary = PROJECT_ROOT / "Saved" / "AsianVillageAudit" / "target-verify-latest.json"
    if temporary.exists():
        temporary.unlink()
    if is_editor_running():
        raise OrchestrationError("verify-only commandlet requires the editor to be closed")
    report = run_commandlet(UE58_CMD, TARGET_UPROJECT, "target-verify", temporary)
    temporary.unlink(missing_ok=True)
    return {
        "ok": True,
        "counts": verified["counts"],
        "audit_asset_count": report["asset_count"],
        "protected_hashes": initial["protected_hashes"],
    }


def _print(value: dict[str, Any]) -> None:
    sys.stdout.buffer.write(canonical_json_bytes(value))


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    action = parser.add_mutually_exclusive_group(required=True)
    action.add_argument("--preflight", action="store_true")
    action.add_argument("--execute", action="store_true")
    action.add_argument("--verify-only", action="store_true")
    args = parser.parse_args(argv)
    try:
        if args.preflight:
            result = preflight(require_target_absent=True)
        elif args.execute:
            result = execute_migration()
        else:
            result = verify_only()
        _print(result)
        return 0
    except (OrchestrationError, MigrationError) as exc:
        sys.stderr.write(str(exc).encode("ascii", "backslashreplace").decode("ascii") + "\n")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
