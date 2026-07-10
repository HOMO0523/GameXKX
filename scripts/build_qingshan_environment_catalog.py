"""Build the deterministic Qingshan environment concept catalog."""

from __future__ import annotations

import argparse
import json
import math
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from qingshan_environment_assets import (
    ALLOWED_VERSIONS,
    BATCH_CALL_CAPS,
    BATCH_COUNTS,
    EXPECTED_VIEWS,
    CatalogError,
    derive_manifest_generation,
    validate_asset,
    validate_catalog,
    verify_asset_evidence,
    write_files_transactionally,
)


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_ROOT = PROJECT_ROOT / "SourceAssets" / "TownPCG" / "QingshanEnvironment"

CATALOG = {
    "REGISTRY": ["BLD_QS_L_A_MARKET_SHOP"],
    "B0": [
        "REF_QS_ENV_STYLE_LOCK",
        "REF_QS_SCALE_LINEUP",
        "REF_QS_CAMERA_ROUTE",
        "REF_QS_SURFACE_PALETTE",
    ],
    "B1": [
        "BLD_QS_M_A_INN",
        "BLD_QS_S_A_HOUSE",
        "LMK_QS_GATE_NORTH",
        "LMK_QS_BRIDGE_MAIN",
        "LMK_QS_DOCK_SOUTH",
        "ENV_QS_MTN_NEAR_A_RIDGE",
        "P2D_QS_MTN_FAR_A_MIST",
        "ENV_QS_ROCK_KIT_A_FIELD",
        "P2D_QS_TREE_PINE_A",
        "P2D_QS_SHRUB_A",
        "SRF_QS_GROUND_EARTH_A",
        "SRF_QS_WATER_RIVER_A",
    ],
    "B2": [
        "BLD_QS_M_B_STREET_SHOP",
        "BLD_QS_S_B_WORKSHOP",
        "BLD_QS_S_C_RIVER_HUT",
        "ENV_QS_MTN_NEAR_B_CLIFF",
        "P2D_QS_MTN_FAR_B_SILHOUETTE",
        "ENV_QS_ROCK_KIT_B_RIVERBANK",
        "P2D_QS_TREE_BROADLEAF_A",
        "P2D_QS_TREE_BAMBOO_A",
        "P2D_QS_GRASS_TUFT_A",
        "P2D_QS_FLOWER_WILD_A",
        "SRF_QS_GROUND_GRASS_A",
        "SRF_QS_ROAD_STONE_A",
        "SRF_QS_RIVERBANK_BLEND_A",
    ],
    "B3": [
        "KIT_QS_FENCE_WOOD_A",
        "KIT_QS_PATH_STEPPING_STONE_A",
        "PROP_QS_MARKET_KIT_A",
        "PROP_QS_DOCK_KIT_A",
        "PROP_QS_STREET_KIT_A",
    ],
}

COMMON_NEGATIVE = [
    "photorealistic",
    "modern object",
    "readable text",
    "logo",
    "watermark",
    "neon green",
    "mirror symmetry",
    "cropped subject",
    "PBR plastic shine",
]
COMMON_MATERIAL = ["QS_InkToon_v1", "two_to_three_value_bands", "dry_brush_edges"]
COMMON_PALETTE = [
    "warm_off_white",
    "dark_teal_ink",
    "jade_green",
    "blue_gray",
    "warm_ochre",
]

GENERATED_BOARD_PATHS = {
    "REF_QS_ENV_STYLE_LOCK": "style/boards/REF_QS_ENV_STYLE_LOCK__board__v001.png",
    "REF_QS_SCALE_LINEUP": "style/boards/REF_QS_SCALE_LINEUP__board__v001.png",
}

STYLE_REFERENCE_ROLES = {
    "style_env_day.jpeg": "environment_style",
    "style_env_night.jpeg": "environment_atmosphere",
    "style_character_scale.jpeg": "character_scale",
    "style_nature_group.jpeg": "shape_language",
    "style_creature_warm.jpeg": "warm_palette",
    "style_creature_mass.jpeg": "mass_and_outline",
    "layout_dense_foliage.jpg": "layout_density_only",
}


class CatalogBuildError(RuntimeError):
    """Raised when catalog source facts are missing or generated JSON drifts."""


@dataclass(frozen=True)
class AssetSpec:
    category: str
    dimensions: tuple[float, float, float] | None
    zones: tuple[str, ...]
    role: str
    display_name: str
    primary_request: str
    accent: str
    silhouettes: tuple[str, ...]


