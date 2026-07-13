import json
import sys
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT_ROOT / "Content" / "Python"))

INVENTORY_SCRIPT = (
    PROJECT_ROOT
    / "Content"
    / "Python"
    / "gamexxk_inventory_asian_village_occlusion_materials.py"
)
INVENTORY_MANIFEST = (
    PROJECT_ROOT
    / "Config"
    / "GameXXK"
    / "Occlusion"
    / "AsianVillageMaterialInventory.json"
)
AUTHOR_SCRIPT = (
    PROJECT_ROOT
    / "Content"
    / "Python"
    / "gamexxk_author_asian_village_occlusion_materials.py"
)
AUTHOR_REPORT = (
    PROJECT_ROOT
    / "Config"
    / "GameXXK"
    / "Occlusion"
    / "AsianVillageMaterialAuthoringReport.json"
)
ACTIVE_GENERATION = (
    PROJECT_ROOT
    / "Config"
    / "GameXXK"
    / "Occlusion"
    / "AsianVillageOcclusionActiveGeneration.json"
)

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

    def test_near_match_exclusion_directory_is_eligible(self):
        source_path = (
            "/Game/Asian_Village/materials/notwater_materials_backup/MI_Wall.MI_Wall"
        )

        self.assertTrue(is_eligible_material(source_path))

    def test_asset_name_containing_exclusion_text_is_eligible(self):
        source_path = (
            "/Game/Asian_Village/Materials/Buildings/"
            "MI_sky_materials_Wall.MI_sky_materials_Wall"
        )

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


class OcclusionMaterialInventoryContractTests(unittest.TestCase):
    def test_inventory_script_has_required_read_only_contract(self):
        self.assertTrue(INVENTORY_SCRIPT.is_file())
        source = INVENTORY_SCRIPT.read_text(encoding="utf-8")

        self.assertIn("StaticMeshComponent", source)
        for field in (
            '"source_path"',
            '"target_path"',
            '"blend_mode"',
            '"usage_count"',
            '"excluded"',
        ):
            with self.subTest(field=field):
                self.assertIn(field, source)
        self.assertIn("Config/GameXXK/Occlusion", source.replace("\\\\", "/"))
        self.assertIn("gamexxk_occlusion_material_naming", source)
        self.assertIn("is_excluded_source", source)

    def test_manifest_blend_modes_are_canonical_enum_names(self):
        inventory = json.loads(INVENTORY_MANIFEST.read_text(encoding="utf-8"))

        for material in inventory["materials"]:
            with self.subTest(source_path=material["source_path"]):
                self.assertRegex(material["blend_mode"], r"^BLEND_[A-Z_]+$")


class OcclusionMaterialAuthoringContractTests(unittest.TestCase):
    def setUp(self):
        self.assertTrue(AUTHOR_SCRIPT.is_file())
        self.source = AUTHOR_SCRIPT.read_text(encoding="utf-8")

    def test_author_handles_each_supported_blend_mode_explicitly(self):
        for blend_mode in (
            "BLEND_OPAQUE",
            "BLEND_MASKED",
            "BLEND_TRANSLUCENT",
        ):
            with self.subTest(blend_mode=blend_mode):
                self.assertIn(blend_mode, self.source)

    def test_author_uses_blend_safe_material_properties(self):
        self.assertIn("MP_OPACITY_MASK", self.source)
        self.assertIn("MP_OPACITY", self.source)
        self.assertIn("MaterialExpressionMultiply", self.source)
        self.assertIn("get_material_property_input_node", self.source)
        self.assertIn("get_material_property_input_node_output_name", self.source)
        self.assertIn("exact_output=True", self.source)

    def test_opaque_is_circle_only_while_masked_and_translucent_preserve_input(self):
        self.assertIn("preserve_existing = False", self.source)
        self.assertGreaterEqual(self.source.count("preserve_existing = True"), 2)
        self.assertIn("if preserve_existing", self.source)

    def test_author_recompiles_saves_and_only_forces_masked_roots(self):
        self.assertIn(
            "GameXXKMaterialAuthoringLibrary.force_masked_material_compilation",
            self.source,
        )
        self.assertIn("MaterialEditingLibrary.recompile_material", self.source)
        self.assertIn("EditorAssetLibrary.save_asset", self.source)
        self.assertIn("force_masked", self.source)

    def test_author_is_inventory_driven_and_project_owned(self):
        normalized = self.source.replace("\\\\", "/")
        self.assertIn("AsianVillageMaterialInventory.json", normalized)
        self.assertIn(
            "/Game/GameXXK/Materials/Occlusion/AsianVillageFull", self.source
        )
        self.assertIn("cutout_object_path", self.source)
        self.assertIn("refusing", self.source.lower())
        self.assertNotIn("delete_asset(source_path", self.source)

    def test_committed_authoring_report_proves_complete_inventory_coverage(self):
        self.assertTrue(AUTHOR_REPORT.is_file())
        report = json.loads(AUTHOR_REPORT.read_text(encoding="utf-8"))
        self.assertEqual(report["eligible_count"], 74)
        self.assertEqual(report["excluded_count"], 6)
        self.assertEqual(report["inventory_mapping_count"], 74)
        self.assertEqual(report["missing_targets"], [])
        self.assertEqual(report["failures"], [])
        self.assertEqual(report["validation_errors"], [])
        self.assertEqual(report["opaque_count"], 65)
        self.assertEqual(report["masked_count"], 8)
        self.assertEqual(report["translucent_count"], 1)
        self.assertRegex(report["content_signature"], r"^[0-9A-F]{12}$")
        self.assertTrue(report["generation_folder"].endswith(report["content_signature"]))
        self.assertEqual(report["generation_asset_count"], 98)
        self.assertTrue(report["generation_reused"])

    def test_active_generation_manifest_has_concrete_complete_mapping(self):
        manifest = json.loads(ACTIVE_GENERATION.read_text(encoding="utf-8"))
        self.assertRegex(manifest["content_signature"], r"^[0-9A-F]{12}$")
        self.assertTrue(manifest["generation_folder"].endswith(manifest["content_signature"]))
        self.assertEqual(len(manifest["inventory_mapping"]), 74)
        for concrete_path in manifest["inventory_mapping"].values():
            self.assertTrue(concrete_path.startswith(manifest["generation_folder"] + "/"))

    def test_author_uses_immutable_generation_and_atomic_manifest_switch(self):
        for required in (
            "Gen_",
            "content_signature",
            "ACTIVE_GENERATION_PATH",
            "os.replace",
            "_validate_family",
            "generation_reused",
        ):
            with self.subTest(required=required):
                self.assertIn(required, self.source)
        self.assertNotIn("rename_asset", self.source)
        self.assertNotIn("delete_asset", self.source)

    def test_success_report_is_written_only_after_canonical_validation(self):
        validation = self.source.index("validation_errors = _validate_family")
        report_write = self.source.index("REPORT_PATH.write_text")
        self.assertLess(validation, report_write)

    def test_author_supports_controlled_promotion_fault_injection(self):
        self.assertIn("--fault-before-manifest-switch", self.source)
        self.assertIn("intentional pre-switch fault", self.source)


if __name__ == "__main__":
    unittest.main()
