"""Author the isolated Qingshan B1 proxy assets in Unreal Engine 5.8.

The manifest/planning helpers intentionally remain host-safe so the import
contract can be tested without loading Unreal.  Mutating entry points fail
closed unless they are running inside the editor and no map package is dirty.
"""

from __future__ import annotations

import copy
import hashlib
import json
import math
from pathlib import Path
import re
from typing import Any, Iterable

try:
    import unreal  # type: ignore
except ModuleNotFoundError:  # Host-side contract tests do not load Unreal.
    unreal = None


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SOURCE_ROOT = PROJECT_ROOT / "Saved" / "Automation" / "QingshanB1ProxyKit"
MANIFEST_PATH = SOURCE_ROOT / "proxy-kit-manifest.json"
PLANT_SOURCE = (
    PROJECT_ROOT / "Content" / "ArtSource" / "Qingshan" / "B1"
    / "T_QS_B1_PlantProxy_4F.png"
)

ASSET_ROOT = "/Game/GameXXK/Environment/TownPCG/B1"
MATERIAL_ROOT = f"{ASSET_ROOT}/Materials"
MESH_ROOT = f"{ASSET_ROOT}/Meshes"
MESH_VARIANT_ROOT = f"{MESH_ROOT}/Variants"
PAPER2D_ROOT = f"{ASSET_ROOT}/Paper2D"

BUILDING_IDS = (
    "gable_shop",
    "tall_house",
    "wide_house",
    "courtyard_wing",
    "bridge_house",
    "dock_shed",
)
PROP_IDS = (
    "sign",
    "lantern",
    "banner",
    "fence",
    "crate",
    "stall",
    "well",
    "cart",
    "rock",
    "dock_post",
    "plant_card",
    "mountain",
)
ROOF_PALETTES = ("orange", "teal", "indigo", "ochre")
BUILDING_SLOT_ORDER = ("Wall", "Timber", "WindowPaper", "Roof")
PROP_SLOT_TABLE = {
    "sign": ("Timber", "Prop"),
    "lantern": ("Timber", "WindowPaper"),
    "banner": ("Timber", "Prop"),
    "fence": ("Timber",),
    "crate": ("Timber",),
    "stall": ("Timber", "Prop"),
    "well": ("Prop", "Timber"),
    "cart": ("Timber",),
    "rock": ("Prop",),
    "dock_post": ("Timber",),
    "plant_card": ("Foliage",),
    "mountain": ("Mountain",),
}
COLLISION_POLICY = {
    **{name: True for name in BUILDING_IDS},
    "sign": False,
    "lantern": False,
    "banner": False,
    "fence": True,
    "crate": True,
    "stall": True,
    "well": True,
    "cart": True,
    "rock": True,
    "dock_post": True,
    "plant_card": False,
    "mountain": False,
}
_SHA256 = re.compile(r"^[0-9a-f]{64}$")
_TEXTURE_DIMENSIONS = re.compile(r"^(\d+)x(\d+)$")
TEXTURE_UV_INPUT_CANDIDATES = ("UVs", "Coordinates")
BOUNDS_REL_TOLERANCE = 0.02
BOUNDS_ABS_TOLERANCE_CM = 0.5


def _sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _finite_positive_vector(value: Any, field: str, length: int = 3) -> list[float]:
    if not isinstance(value, list) or len(value) != length:
        raise ValueError(f"{field} must contain {length} numbers")
    result = []
    for item in value:
        if isinstance(item, bool) or not isinstance(item, (int, float)):
            raise ValueError(f"{field} must contain only numbers")
        number = float(item)
        if not math.isfinite(number) or number <= 0.0:
            raise ValueError(f"{field} must be finite and positive")
        result.append(number)
    return result


def _finite_vector(value: Any, field: str, length: int = 3) -> list[float]:
    if not isinstance(value, list) or len(value) != length:
        raise ValueError(f"{field} must contain {length} numbers")
    result = []
    for item in value:
        if isinstance(item, bool) or not isinstance(item, (int, float)):
            raise ValueError(f"{field} must contain only numbers")
        number = float(item)
        if not math.isfinite(number):
            raise ValueError(f"{field} must be finite")
        result.append(number)
    return result


def require_b1_asset_path(path: str) -> str:
    if not isinstance(path, str) or not path.startswith(ASSET_ROOT + "/"):
        raise ValueError(f"asset path must stay under the exact B1 asset root: {path!r}")
    if "//" in path or "." in path.rsplit("/", 1)[-1]:
        raise ValueError(f"asset path must stay under the exact B1 asset root: {path!r}")
    return path


def _source_path(project_root: Path, filename: str) -> Path:
    if not isinstance(filename, str) or Path(filename).name != filename:
        raise ValueError(f"manifest source file must be a basename: {filename!r}")
    return Path(project_root) / "Saved" / "Automation" / "QingshanB1ProxyKit" / filename


def validate_source_manifest(manifest: Any, project_root: Path = PROJECT_ROOT) -> dict[str, Any]:
    if not isinstance(manifest, dict) or manifest.get("schema_version") != 1:
        raise ValueError("proxy source manifest must be a schema-version 1 object")
    buildings = manifest.get("buildings")
    props = manifest.get("props")
    if not isinstance(buildings, dict) or set(buildings) != set(BUILDING_IDS):
        raise ValueError("proxy source manifest building IDs do not match the B1 contract")
    if not isinstance(props, dict) or set(props) != set(PROP_IDS):
        raise ValueError("proxy source manifest prop IDs do not match the B1 contract")

    for category, entries in (("building", buildings), ("prop", props)):
        for stable_id, entry in entries.items():
            if not isinstance(entry, dict):
                raise ValueError(f"{category} {stable_id} manifest entry must be an object")
            expected_slots = (
                list(BUILDING_SLOT_ORDER)
                if category == "building"
                else list(PROP_SLOT_TABLE[stable_id])
            )
            if entry.get("material_slots") != expected_slots:
                raise ValueError(f"{category} {stable_id} material slots violate the source contract")
            filename = entry.get("file")
            source = _source_path(Path(project_root), filename)
            if not source.is_file() or source.stat().st_size <= 256:
                raise ValueError(f"source FBX is missing or empty: {source}")
            expected_hash = entry.get("file_sha256")
            if not isinstance(expected_hash, str) or not _SHA256.fullmatch(expected_hash):
                raise ValueError(f"{category} {stable_id} file_sha256 is invalid")
            observed_hash = _sha256_file(source)
            if observed_hash != expected_hash:
                raise ValueError(
                    f"source FBX hash mismatch for {stable_id}: "
                    f"expected {expected_hash}, observed {observed_hash}"
                )
            geometry_digest = entry.get("geometry_digest")
            if not isinstance(geometry_digest, str) or not _SHA256.fullmatch(geometry_digest):
                raise ValueError(f"{category} {stable_id} geometry_digest is invalid")
            triangles = entry.get("triangle_count")
            if isinstance(triangles, bool) or not isinstance(triangles, int) or not 0 < triangles < 50000:
                raise ValueError(f"{category} {stable_id} triangle count must be 1..49999")
            bounds = _finite_positive_vector(
                entry.get("normalized_bounds_m"),
                f"{category}.{stable_id}.normalized_bounds_m",
            )
            if category == "building" and any(abs(value - 1.0) > 1.0e-6 for value in bounds):
                raise ValueError(f"building {stable_id} must keep its normalized 1m bounds")
            if entry.get("pivot") != "bottom_center" or entry.get("front_axis") != "+Y":
                raise ValueError(f"{category} {stable_id} pivot/front-axis contract drifted")
    return manifest


def load_source_manifest(project_root: Path = PROJECT_ROOT) -> dict[str, Any]:
    path = Path(project_root) / "Saved" / "Automation" / "QingshanB1ProxyKit" / "proxy-kit-manifest.json"
    try:
        manifest = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ValueError(f"could not read proxy source manifest {path}: {error}") from error
    return validate_source_manifest(manifest, Path(project_root))