SPECS = {
    "BLD_QS_L_A_MARKET_SHOP": AssetSpec(
        "registry", None, ("market",), "landmark_shop", "青山镇最大商铺 L-A（现有登记）",
        "Register the existing approved largest shop without new generation",
        "warm_red_brown", ("largest_shop_anchor", "existing_approved_mass"),
    ),
    "REF_QS_ENV_STYLE_LOCK": AssetSpec(
        "reference", (160, 100, 50), ("global",), "style_board", "青山镇环境风格锁定板",
        "Qingshan ink-cartoon environment style board", "jade_green",
        ("ink_cartoon_town", "layered_mountains", "material_swatches"),
    ),
    "REF_QS_SCALE_LINEUP": AssetSpec(
        "reference", (60, 10, 20), ("global",), "scale_board", "青山镇资产尺度队列板",
        "Character, buildings, vegetation, bridge and mountain scale lineup", "warm_ochre",
        ("consistent_scale", "fixed_camera_readability"),
    ),
    "REF_QS_CAMERA_ROUTE": AssetSpec(
        "reference", (160, 100, 40), ("global",), "route_board", "青山镇玩家镜头路线板",
        "Four fixed-camera views from gate to market, bridge and dock", "blue_green",
        ("right_gate", "gentle_s_route", "asymmetric_clusters"),
    ),
    "REF_QS_SURFACE_PALETTE": AssetSpec(
        "reference", (8, 8, 1), ("global",), "surface_board", "青山镇地表与水体色板",
        "Earth, grass, road, bank, water and rock palette relationship", "earth_ochre",
        ("surface_transitions", "toon_water", "material_relationship"),
    ),
    "BLD_QS_M_A_INN": AssetSpec(
        "building", (4.8, 5.6, 5.2), ("town_interior",), "street_node", "青山镇中型客栈 A",
        "Two-storey inn with warm red-brown asymmetric xieshan roof", "warm_red_brown",
        ("two_storey", "asymmetric_xieshan", "deep_eaves"),
    ),
    "BLD_QS_S_A_HOUSE": AssetSpec(
        "building", (3.6, 4.2, 3.8), ("courtyard",), "small_house", "青山镇小民居 A",
        "Compact warm-white house with indigo double-slope roof", "indigo",
        ("compact_house", "double_slope", "offset_door"),
    ),
    "LMK_QS_GATE_NORTH": AssetSpec(
        "landmark", (24, 6, 12), ("north_gate",), "fixed_anchor", "青山镇北门",
        "Asymmetric Chinese town gate preserving an 18–24m valley opening", "warm_red_brown",
        ("right_screen_gate", "open_valley", "asymmetric_towers"),
    ),
    "LMK_QS_BRIDGE_MAIN": AssetSpec(
        "landmark", (9, 24, 5), ("bridge",), "traversal", "青山镇主桥",
        "Nine-metre-wide timber-and-stone main bridge with clear deck", "dark_timber",
        ("clear_deck", "offset_bridge", "stone_abutments"),
    ),
    "LMK_QS_DOCK_SOUTH": AssetSpec(
        "landmark", (30, 18, 2), ("south_bank",), "traversal", "青山镇南岸码头",
        "Modular dock platform, posts, steps and railing without boats", "dark_timber",
        ("main_platform", "posts_steps_railing", "no_boats"),
    ),
    "ENV_QS_MTN_NEAR_A_RIDGE": AssetSpec(
        "near_mountain", (60, 24, 18), ("perimeter",), "solid_ridge", "青山镇近山长脊 A",
        "Long low near-mountain ridge with two joinable variants", "blue_gray",
        ("long_low_ridge", "gentle_slope", "joinable_pair"),
    ),
    "P2D_QS_MTN_FAR_A_MIST": AssetSpec(
        "far_mountain", (120, 0.1, 45), ("far_backdrop",), "mist_layer", "青山镇雾山远景 A",
        "Pale layered mist-mountain band for the first depth layer", "pale_blue_gray",
        ("mist_band", "offset_peaks", "paper2d_depth_one"),
    ),
    "ENV_QS_ROCK_KIT_A_FIELD": AssetSpec(
        "rock_kit", (2.5, 2, 1.8), ("perimeter", "yard"), "rock_kit", "青山镇旱地山石套件 A",
        "S/M/L dry field-rock trio with distinct silhouettes", "dry_ochre_gray",
        ("small_medium_large", "dry_field_rock", "flat_bases"),
    ),
    "P2D_QS_TREE_PINE_A": AssetSpec(
        "paper2d_plant", (4, 0.05, 7), ("perimeter",), "tree", "青山镇松树族 A",
        "Three pine silhouettes; only A receives five wind poses", "jade_green",
        ("pine_crown", "stable_trunk", "three_variants"),
    ),
    "P2D_QS_SHRUB_A": AssetSpec(
        "paper2d_plant", (2, 0.05, 1.5), ("yard", "road_edge"), "shrub", "青山镇灌木族 A",
        "Three shrub silhouettes; only A receives five wind poses", "jade_green",
        ("root_fixed", "overlapping_leaf_mass", "three_variants"),
    ),
    "SRF_QS_GROUND_EARTH_A": AssetSpec(
        "surface", (4, 4, 0.02), ("town_interior",), "ground", "青山镇泥土地表 A",
        "Seamless compact earth with ink-toon transition edges", "earth_ochre",
        ("compact_earth", "dry_brush_transition", "non_repeating"),
    ),
    "SRF_QS_WATER_RIVER_A": AssetSpec(
        "surface", (8, 8, 0.02), ("river",), "water", "青山镇河流水体 A",
        "Blue-green toon river with flow, shallow/deep bands and foam", "blue_green",
        ("flow_direction", "shallow_deep_bands", "restrained_foam"),
    ),
    "BLD_QS_M_B_STREET_SHOP": AssetSpec(
        "building", (4.2, 5, 4.6), ("town_interior",), "street_shop", "青山镇沿街商铺 B",
        "One-and-a-half-storey shop with ink-green deep eaves", "ink_green",
        ("one_and_half_storey", "deep_eaves", "street_front"),
    ),
    "BLD_QS_S_B_WORKSHOP": AssetSpec(
        "building", (4, 4.8, 3.6), ("side_lane",), "workshop", "青山镇作坊 B",
        "Ochre hard-gable workshop with one attached side shed", "ochre",
        ("hard_gable", "side_shed", "workshop_door"),
    ),
    "BLD_QS_S_C_RIVER_HUT": AssetSpec(
        "building", (3.8, 4.5, 3.5), ("riverbank",), "cottage", "青山镇河岸小屋 C",
        "Teal low-eave riverside cottage on a compact footprint", "teal",
        ("low_eave", "compact_footprint", "river_facing_door"),
    ),
    "ENV_QS_MTN_NEAR_B_CLIFF": AssetSpec(
        "near_mountain", (40, 20, 22), ("perimeter",), "cliff_turn", "青山镇近山陡壁 B",
        "Joinable near-cliff corner pair with broken ink silhouettes", "blue_gray",
        ("cliff_corner", "broken_ink_edge", "joinable_pair"),
    ),
    "P2D_QS_MTN_FAR_B_SILHOUETTE": AssetSpec(
        "far_mountain", (120, 0.1, 60), ("far_backdrop",), "silhouette", "青山镇远山剪影 B",
        "Deeper blue-gray far-peak silhouette layer", "deep_blue_gray",
        ("deep_peak_band", "offset_silhouette", "paper2d_depth_two"),
    ),
    "ENV_QS_ROCK_KIT_B_RIVERBANK": AssetSpec(
        "rock_kit", (2.8, 2.2, 1.5), ("riverbank",), "wet_rock_kit", "青山镇河岸湿石套件 B",
        "S/M/L damp river rocks with moss and flattened bases", "moss_blue_gray",
        ("small_medium_large", "damp_rock", "flattened_bases"),
    ),
    "P2D_QS_TREE_BROADLEAF_A": AssetSpec(
        "paper2d_plant", (5, 0.05, 6), ("yard", "perimeter"), "tree", "青山镇阔叶树族 A",
        "Three broadleaf silhouettes; only A receives five wind poses", "jade_green",
        ("broad_canopy", "fixed_trunk", "three_variants"),
    ),
    "P2D_QS_TREE_BAMBOO_A": AssetSpec(
        "paper2d_plant", (4, 0.05, 6), ("perimeter", "yard"), "bamboo", "青山镇竹丛族 A",
        "Three bamboo-clump silhouettes; only A receives five wind poses", "jade_green",
        ("bamboo_cluster", "stable_root", "three_variants"),
    ),
    "P2D_QS_GRASS_TUFT_A": AssetSpec(
        "paper2d_plant", (0.8, 0.05, 0.8), ("ground",), "grass", "青山镇草簇族 A",
        "Three low grass tufts; only A receives four wind poses", "muted_jade",
        ("low_tuft", "root_fixed", "three_variants"),
    ),
    "P2D_QS_FLOWER_WILD_A": AssetSpec(
        "paper2d_plant", (0.7, 0.05, 0.9), ("yard", "riverbank"), "flower", "青山镇野花族 A",
        "Three restrained wildflower clumps; only A receives four wind poses", "warm_ochre",
        ("restrained_bloom", "thin_stems", "three_variants"),
    ),
    "SRF_QS_GROUND_GRASS_A": AssetSpec(
        "surface", (4, 4, 0.02), ("perimeter",), "ground", "青山镇草地表面 A",
        "Sparse low-saturation grass ground, not individual blades", "muted_jade",
        ("sparse_grass_surface", "low_saturation", "broad_shapes"),
    ),
    "SRF_QS_ROAD_STONE_A": AssetSpec(
        "surface", (4, 4, 0.02), ("road",), "surface", "青山镇石土道路 A",
        "Worn stone-and-earth road compatible with QuickRoad geometry", "earth_ochre",
        ("worn_stone", "earth_infill", "quickroad_compatible"),
    ),
    "SRF_QS_RIVERBANK_BLEND_A": AssetSpec(
        "surface", (4, 4, 0.02), ("riverbank",), "transition", "青山镇河岸混合带 A",
        "Wet mud, gravel and moss transition family", "moss_ochre",
        ("wet_mud", "gravel_band", "moss_transition"),
    ),
    "KIT_QS_FENCE_WOOD_A": AssetSpec(
        "linear_kit", (3, 0.25, 1.5), ("yard",), "fence", "青山镇木围栏套件 A",
        "Straight, corner and gate wooden-fence trio", "dark_timber",
        ("straight", "corner", "gate_opening"),
    ),
    "KIT_QS_PATH_STEPPING_STONE_A": AssetSpec(
        "linear_kit", (3, 1, 0.2), ("yard", "path"), "stepping_stones", "青山镇汀步石套件 A",
        "Straight, curved and scattered stepping-stone trio", "blue_gray",
        ("straight", "curved", "scattered"),
    ),
    "PROP_QS_MARKET_KIT_A": AssetSpec(
        "prop_kit", (3, 2, 2.8), ("market",), "props", "青山镇集市道具套件 A",
        "Stall frame, sign frame and basket trio", "warm_ochre",
        ("stall_frame", "blank_sign_frame", "basket"),
    ),
    "PROP_QS_DOCK_KIT_A": AssetSpec(
        "prop_kit", (2.5, 2, 1.8), ("dock",), "props", "青山镇码头道具套件 A",
        "Mooring post, rope coil and crate trio", "dark_timber",
        ("mooring_post", "rope_coil", "crate"),
    ),
    "PROP_QS_STREET_KIT_A": AssetSpec(
        "prop_kit", (2, 1, 3), ("street",), "props", "青山镇街道道具套件 A",
        "Lantern frame, bench and road-sign frame trio", "warm_red_brown",
        ("lantern_frame", "bench", "blank_road_sign_frame"),
    ),
}

