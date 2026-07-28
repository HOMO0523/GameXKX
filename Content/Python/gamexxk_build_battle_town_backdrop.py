"""Build an isolated battle-only backdrop from the live Qingshan town map.

The live town is source-only. This module is intentionally safe to import
outside Unreal so its contract can be verified by regular Python unit tests.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from collections import Counter
from pathlib import Path, PurePosixPath

try:
    import unreal
except ModuleNotFoundError:
    unreal = None


SOURCE_MAP = "/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo"
LEGACY_BATTLE_MAP = "/Game/GameXXK/Maps/L_BattleScene"
TARGET_BATTLE_MAP = "/Game/GameXXK/Maps/L_BattleTown"
KEEP_MESH_PREFIXES = (
    "/Game/Asian_Village/meshes/trees/",
    "/Game/Asian_Village/meshes/plants/",
    "/Game/Asian_Village/meshes/cliff/",
)

KEEP_ACTOR_CLASSES = ("Landscape", "LandscapeStreamingProxy")
PRESENTER_LABEL = "GameXXK_BattleScene_Presenter"
CAMERA_LABEL = "GameXXK_BattleScene_Camera"
PLAYER_START_LABEL = "GameXXK_Encounter_PlayerStart"
LIGHT_LABEL = "GameXXK_Encounter_Light"
BATTLE_ANCHOR_XY = (20400.0, 4580.0)
BATTLE_PRESENTER_OFFSET = (0.0, 0.0, 0.0)
BATTLE_CAMERA_OFFSET = (-420.0, 0.0, 720.0)
BATTLE_CAMERA_ROTATION = (-60.0, 0.0, 0.0)
BATTLE_CAMERA_FOV = 63.0
BATTLE_PLAYER_START_OFFSET = (220.0, 0.0, 80.0)
BATTLE_LIGHT_OFFSET = (-300.0, -300.0, 500.0)
BATTLE_LIGHT_ROTATION = (-45.0, -35.0, 0.0)
PROJECT_ROOT = Path(__file__).resolve().parents[2]
CONTENT_DIR = PROJECT_ROOT / "Content"
MANIFEST_PATH = PROJECT_ROOT / "Saved" / "HarnessReports" / "battle-town-backdrop-manifest.json"
AUDIT_PATH = PROJECT_ROOT / "Saved" / "HarnessReports" / "battle-town-backdrop-audit.json"


def should_keep_mesh_path(mesh_path: str) -> bool:
    """Return whether a mesh belongs to the town's allowed natural set."""
    package_path = str(mesh_path or "").split(".", 1)[0]
    return package_path.startswith(KEEP_MESH_PREFIXES)


def should_keep_actor_class(class_name: str) -> bool:
    """Landscape is the only actor class retained without a mesh-path check."""
    return str(class_name or "") in KEEP_ACTOR_CLASSES


def classify_actor_for_cleanup(class_name: str, mesh_paths: tuple[str, ...]) -> str:
    """Classify a town actor without mutating it.

    Foliage is intentionally all-or-nothing: Python cannot safely remove one
    foliage component while preserving its `FoliageInfos` bookkeeping.
    """
    normalized_class = str(class_name or "")
    normalized_meshes = tuple(str(path or "").split(".", 1)[0] for path in mesh_paths)
    if should_keep_actor_class(normalized_class):
        return "keep_landscape"
    if normalized_class == "InstancedFoliageActor":
        if normalized_meshes and all(should_keep_mesh_path(path) for path in normalized_meshes):
            return "keep_foliage"
        if any(should_keep_mesh_path(path) for path in normalized_meshes):
            return "reject_mixed_foliage"
        return "delete"
    if normalized_class == "StaticMeshActor" and normalized_meshes and all(
        should_keep_mesh_path(path) for path in normalized_meshes
    ):
        return "keep_mesh"
    return "delete"


def summarize_cleanup_records(records: list[dict[str, object]]) -> dict[str, object]:
    """Summarize an inventory before any actor destruction is permitted."""
    actions: Counter[str] = Counter()
    mixed_foliage_labels: list[str] = []
    for record in records:
        action = classify_actor_for_cleanup(
            str(record.get("class_name", "")),
            tuple(str(path) for path in record.get("mesh_paths", ())),
        )
        actions[action] += 1
        if action == "reject_mixed_foliage":
            mixed_foliage_labels.append(str(record.get("label", "")))
    return {
        "actor_count": len(records),
        "actions": dict(sorted(actions.items())),
        "mixed_foliage_labels": sorted(label for label in mixed_foliage_labels if label),
    }


