"""Import approved battle animation atlases and idle PaperFlipbooks.

All actions are imported as Texture2D atlases for the BattleBoard cinematic.
Only idle clips create PaperSprite/PaperFlipbook assets for persistent scene units.
The script is idempotent and supports small MCP-driven batches.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import NamedTuple

try:
    import unreal
except ImportError:  # Pure discovery/contract tests run outside the editor.
    unreal = None


PRODUCTION_RELATIVE_ROOT = Path("SourceAssets/AnimationProcessing/Production")
ATLAS_ASSET_DIR = "/Game/GameXXK/BattleAnimations/Atlases"
IDLE_SPRITE_ROOT = "/Game/GameXXK/BattleAnimations/IdleSprites"
IDLE_FLIPBOOK_DIR = "/Game/GameXXK/BattleAnimations/IdleFlipbooks"
FRAME_COUNT = 60
CELL_SIZE = 512
ATLAS_SIZE = 4096
ATLAS_COLUMNS = 8
ATLAS_ROWS = 8
FRAMES_PER_SECOND = 12.0
PIXELS_PER_UNREAL_UNIT = 2.5
TEXTURE_COMPRESSION_SETTING = "TC_BC7"
TEXTURE_MIP_SETTING = "TMGS_NO_MIPMAPS"
TEXTURE_FILTER_SETTING = "TF_BILINEAR"
TEXTURE_SRGB = True
# 2K memory-optimization staging: same 8x8 grid at half resolution.
TWO_K_PRODUCTION_RELATIVE_ROOT = Path("SourceAssets/AnimationProcessing/Production2K")
TWO_K_CELL_SIZE = 256
TWO_K_ATLAS_SIZE = 2048
# 1K staging: quarter resolution, same grid.
ONE_K_PRODUCTION_RELATIVE_ROOT = Path("SourceAssets/AnimationProcessing/Production1K")
ONE_K_CELL_SIZE = 128
ONE_K_ATLAS_SIZE = 1024
PILOT_ASSET_IDS = {
    "character_00_hero_idle",
    "character_00_hero_attack",
    "character_00_hero_hit",
    "enemy_01_rooster_idle",
    "enemy_01_rooster_attack",
    "enemy_01_rooster_hit",
}


def _apply_resolution_mode(relative_root: Path, cell_size: int, atlas_size: int) -> None:
    global PRODUCTION_RELATIVE_ROOT, CELL_SIZE, ATLAS_SIZE
    PRODUCTION_RELATIVE_ROOT = relative_root
    CELL_SIZE = cell_size
    ATLAS_SIZE = atlas_size


def _apply_two_k_mode() -> None:
    _apply_resolution_mode(TWO_K_PRODUCTION_RELATIVE_ROOT, TWO_K_CELL_SIZE, TWO_K_ATLAS_SIZE)


def _apply_one_k_mode() -> None:
    _apply_resolution_mode(ONE_K_PRODUCTION_RELATIVE_ROOT, ONE_K_CELL_SIZE, ONE_K_ATLAS_SIZE)


def _with_variant_suffix(entries: list, suffix: str) -> list:
    """Rename entries so the import writes NEW sibling assets (4K originals stay untouched).

    The suffix is inserted before the action suffix so the names match the animation
    presentation's resolution convention: "character_00_hero_idle" -> "character_00_hero_2k_idle".
    """
    if not suffix:
        return entries
    variants = []
    for entry in entries:
        base, _, action = entry.asset_id.rpartition("_")
        variant_id = f"{base}{suffix}_{action}" if action else f"{entry.asset_id}{suffix}"
        variants.append(entry._replace(
            asset_id=variant_id,
            texture_name=f"T_{variant_id}_atlas",
            flipbook_name=f"FB_{variant_id}" if entry.create_idle_flipbook else "",
        ))
    return variants


def _resolve_replace_existing(
    restore: bool,
    two_k: bool,
    one_k: bool,
    variant_suffix: str,
    replace_existing_variants: bool,
) -> bool:
    """Keep sibling imports non-destructive unless replacement is explicit."""
    if variant_suffix:
        return bool(replace_existing_variants)
    return bool(restore or two_k or one_k)


class AnimationAssetEntry(NamedTuple):
    asset_id: str
    manifest_path: Path
    atlas_path: Path
    texture_name: str
    create_idle_flipbook: bool
    flipbook_name: str


def validate_manifest(asset_id: str, payload: dict, require_atlas_file: bool = True) -> Path:
    expected = {
        "frameCount": FRAME_COUNT,
        "canvasSize": CELL_SIZE,
        "fps": int(FRAMES_PER_SECOND),
    }
    for key, value in expected.items():
        if int(payload.get(key, -1)) != value:
            raise RuntimeError(f"{asset_id} manifest {key} drifted: {payload.get(key)!r} != {value}")
    grid = payload.get("atlasGrid") or {}
    grid_expected = {
        "columns": ATLAS_COLUMNS,
        "rows": ATLAS_ROWS,
        "cellWidth": CELL_SIZE,
        "cellHeight": CELL_SIZE,
    }
    for key, value in grid_expected.items():
        if int(grid.get(key, -1)) != value:
            raise RuntimeError(f"{asset_id} manifest atlasGrid.{key} drifted: {grid.get(key)!r} != {value}")
    atlas_path = Path(str(payload.get("atlas", ""))).resolve()
    if atlas_path.name != f"{asset_id}_atlas.png":
        raise RuntimeError(f"{asset_id} atlas name is unexpected: {atlas_path}")
    if require_atlas_file and not atlas_path.is_file():
        raise RuntimeError(f"{asset_id} atlas is missing: {atlas_path}")
    return atlas_path


def discover_animation_assets(production_root: Path) -> list[AnimationAssetEntry]:
    entries: list[AnimationAssetEntry] = []
    for asset_dir in sorted(path for path in production_root.iterdir() if path.is_dir()):
        manifest_path = asset_dir / "manifest.json"
        if not manifest_path.is_file():
            continue
        asset_id = asset_dir.name
        payload = json.loads(manifest_path.read_text(encoding="utf-8"))
        atlas_path = validate_manifest(asset_id, payload)
        create_idle = asset_id.endswith("_idle")
        entries.append(
            AnimationAssetEntry(
                asset_id=asset_id,
                manifest_path=manifest_path.resolve(),
                atlas_path=atlas_path,
                texture_name=f"T_{asset_id}_atlas",
                create_idle_flipbook=create_idle,
                flipbook_name=f"FB_{asset_id}" if create_idle else "",
            )
        )
    return entries


def _require_unreal() -> None:
    if unreal is None:
        raise RuntimeError("production battle animation import requires UE editor Python")


def _project_root() -> Path:
    return Path(str(unreal.Paths.project_dir())).resolve()


def _ensure_directory(path: str) -> None:
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        if not unreal.EditorAssetLibrary.make_directory(path):
            raise RuntimeError(f"could not create animation asset directory: {path}")


def _save(asset: object) -> None:
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset):
        raise RuntimeError(f"could not save animation asset: {asset.get_path_name()}")


def _texture_size(texture: object) -> tuple[int, int]:
    for width_name, height_name in (("blueprint_get_size_x", "blueprint_get_size_y"), ("get_size_x", "get_size_y")):
        width = getattr(texture, width_name, None)
        height = getattr(texture, height_name, None)
        if callable(width) and callable(height):
            return int(width()), int(height())
    raise RuntimeError("could not query animation texture dimensions")


def _configure_texture(texture: object) -> None:
    texture.set_editor_property(
        "mip_gen_settings",
        getattr(unreal.TextureMipGenSettings, TEXTURE_MIP_SETTING),
    )
    texture.set_editor_property(
        "filter",
        getattr(unreal.TextureFilter, TEXTURE_FILTER_SETTING),
    )
    texture.set_editor_property(
        "compression_settings",
        getattr(unreal.TextureCompressionSettings, TEXTURE_COMPRESSION_SETTING),
    )
    texture.set_editor_property("srgb", TEXTURE_SRGB)


def _import_texture(entry: AnimationAssetEntry, replace_existing: bool = False):
    asset_path = f"{ATLAS_ASSET_DIR}/{entry.texture_name}"
    texture = unreal.EditorAssetLibrary.load_asset(asset_path) if unreal.EditorAssetLibrary.does_asset_exist(asset_path) else None
    if replace_existing:
        task = unreal.AssetImportTask()
        task.filename = str(entry.atlas_path)
        task.destination_path = ATLAS_ASSET_DIR
        task.destination_name = entry.texture_name
        task.automated = True
        task.save = False
        task.replace_existing = True
        task.replace_existing_settings = True
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        texture = unreal.EditorAssetLibrary.load_asset(asset_path)
        if texture is None or not isinstance(texture, unreal.Texture2D):
            raise RuntimeError(f"could not re-import animation Texture2D: {asset_path}")
        if _texture_size(texture) != (ATLAS_SIZE, ATLAS_SIZE):
            raise RuntimeError(f"{entry.asset_id} atlas must be {ATLAS_SIZE}x{ATLAS_SIZE}")
        _configure_texture(texture)
        _save(texture)
        return texture, False
    created = texture is None
    if texture is None:
        task = unreal.AssetImportTask()
        task.filename = str(entry.atlas_path)
        task.destination_path = ATLAS_ASSET_DIR
        task.destination_name = entry.texture_name
        task.automated = True
        task.save = False
        task.replace_existing = False
        task.replace_existing_settings = False
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        texture = unreal.EditorAssetLibrary.load_asset(asset_path)
    if texture is None or not isinstance(texture, unreal.Texture2D):
        raise RuntimeError(f"could not import animation Texture2D: {asset_path}")
    if _texture_size(texture) != (ATLAS_SIZE, ATLAS_SIZE):
        raise RuntimeError(f"{entry.asset_id} atlas must be {ATLAS_SIZE}x{ATLAS_SIZE}")
    _configure_texture(texture)
    _save(texture)
    return texture, created


def _create_or_load(name: str, directory: str, asset_class: object, factory: object):
    path = f"{directory}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        asset = unreal.EditorAssetLibrary.load_asset(path)
        if asset is None:
            raise RuntimeError(f"could not load animation asset: {path}")
        return asset, False
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, directory, asset_class, factory)
    if asset is None:
        raise RuntimeError(f"could not create animation asset: {path}")
    return asset, True


def _configure_sprite(sprite: object, texture: object, frame_index: int) -> None:
    column = frame_index % ATLAS_COLUMNS
    row = frame_index // ATLAS_COLUMNS
    sprite.set_editor_property("source_texture", texture)
    sprite.set_editor_property("source_uv", unreal.Vector2D(float(column * CELL_SIZE), float(row * CELL_SIZE)))
    sprite.set_editor_property("source_dimension", unreal.Vector2D(float(CELL_SIZE), float(CELL_SIZE)))
    sprite.set_editor_property("source_texture_dimension", unreal.Vector2D(float(ATLAS_SIZE), float(ATLAS_SIZE)))
    sprite.set_editor_property("pixels_per_unreal_unit", PIXELS_PER_UNREAL_UNIT)
    sprite.set_editor_property("pivot_mode", unreal.SpritePivotMode.BOTTOM_CENTER)
    sprite.set_editor_property("custom_pivot_point", unreal.Vector2D(CELL_SIZE * 0.5, float(CELL_SIZE)))


def _create_idle_flipbook(entry: AnimationAssetEntry, texture: object) -> tuple[object, int, bool]:
    sprite_dir = f"{IDLE_SPRITE_ROOT}/{entry.asset_id}"
    _ensure_directory(sprite_dir)
    sprites = []
    created_sprites = 0
    for frame_index in range(FRAME_COUNT):
        sprite, created = _create_or_load(
            f"SPR_{entry.asset_id}_{frame_index:02d}",
            sprite_dir,
            unreal.PaperSprite,
            unreal.PaperSpriteFactory(),
        )
        _configure_sprite(sprite, texture, frame_index)
        _save(sprite)
        sprites.append(sprite)
        created_sprites += int(created)
    flipbook, created_flipbook = _create_or_load(
        entry.flipbook_name,
        IDLE_FLIPBOOK_DIR,
        unreal.PaperFlipbook,
        unreal.PaperFlipbookFactory(),
    )
    keyframes = []
    for sprite in sprites:
        keyframe = unreal.PaperFlipbookKeyFrame()
        keyframe.set_editor_property("sprite", sprite)
        keyframe.set_editor_property("frame_run", 1)
        keyframes.append(keyframe)
    flipbook.set_editor_property("frames_per_second", FRAMES_PER_SECOND)
    flipbook.set_editor_property("key_frames", keyframes)
    invalidate = getattr(flipbook, "invalidate_cached_data", None)
    if callable(invalidate):
        invalidate()
    _save(flipbook)
    return flipbook, created_sprites, bool(created_flipbook)


def _select_entries(entries: list, asset_ids: set[str] | None, limit: int, offset: int = 0) -> list:
    """Filter by BASE asset ids and limit BEFORE any variant-suffix rename."""
    if asset_ids:
        entries = [entry for entry in entries if entry.asset_id in asset_ids]
    if offset > 0:
        entries = entries[offset:]
    if limit > 0:
        entries = entries[:limit]
    return entries


def import_production(
    asset_ids: set[str] | None = None,
    textures_only: bool = False,
    limit: int = 0,
    two_k: bool = False,
    one_k: bool = False,
    restore: bool = False,
    variant_suffix: str = "",
    offset: int = 0,
    replace_existing_variants: bool = False,
) -> dict:
    _require_unreal()
    if two_k and one_k:
        raise RuntimeError("two-k and one-k modes are mutually exclusive")
    if restore:
        # Default 4K root/constants; replace the current (possibly downscaled) assets.
        pass
    elif two_k:
        _apply_two_k_mode()
    elif one_k:
        _apply_one_k_mode()
    replace_existing = _resolve_replace_existing(
        restore,
        two_k,
        one_k,
        variant_suffix,
        replace_existing_variants,
    )
    entries = discover_animation_assets(_project_root() / PRODUCTION_RELATIVE_ROOT)
    entries = _select_entries(entries, asset_ids, limit, offset)
    if variant_suffix:
        entries = _with_variant_suffix(entries, variant_suffix)
    for path in (ATLAS_ASSET_DIR, IDLE_SPRITE_ROOT, IDLE_FLIPBOOK_DIR):
        _ensure_directory(path)

    results = []
    for entry in entries:
        texture, created_texture = _import_texture(entry, replace_existing=replace_existing)
        item = {
            "asset_id": entry.asset_id,
            "texture": texture.get_path_name(),
            "created_texture": bool(created_texture),
            "flipbook": "",
            "created_sprites": 0,
            "created_flipbook": False,
        }
        if entry.create_idle_flipbook and not textures_only:
            flipbook, created_sprites, created_flipbook = _create_idle_flipbook(entry, texture)
            item.update(
                flipbook=flipbook.get_path_name(),
                created_sprites=created_sprites,
                created_flipbook=created_flipbook,
            )
        results.append(item)
        # Release UE object references immediately: batch drivers GC between chunks, and
        # retaining every texture/sprites of a chunk is what OOM-ed 16 GB machines before.
        texture = None
        flipbook = None
    return {
        "ok": True,
        "asset_root": ATLAS_ASSET_DIR,
        "two_k": bool(two_k),
        "one_k": bool(one_k),
        "restore": bool(restore),
        "variant_suffix": variant_suffix,
        "replace_existing_variants": bool(replace_existing_variants),
        "requested_count": len(entries),
        "imported": results,
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--asset-id", action="append", default=[])
    parser.add_argument("--textures-only", action="store_true")
    parser.add_argument("--limit", type=int, default=0)
    parser.add_argument("--two-k", action="store_true")
    parser.add_argument("--one-k", action="store_true")
    parser.add_argument("--restore", action="store_true")
    parser.add_argument("--variant-suffix", default="")
    parser.add_argument("--offset", type=int, default=0)
    parser.add_argument("--replace-existing-variants", action="store_true")
    args = parser.parse_args()
    print(json.dumps(
        import_production(
            set(args.asset_id),
            args.textures_only,
            args.limit,
            args.two_k,
            args.one_k,
            args.restore,
            args.variant_suffix,
            args.offset,
            args.replace_existing_variants,
        ),
        ensure_ascii=False,
    ))


if __name__ == "__main__":
    main()