ROOFS = {
    "BLD_QS_M_A_INN": ("asymmetric_xieshan", "warm_red_brown", "M", 3.3),
    "BLD_QS_S_A_HOUSE": ("double_slope", "indigo", "S", 2.4),
    "BLD_QS_M_B_STREET_SHOP": ("deep_eave_half_storey", "ink_green", "M", 2.9),
    "BLD_QS_S_B_WORKSHOP": ("hard_gable", "ochre", "S", 2.3),
    "BLD_QS_S_C_RIVER_HUT": ("low_eave", "teal", "S", 2.1),
}

BUILDING_QUAD_BUDGETS = {
    "BLD_QS_M_A_INN": ([30000, 35000], 35000),
    "BLD_QS_M_B_STREET_SHOP": ([25000, 30000], 30000),
    "BLD_QS_S_A_HOUSE": ([15000, 20000], 20000),
    "BLD_QS_S_B_WORKSHOP": ([12000, 18000], 18000),
    "BLD_QS_S_C_RIVER_HUT": ([15000, 20000], 20000),
}

PLANTS = {
    "P2D_QS_TREE_PINE_A": (
        5, 7, (2048, 2048), (512, 512), (256, 448), 0.64, "height", 3, 5.5, 9000
    ),
    "P2D_QS_SHRUB_A": (
        5, 7, (1024, 1024), (256, 256), (240, 180), 1.2, "height", 5, 1.5, 5000
    ),
    "P2D_QS_TREE_BROADLEAF_A": (
        5, 7, (2048, 2048), (512, 512), (360, 432), 0.72, "height", 3, 5.0, 9000
    ),
    "P2D_QS_TREE_BAMBOO_A": (
        5, 7, (2048, 2048), (512, 512), (288, 432), 0.72, "height", 4, 3.5, 8000
    ),
    "P2D_QS_GRASS_TUFT_A": (
        4, 6, (1024, 1024), (256, 256), (192, 192), 2.4, "height", 7, 0.65, 3500
    ),
    "P2D_QS_FLOWER_WILD_A": (
        4, 6, (1024, 1024), (256, 256), (168, 216), 2.4, "height", 6, 0.7, 3500
    ),
}

B0_PROMPTS = {
    "REF_QS_ENV_STYLE_LOCK": """Use case: stylized-concept
Asset type: game environment style-lock board
Primary request: define the approved visual language for Qingshan town, a Chinese ink-cartoon town enclosed by layered mountains
Input images: environment day and night images are atmosphere and brushwork references; character and creature images are proportion, outline, shape-language, and palette references; do not copy their subjects
Scene/backdrop: warm rice-paper board with a compact town vignette, small material swatches, roof-color families, foliage shapes, mountain layers, river and stone-road samples; no written labels
Subject: warm off-white walls, dark timber, roofs in warm red-brown, ink green, indigo, ochre and teal; jade and blue-green foliage; blue-gray mist mountains; dark teal variable ink outlines
Style/medium: hand-drawn Chinese ink cartoon concept art, dry-brush broken edges, simplified chunky silhouettes, two to three value bands, restrained watercolor wash
Composition/framing: wide coherent style board, central environment vignette with orderly visual samples around it, generous margins
Lighting/mood: calm bright morning with a small cool mist sample, friendly adventure tone
Constraints: no text, no symbols, no logos, no watermark, no photorealism, no PBR shine, no neon green, no modern objects, no mirror symmetry""",
    "REF_QS_SCALE_LINEUP": """Use case: stylized-concept
Asset type: game environment scale-lineup board
Primary request: show Qingshan town gameplay assets at one consistent scale for a fixed high three-quarter camera
Input images: project environment and character images define ink-cartoon style and hero scale; the Qingshan style-lock board defines palette and materials
Scene/backdrop: plain warm rice-paper background with a single horizontal ground baseline, no full scene
Subject: one small project hero, L/M/S Chinese town-building silhouettes, north gate, nine-meter bridge section, dock section, pine, broadleaf tree, bamboo, shrub and near mountain-foot module, all clearly proportional
Style/medium: the exact QS_InkToon_v1 hand-drawn ink-cartoon language
Composition/framing: wide lineup, every subject fully visible and separated, high three-quarter asset view, no cropping
Lighting/mood: neutral soft daylight for shape comparison
Constraints: no labels, no numbers, no text, no watermark, no duplicated subjects, no perspective distortion, no modern objects""",
    "REF_QS_CAMERA_ROUTE": """Use case: stylized-concept
Asset type: four-panel game environment camera-route concept board
Primary request: show the Qingshan town player route in four consistent fixed-camera views: north gate arrival, market and largest shop, main bridge, south dock
Input images: project ink illustrations define style; the foliage screenshot defines only dense overlapping canopy rhythm, never its rendering style; the style-lock board defines final palette and materials
Scene/backdrop: one coherent mountain-enclosed Chinese town across four equal visual panels without borders or text
Subject: north gate remains on screen right; a readable horizontal gentle S-shaped road runs right to left through an asymmetric market, offset bridge, river and left-side dock; one large shop, smaller staggered houses, clustered foliage, near solid mountain feet and pale Paper2D-like far mountain silhouettes
Style/medium: QS_InkToon_v1 hand-drawn Chinese ink cartoon environment concept
Composition/framing: fixed high three-quarter camera matching the game; four route moments share scale, geography and asset identity; player path remains clear
Lighting/mood: fresh morning, calm mist, readable gameplay contrast
Constraints: no text, no UI, no labels, no watermark, no straight building rows, no mirror symmetry, no fluorescent foliage, no mountain wall blocking the lower center, no modern objects""",
    "REF_QS_SURFACE_PALETTE": """Use case: stylized-concept
Asset type: game environment surface-palette board
Primary request: define the coordinated ground and water surfaces for Qingshan town
Input images: project environment references define ink wash and atmosphere; warm creature reference defines restrained ochre accents; the style-lock board defines final palette
Scene/backdrop: warm rice-paper presentation board, no written labels
Subject: adjacent samples of compact earth, sparse grass ground, worn stone-and-dirt road, wet muddy riverbank, blue-green shallow river water, mossy field rock and dry mountain rock; include two clean transition strips and one small player-camera terrain vignette
Style/medium: QS_InkToon_v1 hand-painted environment material concept, large readable shapes, dry-brush edges, two to three tonal bands
Composition/framing: wide organized material board with separated but visually connected samples, no perspective-heavy hero object
Lighting/mood: neutral overcast-soft light for color comparison
Constraints: no text, no labels, no watermark, no photoreal PBR spheres, no shiny plastic, no neon colors, no obvious repeating photo texture""",
}


