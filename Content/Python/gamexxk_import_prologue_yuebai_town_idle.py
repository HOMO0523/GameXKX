"""Import YueBai's approved blue-flame 2K town idle without clipping.

The blue-flame atlas replaced an older texture whose PaperSprites retained
tight render boxes generated from the old silhouettes.  This isolated
narrative copy keeps the intended 60-frame blue-flame atlas while forcing each
sprite to render its complete 256px source cell.  Battle assets are untouched.
"""

from __future__ import annotations

import hashlib
import json
import sys
from pathlib import Path

import unreal


SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import gamexxk_import_battle_animation_production as production


DERIVED_ASSET_ID = "character_09_yue_bai_town_2k_idle"
CANDIDATE_MANIFEST_RELATIVE = Path(
    "SourceAssets/AnimationProduction/upgrade_20260827_corrected/"
    "candidate_yue_fire_idle/candidate_atlas/manifest.json"
)
ATLAS_DIR = "/Game/GameXXK/Cinematics/Prologue/Atlases"
SPRITE_ROOT = "/Game/GameXXK/Cinematics/Prologue/IdleSprites"
FLIPBOOK_DIR = "/Game/GameXXK/Cinematics/Prologue/IdleFlipbooks"
TEXTURE_NAME = f"T_{DERIVED_ASSET_ID}_atlas"
FLIPBOOK_NAME = f"FB_{DERIVED_ASSET_ID}"
TEXTURE_PATH = f"{ATLAS_DIR}/{TEXTURE_NAME}"
FLIPBOOK_PATH = f"{FLIPBOOK_DIR}/{FLIPBOOK_NAME}"
FRAME_COUNT = 60
CELL_SIZE = 256
ATLAS_SIZE = 2048
APPROVED_FPS = 14.74201474201474


def _project_root() -> Path:
    return Path(str(unreal.Paths.project_dir())).resolve()


def _source_contract() -> dict:
    manifest_path = _project_root() / CANDIDATE_MANIFEST_RELATIVE
    if not manifest_path.is_file():
        raise RuntimeError(f"blue-flame YueBai manifest is missing: {manifest_path}")
    payload = json.loads(manifest_path.read_text(encoding="utf-8"))
    atlas_contract = (payload.get("atlases") or {}).get("2K") or {}
    atlas_relative = str(atlas_contract.get("atlas", ""))
    atlas_path = (_project_root() / atlas_relative).resolve()
    if int(payload.get("frameCount", 0)) != FRAME_COUNT:
        raise RuntimeError("blue-flame YueBai idle must retain all 60 frames")
    if list(atlas_contract.get("size") or []) != [ATLAS_SIZE, ATLAS_SIZE]:
        raise RuntimeError("blue-flame YueBai idle must use its 2048px atlas")
    fps = float(payload.get("fps", 0.0))
    if abs(fps - APPROVED_FPS) > 1.0e-6:
        raise RuntimeError(f"blue-flame YueBai FPS drifted: {fps}")
    edge_alpha = payload.get("edgeAlphaMax") or {}
    if any(
        int(edge_alpha.get(edge, -1)) != 0
        for edge in ("left", "top", "right", "bottom")
    ):
        raise RuntimeError("blue-flame YueBai idle touches an atlas cell edge")
    if not atlas_path.is_file():
        raise RuntimeError(f"blue-flame YueBai 2K atlas is missing: {atlas_path}")
    return {
        "manifest": str(manifest_path),
        "atlas": str(atlas_path),
        "sha256": hashlib.sha256(atlas_path.read_bytes()).hexdigest(),
        "edgeAlphaMax": edge_alpha,
        "fps": fps,
    }


def _import_texture(source: Path):
    task = unreal.AssetImportTask()
    task.filename = str(source)
    task.destination_path = ATLAS_DIR
    task.destination_name = TEXTURE_NAME
    task.automated = True
    task.save = False
    task.replace_existing = True
    task.replace_existing_settings = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.EditorAssetLibrary.load_asset(TEXTURE_PATH)
    if texture is None or not isinstance(texture, unreal.Texture2D):
        raise RuntimeError(f"YueBai town-idle texture did not load: {TEXTURE_PATH}")
    production._configure_texture(texture)
    production._save(texture)
    return texture


