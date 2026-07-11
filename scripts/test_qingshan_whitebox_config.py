"""Host-side tests for the Qingshan B0R whitebox layout contract."""

from __future__ import annotations

import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[1]
MODULE_PATH = PROJECT_ROOT / "Content" / "Python" / "gamexxk_qingshan_whitebox_config.py"


def _load_module():
    spec = importlib.util.spec_from_file_location("gamexxk_qingshan_whitebox_config", MODULE_PATH)
    if spec is None or spec.loader is None:
        raise ImportError(f"Unable to load configuration module: {MODULE_PATH}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class QingshanWhiteboxConfigTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.module = _load_module()

    def setUp(self):
        self.data = self.module.load_config()

    def assert_invalid(self, data, pattern):
        with self.assertRaisesRegex(ValueError, pattern):
            self.module.validate_config(data)

    def test_public_api_and_config_path(self):
        expected = PROJECT_ROOT / "Config" / "GameXXK" / "TownPCG" / "QingshanWhiteboxB0R.json"
        self.assertEqual(self.module.__all__, ("CONFIG_PATH", "load_config", "validate_config"))
        self.assertEqual(self.module.CONFIG_PATH, expected)
        self.assertEqual(json.loads(expected.read_text(encoding="utf-8")), self.data)

    def test_load_config_accepts_custom_path(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "whitebox.json"
            path.write_text(json.dumps(self.data), encoding="utf-8")
            self.assertEqual(self.module.load_config(path), self.data)

    def test_canonical_contract_values(self):
        data = self.data
        self.assertEqual(data["schema_version"], 1)
        self.assertEqual(data["revision_id"], "B0R")
        self.assertEqual(data["seed"], 20260711)
        self.assertEqual(data["source_map"], "/Game/GameXXK/Maps/L_QingshanInn")
        self.assertEqual(data["whitebox_map"], "/Game/GameXXK/Maps/Dev/L_Qingshan_PCG_Whitebox_B0R")
        self.assertEqual(data["asset_root"], "/Game/GameXXK/Environment/TownPCG/B0R")
        self.assertEqual(data["coordinate_reference"]["anchor_id"], "NorthGateFAnchor")
        self.assertEqual(data["coordinate_reference"]["actor_label"], "QingshanInn_TownExit")
        self.assertEqual(data["playable_bounds_cm"], [-30000, 0, -12000, 12000])
        self.assertEqual(data["world_bounds_cm"], [-38000, 7000, -19000, 19000])
        self.assertFalse(data["runtime_generation"])
        self.assertEqual(data["spawn_caps"], {"buildings": 19, "foliage": 100, "mountains": 30, "crossings": 2})
        self.assertEqual(data["fixed_anchors"], [
            {"id": "NorthGateFAnchor", "location_cm": [0, 0, 120], "yaw_degrees": 0, "protected_radius_cm": 800},
            {"id": "TownCoreAnchor", "location_cm": [-12000, -2200, 300], "yaw_degrees": 25, "protected_radius_cm": 1200},
            {"id": "MainBridgeAnchor", "location_cm": [-15500, -6500, 100], "yaw_degrees": -35, "protected_radius_cm": 1200},
            {"id": "SouthDockAnchor", "location_cm": [-21500, -10000, -200], "yaw_degrees": -20, "protected_radius_cm": 1000},
        ])
        self.assertEqual(data["road_splines"], [
            {"id": "Road_Main", "width_cm": 800, "points_cm": [[0, 0, 120], [-3500, -500, 140], [-7500, -2500, 180], [-11500, -3500, 250], [-15500, -6500, 100], [-20500, -9000, -100]]},
            {"id": "Road_Core_North", "width_cm": 450, "points_cm": [[-7500, -2500, 180], [-9000, 1200, 250], [-13000, 2600, 400]]},
            {"id": "Road_Core_South", "width_cm": 400, "points_cm": [[-11500, -3500, 250], [-14500, -1500, 300], [-18500, -3000, 250]]},
        ])
        self.assertEqual(data["river_splines"], [
            {"id": "River_Main", "width_cm": 1800, "points_cm": [[-26000, -12000, -250], [-21000, -9000, -200], [-15500, -6500, -150], [-9000, -8000, -100], [-3500, -11000, -100]]},
        ])
        self.assertEqual(data["terrain_zones"], [
            {"id": "Terrain_Base", "center_cm": [-15000, 0, -500], "size_cm": [30000, 24000, 400], "yaw_degrees": 0},
            {"id": "Terrain_CorePlateau", "center_cm": [-12000, -1800, -150], "size_cm": [12500, 8500, 500], "yaw_degrees": 10},
            {"id": "Terrain_SouthSlope", "center_cm": [-17800, -6800, -350], "size_cm": [10500, 6500, 350], "yaw_degrees": -25},
            {"id": "Terrain_DockLowland", "center_cm": [-21800, -9800, -550], "size_cm": [8500, 4800, 250], "yaw_degrees": -20},
        ])
        expected_plots = [
            ("BLD_L_01", "large", [-12000, -1800, 300], 25, [1800, 1400, 1200], "core"),
            ("BLD_M_01", "medium", [-8200, -4700, 220], -15, [1200, 900, 800], "core"),
            ("BLD_M_02", "medium", [-10000, 800, 380], 18, [1100, 850, 750], "core"),
            ("BLD_M_03", "medium", [-14800, -1900, 360], 42, [1400, 1000, 900], "bridge"),
            ("BLD_M_04", "medium", [-17400, -10800, 20], -28, [1000, 800, 700], "south"),
            ("BLD_M_05", "medium", [-19200, -4800, 180], 12, [1300, 900, 850], "south"),
            ("BLD_S_01", "small", [-5200, -2500, 180], -22, [800, 600, 500], "approach"),
            ("BLD_S_02", "small", [-6500, 1700, 300], 31, [700, 550, 450], "approach"),
            ("BLD_S_03", "small", [-7200, -6200, 80], 11, [900, 700, 600], "core"),
            ("BLD_S_04", "small", [-12200, 3300, 460], -35, [750, 600, 500], "core"),
            ("BLD_S_05", "small", [-14700, -3500, 180], 55, [850, 650, 550], "core"),
            ("BLD_S_06", "small", [-16200, 900, 420], -12, [650, 550, 450], "bridge"),
            ("BLD_S_07", "small", [-18500, -1500, 300], 27, [900, 650, 550], "bridge"),
            ("BLD_S_08", "small", [-21400, -5200, -20], -46, [750, 550, 500], "south"),
            ("BLD_S_09", "small", [-24800, -7600, -80], 16, [850, 700, 550], "south"),
            ("BLD_S_10", "small", [-23800, -5800, 100], 39, [700, 600, 480], "south"),
        ]
        self.assertEqual(data["building_plots"], [
            {"id": plot_id, "size_class": size_class, "location_cm": location, "yaw_degrees": yaw,
             "size_cm": size, "entrance_axis": "+Y", "cluster_id": cluster}
            for plot_id, size_class, location, yaw, size, cluster in expected_plots
        ])
        self.assertEqual(data["proxy_generation"], {
            "foliage": {"count": 100, "scale_range": [2.5, 6.5], "exclusion_margin_cm": 250},
            "mountains": {"count": 24, "scale_xy_range": [25, 65], "scale_z_range": [35, 95], "perimeter_band_cm": [1500, 7000]},
        })
        expected_exclusions = [
            ("EXC_ANCHOR_NORTH_GATE", "anchor_circle", "NorthGateFAnchor", 0),
            ("EXC_ANCHOR_MAIN_BRIDGE", "anchor_circle", "MainBridgeAnchor", 0),
            ("EXC_ANCHOR_SOUTH_DOCK", "anchor_circle", "SouthDockAnchor", 0),
            ("EXC_ROAD_MAIN", "road_corridor", "Road_Main", 250),
            ("EXC_ROAD_CORE_NORTH", "road_corridor", "Road_Core_North", 250),
            ("EXC_ROAD_CORE_SOUTH", "road_corridor", "Road_Core_South", 250),
            ("EXC_RIVER_MAIN", "river_corridor", "River_Main", 400),
        ] + [
            (f"EXC_{plot_id}", "building_footprint", plot_id, 200)
            for plot_id, *_ in expected_plots
        ]
        self.assertEqual(data["exclusion_zones"], [
            {"id": zone_id, "kind": kind, "source_id": source_id, "margin_cm": margin}
            for zone_id, kind, source_id, margin in expected_exclusions
        ])
        self.assertEqual(data["cameras"], {
            "CAM_QS_GATE_ARRIVAL": {"location_cm": [-2500, 2500, 3200], "target_cm": [-6500, -1200, 300], "fov_degrees": 50},
            "CAM_QS_TOWN_CORE": {"location_cm": [-5000, 3500, 4500], "target_cm": [-12500, -3000, 300], "fov_degrees": 50},
            "CAM_QS_MAIN_BRIDGE": {"location_cm": [-9000, -1000, 4000], "target_cm": [-15500, -6500, 0], "fov_degrees": 48},
            "CAM_QS_SOUTH_DOCK": {"location_cm": [-15000, -4000, 4200], "target_cm": [-21500, -9500, -200], "fov_degrees": 50},
        })

    def test_rejects_duplicate_stable_ids(self):
        data = copy.deepcopy(self.data)
        data["building_plots"][1]["id"] = data["building_plots"][0]["id"]
        self.assert_invalid(data, "duplicate stable id")

    def test_rejects_non_finite_vectors(self):
        for value in (float("nan"), float("inf"), float("-inf")):
            with self.subTest(value=value):
                data = copy.deepcopy(self.data)
                data["road_splines"][0]["points_cm"][0][0] = value
                self.assert_invalid(data, "must contain only finite numbers")

    def test_rejects_oversized_integer_scalars_and_vectors_as_non_finite(self):
        huge = 10**10000
        data = copy.deepcopy(self.data)
        data["cameras"]["CAM_QS_GATE_ARRIVAL"]["fov_degrees"] = huge
        self.assert_invalid(data, r"CAM_QS_GATE_ARRIVAL\.fov_degrees must be finite")

        data = copy.deepcopy(self.data)
        data["road_splines"][0]["points_cm"][0][0] = huge
        self.assert_invalid(data, r"road_splines\[0\]\.points_cm\[0\] must contain only finite numbers")

    def test_requires_north_gate_anchor(self):
        data = copy.deepcopy(self.data)
        data["fixed_anchors"] = [a for a in data["fixed_anchors"] if a["id"] != "NorthGateFAnchor"]
        self.assert_invalid(data, "NorthGateFAnchor")

    def test_building_count_must_be_13_through_19(self):
        for count in (12, 20):
            with self.subTest(count=count):
                data = copy.deepcopy(self.data)
                template = copy.deepcopy(data["building_plots"][-1])
                while len(data["building_plots"]) < count:
                    item = copy.deepcopy(template)
                    item["id"] = f"EXTRA_{len(data['building_plots'])}"
                    data["building_plots"].append(item)
                data["building_plots"] = data["building_plots"][:count]
                self.assert_invalid(data, "building_plots count must be between 13 and 19")

    def test_camera_fov_must_be_35_through_65(self):
        for fov in (34.9, 65.1):
            data = copy.deepcopy(self.data)
            data["cameras"]["CAM_QS_GATE_ARRIVAL"]["fov_degrees"] = fov
            self.assert_invalid(data, "fov_degrees must be between 35 and 65")

    def test_camera_requires_location_and_target(self):
        for field in ("location_cm", "target_cm"):
            data = copy.deepcopy(self.data)
            del data["cameras"]["CAM_QS_GATE_ARRIVAL"][field]
            self.assert_invalid(data, field)

    def test_rejects_runtime_generation(self):
        data = copy.deepcopy(self.data)
        data["runtime_generation"] = True
        self.assert_invalid(data, "runtime_generation must be false")

    def test_exclusion_sources_must_resolve_by_kind(self):
        cases = (
            (0, "MissingAnchor", "anchor_circle source_id must resolve to fixed_anchors"),
            (3, "River_Main", "road_corridor source_id must resolve to road_splines"),
            (6, "Road_Main", "river_corridor source_id must resolve to river_splines"),
            (7, "TownCoreAnchor", "building_footprint source_id must resolve to building_plots"),
        )
        for index, source_id, pattern in cases:
            with self.subTest(index=index, source_id=source_id):
                data = copy.deepcopy(self.data)
                data["exclusion_zones"][index]["source_id"] = source_id
                self.assert_invalid(data, pattern)

    def test_proxy_counts_must_not_exceed_spawn_caps(self):
        cases = (("foliage", 101), ("mountains", 31))
        for category, count in cases:
            with self.subTest(category=category):
                data = copy.deepcopy(self.data)
                data["proxy_generation"][category]["count"] = count
                self.assert_invalid(data, "count must not exceed spawn_caps")

    def test_proxy_ranges_and_margin_are_valid(self):
        mutations = (
            (lambda d: d["proxy_generation"]["foliage"].__setitem__("scale_range", [6.5, 2.5]), "scale_range must be ordered"),
            (lambda d: d["proxy_generation"]["foliage"].__setitem__("scale_range", [-1, 2]), "scale_range must be strictly positive"),
            (lambda d: d["proxy_generation"]["mountains"].__setitem__("scale_xy_range", [65, 25]), "scale_xy_range must be ordered"),
            (lambda d: d["proxy_generation"]["mountains"].__setitem__("scale_z_range", [0, 95]), "scale_z_range must be strictly positive"),
            (lambda d: d["proxy_generation"]["mountains"].__setitem__("perimeter_band_cm", [7000, 1500]), "perimeter_band_cm must have outer greater than inner"),
            (lambda d: d["proxy_generation"]["mountains"].__setitem__("perimeter_band_cm", [-1, 7000]), "perimeter_band_cm must be nonnegative"),
            (lambda d: d["proxy_generation"]["foliage"].__setitem__("exclusion_margin_cm", -1), "exclusion_margin_cm must be nonnegative"),
        )
        for mutate, pattern in mutations:
            with self.subTest(pattern=pattern):
                data = copy.deepcopy(self.data)
                mutate(data)
                self.assert_invalid(data, pattern)

    def test_requires_nonempty_road_and_river_splines(self):
        for field in ("road_splines", "river_splines"):
            with self.subTest(field=field):
                data = copy.deepcopy(self.data)
                data[field] = []
                self.assert_invalid(data, f"{field} must contain at least one spline")

    def test_rejects_spatial_data_outside_world_bounds(self):
        cases = (
            ("road", lambda d: d["road_splines"][0]["points_cm"][0].__setitem__(0, 8000)),
            ("river", lambda d: d["river_splines"][0]["points_cm"][0].__setitem__(1, 20000)),
            ("plot", lambda d: d["building_plots"][0]["location_cm"].__setitem__(0, -39000)),
            ("camera", lambda d: d["cameras"]["CAM_QS_GATE_ARRIVAL"]["target_cm"].__setitem__(1, -20000)),
        )
        for name, mutate in cases:
            with self.subTest(name=name):
                data = copy.deepcopy(self.data)
                mutate(data)
                self.assert_invalid(data, "outside world_bounds_cm")

    def test_bridge_center_must_touch_road_and_river_control_point(self):
        data = copy.deepcopy(self.data)
        bridge = next(a for a in data["fixed_anchors"] if a["id"] == "MainBridgeAnchor")
        bridge["location_cm"] = [-15650, -6650, 100]
        self.assert_invalid(data, "MainBridgeAnchor must be within 100cm")

    def test_rejects_malformed_containers(self):
        cases = (
            ([], "config must be an object"),
            ({**self.data, "road_splines": {}}, "road_splines must be a list"),
            ({**self.data, "fixed_anchors": ["bad"]}, r"fixed_anchors\[0\] must be an object"),
            ({**self.data, "cameras": []}, "cameras must be an object"),
        )
        for data, pattern in cases:
            self.assert_invalid(copy.deepcopy(data), pattern)

    def test_rejects_bool_numeric_values(self):
        mutations = (
            lambda d: d.__setitem__("seed", True),
            lambda d: d["road_splines"][0].__setitem__("width_cm", True),
            lambda d: d["cameras"]["CAM_QS_GATE_ARRIVAL"].__setitem__("fov_degrees", True),
            lambda d: d["building_plots"][0]["location_cm"].__setitem__(0, True),
        )
        for mutate in mutations:
            data = copy.deepcopy(self.data)
            mutate(data)
            self.assert_invalid(data, "bool")

    def test_rejects_nonstandard_json_numeric_constants(self):
        canonical = self.module.CONFIG_PATH.read_text(encoding="utf-8")
        for constant in ("NaN", "Infinity", "-Infinity"):
            with self.subTest(constant=constant), tempfile.TemporaryDirectory() as temp_dir:
                path = Path(temp_dir) / "invalid.json"
                path.write_text(canonical.replace("20260711", constant, 1), encoding="utf-8")
                with self.assertRaisesRegex(ValueError, f"non-standard JSON numeric constant {constant} is not allowed"):
                    self.module.load_config(path)

    def test_rejects_bad_package_paths(self):
        for value in (" /Game/Foo", "/Game/Foo.Bar", "/Game/Foo Bar", "/Engine/Foo"):
            data = copy.deepcopy(self.data)
            data["source_map"] = value
            self.assert_invalid(data, "source_map must be a valid Unreal package path")


if __name__ == "__main__":
    unittest.main()
