"""Host-side tests for the Qingshan B1 dress overlay contract."""

from __future__ import annotations

from collections import Counter
import copy
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import tempfile
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[1]
MODULE_PATH = PROJECT_ROOT / "Content" / "Python" / "gamexxk_qingshan_dress_b1_config.py"
B0R_MODULE_PATH = PROJECT_ROOT / "Content" / "Python" / "gamexxk_qingshan_whitebox_config.py"
OVERLAY_PATH = PROJECT_ROOT / "Config" / "GameXXK" / "TownPCG" / "QingshanDressB1.json"

PROTECTED_FILES = {
    "Content/GameXXK/Maps/L_QingshanInn.umap":
        "a3639b38623d00e8ad3e5a610a3e1695a47b38c1d1e6fedb8115e1e9fdf5c8a8",
    "Content/GameXXK/Maps/Dev/L_Qingshan_PCG_Whitebox_B0R.umap":
        "74292340df0cea97d99e905dd193a921038326bfec2f3ce034a5e9f70bd3f107",
    "Config/GameXXK/TownPCG/QingshanWhiteboxB0R.json":
        "3f231876d0083bffe28ee555b60af1a20b0966edbde9dc4bb6f2647e920eadb1",
}
ARCHETYPES = {
    "gable_shop", "tall_house", "wide_house",
    "courtyard_wing", "bridge_house", "dock_shed",
}
ROOF_PALETTES = {"orange", "teal", "indigo", "ochre"}
ASSET_ROOT = "/Game/GameXXK/Environment/TownPCG/B1"
EXPECTED_ASSET_CATALOG = {
    "building_meshes": {
        "gable_shop": f"{ASSET_ROOT}/Meshes/SM_QS_B1_GableShop",
        "tall_house": f"{ASSET_ROOT}/Meshes/SM_QS_B1_TallHouse",
        "wide_house": f"{ASSET_ROOT}/Meshes/SM_QS_B1_WideHouse",
        "courtyard_wing": f"{ASSET_ROOT}/Meshes/SM_QS_B1_CourtyardWing",
        "bridge_house": f"{ASSET_ROOT}/Meshes/SM_QS_B1_BridgeHouse",
        "dock_shed": f"{ASSET_ROOT}/Meshes/SM_QS_B1_DockShed",
    },
    "prop_meshes": {
        "sign": f"{ASSET_ROOT}/Meshes/SM_QS_B1_Sign",
        "lantern": f"{ASSET_ROOT}/Meshes/SM_QS_B1_Lantern",
        "banner": f"{ASSET_ROOT}/Meshes/SM_QS_B1_Banner",
        "fence": f"{ASSET_ROOT}/Meshes/SM_QS_B1_Fence",
        "crate": f"{ASSET_ROOT}/Meshes/SM_QS_B1_Crate",
        "stall": f"{ASSET_ROOT}/Meshes/SM_QS_B1_Stall",
        "well": f"{ASSET_ROOT}/Meshes/SM_QS_B1_Well",
        "cart": f"{ASSET_ROOT}/Meshes/SM_QS_B1_Cart",
        "rock": f"{ASSET_ROOT}/Meshes/SM_QS_B1_Rock",
        "dock_post": f"{ASSET_ROOT}/Meshes/SM_QS_B1_DockPost",
    },
    "plant_card_mesh": f"{ASSET_ROOT}/Meshes/SM_QS_B1_PlantCard",
    "plant_flipbook": f"{ASSET_ROOT}/Paper2D/FB_QS_B1_Plant_Sway",
    "mountain_mesh": f"{ASSET_ROOT}/Meshes/SM_QS_B1_Mountain",
}
ADDITIONAL_PLOT_IDS = {
    "BLD_S_11", "BLD_S_12", "BLD_M_06", "BLD_S_13", "BLD_S_14",
    "BLD_S_15", "BLD_M_07", "BLD_S_16", "BLD_S_17", "BLD_S_18",
}
GEOMETRY_FIELDS = (
    "id", "size_class", "location_cm", "yaw_degrees",
    "size_cm", "entrance_axis", "cluster_id",
)


def _load_module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"unable to import {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


MODULE = _load_module("gamexxk_qingshan_dress_b1_config", MODULE_PATH)
B0R_MODULE = _load_module("gamexxk_qingshan_whitebox_config_for_b1", B0R_MODULE_PATH)


