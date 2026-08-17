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

PROJECT_ROOT = Path(__file__).resolve().parents[1]
if str(PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT))

from scripts.asian_village_migration import (
    EXPECTED_COUNTS,
    MigrationError,
    build_manifest,
    canonical_json_bytes,
    _load_manifest,
    resolve_source_asset_dir as _resolve_source_asset_dir,
    stage_and_promote,
    verify_manifest,
    write_manifest,
)
from scripts.ue_mcp_client import DEFAULT_HOST, DEFAULT_PATH, DEFAULT_PORT, UnrealMCPClient
from scripts.ue_tdd_pipeline import is_editor_running, kill_editor, launch_editor, wait_for_mcp


TARGET_UPROJECT = PROJECT_ROOT / "GameXXK.uproject"
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


def resolve_source_asset_dir(cli_value: str | os.PathLike[str] | None = None) -> Path:
    try:
        return _resolve_source_asset_dir(cli_value)
    except RuntimeError as exc:
        raise OrchestrationError(str(exc)) from exc


def resolve_engine_root(
    cli_value: str | os.PathLike[str] | None,
    *,
    environment_key: str,
    option_name: str,
    display_name: str,
) -> Path:
    value = cli_value or os.environ.get(environment_key)
    if not value:
        raise OrchestrationError(
            f"{display_name} is not configured; pass {option_name} or set {environment_key}"
        )
    return Path(value).expanduser().resolve(strict=False)


def resolve_migration_paths(
    *,
    source: str | os.PathLike[str] | None = None,
    source_uproject: str | os.PathLike[str] | None = None,
    ue54_root: str | os.PathLike[str] | None = None,
    ue58_root: str | os.PathLike[str] | None = None,
) -> dict[str, Path]:
    source_asset_dir = resolve_source_asset_dir(source)
    if source_uproject:
        source_project = Path(source_uproject).expanduser().resolve(strict=False)
    else:
        configured_project = os.environ.get("GAMEXXK_ASIAN_VILLAGE_UPROJECT")
        if configured_project:
            source_project = Path(configured_project).expanduser().resolve(strict=False)
        else:
            source_project_root = source_asset_dir.parent.parent
            source_project = source_project_root / f"{source_project_root.name}.uproject"
    ue54_root_path = resolve_engine_root(
        ue54_root,
        environment_key="GAMEXXK_UE54_ROOT",
        option_name="--ue54-root",
        display_name="UE5.4 root",
    )
    ue58_root_path = resolve_engine_root(
        ue58_root,
        environment_key="GAMEXXK_UE_ROOT",
        option_name="--ue58-root",
        display_name="UE5.8 root",
    )
    return {
        "source_asset_dir": source_asset_dir,
        "source_uproject": source_project,
        "ue54_cmd": ue54_root_path / "Engine" / "Binaries" / "Win64" / "UnrealEditor-Cmd.exe",
        "ue58_cmd": ue58_root_path / "Engine" / "Binaries" / "Win64" / "UnrealEditor-Cmd.exe",
    }


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


