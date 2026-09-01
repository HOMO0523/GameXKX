"""Import the approved 2026-08-27 corrected animation batch into UE.

Runtime battle clips replace only their 2K/1K sibling textures; the untouched
4K masters remain the source-of-truth fallback.  Town hero clips are verified
against the dedicated TownHorizontal import, while prologue-only clips are
stored in an isolated cinematic catalog for the upcoming tutorial sequence.
The retired deer-bow source is never discovered or imported.
"""

from __future__ import annotations

import json
import math
from pathlib import Path
from typing import NamedTuple

try:
    import unreal
except ImportError:  # Pure contract tests load this module outside UE.
    unreal = None


MAPPING_REVIEW_RELATIVE = Path(
    "Saved/HarnessReports/animation-upgrade-20260827-corrected/mapping-review/mapping-review.json"
)
CANDIDATE_ROOT_RELATIVE = Path(
    "SourceAssets/AnimationProduction/upgrade_20260827_corrected"
)
BATTLE_ATLAS_DIR = "/Game/GameXXK/BattleAnimations/Atlases"
BATTLE_IDLE_FLIPBOOK_DIR = "/Game/GameXXK/BattleAnimations/IdleFlipbooks"
TOWN_ATLAS_DIR = "/Game/GameXXK/Characters/Hero/TownHorizontal/Atlases"
CINEMATIC_ATLAS_DIR = "/Game/GameXXK/Cinematics/Prologue/Atlases"
REPORT_RELATIVE = Path(
    "Saved/HarnessReports/animation-upgrade-20260827-corrected/runtime-import-report.json"
)


class CandidateEntry(NamedTuple):
    candidate_id: str
    source_relative: str
    fps: float
    frame_count: int
    duration_seconds: float
    atlas_2k: Path
    atlas_1k: Path


class RuntimeTargetSpec(NamedTuple):
    asset_id_2k: str
    asset_id_1k: str
    fps: float
    update_idle_flipbook: bool


def _target(asset_id_2k: str, fps: float, idle: bool = False) -> RuntimeTargetSpec:
    if "_2k_" not in asset_id_2k:
        raise ValueError(f"runtime target must use a 2K sibling id: {asset_id_2k}")
    return RuntimeTargetSpec(
        asset_id_2k=asset_id_2k,
        asset_id_1k=asset_id_2k.replace("_2k_", "_1k_"),
        fps=float(fps),
        update_idle_flipbook=bool(idle),
    )


def runtime_target_specs() -> dict[str, RuntimeTargetSpec]:
    return {
        "character_00_hero_combat_idle_candidate": _target(
            "character_00_hero_2k_idle", 29.702970297029704, True
        ),
        "character_00_hero_attack_punch_candidate": _target(
            "character_00_hero_2k_attack_punch", 46.15384615384615
        ),
        "character_00_hero_attack_kick_candidate": _target(
            "character_00_hero_2k_attack_kick", 46.15384615384615
        ),
        "enemy_01_rooster_idle_candidate": _target(
            "enemy_01_rooster_2k_idle", 14.74201474201474, True
        ),
        "enemy_01_rooster_attack_candidate": _target(
            "enemy_01_rooster_2k_attack", 46.15384615384615
        ),
        "enemy_03_weasel_idle_candidate": _target(
            "enemy_03_weasel_2k_idle", 20.689655172413794, True
        ),
        "enemy_03_weasel_attack_candidate": _target(
            "enemy_03_weasel_2k_attack", 41.66666666666667
        ),
        "enemy_05_ironfeather_idle_candidate": _target(
            "enemy_05_ironfeather_2k_idle", 11.904761904761905, True
        ),
        "enemy_07_graywolf_idle_candidate": _target(
            "enemy_07_graywolf_2k_idle", 14.74201474201474, True
        ),
        "enemy_11_graymane_attack_candidate": _target(
            "enemy_11_graymane_2k_attack", 42.25352112676057
        ),
        "enemy_16_toad_idle_candidate": _target(
            "enemy_16_toad_2k_idle", 11.904761904761905, True
        ),
        "enemy_18_deer_idle_candidate": _target(
            "enemy_18_deer_2k_idle", 11.904761904761905, True
        ),
        "enemy_18_deer_attack_candidate": _target(
            "enemy_18_deer_2k_attack", 44.44444444444444
        ),
        "candidate_yue_fire_idle": _target(
            "character_09_yue_bai_2k_idle", 14.74201474201474, True
        ),
    }


TOWN_ASSET_NAMES: dict[str, str] = {
    "character_00_hero_state_idle_candidate": "T_Hero_Town_Idle_Left_Atlas",
    "character_00_hero_walk_start_candidate": "T_Hero_Town_WalkStart_Left_Atlas",
    "character_00_hero_walk_loop_candidate": "T_Hero_Town_WalkLoop_Left_Atlas",
    "character_00_hero_walk_stop_candidate": "T_Hero_Town_WalkStop_Left_Atlas",
    "cinematic_hero_adjust_backpack": "T_Hero_Town_AdjustBackpack_Left_Atlas",
    "cinematic_hero_deep_breath": "T_Hero_Town_DeepBreath_Left_Atlas",
    "cinematic_hero_collect_item_with_ui": "T_Hero_Town_CollectItem_Left_Atlas",
    "character_00_hero_combat_idle_candidate": "T_Hero_Town_CombatIdle_Left_Atlas",
    "character_00_hero_attack_punch_candidate": "T_Hero_Town_Punch_Left_Atlas",
    "character_00_hero_attack_kick_candidate": "T_Hero_Town_Kick_Left_Atlas",
}


