#!/usr/bin/env python3
"""Measure the existing Qingshan L-A shop through read-only UE MCP."""

from __future__ import annotations

import argparse
import json
import math
import os
import sys
import tempfile
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from gamexxk_content_assembly_check import parse_stdout_json
from ue_mcp_client import DEFAULT_HOST, DEFAULT_PATH, DEFAULT_PORT, UnrealMCPClient


PROJECT_ROOT = Path(__file__).resolve().parents[1]
PROBE_PATH = "Content/Python/gamexxk_probe_qingshan_environment_sources.py"
APPROVED_ROOT = (
    PROJECT_ROOT
    / "SourceAssets"
    / "TownPCG"
    / "QingshanEnvironment"
)
OUTPUT_PATH = APPROVED_ROOT / "source_metrics.json"
SHOP = (
    "/Game/GameXXK/Environment/TownPCG/Prototype/QingshanShopA/"
    "SM_Qingshan_Shop_A_HQ_Retop50K"
)


def parse_probe(payload: dict[str, Any]) -> dict[str, Any]:
    if not isinstance(payload, dict):
        raise ValueError("probe result must be a JSON object")
    if payload.get("ok") is not True:
        raise ValueError("probe result must report ok=true")
    if payload.get("asset_path") != SHOP:
        raise ValueError(f"probe asset_path must equal {SHOP}")

    bounds = payload.get("bounds_size_m")
    if not isinstance(bounds, list) or len(bounds) != 3:
        raise ValueError("bounds_size_m must contain three positive finite values")
    for value in bounds:
        if isinstance(value, bool) or not isinstance(value, (int, float)):
            raise ValueError("bounds_size_m must contain three positive finite values")
        if not math.isfinite(float(value)) or float(value) <= 0:
            raise ValueError("bounds_size_m must contain three positive finite values")

    material_slot_count = payload.get("material_slot_count")
    if (
        isinstance(material_slot_count, bool)
        or not isinstance(material_slot_count, int)
        or material_slot_count <= 0
    ):
        raise ValueError("material_slot_count must be a positive integer")
    return dict(payload)


def build_source_metrics(
    payload: dict[str, Any],
    *,
    endpoint: str,
    captured_at: str,
) -> dict[str, Any]:
    metrics = parse_probe(payload)
    metrics.update(
        {
            "captured_at": captured_at,
            "editor_endpoint": endpoint,
            "probe_path": PROBE_PATH,
        }
    )
    return metrics


def _approved_output_path(output_path: Path) -> Path:
    root = Path(os.path.abspath(APPROVED_ROOT))
    root_resolved = root.resolve(strict=True)
    candidate_input = output_path if output_path.is_absolute() else PROJECT_ROOT / output_path
    candidate = Path(os.path.abspath(candidate_input))

    try:
        candidate.relative_to(root)
    except ValueError as exc:
        raise ValueError(f"output must remain within approved root: {APPROVED_ROOT}") from exc
    if candidate == root:
        raise ValueError(f"output must be a file within approved root: {APPROVED_ROOT}")

    resolved = candidate.resolve(strict=False)
    try:
        resolved.relative_to(root_resolved)
    except ValueError as exc:
        raise ValueError(f"output must remain within approved root: {APPROVED_ROOT}") from exc

    current = candidate
    while current != root:
        is_junction = getattr(current, "is_junction", lambda: False)
        if current.is_symlink() or is_junction():
            raise ValueError(
                f"output must not traverse a symlink or junction outside approved root: {APPROVED_ROOT}"
            )
        current = current.parent
    return candidate


def write_source_metrics(output_path: Path, metrics: dict[str, Any]) -> None:
    output_path = _approved_output_path(output_path)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path = _approved_output_path(output_path)
    temporary_path: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="w",
            encoding="utf-8",
            newline="\n",
            dir=output_path.parent,
            prefix=f".{output_path.name}.",
            suffix=".tmp",
            delete=False,
        ) as handle:
            temporary_path = Path(handle.name)
            json.dump(metrics, handle, ensure_ascii=False, indent=2, sort_keys=True)
            handle.write("\n")
            handle.flush()
            os.fsync(handle.fileno())
        output_path = _approved_output_path(output_path)
        os.replace(temporary_path, output_path)
        temporary_path = None
    finally:
        if temporary_path is not None:
            temporary_path.unlink(missing_ok=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default=DEFAULT_HOST)
    parser.add_argument("--port", type=int, default=DEFAULT_PORT)
    parser.add_argument("--path", default=DEFAULT_PATH)
    parser.add_argument("--output", type=Path, default=OUTPUT_PATH)
    args = parser.parse_args()

    client = UnrealMCPClient(host=args.host, port=args.port, path=args.path, timeout=120.0)
    try:
        if not client.connect():
            raise RuntimeError(f"Cannot connect to UE MCP at {client.endpoint}")
        response = client.run_project_python_file(PROBE_PATH)
        probe = parse_stdout_json(str(response.get("stdout", "")))
        metrics = build_source_metrics(
            probe,
            endpoint=client.endpoint,
            captured_at=datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
        )
        write_source_metrics(args.output, metrics)
    except Exception as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        return 1

    print(json.dumps(metrics, ensure_ascii=False, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    sys.exit(main())