def _read_json(path: Path, label: str) -> dict[str, Any]:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError as exc:
        raise CatalogBuildError(f"missing {label}: {path}") from exc
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise CatalogBuildError(f"invalid {label} {path}: {exc}") from exc
    if not isinstance(data, dict):
        raise CatalogBuildError(f"{label} must be a JSON object: {path}")
    return data


def _load_source_inputs(root: Path) -> tuple[dict[str, Any], list[dict[str, str]]]:
    metrics = _read_json(root / "source_metrics.json", "source metrics")
    bounds = metrics.get("bounds_size_m")
    if (
        metrics.get("ok") is not True
        or not isinstance(bounds, list)
        or len(bounds) != 3
        or any(type(value) not in (int, float) or not math.isfinite(value) or value <= 0 for value in bounds)
        or not isinstance(metrics.get("asset_path"), str)
        or not metrics["asset_path"]
        or type(metrics.get("material_slot_count")) is not int
        or metrics["material_slot_count"] < 1
    ):
        raise CatalogBuildError("source metrics are incomplete or invalid")

    provenance = _read_json(
        root / "style" / "references" / "provenance.json", "reference provenance"
    )
    references = provenance.get("references")
    if not isinstance(references, list) or len(references) != len(STYLE_REFERENCE_ROLES):
        raise CatalogBuildError("reference provenance must contain the seven stable references")
    normalized: list[dict[str, str]] = []
    seen: set[str] = set()
    for entry in references:
        if not isinstance(entry, dict):
            raise CatalogBuildError("reference provenance entries must be objects")
        destination = entry.get("destination")
        role = entry.get("role")
        digest = entry.get("sha256")
        if destination not in STYLE_REFERENCE_ROLES or destination in seen:
            raise CatalogBuildError(f"unexpected or duplicate provenance destination: {destination!r}")
        if role != STYLE_REFERENCE_ROLES[destination]:
            raise CatalogBuildError(f"reference role mismatch for {destination}")
        if not isinstance(digest, str) or len(digest) != 64:
            raise CatalogBuildError(f"reference SHA-256 is invalid for {destination}")
        seen.add(destination)
        normalized.append(
            {
                "path": f"style/references/{destination}",
                "role": role,
                "sha256": digest,
            }
        )
    if seen != set(STYLE_REFERENCE_ROLES):
        raise CatalogBuildError("reference provenance is missing required stable references")
    return metrics, normalized


def _reference_selection(asset_id: str, references: list[dict[str, str]]) -> list[dict[str, str]]:
    by_name = {Path(item["path"]).name: item for item in references}
    names = ["style_env_day.jpeg", "style_character_scale.jpeg", "style_nature_group.jpeg"]
    if asset_id == "REF_QS_ENV_STYLE_LOCK":
        names = [name for name in STYLE_REFERENCE_ROLES if name != "layout_dense_foliage.jpg"]
    elif asset_id == "REF_QS_SCALE_LINEUP":
        names = ["style_env_day.jpeg", "style_character_scale.jpeg", "style_nature_group.jpeg"]
    elif asset_id == "REF_QS_CAMERA_ROUTE":
        names = [
            "style_env_day.jpeg",
            "style_env_night.jpeg",
            "layout_dense_foliage.jpg",
            "style_character_scale.jpeg",
        ]
    elif asset_id == "REF_QS_SURFACE_PALETTE":
        names = ["style_env_day.jpeg", "style_env_night.jpeg", "style_creature_warm.jpeg"]
    elif asset_id.startswith("P2D_QS_"):
        names = ["style_env_day.jpeg", "style_env_night.jpeg", "style_nature_group.jpeg"]
    return [by_name[name] for name in names]


def _prompt_sections(prompt: str) -> dict[str, Any]:
    sections: dict[str, Any] = {}
    for line in prompt.splitlines():
        key, separator, value = line.partition(": ")
        if separator:
            sections[key.lower().replace("/", "_").replace(" ", "_")] = value
    return sections


def _normal_prompt(spec: AssetSpec, view_contracts: dict[str, dict[str, Any]]) -> str:
    lines = [
            "Use case: stylized-concept",
            f"Asset type: Qingshan {spec.category} game environment reference",
            f"Primary request: {spec.primary_request}",
            "Input images: project references define ink-cartoon brushwork, scale, shape language and restrained palette; do not copy their subjects",
            "Scene/backdrop: clean warm rice-paper background suitable for a single asset or the approved compact kit",
            f"Subject: {spec.primary_request}; intended gameplay role is {spec.role}",
            "Style/medium: QS_InkToon_v1 hand-drawn Chinese ink cartoon, dry-brush broken edges, simplified chunky silhouettes, two to three value bands",
            "Composition/framing: complete uncropped asset with readable base, scale and connection points for the fixed high three-quarter game camera",
            "Lighting/mood: neutral soft daylight with calm mist-compatible contrast",
            "Constraints: no text, no logos, no watermark, no photorealism, no PBR shine, no neon green, no modern objects, no mirror symmetry",
    ]
    lines.extend(
        "View {kind}: {instruction}; required elements: {elements}".format(
            kind=kind,
            instruction=contract["instruction"],
            elements=", ".join(contract["required_elements"]),
        )
        for kind, contract in view_contracts.items()
    )
    return "\n".join(lines)


