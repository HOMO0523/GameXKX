"""Read-only baseline audit for copying an approved Qingshan town terrain slice.

This script deliberately creates no Unreal assets and changes no map content.  It
loads the two approved maps only after confirming that the editor has no dirty
packages, collects a deterministic snapshot, and writes that JSON report to an
explicit absolute path supplied by the caller.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import sys
import tempfile
from pathlib import Path
from typing import Any

try:
    import unreal
except ModuleNotFoundError:
    unreal = None


SCHEMA_VERSION = 1
TOWN_MAP = "/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo"
BATTLE_MAP = "/Game/GameXXK/Maps/L_BattleScene"
ENCOUNTER_FLOOR_ID = "GameXXK_Encounter_Floor"
PROJECT_ROOT = Path(__file__).resolve().parents[2]
SLICE_WIDTH_CM = 2400.0
SLICE_DEPTH_CM = 1400.0
MAX_SLICE_HEIGHT_DELTA_CM = 160.0


def _require_unreal() -> None:
    if unreal is None:
        raise RuntimeError("UE Python is required; run this through the project UE MCP editor session.")


def _object_path(value: object) -> str:
    if value is None:
        return ""
    try:
        return str(value.get_path_name()).split(".", 1)[0]
    except Exception:
        return str(value)


def _class_name(value: object) -> str:
    try:
        return str(value.get_class().get_name())
    except Exception:
        return ""


def _class_path(value: object) -> str:
    try:
        class_object = value.get_class()
        class_name = str(class_object.get_name())
        class_path = str(class_object.get_path_name())
    except Exception:
        return ""
    if not class_path:
        return class_name
    script_path = class_path.removeprefix("/Script/")
    if (
        class_path.startswith("/Script/")
        and script_path.count(".") >= 2
        and class_name
        and class_path.endswith(f".{class_name}")
    ):
        return class_path.rsplit(".", 1)[0]
    return class_path


def _actor_label(actor: object) -> str:
    try:
        return str(actor.get_actor_label())
    except Exception:
        return ""


def _actor_tags(actor: object) -> list[str]:
    try:
        return sorted(str(tag) for tag in actor.get_editor_property("tags"))
    except Exception:
        return []


def _vector_payload(vector: object) -> dict[str, float]:
    return {axis: round(float(getattr(vector, axis)), 4) for axis in ("x", "y", "z")}


def _transform_payload(actor: object) -> dict[str, Any]:
    transform = actor.get_actor_transform()
    return {
        "translation": _vector_payload(transform.translation),
        "rotation": {
            axis: round(float(getattr(transform.rotation, axis)), 6)
            for axis in ("x", "y", "z", "w")
        },
        "scale": _vector_payload(transform.scale3d),
    }


def _bounds_payload(actor: object) -> dict[str, dict[str, float]]:
    try:
        center, extent = actor.get_actor_bounds(False)
    except Exception as exc:
        raise RuntimeError(f"could not inspect bounds for {_actor_label(actor) or actor.get_name()}: {exc}") from exc
    return {"center": _vector_payload(center), "extent": _vector_payload(extent)}


def _editor_world() -> object:
    if hasattr(unreal, "EditorLevelLibrary"):
        world = unreal.EditorLevelLibrary.get_editor_world()
        if world is not None:
            return world
    subsystem_type = getattr(unreal, "UnrealEditorSubsystem", None)
    get_subsystem = getattr(unreal, "get_editor_subsystem", None)
    subsystem = get_subsystem(subsystem_type) if subsystem_type and get_subsystem else None
    if subsystem and hasattr(subsystem, "get_editor_world"):
        world = subsystem.get_editor_world()
        if world is not None:
            return world
    raise RuntimeError("editor world is unavailable after loading the requested map")


def _current_map_path() -> str:
    world = _editor_world()
    return str(world.get_outermost().get_path_name()).split(".", 1)[0]


def _all_level_actors() -> list[object]:
    if not hasattr(unreal, "EditorLevelLibrary"):
        raise RuntimeError("EditorLevelLibrary is unavailable; cannot enumerate the active map safely")
    return list(unreal.EditorLevelLibrary.get_all_level_actors())


def _require_no_active_pie() -> None:
    subsystem_type = getattr(unreal, "UnrealEditorSubsystem", None)
    get_subsystem = getattr(unreal, "get_editor_subsystem", None)
    if subsystem_type is None:
        raise RuntimeError("UnrealEditorSubsystem API is unavailable; cannot prove that PIE is stopped")
    if get_subsystem is None:
        raise RuntimeError("unreal.get_editor_subsystem is unavailable; cannot prove that PIE is stopped")
    try:
        subsystem = get_subsystem(subsystem_type)
    except Exception as exc:
        raise RuntimeError("could not query the Unreal editor subsystem for PIE state") from exc
    if subsystem is None:
        raise RuntimeError("Unreal editor subsystem is unavailable; cannot verify that PIE is stopped")
    get_game_world = getattr(subsystem, "get_game_world", None)
    if get_game_world is None:
        raise RuntimeError("UnrealEditorSubsystem.get_game_world is unavailable; cannot prove that PIE is stopped")
    try:
        game_world = get_game_world()
    except Exception as exc:
        raise RuntimeError("could not query PIE state; refusing the terrain audit") from exc
    if game_world is not None:
        raise RuntimeError("refusing the terrain audit while PIE is active")


def _project_content_dir() -> Path:
    paths = getattr(unreal, "Paths", None)
    if paths is None:
        raise RuntimeError("UE Paths API is unavailable; cannot locate the project Content directory")
    project_content_dir = getattr(paths, "project_content_dir", None)
    make_absolute = getattr(paths, "convert_relative_path_to_full", None)
    if project_content_dir is None or make_absolute is None:
        raise RuntimeError("UE Paths API cannot resolve the project Content directory")
    content_dir = Path(str(make_absolute(project_content_dir()))).resolve()
    if not content_dir.is_dir():
        raise RuntimeError(f"verified project Content directory is missing: {content_dir}")
    return content_dir


def _package_file(package_path: str) -> Path:
    if not package_path.startswith("/Game/"):
        raise RuntimeError(f"audit only accepts project packages under /Game/: {package_path}")
    content_dir = _project_content_dir()
    filename = (content_dir / package_path.removeprefix("/Game/")).with_suffix(".umap").resolve()
    if content_dir not in filename.parents or not filename.is_file():
        raise RuntimeError(f"verified map package file is missing: {filename}")
    return filename


def sha256_file(filename: Path) -> str:
    digest = hashlib.sha256()
    with filename.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _package_records(directory: Path, content_dir: Path) -> list[dict[str, str]]:
    if not directory.is_dir():
        return []
    resolved_directory = directory.resolve()
    records: list[dict[str, str]] = []
    files = [filename for filename in directory.rglob("*") if filename.is_file()]
    for filename in sorted(files, key=lambda item: item.relative_to(directory).as_posix()):
        resolved = filename.resolve()
        try:
            relative_path = resolved.relative_to(resolved_directory).as_posix()
        except ValueError as exc:
            raise RuntimeError(f"terrain audit sidecar escapes its detected package root: {resolved}") from exc
        if not relative_path:
            continue
        try:
            resolved.relative_to(content_dir)
        except ValueError as exc:
            raise RuntimeError(f"terrain audit sidecar escapes project Content: {resolved}") from exc
        records.append(
            {
                "relative_path": relative_path,
                "file": str(resolved),
                "sha256": sha256_file(resolved),
            }
        )
    return records


def _map_package_baseline(package_path: str, map_file: Path) -> dict[str, object]:
    content_dir = _project_content_dir()
    resolved_map = map_file.resolve()
    try:
        resolved_map.relative_to(content_dir)
    except ValueError as exc:
        raise RuntimeError(f"map package is outside the project Content directory: {resolved_map}") from exc
    if not resolved_map.is_file():
        raise RuntimeError(f"map package file is missing: {resolved_map}")

    relative_package = Path(package_path.removeprefix("/Game/"))
    candidate_directories = [
        ("external_actor", content_dir / "__ExternalActors__" / relative_package),
        ("sidecar", content_dir / "__ExternalObjects__" / relative_package),
        ("sidecar", resolved_map.parent / f"{resolved_map.stem}_ExternalActors"),
        ("sidecar", resolved_map.parent / f"{resolved_map.stem}_ExternalObjects"),
        ("sidecar", resolved_map.parent / f"{resolved_map.stem}_ExternalData"),
    ]
    external_actor_packages: list[dict[str, str]] = []
    sidecar_packages: list[dict[str, str]] = []
    seen_directories: set[Path] = set()
    for package_kind, candidate in candidate_directories:
        resolved_candidate = candidate.resolve()
        if resolved_candidate in seen_directories:
            continue
        seen_directories.add(resolved_candidate)
        records = _package_records(resolved_candidate, content_dir)
        if package_kind == "external_actor":
            external_actor_packages.extend(records)
        else:
            sidecar_packages.extend(records)

    return {
        "package": package_path,
        "umap_file": str(resolved_map),
        "umap_sha256": sha256_file(resolved_map),
        "external_actor_packages": external_actor_packages,
        "sidecar_packages": sidecar_packages,
        "hash_scope": "map .umap plus complete regular-file payloads under detected project Content external-actor and sidecar package roots",
    }


def _dirty_package_names() -> list[str]:
    utilities = getattr(unreal, "EditorLoadingAndSavingUtils", None)
    if utilities is None:
        raise RuntimeError("EditorLoadingAndSavingUtils is unavailable; cannot prove the editor is clean")

    dirty: list[str] = []
    for method_name in ("get_dirty_map_packages", "get_dirty_content_packages"):
        method = getattr(utilities, method_name, None)
        if method is None:
            raise RuntimeError(f"UE 5.8 API {method_name} is unavailable; refusing to switch maps")
        for package in method():
            dirty.append(_object_path(package))
    return sorted(set(path for path in dirty if path))


def _require_clean_editor() -> None:
    dirty = _dirty_package_names()
    if dirty:
        raise RuntimeError(
            "refusing the terrain audit while packages are dirty; preserve the user's work first: "
            + ", ".join(dirty)
        )


def _require_audit_session_safe() -> None:
    _require_no_active_pie()
    _require_clean_editor()


def _capture_original_map() -> str:
    _require_audit_session_safe()
    original_map = _current_map_path()
    if not original_map.startswith("/Game/"):
        commandlet_mode = os.environ.get("GAMEXXK_BATTLE_TERRAIN_AUDIT_COMMANDLET", "").strip() == "1"
        if commandlet_mode and original_map.startswith("/Temp/"):
            # An isolated PythonScriptCommandlet starts from Untitled_0 instead
            # of the user's editor map.  It has no user map to restore, so use
            # the audited battle map as the explicit restoration anchor.
            original_map = BATTLE_MAP
        else:
            raise RuntimeError(f"current editor map is not a supported project map: {original_map}")
    asset_library = getattr(unreal, "EditorAssetLibrary", None)
    does_asset_exist = getattr(asset_library, "does_asset_exist", None)
    if does_asset_exist is None:
        raise RuntimeError("EditorAssetLibrary.does_asset_exist is unavailable; cannot validate the original map")
    if not does_asset_exist(original_map):
        raise RuntimeError(f"current editor map asset is missing: {original_map}")
    return original_map


def _load_map_for_reading(map_path: str) -> object:
    _require_audit_session_safe()
    asset_library = getattr(unreal, "EditorAssetLibrary", None)
    does_asset_exist = getattr(asset_library, "does_asset_exist", None)
    if does_asset_exist is None:
        raise RuntimeError("EditorAssetLibrary.does_asset_exist is unavailable; cannot validate the requested map")
    if not does_asset_exist(map_path):
        raise RuntimeError(f"required map asset is missing: {map_path}")
    loading_utilities = getattr(unreal, "EditorLoadingAndSavingUtils", None)
    loader = getattr(loading_utilities, "load_map", None)
    if loader is None:
        raise RuntimeError("UE 5.8 API EditorLoadingAndSavingUtils.load_map is unavailable")
    if _current_map_path() != map_path and not loader(map_path):
        raise RuntimeError(f"could not load map for terrain audit: {map_path}")
    if _current_map_path() != map_path:
        raise RuntimeError(f"loaded map mismatch: expected {map_path}, got {_current_map_path()}")
    _require_audit_session_safe()
    return _editor_world()


def _restore_original_map(original_map: str) -> None:
    _require_no_active_pie()
    dirty_packages = _dirty_package_names()
    if dirty_packages:
        raise RuntimeError(
            "audit detected unexpected dirty packages before restoring the original map; "
            "leaving the active map unchanged to preserve them: "
            + ", ".join(dirty_packages)
        )
    if _current_map_path() != original_map:
        _load_map_for_reading(original_map)
    if _current_map_path() != original_map:
        raise RuntimeError(f"could not restore the original editor map: {original_map}")
    _require_no_active_pie()
    dirty_packages = _dirty_package_names()
    if dirty_packages:
        raise RuntimeError(
            "audit detected dirty packages after restoring the original map; "
            "leaving the active map unchanged to preserve them: "
            + ", ".join(dirty_packages)
        )


def _component_resources(component: object) -> dict[str, object]:
    resources: dict[str, object] = {}
    try:
        mesh = component.get_editor_property("static_mesh")
    except Exception:
        mesh = None
    mesh_path = _object_path(mesh)
    if mesh_path:
        resources["static_mesh"] = mesh_path

    material_paths: list[str] = []
    try:
        material_count = int(component.get_num_materials())
        material_paths = [_object_path(component.get_material(index)) for index in range(material_count)]
    except Exception:
        pass
    if material_paths:
        resources["materials"] = material_paths

    for property_name in ("sprite", "flipbook", "skeletal_mesh", "texture"):
        try:
            path = _object_path(component.get_editor_property(property_name))
        except Exception:
            path = ""
        if path:
            resources[property_name] = path
    return resources


def _component_snapshot(actor: object) -> list[dict[str, object]]:
    component_class = getattr(unreal, "ActorComponent", None)
    if component_class is None:
        raise RuntimeError("UE 5.8 API ActorComponent is unavailable; cannot create a component baseline")
    components: list[dict[str, object]] = []
    for component in actor.get_components_by_class(component_class):
        components.append(
            {
                "name": str(component.get_name()),
                "class": _class_path(component),
                "resources": _component_resources(component),
            }
        )
    return sorted(components, key=lambda item: (str(item["class"]), str(item["name"])))


def _actor_snapshot(actor: object) -> dict[str, object]:
    return {
        "name": str(actor.get_name()),
        "label": _actor_label(actor),
        "class": _class_path(actor),
        "tags": _actor_tags(actor),
        "transform": _transform_payload(actor),
        "components": _component_snapshot(actor),
    }


def _is_landscape(actor: object) -> bool:
    return "landscape" in _class_name(actor).lower() or "landscape" in _class_path(actor).lower()


def landscape_snapshot() -> list[dict[str, object]]:
    landscapes: list[dict[str, object]] = []
    for actor in _all_level_actors():
        if not _is_landscape(actor):
            continue
        record = _actor_snapshot(actor)
        record["bounds"] = _bounds_payload(actor)
        landscapes.append(record)
    if not landscapes:
        raise RuntimeError("no Landscape actor was found in the approved Qingshan town map")
    return sorted(landscapes, key=lambda item: (str(item["label"]), str(item["name"])))


def _slice_bounds(center: dict[str, float]) -> dict[str, dict[str, float]]:
    half_width = SLICE_WIDTH_CM * 0.5
    half_depth = SLICE_DEPTH_CM * 0.5
    x = float(center["x"])
    y = float(center["y"])
    return {
        "min": {"x": x - half_width, "y": y - half_depth},
        "max": {"x": x + half_width, "y": y + half_depth},
    }


def _slice_is_inside_landscape(
    slice_bounds: dict[str, dict[str, float]],
    landscape_bounds: dict[str, dict[str, float]],
) -> bool:
    center = landscape_bounds["center"]
    extent = landscape_bounds["extent"]
    return (
        float(slice_bounds["min"]["x"]) >= float(center["x"]) - float(extent["x"])
        and float(slice_bounds["max"]["x"]) <= float(center["x"]) + float(extent["x"])
        and float(slice_bounds["min"]["y"]) >= float(center["y"]) - float(extent["y"])
        and float(slice_bounds["max"]["y"]) <= float(center["y"]) + float(extent["y"])
    )


def _sample_slice_height_delta(
    slice_bounds: dict[str, dict[str, float]],
    sample_height: Any,
) -> float:
    samples: list[float] = []
    for x_fraction in (0.0, 0.5, 1.0):
        for y_fraction in (0.0, 0.5, 1.0):
            x = float(slice_bounds["min"]["x"]) + (
                float(slice_bounds["max"]["x"]) - float(slice_bounds["min"]["x"])
            ) * x_fraction
            y = float(slice_bounds["min"]["y"]) + (
                float(slice_bounds["max"]["y"]) - float(slice_bounds["min"]["y"])
            ) * y_fraction
            height = sample_height(x, y)
            if height is None:
                raise RuntimeError("candidate height sampling returned no landscape surface")
            samples.append(float(height))
    return max(samples) - min(samples)


def select_candidate_slice(
    landscapes: list[dict[str, object]],
    candidate_centers: list[dict[str, float]],
    sample_height: Any,
    overlaps_forbidden: Any,
) -> dict[str, object]:
    """Select one local, flat Landscape slice without touching the editor state.

    The caller supplies the engine-specific surface sampler and overlap query so
    this safety contract is executable both in UE and in static unit tests.
    """
    if not landscapes:
        raise RuntimeError("no Landscape actor was found in the approved Qingshan town map")
    if not candidate_centers:
        raise RuntimeError("no candidate centers were supplied for the town terrain slice")

    for landscape in landscapes:
        landscape_bounds = landscape.get("bounds")
        if not isinstance(landscape_bounds, dict):
            continue
        source_landscape = str(landscape.get("name") or landscape.get("label") or "")
        if not source_landscape:
            continue
        for center in candidate_centers:
            if "x" not in center or "y" not in center:
                continue
            bounds = _slice_bounds(center)
            if not _slice_is_inside_landscape(bounds, landscape_bounds):
                continue
            if bool(overlaps_forbidden(bounds)):
                continue
            height_delta = _sample_slice_height_delta(bounds, sample_height)
            if height_delta > MAX_SLICE_HEIGHT_DELTA_CM:
                continue
            return {
                "source_landscape": source_landscape,
                "center_cm": {"x": float(center["x"]), "y": float(center["y"])},
                "size_cm": {"x": SLICE_WIDTH_CM, "y": SLICE_DEPTH_CM},
                "world_bounds_cm": bounds,
                "max_height_delta_cm": height_delta,
                "overlaps_forbidden": False,
            }

    raise RuntimeError(
        "no safe 2400.0 by 1400.0 cm Landscape candidate met the flatness and exclusion requirements"
    )


def _floor_label_matches(actor: object) -> bool:
    return _actor_label(actor) == ENCOUNTER_FLOOR_ID


def _is_static_mesh_actor(actor: object) -> bool:
    return _class_name(actor) == "StaticMeshActor"


def find_single_encounter_floor() -> object:
    matches = [actor for actor in _all_level_actors() if _floor_label_matches(actor)]
    if len(matches) != 1:
        raise RuntimeError(
            f"expected exactly one protected encounter floor Actor Label {ENCOUNTER_FLOOR_ID}, got {len(matches)}"
        )
    floor = matches[0]
    if not _is_static_mesh_actor(floor):
        raise RuntimeError(
            f"protected encounter floor {ENCOUNTER_FLOOR_ID} must be an exact StaticMeshActor, got {_class_path(floor)}"
        )
    return floor


def encounter_floor_snapshot() -> dict[str, object]:
    floor = find_single_encounter_floor()
    record = _actor_snapshot(floor)
    static_components = [
        component
        for component in record["components"]
        if str(component["class"]).rsplit(".", 1)[-1] == "StaticMeshComponent"
    ]
    if len(static_components) != 1:
        raise RuntimeError(
            f"protected encounter floor must expose exactly one StaticMeshComponent, got {len(static_components)}"
        )
    resources = static_components[0]["resources"]
    record["mesh"] = str(resources.get("static_mesh", ""))
    materials = resources.get("materials", [])
    record["material"] = str(materials[0]) if materials else ""
    return record


def _protection_reasons(actor: object) -> list[str]:
    label = _actor_label(actor).lower()
    class_path = _class_path(actor).lower()
    class_name = _class_name(actor).lower()
    tags = [tag.lower() for tag in _actor_tags(actor)]
    signals = [label, class_path, class_name, *tags]
    reasons: list[str] = []
    if _floor_label_matches(actor):
        reasons.append("encounter_floor")
    if "camera" in label or "camera" in class_path or "camera" in class_name:
        reasons.append("camera")
    if "presenter" in label or "presenter" in class_path or "presenter" in class_name:
        reasons.append("presenter")
    if "playerstart" in class_path or "playerstart" in class_name or "player_start" in label:
        reasons.append("player_start")
    if "light" in class_path or "light" in class_name or "light" in label:
        reasons.append("light")
    if any("unit" in signal or "combatant" in signal or "enemy" in signal for signal in signals):
        reasons.append("unit")
    if label.startswith("gamexxk_") or any(tag.startswith("gamexxk_") for tag in tags):
        reasons.append("gamexxk_actor")
    return sorted(set(reasons))


def protected_battle_actor_snapshot() -> dict[str, object]:
    protected: list[dict[str, object]] = []
    categories: dict[str, list[str]] = {
        "encounter_floor": [],
        "camera": [],
        "presenter": [],
        "player_start": [],
        "light": [],
        "unit": [],
        "gamexxk_actor": [],
    }
    for actor in _all_level_actors():
        reasons = _protection_reasons(actor)
        if not reasons:
            continue
        record = _actor_snapshot(actor)
        record["protection_reasons"] = reasons
        protected.append(record)
        identifier = str(record["label"] or record["name"])
        for reason in reasons:
            categories[reason].append(identifier)
    for names in categories.values():
        names.sort()
    return {
        "actors": sorted(protected, key=lambda item: (str(item["label"]), str(item["name"]))),
        "categories": categories,
    }


def _project_saved_dir() -> Path:
    return (PROJECT_ROOT / "Saved").resolve()


def _output_under_project_saved(output: Path) -> Path:
    if not output.is_absolute():
        raise ValueError("--output must be an absolute path")
    resolved_output = output.resolve()
    saved_dir = _project_saved_dir()
    try:
        resolved_output.relative_to(saved_dir)
    except ValueError as exc:
        raise ValueError(f"--output must be inside the project Saved directory: {saved_dir}") from exc
    if resolved_output == saved_dir:
        raise ValueError("--output must name a JSON file below the project Saved directory")
    return resolved_output


def _parse_args(argv: list[str] | None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    environment_output = os.environ.get("GAMEXXK_BATTLE_TERRAIN_AUDIT_OUTPUT", "").strip()
    parser.add_argument(
        "--output",
        required=False,
        default=Path(environment_output) if environment_output else None,
        type=Path,
        help=(
            "Absolute JSON report path, normally under <Project>/Saved/. "
            "When running through the UE commandlet, GAMEXXK_BATTLE_TERRAIN_AUDIT_OUTPUT "
            "avoids splitting a path that contains spaces."
        ),
    )
    args = parser.parse_args(argv)
    if args.output is None:
        parser.error("--output or GAMEXXK_BATTLE_TERRAIN_AUDIT_OUTPUT is required")
    try:
        args.output = _output_under_project_saved(args.output)
    except ValueError as exc:
        parser.error(str(exc))
    return args


def _write_report(output: Path, snapshot: dict[str, object]) -> None:
    safe_output = _output_under_project_saved(output)
    if safe_output.exists():
        raise RuntimeError(f"refusing to overwrite an existing terrain audit snapshot: {safe_output}")
    safe_output.parent.mkdir(parents=True, exist_ok=True)
    if safe_output.exists():
        raise RuntimeError(f"refusing to overwrite an existing terrain audit snapshot: {safe_output}")

    descriptor, temporary_name = tempfile.mkstemp(
        prefix=f".{safe_output.stem}.",
        suffix=".tmp",
        dir=str(safe_output.parent),
    )
    temporary_output = Path(temporary_name)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8", newline="\n") as handle:
            json.dump(snapshot, handle, ensure_ascii=False, indent=2, sort_keys=True)
            handle.write("\n")
            handle.flush()
            os.fsync(handle.fileno())
        if safe_output.exists():
            raise RuntimeError(f"refusing to overwrite an existing terrain audit snapshot: {safe_output}")
        os.replace(temporary_output, safe_output)
    finally:
        if temporary_output.exists():
            temporary_output.unlink()


def _collect_audit_snapshot(original_map: str) -> dict[str, object]:
    map_files = {TOWN_MAP: _package_file(TOWN_MAP), BATTLE_MAP: _package_file(BATTLE_MAP)}
    baselines_before = {
        map_path: _map_package_baseline(map_path, filename)
        for map_path, filename in map_files.items()
    }

    _load_map_for_reading(TOWN_MAP)
    landscapes = landscape_snapshot()

    _load_map_for_reading(BATTLE_MAP)
    encounter_floor = encounter_floor_snapshot()
    protected_actors = protected_battle_actor_snapshot()

    baselines_after = {
        map_path: _map_package_baseline(map_path, filename)
        for map_path, filename in map_files.items()
    }
    if baselines_before != baselines_after:
        raise RuntimeError("map package or detected sidecar hash changed during a read-only audit")

    return {
        "schema": SCHEMA_VERSION,
        "town_map": TOWN_MAP,
        "battle_map": BATTLE_MAP,
        "town_package_sha256": str(baselines_before[TOWN_MAP]["umap_sha256"]),
        "battle_package_sha256": str(baselines_before[BATTLE_MAP]["umap_sha256"]),
        "source_map_package_baseline": baselines_before[TOWN_MAP],
        "target_map_package_baseline": baselines_before[BATTLE_MAP],
        "landscapes": landscapes,
        "protected_battle_actors": protected_actors,
        "encounter_floor": encounter_floor,
        "audit_package_baselines_after": baselines_after,
        "audit_session": {"original_map": original_map, "restored_map": ""},
    }


def main(argv: list[str] | None = None) -> dict[str, object]:
    """Create the before snapshot without changing any Unreal package content."""
    _require_unreal()
    args = _parse_args(argv)
    _require_audit_session_safe()
    original_map = _capture_original_map()
    try:
        snapshot = _collect_audit_snapshot(original_map)
    finally:
        _restore_original_map(original_map)

    _require_audit_session_safe()
    snapshot["audit_session"]["restored_map"] = _current_map_path()
    _write_report(args.output, snapshot)
    unreal.log("[GameXXK][BattleTownTerrainAudit] " + json.dumps(snapshot, ensure_ascii=False))
    return snapshot


if __name__ == "__main__":
    print(json.dumps(main(sys.argv[1:]), ensure_ascii=False, indent=2, sort_keys=True))
