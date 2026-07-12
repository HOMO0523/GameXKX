from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
AUTHOR = ROOT / "Content" / "Python" / "gamexxk_author_occlusion_cutout_pilot.py"


class OcclusionCutoutPilotAssetContractTest(unittest.TestCase):
    def test_authoring_script_creates_project_owned_masked_roof_cutout(self):
        self.assertTrue(AUTHOR.exists(), f"missing authoring script: {AUTHOR}")
        source = AUTHOR.read_text(encoding="utf-8")
        for token in (
            "/Game/GameXXK/Materials/Occlusion/AsianVillage",
            "MPC_PlayerOcclusion",
            "BLEND_MASKED",
            "MP_OPACITY_MASK",
            "MaterialExpressionScreenPosition",
            "OcclusionCenter",
            "OcclusionAspect",
            "OcclusionRadius",
            "recompile_material",
            "save_asset",
        ):
            self.assertIn(token, source)
        self.assertNotIn('save_asset("/Game/Asian_Village', source)
        self.assertIn("SM_thatched_roof_10", source)
        self.assertIn("SM_thatched_roof_12", source)


if __name__ == "__main__":
    unittest.main()
