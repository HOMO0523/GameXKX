#!/usr/bin/env python3
"""Regression contract for PartyDeck card portraits and isolated UI import."""

from __future__ import annotations

import importlib.util
import json
import sys
import tempfile
import unittest
from pathlib import Path

from PIL import Image


PROJECT_ROOT = Path(__file__).resolve().parents[1]
PIPELINE_PATH = PROJECT_ROOT / "Content" / "Python" / "gamexxk_import_party_deck_card_portraits.py"
IMPORT_ROOT = "/Game/GameXXK/UI/PartyDeck/CardArt"
ROUTE_MANIFEST_PATH = (
    PROJECT_ROOT / "SourceAssets" / "PartyDeck" / "card-portraits" / "route-card-art-manifest.json"
)


def _load_pipeline():
    spec = importlib.util.spec_from_file_location("gamexxk_import_party_deck_card_portraits", PIPELINE_PATH)
    if spec is None or spec.loader is None:
        raise AssertionError(f"PartyDeck portrait pipeline is missing: {PIPELINE_PATH}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


class PartyDeckCardPortraitPipelineTests(unittest.TestCase):
    def test_exact_owner_portrait_plan_is_isolated_and_uses_original_identity_sources(self) -> None:
        pipeline = _load_pipeline()
        plan = pipeline.validate_portrait_plan()

        self.assertTrue(plan["ok"])
        self.assertEqual(plan["portrait_count"], 17)
        self.assertEqual(plan["destination_root"], IMPORT_ROOT)
        records = plan["records"]
        self.assertEqual({record["key"] for record in records if record["key"].startswith("Role.")}, {
            "Role.Blade", "Role.Guard", "Role.Healer", "Role.Hunter", "Role.Sorcerer", "Role.FormationMaster",
        })
        self.assertEqual({record["key"] for record in records if record["key"].startswith("Npc.")}, {
            "Npc.TusiChief", "Npc.SongJinBao", "Npc.YueBai", "Npc.ZhouGuangZu", "Npc.JinGui", "Npc.QiongMeiEr",
        })
        self.assertIn("Hero", {record["key"] for record in records})
        route_records = [record for record in records if record["key"].startswith("Route.")]
        self.assertEqual({record["key"] for record in route_records}, {
            "Route.General", "Route.Terrain", "Route.Rare", "Route.Boss",
        })
        self.assertTrue(all(record["source_mode"] == "generated_alpha" for record in route_records))
        self.assertEqual({record["asset_name"] for record in route_records}, {
            "T_CardPortrait_Route_General",
            "T_CardPortrait_Route_Terrain",
            "T_CardPortrait_Route_Rare",
            "T_CardPortrait_Route_Boss",
        })
        for record in records:
            self.assertTrue(record["source_path"].is_file())
            self.assertTrue(record["asset_path"].startswith(f"{IMPORT_ROOT}/T_CardPortrait_"))
            self.assertEqual(tuple(record["portrait_size"]), (171, 205))
        for record in records:
            if record["key"].startswith("Role."):
                self.assertEqual(record["source_mode"], "role_south_cell")
            elif record["key"].startswith("Npc."):
                self.assertIn(record["source_mode"], {"original_alpha", "original_opaque"})
        for record in route_records:
            self.assertEqual(record["source_path"].parent, PROJECT_ROOT / "SourceAssets" / "PartyDeck" / "card-portraits" / "route-alpha")
            self.assertEqual(record["derived_path"].parent, PROJECT_ROOT / "SourceAssets" / "PartyDeck" / "card-portraits" / "generated")
            self.assertNotEqual(record["source_path"], record["derived_path"])

    def test_preparation_makes_exact_card_portraits_without_writing_to_sources(self) -> None:
        pipeline = _load_pipeline()
        original_hashes = {record.key: pipeline._sha256(record.source) for record in pipeline.PORTRAITS}
        manifest = json.loads(ROUTE_MANIFEST_PATH.read_text(encoding="utf-8"))
        raw_route_hashes = {
            record["key"]: pipeline._sha256(ROUTE_MANIFEST_PATH.parent / record["raw_chroma_source"])
            for record in manifest["records"]
        }
        with tempfile.TemporaryDirectory() as raw_destination:
            result = pipeline.prepare_portrait_sources(Path(raw_destination))
            self.assertEqual(result["prepared_count"], 17)
            for portrait in result["prepared"]:
                path = Path(portrait)
                self.assertTrue(path.is_file())
                with Image.open(path) as image:
                    self.assertEqual(image.size, (171, 205))
                    self.assertEqual(image.mode, "RGBA")
            for record in pipeline.PORTRAITS:
                if not record.key.startswith("Route."):
                    continue
                with Image.open(Path(raw_destination) / record.derived_name) as image:
                    self.assertEqual(image.getchannel("A").getpixel((0, 0)), 0)
                    self.assertIsNotNone(image.getchannel("A").getbbox())
        self.assertEqual(original_hashes, {record.key: pipeline._sha256(record.source) for record in pipeline.PORTRAITS})
        self.assertEqual(raw_route_hashes, {
            record["key"]: pipeline._sha256(ROUTE_MANIFEST_PATH.parent / record["raw_chroma_source"])
            for record in manifest["records"]
        })

    def test_import_contract_never_replaces_or_deletes_portraits(self) -> None:
        source = PIPELINE_PATH.read_text(encoding="utf-8")
        self.assertIn("task.replace_existing = False", source)
        self.assertNotIn("delete_asset(", source)
        self.assertNotIn("delete_directory(", source)
        self.assertIn("TextureMipGenSettings.TMGS_NO_MIPMAPS", source)
        self.assertIn("TextureGroup.TEXTUREGROUP_UI", source)

    def test_route_source_manifest_locks_chroma_and_alpha_provenance(self) -> None:
        self.assertTrue(ROUTE_MANIFEST_PATH.is_file())
        manifest = json.loads(ROUTE_MANIFEST_PATH.read_text(encoding="utf-8"))
        self.assertEqual(manifest["schema_version"], 1)
        self.assertEqual(manifest["generation_mode"], "built_in_imagegen_chroma_key")
        records = manifest["records"]
        self.assertEqual({record["key"] for record in records}, {
            "Route.General", "Route.Terrain", "Route.Rare", "Route.Boss",
        })
        for record in records:
            self.assertTrue((ROUTE_MANIFEST_PATH.parent / record["raw_chroma_source"]).is_file())
            self.assertTrue((ROUTE_MANIFEST_PATH.parent / record["alpha_source"]).is_file())
            self.assertEqual(record["source_size"], [1024, 1536])
            self.assertEqual(record["alpha_source_size"], [1024, 1536])
            self.assertEqual(len(record["raw_chroma_sha256"]), 64)
            self.assertEqual(len(record["alpha_sha256"]), 64)


if __name__ == "__main__":
    unittest.main()