def _view_contracts(asset_id: str, spec: AssetSpec) -> dict[str, dict[str, Any]]:
    category = spec.category
    if category == "reference":
        return {
            "board": {
                "instruction": "Generate the single approved Batch 0 board exactly as specified by the full prompt",
                "required_elements": ["complete_board", "consistent_QS_InkToon_v1", "no_written_labels"],
            }
        }
    if category == "registry":
        return {
            "existing_asset_registry": {
                "instruction": "Record the existing L-A mesh facts without generating or altering a model",
                "required_elements": [
                    "source_mesh_path", "measured_bounds_m", "material_slot_count",
                    "pivot_and_forward_axis", "scale_lineup_relationship",
                    "existing_50k_quad_registration",
                ],
            }
        }
    if category == "building":
        return {
            "hero_3q": {
                "instruction": "Show one complete Tripo-ready building from the fixed high three-quarter camera",
                "required_elements": [
                    "complete_uncropped_silhouette", "visible_base", "entrance_direction",
                    "roof_readability", "fixed_high_three_quarter_camera",
                    "single_asset_clean_warm_background",
                ],
            },
            "structure_sheet": {
                "instruction": "Derive one controlled structure sheet from the approved hero view",
                "required_elements": [
                    "entrance_front", "side_and_rear", "top_down_footprint",
                    "player_scale_reference", "material_regions",
                ],
            },
        }
    if category == "landmark":
        return {
            "route_3q": {
                "instruction": "Show the landmark as a readable traversal moment on the player route",
                "required_elements": [
                    "player_route_primary_view", "entry_and_exit", "traversal_clearance",
                    "fixed_high_three_quarter_camera",
                ],
            },
            "structure_connection_sheet": {
                "instruction": "Show structure, passage dimensions and all route connection interfaces in one sheet",
                "required_elements": [
                    "front_and_side_structure", "top_down_passage_dimensions",
                    "connection_interfaces", "material_regions", "simple_collision_clearance",
                ],
            },
        }
    if category == "near_mountain":
        return {
            "module_lineup": {
                "instruction": "Show the approved joinable near-mountain modules as a separated lineup",
                "required_elements": [
                    "variant_module_lineup", "join_interfaces", "complete_base", "distinct_silhouette",
                ],
            },
            "assembly_player_scale": {
                "instruction": "Assemble the modules at map edge with player and building scale references",
                "required_elements": [
                    "assembled_edge_example", "player_and_building_scale",
                    "north_gate_gap_preserved", "lower_center_open",
                ],
            },
        }
    if category == "far_mountain":
        return {
            "silhouette_layers": {
                "instruction": "Show two joinable Paper2D mountain variants as separated depth silhouettes",
                "required_elements": [
                    "two_joinable_variants", "clean_silhouette_band", "separated_depth_layer_shapes",
                ],
            },
            "player_camera_composite": {
                "instruction": "Composite the far layer behind solid near mountains in the fixed player camera",
                "required_elements": [
                    "fixed_player_camera", "near_mountain_relationship",
                    "north_gate_gap_visible", "depth_layer_readability",
                ],
            },
        }
    if category == "rock_kit":
        return {
            "variant_lineup": {
                "instruction": "Show S, M and L rocks as complete separated model-ready variants",
                "required_elements": [
                    "S_M_L_variants", "complete_separated_silhouettes", "flat_bases", "material_state",
                ],
            },
            "silhouette_scale": {
                "instruction": "Show black silhouettes, gameplay scale and one asymmetric placement cluster",
                "required_elements": [
                    "black_silhouette", "player_and_building_scale", "placement_cluster",
                ],
            },
        }
    if category == "paper2d_plant":
        frames = PLANTS[asset_id][0]
        frame_sequence = (
            ["neutral", "left_small", "left_large", "right_small", "right_large"]
            if frames == 5
            else ["neutral", "left", "right", "slight_return"]
        )
        return {
            "neutral_variants": {
                "instruction": "Show the A, B and C plant silhouettes separately before any animation",
                "required_elements": ["separate_A_B_C", "stable_root_or_trunk", "warm_plain_background"],
            },
            "silhouette_scale_cluster": {
                "instruction": "Show black silhouettes with the project scale ruler and a 3–5 plant cluster",
                "required_elements": [
                    "black_silhouette", "project_character_and_building_scale_ruler", "cluster_of_3_to_5",
                ],
            },
            "wind_poses": {
                "instruction": "Derive the ordered wind poses only from approved neutral variant A",
                "required_elements": [
                    "only_variant_A_animated", "B_and_C_static", "explicit_frame_sequence",
                    "stable_root_or_trunk",
                ],
                "frame_sequence": frame_sequence,
            },
        }
    if category == "surface":
        seamless = [
            "no_perspective_tile_sample", "boundary_transition_strips", "non_repeating_shape_check",
        ]
        player_camera = ["sloped_material_example", "fixed_player_camera_samples_5m_10m_20m"]
        if asset_id == "SRF_QS_WATER_RIVER_A":
            water = ["flow_direction", "bank_line", "shallow_deep_bands", "foam_layer"]
            seamless += water
            player_camera += water
        return {
            "seamless_transition_sheet": {
                "instruction": "Show a no-perspective tiling sample and clean material boundary strips",
                "required_elements": seamless,
            },
            "player_camera_material_sheet": {
                "instruction": "Show slope behavior and fixed player-camera samples at 5m, 10m and 20m",
                "required_elements": player_camera,
            },
        }
    if category in ("linear_kit", "prop_kit"):
        return {
            "variant_lineup": {
                "instruction": "Show the three named kit members as complete separated production variants",
                "required_elements": [
                    "three_named_variants", "complete_separated_silhouettes", "connection_pivots",
                ],
                "named_variants": list(spec.silhouettes),
            },
            "usage_scale_sheet": {
                "instruction": "Show gameplay scale, intended placement examples and clearance at connections",
                "required_elements": [
                    "project_character_and_building_scale", "placement_examples", "connection_clearance",
                ],
                "intended_zones": list(spec.zones),
            },
        }
    raise CatalogBuildError(f"missing view contracts for category {category}")


def _palette(accent: str) -> list[str]:
    return list(dict.fromkeys((*COMMON_PALETTE, accent)))


def _generation(
    asset_id: str,
    spec: AssetSpec,
    stable_references: list[dict[str, str]],
) -> tuple[dict[str, Any], list[str]]:
    view_contracts = _view_contracts(asset_id, spec)
    selected = _reference_selection(asset_id, stable_references)
    inputs = [item["path"] for item in selected]
    input_roles = [{"path": item["path"], "role": item["role"]} for item in selected]
    generated_inputs: list[tuple[str, str]] = []
    if asset_id in ("REF_QS_SCALE_LINEUP", "REF_QS_CAMERA_ROUTE", "REF_QS_SURFACE_PALETTE"):
        generated_inputs.append((GENERATED_BOARD_PATHS["REF_QS_ENV_STYLE_LOCK"], "generated_style_lock"))
    elif spec.category not in ("reference", "registry"):
        generated_inputs.extend(
            (
                (GENERATED_BOARD_PATHS["REF_QS_ENV_STYLE_LOCK"], "generated_style_lock"),
                (GENERATED_BOARD_PATHS["REF_QS_SCALE_LINEUP"], "generated_scale_lineup"),
            )
        )
    for board, role in generated_inputs:
        inputs.append(board)
        input_roles.append({"path": board, "role": role})
    prompt = B0_PROMPTS.get(asset_id, _normal_prompt(spec, view_contracts))
    sections = _prompt_sections(prompt)
    sections["view_contracts"] = view_contracts
    expected_views = list(EXPECTED_VIEWS[spec.category])
    generation = {
        "asset_type": sections.get("asset_type", f"Qingshan {spec.category} reference"),
        "avoid": COMMON_NEGATIVE,
        "blocked_after_revisions": 2,
        "composition_framing": sections["composition_framing"],
        "constraints": COMMON_NEGATIVE,
        "generation_calls_used": 0,
        "input_image_roles": input_roles,
        "input_images": inputs,
        "lighting_mood": sections["lighting_mood"],
        "max_generation_calls": 0 if spec.category == "registry" else len(expected_views) * 3,
        "max_versions_per_view": 3,
        "palette": _palette(spec.accent),
        "prompt": prompt,
        "prompt_sections": sections,
        "required_view_kinds": expected_views,
        "scene_backdrop": sections["scene_backdrop"],
        "style_medium": sections["style_medium"],
        "subject": sections["subject"],
        "use_case": "stylized-concept",
    }
    return generation, [item["path"] for item in selected]