def select_delete_records(records: list[dict[str, object]], limit: int) -> list[dict[str, object]]:
    """Pick a stable bounded deletion batch after the full inventory is safe."""
    if limit <= 0:
        raise ValueError("cleanup batch limit must be positive")
    summary = summarize_cleanup_records(records)
    if summary["mixed_foliage_labels"]:
        raise RuntimeError("mixed foliage prevents every deletion until resolved safely")
    candidates = [
        record
        for record in records
        if classify_actor_for_cleanup(
            str(record.get("class_name", "")),
            tuple(str(path) for path in record.get("mesh_paths", ())),
        )
        == "delete"
    ]
    candidates.sort(
        key=lambda record: (
            str(record.get("label", "")),
            str(record.get("class_name", "")),
            tuple(str(path) for path in record.get("mesh_paths", ())),
        )
    )
    return candidates[:limit]


def final_scaffold_classes() -> dict[str, str]:
    return {
        PRESENTER_LABEL: "GameXXKBattleScenePresenter",
        CAMERA_LABEL: "CameraActor",
        PLAYER_START_LABEL: "PlayerStart",
        LIGHT_LABEL: "DirectionalLight",
    }


def validate_final_battle_town_records(records: list[dict[str, object]]) -> dict[str, object]:
    """Accept only natural town content plus one exact instance of each battle actor."""
    expected_scaffold = final_scaffold_classes()
    scaffold_counts: Counter[str] = Counter()
    unexpected_labels: list[str] = []
    for record in records:
        label = str(record.get("label", ""))
        class_name = str(record.get("class_name", ""))
        if label in expected_scaffold:
            if class_name == expected_scaffold[label]:
                scaffold_counts[label] += 1
                continue
            unexpected_labels.append(label)
            continue
        action = classify_actor_for_cleanup(
            class_name,
            tuple(str(path) for path in record.get("mesh_paths", ())),
        )
        if action not in {"keep_landscape", "keep_foliage", "keep_mesh"}:
            unexpected_labels.append(label)
    missing_or_duplicate = sorted(
        label for label in expected_scaffold if scaffold_counts[label] != 1
    )
    return {
        "ok": not unexpected_labels and not missing_or_duplicate,
        "unexpected_labels": sorted(label for label in unexpected_labels if label),
        "missing_or_duplicate_scaffold_labels": missing_or_duplicate,
        "scaffold_counts": dict(sorted(scaffold_counts.items())),
    }


def map_file_for_package(package_path: str) -> Path:
    """Resolve a project map package without allowing filesystem escape."""
    if not str(package_path or "").startswith("/Game/"):
        raise ValueError(f"only project map packages are allowed: {package_path}")
    filename = (CONTENT_DIR / str(package_path).removeprefix("/Game/")).with_suffix(".umap").resolve()
    try:
        filename.relative_to(CONTENT_DIR.resolve())
    except ValueError as exc:
        raise ValueError(f"map package escapes project Content: {package_path}") from exc
    return filename


def sha256_file(filename: Path) -> str:
    if not filename.is_file():
        raise FileNotFoundError(f"map package file is missing: {filename}")
    digest = hashlib.sha256()
    with filename.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def map_hash_baseline() -> dict[str, str]:
    """Capture immutable source and legacy-map evidence before any mutation."""
    source_file = map_file_for_package(SOURCE_MAP)
    legacy_file = map_file_for_package(LEGACY_BATTLE_MAP)
    return {
        "source_map": SOURCE_MAP,
        "source_file": str(source_file),
        "source_sha256": sha256_file(source_file),
        "legacy_battle_map": LEGACY_BATTLE_MAP,
        "legacy_battle_file": str(legacy_file),
        "legacy_battle_sha256": sha256_file(legacy_file),
    }


def build_duplicate_manifest(
    before: dict[str, str], after: dict[str, str], target_sha256: str, stage: str = "duplicate_only"
) -> dict[str, str | int]:
    """Prove the protected source and legacy battle map stayed byte-identical."""
    if before.get("source_sha256") != after.get("source_sha256"):
        raise RuntimeError("protected town source map hash changed during duplicate")
    if before.get("legacy_battle_sha256") != after.get("legacy_battle_sha256"):
        raise RuntimeError("protected legacy battle map hash changed during duplicate")
    return {
        "schema_version": 1,
        "stage": stage,
        "source_map": str(before.get("source_map", "")),
        "source_sha256": str(before.get("source_sha256", "")),
        "legacy_battle_map": str(before.get("legacy_battle_map", "")),
        "legacy_battle_sha256": str(before.get("legacy_battle_sha256", "")),
        "target_map": TARGET_BATTLE_MAP,
        "target_sha256": str(target_sha256),
    }


