"""Assemble isolated eight-direction PartyDeck Paper2D character visuals.

The reviewed source Texture2D assets already exist below
/Game/GameXXK/Sprites/Generated/PartyDeck. This script only creates or updates
PaperSprite and PaperFlipbook assets below the PartyDeckNPC and
PartyDeckPartners roots. It never imports, deletes, or replaces texture assets.
"""

from __future__ import annotations

import argparse
import json
from typing import Any

try:
    import unreal
except ImportError:  # Keep the deterministic plan testable outside Unreal.
    unreal = None


TEXTURE_ROOT = "/Game/GameXXK/Sprites/Generated/PartyDeck"
NPC_ROOT = "/Game/GameXXK/Characters/PartyDeckNPC"
PARTNER_ROOT = "/Game/GameXXK/Characters/PartyDeckPartners"
CELL_WIDTH = 171.0
CELL_HEIGHT = 205.0
WALK_FRAME_COUNT = 6
WALK_FPS = 8.0
IDLE_FPS = 1.0
PIXELS_PER_UNREAL_UNIT = 1.0
IDLE_TEXTURE_SIZE = (171, 1640)
WALK_TEXTURE_SIZE = (1026, 1640)

DIRECTIONS = (
    {"name": "South", "row": 0},
    {"name": "SouthWest", "row": 1},
    {"name": "West", "row": 2},
    {"name": "NorthWest", "row": 3},
    {"name": "North", "row": 4},
    {"name": "NorthEast", "row": 5},
    {"name": "East", "row": 6},
    {"name": "SouthEast", "row": 7},
)

NPC_NAMES = (
    "TusiChief",
    "SongJinBao",
    "YueBai",
    "ZhouGuangZu",
    "JinGui",
    "QiongMeiEr",
)
PARTNER_ROLES = (
    "Blade",
    "Guard",
    "Healer",
    "Hunter",
    "Sorcerer",
    "FormationMaster",
)


def _target_specs() -> tuple[dict[str, str], ...]:
    npc_specs = tuple(
        {
            "target_id": f"Npc.{name}",
            "asset_root": f"{NPC_ROOT}/{name}",
            "texture_prefix": f"T_PartyDeck_Npc_{name}",
            "sprite_prefix": f"SPR_PartyDeckNPC_{name}",
            "flipbook_prefix": f"FB_PartyDeckNPC_{name}",
        }
        for name in NPC_NAMES
    )
    partner_specs = tuple(
        {
            "target_id": f"PartnerRole.{role}",
            "asset_root": f"{PARTNER_ROOT}/{role}",
            "texture_prefix": f"T_PartyDeck_PartnerRole_{role}",
            "sprite_prefix": f"SPR_PartyDeckPartner_{role}",
            "flipbook_prefix": f"FB_PartyDeckPartner_{role}",
        }
        for role in PARTNER_ROLES
    )
    return npc_specs + partner_specs


def build_assembly_plan() -> dict[str, Any]:
    """Return the deterministic, write-free Paper2D assembly plan."""

    targets: list[dict[str, Any]] = []
    for spec in _target_specs():
        asset_root = spec["asset_root"]
        targets.append(
            {
                **spec,
                "sprite_dir": f"{asset_root}/Sprites",
                "flipbook_dir": f"{asset_root}/Flipbooks",
                "idle_texture": f"{TEXTURE_ROOT}/{spec['texture_prefix']}_Idle8Dir",
                "walk_texture": f"{TEXTURE_ROOT}/{spec['texture_prefix']}_Walk8Dir",
                "default_idle_flipbook": f"{asset_root}/Flipbooks/{spec['flipbook_prefix']}_Idle_South",
                "directions": [entry["name"] for entry in DIRECTIONS],
                "idle_frame_count": 1,
                "walk_frame_count": WALK_FRAME_COUNT,
                "sprite_count": len(DIRECTIONS) * (WALK_FRAME_COUNT + 1),
                "flipbook_count": len(DIRECTIONS) * 2,
            }
        )
    return {
        "ok": True,
        "texture_root": TEXTURE_ROOT,
        "npc_root": NPC_ROOT,
        "partner_root": PARTNER_ROOT,
        "directions": [entry["name"] for entry in DIRECTIONS],
        "targets": targets,
        "sprite_count": sum(int(target["sprite_count"]) for target in targets),
        "flipbook_count": sum(int(target["flipbook_count"]) for target in targets),
        "default_flipbook_count": len(targets),
    }


