#!/usr/bin/env python3
"""Measure the existing Qingshan L-A shop through read-only UE MCP."""

from __future__ import annotations

import argparse
import json
import math
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from gamexxk_content_assembly_check import parse_stdout_json
from ue_mcp_client import DEFAULT_HOST, DEFAULT_PATH, DEFAULT_PORT, UnrealMCPClient


PROJECT_ROOT = Path(__file__).resolve().parents[1]
PROBE_PATH = "Content/Python/gamexxk_probe_qingshan_environment_sources.py"
OUTPUT_PATH = (
    PROJECT_ROOT
    / "SourceAssets"
    / "TownPCG"
    / "QingshanEnvironment"
    / "source_metrics.json"
)
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
        or material_slot_count < 0
    ):
        raise ValueError("material_slot_count must be a non-negative integer")
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
    except Exception as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        return 1

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(metrics, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    print(json.dumps(metrics, ensure_ascii=False, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    sys.exit(main())