def scaffold_plan() -> dict[str, object]:
    """Describe the battle-only actors that may be added after cleanup."""
    return {
        "map": TARGET_BATTLE_MAP,
        "actors": (
            PRESENTER_LABEL,
            CAMERA_LABEL,
            PLAYER_START_LABEL,
            LIGHT_LABEL,
        ),
    }


def battle_scaffold_spec() -> dict[str, object]:
    """Describe the battle objects relative to a verified town-ground anchor."""
    return {
        "map": TARGET_BATTLE_MAP,
        "anchor_xy": BATTLE_ANCHOR_XY,
        "presenter": {"label": PRESENTER_LABEL, "offset": BATTLE_PRESENTER_OFFSET},
        "camera": {
            "label": CAMERA_LABEL,
            "offset": BATTLE_CAMERA_OFFSET,
            "rotation": BATTLE_CAMERA_ROTATION,
            "fov": BATTLE_CAMERA_FOV,
        },
        "player_start": {"label": PLAYER_START_LABEL, "offset": BATTLE_PLAYER_START_OFFSET},
        "light": {"label": LIGHT_LABEL, "offset": BATTLE_LIGHT_OFFSET, "rotation": BATTLE_LIGHT_ROTATION},
    }


def battle_scaffold_locations_for_ground(ground_z: float) -> dict[str, tuple[float, float, float]]:
    """Place the battle presentation above the copied town's collision ground."""
    anchor_x, anchor_y = BATTLE_ANCHOR_XY

    def at(offset: tuple[float, float, float]) -> tuple[float, float, float]:
        return (
            float(anchor_x + offset[0]),
            float(anchor_y + offset[1]),
            float(ground_z + offset[2]),
        )

    return {
        "presenter": at(BATTLE_PRESENTER_OFFSET),
        "camera": at(BATTLE_CAMERA_OFFSET),
        "player_start": at(BATTLE_PLAYER_START_OFFSET),
        "light": at(BATTLE_LIGHT_OFFSET),
    }


def _require_unreal() -> None:
    if unreal is None:
        raise RuntimeError("UE Python is required; run this only through the open interactive editor")


def _short_map_name(package_path: str) -> str:
    name = PurePosixPath(str(package_path or "")).name
    if name.startswith("UEDPIE_"):
        separator = name.find("_", len("UEDPIE_"))
        if separator >= 0:
            return name[separator + 1 :]
    return name


def package_path_from_object_path(object_path: str) -> str:
    """Strip an Unreal object suffix while preserving a package path."""
    return str(object_path or "").split(".", 1)[0]


def is_target_battle_map(package_path: str) -> bool:
    """Accept the exact authored map and its PIE package-name variant only."""
    value = package_path_from_object_path(package_path)
    return value == TARGET_BATTLE_MAP or _short_map_name(value) == _short_map_name(TARGET_BATTLE_MAP)


def _require_no_active_pie() -> None:
    subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if subsystem is None or not hasattr(subsystem, "get_game_world"):
        raise RuntimeError("cannot prove PIE is stopped before duplicating the battle map")
    if subsystem.get_game_world() is not None:
        raise RuntimeError("refusing to duplicate the battle map while PIE is active")


def _dirty_package_names() -> list[str]:
    package_names: list[str] = []
    utilities = unreal.EditorLoadingAndSavingUtils
    for method_name in ("get_dirty_map_packages", "get_dirty_content_packages"):
        method = getattr(utilities, method_name, None)
        if method is None:
            continue
        for package in method() or []:
            try:
                package_names.append(str(package.get_path_name()))
            except Exception:
                package_names.append(str(package))
    return sorted(set(package_names))


def _require_clean_editor() -> None:
    dirty_packages = _dirty_package_names()
    if dirty_packages:
        raise RuntimeError("refusing to duplicate with dirty packages: " + ", ".join(dirty_packages))


def _require_no_unrelated_dirty_packages() -> None:
    allowed = {TARGET_BATTLE_MAP}
    unexpected = [name for name in _dirty_package_names() if name not in allowed]
    if unexpected:
        raise RuntimeError("refusing recovery with unrelated dirty packages: " + ", ".join(unexpected))


