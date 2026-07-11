from __future__ import annotations

import hashlib
import importlib.util
import json
import subprocess
import tempfile
import unittest
from collections import deque
from pathlib import Path

from PIL import Image


PROJECT_ROOT = Path(__file__).resolve().parents[1]
BUILDER_PATH = PROJECT_ROOT / "scripts" / "blender" / "build_qingshan_b1_proxy_kit.py"
BLENDER_EXE = Path("D:/Blender/blender.exe")
PLANT_STRIP = (
    PROJECT_ROOT
    / "Content"
    / "ArtSource"
    / "Qingshan"
    / "B1"
    / "T_QS_B1_PlantProxy_4F.png"
)

BUILDINGS = {
    "gable_shop",
    "tall_house",
    "wide_house",
    "courtyard_wing",
    "bridge_house",
    "dock_shed",
}
PROPS = {
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
}
PROP_SLOTS = {
    "sign": ["Timber", "Prop"],
    "lantern": ["Timber", "WindowPaper"],
    "banner": ["Timber", "Prop"],
    "fence": ["Timber"],
    "crate": ["Timber"],
    "stall": ["Timber", "Prop"],
    "well": ["Prop", "Timber"],
    "cart": ["Timber"],
    "rock": ["Prop"],
    "dock_post": ["Timber"],
    "plant_card": ["Foliage"],
    "mountain": ["Mountain"],
}
EXPORT_SETTINGS = {
    "axis_forward": "-Y",
    "axis_up": "Z",
    "use_triangles": True,
    "apply_unit_scale": True,
    "apply_scale_options": "FBX_SCALE_UNITS",
    "path_mode": "STRIP",
    "bake_anim": False,
}


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _load_builder_host_safe():
    spec = importlib.util.spec_from_file_location("qingshan_b1_proxy_kit", BUILDER_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"unable to load proxy builder: {BUILDER_PATH}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _run_blender(output: Path) -> tuple[dict, str]:
    command = [
        str(BLENDER_EXE),
        "--background",
        "--factory-startup",
        "--python",
        str(BUILDER_PATH),
        "--",
        "--output",
        str(output),
    ]
    result = subprocess.run(
        command,
        cwd=PROJECT_ROOT,
        capture_output=True,
        text=True,
        timeout=180,
    )
    combined = result.stdout + result.stderr
    if result.returncode != 0:
        raise AssertionError(f"Blender proxy generation failed ({result.returncode}):\n{combined}")
    manifest_path = output / "proxy-kit-manifest.json"
    if not manifest_path.is_file():
        raise AssertionError(f"missing proxy manifest: {manifest_path}\n{combined}")
    return json.loads(manifest_path.read_text(encoding="utf-8")), combined


def _alpha_component_ratio(alpha: Image.Image, threshold: int = 8) -> float:
    pixels = alpha.load()
    remaining = {
        (x, y)
        for y in range(alpha.height)
        for x in range(alpha.width)
        if pixels[x, y] > threshold
    }
    total = len(remaining)
    if total == 0:
        return 0.0
    largest = 0
    while remaining:
        queue = deque([remaining.pop()])
        count = 0
        while queue:
            x, y = queue.popleft()
            count += 1
            for delta_y in (-1, 0, 1):
                for delta_x in (-1, 0, 1):
                    if delta_x == 0 and delta_y == 0:
                        continue
                    neighbor = (x + delta_x, y + delta_y)
                    if neighbor in remaining:
                        remaining.remove(neighbor)
                        queue.append(neighbor)
        largest = max(largest, count)
    return largest / total


class PlantStripSourceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not PLANT_STRIP.is_file():
            raise AssertionError(f"missing approved plant strip: {PLANT_STRIP}")
        cls.image = Image.open(PLANT_STRIP).convert("RGBA")
        cls.cells = [
            cls.image.crop((index * 512, 0, (index + 1) * 512, 512))
            for index in range(4)
        ]

    @classmethod
    def tearDownClass(cls):
        cls.image.close()
        for cell in cls.cells:
            cell.close()

    def test_strip_is_original_rgba_size_with_four_unique_equal_frames(self):
        with Image.open(PLANT_STRIP) as source:
            self.assertEqual(source.size, (2048, 512))
            self.assertEqual(source.mode, "RGBA")
        frame_hashes = {
            hashlib.sha256(cell.tobytes()).hexdigest() for cell in self.cells
        }
        self.assertEqual(len(frame_hashes), 4)

    def test_magenta_chroma_key_is_absent_from_visible_pixels(self):
        pixels = (
            self.image.get_flattened_data()
            if hasattr(self.image, "get_flattened_data")
            else self.image.getdata()
        )
        visible = [pixel for pixel in pixels if pixel[3] > 8]
        self.assertTrue(visible)
        magenta = [
            pixel
            for pixel in visible
            if pixel[0] > 180 and pixel[2] > 180 and pixel[1] < 120
        ]
        self.assertEqual(magenta, [])

    def test_each_frame_has_transparent_border_and_fits_content_box(self):
        for index, cell in enumerate(self.cells):
            with self.subTest(frame=index):
                alpha = cell.getchannel("A")
                bbox = alpha.point(lambda value: 255 if value > 8 else 0).getbbox()
                self.assertIsNotNone(bbox)
                left, top, right, bottom = bbox
                self.assertLessEqual(right - left, 360)
                self.assertLessEqual(bottom - top, 432)
                border = [
                    *(alpha.getpixel((x, 0)) for x in range(512)),
                    *(alpha.getpixel((x, 511)) for x in range(512)),
                    *(alpha.getpixel((0, y)) for y in range(512)),
                    *(alpha.getpixel((511, y)) for y in range(512)),
                ]
                self.assertEqual(max(border), 0)

    def test_root_pivots_align_to_256_472_within_four_pixels(self):
        for index, cell in enumerate(self.cells):
            with self.subTest(frame=index):
                alpha = cell.getchannel("A")
                pixels = alpha.load()
                opaque = [
                    (x, y, pixels[x, y])
                    for y in range(512)
                    for x in range(512)
                    if pixels[x, y] > 8
                ]
                bottom_y = max(y for _x, y, _value in opaque)
                root_band = [
                    (x, value)
                    for x, y, value in opaque
                    if bottom_y - 31 <= y <= bottom_y
                ]
                root_x = sum(x * value for x, value in root_band) / sum(
                    value for _x, value in root_band
                )
                self.assertLessEqual(abs(root_x - 256.0), 4.0)
                self.assertLessEqual(abs(bottom_y - 472), 4)

    def test_alpha_is_connected_and_area_stays_within_fifteen_percent(self):
        alpha_areas = []
        for index, cell in enumerate(self.cells):
            alpha = cell.getchannel("A")
            with self.subTest(frame=index):
                self.assertGreaterEqual(_alpha_component_ratio(alpha), 0.98)
            flattened = (
                alpha.get_flattened_data()
                if hasattr(alpha, "get_flattened_data")
                else alpha.getdata()
            )
            alpha_areas.append(sum(int(value) for value in flattened) / 255.0)
        mean_area = sum(alpha_areas) / len(alpha_areas)
        self.assertLessEqual((max(alpha_areas) - min(alpha_areas)) / mean_area, 0.15)

    def test_crown_centroid_moves_monotonically_left_to_right(self):
        centroids = []
        for cell in self.cells:
            alpha = cell.getchannel("A")
            pixels = alpha.load()
            crown = [
                (x, pixels[x, y])
                for y in range(330)
                for x in range(512)
                if pixels[x, y] > 8
            ]
            centroids.append(
                sum(x * value for x, value in crown)
                / sum(value for _x, value in crown)
            )
        self.assertEqual(centroids, sorted(centroids))
        self.assertTrue(
            all(following - current >= 20.0 for current, following in zip(centroids, centroids[1:]))
        )


class ProxyKitHostContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.builder = _load_builder_host_safe()

    def test_dry_run_manifest_declares_exact_categories_and_slots(self):
        manifest = self.builder.build_manifest_dry_run()
        self.assertEqual(set(manifest["buildings"]), BUILDINGS)
        self.assertEqual(set(manifest["props"]), PROPS)
        for mesh in manifest["buildings"].values():
            self.assertEqual(
                mesh["material_slots"], ["Wall", "Timber", "WindowPaper", "Roof"]
            )
            self.assertTrue(mesh["window_paper_closed"])
            self.assertEqual(mesh["normalized_bounds_m"], [1.0, 1.0, 1.0])
            self.assertEqual(mesh["pivot"], "bottom_center")
            self.assertEqual(mesh["front_axis"], "+Y")
            self.assertGreaterEqual(mesh["asymmetry_ratio"], 0.05)
            self.assertLessEqual(mesh["asymmetry_ratio"], 0.10)
            self.assertIn(mesh["roof_sections"], {3, 4, 5})
            self.assertEqual(mesh["door_count"], 1)
            self.assertGreaterEqual(mesh["window_count"], 1)
        for name, mesh in manifest["props"].items():
            self.assertEqual(mesh["material_slots"], PROP_SLOTS[name])
        self.assertEqual(manifest["export_settings"], EXPORT_SETTINGS)

    def test_source_uses_low_segment_roofs_and_has_no_text_or_tile_generation(self):
        source = BUILDER_PATH.read_text(encoding="utf-8")
        lowered = source.lower()
        self.assertNotIn("create_curved_roof", source)
        self.assertNotIn("bpy.ops.object.text_add", source)
        self.assertNotIn("roof_tile", lowered)
        self.assertIn("create_low_segment_roof", source)
        self.assertIn("create_chunky_window", source)
        self.assertIn("_create_closed_window_pane", source)


@unittest.skipUnless(BLENDER_EXE.is_file(), "Blender 4.2.3 LTS is required")
class ProxyKitBlenderIntegrationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.first_dir_obj = tempfile.TemporaryDirectory()
        cls.second_dir_obj = tempfile.TemporaryDirectory()
        cls.first_dir = Path(cls.first_dir_obj.name)
        cls.second_dir = Path(cls.second_dir_obj.name)
        cls.first, cls.first_log = _run_blender(cls.first_dir)
        cls.second, cls.second_log = _run_blender(cls.second_dir)

    @classmethod
    def tearDownClass(cls):
        cls.first_dir_obj.cleanup()
        cls.second_dir_obj.cleanup()

    def _all_entries(self, manifest):
        return {**manifest["buildings"], **manifest["props"]}

    def test_real_manifest_has_18_current_hashed_fbx_files(self):
        self.assertEqual(self.first["schema_version"], 1)
        self.assertGreaterEqual(tuple(self.first["blender_version"]), (4, 2, 3))
        self.assertEqual(set(self.first["buildings"]), BUILDINGS)
        self.assertEqual(set(self.first["props"]), PROPS)
        entries = self._all_entries(self.first)
        self.assertEqual(len(entries), 18)
        self.assertEqual(len(list(self.first_dir.glob("*.fbx"))), 18)
        for name, entry in entries.items():
            with self.subTest(mesh=name):
                path = self.first_dir / entry["file"]
                self.assertTrue(path.is_file())
                self.assertGreater(path.stat().st_size, 256)
                self.assertEqual(entry["file_sha256"], _sha256(path))
                self.assertEqual(len(entry["geometry_digest"]), 64)
                self.assertGreater(entry["vertex_count"], 0)
                self.assertGreater(entry["triangle_count"], 0)
                self.assertEqual(entry["non_manifold_edges"], 0)
                self.assertEqual(entry["pivot"], "bottom_center")
                self.assertEqual(entry["front_axis"], "+Y")
                self.assertEqual(entry["export_settings"], EXPORT_SETTINGS)
                bounds = entry["bounds_m"]
                self.assertAlmostEqual(bounds["min"][0] + bounds["max"][0], 0.0, places=5)
                self.assertAlmostEqual(bounds["min"][1] + bounds["max"][1], 0.0, places=5)
                self.assertAlmostEqual(bounds["min"][2], 0.0, places=6)
        extras = [
            path.name
            for path in self.first_dir.iterdir()
            if path.suffix.lower() in {".blend", ".blend1", ".tmp"}
        ]
        self.assertEqual(extras, [])

    def test_buildings_are_normalized_distinct_chunky_warm_paper_meshes(self):
        silhouettes = set()
        for name, entry in self.first["buildings"].items():
            with self.subTest(building=name):
                self.assertEqual(
                    entry["material_slots"], ["Wall", "Timber", "WindowPaper", "Roof"]
                )
                self.assertTrue(entry["window_paper_closed"])
                self.assertEqual(entry["window_paper_color_rgba"], [0.86, 0.66, 0.3, 1.0])
                self.assertEqual(entry["normalized_bounds_m"], [1.0, 1.0, 1.0])
                self.assertEqual(entry["bounds_m"]["min"], [-0.5, -0.5, 0.0])
                self.assertEqual(entry["bounds_m"]["max"], [0.5, 0.5, 1.0])
                self.assertEqual(entry["door_count"], 1)
                self.assertGreaterEqual(entry["window_count"], 1)
                self.assertGreaterEqual(entry["asymmetry_ratio"], 0.05)
                self.assertLessEqual(entry["asymmetry_ratio"], 0.10)
                self.assertIn(entry["roof_sections"], {3, 4, 5})
                self.assertGreaterEqual(entry["triangle_count"], 100)
                self.assertLessEqual(entry["triangle_count"], entry["triangle_budget"])
                self.assertLessEqual(entry["triangle_budget"], 2500)
                silhouettes.add(entry["silhouette_signature"])
        self.assertEqual(len(silhouettes), 6)
        self.assertEqual(
            {entry["story_count"] for entry in self.first["buildings"].values()},
            {1, 2},
        )

    def test_props_use_exact_slots_budget_and_plant_card_uv0(self):
        for name, entry in self.first["props"].items():
            with self.subTest(prop=name):
                self.assertEqual(entry["material_slots"], PROP_SLOTS[name])
                self.assertLessEqual(entry["triangle_count"], entry["triangle_budget"])
                self.assertLessEqual(entry["triangle_budget"], 1000)
        self.assertIn("UVMap", self.first["props"]["plant_card"]["uv_channels"])
        self.assertEqual(self.first["props"]["plant_card"]["uv_channel_count"], 1)

    def test_directional_cart_extends_toward_declared_positive_y_front(self):
        cart = self.first["props"]["cart"]
        self.assertEqual(cart["front_axis"], "+Y")
        self.assertGreater(cart["bounds_m"]["size"][1], cart["bounds_m"]["size"][0])

    def test_geometry_digests_are_stable_across_two_real_blender_runs(self):
        first_entries = self._all_entries(self.first)
        second_entries = self._all_entries(self.second)
        self.assertEqual(set(first_entries), set(second_entries))
        for name in sorted(first_entries):
            with self.subTest(mesh=name):
                self.assertEqual(
                    first_entries[name]["geometry_digest"],
                    second_entries[name]["geometry_digest"],
                )
                second_file = self.second_dir / second_entries[name]["file"]
                self.assertEqual(second_entries[name]["file_sha256"], _sha256(second_file))
                for field in (
                    "bounds_m",
                    "normalized_bounds_m",
                    "vertex_count",
                    "triangle_count",
                    "material_slots",
                    "uv_channels",
                    "pivot",
                    "front_axis",
                    "export_settings",
                ):
                    self.assertEqual(first_entries[name][field], second_entries[name][field])


if __name__ == "__main__":
    unittest.main()
