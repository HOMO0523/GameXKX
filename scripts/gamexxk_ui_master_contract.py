"""Canonical contract and locked-source validation for GameXXK UI Master."""

from __future__ import annotations

from dataclasses import dataclass
import hashlib
import json
from pathlib import Path, PurePosixPath
from typing import Any

from PIL import Image


LOCK_KEYS = (
    "approvedReference",
    "heroIdle",
    "partnerIdle",
    "monsterIdle",
)


@dataclass(frozen=True)
class PageSpec:
    name: str


@dataclass(frozen=True)
class UiMasterContract:
    master_size: tuple[int, int]
    page_size: tuple[int, int]
    columns: int
    gap: int
    pages: tuple[PageSpec, ...]
    output_psd: Path
    overview_scale: float
    phase: str

    def page_origin(self, index: int) -> tuple[int, int]:
        if index < 0 or index >= len(self.pages):
            raise IndexError(f"page index out of range: {index}")
        page_width, page_height = self.page_size
        column = index % self.columns
        row = index // self.columns
        return (
            column * (page_width + self.gap),
            row * (page_height + self.gap),
        )


def _require_mapping(value: Any, field: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise ValueError(f"{field} must be an object")
    return value


def _require_positive_int(mapping: dict[str, Any], field: str) -> int:
    value = mapping.get(field)
    if isinstance(value, bool) or not isinstance(value, int) or value <= 0:
        raise ValueError(f"{field} must be a positive integer")
    return value


def load_contract(path: Path) -> UiMasterContract:
    data = _require_mapping(json.loads(path.read_text(encoding="utf-8")), "contract")
    master = _require_mapping(data.get("masterCanvas"), "masterCanvas")
    page = _require_mapping(data.get("page"), "page")
    pages_value = data.get("pages")
    if not isinstance(pages_value, list) or not pages_value:
        raise ValueError("pages must be a non-empty array")

    pages: list[PageSpec] = []
    for index, item in enumerate(pages_value):
        page_item = _require_mapping(item, f"pages[{index}]")
        name = page_item.get("name")
        if not isinstance(name, str) or not name.strip():
            raise ValueError(f"pages[{index}].name must be a non-empty string")
        pages.append(PageSpec(name=name))
    if len({item.name for item in pages}) != len(pages):
        raise ValueError("page names must be unique")

    contract = UiMasterContract(
        master_size=(
            _require_positive_int(master, "width"),
            _require_positive_int(master, "height"),
        ),
        page_size=(
            _require_positive_int(page, "width"),
            _require_positive_int(page, "height"),
        ),
        columns=_require_positive_int(page, "columns"),
        gap=_require_positive_int(page, "gap"),
        pages=tuple(pages),
        output_psd=Path(str(data.get("outputPsd", ""))),
        overview_scale=float(data.get("overviewScale", 0)),
        phase=str(data.get("phase", "")),
    )

    if not str(contract.output_psd):
        raise ValueError("outputPsd must be a non-empty path")
    if not 0 < contract.overview_scale <= 1:
        raise ValueError("overviewScale must be greater than 0 and at most 1")
    if not contract.phase:
        raise ValueError("phase must be non-empty")

    rows = (len(contract.pages) + contract.columns - 1) // contract.columns
    expected_master = (
        contract.columns * contract.page_size[0]
        + (contract.columns - 1) * contract.gap,
        rows * contract.page_size[1] + (rows - 1) * contract.gap,
    )
    if contract.master_size != expected_master:
        raise ValueError(
            f"masterCanvas does not match page grid: expected {expected_master}"
        )
    return contract


def _is_retired(source_path: str, retired_roots: list[str]) -> bool:
    normalized = PurePosixPath(source_path.replace("\\", "/")).as_posix().rstrip("/")
    return any(
        normalized == root.rstrip("/")
        or normalized.startswith(root.rstrip("/") + "/")
        for root in retired_roots
    )


def validate_source_lock(lock_path: Path, project_root: Path) -> dict[str, Any]:
    errors: list[str] = []
    checked: list[str] = []
    try:
        lock = _require_mapping(
            json.loads(lock_path.read_text(encoding="utf-8")), "source lock"
        )
    except (OSError, json.JSONDecodeError, ValueError) as exc:
        return {"ok": False, "checked": checked, "errors": [str(exc)]}

    retired_value = lock.get("retiredSourceRoots", [])
    retired_roots = (
        [str(item).replace("\\", "/").rstrip("/") for item in retired_value]
        if isinstance(retired_value, list)
        else []
    )
    root = project_root.resolve()

    for key in LOCK_KEYS:
        record = lock.get(key)
        if not isinstance(record, dict):
            errors.append(f"missing source lock record: {key}")
            continue
        checked.append(key)
        source_path = record.get("path")
        if not isinstance(source_path, str) or not source_path:
            errors.append(f"invalid source path: {key}")
            continue
        normalized = source_path.replace("\\", "/")
        pure_path = PurePosixPath(normalized)
        if pure_path.is_absolute() or ".." in pure_path.parts:
            errors.append(f"source path must stay inside project: {key}")
            continue
        if _is_retired(normalized, retired_roots):
            errors.append(f"retired source path: {key}")
            continue

        full_path = (root / Path(*pure_path.parts)).resolve()
        try:
            full_path.relative_to(root)
        except ValueError:
            errors.append(f"source path must stay inside project: {key}")
            continue
        if not full_path.is_file():
            errors.append(f"source file missing: {key}")
            continue

        digest = hashlib.sha256(full_path.read_bytes()).hexdigest()
        if digest.lower() != str(record.get("sha256", "")).lower():
            errors.append(f"source hash mismatch: {key}")
        try:
            with Image.open(full_path) as image:
                dimensions = image.size
        except OSError:
            errors.append(f"source image unreadable: {key}")
            continue
        expected_dimensions = (record.get("width"), record.get("height"))
        if dimensions != expected_dimensions:
            errors.append(f"source dimensions mismatch: {key}")

    return {"ok": not errors, "checked": checked, "errors": errors}
