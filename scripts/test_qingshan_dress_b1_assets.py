"""Host-side tests for the Qingshan B1 UE asset authoring script."""

from __future__ import annotations

import copy
import importlib.util
from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[1]
AUTHOR_PATH = (
    PROJECT_ROOT
    / "Content"
    / "Python"
    / "gamexxk_author_qingshan_dress_b1_assets.py"
)
SOURCE_ROOT = PROJECT_ROOT / "Saved" / "Automation" / "QingshanB1ProxyKit"

EXPECTED_BUILDINGS = {
    "gable_shop",
    "tall_house",
    "wide_house",
    "courtyard_wing",
    "bridge_house",
    "dock_shed",
}
EXPECTED_PROPS = {
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
ROOF_PALETTES = {"orange", "teal", "indigo", "ochre"}
SOLID_PROPS = {"fence", "crate", "stall", "well", "cart", "rock", "dock_post"}
NO_COLLISION_PROPS = {"sign", "lantern", "banner", "plant_card", "mountain"}


def _load_author_module():
    spec = importlib.util.spec_from_file_location(
        "gamexxk_author_qingshan_dress_b1_assets",
        AUTHOR_PATH,
    )
    if spec is None or spec.loader is None:
        raise RuntimeError(f"could not load {AUTHOR_PATH}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class QingshanDressB1AssetAuthoringTests(unittest.TestCase):
    def test_asset_authoring_script_exists(self):
        self.assertTrue(AUTHOR_PATH.is_file())

    @classmethod
    def setUpClass(cls):
        cls.module = _load_author_module()
        cls.source = AUTHOR_PATH.read_text(encoding="utf-8")

    def test_real_source_manifest_and_all_18_fbx_files_validate(self):
        manifest = self.module.load_source_manifest(PROJECT_ROOT)
        self.assertEqual(set(manifest["buildings"]), EXPECTED_BUILDINGS)
        self.assertEqual(set(manifest["props"]), EXPECTED_PROPS)
        specs = self.module.source_import_specs(manifest, PROJECT_ROOT)
        self.assertEqual(len(specs), 18)
        self.assertEqual(len({item["asset_path"] for item in specs}), 18)
        for item in specs:
            self.assertTrue(Path(item["source_file"]).is_file(), item)
            self.assertEqual(len(item["source_sha256"]), 64)
            self.assertTrue(item["asset_path"].startswith(self.module.MESH_ROOT + "/"))

    def test_import_task_paths_must_prove_the_current_import_created_the_expected_asset(self):
        expected = f"{self.module.MESH_ROOT}/SM_QS_B1_Test"
        self.assertEqual(
            self.module.validate_imported_object_paths(
                expected,
                [f"{expected}.SM_QS_B1_Test"],
            ),
            [expected],
        )
        with self.assertRaisesRegex(RuntimeError, "reported no imported objects"):
            self.module.validate_imported_object_paths(expected, [])
        with self.assertRaisesRegex(RuntimeError, "did not report expected asset"):
            self.module.validate_imported_object_paths(
                expected,
                [f"{self.module.MESH_ROOT}/SM_Stale.SM_Stale"],
            )

    def test_only_buildings_use_the_normalized_100cm_import_contract(self):
        manifest = self.module.load_source_manifest(PROJECT_ROOT)
        specs = self.module.source_import_specs(manifest, PROJECT_ROOT)
        buildings = [item for item in specs if item["category"] == "building"]
        props = [item for item in specs if item["category"] == "prop"]
        self.assertEqual(len(buildings), 6)
        self.assertEqual(len(props), 12)
        self.assertTrue(all(item["expected_bounds_cm"] == [100.0, 100.0, 100.0] for item in buildings))
        self.assertTrue(any(item["expected_bounds_cm"] != [100.0, 100.0, 100.0] for item in props))

    def test_manifest_tampering_is_rejected_before_ue_import(self):
        manifest = self.module.load_source_manifest(PROJECT_ROOT)
        tampered = copy.deepcopy(manifest)
        tampered["buildings"]["gable_shop"]["material_slots"] = ["Wall", "Roof"]
        with self.assertRaisesRegex(ValueError, "material slots"):
            self.module.validate_source_manifest(tampered, PROJECT_ROOT)

    def test_slot_mapping_is_by_name_and_rejects_missing_or_unknown_slots(self):
        paths = {
            "Wall": f"{self.module.MATERIAL_ROOT}/Wall",
            "Timber": f"{self.module.MATERIAL_ROOT}/Timber",
            "WindowPaper": f"{self.module.MATERIAL_ROOT}/Paper",
            "Roof": f"{self.module.MATERIAL_ROOT}/Roof",
        }
        ordered = self.module.material_paths_for_slots(
            ["Roof", "Wall", "WindowPaper", "Timber"], paths
        )
        self.assertEqual(
            ordered,
            [paths["Roof"], paths["Wall"], paths["WindowPaper"], paths["Timber"]],
        )
        with self.assertRaisesRegex(ValueError, "slot contract"):
            self.module.material_paths_for_slots(["Wall", "Roof"], paths)
        with self.assertRaisesRegex(ValueError, "duplicate"):
            self.module.material_paths_for_slots(
                ["Wall", "Timber", "WindowPaper", "Roof", "Roof"], paths
            )

    def test_material_plan_contains_opaque_toon_and_masked_two_sided_foliage(self):
        materials = self.module.material_specs()
        toon = materials["M_QS_B1_Toon"]
        foliage = materials["M_QS_B1_FoliageCard"]
        self.assertEqual(toon["kind"], "opaque_toon_master")
        self.assertFalse(toon["two_sided"])
        self.assertEqual(toon["blend_mode"], "Opaque")
        self.assertEqual(toon["expected_expression_count"], 5)
        self.assertEqual(foliage["kind"], "masked_foliage_master")
        self.assertTrue(foliage["two_sided"])
        self.assertEqual(foliage["blend_mode"], "Masked")
        self.assertEqual(foliage["expected_expression_count"], 14)
        required_instances = {
            "MI_QS_B1_Wall_Warm",
            "MI_QS_B1_Timber_Dark",
            "MI_QS_B1_Window_Paper",
            "MI_QS_B1_Roof_Orange",
            "MI_QS_B1_Roof_Teal",
            "MI_QS_B1_Roof_Indigo",
            "MI_QS_B1_Roof_Ochre",
            "MI_QS_B1_Ground",
            "MI_QS_B1_Road_Earth",
            "MI_QS_B1_Water_Teal",
            "MI_QS_B1_Prop_Jade",
            "MI_QS_B1_Mountain_BlueGrey",
            "MI_QS_B1_Foliage_F0",
            "MI_QS_B1_Foliage_F1",
            "MI_QS_B1_Foliage_F2",
            "MI_QS_B1_Foliage_F3",
            "MI_QS_B1_Foliage_Animated",
        }
        self.assertTrue(required_instances.issubset(materials))
        self.assertEqual(
            [materials[f"MI_QS_B1_Foliage_F{i}"]["atlas_offset_x"] for i in range(4)],
            [0.0, 0.25, 0.5, 0.75],
        )
        self.assertTrue(all(materials[f"MI_QS_B1_Foliage_F{i}"]["atlas_scale_x"] == 0.25 for i in range(4)))

    def test_ue58_false_mi_setter_return_is_accepted_only_when_readback_matches(self):
        audit = self.module.confirm_parameter_write(
            "BaseColor",
            [0.6, 0.4, 0.2, 1.0],
            [0.6, 0.4, 0.2, 1.0],
            False,
        )
        self.assertFalse(audit["setter_return"])
        self.assertTrue(audit["readback_matched"])
        with self.assertRaisesRegex(RuntimeError, "readback drifted"):
            self.module.confirm_parameter_write(
                "AtlasScaleX", 1.0, 0.25, False
            )

    def test_paper2d_uses_full_rect_masked_toon_material_without_double_atlas_crop(self):
        materials = self.module.material_specs()
        animated = materials["MI_QS_B1_Foliage_Animated"]
        self.assertEqual(animated["parent"], "M_QS_B1_FoliageCard")
        self.assertEqual(animated["atlas_scale_x"], 1.0)
        self.assertEqual(animated["atlas_offset_x"], 0.0)

        paper = self.module.paper2d_spec(PROJECT_ROOT)
        expected_material = (
            f"{self.module.MATERIAL_ROOT}/MI_QS_B1_Foliage_Animated"
        )
        self.assertEqual(paper["default_material"], expected_material)
        self.assertTrue(
            all(item["default_material"] == expected_material for item in paper["sprites"])
        )
        self.assertTrue(all(item["collision_domain"] == "NONE" for item in paper["sprites"]))
        self.assertIn('_set_property(sprite, "default_material"', self.source)
        self.assertIn('_set_property(flipbook, "default_material"', self.source)

    def test_all_24_building_variants_use_warm_paper_and_four_roof_palettes(self):
        manifest = self.module.load_source_manifest(PROJECT_ROOT)
        variants = self.module.building_variant_specs(manifest)
        self.assertEqual(len(variants), 24)
        self.assertEqual(
            {(item["archetype_id"], item["roof_palette"]) for item in variants},
            {(archetype, palette) for archetype in EXPECTED_BUILDINGS for palette in ROOF_PALETTES},
        )
        self.assertEqual(len({item["asset_path"] for item in variants}), 24)
        for item in variants:
            self.assertEqual(
                item["slot_materials"]["WindowPaper"],
                f"{self.module.MATERIAL_ROOT}/MI_QS_B1_Window_Paper",
            )
            self.assertEqual(
                item["slot_materials"]["Roof"],
                f"{self.module.MATERIAL_ROOT}/MI_QS_B1_Roof_{item['roof_palette'].title()}",
            )

    def test_paper2d_plan_uses_four_exact_cells_custom_root_pivot_and_ping_pong(self):
        spec = self.module.paper2d_spec(PROJECT_ROOT)
        self.assertEqual(spec["texture_asset"], f"{self.module.PAPER2D_ROOT}/T_QS_B1_PlantProxy_4F")
        self.assertEqual(len(spec["sprites"]), 4)
        self.assertEqual(
            [item["source_uv"] for item in spec["sprites"]],
            [[0, 0], [512, 0], [1024, 0], [1536, 0]],
        )
        self.assertTrue(all(item["source_dimension"] == [512, 512] for item in spec["sprites"]))
        self.assertTrue(all(item["pivot_mode"] == "CUSTOM" for item in spec["sprites"]))
        self.assertEqual(
            [item["custom_pivot"] for item in spec["sprites"]],
            [[256, 472], [768, 472], [1280, 472], [1792, 472]],
        )
        self.assertTrue(all(item["pixels_per_unreal_unit"] == 2.4 for item in spec["sprites"]))
        self.assertEqual(spec["flipbook_asset"], f"{self.module.PAPER2D_ROOT}/FB_QS_B1_Plant_Sway")
        self.assertEqual(spec["frames_per_second"], 5.0)
        self.assertEqual(spec["frame_order"], [0, 1, 2, 3, 2, 1])

    def test_post_reload_texture_audit_uses_imported_dimensions_not_resident_mip(self):
        self.assertEqual(
            self.module.parse_texture_dimensions("2048x512"),
            [2048, 512],
        )
        with self.assertRaisesRegex(RuntimeError, "Dimensions tag"):
            self.module.parse_texture_dimensions("32")
        self.assertIn("get_tag_values", self.source)

    def test_collision_and_nanite_policies_are_exact(self):
        manifest = self.module.load_source_manifest(PROJECT_ROOT)
        specs = self.module.source_import_specs(manifest, PROJECT_ROOT)
        by_id = {item["id"]: item for item in specs}
        self.assertTrue(all(by_id[item]["collision_enabled"] for item in SOLID_PROPS))
        self.assertTrue(all(not by_id[item]["collision_enabled"] for item in NO_COLLISION_PROPS))
        self.assertTrue(all(item["collision_enabled"] for item in specs if item["category"] == "building"))
        self.assertTrue(all(item["nanite_enabled"] is False for item in specs))

    def test_all_18_source_mesh_audits_enforce_bounds_and_triangle_contracts(self):
        manifest = self.module.load_source_manifest(PROJECT_ROOT)
        specs = self.module.source_import_specs(manifest, PROJECT_ROOT)
        self.assertEqual(len(specs), 18)
        for spec in specs:
            audit = self.module.source_geometry_audit(
                spec,
                spec["expected_bounds_cm"],
                spec["triangle_count"],
                [0.0, 0.0, spec["expected_bounds_cm"][2] * 0.5],
            )
            self.assertEqual(audit["source_sha256"], spec["source_sha256"])
            self.assertEqual(audit["geometry_digest"], spec["geometry_digest"])
            self.assertEqual(audit["expected_triangle_count"], spec["triangle_count"])
            self.assertEqual(audit["actual_triangle_count"], spec["triangle_count"])
            self.assertEqual(audit["triangle_count"], spec["triangle_count"])
            self.assertEqual(audit["expected_bounds_cm"], spec["expected_bounds_cm"])
            self.assertEqual(audit["actual_bounds_cm"], spec["expected_bounds_cm"])
            self.assertAlmostEqual(audit["min_z_cm"], 0.0)
            self.assertEqual(audit["pivot_xy_cm"], [0.0, 0.0])

        with self.assertRaisesRegex(RuntimeError, "bounds drifted"):
            self.module.source_geometry_audit(
                specs[-1],
                [value * 1.5 for value in specs[-1]["expected_bounds_cm"]],
                specs[-1]["triangle_count"],
                [0.0, 0.0, specs[-1]["expected_bounds_cm"][2] * 0.5],
            )
        with self.assertRaisesRegex(RuntimeError, "triangle count drifted"):
            self.module.source_geometry_audit(
                specs[-1],
                specs[-1]["expected_bounds_cm"],
                specs[-1]["triangle_count"] + 1,
                [0.0, 0.0, specs[-1]["expected_bounds_cm"][2] * 0.5],
            )
        with self.assertRaisesRegex(RuntimeError, "bottom-center pivot drifted"):
            self.module.source_geometry_audit(
                specs[-1],
                specs[-1]["expected_bounds_cm"],
                specs[-1]["triangle_count"],
                [10.0, 0.0, specs[-1]["expected_bounds_cm"][2] * 0.5 + 5.0],
            )
        self.assertIn("get_num_triangles(0)", self.source)

    def test_asset_paths_are_restricted_to_the_exact_b1_root(self):
        self.assertEqual(
            self.module.require_b1_asset_path(
                "/Game/GameXXK/Environment/TownPCG/B1/Meshes/SM_Test"
            ),
            "/Game/GameXXK/Environment/TownPCG/B1/Meshes/SM_Test",
        )
        for invalid in (
            "/Game/GameXXK/Environment/TownPCG/B0R/SM_Test",
            "/Game/GameXXK/Environment/TownPCG/B1Sibling/SM_Test",
            "/Game/GameXXK/Maps/Dev/L_Qingshan_PCG_Dress_B1",
        ):
            with self.assertRaisesRegex(ValueError, "B1 asset root"):
                self.module.require_b1_asset_path(invalid)

    def test_ue_source_uses_required_authoring_apis_and_never_global_saves(self):
        required_tokens = (
            "AssetImportTask",
            "FbxImportUI",
            "combine_meshes",
            "auto_generate_collision",
            "MaterialExpressionSubstrateToonBSDF",
            "MP_FRONT_MATERIAL",
            "MP_OPACITY_MASK",
            "two_sided",
            "BLEND_MASKED",
            "MaterialInstanceConstantFactoryNew",
            "set_material_instance_vector_parameter_value",
            "SpritePivotMode.CUSTOM",
            "PaperSpriteFactory",
            "PaperFlipbookFactory",
            "PaperFlipbookKeyFrame",
            "duplicate_asset",
            "get_dirty_map_packages",
            "save_asset",
            "imported_object_paths",
            "get_num_triangles",
            "get_material_instance_scalar_parameter_value",
            "get_material_instance_vector_parameter_value",
            "reload_packages",
            "ReloadPackagesInteractionMode.ASSUME_POSITIVE",
        )
        for token in required_tokens:
            self.assertIn(token, self.source)
        self.assertNotIn("save_dirty_packages", self.source)

    def test_variants_rebuild_from_source_and_saved_assets_are_observed_after_reload(self):
        manifest = self.module.load_source_manifest(PROJECT_ROOT)
        variants = self.module.building_variant_specs(manifest)
        self.assertTrue(all(item["rebuild_from_source"] is True for item in variants))
        required_tokens = (
            "delete_asset",
            "post_save_observed_audit",
            "observed_material_instances",
            "observed_sprites",
            "observed_flipbook",
            "observed_texture",
            "observed_building_variants",
        )
        for token in required_tokens:
            self.assertIn(token, self.source)

    def test_texture_sample_uv_connection_uses_ue58_short_pin_name_first(self):
        self.assertEqual(
            self.module.TEXTURE_UV_INPUT_CANDIDATES,
            ("UVs", "Coordinates"),
        )
        self.assertIn(
            'get_class().get_name() == "MaterialExpressionTextureSampleParameter2D"',
            self.source,
        )
        self.assertIn("get_inputs_for_material_expression", self.source)
        self.assertIn("if node is not None", self.source)

    def test_material_graph_rebuild_removes_every_old_expression_one_by_one(self):
        self.assertIn("def _clear_material_expressions", self.source)
        self.assertIn("delete_material_expression(material, expressions[-1])", self.source)
        self.assertEqual(self.source.count("    _clear_material_expressions(material)"), 2)

    def test_result_contract_lists_only_b1_assets_and_all_expected_counts(self):
        manifest = self.module.load_source_manifest(PROJECT_ROOT)
        result = self.module.expected_result_contract(manifest, PROJECT_ROOT)
        self.assertEqual(result["source_fbx_count"], 18)
        self.assertEqual(result["source_building_mesh_count"], 6)
        self.assertEqual(result["source_category_mesh_count"], 12)
        self.assertEqual(result["building_variant_count"], 24)
        self.assertEqual(result["sprite_count"], 4)
        self.assertEqual(result["flipbook_count"], 1)
        self.assertTrue(all(path.startswith(self.module.ASSET_ROOT + "/") for path in result["expected_assets"]))


if __name__ == "__main__":
    unittest.main()