def source_import_specs(
        manifest: dict[str, Any] | None = None,
        project_root: Path = PROJECT_ROOT) -> list[dict[str, Any]]:
    manifest = validate_source_manifest(
        manifest if manifest is not None else load_source_manifest(project_root),
        Path(project_root),
    )
    result = []
    for category, stable_ids, entries in (
        ("building", BUILDING_IDS, manifest["buildings"]),
        ("prop", PROP_IDS, manifest["props"]),
    ):
        for stable_id in stable_ids:
            entry = entries[stable_id]
            filename = entry["file"]
            asset_name = Path(filename).stem
            result.append({
                "id": stable_id,
                "category": category,
                "source_file": str(_source_path(Path(project_root), filename).resolve()),
                "source_sha256": entry["file_sha256"],
                "geometry_digest": entry["geometry_digest"],
                "asset_name": asset_name,
                "asset_path": require_b1_asset_path(f"{MESH_ROOT}/{asset_name}"),
                "slot_names": list(entry["material_slots"]),
                "expected_bounds_cm": [round(float(value) * 100.0, 6) for value in entry["normalized_bounds_m"]],
                "triangle_count": int(entry["triangle_count"]),
                "collision_enabled": bool(COLLISION_POLICY[stable_id]),
                "nanite_enabled": False,
            })
    return result


def _package_path_from_imported_object(value: Any) -> str:
    text = str(value).strip()
    if "'" in text:
        parts = text.split("'", 2)
        if len(parts) >= 2 and parts[1]:
            text = parts[1]
    text = text.rstrip("'")
    package = text.split(".", 1)[0]
    return require_b1_asset_path(package)


def validate_imported_object_paths(
        expected_asset_path: str,
        imported_object_paths: Iterable[Any]) -> list[str]:
    expected = require_b1_asset_path(expected_asset_path)
    reported = list(imported_object_paths)
    if not reported:
        raise RuntimeError(f"import task reported no imported objects for {expected}")
    normalized = [_package_path_from_imported_object(value) for value in reported]
    if expected not in normalized:
        raise RuntimeError(
            f"import task did not report expected asset {expected}: {normalized}"
        )
    return normalized


def source_geometry_audit(
        spec: dict[str, Any],
        actual_bounds_cm: Iterable[float],
        actual_triangle_count: int,
        actual_origin_cm: Iterable[float]) -> dict[str, Any]:
    """Validate and describe one imported mesh against its immutable source contract."""
    expected_bounds = _finite_positive_vector(
        list(spec["expected_bounds_cm"]),
        f"{spec['id']}.expected_bounds_cm",
    )
    actual_bounds = _finite_positive_vector(
        list(actual_bounds_cm),
        f"{spec['id']}.actual_bounds_cm",
    )
    actual_origin = _finite_vector(
        list(actual_origin_cm),
        f"{spec['id']}.actual_origin_cm",
    )
    for axis, (actual, expected) in enumerate(zip(actual_bounds, expected_bounds)):
        if not math.isclose(
                actual,
                expected,
                rel_tol=BOUNDS_REL_TOLERANCE,
                abs_tol=BOUNDS_ABS_TOLERANCE_CM):
            raise RuntimeError(
                f"source mesh bounds drifted for {spec['id']} on axis {axis}: "
                f"expected {expected_bounds}, got {actual_bounds}"
            )
    expected_triangles = int(spec["triangle_count"])
    if isinstance(actual_triangle_count, bool) or int(actual_triangle_count) != expected_triangles:
        raise RuntimeError(
            f"source mesh triangle count drifted for {spec['id']}: "
            f"expected {expected_triangles}, got {actual_triangle_count}"
        )
    min_z = actual_origin[2] - actual_bounds[2] * 0.5
    if (
        abs(actual_origin[0]) > BOUNDS_ABS_TOLERANCE_CM
        or abs(actual_origin[1]) > BOUNDS_ABS_TOLERANCE_CM
        or abs(min_z) > BOUNDS_ABS_TOLERANCE_CM
    ):
        raise RuntimeError(
            f"source mesh bottom-center pivot drifted for {spec['id']}: "
            f"origin={actual_origin}, min_z={min_z}"
        )
    rounded_expected = [round(value, 4) for value in expected_bounds]
    rounded_actual = [round(value, 4) for value in actual_bounds]
    return {
        "id": spec["id"],
        "asset_path": spec["asset_path"],
        "source_file": spec["source_file"],
        "source_sha256": spec["source_sha256"],
        "geometry_digest": spec["geometry_digest"],
        "expected_triangle_count": expected_triangles,
        "actual_triangle_count": int(actual_triangle_count),
        "triangle_count": int(actual_triangle_count),
        "expected_bounds_cm": rounded_expected,
        "actual_bounds_cm": rounded_actual,
        "bounds_cm": rounded_actual,
        "origin_cm": [round(value, 4) for value in actual_origin],
        "pivot_xy_cm": [round(actual_origin[0], 4), round(actual_origin[1], 4)],
        "min_z_cm": round(min_z, 4),
    }


def material_specs() -> dict[str, dict[str, Any]]:
    toon_instances = {
        "MI_QS_B1_Wall_Warm": (0.66, 0.48, 0.31, 1.0),
        "MI_QS_B1_Timber_Dark": (0.075, 0.045, 0.032, 1.0),
        "MI_QS_B1_Window_Paper": (0.93, 0.68, 0.28, 1.0),
        "MI_QS_B1_Roof_Orange": (0.63, 0.19, 0.09, 1.0),
        "MI_QS_B1_Roof_Teal": (0.16, 0.39, 0.37, 1.0),
        "MI_QS_B1_Roof_Indigo": (0.16, 0.23, 0.39, 1.0),
        "MI_QS_B1_Roof_Ochre": (0.58, 0.39, 0.13, 1.0),
        "MI_QS_B1_Ground": (0.43, 0.49, 0.32, 1.0),
        "MI_QS_B1_Road_Earth": (0.43, 0.30, 0.20, 1.0),
        "MI_QS_B1_Water_Teal": (0.11, 0.43, 0.47, 1.0),
        "MI_QS_B1_Prop_Jade": (0.17, 0.43, 0.38, 1.0),
        "MI_QS_B1_Mountain_BlueGrey": (0.24, 0.34, 0.39, 1.0),
    }
    specs: dict[str, dict[str, Any]] = {
        "TP_QS_B1_Toon": {"kind": "toon_profile"},
        "M_QS_B1_Toon": {
            "kind": "opaque_toon_master",
            "two_sided": False,
            "blend_mode": "Opaque",
            "expected_expression_count": 5,
        },
        "M_QS_B1_FoliageCard": {
            "kind": "masked_foliage_master",
            "two_sided": True,
            "blend_mode": "Masked",
            "opacity_mask_clip_value": 0.25,
            "expected_expression_count": 14,
        },
    }
    for name, color in toon_instances.items():
        specs[name] = {
            "kind": "toon_instance",
            "parent": "M_QS_B1_Toon",
            "base_color": list(color),
        }
    for frame_index, offset in enumerate((0.0, 0.25, 0.5, 0.75)):
        specs[f"MI_QS_B1_Foliage_F{frame_index}"] = {
            "kind": "foliage_instance",
            "parent": "M_QS_B1_FoliageCard",
            "atlas_scale_x": 0.25,
            "atlas_offset_x": offset,
        }
    specs["MI_QS_B1_Foliage_Animated"] = {
        "kind": "foliage_instance",
        "parent": "M_QS_B1_FoliageCard",
        "atlas_scale_x": 1.0,
        "atlas_offset_x": 0.0,
    }
    return copy.deepcopy(specs)


def confirm_parameter_write(
        parameter_name: str,
        expected: float | Iterable[float],
        observed: float | Iterable[float],
        setter_return: Any) -> dict[str, Any]:
    expected_values = (
        [float(value) for value in expected]
        if isinstance(expected, (list, tuple))
        else [float(expected)]
    )
    observed_values = (
        [float(value) for value in observed]
        if isinstance(observed, (list, tuple))
        else [float(observed)]
    )
    matched = len(expected_values) == len(observed_values) and all(
        math.isclose(actual, target, rel_tol=1.0e-6, abs_tol=1.0e-6)
        for actual, target in zip(observed_values, expected_values)
    )
    if not matched:
        raise RuntimeError(
            f"{parameter_name} readback drifted: "
            f"expected {expected_values}, got {observed_values}, "
            f"setter_return={setter_return!r}"
        )
    return {
        "parameter": parameter_name,
        "expected": expected_values,
        "observed": observed_values,
        "setter_return": bool(setter_return),
        "readback_matched": True,
    }