def _require_unreal() -> None:
    if unreal is None:
        raise RuntimeError("PartyDeck Paper2D assembly requires UE Python through UnrealEditor-Cmd.")


def _ensure_directory(path: str) -> bool:
    if unreal.EditorAssetLibrary.does_directory_exist(path):
        return False
    if not unreal.EditorAssetLibrary.make_directory(path):
        raise RuntimeError(f"could not create PartyDeck directory: {path}")
    return True


def _create_or_load_asset(asset_name: str, package_path: str, asset_class: object, factory: object):
    asset_path = f"{package_path}/{asset_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        asset = unreal.EditorAssetLibrary.load_asset(asset_path)
        if asset is None:
            raise RuntimeError(f"could not load existing PartyDeck asset: {asset_path}")
        if asset.get_class().get_name() != asset_class.static_class().get_name():
            raise RuntimeError(f"PartyDeck asset has the wrong class: {asset_path}")
        return asset, False
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(asset_name, package_path, asset_class, factory)
    if asset is None:
        raise RuntimeError(f"could not create PartyDeck asset: {asset_path}")
    return asset, True


def _set_property(asset: object, property_name: str, value: object) -> None:
    try:
        asset.set_editor_property(property_name, value)
    except Exception as exc:
        raise RuntimeError(f"could not set {property_name} on {asset}: {exc}") from exc


def _texture_size(texture: object) -> tuple[int, int]:
    for width_name, height_name in (("blueprint_get_size_x", "blueprint_get_size_y"), ("get_size_x", "get_size_y")):
        width = getattr(texture, width_name, None)
        height = getattr(texture, height_name, None)
        if callable(width) and callable(height):
            return int(width()), int(height())
    source = texture.get_editor_property("source")
    width = getattr(source, "get_size_x", None)
    height = getattr(source, "get_size_y", None)
    if callable(width) and callable(height):
        return int(width()), int(height())
    raise RuntimeError(f"could not read texture dimensions: {texture}")


def _load_texture(path: str, expected_size: tuple[int, int]) -> object:
    texture = unreal.EditorAssetLibrary.load_asset(path)
    if texture is None:
        raise RuntimeError(f"reviewed PartyDeck texture is missing: {path}")
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError(f"PartyDeck source is not a Texture2D: {path}")
    actual_size = _texture_size(texture)
    if actual_size != expected_size:
        raise RuntimeError(f"PartyDeck texture dimensions drifted at {path}: got {actual_size}, expected {expected_size}")
    return texture


def _configure_sprite(sprite: object, texture: object, row: int, frame: int, texture_size: tuple[int, int]) -> None:
    _set_property(sprite, "source_texture", texture)
    _set_property(sprite, "source_uv", unreal.Vector2D(CELL_WIDTH * frame, CELL_HEIGHT * row))
    _set_property(sprite, "source_dimension", unreal.Vector2D(CELL_WIDTH, CELL_HEIGHT))
    _set_property(sprite, "source_texture_dimension", unreal.Vector2D(float(texture_size[0]), float(texture_size[1])))
    _set_property(sprite, "pixels_per_unreal_unit", PIXELS_PER_UNREAL_UNIT)
    _set_property(sprite, "pivot_mode", unreal.SpritePivotMode.BOTTOM_CENTER)
    _set_property(sprite, "custom_pivot_point", unreal.Vector2D(CELL_WIDTH * 0.5, CELL_HEIGHT))


def _configure_flipbook(flipbook: object, sprites: list[object], frames_per_second: float) -> None:
    keyframes = []
    for sprite in sprites:
        keyframe = unreal.PaperFlipbookKeyFrame()
        keyframe.set_editor_property("sprite", sprite)
        keyframe.set_editor_property("frame_run", 1)
        keyframes.append(keyframe)
    _set_property(flipbook, "frames_per_second", frames_per_second)
    _set_property(flipbook, "key_frames", keyframes)
    invalidate = getattr(flipbook, "invalidate_cached_data", None)
    if callable(invalidate):
        invalidate()


def _save_asset(asset: object) -> None:
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset):
        raise RuntimeError(f"could not save PartyDeck asset: {asset.get_path_name()}")


