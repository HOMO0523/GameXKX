"""Import the approved low-saturation battle-status icon set.

This importer uses the 13 reviewed one-accent v4 sources, the matching
BlockShield addition, and the seven approved route-status alpha glyphs. It creates a new Battle/StatusIcons texture
tree and does not touch any artist-tuned level, character, or legacy UI asset.
Run through Unreal using this project's .uproject path.
"""

from __future__ import annotations

import json
import os
from pathlib import Path
import struct
import sys

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SOURCE_DIR = PROJECT_ROOT / "SourceArt" / "UI" / "Battle" / "StatusIcons"
ROUTE_STATUS_SOURCE_DIR = PROJECT_ROOT / "SourceArt" / "Generated" / "Draft" / "V1" / "Status"
DESTINATION = "/Game/GameXXK/UI/Battle/StatusIcons"

IMPORTS: tuple[tuple[str, str], ...] = (
    ("battle_status_armor_shield_inkflat_v4.png", "T_BattleStatus_ArmorShield"),
    ("battle_status_momentum_seal_inkflat_v4.png", "T_BattleStatus_MomentumSeal"),
    ("battle_status_agility_wing_inkflat_v4.png", "T_BattleStatus_AgilityWing"),
    ("battle_status_vulnerability_mask_inkflat_v4.png", "T_BattleStatus_VulnerabilityMask"),
    ("battle_status_bleed_drop_inkflat_v4.png", "T_BattleStatus_BleedDrop"),
    ("battle_status_poison_vial_inkflat_v4.png", "T_BattleStatus_PoisonVial"),
    ("battle_status_burn_flame_inkflat_v4.png", "T_BattleStatus_BurnFlame"),
    ("battle_status_mark_target_inkflat_v4.png", "T_BattleStatus_MarkTarget"),
    ("battle_status_guard_shield_inkflat_v4.png", "T_BattleStatus_GuardShield"),
    ("battle_status_rot_spiral_inkflat_v4.png", "T_BattleStatus_RotSpiral"),
    ("battle_status_immunity_talisman_inkflat_v4.png", "T_BattleStatus_ImmunityTalisman"),
    ("battle_status_tactic_seal_inkflat_v4.png", "T_BattleStatus_TacticSeal"),
    ("battle_status_terrain_redirect_inkflat_v4.png", "T_BattleStatus_TerrainAndRedirect"),
)
BLOCK_STATUS_IMPORTS: tuple[tuple[str, str], ...] = (
    ("battle_status_block_shield_inkflat_v4.png", "T_BattleStatus_BlockShield"),
)
ROUTE_STATUS_IMPORTS: tuple[tuple[str, str], ...] = (
    ("status_glyph_medicine_draft_v1_alpha.png", "T_BattleStatus_MedicineHerbs"),
    ("status_glyph_weak_drooping_broken_blade_draft_v1_alpha.png", "T_BattleStatus_WeakBrokenBlade"),
    ("status_glyph_wealth_square_coin_draft_v1_alpha.png", "T_BattleStatus_WealthCoin"),
    ("status_glyph_rage_horn_flame_draft_v1_alpha.png", "T_BattleStatus_RageFlame"),
    ("status_glyph_prey_ink_target_draft_v1_alpha.png", "T_BattleStatus_PreyTargetEye"),
    ("status_glyph_charge_spiral_horn_draft_v1_alpha.png", "T_BattleStatus_ChargeSpiralHorn"),
    ("status_glyph_counter_return_hook_blade_draft_v1_alpha.png", "T_BattleStatus_CounterHookBlade"),
)
EXPECTED_SIZE = (1254, 1254)
ROUTE_STATUS_EXPECTED_SIZE = (512, 512)


def _read_png_dimensions(source: Path) -> tuple[int, int]:
    header = source.read_bytes()[:24]
    if len(header) != 24 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        raise RuntimeError(f"expected PNG source: {source}")
    return struct.unpack(">II", header[16:24])


def _verify_source(source: Path, expected_size: tuple[int, int]) -> None:
    if not source.is_file():
        raise RuntimeError(f"missing approved battle-status source: {source}")
    if _read_png_dimensions(source) != expected_size:
        raise RuntimeError(f"unexpected battle-status source size: {source}")


def _try_set(texture: unreal.Texture2D, name: str, value: object) -> None:
    try:
        texture.set_editor_property(name, value)
    except (AttributeError, RuntimeError):
        pass


def _configure_texture(texture: unreal.Texture2D) -> None:
    _try_set(texture, "mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    _try_set(texture, "compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    _try_set(texture, "lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    _try_set(texture, "filter", unreal.TextureFilter.TF_BILINEAR)
    _try_set(texture, "address_x", unreal.TextureAddress.TA_CLAMP)
    _try_set(texture, "address_y", unreal.TextureAddress.TA_CLAMP)
    _try_set(texture, "srgb", True)
    _try_set(texture, "never_stream", True)
    _try_set(texture, "compression_no_alpha", False)


def _import_texture(
    source_dir: Path,
    filename: str,
    asset_name: str,
    expected_size: tuple[int, int],
    *,
    replace_existing: bool = True,
) -> str:
    source = source_dir / filename
    _verify_source(source, expected_size)
    if not unreal.EditorAssetLibrary.does_directory_exist(DESTINATION):
        unreal.EditorAssetLibrary.make_directory(DESTINATION)

    asset_path = f"{DESTINATION}/{asset_name}"
    if not replace_existing and unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        raise RuntimeError(f"refusing to overwrite existing battle-status texture: {asset_path}")

    task = unreal.AssetImportTask()
    task.filename = str(source)
    task.destination_path = DESTINATION
    task.destination_name = asset_name
    task.automated = True
    task.replace_existing = replace_existing
    task.replace_existing_settings = replace_existing
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    texture = (
        unreal.EditorAssetLibrary.load_asset(f"{asset_path}.{asset_name}")
        or unreal.EditorAssetLibrary.load_asset(asset_path)
    )
    if not isinstance(texture, unreal.Texture2D):
        loaded_class = texture.get_class().get_name() if texture else "None"
        raise RuntimeError(f"failed to import Texture2D: {asset_path}; class={loaded_class}")
    _configure_texture(texture)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
    return texture.get_path_name()


def main() -> None:
    import_mode = os.environ.get("GAMEXXK_BATTLE_STATUS_IMPORT_MODE", "").strip().lower()
    route_only = "--route-only" in sys.argv[1:] or import_mode == "route-only"
    block_only = "--block-only" in sys.argv[1:] or import_mode == "block-only"
    if route_only and block_only:
        raise RuntimeError("--route-only and --block-only are mutually exclusive")

    if block_only:
        imported = [
            _import_texture(
                SOURCE_DIR,
                filename,
                asset_name,
                EXPECTED_SIZE,
                replace_existing=False,
            )
            for filename, asset_name in BLOCK_STATUS_IMPORTS
        ]
    else:
        imported = [] if route_only else [
            _import_texture(SOURCE_DIR, filename, asset_name, EXPECTED_SIZE)
            for filename, asset_name in (*IMPORTS, *BLOCK_STATUS_IMPORTS)
        ]
        imported.extend(
            _import_texture(ROUTE_STATUS_SOURCE_DIR, filename, asset_name, ROUTE_STATUS_EXPECTED_SIZE)
            for filename, asset_name in ROUTE_STATUS_IMPORTS
        )
    unreal.EditorAssetLibrary.save_directory(DESTINATION, only_if_is_dirty=False, recursive=True)
    print(json.dumps({"ok": True, "imported_count": len(imported), "imported": imported}, ensure_ascii=False))


if __name__ == "__main__":
    main()
