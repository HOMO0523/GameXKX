from __future__ import annotations

import hashlib
import importlib.util
import json
import math
import tempfile
import unittest
from pathlib import Path

from PIL import Image


PROJECT_ROOT = Path(__file__).resolve().parents[1]
BUILDER_PATH = PROJECT_ROOT / "scripts" / "build_qingshan_b1_heightmap.py"
COMMITTED_HEIGHTMAP = (
    PROJECT_ROOT
    / "Content"
    / "ArtSource"
    / "Qingshan"
    / "B1"
    / "H_QS_B1_Terrain_505.png"
)
COMMITTED_SAMPLES = COMMITTED_HEIGHTMAP.with_suffix(".samples.json")


def _load_builder():
    spec = importlib.util.spec_from_file_location("qingshan_b1_heightmap", BUILDER_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"unable to load heightmap builder: {BUILDER_PATH}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _pixels(image: Image.Image) -> list[int]:
    data = (
        image.get_flattened_data()
        if hasattr(image, "get_flattened_data")
        else image.getdata()
    )
    return [int(value) for value in data]


def _max_adjacent_delta(image: Image.Image) -> int:
    width, height = image.size
    values = _pixels(image)
    maximum = 0
    for y in range(height):
        row = y * width
        for x in range(width):
            value = values[row + x]
            if x + 1 < width:
                maximum = max(maximum, abs(value - values[row + x + 1]))
            if y + 1 < height:
                maximum = max(maximum, abs(value - values[row + width + x]))
    return maximum


class QingshanB1HeightmapTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.builder = _load_builder()

    def _generate_pair(self, root: Path):
        heightmap = root / "H_QS_B1_Terrain_505.png"
        samples = root / "H_QS_B1_Terrain_505.samples.json"
        result = self.builder.generate_heightmap(heightmap, samples)
        self.assertEqual(result["heightmap"], str(heightmap.resolve()))
        self.assertEqual(result["samples"], str(samples.resolve()))
        self.assertTrue(heightmap.is_file())
        self.assertTrue(samples.is_file())
        return heightmap, samples, json.loads(samples.read_text(encoding="utf-8"))

    def test_generation_is_byte_deterministic_and_writes_current_digest(self):
        with tempfile.TemporaryDirectory() as first_dir, tempfile.TemporaryDirectory() as second_dir:
            first_png, first_json, first = self._generate_pair(Path(first_dir))
            second_png, second_json, second = self._generate_pair(Path(second_dir))

            self.assertEqual(first_png.read_bytes(), second_png.read_bytes())
            self.assertEqual(first_json.read_bytes(), second_json.read_bytes())
            self.assertEqual(first, second)
            self.assertEqual(first["png_sha256"], _sha256(first_png))
            self.assertEqual(first["schema_version"], 1)

    def test_heightmap_is_16bit_505_square_with_broad_relief(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            heightmap, _samples, payload = self._generate_pair(Path(temp_dir))
            with Image.open(heightmap) as image:
                self.assertEqual(image.size, (505, 505))
                self.assertIn(image.mode, {"I;16", "I;16L", "I"})
                self.assertLessEqual(_max_adjacent_delta(image), 900)

            samples = payload["named_samples"]
            self.assertEqual(
                set(samples), {"gate", "core", "south", "dock_bank", "riverbed"}
            )
            decoded = {name: sample["decoded_elevation_cm"] for name, sample in samples.items()}
            self.assertGreater(decoded["core"], decoded["gate"])
            self.assertGreater(decoded["core"], decoded["south"])
            self.assertGreater(decoded["south"], decoded["gate"])
            self.assertGreater(decoded["gate"], decoded["dock_bank"])
            self.assertGreater(decoded["dock_bank"], decoded["riverbed"])
            self.assertGreaterEqual(decoded["core"] - decoded["dock_bank"], 320.0)

    def test_named_samples_use_unreal_encoding_and_spatial_contract(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            heightmap, _samples, payload = self._generate_pair(Path(temp_dir))
            config = self.builder.load_b1_config()
            scale_z = float(config["landscape"]["scale_cm"][2])
            encoded_step_cm = scale_z / 128.0

            with Image.open(heightmap) as image:
                for name, sample in payload["named_samples"].items():
                    pixel = tuple(sample["pixel_xy"])
                    raw = int(image.getpixel(pixel))
                    decoded = (raw - 32768) * scale_z / 128.0
                    self.assertEqual(raw, sample["raw"])
                    self.assertAlmostEqual(decoded, sample["decoded_elevation_cm"], places=7)
                    expected = self.builder.terrain_elevation_cm(
                        config, *sample["local_cm"]
                    )
                    self.assertLessEqual(abs(decoded - expected), encoded_step_cm)

            river = self.builder.main_river_spline(config)["points_cm"]
            dock_bank = payload["named_samples"]["dock_bank"]["local_cm"]
            riverbed = payload["named_samples"]["riverbed"]["local_cm"]
            self.assertGreaterEqual(
                self.builder.polyline_distance_cm(dock_bank, river), 1400.0
            )
            self.assertLessEqual(
                self.builder.polyline_distance_cm(riverbed, river), 1.0
            )

    def test_landscape_center_maps_to_actor_origin_and_pixel_grid(self):
        config = self.builder.load_b1_config()
        landscape = config["landscape"]
        resolution = landscape["resolution"]
        scale = landscape["scale_cm"]
        center = landscape["center_local_cm"]
        expected_origin = [
            center[0] - (resolution[0] - 1) * scale[0] / 2.0,
            center[1] - (resolution[1] - 1) * scale[1] / 2.0,
            center[2],
        ]
        self.assertEqual(landscape["origin_local_cm"], expected_origin)
        self.assertEqual(
            self.builder.landscape_actor_origin_local_cm(landscape), expected_origin
        )
        self.assertEqual(self.builder.local_to_pixel(landscape, center[:2]), (252, 252))
        self.assertEqual(
            self.builder.pixel_to_local(landscape, (252, 252)),
            (float(center[0]), float(center[1])),
        )
        self.assertEqual(
            self.builder.pixel_to_local(landscape, (0, 0)),
            (float(expected_origin[0]), float(expected_origin[1])),
        )
        self.assertEqual(
            self.builder.pixel_to_local(landscape, (504, 504)),
            (
                float(expected_origin[0] + 504 * scale[0]),
                float(expected_origin[1] + 504 * scale[1]),
            ),
        )

    def test_noise_is_two_low_frequency_octaves_with_at_most_20cm_amplitude(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            _heightmap, _samples, payload = self._generate_pair(Path(temp_dir))
        octaves = payload["algorithm"]["noise_octaves"]
        self.assertEqual(len(octaves), 2)
        self.assertGreaterEqual(min(item["wavelength_cm"] for item in octaves), 6000.0)
        self.assertLessEqual(sum(abs(item["amplitude_cm"]) for item in octaves), 20.0)
        self.assertEqual(payload["encoding"], {
            "value_type": "uint16",
            "zero_raw": 32768,
            "scale_z_cm": 100.0,
            "units_per_cm": 1.28,
            "formula": "raw=clamp(round(32768+elevation_cm*128/scale_z_cm),0,65535)",
        })

    def test_committed_artifacts_match_a_fresh_generation(self):
        self.assertTrue(COMMITTED_HEIGHTMAP.is_file())
        self.assertTrue(COMMITTED_SAMPLES.is_file())
        with tempfile.TemporaryDirectory() as temp_dir:
            generated_png, generated_json, _payload = self._generate_pair(Path(temp_dir))
            self.assertEqual(COMMITTED_HEIGHTMAP.read_bytes(), generated_png.read_bytes())
            self.assertEqual(COMMITTED_SAMPLES.read_bytes(), generated_json.read_bytes())


if __name__ == "__main__":
    unittest.main()
