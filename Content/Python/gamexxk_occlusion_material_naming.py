"""Deterministic naming and eligibility rules for Asian Village occlusion materials."""

import hashlib


DESTINATION_PATH = "/Game/GameXXK/Materials/Occlusion/AsianVillageFull"
SOURCE_PATH_PREFIX = "/Game/Asian_Village/"
EXCLUDED_PATH_PARTS = (
    "water_materials",
    "postprocess_materials",
    "sky_materials",
)


def cutout_asset_name(source_object_path: str) -> str:
    """Return the stable project-owned asset name for a source material."""
    source_object_name = source_object_path.rsplit(".", 1)[-1]
    path_hash = hashlib.md5(source_object_path.encode("utf-8")).hexdigest()[:8].upper()
    return f"MI_OC_{source_object_name}_{path_hash}"


def cutout_object_path(source_object_path: str) -> str:
    """Return the destination object path for a source material."""
    asset_name = cutout_asset_name(source_object_path)
    return f"{DESTINATION_PATH}/{asset_name}.{asset_name}"


def is_eligible_material(source_object_path: str) -> bool:
    """Return whether a source material belongs to the eligible content subtree."""
    normalized_path = source_object_path.lower()
    if not normalized_path.startswith(SOURCE_PATH_PREFIX.lower()):
        return False
    directory_segments = normalized_path.rsplit("/", 1)[0].split("/")
    return not any(part in directory_segments for part in EXCLUDED_PATH_PARTS)