def _current_editor_map_path() -> str:
    world = unreal.EditorLevelLibrary.get_editor_world()
    if world is None:
        raise RuntimeError("editor world is unavailable")
    outermost = world.get_outermost()
    if outermost is None:
        raise RuntimeError("editor world has no map package")
    return package_path_from_object_path(str(outermost.get_path_name()))


def _asset_package_path(asset: object) -> str:
    if asset is None:
        return ""
    try:
        return package_path_from_object_path(str(asset.get_path_name()))
    except Exception:
        return ""


def _actor_class_name(actor: object) -> str:
    try:
        return str(actor.get_class().get_name())
    except Exception:
        return ""


def _actor_label(actor: object) -> str:
    try:
        return str(actor.get_actor_label())
    except Exception:
        try:
            return str(actor.get_name())
        except Exception:
            return ""


def _static_mesh_paths_for_actor(actor: object, class_name: str) -> tuple[str, ...]:
    components: list[object] = []
    if class_name == "StaticMeshActor":
        try:
            component = actor.get_editor_property("static_mesh_component")
        except Exception:
            component = None
        if component is not None:
            components.append(component)
    elif class_name == "InstancedFoliageActor":
        try:
            components.extend(actor.get_components_by_class(unreal.StaticMeshComponent) or [])
        except Exception:
            components = []

    mesh_paths: list[str] = []
    for component in components:
        try:
            mesh = component.get_editor_property("static_mesh")
        except Exception:
            mesh = None
        mesh_path = _asset_package_path(mesh)
        if mesh_path:
            mesh_paths.append(mesh_path)
    return tuple(sorted(set(mesh_paths)))


def _actor_cleanup_record(actor: object) -> dict[str, object]:
    class_name = _actor_class_name(actor)
    return {
        "label": _actor_label(actor),
        "class_name": class_name,
        "mesh_paths": _static_mesh_paths_for_actor(actor, class_name),
    }


def _current_target_cleanup_records() -> list[dict[str, object]]:
    _require_unreal()
    _require_no_active_pie()
    current_map = _current_editor_map_path()
    if not is_target_battle_map(current_map):
        raise RuntimeError(
            f"refusing actor inspection because the visible editor map is {current_map}, not {TARGET_BATTLE_MAP}"
        )
    return [_actor_cleanup_record(actor) for actor in unreal.EditorLevelLibrary.get_all_level_actors()]


def audit_current_map() -> dict[str, object]:
    """Read-only cleanup audit for the already visible target map."""
    records = _current_target_cleanup_records()
    summary = summarize_cleanup_records(records)
    class_counts = Counter(str(record["class_name"]) for record in records)
    delete_labels = [
        str(record["label"])
        for record in records
        if classify_actor_for_cleanup(
            str(record["class_name"]), tuple(str(path) for path in record["mesh_paths"])
        )
        == "delete"
    ]
    target_file = map_file_for_package(TARGET_BATTLE_MAP)
    baseline = map_hash_baseline()
    report: dict[str, object] = {
        "stage": "audit_current_map",
        "ok": not bool(summary["mixed_foliage_labels"]),
        "current_map": _current_editor_map_path(),
        "target_sha256": sha256_file(target_file),
        "source_sha256": baseline["source_sha256"],
        "legacy_battle_sha256": baseline["legacy_battle_sha256"],
        "summary": summary,
        "class_counts": dict(sorted(class_counts.items())),
        "delete_sample_labels": delete_labels[:40],
    }
    if summary["mixed_foliage_labels"]:
        report["error"] = "mixed foliage actor components require a C++ editor utility; no actors were changed"
    AUDIT_PATH.parent.mkdir(parents=True, exist_ok=True)
    AUDIT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return report


