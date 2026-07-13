"""Inventory materials used by static-mesh components in the Asian Village map."""

from __future__ import annotations

import json
import re
from collections import defaultdict
from pathlib import Path

import unreal

import gamexxk_occlusion_material_naming as naming


MAP_PATH = "/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo"
OUTPUT_RELATIVE_PATH = "Config/GameXXK/Occlusion/AsianVillageMaterialInventory.json"


def is_excluded_source(source_path: str) -> bool:
    """Return whether a source material is outside the naming module's eligible set."""
    return not naming.is_eligible_material(source_path)


def _blend_mode_name(material: object) -> str:
    value = str(material.get_blend_mode()).upper()
    match = re.search(r"\bBLEND_[A-Z_]+\b", value)
    if match is None:
        raise RuntimeError(f"Could not normalize material blend mode: {value!r}")
    return match.group(0)


def _actor_label(actor: object) -> str:
    try:
        return str(actor.get_actor_label())
    except Exception:
        return str(actor.get_name())


def _project_output_path() -> Path:
    project_dir = Path(str(unreal.Paths.project_dir())).resolve()
    output_path = (project_dir / OUTPUT_RELATIVE_PATH).resolve()
    allowed_dir = (project_dir / "Config" / "GameXXK" / "Occlusion").resolve()
    if output_path.parent != allowed_dir:
        raise RuntimeError(f"Inventory output escaped allowed directory: {output_path}")
    return output_path


def collect_inventory() -> dict[str, object]:
    if not unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH):
        raise RuntimeError(f"Failed to load editor map: {MAP_PATH}")

    aggregates: dict[str, dict[str, object]] = {}
    component_ids: dict[str, set[str]] = defaultdict(set)
    actor_labels: dict[str, set[str]] = defaultdict(set)

    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    for actor in actors:
        label = _actor_label(actor)
        components = actor.get_components_by_class(unreal.StaticMeshComponent)
        for component in components:
            component_id = str(component.get_path_name())
            for slot_index in range(int(component.get_num_materials())):
                material = component.get_material(slot_index)
                if material is None:
                    continue
                source_path = str(material.get_path_name())
                entry = aggregates.setdefault(
                    source_path,
                    {
                        "source_path": source_path,
                        "target_path": naming.cutout_object_path(source_path),
                        "blend_mode": _blend_mode_name(material),
                        "usage_count": 0,
                        "excluded": is_excluded_source(source_path),
                    },
                )
                entry["usage_count"] = int(entry["usage_count"]) + 1
                component_ids[source_path].add(component_id)
                actor_labels[source_path].add(label)

    materials: list[dict[str, object]] = []
    targets: dict[str, str] = {}
    for source_path in sorted(aggregates):
        entry = aggregates[source_path]
        target_path = str(entry["target_path"])
        previous_source = targets.get(target_path)
        if previous_source is not None and previous_source != source_path:
            raise RuntimeError(
                f"Duplicate target path {target_path}: {previous_source} and {source_path}"
            )
        targets[target_path] = source_path
        entry["component_count"] = len(component_ids[source_path])
        entry["representative_actor_labels"] = sorted(actor_labels[source_path])[:10]
        materials.append(entry)

    excluded_count = sum(bool(entry["excluded"]) for entry in materials)
    return {
        "map": MAP_PATH,
        "summary": {
            "material_count": len(materials),
            "eligible_count": len(materials) - excluded_count,
            "excluded_count": excluded_count,
            "usage_count": sum(int(entry["usage_count"]) for entry in materials),
            "component_count": len({item for values in component_ids.values() for item in values}),
            "duplicate_target_count": 0,
        },
        "materials": materials,
    }


def main() -> dict[str, object]:
    inventory = collect_inventory()
    output_path = _project_output_path()
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(
        json.dumps(inventory, indent=2, ensure_ascii=False, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    result = {
        "ok": True,
        "output": str(output_path),
        **dict(inventory["summary"]),
    }
    unreal.log(f"[GameXXK] Asian Village material inventory: {json.dumps(result, sort_keys=True)}")
    print(json.dumps(result, sort_keys=True))
    return result


if __name__ == "__main__":
    RESULT = main()