def _variant_limit(category: str) -> int:
    return {
        "near_mountain": 2,
        "far_mountain": 2,
        "rock_kit": 3,
        "paper2d_plant": 3,
        "linear_kit": 3,
        "prop_kit": 3,
    }.get(category, 1)


def _unreal_contract(category: str) -> dict[str, Any]:
    asset_class = {
        "reference": "ConceptReference",
        "registry": "StaticMesh",
        "building": "StaticMesh",
        "landmark": "StaticMesh",
        "near_mountain": "StaticMesh",
        "far_mountain": "PaperSprite",
        "rock_kit": "StaticMesh",
        "paper2d_plant": "PaperSprite",
        "surface": "MaterialInstance",
        "linear_kit": "StaticMesh",
        "prop_kit": "StaticMesh",
    }[category]
    return {
        "asset_class": asset_class,
        "bsdf_style": "UE5.8_Substrate_BSDF_ink_toon",
        "engine_version": "5.8",
        "material_pipeline": "retopology_or_sprite_lock_before_material_authoring",
        "performance_budget": "conservative_initial_adjust_after_profile",
        "shading_value_bands": 3,
    }


def _pcg_contract(spec: AssetSpec) -> dict[str, Any]:
    return {
        "allowed_zones": list(spec.zones),
        "density_policy": "conservative_initial",
        "excluded_zones": ["road_clearance", "river_clearance", "interaction_clearance"],
        "instance_cap_initial": 12 if spec.category == "paper2d_plant" else 6,
        "performance_adjustment": "profile_then_raise_or_lower",
        "placement_pattern": "staggered_not_row" if spec.category == "building" else "role_driven",
    }


def _category_fields(asset_id: str, spec: AssetSpec, metrics: dict[str, Any]) -> dict[str, Any]:
    category = spec.category
    fields: dict[str, Any] = {"variant_limit": _variant_limit(category)}
    if category == "registry":
        fields["registry"] = {
            "asset_path": metrics["asset_path"],
            "existing_quad_faces_target": 50000,
            "material_slot_count": metrics["material_slot_count"],
            "scale_anchor": "largest_approved_town_building",
            "source_metrics": "source_metrics.json",
        }
    elif category == "building":
        roof_form, roof_color, size_class, eave_height = ROOFS[asset_id]
        approved_range, target_quad_faces = BUILDING_QUAD_BUDGETS[asset_id]
        fields.update(
            {
                "allowed_cluster_roles": ["market_edge", "street_offset", "courtyard_offset"],
                "building": {
                    "model_pipeline": {
                        "approved_quad_face_range": approved_range,
                        "material_stage": "after_retopology",
                        "source": "Tripo_high_precision",
                        "target_quad_faces": target_quad_faces,
                        "topology": "quad",
                    },
                    "scale_anchor": "BLD_QS_L_A_MARKET_SHOP",
                },
                "door_socket": "Door_Main",
                "eave_height_m": eave_height,
                "entrance_axis": "+Y",
                "footprint_m": [spec.dimensions[0], spec.dimensions[1]],
                "retopo_target_quads": target_quad_faces,
                "roof": {"form": roof_form, "primary_color": roof_color},
                "simple_collision_required": True,
                "size_class": size_class,
                "texture_resolution": "4K_source_2K_runtime",
                "use": spec.role,
            }
        )
    elif category == "landmark":
        target = {"LMK_QS_GATE_NORTH": 50000, "LMK_QS_BRIDGE_MAIN": 35000, "LMK_QS_DOCK_SOUTH": 25000}[asset_id]
        fields.update(
            {
                "landmark": {
                    "material_stage": "after_retopology",
                    "model_source": "Tripo_high_precision_then_DCC_structure_check",
                    "route_clearance_required": True,
                    "topology": "quad",
                },
                "retopo_target_quads": target,
                "simple_collision_required": True,
                "texture_resolution": "4K_source_2K_runtime",
            }
        )
    elif category == "near_mountain":
        fields["environment"] = {
            "assembly": "joinable_solid_near_mountain_modules",
            "model_pipeline": "high_precision_source_then_quad_retopology",
            "perimeter_role": "hybrid_mountain_ring",
        }
        fields["retopo_target_quads"] = 30000
        fields["simple_collision_required"] = True
    elif category == "far_mountain":
        fields["environment"] = {
            "assembly": "layered_far_backdrop",
            "perimeter_role": "hybrid_mountain_ring",
        }
        fields["paper2d"] = {
            "background": "warm_keyed_production_pending",
            "depth_role": spec.role,
            "variants": ["A", "B"],
        }
        fields["depth_layer"] = "far_mist" if asset_id.endswith("MIST") else "far_silhouette"
        fields["facing_mode"] = "fixed_camera"
        fields["masked_material"] = True
        fields["source_canvas_px"] = [4096, 2048]
    elif category == "rock_kit":
        fields["environment"] = {
            "model_pipeline": "high_precision_source_then_quad_retopology_then_material",
            "variants": ["S", "M", "L"],
        }
        fields["retopo_target_quads"] = 12000
        fields["simple_collision_required"] = True
    elif category == "paper2d_plant":
        (
            frames,
            cells,
            canvas,
            cell,
            content_box,
            pixels_per_unit,
            ppu_basis,
            cluster,
            spacing,
            cull,
        ) = PLANTS[asset_id]
        fields.update(
            {
                "animated_variants": ["A"],
                "atlas_cell_count": cells,
                "atlas_group": "QS_Plant_A_Wind",
                "card_world_size_m": [spec.dimensions[0], spec.dimensions[2]],
                "cluster_size": cluster,
                "collision": False,
                "cull_distance_m": cull / 100,
                "depth_layer": "midground_foliage",
                "facing_mode": "fixed_camera_card",
                "ground_trace_required": True,
                "masked_material": True,
                "paper2d": {
                    "animation_rule": "only_A_animated_B_and_C_static",
                    "atlas_cell_count": cells,
                    "background_policy": "concept_warm_plain_production_magenta_key",
                    "content_box_px": list(content_box),
                    "expected_world_size_m": [spec.dimensions[0], spec.dimensions[2]],
                    "import_scale_override": 1.0,
                    "ppu_definition": "pixels_per_unreal_unit_centimeter",
                    "ppu_basis_dimension": ppu_basis,
                    "unreal_units_per_meter": 100,
                    "variants": ["A", "B", "C"],
                    "world_size_formula": "content_box_px/(pixels_per_unreal_unit*100)*import_scale_override",
                },
                "pivot_mode": "bottom_center_root_locked",
                "pixels_per_unreal_unit": pixels_per_unit,
                "source_canvas_px": list(canvas),
                "spacing_m": spacing,
                "sprite_cell_px": list(cell),
                "wind_frames": frames,
            }
        )
    elif category == "surface":
        fields["surface"] = {
            "material_family": spec.role,
            "transition_required": True,
            "world_aligned_scale_m": [spec.dimensions[0], spec.dimensions[1]],
        }
    elif category in ("linear_kit", "prop_kit"):
        fields["kit"] = {
            "production_state": "concept_only_performance_gate_locked",
            "variants": list(spec.silhouettes),
        }
        fields["retopo_target_quads"] = 10000 if category == "linear_kit" else 8000
        fields["simple_collision_required"] = True
    return fields