def parse_texture_dimensions(value: Any) -> list[int]:
    match = _TEXTURE_DIMENSIONS.fullmatch(str(value).strip())
    if match is None:
        raise RuntimeError(f"texture AssetRegistry Dimensions tag is invalid: {value!r}")
    size = [int(match.group(1)), int(match.group(2))]
    if min(size) <= 0:
        raise RuntimeError(f"texture AssetRegistry Dimensions tag is invalid: {value!r}")
    return size


def material_paths_for_slots(
        actual_slot_names: Iterable[str],
        slot_materials: dict[str, str]) -> list[str]:
    names = [str(value) for value in actual_slot_names]
    if len(names) != len(set(names)):
        raise ValueError(f"duplicate StaticMesh material slot in {names}")
    if set(names) != set(slot_materials) or len(names) != len(slot_materials):
        raise ValueError(
            f"StaticMesh slot contract mismatch: actual={names}, expected={sorted(slot_materials)}"
        )
    return [require_b1_asset_path(slot_materials[name]) for name in names]


def _building_source_asset(manifest: dict[str, Any], archetype_id: str) -> str:
    return f"{MESH_ROOT}/{Path(manifest['buildings'][archetype_id]['file']).stem}"


def building_variant_specs(manifest: dict[str, Any] | None = None) -> list[dict[str, Any]]:
    manifest = validate_source_manifest(
        manifest if manifest is not None else load_source_manifest(PROJECT_ROOT),
        PROJECT_ROOT,
    )
    result = []
    for archetype_id in BUILDING_IDS:
        source_asset = _building_source_asset(manifest, archetype_id)
        source_name = source_asset.rsplit("/", 1)[-1]
        for palette in ROOF_PALETTES:
            result.append({
                "archetype_id": archetype_id,
                "roof_palette": palette,
                "source_asset": source_asset,
                "asset_name": f"{source_name}_{palette.title()}",
                "asset_path": require_b1_asset_path(
                    f"{MESH_VARIANT_ROOT}/{source_name}_{palette.title()}"
                ),
                "slot_materials": {
                    "Wall": f"{MATERIAL_ROOT}/MI_QS_B1_Wall_Warm",
                    "Timber": f"{MATERIAL_ROOT}/MI_QS_B1_Timber_Dark",
                    "WindowPaper": f"{MATERIAL_ROOT}/MI_QS_B1_Window_Paper",
                    "Roof": f"{MATERIAL_ROOT}/MI_QS_B1_Roof_{palette.title()}",
                },
                "rebuild_from_source": True,
            })
    return result


def paper2d_spec(project_root: Path = PROJECT_ROOT) -> dict[str, Any]:
    source = (
        Path(project_root) / "Content" / "ArtSource" / "Qingshan" / "B1"
        / "T_QS_B1_PlantProxy_4F.png"
    )
    if not source.is_file():
        raise ValueError(f"Paper2D plant source is missing: {source}")
    default_material = f"{MATERIAL_ROOT}/MI_QS_B1_Foliage_Animated"
    sprites = []
    for frame_index in range(4):
        sprites.append({
            "frame_index": frame_index,
            "asset_name": f"SPR_QS_B1_Plant_Sway_{frame_index:02d}",
            "asset_path": f"{PAPER2D_ROOT}/SPR_QS_B1_Plant_Sway_{frame_index:02d}",
            "source_uv": [frame_index * 512, 0],
            "source_dimension": [512, 512],
            "source_texture_dimension": [2048, 512],
            "pivot_mode": "CUSTOM",
            "custom_pivot": [frame_index * 512 + 256, 472],
            "pixels_per_unreal_unit": 2.4,
            "default_material": default_material,
            "collision_domain": "NONE",
        })
    return {
        "source_file": str(source.resolve()),
        "texture_asset": f"{PAPER2D_ROOT}/T_QS_B1_PlantProxy_4F",
        "texture_filter": "Bilinear",
        "sprites": sprites,
        "flipbook_asset": f"{PAPER2D_ROOT}/FB_QS_B1_Plant_Sway",
        "frames_per_second": 5.0,
        "frame_order": [0, 1, 2, 3, 2, 1],
        "default_material": default_material,
    }


def _material_asset_paths() -> list[str]:
    return [require_b1_asset_path(f"{MATERIAL_ROOT}/{name}") for name in material_specs()]


def expected_result_contract(
        manifest: dict[str, Any] | None = None,
        project_root: Path = PROJECT_ROOT) -> dict[str, Any]:
    manifest = validate_source_manifest(
        manifest if manifest is not None else load_source_manifest(project_root),
        Path(project_root),
    )
    imports = source_import_specs(manifest, Path(project_root))
    variants = building_variant_specs(manifest)
    paper = paper2d_spec(Path(project_root))
    assets = [item["asset_path"] for item in imports]
    assets += [item["asset_path"] for item in variants]
    assets += _material_asset_paths()
    assets += [paper["texture_asset"], paper["flipbook_asset"]]
    assets += [item["asset_path"] for item in paper["sprites"]]
    assets = sorted(set(require_b1_asset_path(path) for path in assets))
    return {
        "source_fbx_count": len(imports),
        "source_building_mesh_count": sum(item["category"] == "building" for item in imports),
        "source_category_mesh_count": sum(item["category"] == "prop" for item in imports),
        "building_variant_count": len(variants),
        "sprite_count": len(paper["sprites"]),
        "flipbook_count": 1,
        "expected_assets": assets,
    }


def _require_unreal() -> None:
    if unreal is None:
        raise RuntimeError("Qingshan B1 asset authoring must run inside Unreal Editor")


def _package_path(value: Any) -> str:
    return str(value.get_outermost().get_path_name()).split(".", 1)[0]


def _asset_path(value: Any) -> str:
    if value is None:
        return ""
    return str(value.get_path_name()).split(".", 1)[0]


def _dirty_map_packages() -> list[str]:
    _require_unreal()
    return sorted(_package_path(package) for package in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())


def _require_no_dirty_maps(context: str) -> list[str]:
    dirty = _dirty_map_packages()
    if dirty:
        raise RuntimeError(f"{context} refuses to risk saving dirty maps: {dirty}")
    return dirty


def _ensure_directory(path: str) -> None:
    require_b1_asset_path(path + "/_DirectorySentinel")
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def _load_asset(path: str):
    require_b1_asset_path(path)
    return unreal.EditorAssetLibrary.load_asset(path)


def _create_or_load_asset(asset_name: str, package_path: str, asset_class, factory):
    asset_path = require_b1_asset_path(f"{package_path}/{asset_name}")
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        asset = _load_asset(asset_path)
        if asset is None or not isinstance(asset, asset_class):
            actual = asset.get_class().get_name() if asset is not None else "None"
            raise RuntimeError(f"existing asset has wrong class at {asset_path}: {actual}")
        return asset, False
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        asset_name, package_path, asset_class, factory
    )
    if asset is None:
        raise RuntimeError(f"could not create asset {asset_path}")
    return asset, True


def _set_property(obj, name: str, value) -> None:
    try:
        obj.set_editor_property(name, value)
    except Exception as error:
        raise RuntimeError(f"could not set {name} on {obj}: {error}") from error


def _run_import_task(source_file: str, destination_path: str, destination_name: str, options=None):
    task = unreal.AssetImportTask()
    _set_property(task, "filename", source_file)
    _set_property(task, "destination_path", destination_path)
    _set_property(task, "destination_name", destination_name)
    _set_property(task, "automated", True)
    _set_property(task, "replace_existing", True)
    _set_property(task, "replace_existing_settings", True)
    _set_property(task, "save", False)
    if options is not None:
        _set_property(task, "options", options)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    asset_path = require_b1_asset_path(f"{destination_path}/{destination_name}")
    reported_paths = task.get_editor_property("imported_object_paths")
    validate_imported_object_paths(asset_path, reported_paths)
    asset = _load_asset(asset_path)
    if asset is None:
        raise RuntimeError(f"import did not create {asset_path}")
    return asset


