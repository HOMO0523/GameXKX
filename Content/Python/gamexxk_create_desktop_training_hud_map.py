"""Create an isolated HUD-only copy of the Qingshan town map.

The source town map is never loaded for mutation.  The copied map keeps only a
PlayerStart; world actors, scenery, lights, cameras, NPCs and placed
characters are removed so the MVP can be tested as a pure HUD surface.
"""

from __future__ import annotations

import json
from pathlib import Path

import unreal


SOURCE_MAP = "/Game/GameXXK/Maps/L_QingshanInn"
TARGET_MAP = "/Game/GameXXK/Maps/L_DesktopTrainingHUD"
REPORT = Path(unreal.Paths.project_saved_dir()) / "HarnessReports/desktop-training-hud-map.json"


def _class_name(actor) -> str:
    try:
        return str(actor.get_class().get_name())
    except Exception:
        return ""


def _label(actor) -> str:
    try:
        return str(actor.get_actor_label())
    except Exception:
        return str(actor)


def _current_package() -> str:
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if not world:
        return ""
    try:
        return str(world.get_package().get_name())
    except Exception:
        return ""


def _ensure_duplicate() -> bool:
    if unreal.EditorAssetLibrary.does_asset_exist(TARGET_MAP):
        return False
    if not unreal.EditorAssetLibrary.does_asset_exist(SOURCE_MAP):
        raise RuntimeError(f"source map is missing: {SOURCE_MAP}")
    duplicated = unreal.EditorAssetLibrary.duplicate_asset(SOURCE_MAP, TARGET_MAP)
    if duplicated is None:
        raise RuntimeError(f"failed to duplicate {SOURCE_MAP} -> {TARGET_MAP}")
    if not unreal.EditorAssetLibrary.save_loaded_asset(duplicated, True):
        raise RuntimeError(f"failed to save duplicated map: {TARGET_MAP}")
    return True


def _load_target() -> None:
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if level_subsystem is None or not level_subsystem.load_level(TARGET_MAP):
        raise RuntimeError(f"failed to load target map: {TARGET_MAP}")
    if _current_package() != TARGET_MAP:
        raise RuntimeError(f"target map did not become current: {_current_package()}")


def _remove_world_actors() -> tuple[list[dict], list[dict]]:
    actors = list(unreal.EditorLevelLibrary.get_all_level_actors())
    removed: list[dict] = []
    kept: list[dict] = []
    for actor in actors:
        class_name = _class_name(actor)
        label = _label(actor)
        # Keep a spawn anchor only. HUD and the GameMode/PlayerController are
        # runtime-owned and are not level actors, so no placed character is
        # needed in this map.
        if class_name in {"PlayerStart", "PlayerStartPIE"}:
            kept.append({"label": label, "class": class_name})
            continue
        if not unreal.EditorLevelLibrary.destroy_actor(actor):
            raise RuntimeError(f"failed to remove actor: {label} ({class_name})")
        removed.append({"label": label, "class": class_name})
    saved = bool(unreal.EditorLoadingAndSavingUtils.save_current_level())
    if not saved:
        saved = bool(unreal.EditorLevelLibrary.save_current_level())
    if not saved:
        # Some headless/editor MCP sessions do not expose a LevelEditor save
        # transaction even though the duplicated map package is loaded.
        # Saving the isolated package is still safe and deterministic.
        map_asset = unreal.EditorAssetLibrary.load_asset(TARGET_MAP)
        saved = bool(map_asset and unreal.EditorAssetLibrary.save_loaded_asset(map_asset, True))
    if not saved:
        raise RuntimeError(f"failed to save cleaned map: {TARGET_MAP}")
    return removed, kept


def main() -> None:
    created = _ensure_duplicate()
    _load_target()
    # An existing target is deliberately revalidated/re-cleaned only in this
    # isolated map; SOURCE_MAP is never touched.
    removed, kept = _remove_world_actors()
    report = {
        "schemaVersion": 1,
        "ok": True,
        "sourceMap": SOURCE_MAP,
        "targetMap": TARGET_MAP,
        "created": created,
        "removedActorCount": len(removed),
        "removedActors": removed,
        "keptActors": kept,
        "policy": "HUD-only; no placed scenery/camera/light/character actors",
    }
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("[GAMEXXK] " + json.dumps(report, ensure_ascii=False))


if __name__ == "__main__":
    main()