def cleanup_current_map(limit: int) -> dict[str, object]:
    """Delete one bounded batch from the visible target map and save only it."""
    if limit <= 0 or limit > 50:
        raise ValueError("cleanup batch limit must be between 1 and 50")
    _require_unreal()
    _require_no_active_pie()

    current_map = _current_editor_map_path()
    if not is_target_battle_map(current_map):
        raise RuntimeError(
            f"refusing cleanup because the visible editor map is {current_map}, not {TARGET_BATTLE_MAP}"
        )

    actors = list(unreal.EditorLevelLibrary.get_all_level_actors())
    pairs = [(actor, _actor_cleanup_record(actor)) for actor in actors]
    records = [record for _, record in pairs]
    selected_records = select_delete_records(records, limit)
    selected_ids = {id(record) for record in selected_records}
    selected_pairs = [(actor, record) for actor, record in pairs if id(record) in selected_ids]
    if len(selected_pairs) != len(selected_records):
        raise RuntimeError("cleanup selection lost actor ownership; no deletion was attempted")

    before = map_hash_baseline()
    target_file = map_file_for_package(TARGET_BATTLE_MAP)
    target_sha_before = sha256_file(target_file)
    removed_labels: list[str] = []
    for actor, record in selected_pairs:
        if not unreal.EditorLevelLibrary.destroy_actor(actor):
            raise RuntimeError(f"could not delete selected town actor: {record['label']}")
        removed_labels.append(str(record["label"]))

    if selected_pairs and not unreal.EditorLoadingAndSavingUtils.save_current_level():
        raise RuntimeError(f"could not save cleaned battle map: {TARGET_BATTLE_MAP}")

    after = map_hash_baseline()
    if before["source_sha256"] != after["source_sha256"]:
        raise RuntimeError("protected town source map hash changed during cleanup")
    if before["legacy_battle_sha256"] != after["legacy_battle_sha256"]:
        raise RuntimeError("protected legacy battle map hash changed during cleanup")

    summary_before = summarize_cleanup_records(records)
    report: dict[str, object] = {
        "schema_version": 1,
        "stage": "cleanup_current_map",
        "current_map": current_map,
        "target_map": TARGET_BATTLE_MAP,
        "target_sha256_before": target_sha_before,
        "target_sha256_after": sha256_file(target_file),
        "source_sha256": after["source_sha256"],
        "legacy_battle_sha256": after["legacy_battle_sha256"],
        "removed_count": len(removed_labels),
        "removed_labels": removed_labels,
        "remaining_delete_count": int(summary_before["actions"].get("delete", 0)) - len(removed_labels),
        "mixed_foliage_labels": summary_before["mixed_foliage_labels"],
    }
    MANIFEST_PATH.parent.mkdir(parents=True, exist_ok=True)
    MANIFEST_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return report


def _vector(values: tuple[float, float, float]) -> object:
    return unreal.Vector(float(values[0]), float(values[1]), float(values[2]))


def _rotator(values: tuple[float, float, float]) -> object:
    rotation = unreal.Rotator()
    rotation.pitch = float(values[0])
    rotation.yaw = float(values[1])
    rotation.roll = float(values[2])
    return rotation


def _sample_landscape_ground_z(world: object) -> float:
    anchor_x, anchor_y = BATTLE_ANCHOR_XY
    hit = unreal.SystemLibrary.line_trace_single(
        world,
        unreal.Vector(float(anchor_x), float(anchor_y), 100000.0),
        unreal.Vector(float(anchor_x), float(anchor_y), -50000.0),
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
        True,
        [],
        unreal.DrawDebugTrace.NONE,
        True,
    )
    if hit is None:
        raise RuntimeError(f"could not ground the battle scaffold at {BATTLE_ANCHOR_XY}")
    payload = hit.to_dict()
    point = payload.get("impact_point") if isinstance(payload, dict) else None
    if point is None:
        raise RuntimeError(f"battle scaffold ground trace returned no impact point at {BATTLE_ANCHOR_XY}")
    return float(point.z)


def _actor_transform_payload(actor: object) -> dict[str, tuple[float, float, float]]:
    """Capture only the persisted transform fields needed for the save contract."""
    location = actor.get_actor_location()
    rotation = actor.get_actor_rotation()
    return {
        "location": (float(location.x), float(location.y), float(location.z)),
        "rotation": (float(rotation.pitch), float(rotation.yaw), float(rotation.roll)),
    }


def _root_component(actor: object) -> object | None:
    """Resolve the root through both UE Python actor bindings used by this project."""
    try:
        component = actor.get_root_component()
    except Exception:
        component = None
    if component is not None:
        return component
    try:
        return actor.get_editor_property("root_component")
    except Exception:
        return None


def _set_actor_transform(actor: object, location: tuple[float, float, float], rotation: tuple[float, float, float] | None = None) -> None:
    """Mark the actor package dirty before changing a map-owned transform."""
    actor.modify()
    root_component = _root_component(actor)
    if root_component is not None:
        root_component.modify()
    actor.set_actor_location(_vector(location), False, False)
    if rotation is not None:
        actor.set_actor_rotation(_rotator(rotation), False)


def _actors_with_label(label: str) -> list[object]:
    return [actor for actor in unreal.EditorLevelLibrary.get_all_level_actors() if _actor_label(actor) == label]