def _static_mesh_import_options():
    options = unreal.FbxImportUI()
    _set_property(options, "import_mesh", True)
    _set_property(options, "import_as_skeletal", False)
    _set_property(options, "import_materials", False)
    _set_property(options, "import_textures", False)
    _set_property(options, "mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    static_data = options.get_editor_property("static_mesh_import_data")
    _set_property(static_data, "combine_meshes", True)
    _set_property(static_data, "import_uniform_scale", 1.0)
    _set_property(static_data, "auto_generate_collision", False)
    return options


def _mesh_slot_names(static_mesh) -> list[str]:
    result = []
    for item in static_mesh.get_editor_property("static_materials"):
        result.append(str(item.get_editor_property("material_slot_name")))
    return result


def _mesh_bounds_cm(static_mesh) -> list[float]:
    extent = static_mesh.get_bounds().box_extent
    return [float(extent.x) * 2.0, float(extent.y) * 2.0, float(extent.z) * 2.0]


def _mesh_origin_cm(static_mesh) -> list[float]:
    origin = static_mesh.get_bounds().origin
    return [float(origin.x), float(origin.y), float(origin.z)]


def _mesh_geometry_snapshot(static_mesh) -> dict[str, Any]:
    return {
        "bounds_cm": [round(value, 4) for value in _mesh_bounds_cm(static_mesh)],
        "origin_cm": [round(value, 4) for value in _mesh_origin_cm(static_mesh)],
        "triangle_count": int(static_mesh.get_num_triangles(0)),
    }


def _require_matching_mesh_geometry(source_mesh, candidate_mesh, context: str) -> dict[str, Any]:
    source = _mesh_geometry_snapshot(source_mesh)
    candidate = _mesh_geometry_snapshot(candidate_mesh)
    if source["triangle_count"] != candidate["triangle_count"]:
        raise RuntimeError(
            f"{context} triangle count drifted: source={source}, candidate={candidate}"
        )
    for field in ("bounds_cm", "origin_cm"):
        if any(
            not math.isclose(actual, expected, rel_tol=1.0e-5, abs_tol=1.0e-3)
            for actual, expected in zip(candidate[field], source[field])
        ):
            raise RuntimeError(
                f"{context} geometry drifted: source={source}, candidate={candidate}"
            )
    return candidate


def _set_nanite(static_mesh, enabled: bool) -> None:
    settings = static_mesh.get_editor_property("nanite_settings")
    _set_property(settings, "enabled", bool(enabled))
    _set_property(static_mesh, "nanite_settings", settings)
    static_mesh.modify()


def _set_simple_collision(static_mesh, enabled: bool) -> int:
    library = unreal.EditorStaticMeshLibrary
    count = int(library.get_simple_collision_count(static_mesh))
    if enabled and count == 0:
        library.add_simple_collisions(static_mesh, unreal.ScriptingCollisionShapeType.BOX)
    elif not enabled and count:
        remover = getattr(library, "remove_collisions", None)
        if remover is None:
            raise RuntimeError("UE Python has no EditorStaticMeshLibrary.remove_collisions")
        remover(static_mesh)
    final_count = int(library.get_simple_collision_count(static_mesh))
    if enabled and final_count < 1:
        raise RuntimeError(f"solid mesh {static_mesh} has no simple collision")
    if not enabled and final_count != 0:
        raise RuntimeError(f"non-colliding mesh {static_mesh} retained simple collision")
    static_mesh.modify()
    return final_count


def _constant(material, value: float, x: int, y: int):
    expression = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant, x, y
    )
    _set_property(expression, "r", float(value))
    return expression


def _connect_expression(source, output: str, destination, candidates: Iterable[str]) -> str:
    for input_name in candidates:
        if unreal.MaterialEditingLibrary.connect_material_expressions(
            source, output, destination, input_name
        ):
            return input_name
    raise RuntimeError(f"could not connect material input candidates {tuple(candidates)}")


def _connect_property(source, outputs: Iterable[str], material_property) -> str:
    for output in outputs:
        if unreal.MaterialEditingLibrary.connect_material_property(
            source, output, material_property
        ):
            return output
    raise RuntimeError(f"could not connect material property from outputs {tuple(outputs)}")


def _blend_mode(name: str):
    enum_type = getattr(unreal, "BlendMode", None) or getattr(unreal, "MaterialBlendMode", None)
    value_name = "BLEND_MASKED" if name == "Masked" else "BLEND_OPAQUE"
    if enum_type is None or not hasattr(enum_type, value_name):
        raise RuntimeError(f"UE Python does not expose {value_name}")
    return getattr(enum_type, value_name)


def _create_toon_profile():
    return _create_or_load_asset(
        "TP_QS_B1_Toon", MATERIAL_ROOT, unreal.ToonProfile, unreal.ToonProfileFactory()
    )[0]


def _clear_material_expressions(material) -> None:
    for _ in range(256):
        expressions = list(unreal.MaterialEditingLibrary.get_material_expressions(material))
        if not expressions:
            return
        unreal.MaterialEditingLibrary.delete_material_expression(material, expressions[-1])
    remaining = int(unreal.MaterialEditingLibrary.get_num_material_expressions(material))
    raise RuntimeError(f"could not clear material graph after 256 deletes: {remaining} remain")


def _create_toon_master(toon_profile):
    material = _create_or_load_asset(
        "M_QS_B1_Toon", MATERIAL_ROOT, unreal.Material, unreal.MaterialFactoryNew()
    )[0]
    _clear_material_expressions(material)
    _set_property(material, "two_sided", False)
    _set_property(material, "blend_mode", _blend_mode("Opaque"))

    base_color = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionVectorParameter, -650, -100
    )
    _set_property(base_color, "parameter_name", "BaseColor")
    _set_property(base_color, "default_value", unreal.LinearColor(0.66, 0.48, 0.31, 1.0))
    metallic = _constant(material, 0.0, -650, 100)
    specular = _constant(material, 0.15, -650, 220)
    roughness = _constant(material, 0.85, -650, 340)
    toon = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionSubstrateToonBSDF, -150, 0
    )
    _set_property(toon, "toon_profile", toon_profile)
    _connect_expression(base_color, "RGB", toon, ("Base Color", "BaseColor"))
    _connect_expression(metallic, "", toon, ("Metallic",))
    _connect_expression(specular, "", toon, ("Specular",))
    _connect_expression(roughness, "", toon, ("Roughness",))
    _connect_property(toon, ("",), unreal.MaterialProperty.MP_FRONT_MATERIAL)
    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    compile_errors = [
        str(value) for value in unreal.MaterialEditingLibrary.recompile_material(material)
    ]
    if compile_errors:
        raise RuntimeError(f"toon material compilation failed: {compile_errors}")
    material.modify()
    toon_profile.modify()
    return material


def _append_scalar_pair(material, first_name: str, first_default: float, second_default: float, x: int, y: int):
    first = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionScalarParameter, x, y
    )
    _set_property(first, "parameter_name", first_name)
    _set_property(first, "default_value", float(first_default))
    second = _constant(material, second_default, x, y + 100)
    append = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionAppendVector, x + 180, y + 25
    )
    _connect_expression(first, "", append, ("A",))
    _connect_expression(second, "", append, ("B",))
    return append


