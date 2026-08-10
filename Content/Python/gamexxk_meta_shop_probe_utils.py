from __future__ import annotations

from collections.abc import Iterable
from typing import Any


def warehouse_ids_from_snapshot(value: Any) -> list[str]:
    """Normalize UE Array and legacy reflected out-parameter tuple shapes."""
    if value is None:
        return []

    candidate = value
    if isinstance(value, tuple):
        nested_sequences = [
            entry
            for entry in value
            if not isinstance(entry, (str, bytes, bool)) and isinstance(entry, Iterable)
        ]
        if nested_sequences:
            candidate = nested_sequences[-1]

    if isinstance(candidate, (str, bytes)) or not isinstance(candidate, Iterable):
        return []
    return [str(entry) for entry in candidate]