def _apply_layout_specifics(asset_id: str, pcg: dict[str, Any]) -> None:
    if asset_id == "LMK_QS_GATE_NORTH":
        pcg.update(
            {
                "existing_exit_anchor_policy": "do_not_move_QingshanInn_TownExit",
                "player_default_facing_relation": "not_facing_gate_at_start",
                "screen_anchor": "right",
                "valley_opening_m": [18, 24],
            }
        )
    elif asset_id == "LMK_QS_BRIDGE_MAIN":
        pcg.update(
            {
                "crosses": "river",
                "road_provider": "QuickRoad",
                "spline_connection": "road_to_bridge_deck_to_road",
            }
        )
    elif asset_id == "LMK_QS_DOCK_SOUTH":
        pcg.update({"river_side": "south_bank", "route_position": "left_side_destination"})
    elif asset_id == "SRF_QS_ROAD_STONE_A":
        pcg.update(
            {
                "road_provider": "QuickRoad",
                "usage": "material_reference_for_quickroad_geometry_no_direct_material_copy",
            }
        )
    elif asset_id.startswith("ENV_QS_MTN_NEAR") or asset_id.startswith("P2D_QS_MTN_FAR"):
        pcg.update({"map_edge_role": "hybrid_mountain_ring", "lower_center_policy": "keep_open"})
    elif asset_id.startswith("P2D_QS_TREE") or asset_id in ("P2D_QS_SHRUB_A", "P2D_QS_GRASS_TUFT_A", "P2D_QS_FLOWER_WILD_A"):
        pcg.update({"cluster_rhythm": "overlapping_asymmetric", "road_clearance_priority": True})


def _make_asset(
    asset_id: str,
    batch: str,
    spec: AssetSpec,
    metrics: dict[str, Any],
    references: list[dict[str, str]],
    root: Path,
) -> dict[str, Any]:
    generation, stable_paths = _generation(asset_id, spec, references)
    dimensions = metrics["bounds_size_m"] if spec.dimensions is None else list(spec.dimensions)
    pcg = _pcg_contract(spec)
    _apply_layout_specifics(asset_id, pcg)
    is_b0 = batch == "B0"
    is_registry = batch == "REGISTRY"
    generated_dependencies = [
        dependency_id
        for dependency_id, board_path in GENERATED_BOARD_PATHS.items()
        if board_path in generation["input_images"]
    ]
    if is_registry:
        dependencies = ["source_metrics.json", "style/references/provenance.json"]
    elif is_b0:
        dependencies = [*stable_paths, *generated_dependencies]
    else:
        dependencies = generated_dependencies
    asset = {
        "acceptance": {
            "connection_readability": False,
            "player_camera_readability": False,
            "silhouette_unique": False,
        },
        "asset_id": asset_id,
        "batch": batch,
        "category": spec.category,
        "dependencies": dependencies,
        "display_name": spec.display_name,
        "forward_axis": "+Y",
        "gameplay_role": spec.role,
        "generation": generation,
        "intended_zone": list(spec.zones),
        "material_language": COMMON_MATERIAL,
        "negative_prompt": COMMON_NEGATIVE,
        "palette": _palette(spec.accent),
        "pcg": pcg,
        "pivot": "bottom_center",
        "priority": "P0" if batch in ("REGISTRY", "B0", "B1") else "P1" if batch == "B2" else "P2",
        "reference_images": [
            {
                "approval_state": "existing_asset_registered" if is_registry else "planned",
                "background": "existing_project_asset" if is_registry else "warm_rice_paper",
                "camera": "existing_asset_registry" if is_registry else "fixed_high_three_quarter_or_category_sheet",
                "file_stub": f"{asset_id}__{kind}",
                "kind": kind,
                "required_annotations": generation["prompt_sections"]["view_contracts"][kind]["required_elements"],
            }
            for kind in EXPECTED_VIEWS[spec.category]
        ],
        "schema_version": 1,
        "silhouette_keywords": list(spec.silhouettes),
        "source_provenance": {
            "production_intent": "concept_json_before_model_sprite_or_unreal_production",
            "reference_provenance": "style/references/provenance.json",
            "source_kind": "existing_project_asset" if is_registry else "project_concept",
            "stable_reference_images": stable_paths,
            "style_profile": "QS_InkToon_v1",
        },
        "status": "existing_asset_registered" if is_registry else "concept_planned",
        "style_profile": "QS_InkToon_v1",
        "target_dimensions_m": dimensions,
        "unreal": _unreal_contract(spec.category),
        "workflow_gates": {
            "batch_approval_id": None,
            "batch_unlocked": is_b0,
            "concept_generation_allowed": is_b0,
            "model_or_sprite_production_allowed": False,
            "previous_batch_approved": False,
            "reference_approved": is_registry,
            "style_locked": False,
            "tripo_allowed": False,
            "ue_import_allowed": False,
        },
    }
    if is_registry:
        asset["source_provenance"]["source_metrics"] = "source_metrics.json"
    asset.update(_category_fields(asset_id, spec, metrics))
    _overlay_registered_output(root, asset)
    try:
        validate_asset(asset)
        verify_asset_evidence(root, asset)
    except CatalogError as exc:
        raise CatalogBuildError(f"generated asset {asset_id} is invalid: {exc}") from exc
    return asset


def _overlay_registered_output(root: Path, asset: dict[str, Any]) -> None:
    """Preserve only evidence fields owned by the output-registration CLI."""

    path = root / "assets" / asset["asset_id"] / "asset.json"
    if not path.is_file():
        return
    try:
        current = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError):
        return
    if not isinstance(current, dict) or current.get("asset_id") != asset["asset_id"]:
        return
    current_generation = current.get("generation")
    if isinstance(current_generation, dict) and type(current_generation.get("generation_calls_used")) is int:
        asset["generation"]["generation_calls_used"] = current_generation["generation_calls_used"]
    current_references = current.get("reference_images")
    if not isinstance(current_references, list):
        return
    by_kind = {
        item.get("kind"): item
        for item in current_references
        if isinstance(item, dict) and isinstance(item.get("kind"), str)
    }
    for reference in asset["reference_images"]:
        current_reference = by_kind.get(reference["kind"])
        if not isinstance(current_reference, dict):
            continue
        for key in (
            "approval_state",
            "output_path",
            "sha256",
            "version",
            "rejection_reason",
        ):
            if key in current_reference:
                reference[key] = current_reference[key]
        trace_fields = (
            "generation_input_paths",
            "generation_reference_lineage",
            "generation_prompt",
        )
        if current_reference.get("version") in ALLOWED_VERSIONS and all(
            key in current_reference for key in trace_fields
        ):
            for key in trace_fields:
                reference[key] = current_reference[key]