def _point_segment_distance(point, start, end):
    px, py = point[:2]
    ax, ay = start[:2]
    bx, by = end[:2]
    dx, dy = bx - ax, by - ay
    length_squared = dx * dx + dy * dy
    if length_squared == 0:
        return math.hypot(px - ax, py - ay)
    t = max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / length_squared))
    return math.hypot(px - (ax + t * dx), py - (ay + t * dy))


def _polyline_distance(point, points):
    return min(
        _point_segment_distance(point, start, end)
        for start, end in zip(points, points[1:])
    )


def _stable_frame_variant(seed, stable_id):
    digest = hashlib.sha256(f"{seed}:{stable_id}".encode("utf-8")).hexdigest()
    return int(digest[:8], 16) % 4


class QingshanDressB1ConfigTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.base = B0R_MODULE.load_config()
        cls.data = MODULE.load_config()
        cls.overlay = json.loads(OVERLAY_PATH.read_text(encoding="utf-8"))

    def assert_invalid(self, data, pattern):
        with self.assertRaisesRegex(ValueError, pattern):
            MODULE.validate_config(data, base_config=self.base)

    def test_coordinate_contract_is_anchor_local(self):
        data = self.data
        self.assertEqual(data["coordinate_space"], "anchor_local")
        self.assertEqual(data["coordinate_reference_actor"], "QingshanInn_TownExit")
        self.assertEqual(data["landscape"]["center_local_cm"], [-15500.0, 0.0, 0.0])

    def test_landscape_origin_and_height_encoding_are_unambiguous(self):
        landscape = self.data["landscape"]
        self.assertEqual(landscape["resolution"], [505, 505])
        self.assertEqual(landscape["scale_cm"], [100.0, 100.0, 100.0])
        expected_origin = [
            landscape["center_local_cm"][axis]
            - ((landscape["resolution"][axis] - 1) * landscape["scale_cm"][axis] / 2.0)
            for axis in range(2)
        ] + [landscape["center_local_cm"][2]]
        self.assertEqual(landscape["origin_local_cm"], expected_origin)
        self.assertEqual(landscape["origin_local_cm"], [-40700.0, -25200.0, 0.0])
        self.assertEqual(
            landscape["height_encoding"],
            {
                "value_type": "uint16",
                "formula": "uint16=clamp(round(32768+elevation_cm*128/scale_z),0,65535)",
            },
        )

    def test_protected_files_have_exact_raw_hashes(self):
        self.assertEqual(self.data["protected_files"], PROTECTED_FILES)
        observed = MODULE.validate_protected_file_hashes(PROTECTED_FILES, PROJECT_ROOT)
        self.assertEqual(observed, PROTECTED_FILES)

    def test_protected_file_hash_api_rejects_raw_byte_mismatch(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            target = root / "protected.bin"
            target.write_bytes(b"approved bytes")
            expected = hashlib.sha256(target.read_bytes()).hexdigest()
            self.assertEqual(
                MODULE.validate_protected_file_hashes({"protected.bin": expected}, root),
                {"protected.bin": expected},
            )
            target.write_bytes(b"mutated bytes")
            with self.assertRaisesRegex(ValueError, "raw SHA-256 mismatch"):
                MODULE.validate_protected_file_hashes({"protected.bin": expected}, root)

    def test_overlay_keeps_b0r_geometry_out_of_style_assignments(self):
        assignments = self.overlay["base_plot_assignments"]
        self.assertEqual(len(assignments), 16)
        self.assertEqual(
            {tuple(sorted(item)) for item in assignments},
            {("archetype_id", "id", "roof_palette")},
        )

    def test_all_b0r_plot_geometry_is_copied_without_change(self):
        merged_by_id = {item["id"]: item for item in self.data["building_plots"]}
        for source in self.base["building_plots"]:
            observed = merged_by_id[source["id"]]
            self.assertEqual(
                {field: observed[field] for field in GEOMETRY_FIELDS},
                {field: source[field] for field in GEOMETRY_FIELDS},
            )

    def test_building_and_style_counts_are_exact(self):
        plots = self.data["building_plots"]
        self.assertEqual(len(plots), 26)
        self.assertEqual(self.data["base_building_count"], 16)
        self.assertEqual(self.data["additional_building_count"], 10)
        self.assertEqual(Counter(p["size_class"] for p in plots), {
            "large": 1, "medium": 7, "small": 18,
        })
        self.assertEqual({p["archetype_id"] for p in plots}, ARCHETYPES)
        self.assertEqual({p["roof_palette"] for p in plots}, ROOF_PALETTES)
        self.assertEqual(
            {p["id"] for p in plots[16:]},
            ADDITIONAL_PLOT_IDS,
        )

    def test_building_pcg_groups_include_real_material_variants(self):
        roof_materials = self.data["building_materials"]["roof_palette_materials"]
        window_material = self.data["building_materials"]["shared_slot_materials"]["WindowPaper"]
        self.assertEqual(set(roof_materials), ROOF_PALETTES)
        self.assertTrue(window_material.endswith("/MI_QS_B1_Window_Paper"))
        for plot in self.data["building_plots"]:
            self.assertEqual(
                plot["pcg_group_key"],
                f"{plot['archetype_id']}__{plot['roof_palette']}",
            )
            self.assertEqual(plot["material_overrides"], {
                "Roof": roof_materials[plot["roof_palette"]],
                "WindowPaper": window_material,
            })

    def test_asset_catalog_and_b1_roots_are_exact(self):
        self.assertEqual(self.data["asset_root"], ASSET_ROOT)
        self.assertEqual(self.data["asset_catalog"], EXPECTED_ASSET_CATALOG)
        self.assertEqual(self.data["landscape"]["label"], "QS_B1_Landscape")
        self.assertEqual(
            self.data["landscape"]["heightmap_source"],
            "Content/ArtSource/Qingshan/B1/H_QS_B1_Terrain_505.png",
        )
        self.assertEqual(
            self.data["landscape"]["ground_material"],
            f"{ASSET_ROOT}/Materials/MI_QS_B1_Ground",
        )
        for category in ("building_meshes", "prop_meshes"):
            for path in self.data["asset_catalog"][category].values():
                self.assertTrue(path.startswith(f"{ASSET_ROOT}/"))
        for field in ("plant_card_mesh", "plant_flipbook", "mountain_mesh"):
            self.assertTrue(self.data["asset_catalog"][field].startswith(f"{ASSET_ROOT}/"))

    def test_additional_plots_do_not_overlap_buildings_roads_river_or_anchors(self):
        plots = self.data["building_plots"]
        additional = [item for item in plots if item["id"] in ADDITIONAL_PLOT_IDS]
        roads = {road["id"]: road for road in self.base["road_splines"]}
        river = self.base["river_splines"][0]
        for plot in additional:
            radius = math.hypot(plot["size_cm"][0] / 2.0, plot["size_cm"][1] / 2.0)
            for other in plots:
                if other["id"] == plot["id"]:
                    continue
                other_radius = math.hypot(other["size_cm"][0] / 2.0, other["size_cm"][1] / 2.0)
                distance = math.dist(plot["location_cm"][:2], other["location_cm"][:2])
                self.assertGreater(distance, radius + other_radius + 100.0)
            for road in roads.values():
                clearance = radius + road["width_cm"] / 2.0 + 250.0
                self.assertGreater(
                    _polyline_distance(plot["location_cm"], road["points_cm"]),
                    clearance,
                )
            self.assertGreater(
                _polyline_distance(plot["location_cm"], river["points_cm"]),
                radius + river["width_cm"] / 2.0 + 400.0,
            )
            for anchor in self.base["fixed_anchors"]:
                self.assertGreater(
                    math.dist(plot["location_cm"][:2], anchor["location_cm"][:2]),
                    radius + anchor["protected_radius_cm"],
                )

    def test_quickroad_network_and_material_contract_is_exact(self):
        quickroad = self.data["quickroad"]
        self.assertEqual(
            quickroad["road_material"],
            "/Game/GameXXK/Environment/TownPCG/B1/Materials/MI_QS_B1_Road_Earth",
        )
        self.assertEqual(
            [(n["tag"], n["source_spline_id"], n["width_cm"]) for n in quickroad["networks"]],
            [
                ("QS_B1_Main", "Road_Main", 800),
                ("QS_B1_CoreNorth", "Road_Core_North", 450),
                ("QS_B1_CoreSouth", "Road_Core_South", 400),
            ],
        )
        self.assertEqual(
            quickroad["combined_tag_expression"],
            "QS_B1_Main|QS_B1_CoreNorth|QS_B1_CoreSouth",
        )
        self.assertEqual(quickroad["ribbon"], {
            "mesh_sample_distance_cm": 300,
            "width_subdivisions": 3,
            "curvature_threshold_degrees": 8,
            "max_curvature_subdivisions": 2,
        })
        self.assertEqual(quickroad["landscape_influence"], {
            "falloff_cm": 250,
            "blend": 0.9,
            "vertical_offset_cm": -5,
            "smooth_iterations": 4,
            "smooth_strength": 0.6,
        })
        self.assertEqual(quickroad["intersection"], {
            "sample_distance_cm": 500,
            "border_offset_cm": 100,
            "corner_radius_cm": 50,
            "branch_width_scale": 1.2,
        })
        self.assertEqual(quickroad["bake"], {"split_length_cm": 5000})

    def test_population_caps_and_counts_are_exact(self):
        self.assertEqual(self.data["caps"], {
            "buildings": 26, "props": 72, "vegetation": 100,
            "animated_vegetation": 30, "mountains": 24, "crossings": 2,
        })
        self.assertFalse(self.data["runtime_generation"])
        self.assertEqual(len(self.data["prop_records"]), 72)
        self.assertEqual(len(self.data["vegetation_records"]), 100)
        self.assertEqual(len(self.data["mountain_records"]), 24)
        self.assertEqual(len(self.data["cameras"]), 4)

    def test_collision_policy_is_explicit_and_applied(self):
        policy = self.data["collision_policy"]
        for asset_type in ("sign", "plant_card", "lantern", "banner", "mountain"):
            self.assertIs(policy[asset_type], False)
        for record in self.data["prop_records"]:
            self.assertIs(record["collision_enabled"], policy[record["prop_type"]])
        self.assertTrue(all(not item["collision_enabled"] for item in self.data["vegetation_records"]))
        self.assertTrue(all(not item["collision_enabled"] for item in self.data["mountain_records"]))

    def test_static_vegetation_has_deterministic_frame_variants(self):
        animated = [v for v in self.data["vegetation_records"] if v["render_mode"] == "animated_flipbook"]
        static = [v for v in self.data["vegetation_records"] if v["render_mode"] == "static_card"]
        self.assertEqual(len(animated), 30)
        self.assertEqual(len(static), 70)
        self.assertTrue(all("frame_variant" not in item for item in animated))
        expected = [
            _stable_frame_variant(self.data["seed"], item["id"])
            for item in static
        ]
        self.assertEqual([item["frame_variant"] for item in static], expected)
        self.assertEqual({item["frame_variant"] for item in static}, {0, 1, 2, 3})
        self.assertTrue(all(item["collision_enabled"] is False for item in static))

    def test_all_managed_record_ids_are_unique(self):
        ids = [item["id"] for item in self.data["building_plots"]]
        ids += [item["id"] for item in self.data["prop_records"]]
        ids += [item["id"] for item in self.data["vegetation_records"]]
        ids += [item["id"] for item in self.data["mountain_records"]]
        ids += [item["id"] for item in self.data["cameras"]]
        self.assertEqual(len(ids), len(set(ids)))

    def test_contract_hash_is_deterministic_and_not_the_live_layout_hash(self):
        first = MODULE.contract_sha256(self.data)
        second = MODULE.contract_sha256(copy.deepcopy(self.data))
        self.assertEqual(first, second)
        self.assertRegex(first, r"^[0-9a-f]{64}$")
        self.assertNotEqual(first, self.data["expected_base_live_layout_sha256"])

        live_only = copy.deepcopy(self.data)
        live_only["expected_base_live_layout_sha256"] = "f" * 64
        self.assertEqual(MODULE.contract_sha256(live_only), first)

        contract_change = copy.deepcopy(self.data)
        contract_change["prop_records"][0]["yaw_degrees"] += 1
        self.assertNotEqual(MODULE.contract_sha256(contract_change), first)

    def test_contract_hash_covers_all_authoring_asset_inputs(self):
        baseline = MODULE.contract_sha256(self.data)
        mutations = (
            lambda d: d.__setitem__("source_map", "/Game/GameXXK/Maps/Dev/Alternate"),
            lambda d: d.__setitem__("dress_map", "/Game/GameXXK/Maps/Dev/AlternateB1"),
            lambda d: d.__setitem__("asset_root", f"{ASSET_ROOT}/Alternate"),
            lambda d: d["asset_catalog"].__setitem__("mountain_mesh", f"{ASSET_ROOT}/Meshes/Alternate"),
            lambda d: d["building_materials"]["roof_palette_materials"].__setitem__(
                "orange", f"{ASSET_ROOT}/Materials/Alternate"
            ),
            lambda d: d["collision_policy"].__setitem__("rock", False),
            lambda d: d["protected_files"].__setitem__(
                next(iter(d["protected_files"])), "0" * 64
            ),
        )
        for mutate in mutations:
            data = copy.deepcopy(self.data)
            mutate(data)
            self.assertNotEqual(MODULE.contract_sha256(data), baseline)

    def test_rejects_non_finite_values(self):
        mutations = (
            lambda d: d["landscape"]["center_local_cm"].__setitem__(0, float("nan")),
            lambda d: d["quickroad"]["networks"][0].__setitem__("width_cm", float("inf")),
            lambda d: d["building_plots"][0]["location_cm"].__setitem__(1, float("-inf")),
            lambda d: d["prop_records"][0].__setitem__("scale", True),
        )
        for mutate in mutations:
            data = copy.deepcopy(self.data)
            mutate(data)
            self.assert_invalid(data, "finite|bool")

    def test_rejects_duplicate_ids_unknown_types_and_invalid_styles(self):
        cases = (
            (lambda d: d["prop_records"][0].__setitem__("id", d["building_plots"][0]["id"]), "duplicate stable id"),
            (lambda d: d["prop_records"][0].__setitem__("prop_type", "unknown_prop"), "prop_type is invalid"),
            (lambda d: d["vegetation_records"][0].__setitem__("render_mode", "billboard_cloud"), "render_mode is invalid"),
            (lambda d: d["building_plots"][0].__setitem__("archetype_id", "unknown_house"), "archetype_id is invalid"),
            (lambda d: d["building_plots"][0].__setitem__("roof_palette", "purple"), "roof_palette is invalid"),
        )
        for mutate, pattern in cases:
            data = copy.deepcopy(self.data)
            mutate(data)
            self.assert_invalid(data, pattern)

    def test_rejects_invalid_caps_landscape_and_protected_contract(self):
        cases = (
            (lambda d: d["caps"].__setitem__("props", 71), "caps must equal"),
            (lambda d: d["landscape"].__setitem__("resolution", [504, 505]), "resolution"),
            (lambda d: d["landscape"]["origin_local_cm"].__setitem__(0, -40699), "origin_local_cm"),
            (lambda d: d["protected_files"].__setitem__(next(iter(PROTECTED_FILES)), "0" * 64), "protected_files must equal"),
        )
        for mutate, pattern in cases:
            data = copy.deepcopy(self.data)
            mutate(data)
            self.assert_invalid(data, pattern)

    def test_rejects_asset_path_escape_and_malformed_material_catalogs(self):
        mutations = (
            (lambda d: d.__setitem__("asset_root", f"{ASSET_ROOT}/Wrong"), "asset_root"),
            (lambda d: d["asset_catalog"]["building_meshes"].__setitem__(
                "gable_shop", f"{ASSET_ROOT}Evil/Meshes/SM_Escape"
            ), "inside asset_root"),
            (lambda d: d["asset_catalog"].__setitem__("prop_meshes", []), "asset_catalog.prop_meshes must be an object"),
            (lambda d: d["landscape"].__setitem__(
                "ground_material", "/Game/Elsewhere/MI_Ground"
            ), "ground_material"),
            (lambda d: d["landscape"].__setitem__(
                "heightmap_source", "Content/ArtSource/Qingshan/B1/../Escape.png"
            ), "heightmap_source"),
        )
        for mutate, pattern in mutations:
            data = copy.deepcopy(self.data)
            mutate(data)
            self.assert_invalid(data, pattern)

        overlay = copy.deepcopy(self.overlay)
        overlay["building_materials"]["roof_palette_materials"] = []
        with self.assertRaisesRegex(ValueError, "roof_palette_materials must be an object"):
            MODULE.merge_config(overlay, self.base)

    def test_rejects_malformed_or_invalid_b0r_before_merge(self):
        malformed = copy.deepcopy(self.base)
        malformed["building_plots"][0]["size_cm"][0] = 0
        with self.assertRaisesRegex(ValueError, "size_cm"):
            MODULE.merge_config(self.overlay, malformed)

        malformed = copy.deepcopy(self.base)
        del malformed["road_splines"]
        with self.assertRaisesRegex(ValueError, "road_splines"):
            MODULE.merge_config(self.overlay, malformed)

    def test_merge_rejects_malformed_quickroad_containers_as_value_error(self):
        for value in (None, [], "invalid"):
            with self.subTest(quickroad=value):
                overlay = copy.deepcopy(self.overlay)
                overlay["quickroad"] = value
                with self.assertRaisesRegex(ValueError, "quickroad must be an object"):
                    MODULE.merge_config(overlay, self.base)

        for value in (None, {}, "invalid"):
            with self.subTest(networks=value):
                overlay = copy.deepcopy(self.overlay)
                overlay["quickroad"]["networks"] = value
                with self.assertRaisesRegex(ValueError, "quickroad.networks must be a list"):
                    MODULE.merge_config(overlay, self.base)

        for value in (None, [], "invalid"):
            with self.subTest(network_item=value):
                overlay = copy.deepcopy(self.overlay)
                overlay["quickroad"]["networks"][0] = value
                with self.assertRaisesRegex(ValueError, r"quickroad.networks\[0\] must be an object"):
                    MODULE.merge_config(overlay, self.base)

    def test_merge_rejects_malformed_overlay_items_as_value_error(self):
        item_cases = (
            ("base_plot_assignments", r"base_plot_assignments\[0\] must be an object"),
            ("additional_building_plots", r"additional_building_plots\[0\] must be an object"),
            ("prop_records", r"prop_records\[0\] must be an object"),
            ("vegetation_records", r"vegetation_records\[0\] must be an object"),
            ("mountain_records", r"mountain_records\[0\] must be an object"),
            ("cameras", r"cameras\[0\] must be an object"),
        )
        for field, pattern in item_cases:
            with self.subTest(item=field):
                overlay = copy.deepcopy(self.overlay)
                overlay[field][0] = None
                with self.assertRaisesRegex(ValueError, pattern):
                    MODULE.merge_config(overlay, self.base)

        container_cases = (
            "base_plot_assignments", "additional_building_plots", "prop_records",
            "vegetation_records", "mountain_records", "cameras",
        )
        for field in container_cases:
            with self.subTest(container=field):
                overlay = copy.deepcopy(self.overlay)
                overlay[field] = {}
                with self.assertRaisesRegex(ValueError, f"{field} must be a list"):
                    MODULE.merge_config(overlay, self.base)

    def test_plot_schema_and_positive_scales_are_complete(self):
        cases = (
            (lambda d: d["building_plots"][0]["size_cm"].__setitem__(0, 0), "size_cm must be strictly positive"),
            (lambda d: d["building_plots"][0].__setitem__("entrance_axis", "-Y"), "entrance_axis"),
            (lambda d: d["building_plots"][0].__setitem__("cluster_id", "unknown"), "cluster_id"),
            (lambda d: d["mountain_records"][0]["scale"].__setitem__(2, -1), "scale must be strictly positive"),
            (lambda d: d["vegetation_records"][0].__setitem__("scale", 0), "scale must be positive"),
        )
        for mutate, pattern in cases:
            data = copy.deepcopy(self.data)
            mutate(data)
            self.assert_invalid(data, pattern)

    def test_all_spatial_records_and_camera_endpoints_must_stay_in_bounds(self):
        cases = (
            (lambda d: d["building_plots"][0]["location_cm"].__setitem__(0, 8000), "outside world/landscape bounds"),
            (lambda d: d["prop_records"][0]["location_cm"].__setitem__(0, -39000), "outside world/landscape bounds"),
            (lambda d: d["vegetation_records"][0]["location_cm"].__setitem__(1, 20000), "outside world/landscape bounds"),
            (lambda d: d["mountain_records"][0]["location_cm"].__setitem__(1, -20000), "outside world/landscape bounds"),
            (lambda d: d["cameras"][0]["location_cm"].__setitem__(0, 8000), "outside world/landscape bounds"),
            (lambda d: d["cameras"][0]["target_cm"].__setitem__(1, 20000), "outside world/landscape bounds"),
        )
        for mutate, pattern in cases:
            data = copy.deepcopy(self.data)
            mutate(data)
            self.assert_invalid(data, pattern)

    def test_obb_distance_handles_rotation_without_circle_false_positive(self):
        first = {
            "location_cm": [0, 0, 0], "size_cm": [1000, 200, 100], "yaw_degrees": 45,
        }
        second = {
            "location_cm": [-636.396103, 636.396103, 0],
            "size_cm": [1000, 200, 100], "yaw_degrees": 45,
        }
        circle_radius = math.hypot(500, 100)
        self.assertLess(math.dist(first["location_cm"][:2], second["location_cm"][:2]), circle_radius * 2)
        self.assertAlmostEqual(MODULE.oriented_plot_distance(first, second), 700.0, places=3)

    def test_rejects_building_obb_conflicts_with_infrastructure_and_buildings(self):
        cases = (
            ([-3500, -500, 140], "road corridor"),
            ([-26000, -12000, -250], "river corridor"),
            ([-15500, -6500, 100], "bridge anchor"),
            (self.data["building_plots"][0]["location_cm"], "building OBB"),
        )
        for location, pattern in cases:
            data = copy.deepcopy(self.data)
            target = next(p for p in data["building_plots"] if p["id"] == "BLD_S_11")
            target["location_cm"] = list(location)
            self.assert_invalid(data, pattern)

    def test_obb_validation_accepts_separated_boxes_whose_circles_overlap(self):
        data = copy.deepcopy(self.data)
        first = next(p for p in data["building_plots"] if p["id"] == "BLD_S_16")
        second = next(p for p in data["building_plots"] if p["id"] == "BLD_S_17")
        first["location_cm"], first["yaw_degrees"] = [-35000, 3000, 0], 0
        second["location_cm"], second["yaw_degrees"] = [-34050, 3000, 0], 0
        first_radius = math.hypot(first["size_cm"][0] / 2, first["size_cm"][1] / 2)
        second_radius = math.hypot(second["size_cm"][0] / 2, second["size_cm"][1] / 2)
        self.assertLess(950, first_radius + second_radius + 100)
        MODULE.validate_config(data, base_config=self.base)

    def test_rejects_ground_population_conflicts(self):
        categories = (
            ("prop_records", lambda items: next(i for i in items if i["prop_type"] == "crate")),
            ("vegetation_records", lambda items: items[0]),
            ("mountain_records", lambda items: items[0]),
        )
        locations = (
            ([-3500, -500, 140], "road corridor"),
            ([-26000, -12000, -250], "river corridor"),
            ([-15500, -6500, 100], "bridge anchor"),
            (self.data["building_plots"][0]["location_cm"], "building OBB"),
        )
        for field, select in categories:
            for location, pattern in locations:
                with self.subTest(field=field, pattern=pattern):
                    data = copy.deepcopy(self.data)
                    select(data[field])["location_cm"] = list(location)
                    self.assert_invalid(data, pattern)

    def test_every_ground_population_category_respects_all_fixed_anchors(self):
        categories = (
            (
                "prop_records",
                lambda items: next(i for i in items if i["prop_type"] == "crate"),
                0.0,
            ),
            ("vegetation_records", lambda items: items[0], 100.0),
            ("mountain_records", lambda items: items[0], 200.0),
        )
        for anchor in self.data["fixed_anchors"]:
            for field, select, clearance in categories:
                with self.subTest(anchor=anchor["id"], field=field):
                    data = copy.deepcopy(self.data)
                    offset = (
                        anchor["protected_radius_cm"] - 1.0
                        if clearance == 0.0
                        else anchor["protected_radius_cm"] + clearance / 2.0
                    )
                    select(data[field])["location_cm"] = [
                        anchor["location_cm"][0] + offset,
                        anchor["location_cm"][1],
                        anchor["location_cm"][2],
                    ]
                    self.assert_invalid(data, f"fixed anchor {anchor['id']}")

    def test_attachment_and_dock_exceptions_do_not_bypass_fixed_anchors(self):
        anchor_by_id = {item["id"]: item for item in self.data["fixed_anchors"]}
        cases = (
            (lambda items: next(i for i in items if i["prop_type"] == "sign"), "NorthGateFAnchor"),
            (lambda items: next(i for i in items if i["prop_type"] == "dock_post"), "SouthDockAnchor"),
        )
        for select, anchor_id in cases:
            data = copy.deepcopy(self.data)
            select(data["prop_records"])["location_cm"] = list(anchor_by_id[anchor_id]["location_cm"])
            self.assert_invalid(data, f"fixed anchor {anchor_id}")

    def test_attachment_props_require_valid_target_and_edge_band(self):
        attachment_types = {"sign", "lantern", "banner"}
        building_ids = {plot["id"] for plot in self.data["building_plots"]}
        attachments = [p for p in self.data["prop_records"] if p["prop_type"] in attachment_types]
        self.assertTrue(attachments)
        self.assertTrue(all(p["attachment_target_id"] in building_ids for p in attachments))

        cases = (
            (lambda p, d: p.pop("attachment_target_id"), "attachment_target_id"),
            (lambda p, d: p.__setitem__("attachment_target_id", "BLD_UNKNOWN"), "attachment_target_id"),
            (lambda p, d: p.__setitem__(
                "location_cm",
                list(next(b for b in d["building_plots"] if b["id"] == p["attachment_target_id"])["location_cm"]),
            ), "attachment band"),
        )
        for mutate, pattern in cases:
            data = copy.deepcopy(self.data)
            prop = next(p for p in data["prop_records"] if p["prop_type"] == "sign")
            mutate(prop, data)
            self.assert_invalid(data, pattern)

        data = copy.deepcopy(self.data)
        prop = next(p for p in data["prop_records"] if p["prop_type"] == "crate")
        prop["attachment_target_id"] = data["building_plots"][0]["id"]
        self.assert_invalid(data, "only sign/lantern/banner")

    def test_dock_post_is_the_only_explicit_river_overlap_exception(self):
        river_point = [-26000, -12000, -250]
        data = copy.deepcopy(self.data)
        crate = next(p for p in data["prop_records"] if p["prop_type"] == "crate")
        crate["allow_river_overlap"] = True
        self.assert_invalid(data, "only dock_post")

        data = copy.deepcopy(self.data)
        dock_post = next(p for p in data["prop_records"] if p["prop_type"] == "dock_post")
        dock_post["location_cm"] = river_point
        dock_post.pop("allow_river_overlap", None)
        self.assert_invalid(data, "river corridor")

        data = copy.deepcopy(self.data)
        dock_post = next(p for p in data["prop_records"] if p["prop_type"] == "dock_post")
        dock_post["location_cm"] = river_point
        dock_post["allow_river_overlap"] = True
        MODULE.validate_config(data, base_config=self.base)

    def test_rejects_tampered_static_frame_variant(self):
        data = copy.deepcopy(self.data)
        static = next(v for v in data["vegetation_records"] if v["render_mode"] == "static_card")
        static["frame_variant"] = (static["frame_variant"] + 1) % 4
        self.assert_invalid(data, "SHA-256")

    def test_rejects_b0r_geometry_drift_and_unknown_assignment_ids(self):
        data = copy.deepcopy(self.data)
        data["building_plots"][0]["location_cm"][0] += 1
        self.assert_invalid(data, "B0R geometry")

        overlay = copy.deepcopy(self.overlay)
        overlay["base_plot_assignments"][0]["id"] = "BLD_UNKNOWN"
        with self.assertRaisesRegex(ValueError, "base plot assignment IDs"):
            MODULE.merge_config(overlay, self.base)

    def test_merge_rejects_invalid_raw_roof_palette_as_validation_error(self):
        overlay = copy.deepcopy(self.overlay)
        overlay["base_plot_assignments"][0]["roof_palette"] = "purple"
        with self.assertRaisesRegex(ValueError, "roof_palette is invalid"):
            MODULE.merge_config(overlay, self.base)

    def test_rejects_nonpositive_landscape_scale_even_with_matching_origin(self):
        data = copy.deepcopy(self.data)
        data["landscape"]["scale_cm"][0] = 0.0
        data["landscape"]["origin_local_cm"][0] = data["landscape"]["center_local_cm"][0]
        self.assert_invalid(data, "scale_cm must be strictly positive")

    def test_rejects_nonstandard_json_numeric_constants(self):
        canonical = OVERLAY_PATH.read_text(encoding="utf-8")
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "invalid.json"
            path.write_text(canonical.replace("20260711", "NaN", 1), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "non-standard JSON numeric constant NaN"):
                MODULE.load_config(
                    path,
                    base_config=self.base,
                    verify_protected_files=False,
                )


if __name__ == "__main__":
    unittest.main()