def _ensure_single_scene_actor(
    label: str,
    expected_class_name: str,
    actor_class: object,
    location: tuple[float, float, float],
    rotation: tuple[float, float, float] = (0.0, 0.0, 0.0),
) -> tuple[object, bool]:
    matches = _actors_with_label(label)
    if len(matches) > 1:
        raise RuntimeError(f"battle scaffold actor label is duplicated: {label}")
    if matches:
        actor = matches[0]
        if _actor_class_name(actor) != expected_class_name:
            raise RuntimeError(
                f"battle scaffold label {label} has unexpected class {_actor_class_name(actor)}"
            )
        return actor, False
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(actor_class, _vector(location), _rotator(rotation))
    if actor is None:
        raise RuntimeError(f"could not spawn battle scaffold actor: {label}")
    actor.set_actor_label(label)
    return actor, True


def _ensure_camera_tag(camera: object) -> None:
    try:
        tags = list(camera.get_editor_property("tags"))
    except Exception:
        tags = []
    tag = unreal.Name(CAMERA_LABEL)
    if tag not in tags:
        tags.append(tag)
        camera.set_editor_property("tags", tags)


def _camera_component(camera: object) -> object | None:
    try:
        component = camera.get_camera_component()
    except Exception:
        component = None
    if component is not None:
        return component
    try:
        return camera.get_editor_property("camera_component")
    except Exception:
        return None


def add_battle_scaffolding() -> dict[str, object]:
    """Add only battle runtime actors to a fully cleaned, visible target map."""
    _require_unreal()
    _require_no_active_pie()
    current_map = _current_editor_map_path()
    if not is_target_battle_map(current_map):
        raise RuntimeError(
            f"refusing battle scaffold because the visible editor map is {current_map}, not {TARGET_BATTLE_MAP}"
        )

    records = _current_target_cleanup_records()
    scaffold_labels = set(scaffold_plan()["actors"])
    remaining_non_scaffold = [record for record in records if str(record["label"]) not in scaffold_labels]
    remaining_summary = summarize_cleanup_records(remaining_non_scaffold)
    if remaining_summary["mixed_foliage_labels"] or remaining_summary["actions"].get("delete", 0):
        raise RuntimeError("battle scaffold requires a fully cleaned town copy")

    before = map_hash_baseline()
    target_file = map_file_for_package(TARGET_BATTLE_MAP)
    target_sha_before = sha256_file(target_file)
    spec = battle_scaffold_spec()
    world = unreal.EditorLevelLibrary.get_editor_world()
    ground_z = _sample_landscape_ground_z(world)
    locations = battle_scaffold_locations_for_ground(ground_z)
    world.get_world_settings().set_editor_property(
        "default_game_mode", unreal.GameXXKFlowMapGameMode.static_class()
    )

    presenter, presenter_created = _ensure_single_scene_actor(
        PRESENTER_LABEL,
        "GameXXKBattleScenePresenter",
        unreal.GameXXKBattleScenePresenter.static_class(),
        locations["presenter"],
    )
    camera, camera_created = _ensure_single_scene_actor(
        CAMERA_LABEL,
        "CameraActor",
        unreal.CameraActor,
        locations["camera"],
        tuple(spec["camera"]["rotation"]),
    )
    _ensure_camera_tag(camera)
    camera_component = _camera_component(camera)
    if camera_component is None:
        raise RuntimeError("battle camera has no camera component")
    camera_component.set_editor_property("projection_mode", unreal.CameraProjectionMode.PERSPECTIVE)
    camera_component.set_editor_property("field_of_view", float(spec["camera"]["fov"]))

    player_start, player_start_created = _ensure_single_scene_actor(
        PLAYER_START_LABEL,
        "PlayerStart",
        unreal.PlayerStart,
        locations["player_start"],
    )
    light, light_created = _ensure_single_scene_actor(
        LIGHT_LABEL,
        "DirectionalLight",
        unreal.DirectionalLight,
        locations["light"],
        tuple(spec["light"]["rotation"]),
    )
    scene_actors = {
        PRESENTER_LABEL: presenter,
        CAMERA_LABEL: camera,
        PLAYER_START_LABEL: player_start,
        LIGHT_LABEL: light,
    }
    transforms_before = {
        label: _actor_transform_payload(actor) for label, actor in scene_actors.items()
    }
    _set_actor_transform(presenter, locations["presenter"])
    _set_actor_transform(camera, locations["camera"], tuple(spec["camera"]["rotation"]))
    _set_actor_transform(player_start, locations["player_start"])
    _set_actor_transform(light, locations["light"], tuple(spec["light"]["rotation"]))
    transforms_after = {
        label: _actor_transform_payload(actor) for label, actor in scene_actors.items()
    }
    transforms_changed = transforms_before != transforms_after
    if not unreal.EditorLoadingAndSavingUtils.save_current_level():
        raise RuntimeError(f"could not save battle scaffolding in {TARGET_BATTLE_MAP}")
    if not unreal.EditorAssetLibrary.save_asset(TARGET_BATTLE_MAP, only_if_is_dirty=False):
        raise RuntimeError(f"could not force-save battle scaffolding map asset: {TARGET_BATTLE_MAP}")

    target_sha_after = sha256_file(target_file)
    if transforms_changed and target_sha_before == target_sha_after:
        raise RuntimeError("transform changed in memory but target map hash did not change")

    after = map_hash_baseline()
    if before["source_sha256"] != after["source_sha256"]:
        raise RuntimeError("protected town source map hash changed while adding battle scaffolding")
    if before["legacy_battle_sha256"] != after["legacy_battle_sha256"]:
        raise RuntimeError("protected legacy battle map hash changed while adding battle scaffolding")

    report: dict[str, object] = {
        "schema_version": 1,
        "stage": "add_battle_scaffolding",
        "target_map": TARGET_BATTLE_MAP,
        "target_sha256_before": target_sha_before,
        "target_sha256_after": target_sha_after,
        "ground_z": ground_z,
        "locations": locations,
        "transforms_changed": transforms_changed,
        "source_sha256": after["source_sha256"],
        "legacy_battle_sha256": after["legacy_battle_sha256"],
        "actors": {
            PRESENTER_LABEL: {"created": presenter_created, "class": _actor_class_name(presenter)},
            CAMERA_LABEL: {"created": camera_created, "class": _actor_class_name(camera)},
            PLAYER_START_LABEL: {"created": player_start_created, "class": _actor_class_name(player_start)},
            LIGHT_LABEL: {"created": light_created, "class": _actor_class_name(light)},
        },
    }
    MANIFEST_PATH.parent.mkdir(parents=True, exist_ok=True)
    MANIFEST_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return report