def _assemble_target(target: dict[str, Any]) -> dict[str, Any]:
    created_directories = []
    for directory in (target["asset_root"], target["sprite_dir"], target["flipbook_dir"]):
        if _ensure_directory(str(directory)):
            created_directories.append(str(directory))

    idle_texture = _load_texture(str(target["idle_texture"]), IDLE_TEXTURE_SIZE)
    walk_texture = _load_texture(str(target["walk_texture"]), WALK_TEXTURE_SIZE)
    idle_sprites: dict[str, object] = {}
    walk_sprites: dict[str, list[object]] = {}
    created_sprites = 0
    created_flipbooks = 0

    for direction in DIRECTIONS:
        direction_name = str(direction["name"])
        row = int(direction["row"])
        idle_sprite, created = _create_or_load_asset(
            f"{target['sprite_prefix']}_Idle_{direction_name}_00",
            str(target["sprite_dir"]),
            unreal.PaperSprite,
            unreal.PaperSpriteFactory(),
        )
        _configure_sprite(idle_sprite, idle_texture, row, 0, IDLE_TEXTURE_SIZE)
        _save_asset(idle_sprite)
        idle_sprites[direction_name] = idle_sprite
        created_sprites += int(created)

        direction_walk_sprites = []
        for frame in range(WALK_FRAME_COUNT):
            walk_sprite, created = _create_or_load_asset(
                f"{target['sprite_prefix']}_Walk_{direction_name}_{frame:02d}",
                str(target["sprite_dir"]),
                unreal.PaperSprite,
                unreal.PaperSpriteFactory(),
            )
            _configure_sprite(walk_sprite, walk_texture, row, frame, WALK_TEXTURE_SIZE)
            _save_asset(walk_sprite)
            direction_walk_sprites.append(walk_sprite)
            created_sprites += int(created)
        walk_sprites[direction_name] = direction_walk_sprites

    for direction in DIRECTIONS:
        direction_name = str(direction["name"])
        idle_flipbook, created = _create_or_load_asset(
            f"{target['flipbook_prefix']}_Idle_{direction_name}",
            str(target["flipbook_dir"]),
            unreal.PaperFlipbook,
            unreal.PaperFlipbookFactory(),
        )
        _configure_flipbook(idle_flipbook, [idle_sprites[direction_name]], IDLE_FPS)
        _save_asset(idle_flipbook)
        created_flipbooks += int(created)

        walk_flipbook, created = _create_or_load_asset(
            f"{target['flipbook_prefix']}_Walk_{direction_name}",
            str(target["flipbook_dir"]),
            unreal.PaperFlipbook,
            unreal.PaperFlipbookFactory(),
        )
        _configure_flipbook(walk_flipbook, walk_sprites[direction_name], WALK_FPS)
        _save_asset(walk_flipbook)
        created_flipbooks += int(created)

    return {
        "target_id": target["target_id"],
        "asset_root": target["asset_root"],
        "default_idle_flipbook": target["default_idle_flipbook"],
        "created_directories": created_directories,
        "created_sprite_count": created_sprites,
        "created_flipbook_count": created_flipbooks,
        "sprite_count": target["sprite_count"],
        "flipbook_count": target["flipbook_count"],
    }


def assemble_party_deck_characters() -> dict[str, Any]:
    """Create or update only isolated PartyDeck PaperSprite and PaperFlipbook assets."""

    _require_unreal()
    plan = build_assembly_plan()
    targets = [_assemble_target(target) for target in plan["targets"]]
    return {
        "ok": True,
        "texture_root": TEXTURE_ROOT,
        "npc_root": NPC_ROOT,
        "partner_root": PARTNER_ROOT,
        "directions": plan["directions"],
        "target_count": len(targets),
        "sprite_count": plan["sprite_count"],
        "flipbook_count": plan["flipbook_count"],
        "created_sprite_count": sum(int(target["created_sprite_count"]) for target in targets),
        "created_flipbook_count": sum(int(target["created_flipbook_count"]) for target in targets),
        "targets": targets,
    }


def main(argv: list[str] | None = None) -> dict[str, Any]:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--execute", action="store_true", help="Create or update the isolated PartyDeck Paper2D assets.")
    args = parser.parse_args(argv)
    result = assemble_party_deck_characters() if args.execute else build_assembly_plan()
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return result


if __name__ == "__main__":
    main()