def _create_foliage_master(toon_profile, texture):
    material = _create_or_load_asset(
        "M_QS_B1_FoliageCard", MATERIAL_ROOT, unreal.Material, unreal.MaterialFactoryNew()
    )[0]
    _clear_material_expressions(material)
    _set_property(material, "two_sided", True)
    _set_property(material, "blend_mode", _blend_mode("Masked"))
    _set_property(material, "opacity_mask_clip_value", 0.25)

    texcoord = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureCoordinate, -1000, -200
    )
    scale = _append_scalar_pair(material, "AtlasScaleX", 0.25, 1.0, -1000, 0)
    multiply = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -700, -150
    )
    _connect_expression(texcoord, "", multiply, ("A",))
    _connect_expression(scale, "", multiply, ("B",))
    offset = _append_scalar_pair(material, "AtlasOffsetX", 0.0, 0.0, -1000, 220)
    add = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionAdd, -470, -120
    )
    _connect_expression(multiply, "", add, ("A",))
    _connect_expression(offset, "", add, ("B",))
    sample = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionTextureSampleParameter2D, -230, -130
    )
    _set_property(sample, "parameter_name", "PlantAtlas")
    _set_property(sample, "texture", texture)
    uv_input = _connect_expression(add, "", sample, TEXTURE_UV_INPUT_CANDIDATES)
    reflected_inputs = [
        str(value)
        for value in unreal.MaterialEditingLibrary.get_material_expression_input_names(sample)
    ]
    if uv_input != "UVs" or "UVs" not in reflected_inputs:
        raise RuntimeError(
            f"foliage TextureSample UV pin readback failed: "
            f"connected={uv_input}, reflected={reflected_inputs}"
        )

    metallic = _constant(material, 0.0, 20, 100)
    specular = _constant(material, 0.10, 20, 220)
    roughness = _constant(material, 0.90, 20, 340)
    toon = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionSubstrateToonBSDF, 260, 0
    )
    _set_property(toon, "toon_profile", toon_profile)
    _connect_expression(sample, "RGB", toon, ("Base Color", "BaseColor"))
    _connect_expression(metallic, "", toon, ("Metallic",))
    _connect_expression(specular, "", toon, ("Specular",))
    _connect_expression(roughness, "", toon, ("Roughness",))
    _connect_property(toon, ("",), unreal.MaterialProperty.MP_FRONT_MATERIAL)
    _connect_property(sample, ("A", "Alpha"), unreal.MaterialProperty.MP_OPACITY_MASK)
    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    compile_errors = [
        str(value) for value in unreal.MaterialEditingLibrary.recompile_material(material)
    ]
    if compile_errors:
        raise RuntimeError(f"foliage material compilation failed: {compile_errors}")
    material.modify()
    return material


def _create_material_instance(
        name: str, parent, spec: dict[str, Any]) -> tuple[Any, list[dict[str, Any]]]:
    instance = _create_or_load_asset(
        name,
        MATERIAL_ROOT,
        unreal.MaterialInstanceConstant,
        unreal.MaterialInstanceConstantFactoryNew(),
    )[0]
    unreal.MaterialEditingLibrary.set_material_instance_parent(instance, parent)
    unreal.MaterialEditingLibrary.clear_all_material_instance_parameters(instance)
    write_audits = []
    if "base_color" in spec:
        color = spec["base_color"]
        setter_return = unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
            instance,
            "BaseColor",
            unreal.LinearColor(*[float(value) for value in color]),
        )
        observed = unreal.MaterialEditingLibrary.get_material_instance_vector_parameter_value(
            instance, "BaseColor"
        )
        audit = confirm_parameter_write(
            "BaseColor",
            color,
            [observed.r, observed.g, observed.b, observed.a],
            setter_return,
        )
        audit["material_instance"] = name
        write_audits.append(audit)
    if "atlas_scale_x" in spec:
        for parameter_name, key in (
            ("AtlasScaleX", "atlas_scale_x"),
            ("AtlasOffsetX", "atlas_offset_x"),
        ):
            expected = float(spec[key])
            setter_return = unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(
                instance, parameter_name, expected
            )
            observed = float(
                unreal.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(
                    instance, parameter_name
                )
            )
            audit = confirm_parameter_write(
                parameter_name, expected, observed, setter_return
            )
            audit["material_instance"] = name
            write_audits.append(audit)
    updater = getattr(unreal.MaterialEditingLibrary, "update_material_instance", None)
    if updater is not None:
        updater(instance)
    instance.modify()
    return instance, write_audits


def _import_plant_texture(spec: dict[str, Any]):
    texture = _run_import_task(
        spec["source_file"], PAPER2D_ROOT, "T_QS_B1_PlantProxy_4F"
    )
    _set_property(texture, "srgb", True)
    filter_enum = getattr(unreal.TextureFilter, "TF_BILINEAR", None)
    if filter_enum is None:
        filter_enum = getattr(unreal.TextureFilter, "TF_TRILINEAR", None)
    if filter_enum is None:
        raise RuntimeError("UE Python exposes neither TF_BILINEAR nor TF_TRILINEAR")
    _set_property(texture, "filter", filter_enum)
    texture.modify()
    size = [int(texture.blueprint_get_size_x()), int(texture.blueprint_get_size_y())]
    if size != [2048, 512]:
        raise RuntimeError(f"plant atlas must import as 2048x512, got {size}")
    return texture


def _author_materials(texture) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    specs = material_specs()
    assets: dict[str, Any] = {}
    toon_profile = _create_toon_profile()
    assets["TP_QS_B1_Toon"] = toon_profile
    toon_master = _create_toon_master(toon_profile)
    assets["M_QS_B1_Toon"] = toon_master
    foliage_master = _create_foliage_master(toon_profile, texture)
    assets["M_QS_B1_FoliageCard"] = foliage_master
    write_audits = []
    for name, spec in specs.items():
        if spec["kind"] == "toon_instance":
            assets[name], writes = _create_material_instance(name, toon_master, spec)
            write_audits.extend(writes)
        elif spec["kind"] == "foliage_instance":
            assets[name], writes = _create_material_instance(name, foliage_master, spec)
            write_audits.extend(writes)
    if set(assets) != set(specs):
        raise RuntimeError(f"material authoring count drifted: {sorted(set(specs) - set(assets))}")
    return assets, write_audits


def _material_map(assets: dict[str, Any], slot_materials: dict[str, str]) -> dict[str, Any]:
    result = {}
    for slot, path in slot_materials.items():
        name = path.rsplit("/", 1)[-1]
        material = assets.get(name)
        if material is None:
            raise RuntimeError(f"material asset is missing for slot {slot}: {path}")
        result[slot] = material
    return result


def _assign_materials_by_slot(static_mesh, slot_material_paths: dict[str, str], assets: dict[str, Any]) -> list[str]:
    slot_names = _mesh_slot_names(static_mesh)
    ordered_paths = material_paths_for_slots(slot_names, slot_material_paths)
    resolved = _material_map(assets, slot_material_paths)
    for index, slot_name in enumerate(slot_names):
        static_mesh.set_material(index, resolved[slot_name])
    static_mesh.modify()
    return ordered_paths


def _base_slot_material_paths(stable_id: str, category: str) -> dict[str, str]:
    if category == "building":
        return {
            "Wall": f"{MATERIAL_ROOT}/MI_QS_B1_Wall_Warm",
            "Timber": f"{MATERIAL_ROOT}/MI_QS_B1_Timber_Dark",
            "WindowPaper": f"{MATERIAL_ROOT}/MI_QS_B1_Window_Paper",
            "Roof": f"{MATERIAL_ROOT}/MI_QS_B1_Roof_Orange",
        }
    mapping = {
        "Timber": f"{MATERIAL_ROOT}/MI_QS_B1_Timber_Dark",
        "WindowPaper": f"{MATERIAL_ROOT}/MI_QS_B1_Window_Paper",
        "Prop": f"{MATERIAL_ROOT}/MI_QS_B1_Prop_Jade",
        "Foliage": f"{MATERIAL_ROOT}/MI_QS_B1_Foliage_F0",
        "Mountain": f"{MATERIAL_ROOT}/MI_QS_B1_Mountain_BlueGrey",
    }
    return {slot: mapping[slot] for slot in PROP_SLOT_TABLE[stable_id]}


def _import_and_configure_source_meshes(
        import_specs: list[dict[str, Any]], material_assets: dict[str, Any]) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    meshes = {}
    audits = []
    for spec in import_specs:
        mesh = _run_import_task(
            spec["source_file"], MESH_ROOT, spec["asset_name"], _static_mesh_import_options()
        )
        if not isinstance(mesh, unreal.StaticMesh):
            raise RuntimeError(f"FBX did not import as StaticMesh: {spec['asset_path']}")
        actual_slots = _mesh_slot_names(mesh)
        if actual_slots != spec["slot_names"]:
            raise RuntimeError(
                f"material slot order drifted for {spec['id']}: "
                f"expected {spec['slot_names']}, got {actual_slots}"
            )
        bounds = _mesh_bounds_cm(mesh)
        geometry_audit = source_geometry_audit(
            spec,
            bounds,
            int(mesh.get_num_triangles(0)),
            _mesh_origin_cm(mesh),
        )
        slot_materials = _base_slot_material_paths(spec["id"], spec["category"])
        assigned = _assign_materials_by_slot(mesh, slot_materials, material_assets)
        _set_nanite(mesh, False)
        collision_count = _set_simple_collision(mesh, spec["collision_enabled"])
        meshes[spec["id"]] = mesh
        audits.append({
            **geometry_audit,
            "slot_names": actual_slots,
            "material_paths": assigned,
            "simple_collision_count": collision_count,
            "nanite_enabled": False,
        })
    return meshes, audits