def _configure_full_cell_sprite(sprite: object, texture: object, frame_index: int) -> None:
    production._configure_sprite(sprite, texture, frame_index)
    render_geometry = sprite.get_editor_property("render_geometry")
    render_geometry.set_editor_property(
        "geometry_type", unreal.SpritePolygonMode.SOURCE_BOUNDING_BOX
    )
    sprite.set_editor_property("render_geometry", render_geometry)
    production._save(sprite)


def import_town_idle() -> dict:
    source = _source_contract()
    production._apply_two_k_mode()
    production.ATLAS_ASSET_DIR = ATLAS_DIR
    production.IDLE_SPRITE_ROOT = SPRITE_ROOT
    production.IDLE_FLIPBOOK_DIR = FLIPBOOK_DIR
    for directory in (ATLAS_DIR, SPRITE_ROOT, FLIPBOOK_DIR):
        production._ensure_directory(directory)

    texture = _import_texture(Path(source["atlas"]))
    if production._texture_size(texture) != (ATLAS_SIZE, ATLAS_SIZE):
        raise RuntimeError("YueBai town-idle texture is not 2048x2048")

    sprite_dir = f"{SPRITE_ROOT}/{DERIVED_ASSET_ID}"
    production._ensure_directory(sprite_dir)
    sprites = []
    source_bounding_count = 0
    for frame_index in range(FRAME_COUNT):
        sprite, _created = production._create_or_load(
            f"SPR_{DERIVED_ASSET_ID}_{frame_index:02d}",
            sprite_dir,
            unreal.PaperSprite,
            unreal.PaperSpriteFactory(),
        )
        _configure_full_cell_sprite(sprite, texture, frame_index)
        geometry = sprite.get_editor_property("render_geometry")
        if (
            geometry.get_editor_property("geometry_type")
            == unreal.SpritePolygonMode.SOURCE_BOUNDING_BOX
        ):
            source_bounding_count += 1
        sprites.append(sprite)

    flipbook, _created = production._create_or_load(
        FLIPBOOK_NAME,
        FLIPBOOK_DIR,
        unreal.PaperFlipbook,
        unreal.PaperFlipbookFactory(),
    )
    keyframes = []
    for sprite in sprites:
        keyframe = unreal.PaperFlipbookKeyFrame()
        keyframe.set_editor_property("sprite", sprite)
        keyframe.set_editor_property("frame_run", 1)
        keyframes.append(keyframe)
    flipbook.set_editor_property("frames_per_second", float(source["fps"]))
    flipbook.set_editor_property("key_frames", keyframes)
    invalidate = getattr(flipbook, "invalidate_cached_data", None)
    if callable(invalidate):
        invalidate()
    production._save(flipbook)

    loaded_flipbook = unreal.EditorAssetLibrary.load_asset(FLIPBOOK_PATH)
    if loaded_flipbook is None or not isinstance(loaded_flipbook, unreal.PaperFlipbook):
        raise RuntimeError(f"YueBai town-idle flipbook did not load: {FLIPBOOK_PATH}")
    loaded_keyframes = list(
        loaded_flipbook.get_editor_property("key_frames") or []
    )
    fps = float(loaded_flipbook.get_editor_property("frames_per_second"))
    if len(loaded_keyframes) != FRAME_COUNT:
        raise RuntimeError(
            f"YueBai town-idle flipbook has {len(loaded_keyframes)} keyframes"
        )
    if abs(fps - float(source["fps"])) > 1.0e-6:
        raise RuntimeError(f"YueBai town-idle flipbook FPS drifted: {fps}")
    if source_bounding_count != FRAME_COUNT:
        raise RuntimeError(
            f"only {source_bounding_count}/60 YueBai sprites use full-cell geometry"
        )

    return {
        "ok": True,
        "source": source,
        "texture": texture.get_path_name(),
        "textureSize": list(production._texture_size(texture)),
        "flipbook": loaded_flipbook.get_path_name(),
        "keyframes": len(loaded_keyframes),
        "fps": fps,
        "sourceBoundingSprites": source_bounding_count,
        "battleAssetsTouched": False,
    }


def main() -> None:
    print(json.dumps(import_town_idle(), ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
