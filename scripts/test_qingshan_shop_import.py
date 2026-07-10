from __future__ import annotations

import importlib.util
import hashlib
import json
import sys
import tempfile
import types
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SCRIPT_PATH = PROJECT_ROOT / "Content" / "Python" / "gamexxk_import_qingshan_shop_toon.py"
SOURCE_ROOT = PROJECT_ROOT / "SourceAssets" / "TownPCG" / "QingshanShopA"
SOURCE_ARCHIVE = SOURCE_ROOT / "SM_Qingshan_Shop_A_HQ_Retop50K_Quad_Textured.zip"
SOURCE_MANIFEST = SOURCE_ROOT / "source_manifest.json"


def _load_module():
    sys.modules.setdefault("unreal", types.SimpleNamespace())
    spec = importlib.util.spec_from_file_location("gamexxk_import_qingshan_shop_toon", SCRIPT_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Could not load module spec for {SCRIPT_PATH}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class QingshanShopImportScriptTests(unittest.TestCase):
    def test_simple_collision_helper_is_idempotent_and_fail_closed(self):
        module = _load_module()

        class FakeMesh:
            def __init__(self):
                self.modified = 0

            def modify(self):
                self.modified += 1

        class FakeLibrary:
            def __init__(self, counts):
                self.counts = list(counts)
                self.add_calls = 0

            def get_simple_collision_count(self, mesh):
                return self.counts.pop(0)

            def add_simple_collisions(self, mesh, shape):
                self.add_calls += 1

        original_unreal = module.unreal
        try:
            for counts, expected_adds in (([0, 1], 1), ([2, 2], 0)):
                library = FakeLibrary(counts)
                module.unreal = types.SimpleNamespace(
                    EditorStaticMeshLibrary=library,
                    ScriptingCollisionShapeType=types.SimpleNamespace(BOX="BOX"),
                )
                self.assertGreaterEqual(module._ensure_simple_collision(FakeMesh()), 1)
                self.assertEqual(library.add_calls, expected_adds)
            library = FakeLibrary([0, 0])
            module.unreal = types.SimpleNamespace(
                EditorStaticMeshLibrary=library,
                ScriptingCollisionShapeType=types.SimpleNamespace(BOX="BOX"),
            )
            with self.assertRaisesRegex(RuntimeError, "at least one simple collision"):
                module._ensure_simple_collision(FakeMesh())
        finally:
            module.unreal = original_unreal

    def test_tracked_source_archive_matches_reproduction_manifest(self):
        self.assertTrue(SOURCE_ARCHIVE.is_file())
        manifest = json.loads(SOURCE_MANIFEST.read_text(encoding="utf-8"))
        self.assertEqual(manifest["source_archive"], SOURCE_ARCHIVE.relative_to(PROJECT_ROOT).as_posix())
        self.assertEqual(
            manifest["sha256"],
            hashlib.sha256(SOURCE_ARCHIVE.read_bytes()).hexdigest(),
        )
        self.assertEqual(manifest["external_origin"], "C:/Users/shxuw/Downloads/SM_Qingshan_Shop_A_HQ_Retop50K_Quad_Textured.zip")
        self.assertEqual(manifest["topology"], "Quad")
        self.assertEqual(manifest["quad_count"], 51110)
        self.assertEqual(manifest["import_uniform_scale"], 6.5)
        self.assertEqual(manifest["texture_resolution"], [4096, 4096])
        self.assertEqual(manifest["texture_count"], 1)

    def test_script_exposes_stable_asset_contract_and_validates_sources(self):
        self.assertTrue(SCRIPT_PATH.exists(), "Qingshan shop import script must exist")
        module = _load_module()
        self.assertEqual(
            module.ASSET_ROOT,
            "/Game/GameXXK/Environment/TownPCG/Prototype/QingshanShopA",
        )
        self.assertEqual(module.MESH_NAME, "SM_Qingshan_Shop_A_HQ_Retop50K")
        self.assertEqual(module.TEXTURE_NAME, "T_Qingshan_Shop_A_Albedo")
        self.assertEqual(module.MATERIAL_NAME, "M_Qingshan_Building_Toon")
        self.assertEqual(module.TOON_PROFILE_NAME, "TP_Qingshan_Building_Toon")
        self.assertEqual(getattr(module, "IMPORT_UNIFORM_SCALE", None), 6.5)
        self.assertTrue(callable(getattr(module, "_ensure_simple_collision", None)))

        source = SCRIPT_PATH.read_text(encoding="utf-8")
        self.assertIn("get_simple_collision_count", source)
        self.assertIn("add_simple_collisions", source)
        self.assertIn("ScriptingCollisionShapeType.BOX", source)
        self.assertIn("if existing_count == 0", source)
        collision_index = source.index("_ensure_simple_collision(static_mesh)")
        save_index = source.index("_save_assets((texture, toon_profile, material, static_mesh))")
        self.assertLess(collision_index, save_index)

        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            fbx = root / "shop.fbx"
            albedo = root / "shop.jpg"
            fbx.write_bytes(b"fbx")
            albedo.write_bytes(b"jpg")
            args = module._parse_args(["--fbx", str(fbx), "--albedo", str(albedo)])
            resolved_fbx, resolved_albedo = module._require_sources(args)
            self.assertEqual(resolved_fbx, fbx.resolve())
            self.assertEqual(resolved_albedo, albedo.resolve())

            albedo.unlink()
            with self.assertRaisesRegex(RuntimeError, "Missing albedo source"):
                module._require_sources(args)


if __name__ == "__main__":
    unittest.main()