def _author_building_variants(
        manifest: dict[str, Any], source_meshes: dict[str, Any], material_assets: dict[str, Any]) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    variants = {}
    audits = []
    for spec in building_variant_specs(manifest):
        source_mesh = source_meshes[spec["archetype_id"]]
        if unreal.EditorAssetLibrary.does_asset_exist(spec["asset_path"]):
            if not unreal.EditorAssetLibrary.delete_asset(spec["asset_path"]):
                raise RuntimeError(f"could not remove stale variant {spec['asset_path']}")
        mesh = unreal.EditorAssetLibrary.duplicate_asset(
            spec["source_asset"], spec["asset_path"]
        )
        if mesh is None or not isinstance(mesh, unreal.StaticMesh):
            raise RuntimeError(f"could not create StaticMesh variant {spec['asset_path']}")
        if _mesh_slot_names(mesh) != list(BUILDING_SLOT_ORDER):
            raise RuntimeError(f"building variant slot contract drifted: {spec['asset_path']}")
        assigned = _assign_materials_by_slot(mesh, spec["slot_materials"], material_assets)
        _set_nanite(mesh, False)
        collision_count = _set_simple_collision(mesh, True)
        geometry = _require_matching_mesh_geometry(
            source_mesh, mesh, f"building variant {spec['asset_path']}"
        )
        variants[spec["asset_path"]] = mesh
        audits.append({
            "asset_path": spec["asset_path"],
            "source_asset": spec["source_asset"],
            "archetype_id": spec["archetype_id"],
            "roof_palette": spec["roof_palette"],
            "material_paths": assigned,
            **geometry,
            "simple_collision_count": collision_count,
            "nanite_enabled": False,
        })
    return variants, audits


def _author_paper2d(
        texture,
        spec: dict[str, Any],
        material_assets: dict[str, Any]) -> tuple[dict[str, Any], Any]:
    material_name = spec["default_material"].rsplit("/", 1)[-1]
    default_material = material_assets.get(material_name)
    if default_material is None:
        raise RuntimeError(f"Paper2D default material is missing: {spec['default_material']}")
    no_collision = getattr(unreal.SpriteCollisionMode, "NONE", None)
    if no_collision is None:
        raise RuntimeError("UE Python does not expose SpriteCollisionMode.NONE")
    sprites = {}
    for item in spec["sprites"]:
        sprite = _create_or_load_asset(
            item["asset_name"],
            PAPER2D_ROOT,
            unreal.PaperSprite,
            unreal.PaperSpriteFactory(),
        )[0]
        _set_property(sprite, "source_texture", texture)
        _set_property(sprite, "source_uv", unreal.Vector2D(*item["source_uv"]))
        _set_property(sprite, "source_dimension", unreal.Vector2D(*item["source_dimension"]))
        _set_property(
            sprite,
            "source_texture_dimension",
            unreal.Vector2D(*item["source_texture_dimension"]),
        )
        _set_property(sprite, "pixels_per_unreal_unit", item["pixels_per_unreal_unit"])
        _set_property(sprite, "pivot_mode", unreal.SpritePivotMode.CUSTOM)
        _set_property(sprite, "custom_pivot_point", unreal.Vector2D(*item["custom_pivot"]))
        _set_property(sprite, "default_material", default_material)
        _set_property(sprite, "sprite_collision_domain", no_collision)
        sprite.modify()
        sprites[item["asset_path"]] = sprite

    flipbook = _create_or_load_asset(
        "FB_QS_B1_Plant_Sway",
        PAPER2D_ROOT,
        unreal.PaperFlipbook,
        unreal.PaperFlipbookFactory(),
    )[0]
    sprite_list = [sprites[item["asset_path"]] for item in spec["sprites"]]
    keyframes = []
    for frame_index in spec["frame_order"]:
        keyframe = unreal.PaperFlipbookKeyFrame()
        _set_property(keyframe, "sprite", sprite_list[frame_index])
        _set_property(keyframe, "frame_run", 1)
        keyframes.append(keyframe)
    _set_property(flipbook, "frames_per_second", spec["frames_per_second"])
    _set_property(flipbook, "key_frames", keyframes)
    _set_property(flipbook, "default_material", default_material)
    invalidate = getattr(flipbook, "invalidate_cached_data", None)
    if invalidate is not None:
        invalidate()
    flipbook.modify()
    return sprites, flipbook


def _save_b1_assets(asset_paths: Iterable[str]) -> None:
    for path in sorted(set(asset_paths)):
        require_b1_asset_path(path)
        if not unreal.EditorAssetLibrary.does_asset_exist(path):
            raise RuntimeError(f"refusing save because expected B1 asset is absent: {path}")
        if not unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False):
            raise RuntimeError(f"failed to save B1 asset: {path}")


def _reload_b1_packages(asset_paths: Iterable[str]) -> dict[str, Any]:
    paths = sorted(set(require_b1_asset_path(path) for path in asset_paths))
    packages = []
    for path in paths:
        asset = _load_asset(path)
        if asset is None:
            raise RuntimeError(f"cannot reload absent B1 asset: {path}")
        packages.append(asset.get_outermost())
    outcome = unreal.EditorLoadingAndSavingUtils.reload_packages(
        packages,
        unreal.ReloadPackagesInteractionMode.ASSUME_POSITIVE,
    )
    if not isinstance(outcome, tuple) or len(outcome) != 2:
        raise RuntimeError(f"unexpected reload_packages result: {outcome!r}")
    any_reloaded, error_message = outcome
    error_text = str(error_message).strip()
    if error_text:
        raise RuntimeError(f"B1 package reload failed: {error_text}")
    if not any_reloaded:
        raise RuntimeError("B1 package reload reported that no package was reloaded")
    return {"package_count": len(packages), "any_reloaded": bool(any_reloaded)}


def _observed_mesh_material_paths(static_mesh) -> list[str]:
    result = []
    for index in range(len(_mesh_slot_names(static_mesh))):
        result.append(_asset_path(static_mesh.get_material(index)))
    return result


def _observe_source_mesh(spec: dict[str, Any]) -> dict[str, Any]:
    mesh = _load_asset(spec["asset_path"])
    if mesh is None or not isinstance(mesh, unreal.StaticMesh):
        raise RuntimeError(f"post-save source mesh is missing: {spec['asset_path']}")
    audit = source_geometry_audit(
        spec,
        _mesh_bounds_cm(mesh),
        int(mesh.get_num_triangles(0)),
        _mesh_origin_cm(mesh),
    )
    actual_slots = _mesh_slot_names(mesh)
    if actual_slots != spec["slot_names"]:
        raise RuntimeError(f"post-save source slot drifted for {spec['id']}: {actual_slots}")
    expected_materials = material_paths_for_slots(
        actual_slots,
        _base_slot_material_paths(spec["id"], spec["category"]),
    )
    actual_materials = _observed_mesh_material_paths(mesh)
    if actual_materials != expected_materials:
        raise RuntimeError(
            f"post-save source material drifted for {spec['id']}: "
            f"expected {expected_materials}, got {actual_materials}"
        )
    collision_count = int(unreal.EditorStaticMeshLibrary.get_simple_collision_count(mesh))
    if bool(collision_count) != bool(spec["collision_enabled"]):
        raise RuntimeError(f"post-save collision drifted for {spec['id']}: {collision_count}")
    nanite_enabled = bool(mesh.get_editor_property("nanite_settings").enabled)
    if nanite_enabled:
        raise RuntimeError(f"post-save Nanite unexpectedly enabled for {spec['id']}")
    return {
        **audit,
        "slot_names": actual_slots,
        "material_paths": actual_materials,
        "simple_collision_count": collision_count,
        "nanite_enabled": nanite_enabled,
    }


