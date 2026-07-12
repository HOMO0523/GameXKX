"""Compile deterministic prompt packets for Qingshan building concepts."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path, PurePosixPath, PureWindowsPath


class ConceptError(ValueError):
    """Raised when a building concept violates its declared contract."""


_TOP_LEVEL_FIELDS = {
    "schema_version",
    "style_id",
    "inherits",
    "inherits_sha256",
    "camera_contract",
    "composition_contract",
    "material_contract",
    "shape_contract",
    "detail_budget",
    "negative_prompt",
    "prompt_contract",
    "reference_images",
}

_CANONICAL_SECTIONS = {
    "camera_contract": {
        "view": "fixed_high_three_quarter",
        "single_complete_object": True,
        "visible_base": True,
    },
    "composition_contract": {
        "background": "clean_warm_rice_paper",
        "subject_count": 1,
        "text_or_annotations": False,
    },
    "material_contract": {
        "wall": "warm_off_white_plaster",
        "window_fill": "warm_yellow_translucent_rice_paper",
        "outline": "dark_teal_variable_dry_brush",
    },
    "shape_contract": {
        "exaggeration": "playful_asymmetric_moderate",
        "doors_windows": "few_large_thick_frames",
        "posts_beams": "chunky_single_readable_masses",
        "roof": "one_bold_bent_silhouette_no_tile_repetition",
    },
}

_CANONICAL_ERROR_DESCRIPTIONS = {
    "camera_contract.view": "fixed high three-quarter camera",
    "camera_contract.single_complete_object": "single complete object",
    "camera_contract.visible_base": "visible base",
    "composition_contract.background": "warm rice paper background",
    "composition_contract.subject_count": "single subject",
    "composition_contract.text_or_annotations": "no text annotations",
    "material_contract.window_fill": "warm yellow xuan paper windows",
    "shape_contract.doors_windows": "bold doors and windows",
    "shape_contract.posts_beams": "bold beams and columns",
    "shape_contract.roof": "one bold roof silhouette",
}

_DETAIL_FORBIDDEN = ["individual_roof_tiles", "repeated_window_lattices"]
_NEGATIVE_PROMPT = [
    "black_window_panes",
    "individual_roof_tiles",
    "fine_window_lattices",
    "triple_layer_fine_column_capitals",
    "over_fragmentation",
    "speckled_noise",
    "excessive_weirdness",
    "mirror_symmetry",
    "text",
    "annotations",
    "multiple_buildings",
    "scene_collage",
]
_PROMPT_POSITIVE = [
    "fixed high three-quarter camera",
    "one standalone building on a solid warm rice-paper background",
    "warm yellow xuan-paper windows",
    "bold doors, windows, beams, and columns",
    "expressive exaggeration without excessive distortion",
]
_PROMPT_NEGATIVE = [
    "text, labels, callouts, or dimension annotations",
    "individual roof tiles or tiny repeated lattice grids",
    "black window panes",
]


def _expect_mapping(value: object, path: str, fields: set[str]) -> dict:
    if type(value) is not dict:
        raise ConceptError(f"{path} must be an object")
    missing = fields - value.keys()
    if missing:
        field = sorted(missing)[0]
        raise ConceptError(f"{path}.{field} is required")
    unknown = value.keys() - fields
    if unknown:
        field = sorted(unknown)[0]
        raise ConceptError(f"{path}.{field} is not allowed")
    return value


def _expect_exact(value: object, expected: object, path: str) -> None:
    if type(value) is not type(expected):
        raise ConceptError(f"{path} must have type {type(expected).__name__}")
    if value != expected:
        description = _CANONICAL_ERROR_DESCRIPTIONS.get(path, repr(expected))
        raise ConceptError(f"{path} must be {description}")


def _expect_string_list(value: object, path: str) -> list[str]:
    if type(value) is not list:
        raise ConceptError(f"{path} must be an array")
    for index, item in enumerate(value):
        if type(item) is not str:
            raise ConceptError(f"{path}[{index}] must be a string")
    return value


def _validate_digest(value: object, path: str) -> None:
    if type(value) is not str or len(value) != 64 or any(character not in "0123456789abcdef" for character in value):
        raise ConceptError(f"{path} must be a lowercase SHA-256 digest")


def _validate_catalog_path(value: object, path: str) -> str:
    if type(value) is not str:
        raise ConceptError(f"{path} must be a string")
    posix_path = PurePosixPath(value)
    windows_path = PureWindowsPath(value)
    if (
        not value
        or "\\" in value
        or posix_path.is_absolute()
        or windows_path.is_absolute()
        or bool(windows_path.drive)
        or ".." in posix_path.parts
        or value != posix_path.as_posix()
    ):
        raise ConceptError(f"{path} must be a canonical catalog-root-relative POSIX path")
    return value


def validate_building_style(data: dict) -> None:
    style = _expect_mapping(data, "style", _TOP_LEVEL_FIELDS)
    _expect_exact(style["schema_version"], 1, "schema_version")
    _expect_exact(style["style_id"], "QS_InkToon_Building_v2", "style_id")
    _expect_exact(style["inherits"], "style/QS_InkToon_v1.json", "inherits")
    _validate_catalog_path(style["inherits"], "inherits")
    _validate_digest(style["inherits_sha256"], "inherits_sha256")

    for section_name, expected_section in _CANONICAL_SECTIONS.items():
        section = _expect_mapping(style[section_name], section_name, set(expected_section))
        for field, expected in expected_section.items():
            _expect_exact(section[field], expected, f"{section_name}.{field}")

    detail_budget = _expect_mapping(style["detail_budget"], "detail_budget", {"forbidden"})
    forbidden = _expect_string_list(detail_budget["forbidden"], "detail_budget.forbidden")
    if forbidden != _DETAIL_FORBIDDEN:
        missing = set(_DETAIL_FORBIDDEN) - set(forbidden)
        description = "individual roof tiles" if "individual_roof_tiles" in missing else "repeated window lattices"
        raise ConceptError(f"detail_budget.forbidden must forbid {description}")

    negative_prompt = _expect_string_list(style["negative_prompt"], "negative_prompt")
    if negative_prompt != _NEGATIVE_PROMPT:
        raise ConceptError("negative_prompt must contain the complete canonical forbidden-token set")

    prompt_contract = _expect_mapping(style["prompt_contract"], "prompt_contract", {"positive", "negative"})
    positive = _expect_string_list(prompt_contract["positive"], "prompt_contract.positive")
    negative = _expect_string_list(prompt_contract["negative"], "prompt_contract.negative")
    if positive != _PROMPT_POSITIVE:
        raise ConceptError("prompt_contract.positive must match the canonical building prompt")
    if negative != _PROMPT_NEGATIVE:
        raise ConceptError("prompt_contract.negative must match the canonical building exclusions")

    references = style["reference_images"]
    if type(references) is not list or not references:
        raise ConceptError("reference_images must be a non-empty array")
    for index, reference in enumerate(references):
        path = f"reference_images[{index}]"
        item = _expect_mapping(reference, path, {"path", "role", "sha256"})
        _validate_catalog_path(item["path"], f"{path}.path")
        if type(item["role"]) is not str or not item["role"]:
            raise ConceptError(f"{path}.role must be a non-empty string")
        _validate_digest(item["sha256"], f"{path}.sha256")


def load_building_style(
    root: Path,
    style_path: str = "style/QS_InkToon_Building_v2.json",
) -> dict:
    resolved_root = root.resolve()
    if not resolved_root.is_dir():
        raise ConceptError("root must resolve to an existing catalog directory")
    style_file = _resolve_catalog_file(resolved_root, style_path, "style_path")
    try:
        data = json.loads(style_file.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise ConceptError(f"style_path could not be loaded as UTF-8 JSON: {error}") from error

    validate_building_style(data)
    _verify_digest(resolved_root, data["inherits"], data["inherits_sha256"], "inherits")
    for index, reference in enumerate(data["reference_images"]):
        _verify_digest(
            resolved_root,
            reference["path"],
            reference["sha256"],
            f"reference_images[{index}].path",
            f"reference_images[{index}].sha256",
        )
    return data


def _resolve_catalog_file(root: Path, stored_path: object, field_path: str) -> Path:
    relative_path = _validate_catalog_path(stored_path, field_path)
    candidate = root.joinpath(*PurePosixPath(relative_path).parts)
    resolved = candidate.resolve()
    if not resolved.is_relative_to(root):
        raise ConceptError(f"{field_path} must resolve inside the catalog root")
    if not resolved.is_file():
        raise ConceptError(f"{field_path} must name an existing regular file")
    return resolved


def _verify_digest(
    root: Path,
    stored_path: str,
    expected_digest: str,
    path_field: str,
    digest_field: str = "inherits_sha256",
) -> None:
    file_path = _resolve_catalog_file(root, stored_path, path_field)
    actual_digest = hashlib.sha256(file_path.read_bytes()).hexdigest()
    if actual_digest != expected_digest:
        raise ConceptError(f"{digest_field} does not match {path_field}")


def validate_golden_asset(asset: dict, style: dict) -> None:
    raise NotImplementedError("golden asset validation contract is not implemented")


def compile_prompt_packet(root: Path, asset_id: str, version: str) -> dict:
    raise NotImplementedError("prompt packet compilation contract is not implemented")


def canonical_json_bytes(data: dict) -> bytes:
    raise NotImplementedError("canonical JSON encoding contract is not implemented")