def _style_profile(references: list[dict[str, str]]) -> dict[str, Any]:
    return {
        "camera_contract": {
            "asset_view": "fixed_high_three_quarter",
            "paper2d_view": "front_or_camera_calibrated_no_exaggerated_perspective",
            "player_route_view": "fixed_game_camera",
        },
        "model_pipeline": {
            "building_source": "Tripo_high_precision",
            "building_quad_face_targets": {
                asset_id: {
                    "approved_range": budget[0],
                    "target_quad_faces": budget[1],
                }
                for asset_id, budget in BUILDING_QUAD_BUDGETS.items()
            },
            "material_stage": "after_quad_retopology",
        },
        "negative_prompt": COMMON_NEGATIVE,
        "palette": COMMON_PALETTE + ["warm_red_brown", "ink_green", "indigo", "teal"],
        "performance_policy": {
            "adjustable_after_profile": True,
            "goal": "preserve_route_readability_and_frame_budget",
            "mode": "conservative_initial",
            "plant_atlas_cell_cap": 40,
            "production_note": "raise_or_lower_instance_caps_and_lods_only_after_measured_profile",
        },
        "reference_images": references,
        "schema_version": 1,
        "style_id": "QS_InkToon_v1",
        "town_layout_contract": {
            "bridge": {"crosses": "river", "placement": "offset_from_market_axis"},
            "building_pattern": "one_largest_shop_plus_smaller_staggered_houses",
            "edge_terrain": {
                "enclosure": "hybrid_mountain_ring",
                "lower_center": "open_for_gameplay",
                "near_mountain": "solid_modules_left_and_corners",
                "far_mountain": "Paper2D_layered_backdrop_upper_left",
            },
            "foliage": {"rendering": "Paper2D", "rhythm": "overlapping_asymmetric_clusters"},
            "north_gate": {
                "player_default_facing_relation": "not_facing_gate_at_start",
                "screen_side": "right",
                "valley_opening_m": [18, 24],
            },
            "playable_bounds_m": [160, 100],
            "river": {"bridge_clearance": "preserve", "flow": "through_main_bridge"},
            "road": {
                "provider": "QuickRoad",
                "shape": "gentle_horizontal_S_right_to_left",
                "terrain_policy": "spline_authority_with_local_grade_and_flattening_verified_in_editor",
            },
            "route_order": ["north_gate", "market", "main_bridge", "south_dock"],
        },
        "unreal_material_contract": {
            "bsdf_style": "UE5.8_Substrate_BSDF_ink_toon",
            "outline": "dark_teal_variable_dry_brush",
            "shading_value_bands": 3,
        },
        "visual_language": {
            "asymmetry": "required",
            "edge": "variable_ink_with_broken_dry_brush",
            "forms": "large_readable_silhouettes",
            "lighting": "two_to_three_value_bands",
            "wash": "restrained_watercolor",
        },
    }


def _json_bytes(data: dict[str, Any]) -> bytes:
    return (json.dumps(data, ensure_ascii=False, indent=2, sort_keys=True) + "\n").encode("utf-8")


def render_catalog(root: Path) -> dict[Path, bytes]:
    root = Path(root)
    metrics, references = _load_source_inputs(root)
    expected_ids = [asset_id for batch in BATCH_COUNTS for asset_id in CATALOG[batch]]
    if len(expected_ids) != 35 or len(set(expected_ids)) != 35 or set(expected_ids) != set(SPECS):
        raise CatalogBuildError("catalog source definitions must contain exactly 35 unique IDs")

    assets: dict[str, dict[str, Any]] = {}
    for batch in BATCH_COUNTS:
        if len(CATALOG[batch]) != BATCH_COUNTS[batch]:
            raise CatalogBuildError(f"{batch} source count does not match the batch contract")
        for asset_id in CATALOG[batch]:
            assets[asset_id] = _make_asset(
                asset_id, batch, SPECS[asset_id], metrics, references, root
            )

    manifest_generation = derive_manifest_generation(list(assets.values()))
    manifest = {
        "asset_ids": expected_ids,
        "batch_call_caps": BATCH_CALL_CAPS,
        "batch_counts": BATCH_COUNTS,
        "batch_state": manifest_generation["batch_state"],
        "catalog": CATALOG,
        "concept_scope": "35_json_records_31_environment_plus_4_reference",
        "generation_calls_used": manifest_generation["generation_calls_used"],
        "performance_policy": "conservative_initial_adjust_after_profile",
        "reference_provenance": "style/references/provenance.json",
        "schema_version": 1,
        "source_metrics": "source_metrics.json",
        "style_profile": "style/QS_InkToon_v1.json",
    }
    rendered = {
        Path("manifest.json"): _json_bytes(manifest),
        Path("style/QS_InkToon_v1.json"): _json_bytes(_style_profile(references)),
    }
    for asset_id in expected_ids:
        rendered[Path("assets") / asset_id / "asset.json"] = _json_bytes(assets[asset_id])
    if len(rendered) != 37:
        raise CatalogBuildError(f"expected 37 generated JSON files, got {len(rendered)}")
    return rendered


def write_catalog(root: Path, *, replace_file=None) -> dict[str, Any]:
    root = Path(root)
    rendered = render_catalog(root)
    try:
        transaction_arguments: dict[str, Any] = {
            "verify_after": lambda: validate_catalog(root),
        }
        if replace_file is not None:
            transaction_arguments["replace_file"] = replace_file
        write_files_transactionally(
            root,
            {root / relative: payload for relative, payload in rendered.items()},
            **transaction_arguments,
        )
        return validate_catalog(root)
    except CatalogError as exc:
        raise CatalogBuildError(f"catalog transaction failed: {exc}") from exc


def check_catalog(root: Path) -> dict[str, Any]:
    root = Path(root)
    rendered = render_catalog(root)
    drift: list[str] = []
    for relative, expected in rendered.items():
        path = root / relative
        try:
            actual = path.read_bytes()
        except OSError:
            drift.append(relative.as_posix())
            continue
        if actual != expected:
            drift.append(relative.as_posix())
    if drift:
        raise CatalogBuildError(f"generated catalog drift detected: {', '.join(drift)}")
    try:
        validate_catalog(root)
    except CatalogError as exc:
        raise CatalogBuildError(f"committed catalog failed validation: {exc}") from exc
    return {"ok": True, "unchanged": len(rendered)}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=DEFAULT_ROOT)
    parser.add_argument("--check-no-rewrite", action="store_true")
    args = parser.parse_args(argv)
    try:
        result = check_catalog(args.root) if args.check_no_rewrite else write_catalog(args.root)
    except CatalogBuildError as exc:
        print(json.dumps({"ok": False, "error": str(exc)}, ensure_ascii=False, sort_keys=True), file=sys.stderr)
        return 1
    print(json.dumps(result, ensure_ascii=False, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