def _observe_building_variant(
        spec: dict[str, Any], source_mesh) -> dict[str, Any]:
    mesh = _load_asset(spec["asset_path"])
    if mesh is None or not isinstance(mesh, unreal.StaticMesh):
        raise RuntimeError(f"post-save building variant is missing: {spec['asset_path']}")
    geometry = _require_matching_mesh_geometry(
        source_mesh, mesh, f"post-save building variant {spec['asset_path']}"
    )
    slots = _mesh_slot_names(mesh)
    expected_materials = material_paths_for_slots(slots, spec["slot_materials"])
    actual_materials = _observed_mesh_material_paths(mesh)
    if slots != list(BUILDING_SLOT_ORDER) or actual_materials != expected_materials:
        raise RuntimeError(
            f"post-save variant material contract drifted for {spec['asset_path']}: "
            f"slots={slots}, materials={actual_materials}"
        )
    collision_count = int(unreal.EditorStaticMeshLibrary.get_simple_collision_count(mesh))
    nanite_enabled = bool(mesh.get_editor_property("nanite_settings").enabled)
    if collision_count < 1 or nanite_enabled:
        raise RuntimeError(
            f"post-save variant runtime policy drifted for {spec['asset_path']}: "
            f"collision={collision_count}, nanite={nanite_enabled}"
        )
    return {
        "asset_path": spec["asset_path"],
        "source_asset": spec["source_asset"],
        "archetype_id": spec["archetype_id"],
        "roof_palette": spec["roof_palette"],
        **geometry,
        "slot_names": slots,
        "material_paths": actual_materials,
        "simple_collision_count": collision_count,
        "nanite_enabled": nanite_enabled,
    }


def _observe_material_instances() -> dict[str, Any]:
    observed = {}
    for name, spec in material_specs().items():
        if spec["kind"] not in ("toon_instance", "foliage_instance"):
            continue
        path = f"{MATERIAL_ROOT}/{name}"
        instance = _load_asset(path)
        if instance is None or not isinstance(instance, unreal.MaterialInstanceConstant):
            raise RuntimeError(f"post-save material instance is missing: {path}")
        expected_parent = f"{MATERIAL_ROOT}/{spec['parent']}"
        actual_parent = _asset_path(instance.get_editor_property("parent"))
        if actual_parent != expected_parent:
            raise RuntimeError(
                f"post-save MI parent drifted for {name}: "
                f"expected {expected_parent}, got {actual_parent}"
            )
        entry = {"asset_path": path, "parent": actual_parent}
        if "base_color" in spec:
            color = unreal.MaterialEditingLibrary.get_material_instance_vector_parameter_value(
                instance, "BaseColor"
            )
            actual = [float(color.r), float(color.g), float(color.b), float(color.a)]
            if any(
                not math.isclose(value, float(expected), abs_tol=1.0e-5)
                for value, expected in zip(actual, spec["base_color"])
            ):
                raise RuntimeError(f"post-save BaseColor drifted for {name}: {actual}")
            entry["base_color"] = [round(value, 6) for value in actual]
        if "atlas_scale_x" in spec:
            for parameter_name, key in (
                ("AtlasScaleX", "atlas_scale_x"),
                ("AtlasOffsetX", "atlas_offset_x"),
            ):
                actual = float(
                    unreal.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(
                        instance, parameter_name
                    )
                )
                if not math.isclose(actual, float(spec[key]), abs_tol=1.0e-6):
                    raise RuntimeError(
                        f"post-save {parameter_name} drifted for {name}: {actual}"
                    )
                entry[key] = actual
        observed[name] = entry
    return observed


def _observe_material_masters() -> dict[str, Any]:
    observed = {}
    specs = material_specs()
    for name in ("M_QS_B1_Toon", "M_QS_B1_FoliageCard"):
        material = _load_asset(f"{MATERIAL_ROOT}/{name}")
        if material is None or not isinstance(material, unreal.Material):
            raise RuntimeError(f"post-save material master is missing: {name}")
        front = unreal.MaterialEditingLibrary.get_material_property_input_node(
            material, unreal.MaterialProperty.MP_FRONT_MATERIAL
        )
        if (
            front is None
            or front.get_class().get_name() != "MaterialExpressionSubstrateToonBSDF"
        ):
            raise RuntimeError(f"post-save FrontMaterial is disconnected: {name}")
        expressions = list(
            unreal.MaterialEditingLibrary.get_material_expressions(material)
        )
        expected_expression_count = int(specs[name]["expected_expression_count"])
        if len(expressions) != expected_expression_count:
            raise RuntimeError(
                f"post-save material expression count drifted for {name}: "
                f"expected {expected_expression_count}, got {len(expressions)}"
            )
        entry = {
            "asset_path": f"{MATERIAL_ROOT}/{name}",
            "two_sided": bool(material.get_editor_property("two_sided")),
            "blend_mode": str(material.get_editor_property("blend_mode")),
            "front_material_node": front.get_class().get_name(),
            "expression_count": len(expressions),
        }
        if name == "M_QS_B1_FoliageCard":
            opacity = unreal.MaterialEditingLibrary.get_material_property_input_node(
                material, unreal.MaterialProperty.MP_OPACITY_MASK
            )
            samples = [
                expression
                for expression in expressions
                if expression.get_class().get_name() == "MaterialExpressionTextureSampleParameter2D"
            ]
            if opacity is None or len(samples) != 1:
                expression_classes = [
                    expression.get_class().get_name()
                    for expression in expressions
                ]
                raise RuntimeError(
                    "post-save foliage material graph is incomplete: "
                    f"opacity={opacity}, samples={len(samples)}, "
                    f"expressions={expression_classes}"
                )
            uv_nodes = unreal.MaterialEditingLibrary.get_inputs_for_material_expression(
                material, samples[0]
            )
            uv_node_classes = [
                node.get_class().get_name() for node in uv_nodes if node is not None
            ]
            null_input_count = sum(node is None for node in uv_nodes)
            if (
                str(opacity.get_path_name()) != str(samples[0].get_path_name())
                or uv_node_classes != ["MaterialExpressionAdd"]
            ):
                raise RuntimeError(
                    "post-save foliage compiled connectivity drifted: "
                    f"opacity={opacity.get_path_name()}, sample={samples[0].get_path_name()}, "
                    f"uv_inputs={uv_node_classes}, null_inputs={null_input_count}"
                )
            uv_inputs = [
                str(value)
                for value in unreal.MaterialEditingLibrary.get_material_expression_input_names(
                    samples[0]
                )
            ]
            if "UVs" not in uv_inputs:
                raise RuntimeError(f"post-save foliage UV pin readback failed: {uv_inputs}")
            entry["opacity_mask_node"] = opacity.get_class().get_name()
            entry["texture_sample_inputs"] = uv_inputs
            entry["texture_sample_input_nodes"] = uv_node_classes
            entry["texture_sample_unconnected_optional_inputs"] = null_input_count
        observed[name] = entry
    if observed["M_QS_B1_Toon"]["two_sided"]:
        raise RuntimeError("post-save opaque Toon master unexpectedly became two-sided")
    if not observed["M_QS_B1_FoliageCard"]["two_sided"]:
        raise RuntimeError("post-save foliage master is not two-sided")
    return observed


def _vector2_list(value) -> list[float]:
    return [round(float(value.x), 4), round(float(value.y), 4)]