def validate_final_map() -> dict[str, object]:
    """Read-only final contract check for the visible town-derived battle map."""
    records = _current_target_cleanup_records()
    validation = validate_final_battle_town_records(records)
    target_file = map_file_for_package(TARGET_BATTLE_MAP)
    baseline = map_hash_baseline()
    report: dict[str, object] = {
        "stage": "validate_final_map",
        "current_map": _current_editor_map_path(),
        "target_sha256": sha256_file(target_file),
        "source_sha256": baseline["source_sha256"],
        "legacy_battle_sha256": baseline["legacy_battle_sha256"],
        "actor_count": len(records),
        **validation,
    }
    AUDIT_PATH.parent.mkdir(parents=True, exist_ok=True)
    AUDIT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return report


def preflight_current_map() -> dict[str, str | int | list[str]]:
    """Verify the visible editor is already on the target before cleanup begins."""
    _require_unreal()
    _require_no_active_pie()
    current_map = _current_editor_map_path()
    if not is_target_battle_map(current_map):
        raise RuntimeError(
            f"refusing cleanup because the visible editor map is {current_map}, not {TARGET_BATTLE_MAP}"
        )
    target_file = map_file_for_package(TARGET_BATTLE_MAP)
    baseline = map_hash_baseline()
    return {
        "stage": "preflight_current_map",
        "current_map": current_map,
        "target_map": TARGET_BATTLE_MAP,
        "target_sha256": sha256_file(target_file),
        "source_sha256": baseline["source_sha256"],
        "legacy_battle_sha256": baseline["legacy_battle_sha256"],
        "actor_count": len(list(unreal.EditorLevelLibrary.get_all_level_actors())),
        "dirty_packages": _dirty_package_names(),
    }


