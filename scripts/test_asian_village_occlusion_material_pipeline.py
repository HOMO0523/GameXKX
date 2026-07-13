import sys
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT_ROOT / "Content" / "Python"))

from gamexxk_occlusion_material_naming import (  # noqa: E402
    cutout_asset_name,
    cutout_object_path,
    is_eligible_material,
)


class OcclusionMaterialNamingTests(unittest.TestCase):
    def test_cutout_asset_name_is_stable(self):
        source_path = "/Game/Asian_Village/A/MI_same.MI_same"

        self.assertEqual(cutout_asset_name(source_path), "MI_OC_MI_same_BF6C7B76")
        self.assertEqual(cutout_asset_name(source_path), cutout_asset_name(source_path))

    def test_same_object_name_at_different_paths_produces_different_names(self):
        path_a = "/Game/Asian_Village/A/MI_same.MI_same"
        path_b = "/Game/Asian_Village/B/MI_same.MI_same"

        self.assertNotEqual(cutout_asset_name(path_a), cutout_asset_name(path_b))

    def test_cutout_asset_name_uses_expected_prefix(self):
        source_path = "/Game/Asian_Village/A/MI_same.MI_same"

        self.assertTrue(cutout_asset_name(source_path).startswith("MI_OC_MI_same_"))

    def test_ineligible_material_categories_are_excluded_case_insensitively(self):
        excluded_paths = (
            "/Game/Asian_Village/Materials/water_materials/MI_Water.MI_Water",
            "/Game/Asian_Village/Materials/PostProcess_Materials/MI_PP.MI_PP",
            "/Game/Asian_Village/Materials/SKY_MATERIALS/MI_Sky.MI_Sky",
        )

        for source_path in excluded_paths:
            with self.subTest(source_path=source_path):
                self.assertFalse(is_eligible_material(source_path))

    def test_building_material_is_eligible(self):
        source_path = "/Game/Asian_Village/Materials/Buildings/MI_Wall.MI_Wall"

        self.assertTrue(is_eligible_material(source_path))

    def test_material_outside_asian_village_is_excluded(self):
        source_path = "/Game/GameXXK/Materials/MI_Wall.MI_Wall"

        self.assertFalse(is_eligible_material(source_path))

    def test_cutout_object_path_uses_destination_and_asset_name(self):
        source_path = "/Game/Asian_Village/A/MI_same.MI_same"
        asset_name = cutout_asset_name(source_path)

        self.assertEqual(
            cutout_object_path(source_path),
            f"/Game/GameXXK/Materials/Occlusion/AsianVillageFull/{asset_name}.{asset_name}",
        )


if __name__ == "__main__":
    unittest.main()