def _observe_sprites(spec: dict[str, Any]) -> dict[str, Any]:
    observed = {}
    no_collision = getattr(unreal.SpriteCollisionMode, "NONE")
    for item in spec["sprites"]:
        sprite = _load_asset(item["asset_path"])
        if sprite is None or not isinstance(sprite, unreal.PaperSprite):
            raise RuntimeError(f"post-save PaperSprite is missing: {item['asset_path']}")
        entry = {
            "source_uv": _vector2_list(sprite.get_editor_property("source_uv")),
            "source_dimension": _vector2_list(sprite.get_editor_property("source_dimension")),
            "custom_pivot": _vector2_list(sprite.get_editor_property("custom_pivot_point")),
            "pixels_per_unreal_unit": float(sprite.get_editor_property("pixels_per_unreal_unit")),
            "default_material": _asset_path(sprite.get_editor_property("default_material")),
            "source_texture": _asset_path(sprite.get_editor_property("source_texture")),
            "collision_domain": str(sprite.get_editor_property("sprite_collision_domain")),
        }
        if (
            entry["source_uv"] != [float(value) for value in item["source_uv"]]
            or entry["source_dimension"] != [float(value) for value in item["source_dimension"]]
            or entry["custom_pivot"] != [float(value) for value in item["custom_pivot"]]
            or not math.isclose(
                entry["pixels_per_unreal_unit"], item["pixels_per_unreal_unit"], abs_tol=1.0e-6
            )
            or entry["default_material"] != item["default_material"]
            or entry["source_texture"] != spec["texture_asset"]
            or sprite.get_editor_property("sprite_collision_domain") != no_collision
            or sprite.get_editor_property("pivot_mode") != unreal.SpritePivotMode.CUSTOM
        ):
            raise RuntimeError(f"post-save PaperSprite contract drifted: {item['asset_path']} {entry}")
        observed[item["asset_path"]] = entry
    return observed


def _observe_flipbook(spec: dict[str, Any]) -> dict[str, Any]:
    flipbook = _load_asset(spec["flipbook_asset"])
    if flipbook is None or not isinstance(flipbook, unreal.PaperFlipbook):
        raise RuntimeError(f"post-save flipbook is missing: {spec['flipbook_asset']}")
    keyframes = list(flipbook.get_editor_property("key_frames"))
    sprite_paths = [
        _asset_path(frame.get_editor_property("sprite")) for frame in keyframes
    ]
    expected_paths = [
        spec["sprites"][index]["asset_path"] for index in spec["frame_order"]
    ]
    frame_runs = [int(frame.get_editor_property("frame_run")) for frame in keyframes]
    fps = float(flipbook.get_editor_property("frames_per_second"))
    default_material = _asset_path(flipbook.get_editor_property("default_material"))
    if (
        sprite_paths != expected_paths
        or frame_runs != [1] * len(expected_paths)
        or not math.isclose(fps, spec["frames_per_second"], abs_tol=1.0e-6)
        or default_material != spec["default_material"]
    ):
        raise RuntimeError(
            f"post-save flipbook contract drifted: sprites={sprite_paths}, "
            f"runs={frame_runs}, fps={fps}, material={default_material}"
        )
    return {
        "asset_path": spec["flipbook_asset"],
        "frames_per_second": fps,
        "sprite_paths": sprite_paths,
        "frame_runs": frame_runs,
        "default_material": default_material,
    }


def _observe_texture(spec: dict[str, Any]) -> dict[str, Any]:
    texture = _load_asset(spec["texture_asset"])
    if texture is None:
        raise RuntimeError(f"post-save plant texture is missing: {spec['texture_asset']}")
    resident_size = [
        int(texture.blueprint_get_size_x()), int(texture.blueprint_get_size_y())
    ]
    subsystem = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    tag_values = {
        str(key): str(value)
        for key, value in subsystem.get_tag_values(spec["texture_asset"]).items()
    }
    size = parse_texture_dimensions(tag_values.get("Dimensions", ""))
    filter_value = str(texture.get_editor_property("filter"))
    srgb = bool(texture.get_editor_property("srgb"))
    if size != [2048, 512] or "BILINEAR" not in filter_value.upper() or not srgb:
        raise RuntimeError(
            f"post-save plant texture drifted: imported_size={size}, "
            f"resident_size={resident_size}, filter={filter_value}, srgb={srgb}"
        )
    return {
        "asset_path": spec["texture_asset"],
        "size": size,
        "resident_size": resident_size,
        "filter": filter_value,
        "srgb": srgb,
    }


def post_save_observed_audit(
        manifest: dict[str, Any],
        paper_spec: dict[str, Any],
        project_root: Path) -> dict[str, Any]:
    source_specs = source_import_specs(manifest, project_root)
    observed_source_meshes = [_observe_source_mesh(spec) for spec in source_specs]
    source_meshes = {
        spec["id"]: _load_asset(spec["asset_path"]) for spec in source_specs
    }
    observed_building_variants = [
        _observe_building_variant(spec, source_meshes[spec["archetype_id"]])
        for spec in building_variant_specs(manifest)
    ]
    observed_material_instances = _observe_material_instances()
    observed_sprites = _observe_sprites(paper_spec)
    observed_flipbook = _observe_flipbook(paper_spec)
    observed_texture = _observe_texture(paper_spec)
    return {
        "observed_source_meshes": observed_source_meshes,
        "observed_building_variants": observed_building_variants,
        "observed_material_masters": _observe_material_masters(),
        "observed_material_instances": observed_material_instances,
        "observed_sprites": observed_sprites,
        "observed_flipbook": observed_flipbook,
        "observed_texture": observed_texture,
    }


def author_assets(project_root: Path = PROJECT_ROOT) -> dict[str, Any]:
    _require_unreal()
    dirty_maps_before = _require_no_dirty_maps("Qingshan B1 asset authoring")
    manifest = load_source_manifest(Path(project_root))
    contract = expected_result_contract(manifest, Path(project_root))
    for directory in (ASSET_ROOT, MATERIAL_ROOT, MESH_ROOT, MESH_VARIANT_ROOT, PAPER2D_ROOT):
        _ensure_directory(directory)

    paper_spec = paper2d_spec(Path(project_root))
    texture = _import_plant_texture(paper_spec)
    material_assets, material_parameter_writes = _author_materials(texture)
    source_meshes, source_audits = _import_and_configure_source_meshes(
        source_import_specs(manifest, Path(project_root)), material_assets
    )
    variants, variant_audits = _author_building_variants(
        manifest, source_meshes, material_assets
    )
    sprites, flipbook = _author_paper2d(texture, paper_spec, material_assets)

    _require_no_dirty_maps("Qingshan B1 pre-save audit")
    _save_b1_assets(contract["expected_assets"])
    material_count = len(material_assets)
    sprite_assets = sorted(sprites)
    flipbook_class = flipbook.get_class().get_name()
    del texture, material_assets, source_meshes, variants, sprites, flipbook
    unreal.SystemLibrary.collect_garbage()
    reload_audit = _reload_b1_packages(contract["expected_assets"])
    observed = post_save_observed_audit(manifest, paper_spec, Path(project_root))
    dirty_maps_after = _require_no_dirty_maps("Qingshan B1 post-save audit")
    return {
        "marker": "GAMEXXK_QINGSHAN_B1_ASSETS",
        "success": True,
        **contract,
        "source_manifest": str(
            Path(project_root) / "Saved" / "Automation" / "QingshanB1ProxyKit"
            / "proxy-kit-manifest.json"
        ),
        "source_meshes": observed["observed_source_meshes"],
        "building_variants": observed["observed_building_variants"],
        "authoring_source_meshes": source_audits,
        "authoring_building_variants": variant_audits,
        "material_count": material_count,
        "material_parameter_writes": material_parameter_writes,
        "texture_asset": paper_spec["texture_asset"],
        "texture_filter": paper_spec["texture_filter"],
        "sprite_assets": sprite_assets,
        "flipbook_asset": paper_spec["flipbook_asset"],
        "flipbook_keyframe_count": len(paper_spec["frame_order"]),
        "dirty_map_packages_before": dirty_maps_before,
        "dirty_map_packages_after": dirty_maps_after,
        "saved_asset_count": len(contract["expected_assets"]),
        "flipbook_class": flipbook_class,
        "reload_audit": reload_audit,
        "post_save_observed_audit": observed,
    }


def main() -> None:
    try:
        result = author_assets(PROJECT_ROOT)
    except Exception as error:
        result = {
            "marker": "GAMEXXK_QINGSHAN_B1_ASSETS",
            "success": False,
            "error": str(error),
            "exception_type": type(error).__name__,
        }
        if unreal is not None:
            unreal.log_error(f"Qingshan B1 asset authoring failed: {error}")
    print(json.dumps(result, ensure_ascii=False, sort_keys=True, separators=(",", ":"), allow_nan=False))
    if result.get("success") is not True:
        raise RuntimeError(result.get("error", "Qingshan B1 asset authoring failed"))


if __name__ == "__main__":
    main()
