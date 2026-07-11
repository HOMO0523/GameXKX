"""Tests for deterministic Qingshan B0R proxy layout expansion."""

from __future__ import annotations

import copy
import importlib.util
import json
import math
from pathlib import Path
import random
import unittest

from scripts.qingshan_whitebox_acceptance import (
    canonical_layout_hash,
    generate_seeded_proxy_transforms,
)


PROJECT_ROOT = Path(__file__).resolve().parents[1]
CONFIG_MODULE_PATH = (
    PROJECT_ROOT / "Content" / "Python" / "gamexxk_qingshan_whitebox_config.py"
)


def _load_config_module():
    spec = importlib.util.spec_from_file_location(
        "gamexxk_qingshan_whitebox_config_for_acceptance", CONFIG_MODULE_PATH
    )
    if spec is None or spec.loader is None:
        raise ImportError(f"Unable to load configuration module: {CONFIG_MODULE_PATH}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _load_config():
    return _load_config_module().load_config()


def _distance_to_segment(point, start, end):
    px, py = point
    ax, ay = start[:2]
    bx, by = end[:2]
    dx, dy = bx - ax, by - ay
    if dx == 0 and dy == 0:
        return math.hypot(px - ax, py - ay)
    t = max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy)))
    return math.hypot(px - (ax + t * dx), py - (ay + t * dy))


def _inside_rotated_plot(point, plot, margin):
    dx = point[0] - plot["location_cm"][0]
    dy = point[1] - plot["location_cm"][1]
    angle = math.radians(-plot["yaw_degrees"])
    local_x = dx * math.cos(angle) - dy * math.sin(angle)
    local_y = dx * math.sin(angle) + dy * math.cos(angle)
    return (
        abs(local_x) <= plot["size_cm"][0] / 2.0 + margin
        and abs(local_y) <= plot["size_cm"][1] / 2.0 + margin
    )


class QingshanWhiteboxAcceptanceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.config = _load_config()

    def test_canonical_counts_and_transform_schema(self):
        layout = generate_seeded_proxy_transforms(self.config)
        self.assertEqual(set(layout), {"foliage", "mountains"})
        self.assertEqual(len(layout["foliage"]), 100)
        self.assertEqual(len(layout["mountains"]), 24)
        for category, records in layout.items():
            with self.subTest(category=category):
                self.assertEqual(len({record["id"] for record in records}), len(records))
                for record in records:
                    self.assertEqual(
                        set(record), {"id", "location_cm", "rotation_degrees", "scale"}
                    )
                    self.assertEqual(len(record["location_cm"]), 3)
                    self.assertEqual(len(record["rotation_degrees"]), 3)
                    self.assertEqual(len(record["scale"]), 3)

    def test_independent_calls_are_byte_and_hash_identical(self):
        first = generate_seeded_proxy_transforms(copy.deepcopy(self.config))
        second = generate_seeded_proxy_transforms(copy.deepcopy(self.config))
        self.assertEqual(
            json.dumps(first, sort_keys=True, separators=(",", ":")),
            json.dumps(second, sort_keys=True, separators=(",", ":")),
        )
        self.assertEqual(canonical_layout_hash(first), canonical_layout_hash(second))

    def test_hash_is_stable_when_transform_order_is_reversed(self):
        layout = generate_seeded_proxy_transforms(self.config)
        reversed_layout = {
            "mountains": list(reversed(layout["mountains"])),
            "foliage": list(reversed(layout["foliage"])),
        }
        first = canonical_layout_hash(layout)
        second = canonical_layout_hash(reversed_layout)
        self.assertEqual(first["sha256"], second["sha256"])
        self.assertEqual(first["canonical_json"], second["canonical_json"])

    def test_different_seed_changes_hash(self):
        changed = copy.deepcopy(self.config)
        changed["seed"] += 1
        self.assertNotEqual(
            canonical_layout_hash(generate_seeded_proxy_transforms(self.config))["sha256"],
            canonical_layout_hash(generate_seeded_proxy_transforms(changed))["sha256"],
        )

    def test_foliage_respects_every_configured_exclusion(self):
        layout = generate_seeded_proxy_transforms(self.config)
        sources = {
            "anchor_circle": {item["id"]: item for item in self.config["fixed_anchors"]},
            "road_corridor": {item["id"]: item for item in self.config["road_splines"]},
            "river_corridor": {item["id"]: item for item in self.config["river_splines"]},
            "building_footprint": {item["id"]: item for item in self.config["building_plots"]},
        }
        x_min, x_max, y_min, y_max = self.config["playable_bounds_cm"]
        for record in layout["foliage"]:
            point = record["location_cm"][:2]
            self.assertTrue(x_min <= point[0] <= x_max and y_min <= point[1] <= y_max)
            for exclusion in self.config["exclusion_zones"]:
                source = sources[exclusion["kind"]][exclusion["source_id"]]
                margin = exclusion["margin_cm"]
                with self.subTest(record=record["id"], exclusion=exclusion["id"]):
                    if exclusion["kind"] == "anchor_circle":
                        self.assertGreater(
                            math.dist(point, source["location_cm"][:2]),
                            source["protected_radius_cm"] + margin,
                        )
                    elif exclusion["kind"] in ("road_corridor", "river_corridor"):
                        distance = min(
                            _distance_to_segment(point, a, b)
                            for a, b in zip(source["points_cm"], source["points_cm"][1:])
                        )
                        self.assertGreater(distance, source["width_cm"] / 2.0 + margin)
                    else:
                        self.assertFalse(_inside_rotated_plot(point, source, margin))

    def test_exclusions_are_selected_by_metadata_not_hardcoded_ids(self):
        config = copy.deepcopy(self.config)
        config["proxy_generation"]["foliage"]["count"] = 20
        for collection in ("fixed_anchors", "road_splines", "river_splines", "building_plots"):
            for index, source in enumerate(config[collection]):
                old_id = source["id"]
                new_id = f"Renamed{collection}{index}"
                source["id"] = new_id
                for exclusion in config["exclusion_zones"]:
                    if exclusion["source_id"] == old_id:
                        exclusion["source_id"] = new_id
        layout = generate_seeded_proxy_transforms(config)
        self.assertEqual(len(layout["foliage"]), 20)

    def test_mountains_stay_in_world_perimeter_and_cover_all_sides(self):
        layout = generate_seeded_proxy_transforms(self.config)
        px0, px1, py0, py1 = self.config["playable_bounds_cm"]
        wx0, wx1, wy0, wy1 = self.config["world_bounds_cm"]
        sides = {"north": [], "south": [], "east": [], "west": []}
        inner, outer = self.config["proxy_generation"]["mountains"]["perimeter_band_cm"]
        for record in layout["mountains"]:
            x, y, _ = record["location_cm"]
            self.assertTrue(wx0 <= x <= wx1 and wy0 <= y <= wy1)
            self.assertFalse(px0 <= x <= px1 and py0 <= y <= py1)
            side = record["id"].split("_")[1].lower()
            sides[side].append(record)
            if side in ("north", "south"):
                self.assertTrue(px0 <= x <= px1)
                offset = y - py1 if side == "north" else py0 - y
            else:
                self.assertTrue(py0 <= y <= py1)
                offset = x - px1 if side == "east" else px0 - x
            self.assertGreaterEqual(offset, inner)
            self.assertLessEqual(offset, outer)
        self.assertTrue(all(len(records) >= 4 for records in sides.values()))
        for side, records in sides.items():
            coordinates = [
                record["location_cm"][0 if side in ("north", "south") else 1]
                for record in records
            ]
            gaps = [round(b - a, 6) for a, b in zip(sorted(coordinates), sorted(coordinates)[1:])]
            self.assertGreater(len(set(gaps)), 1, f"{side} mountains must not be equally spaced")

    def test_generation_does_not_mutate_global_random_state(self):
        random.seed(8675309)
        before = random.getstate()
        generate_seeded_proxy_transforms(self.config)
        self.assertEqual(random.getstate(), before)

    def test_impossible_foliage_fill_reports_requested_and_accepted(self):
        config = copy.deepcopy(self.config)
        config["proxy_generation"]["foliage"]["count"] = 1
        config["exclusion_zones"] = [{
            "id": "CoverEverything",
            "kind": "anchor_circle",
            "source_id": config["fixed_anchors"][0]["id"],
            "margin_cm": 100000,
        }]
        with self.assertRaisesRegex(RuntimeError, r"requested=1.*accepted=0"):
            generate_seeded_proxy_transforms(config)

    def test_malformed_generation_config_has_clear_field_errors(self):
        cases = (
            (lambda c: c.pop("seed"), "seed"),
            (lambda c: c.__setitem__("playable_bounds_cm", [0, 1, 2]), "playable_bounds_cm"),
            (lambda c: c["proxy_generation"]["foliage"].__setitem__("count", True), "foliage.count"),
            (lambda c: c["exclusion_zones"][0].__setitem__("source_id", "Missing"), "source_id"),
            (lambda c: c["proxy_generation"]["mountains"].__setitem__("perimeter_band_cm", [9000, 10000]), "perimeter_band_cm"),
        )
        for mutate, message in cases:
            with self.subTest(message=message):
                config = copy.deepcopy(self.config)
                mutate(config)
                with self.assertRaisesRegex(ValueError, message):
                    generate_seeded_proxy_transforms(config)

    def test_generation_eagerly_rejects_invalid_geometry_when_counts_are_zero(self):
        mutations = (
            (lambda c: c["fixed_anchors"][1].__setitem__("protected_radius_cm", 0), "protected_radius_cm"),
            (lambda c: c["road_splines"][0].__setitem__("width_cm", -1), "width_cm"),
            (lambda c: c["road_splines"][0].__setitem__("points_cm", []), "points_cm"),
            (lambda c: c["river_splines"][0]["points_cm"][1].__setitem__(0, float("nan")), "finite"),
            (lambda c: c["building_plots"][0]["size_cm"].__setitem__(0, 0), "size_cm"),
            (lambda c: c["building_plots"][0].__setitem__("yaw_degrees", float("inf")), "yaw_degrees"),
            (lambda c: c["proxy_generation"]["foliage"].__setitem__("exclusion_margin_cm", float("nan")), "exclusion_margin_cm"),
            (lambda c: c.update({
                "playable_bounds_cm": [-1e308, 1e308, -1000, 1000],
                "world_bounds_cm": [-1e308, 1e308, -2000, 2000],
            }), "MAX_SAFE_COORDINATE_CM"),
        )
        for mutate, message in mutations:
            with self.subTest(message=message):
                config = copy.deepcopy(self.config)
                config["proxy_generation"]["foliage"]["count"] = 0
                config["proxy_generation"]["mountains"]["count"] = 0
                mutate(config)
                with self.assertRaisesRegex(ValueError, message):
                    generate_seeded_proxy_transforms(config)

    def test_generation_validates_later_exclusions_before_predicate_short_circuit(self):
        config = copy.deepcopy(self.config)
        config["proxy_generation"]["foliage"]["count"] = 1
        config["exclusion_zones"][0]["margin_cm"] = 100000
        config["road_splines"][0]["points_cm"][0][0] = float("nan")
        with self.assertRaisesRegex(ValueError, "finite"):
            generate_seeded_proxy_transforms(config)

    def test_generation_rejects_task1_valid_extreme_road_before_geometry_arithmetic(self):
        config = copy.deepcopy(self.config)
        config["world_bounds_cm"] = [-5e199, 5e199, -5e199, 5e199]
        original_points = config["road_splines"][0]["points_cm"]
        config["road_splines"][0]["points_cm"] = [
            [-4e199, 0, 0],
            [4e199, 0, 0],
            *original_points,
        ]
        config_module = _load_config_module()
        self.assertEqual(config_module.validate_config(copy.deepcopy(config)), config)
        with self.assertRaisesRegex(ValueError, "MAX_SAFE_COORDINATE_CM"):
            generate_seeded_proxy_transforms(config)

    def test_generation_rejects_derived_exclusion_extents_beyond_safe_coordinate(self):
        cases = (
            (
                lambda c: (
                    c["fixed_anchors"][0].__setitem__("protected_radius_cm", 600_000_000),
                    c["exclusion_zones"][0].__setitem__("margin_cm", 600_000_000),
                ),
                "protected radius plus margin",
            ),
            (
                lambda c: (
                    c["road_splines"][0].__setitem__("width_cm", 1_000_000_000),
                    c["exclusion_zones"][3].__setitem__("margin_cm", 600_000_000),
                ),
                "corridor half-width plus margin",
            ),
            (
                lambda c: (
                    c["building_plots"][0].__setitem__("size_cm", [1_000_000_000, 100, 100]),
                    c["exclusion_zones"][7].__setitem__("margin_cm", 600_000_000),
                ),
                "footprint half-size plus margin",
            ),
        )
        for mutate, message in cases:
            with self.subTest(message=message):
                config = copy.deepcopy(self.config)
                mutate(config)
                with self.assertRaisesRegex(ValueError, message):
                    generate_seeded_proxy_transforms(config)

    def test_generation_rejects_counts_over_caps_or_hard_limits(self):
        cases = (
            ("foliage", 101, "spawn_caps.foliage"),
            ("mountains", 31, "spawn_caps.mountains"),
            ("foliage", True, "foliage.count"),
            ("mountains", -1, "mountains.count"),
            ("foliage", 10**10000, "hard maximum"),
            ("mountains", 10**10000, "hard maximum"),
        )
        for category, count, message in cases:
            with self.subTest(category=category, huge=count > 1_000_000 if isinstance(count, int) else False):
                config = copy.deepcopy(self.config)
                config["proxy_generation"][category]["count"] = count
                if isinstance(count, int) and not isinstance(count, bool) and count > config["spawn_caps"][category]:
                    config["spawn_caps"][category] = count
                if count in (101, 31):
                    config["spawn_caps"][category] = count - 1
                with self.assertRaisesRegex(ValueError, message):
                    generate_seeded_proxy_transforms(config)

    def test_generation_rejects_invalid_spawn_caps_and_zero_inner_perimeter(self):
        cases = (
            (lambda c: c["spawn_caps"].__setitem__("foliage", True), "spawn_caps.foliage"),
            (lambda c: c["spawn_caps"].__setitem__("mountains", -1), "spawn_caps.mountains"),
            (lambda c: c["proxy_generation"]["mountains"].__setitem__("perimeter_band_cm", [0, 7000]), "inner perimeter"),
        )
        for mutate, message in cases:
            with self.subTest(message=message):
                config = copy.deepcopy(self.config)
                mutate(config)
                with self.assertRaisesRegex(ValueError, message):
                    generate_seeded_proxy_transforms(config)

    def test_hash_rounding_tolerance_and_metadata(self):
        payload_a = {"foliage": [{
            "id": "FOLIAGE_001",
            "location_cm": [1.23444, 2.0, 3.0],
            "rotation_degrees": [0.0, 0.0, 12.0],
            "scale": [2.0, 2.0, 2.0],
        }], "mountains": []}
        payload_b = copy.deepcopy(payload_a)
        payload_b["foliage"][0]["location_cm"][0] = 1.23449
        result = canonical_layout_hash(payload_a, decimals=3)
        self.assertEqual(result["decimal_places"], 3)
        self.assertEqual(result["quantization_step"], 0.001)
        self.assertEqual(result["counts"], {"foliage": 1, "mountains": 0})
        self.assertEqual(result["sha256"], canonical_layout_hash(payload_b, decimals=3)["sha256"])

    def test_hash_rejects_bool_nonfinite_huge_and_malformed_values(self):
        base = {"foliage": [{
            "id": "FOLIAGE_001",
            "location_cm": [1.0, 2.0, 3.0],
            "rotation_degrees": [0.0, 0.0, 12.0],
            "scale": [2.0, 2.0, 2.0],
        }], "mountains": []}
        mutations = (
            (lambda p: p["foliage"][0]["location_cm"].__setitem__(0, True), "bool"),
            (lambda p: p["foliage"][0]["scale"].__setitem__(0, float("nan")), "finite"),
            (lambda p: p["foliage"][0]["scale"].__setitem__(0, float("inf")), "finite"),
            (lambda p: p["foliage"][0]["scale"].__setitem__(0, 10**10000), "finite"),
            (lambda p: p["foliage"][0].__setitem__("id", ""), "id"),
            (lambda p: p["foliage"][0].__setitem__("scale", [1.0, 2.0]), "scale"),
            (lambda p: p["foliage"][0]["location_cm"].__setitem__(0, "one"), "number"),
            (lambda p: p["foliage"][0].pop("rotation_degrees"), "missing rotation_degrees"),
        )
        for mutate, message in mutations:
            with self.subTest(message=message):
                payload = copy.deepcopy(base)
                mutate(payload)
                with self.assertRaisesRegex(ValueError, message):
                    canonical_layout_hash(payload)
        for decimals in (True, -1, 16):
            with self.subTest(decimals=decimals):
                with self.assertRaisesRegex(ValueError, "decimals"):
                    canonical_layout_hash(base, decimals=decimals)

    def test_hash_preserves_distinct_integers_above_float_precision(self):
        first = {"seed": 2**53, "foliage": [], "mountains": []}
        second = {"seed": 2**53 + 1, "foliage": [], "mountains": []}
        first_hash = canonical_layout_hash(first)
        second_hash = canonical_layout_hash(second)
        self.assertEqual(first_hash["canonical_payload"]["seed"], 2**53)
        self.assertEqual(second_hash["canonical_payload"]["seed"], 2**53 + 1)
        self.assertNotEqual(first_hash["sha256"], second_hash["sha256"])

    def test_hash_rejects_partial_or_idless_transform_mappings(self):
        cases = (
            ({"location_cm": [1, 2, 3], "rotation_degrees": [0, 0, 0], "scale": [1, 1, 1]}, "id"),
            ({"id": "", "location_cm": [1, 2, 3], "rotation_degrees": [0, 0, 0], "scale": [1, 1, 1]}, "id"),
            ({"id": "bad id", "location_cm": [1, 2, 3], "rotation_degrees": [0, 0, 0], "scale": [1, 1, 1]}, "stable"),
            ({"id": "VALID_1", "location_cm": [1, 2, 3]}, "missing"),
        )
        for record, message in cases:
            with self.subTest(message=message):
                with self.assertRaisesRegex(ValueError, message):
                    canonical_layout_hash({"foliage": [record], "mountains": []})

    def test_hash_rejects_non_string_mapping_keys_with_field_context(self):
        cases = (
            ({"foliage": [], 1: []}, "payload keys"),
            ({"metadata": {"okay": 1, 2: 2}, "foliage": [], "mountains": []}, "payload.metadata keys"),
        )
        for payload, message in cases:
            with self.subTest(message=message):
                with self.assertRaisesRegex(ValueError, message):
                    canonical_layout_hash(payload)


if __name__ == "__main__":
    unittest.main()
