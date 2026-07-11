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

    def test_canonical_contract(self):
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
        self.assertEqual(len(data["building_plots"]), 16)
        self.assertEqual(sum(p["size_class"] == "large" for p in data["building_plots"]), 1)
        self.assertEqual(sum(p["size_class"] == "medium" for p in data["building_plots"]), 5)
        self.assertEqual(sum(p["size_class"] == "small" for p in data["building_plots"]), 10)
        self.assertEqual(set(data["cameras"]), {"CAM_QS_GATE_ARRIVAL", "CAM_QS_TOWN_CORE", "CAM_QS_MAIN_BRIDGE", "CAM_QS_SOUTH_DOCK"})
        self.assertFalse(data["runtime_generation"])

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