CINEMATIC_CATALOG_IDS: dict[str, str] = {
    "candidate_yue_fire_intro": "character_09_yue_bai_intro",
    "candidate_yue_fire_outro": "character_09_yue_bai_outro",
    "cinematic_horse_idle": "cinematic_horse_idle",
    "cinematic_horse_start_run_stop": "cinematic_horse_start_run_stop",
    "cinematic_carriage_run_stop": "cinematic_carriage_run_stop",
    "cinematic_carriage_post_stop_idle": "cinematic_carriage_post_stop_idle",
}


def discover_entries(project_root: Path) -> list[CandidateEntry]:
    project_root = Path(project_root).resolve()
    review_path = project_root / MAPPING_REVIEW_RELATIVE
    if not review_path.is_file():
        raise FileNotFoundError(review_path)
    review = json.loads(review_path.read_text(encoding="utf-8"))
    if review.get("excludedSources") != ["怪物们/鹿鞠躬.mov"]:
        raise RuntimeError("mapping review must exclude only the retired deer-bow source")

    entries: list[CandidateEntry] = []
    seen: set[str] = set()
    for item in review.get("entries", []):
        candidate_id = str(item.get("id", ""))
        source_relative = str(item.get("source", ""))
        if not candidate_id or candidate_id in seen:
            raise RuntimeError(f"invalid or duplicate candidate id: {candidate_id!r}")
        if source_relative == "怪物们/鹿鞠躬.mov" or "bow" in candidate_id.lower():
            raise RuntimeError("retired deer-bow source leaked into the approved entries")
        manifest_path = (
            project_root
            / CANDIDATE_ROOT_RELATIVE
            / candidate_id
            / "candidate_atlas"
            / "manifest.json"
        )
        if not manifest_path.is_file():
            raise FileNotFoundError(manifest_path)
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        frame_count = int(manifest.get("frameCount", 0))
        fps = float(manifest.get("fps", 0.0))
        duration = float(manifest.get("durationSeconds", 0.0))
        if frame_count != 60 or not math.isfinite(fps) or fps <= 0.0:
            raise RuntimeError(f"{candidate_id}: invalid candidate timing")
        if not math.isclose(fps, float(item.get("fps", 0.0)), abs_tol=1.0e-6):
            raise RuntimeError(f"{candidate_id}: review/manifest fps mismatch")
        atlases = manifest.get("atlases") or {}
        atlas_2k = project_root / str((atlases.get("2K") or {}).get("atlas", ""))
        atlas_1k = project_root / str((atlases.get("1K") or {}).get("atlas", ""))
        if not atlas_2k.is_file() or not atlas_1k.is_file():
            raise FileNotFoundError(f"{candidate_id}: approved atlas siblings are missing")
        entries.append(
            CandidateEntry(
                candidate_id=candidate_id,
                source_relative=source_relative,
                fps=fps,
                frame_count=frame_count,
                duration_seconds=duration,
                atlas_2k=atlas_2k.resolve(),
                atlas_1k=atlas_1k.resolve(),
            )
        )
        seen.add(candidate_id)
    if len(entries) != 27:
        raise RuntimeError(f"expected 27 approved candidates, got {len(entries)}")
    return entries


def _require_unreal() -> None:
    if unreal is None:
        raise RuntimeError("animation upgrade import requires UE editor Python")


def _project_root() -> Path:
    _require_unreal()
    return Path(str(unreal.Paths.project_dir())).resolve()


def _ensure_directory(path: str) -> None:
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        if not unreal.EditorAssetLibrary.make_directory(path):
            raise RuntimeError(f"could not create asset directory: {path}")


def _save(asset: object) -> None:
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset):
        raise RuntimeError(f"could not save asset: {asset.get_path_name()}")


def _texture_size(texture: object) -> tuple[int, int]:
    for width_name, height_name in (
        ("blueprint_get_size_x", "blueprint_get_size_y"),
        ("get_size_x", "get_size_y"),
    ):
        width = getattr(texture, width_name, None)
        height = getattr(texture, height_name, None)
        if callable(width) and callable(height):
            return int(width()), int(height())
    raise RuntimeError("could not query imported texture dimensions")


def _configure_texture(texture: object) -> None:
    texture.set_editor_property(
        "mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS
    )
    texture.set_editor_property("filter", unreal.TextureFilter.TF_BILINEAR)
    texture.set_editor_property(
        "compression_settings", unreal.TextureCompressionSettings.TC_BC7
    )
    texture.set_editor_property("srgb", True)
    try:
        texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    except Exception:
        pass


