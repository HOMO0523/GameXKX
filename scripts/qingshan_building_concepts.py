"""Compile deterministic prompt packets for Qingshan building concepts."""

from __future__ import annotations

from pathlib import Path


class ConceptError(ValueError):
    """Raised when a building concept violates its declared contract."""


def validate_building_style(data: dict) -> None:
    raise NotImplementedError("building style validation contract is not implemented")


def validate_golden_asset(asset: dict, style: dict) -> None:
    raise NotImplementedError("golden asset validation contract is not implemented")


def compile_prompt_packet(root: Path, asset_id: str, version: str) -> dict:
    raise NotImplementedError("prompt packet compilation contract is not implemented")


def canonical_json_bytes(data: dict) -> bytes:
    raise NotImplementedError("canonical JSON encoding contract is not implemented")