def duplicate_battle_map() -> dict[str, str | int]:
    """Duplicate the source map without loading either Landscape map through MCP."""
    _require_unreal()
    _require_no_active_pie()
    _require_clean_editor()
    if not SOURCE_MAP.startswith("/Game/GameXXK/Maps/Prototype/"):
        raise RuntimeError(f"unexpected source map outside the protected town namespace: {SOURCE_MAP}")

    asset_library = unreal.EditorAssetLibrary
    if not asset_library.does_asset_exist(SOURCE_MAP):
        raise RuntimeError(f"protected town source map is missing: {SOURCE_MAP}")
    if asset_library.does_asset_exist(TARGET_BATTLE_MAP):
        raise RuntimeError(f"refusing to overwrite an existing battle map: {TARGET_BATTLE_MAP}")

    before = map_hash_baseline()

    target_directory = TARGET_BATTLE_MAP.rsplit("/", 1)[0]
    if not asset_library.does_directory_exist(target_directory):
        if not asset_library.make_directory(target_directory):
            raise RuntimeError(f"could not create target map directory: {target_directory}")

    duplicated = asset_library.duplicate_asset(SOURCE_MAP, TARGET_BATTLE_MAP)
    if duplicated is None or not asset_library.does_asset_exist(TARGET_BATTLE_MAP):
        raise RuntimeError(f"could not duplicate protected town map to {TARGET_BATTLE_MAP}")
    if not asset_library.save_loaded_asset(duplicated, True):
        raise RuntimeError(f"could not save only the duplicated battle map asset: {TARGET_BATTLE_MAP}")
    target_file = map_file_for_package(TARGET_BATTLE_MAP)
    after = map_hash_baseline()
    manifest = build_duplicate_manifest(before, after, sha256_file(target_file))
    MANIFEST_PATH.parent.mkdir(parents=True, exist_ok=True)
    MANIFEST_PATH.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return manifest


def finalize_existing_duplicate() -> dict[str, str | int]:
    """Save only a target asset left in memory by a prior interrupted duplicate."""
    _require_unreal()
    _require_no_active_pie()
    _require_no_unrelated_dirty_packages()
    asset_library = unreal.EditorAssetLibrary
    target_file = map_file_for_package(TARGET_BATTLE_MAP)
    if target_file.is_file():
        raise RuntimeError(f"target map is already on disk; recovery is not applicable: {TARGET_BATTLE_MAP}")
    if not asset_library.does_asset_exist(TARGET_BATTLE_MAP):
        raise RuntimeError(f"no in-memory target asset is available to recover: {TARGET_BATTLE_MAP}")

    before = map_hash_baseline()
    target_asset = asset_library.load_asset(TARGET_BATTLE_MAP)
    if target_asset is None:
        raise RuntimeError(f"could not resolve in-memory target asset: {TARGET_BATTLE_MAP}")
    if not asset_library.save_loaded_asset(target_asset, True):
        raise RuntimeError(f"could not save only the recovered battle map asset: {TARGET_BATTLE_MAP}")
    after = map_hash_baseline()
    manifest = build_duplicate_manifest(
        before,
        after,
        sha256_file(target_file),
        stage="finalize_existing_duplicate",
    )
    MANIFEST_PATH.parent.mkdir(parents=True, exist_ok=True)
    MANIFEST_PATH.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return manifest


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    stages = parser.add_mutually_exclusive_group(required=True)
    stages.add_argument(
        "--duplicate-only",
        action="store_true",
        help="duplicate the protected town map without loading either Landscape map",
    )
    stages.add_argument(
        "--finalize-existing-duplicate",
        action="store_true",
        help="save only an interrupted in-memory target duplicate without loading a map",
    )
    stages.add_argument(
        "--preflight-current-map",
        action="store_true",
        help="verify that the visible editor has already opened the target battle map",
    )
    stages.add_argument(
        "--audit-current-map",
        action="store_true",
        help="read-only actor retention/deletion audit for the visible target map",
    )
    stages.add_argument(
        "--cleanup-current-map",
        action="store_true",
        help="delete one bounded actor batch from the already visible target map",
    )
    stages.add_argument(
        "--add-battle-scaffolding",
        action="store_true",
        help="add only the battle camera, presenter, PlayerStart, and directional light",
    )
    stages.add_argument(
        "--validate-final-map",
        action="store_true",
        help="validate nature plus the exact battle scaffold in the visible target map",
    )
    parser.add_argument("--limit", type=int, default=50, help="maximum actors to delete in one batch (1-50)")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> dict[str, str]:
    args = parse_args(argv)
    if args.duplicate_only:
        return duplicate_battle_map()
    if args.finalize_existing_duplicate:
        return finalize_existing_duplicate()
    if args.preflight_current_map:
        return preflight_current_map()
    if args.audit_current_map:
        return audit_current_map()
    if args.cleanup_current_map:
        return cleanup_current_map(args.limit)
    if args.add_battle_scaffolding:
        return add_battle_scaffolding()
    if args.validate_final_map:
        return validate_final_map()
    raise RuntimeError("no battle-town backdrop stage was selected")


if __name__ == "__main__":
    print(json.dumps(main(), ensure_ascii=False))
