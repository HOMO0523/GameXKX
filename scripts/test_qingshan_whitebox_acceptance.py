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


def _load_config():
    spec = importlib.util.spec_from_file_location(
        "gamexxk_qingshan_whitebox_config_for_acceptance", CONFIG_MODULE_PATH
    )
    if spec is None or spec.loader is None:
        raise ImportError(f"Unable to load configuration module: {CONFIG_MODULE_PATH}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.load_config()


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
        for record in layout["mountains"]:
            x, y, _ = record["location_cm"]
            self.assertTrue(wx0 <= x <= wx1 and wy0 <= y <= wy1)
            self.assertFalse(px0 <= x <= px1 and py0 <= y <= py1)
            side = record["id"].split("_")[1].lower()
            sides[side].append(record)
            if side in ("north", "south"):
                self.assertTrue(px0 <= x <= px1)
            else:
                self.assertTrue(py0 <= y <= py1)
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
        self.assertEqual(result["absolute_tolerance"], 0.001)
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


if __name__ == "__main__":
    unittest.main()
