from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]
AUTHOR = ROOT / "Content" / "Python" / "gamexxk_author_occlusion_cutout_pilot.py"
MATERIAL_MAP = ROOT / "Source" / "GameXXK" / "Private" / "Town" / "GameXXKOcclusionMaterialMap.cpp"


class OcclusionCutoutPilotAssetContractTest(unittest.TestCase):
    def test_authoring_script_creates_project_owned_masked_roof_cutout(self):
        self.assertTrue(AUTHOR.exists(), f"missing authoring script: {AUTHOR}")
        source = AUTHOR.read_text(encoding="utf-8")
        for token in (
            "/Game/GameXXK/Materials/Occlusion/AsianVillage",
            "MPC_PlayerOcclusion",
            "BLEND_MASKED",
            "BLEND_TRANSLUCENT",
            "MP_OPACITY_MASK",
            "MP_OPACITY",
            "MaterialExpressionScreenPosition",
            "OcclusionCenter",
            "OcclusionAspect",
            "OcclusionRadius",
            "recompile_material",
            "save_asset",
            "all_cutout_blends_preserved",
        ):
            self.assertIn(token, source)
        self.assertNotIn('save_asset("/Game/Asian_Village', source)
        self.assertIn("SM_thatched_roof_10", source)
        self.assertIn("SM_thatched_roof_12", source)

    def test_authoring_script_does_not_reinitialize_an_existing_collection(self):
        """Existing MPC parameter GUIDs are part of material-expression bindings.

        Replacing the parameter arrays on every authoring pass gives every
        scalar/vector a new GUID, invalidating already-authored Cutout and
        full-family materials.  The script must only author defaults when it
        creates the collection for the first time.
        """
        source = AUTHOR.read_text(encoding="utf-8")
        self.assertRegex(
            source,
            re.compile(
                r"collection = unreal\.EditorAssetLibrary\.load_asset\(MPC_PATH\)"
                r"(?:\n    #.*)*"
                r"\n    if collection is not None:\n        return collection"
            ),
        )

    def test_authoring_script_combines_circle_with_camera_to_hero_depth_gate(self):
        source = AUTHOR.read_text(encoding="utf-8")
        for token in (
            "OcclusionCameraLocation",
            "OcclusionCameraForward",
            "OcclusionHeroViewDepth",
            "OcclusionDepthBias",
            "MaterialExpressionWorldPosition",
            "MaterialExpressionDotProduct",
            "MaterialExpressionIf",
            "MaterialExpressionOneMinus",
            "A > B",
            "A == B",
            "A < B",
        ):
            self.assertIn(token, source)

    def test_authoring_script_keeps_the_visual_halo_circular(self):
        source = AUTHOR.read_text(encoding="utf-8")
        for token in ("OcclusionFootY", "foot_gate", "foot_padding", "screen_g"):
            self.assertNotIn(token, source)

    def test_authoring_script_keeps_masked_surface_material_scene_texture_free(self):
        """Masked surface materials cannot sample CustomStencil in UE 5.8.

        The legacy fallback must retain its circular camera-to-hero depth gate,
        but leave silhouette-only refinement to a future post-process path.
        Otherwise UE substitutes the default grey material at runtime.
        """
        source = AUTHOR.read_text(encoding="utf-8")
        for token in (
            "PPI_CUSTOM_STENCIL",
            "MaterialExpressionSceneTexture",
            "hero_silhouette_gate",
            "gated_reveal_on_hero",
        ):
            self.assertNotIn(token, source)
        for token in ("OcclusionCameraLocation", "OcclusionHeroViewDepth", "gated_reveal"):
            self.assertIn(token, source)

    def test_authoring_script_multiplies_existing_mask_before_depth_gate(self):
        source = AUTHOR.read_text(encoding="utf-8")
        for token in (
            "_existing_property_connection",
            "source_blend",
            "preserve_existing",
            "original_node",
            "MaterialExpressionMultiply",
        ):
            self.assertIn(token, source)

    def test_authoring_and_runtime_mapping_cover_multistory_building_slots(self):
        source = AUTHOR.read_text(encoding="utf-8")
        for material_name in (
            "MI_concrete_03_Inst",
            "MI_wooden_board_02_Inst",
            "MI_wallpaper_01_Inst",
            "MI_building_concrete_WPO_01_Inst",
            "MI_wooden_board_WPO_01_Inst",
            "MI_wooden_board_WPO_02_Inst",
            "MI_wooden_board_WPO_03_Inst",
            "MI_stone_tiles_01_Inst",
            "MI_glass_01_Inst",
            "MI_wood_02_Inst",
            "MI_building_metal_03_Inst",
            "MI_roof_02_Inst",
        ):
            self.assertIn(material_name, source)

        material_map_source = MATERIAL_MAP.read_text(encoding="utf-8")
        self.assertIn("OriginalMaterial->GetName()", material_map_source)
        self.assertIn("Materials/Occlusion/AsianVillage", material_map_source)


if __name__ == "__main__":
    unittest.main()
