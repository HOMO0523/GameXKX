"""Host-side tests for the deterministic Qingshan town PCG contract."""

from __future__ import annotations

import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[1]
MODULE_PATH = PROJECT_ROOT / "Content" / "Python" / "gamexxk_qingshan_town_pcg_config.py"


def _load_module():
    spec = importlib.util.spec_from_file_location("gamexxk_qingshan_town_pcg_config", MODULE_PATH)
    if spec is None or spec.loader is None:
        raise ImportError(f"Unable to load configuration module: {MODULE_PATH}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class QingshanTownPCGConfigTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.module = _load_module()

    def setUp(self):
        self.data = self.module.load_config()

    def assert_invalid(self, data, message_pattern):
        with self.assertRaisesRegex(ValueError, message_pattern):
            self.module.validate_config(data)

    def test_config_path_is_project_relative_and_loads_without_unreal(self):
        expected = PROJECT_ROOT / "Config" / "GameXXK" / "TownPCG" / "QingshanTownVerticalSlice.json"
        self.assertEqual(self.module.CONFIG_PATH, expected)
        self.assertEqual(json.loads(expected.read_text(encoding="utf-8")), self.data)

    def test_module_declares_its_public_api(self):
        self.assertEqual(
            self.module.__all__,
            ("CONFIG_PATH", "load_config", "validate_config"),
        )

    def test_load_config_accepts_a_custom_path(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            custom_path = Path(temp_dir) / "town.json"
            custom_path.write_text(json.dumps(self.data), encoding="utf-8")
            self.assertEqual(self.module.load_config(custom_path), self.data)

    def test_canonical_contract_values(self):
        self.assertEqual(self.data["schema_version"], 1)
        self.assertEqual(self.data["seed"], 20260710)
        self.assertEqual(self.data["source_map"], "/Game/GameXXK/Maps/L_QingshanInn")
        self.assertEqual(
            self.data["prototype_map"],
            "/Game/GameXXK/Maps/Prototype/L_QingshanTown_PCG_Prototype",
        )
        self.assertEqual(
            self.data["asset_root"],
            "/Game/GameXXK/Environment/TownPCG/VerticalSlice",
        )
        self.assertEqual(
            self.data["graph_path"],
            "/Game/GameXXK/Environment/TownPCG/VerticalSlice/PCG_QingshanTown_VerticalSlice",
        )
        self.assertEqual(self.data["north_gate_label"], "QingshanInn_TownExit")
        self.assertEqual(self.data["building_limit"], 12)
        self.assertEqual(self.data["building_hard_cap"], 16)
        self.assertEqual(
            self.data["building_mesh"],
            "/Game/GameXXK/Environment/TownPCG/Prototype/QingshanShopA/SM_Qingshan_Shop_A_HQ_Retop50K",
        )
        self.assertEqual(self.data["main_road_width_cm"], 800)
        self.assertEqual(self.data["river_width_cm"], 2000)
        self.assertEqual(self.data["bridge_width_cm"], 900)
        self.assertEqual(self.data["road_setback_cm"], 250)
        self.assertEqual(self.data["tree_instance_limit"], 48)
        self.assertEqual(self.data["prop_instance_limit"], 24)
        self.assertFalse(self.data["runtime_generation"])

    def test_building_points_are_the_canonical_twelve_offsets(self):
        points = self.data["building_points"]
        self.assertEqual(len(points), 12)
        expected_y = [-1600, -3200, -4800, -6400, -8000, -9600]
        self.assertEqual(
            points,
            [
                {"location_cm": [-900, y, 0], "yaw_degrees": 0, "side": "left"}
                for y in expected_y
            ]
            + [
                {"location_cm": [900, y, 0], "yaw_degrees": 180, "side": "right"}
                for y in expected_y
            ],
        )
        self.assertTrue(all(p["yaw_degrees"] == 0 for p in points if p["side"] == "left"))
        self.assertTrue(all(p["yaw_degrees"] == 180 for p in points if p["side"] == "right"))

    def test_validate_rejects_wrong_schema_version(self):
        data = copy.deepcopy(self.data)
        data["schema_version"] = 2
        self.assert_invalid(data, "schema_version must be 1")

    def test_validate_rejects_duplicate_building_points(self):
        data = copy.deepcopy(self.data)
        data["building_points"][1] = copy.deepcopy(data["building_points"][0])
        self.assert_invalid(data, "duplicate building point")

    def test_validate_rejects_building_limit_outside_hard_cap(self):
        for value in (0, 17):
            with self.subTest(building_limit=value):
                data = copy.deepcopy(self.data)
                data["building_limit"] = value
                self.assert_invalid(data, "building_limit must be between 1 and building_hard_cap")

    def test_validate_rejects_more_listed_points_than_hard_cap(self):
        data = copy.deepcopy(self.data)
        data["building_hard_cap"] = 11
        data["building_limit"] = 11
        self.assert_invalid(data, "building_points count exceeds building_hard_cap")

    def test_validate_requires_exact_authored_building_count(self):
        cases = (
            [],
            self.data["building_points"][:-1],
            self.data["building_points"]
            + [{"location_cm": [900, -11200, 0], "yaw_degrees": 180, "side": "right"}],
        )
        for points in cases:
            with self.subTest(point_count=len(points)):
                data = copy.deepcopy(self.data)
                data["building_points"] = copy.deepcopy(points)
                self.assert_invalid(
                    data,
                    "building_points count must equal building_limit",
                )

    def test_validate_rejects_missing_or_empty_package_paths(self):
        fields = ("source_map", "prototype_map", "asset_root", "graph_path", "building_mesh")
        for field in fields:
            for value in (None, "", "   "):
                with self.subTest(field=field, value=value):
                    data = copy.deepcopy(self.data)
                    if value is None:
                        del data[field]
                    else:
                        data[field] = value
                    self.assert_invalid(data, f"{field} must be a valid Unreal package path")

    def test_validate_rejects_malformed_unreal_package_paths(self):
        fields = ("source_map", "prototype_map", "asset_root", "graph_path", "building_mesh")
        for field in fields:
            for value in ("/Engine/Foo", "/Game/Foo.Bar", "/Game/Foo Bar", " /Game/Foo"):
                with self.subTest(field=field, value=value):
                    data = copy.deepcopy(self.data)
                    data[field] = value
                    self.assert_invalid(data, f"{field} must be a valid Unreal package path")

    def test_validate_rejects_missing_or_untrimmed_north_gate_label(self):
        for value in (None, "", "   ", " QingshanInn_TownExit", "QingshanInn_TownExit "):
            with self.subTest(value=value):
                data = copy.deepcopy(self.data)
                if value is None:
                    del data["north_gate_label"]
                else:
                    data["north_gate_label"] = value
                self.assert_invalid(data, "north_gate_label must be a nonblank trimmed string")

    def test_validate_rejects_runtime_generation(self):
        data = copy.deepcopy(self.data)
        data["runtime_generation"] = True
        self.assert_invalid(data, "runtime_generation must be false")

    def test_validate_rejects_malformed_location_vectors(self):
        bad_vectors = ([1, 2], [1, 2, 3, 4], [1, "two", 3], "1,2,3")
        for vector in bad_vectors:
            with self.subTest(vector=vector):
                data = copy.deepcopy(self.data)
                data["building_points"][0]["location_cm"] = vector
                self.assert_invalid(data, "location_cm must be a three-number vector")

    def test_validate_rejects_invalid_side_and_yaw_mismatches(self):
        cases = (
            ("center", 0, "side must be left or right"),
            ("left", 180, "left point yaw_degrees must be 0"),
            ("right", 0, "right point yaw_degrees must be 180"),
        )
        for side, yaw, message in cases:
            with self.subTest(side=side, yaw=yaw):
                data = copy.deepcopy(self.data)
                data["building_points"][0]["side"] = side
                data["building_points"][0]["yaw_degrees"] = yaw
                self.assert_invalid(data, message)

    def test_validate_rejects_non_numeric_yaw(self):
        data = copy.deepcopy(self.data)
        data["building_points"][0]["yaw_degrees"] = "0"
        self.assert_invalid(data, "yaw_degrees must be a number")

    def test_validate_rejects_non_finite_transform_values(self):
        for value in (float("nan"), float("inf"), float("-inf")):
            with self.subTest(field="location_cm", value=value):
                data = copy.deepcopy(self.data)
                data["building_points"][0]["location_cm"][0] = value
                self.assert_invalid(data, "location_cm must contain only finite numbers")
            with self.subTest(field="yaw_degrees", value=value):
                data = copy.deepcopy(self.data)
                data["building_points"][0]["yaw_degrees"] = value
                self.assert_invalid(data, "yaw_degrees must be finite")

    def test_load_config_rejects_non_standard_json_numeric_constants(self):
        canonical_text = self.module.CONFIG_PATH.read_text(encoding="utf-8")
        for constant in ("NaN", "Infinity", "-Infinity"):
            with self.subTest(constant=constant), tempfile.TemporaryDirectory() as temp_dir:
                custom_path = Path(temp_dir) / "invalid.json"
                custom_path.write_text(
                    canonical_text.replace("20260710", constant, 1),
                    encoding="utf-8",
                )
                self.assert_invalid_json_constant(custom_path, constant)

    def assert_invalid_json_constant(self, path, constant):
        with self.assertRaisesRegex(
            ValueError,
            f"non-standard JSON numeric constant {constant} is not allowed",
        ):
            self.module.load_config(path)

    def test_validate_rejects_malformed_container_shapes(self):
        self.assert_invalid([], "config must be an object")

        data = copy.deepcopy(self.data)
        data["building_points"] = "not-a-list"
        self.assert_invalid(data, "building_points must be a list")

        data = copy.deepcopy(self.data)
        data["building_points"][0] = "not-an-object"
        self.assert_invalid(data, "building_points\\[0\\] must be an object")

    def test_validate_rejects_bool_as_numeric_budget(self):
        numeric_fields = (
            "seed",
            "building_limit",
            "building_hard_cap",
            "main_road_width_cm",
            "river_width_cm",
            "bridge_width_cm",
            "road_setback_cm",
            "tree_instance_limit",
            "prop_instance_limit",
        )
        for field in numeric_fields:
            with self.subTest(field=field):
                data = copy.deepcopy(self.data)
                data[field] = True
                self.assert_invalid(data, f"{field} must be an integer, not bool")

    def test_validate_rejects_negative_seed(self):
        data = copy.deepcopy(self.data)
        data["seed"] = -1
        self.assert_invalid(data, "seed must be at least 0")

    def test_validate_rejects_negative_instance_limits(self):
        for field in ("tree_instance_limit", "prop_instance_limit"):
            with self.subTest(field=field):
                data = copy.deepcopy(self.data)
                data[field] = -1
                self.assert_invalid(data, f"{field} must be at least 0")

    def test_validate_rejects_non_positive_dimensions(self):
        dimensions = (
            "main_road_width_cm",
            "river_width_cm",
            "bridge_width_cm",
            "road_setback_cm",
        )
        for field in dimensions:
            for value in (0, -1):
                with self.subTest(field=field, value=value):
                    data = copy.deepcopy(self.data)
                    data[field] = value
                    self.assert_invalid(data, f"{field} must be greater than 0")


if __name__ == "__main__":
    unittest.main()