def _import_texture(source: Path, destination: str, name: str, expected_size: int):
    _ensure_directory(destination)
    task = unreal.AssetImportTask()
    task.filename = source.as_posix()
    task.destination_path = destination
    task.destination_name = name
    task.automated = True
    task.save = False
    task.replace_existing = True
    task.replace_existing_settings = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    asset_path = f"{destination}/{name}"
    texture = unreal.EditorAssetLibrary.load_asset(asset_path)
    if texture is None or not isinstance(texture, unreal.Texture2D):
        raise RuntimeError(f"could not import animation texture: {asset_path}")
    if _texture_size(texture) != (expected_size, expected_size):
        raise RuntimeError(
            f"{asset_path}: expected {expected_size}x{expected_size}, got {_texture_size(texture)}"
        )
    _configure_texture(texture)
    _save(texture)
    return texture


def _update_idle_flipbook(asset_id: str, fps: float) -> str:
    flipbook_name = f"FB_{asset_id}"
    path = f"{BATTLE_IDLE_FLIPBOOK_DIR}/{flipbook_name}"
    flipbook = unreal.EditorAssetLibrary.load_asset(path)
    if flipbook is None or not isinstance(flipbook, unreal.PaperFlipbook):
        raise RuntimeError(f"runtime idle flipbook is missing: {path}")
    keyframes = list(flipbook.get_editor_property("key_frames") or [])
    if len(keyframes) != 60:
        raise RuntimeError(f"{path}: expected 60 idle frames, got {len(keyframes)}")
    flipbook.set_editor_property("frames_per_second", float(fps))
    invalidate = getattr(flipbook, "invalidate_cached_data", None)
    if callable(invalidate):
        invalidate()
    _save(flipbook)
    return flipbook.get_path_name()


def _verify_town_asset(candidate_id: str) -> str:
    name = TOWN_ASSET_NAMES[candidate_id]
    path = f"{TOWN_ATLAS_DIR}/{name}"
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if asset is None or not isinstance(asset, unreal.Texture2D):
        raise RuntimeError(f"approved town state is not imported: {path}")
    return asset.get_path_name()


def import_approved_batch() -> dict:
    _require_unreal()
    project_root = _project_root()
    entries = discover_entries(project_root)
    runtime_targets = runtime_target_specs()
    for directory in (BATTLE_ATLAS_DIR, BATTLE_IDLE_FLIPBOOK_DIR, CINEMATIC_ATLAS_DIR):
        _ensure_directory(directory)

    results: list[dict] = []
    for entry in entries:
        item: dict = {
            "candidateId": entry.candidate_id,
            "source": entry.source_relative,
            "fps": entry.fps,
            "frameCount": entry.frame_count,
            "townAsset": "",
            "runtimeAssets": [],
            "cinematicAssets": [],
            "idleFlipbooks": [],
        }
        if entry.candidate_id in TOWN_ASSET_NAMES:
            item["townAsset"] = _verify_town_asset(entry.candidate_id)

        target = runtime_targets.get(entry.candidate_id)
        if target is not None:
            for asset_id, atlas, expected_size in (
                (target.asset_id_2k, entry.atlas_2k, 2048),
                (target.asset_id_1k, entry.atlas_1k, 1024),
            ):
                texture = _import_texture(
                    atlas,
                    BATTLE_ATLAS_DIR,
                    f"T_{asset_id}_atlas",
                    expected_size,
                )
                item["runtimeAssets"].append(texture.get_path_name())
            if target.update_idle_flipbook:
                item["idleFlipbooks"].append(
                    _update_idle_flipbook(target.asset_id_2k, target.fps)
                )
                one_k_path = (
                    f"{BATTLE_IDLE_FLIPBOOK_DIR}/FB_{target.asset_id_1k}"
                )
                if unreal.EditorAssetLibrary.does_asset_exist(one_k_path):
                    item["idleFlipbooks"].append(
                        _update_idle_flipbook(target.asset_id_1k, target.fps)
                    )

        cinematic_id = CINEMATIC_CATALOG_IDS.get(entry.candidate_id)
        if cinematic_id is not None:
            for suffix, atlas, expected_size in (
                ("2k", entry.atlas_2k, 2048),
                ("1k", entry.atlas_1k, 1024),
            ):
                texture = _import_texture(
                    atlas,
                    CINEMATIC_ATLAS_DIR,
                    f"T_{cinematic_id}_{suffix}_atlas",
                    expected_size,
                )
                item["cinematicAssets"].append(texture.get_path_name())
        results.append(item)

    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    report = {
        "ok": True,
        "sourceRoot": str((project_root / CANDIDATE_ROOT_RELATIVE).resolve()),
        "candidateCount": len(entries),
        "runtimeCandidateCount": len(runtime_targets),
        "townCandidateCount": len(TOWN_ASSET_NAMES),
        "cinematicCandidateCount": len(CINEMATIC_CATALOG_IDS),
        "excludedSources": ["怪物们/鹿鞠躬.mov"],
        "results": results,
    }
    report_path = project_root / REPORT_RELATIVE
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return report


def main() -> None:
    import_approved_batch()


if __name__ == "__main__":
    main()