def preflight(
    *,
    require_target_absent: bool = True,
    resolved_paths: dict[str, Path] | None = None,
) -> dict[str, Any]:
    paths = resolved_paths or resolve_migration_paths()
    required = (
        paths["source_asset_dir"],
        paths["source_uproject"],
        paths["ue54_cmd"],
        TARGET_UPROJECT,
        paths["ue58_cmd"],
        AUDIT_SCRIPT,
    )
    missing = [str(path) for path in required if not path.exists()]
    if missing:
        raise OrchestrationError(f"migration preflight paths are missing: {missing}")
    if require_target_absent and TARGET_ASSET_DIR.exists():
        raise OrchestrationError(f"target already exists: {TARGET_ASSET_DIR}")
    manifest = build_manifest(paths["source_asset_dir"])
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
        "-run=pythonscript",
        f"-script={AUDIT_SCRIPT.as_posix()}",
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
    completed = runner(command, env=environment, capture_output=True)
    def decode_stream(value: Any) -> str:
        if isinstance(value, bytes):
            return value.decode("utf-8", errors="backslashreplace")
        return str(value or "")
    stdout = decode_stream(completed.stdout)
    stderr = decode_stream(completed.stderr)
    log_path = LOG_ROOT / f"{mode}.log"
    log_path.write_text(
        stdout + "\n--- STDERR ---\n" + stderr,
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


def ensure_manifest(path: Path, manifest: dict[str, Any]) -> None:
    """Write a new manifest or require an existing resume checkpoint to match."""
    if path.exists():
        existing = _load_manifest(path)
        if existing != manifest:
            raise OrchestrationError(f"existing migration checkpoint drifted: {path}")
        return
    write_manifest(path, manifest)


def _assert_protected(expected: dict[str, str]) -> None:
    actual = protected_hashes()
    if actual != expected:
        raise OrchestrationError("protected Qingshan files changed during migration")


def execute_migration(
    *,
    source: str | os.PathLike[str] | None = None,
    source_uproject: str | os.PathLike[str] | None = None,
    ue54_root: str | os.PathLike[str] | None = None,
    ue58_root: str | os.PathLike[str] | None = None,
) -> dict[str, Any]:
    state: dict[str, Any] = {"ok": False, "phases": []}
    paths = resolve_migration_paths(
        source=source,
        source_uproject=source_uproject,
        ue54_root=ue54_root,
        ue58_root=ue58_root,
    )
    initial = preflight(require_target_absent=True, resolved_paths=paths)
    protected = dict(initial["protected_hashes"])
    state["phases"].append("preflight")

    EVIDENCE_ROOT.mkdir(parents=True, exist_ok=True)
    source_manifest = build_manifest(paths["source_asset_dir"])
    ensure_manifest(EVIDENCE_ROOT / "source-file-manifest.json", source_manifest)
    state["phases"].append("source_inventory")

    save_and_close_editor()
    state["phases"].extend(("save_target_editor", "close_target_editor"))

    run_commandlet(
        paths["ue54_cmd"],
        paths["source_uproject"],
        "source-readonly",
        EVIDENCE_ROOT / "source-ue54-audit.json",
    )
    state["phases"].append("source_ue54_audit")

    copied = stage_and_promote(
        paths["source_asset_dir"],
        TARGET_ASSET_DIR,
        STAGING_DIR,
        source_manifest,
    )
    ensure_manifest(EVIDENCE_ROOT / "copied-file-manifest.json", copied)
    state["phases"].append("copy_and_verify")

    run_commandlet(
        paths["ue58_cmd"],
        TARGET_UPROJECT,
        "target-upgrade",
        EVIDENCE_ROOT / "target-ue58-upgrade.json",
    )
    state["phases"].append("target_ue58_upgrade")

    upgraded = build_manifest(TARGET_ASSET_DIR)
    ensure_manifest(EVIDENCE_ROOT / "upgraded-file-manifest.json", upgraded)
    run_commandlet(
        paths["ue58_cmd"],
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


def verify_only(
    *,
    source: str | os.PathLike[str] | None = None,
    source_uproject: str | os.PathLike[str] | None = None,
    ue54_root: str | os.PathLike[str] | None = None,
    ue58_root: str | os.PathLike[str] | None = None,
) -> dict[str, Any]:
    paths = resolve_migration_paths(
        source=source,
        source_uproject=source_uproject,
        ue54_root=ue54_root,
        ue58_root=ue58_root,
    )
    initial = preflight(require_target_absent=False, resolved_paths=paths)
    if not TARGET_ASSET_DIR.is_dir():
        raise OrchestrationError(f"target is missing: {TARGET_ASSET_DIR}")
    manifest = _load_manifest(EVIDENCE_ROOT / "upgraded-file-manifest.json")
    verified = verify_manifest(TARGET_ASSET_DIR, manifest)
    temporary = PROJECT_ROOT / "Saved" / "AsianVillageAudit" / "target-verify-latest.json"
    if temporary.exists():
        temporary.unlink()
    if is_editor_running():
        raise OrchestrationError("verify-only commandlet requires the editor to be closed")
    report = run_commandlet(paths["ue58_cmd"], TARGET_UPROJECT, "target-verify", temporary)
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
    parser.add_argument("--source", help="Asian Village source directory (or GAMEXXK_ASIAN_VILLAGE_SOURCE)")
    parser.add_argument("--source-uproject", help="UE5.4 source project file (or GAMEXXK_ASIAN_VILLAGE_UPROJECT)")
    parser.add_argument("--ue54-root", help="UE5.4 installation root (or GAMEXXK_UE54_ROOT)")
    parser.add_argument("--ue58-root", help="UE5.8 installation root (or GAMEXXK_UE_ROOT)")
    args = parser.parse_args(argv)
    try:
        if args.preflight:
            paths = resolve_migration_paths(
                source=args.source,
                source_uproject=args.source_uproject,
                ue54_root=args.ue54_root,
                ue58_root=args.ue58_root,
            )
            result = preflight(require_target_absent=True, resolved_paths=paths)
        elif args.execute:
            result = execute_migration(
                source=args.source,
                source_uproject=args.source_uproject,
                ue54_root=args.ue54_root,
                ue58_root=args.ue58_root,
            )
        else:
            result = verify_only(
                source=args.source,
                source_uproject=args.source_uproject,
                ue54_root=args.ue54_root,
                ue58_root=args.ue58_root,
            )
        _print(result)
        return 0
    except (OrchestrationError, MigrationError) as exc:
        sys.stderr.write(str(exc).encode("ascii", "backslashreplace").decode("ascii") + "\n")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
