from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[1]
AUTHOR_SCRIPT = PROJECT_ROOT / "Content" / "Python" / "gamexxk_author_player_occlusion_reveal.py"


class PlayerOcclusionRevealAssetContractTest(unittest.TestCase):
    def test_authoring_script_defines_true_scene_post_process_reveal(self):
        source = AUTHOR_SCRIPT.read_text(encoding="utf-8")
        for required in (
            "/Game/GameXXK/Materials/Player/M_PP_PlayerOcclusionReveal",
            "MD_POST_PROCESS",
            "PPI_POST_PROCESS_INPUT0",
            "RevealSceneTexture",
            "RevealCenter",
            "RevealRadius",
            "RevealFeather",
            "RevealActive",
            "MaterialExpressionSceneTexture",
            "MaterialExpressionScreenPosition",
            "MaterialExpressionTextureSampleParameter2D",
            "MaterialExpressionLinearInterpolate",
            "recompile_material",
            "save_asset",
        ):
            self.assertIn(required, source)
        self.assertNotIn("disable_depth_test", source)
        self.assertNotIn("SpriteTexture", source)


if __name__ == "__main__":
    unittest.main()
