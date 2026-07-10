from __future__ import annotations

import copy
import hashlib
import json
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT_ROOT / "scripts"))

from qingshan_environment_assets import (  # noqa: E402
    BATCH_CALL_CAPS,
    BATCH_COUNTS,
    EXPECTED_VIEWS,
    CatalogError,
    register_output,
    validate_asset,
    validate_catalog,
    write_batch_report,
)


def valid_building() -> dict:
    return {
        "schema_version": 1,
        "asset_id": "BLD_QS_M_A_INN",
        "display_name": "青山镇中型客栈 A",
        "category": "building",
        "batch": "B1",
        "priority": "P0",
        "status": "concept_planned",
        "style_profile": "QS_InkToon_v1",
        "intended_zone": ["town_interior"],
        "gameplay_role": "street_node",
        "target_dimensions_m": [4.8, 5.6, 5.2],
        "pivot": "bottom_center",
        "forward_axis": "+Y",
        "palette": ["warm_off_white", "warm_red_brown", "dark_teal_ink"],
        "silhouette_keywords": ["two_storey", "asymmetric_xieshan_roof"],
        "material_language": ["ink_toon", "warm_plaster", "dark_timber"],
        "negative_prompt": ["photorealistic", "modern_signage", "mirror_symmetry"],
        "dependencies": ["REF_QS_ENV_STYLE_LOCK", "REF_QS_SCALE_LINEUP"],
        "source_provenance": {"source_kind": "project_concept"},
        "reference_images": [
            {
                "kind": "hero_3q",
                "approval_state": "planned",
                "file_stub": "BLD_QS_M_A_INN__hero_3q",
            },
            {
                "kind": "structure_sheet",
                "approval_state": "planned",
                "file_stub": "BLD_QS_M_A_INN__structure_sheet",
            },
        ],
        "generation": {
            "use_case": "stylized-concept",
            "asset_type": "game_environment_asset_reference",
            "required_view_kinds": ["hero_3q", "structure_sheet"],
            "max_versions_per_view": 3,
            "max_generation_calls": 6,
            "generation_calls_used": 0,
            "blocked_after_revisions": 2,
        },
        "unreal": {"asset_class": "StaticMesh", "nanite": True, "material_slots_max": 2},
        "pcg": {
            "allowed_zones": ["town_interior"],
            "excluded_zones": ["road", "river", "interaction"],
        },
        "workflow_gates": {
            "style_locked": False,
            "reference_approved": False,
            "concept_generation_allowed": False,
            "batch_unlocked": False,
            "previous_batch_approved": False,
            "batch_approval_id": None,
            "model_or_sprite_production_allowed": False,
            "tripo_allowed": False,
            "ue_import_allowed": False,
        },
        "acceptance": {"silhouette_unique": False, "player_camera_readability": False},
    }


def _write_json(path: Path, data: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(data, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


def _asset_for(category: str, asset_id: str, batch: str) -> dict:
    data = valid_building()
    data["asset_id"] = asset_id
    data["display_name"] = asset_id
    data["category"] = category
    data["batch"] = batch
    views = list(EXPECTED_VIEWS[category])
    data["generation"]["required_view_kinds"] = views
    data["reference_images"] = [
        {
            "kind": kind,
            "approval_state": "planned",
            "file_stub": f"{asset_id}__{kind}",
        }
        for kind in views
    ]
    calls = 0 if category == "registry" else 3 if category == "reference" else 9 if category == "paper2d_plant" else 6
    data["generation"]["max_generation_calls"] = calls
    return data


def _write_asset(root: Path, data: dict) -> Path:
    path = root / "assets" / data["asset_id"] / "asset.json"
    _write_json(path, data)
    return path


def _write_manifest(root: Path, asset_ids: list[str], batch_call_caps: dict | None = None) -> None:
    _write_json(
        root / "manifest.json",
        {
            "asset_ids": asset_ids,
            "batch_call_caps": batch_call_caps or BATCH_CALL_CAPS,
        },
    )


def _build_catalog(root: Path) -> list[dict]:
    assets: list[dict] = []
    assets.append(_asset_for("registry", "BLD_QS_REGISTRY_SHOP", "REGISTRY"))
    for index in range(4):
        assets.append(_asset_for("reference", f"REF_QS_BOARD_{index}", "B0"))

    plant_cells = [7, 7, 7, 7, 6, 6]
    for index in range(12):
        if index < len(plant_cells):
            asset = _asset_for("paper2d_plant", f"P2D_QS_PLANT_B1_{index}", "B1")
            asset["paper2d"] = {"atlas_cell_count": plant_cells[index]}
        else:
            asset = _asset_for("building", f"BLD_QS_BUILDING_B1_{index}", "B1")
        assets.append(asset)
    for index in range(13):
        assets.append(_asset_for("surface", f"SRF_QS_SURFACE_B2_{index}", "B2"))
    for index in range(5):
        assets.append(_asset_for("prop_kit", f"PROP_QS_PROP_B3_{index}", "B3"))

    _write_manifest(root, [item["asset_id"] for item in assets])
    for asset in assets:
        _write_asset(root, asset)
    return assets


class AssetValidationTests(unittest.TestCase):
    def test_reference_provenance(self):
        references_root = (
            PROJECT_ROOT
            / "SourceAssets"
            / "TownPCG"
            / "QingshanEnvironment"
            / "style"
            / "references"
        )
        expected = [
            {
                "destination": "style_env_day.jpeg",
                "source_locator": "<TENCENT_ORIGINALS>/74a507717280d3a9e8f075e8a3d8201b.jpeg",
                "role": "environment_style",
                "bytes": 160447,
                "sha256": "bdc4c4e76938dfc416b37ad5d32c710d56604cf344a1346189b152b245bb15bf",
            },
            {
                "destination": "style_env_night.jpeg",
                "source_locator": "<TENCENT_ORIGINALS>/6cb6304fa60ec4084affc48be722555e.jpeg",
                "role": "environment_atmosphere",
                "bytes": 164803,
                "sha256": "9947131a990cfcae6def4f82d26bd392b9900b042e40cd0f9b7b5cae05bde57b",
            },
            {
                "destination": "style_character_scale.jpeg",
                "source_locator": "<TENCENT_ORIGINALS>/b3e9c4c5f18ab38abfa6f49336b623c0.jpeg",
                "role": "character_scale",
                "bytes": 24557,
                "sha256": "ed509274c438f2d7a81f5ec7cb35e9186dba730f33299f158efdf0b40f5c537d",
            },
            {
                "destination": "style_nature_group.jpeg",
                "source_locator": "<TENCENT_ORIGINALS>/b470ad572a2d2eb3959754c13d6cf0f1.jpeg",
                "role": "shape_language",
                "bytes": 48689,
                "sha256": "c6aa7189710f32c59c4842529a9ff47601698e2666cf5a0683dd1b74a6878b3b",
            },
            {
                "destination": "style_creature_warm.jpeg",
                "source_locator": "<TENCENT_ORIGINALS>/d848c66bef90df2589b2b81c7575cd22.jpeg",
                "role": "warm_palette",
                "bytes": 73115,
                "sha256": "c371fc141a614afc54789c0cb65c1603106ed02c9ff374d5a0c18de3b51ef0d9",
            },
            {
                "destination": "style_creature_mass.jpeg",
                "source_locator": "<TENCENT_ORIGINALS>/55bc66504b926d1917317b93c0a42f6b.jpeg",
                "role": "mass_and_outline",
                "bytes": 40962,
                "sha256": "0b38e1901489220b3d82bb6aff983ec2fbb49bac8a29ca7c0db050ce16df6a7d",
            },
            {
                "destination": "layout_dense_foliage.jpg",
                "source_locator": "<WECHAT_TEMP>/ed2a4aca8dd0aac3628ef65c3b405348.jpg",
                "role": "layout_density_only",
                "bytes": 229157,
                "sha256": "e29852562f1002ac6f8436cd260bf7fc1f8e3d64f663f9004279a75c8d73ec93",
            },
        ]

        for item in expected:
            destination = references_root / item["destination"]
            self.assertTrue(destination.is_file(), f"missing reference: {destination}")
            self.assertEqual(destination.stat().st_size, item["bytes"])
            self.assertEqual(hashlib.sha256(destination.read_bytes()).hexdigest(), item["sha256"])

        provenance_path = references_root / "provenance.json"
        self.assertTrue(provenance_path.is_file(), f"missing provenance: {provenance_path}")
        provenance_text = provenance_path.read_text(encoding="utf-8")
        provenance = json.loads(provenance_text)
        self.assertEqual(provenance, {"references": expected})
        self.assertNotIn("source_absolute_path", provenance_text)
        self.assertIsNone(re.search(r"(?i)\b[a-z]:[\\/]+users[\\/]", provenance_text))
        for item in provenance["references"]:
            self.assertRegex(
                item["source_locator"],
                r"^<(?:TENCENT_ORIGINALS|WECHAT_TEMP)>/[0-9a-f]+\.(?:jpeg|jpg)$",
            )
        layout_only = [
            item["destination"]
            for item in provenance["references"]
            if item["role"] == "layout_density_only"
        ]
        self.assertEqual(layout_only, ["layout_dense_foliage.jpg"])

        source_documents = [
            PROJECT_ROOT
            / "docs"
            / "superpowers"
            / "specs"
            / "2026-07-10-qingshan-environment-concept-json-production-design.md",
            PROJECT_ROOT
            / "docs"
            / "superpowers"
            / "plans"
            / "2026-07-10-qingshan-environment-concept-batch0.md",
        ]
        for document in source_documents:
            document_text = document.read_text(encoding="utf-8")
            self.assertIsNone(
                re.search(r"(?i)\b[a-z]:[\\/]+users[\\/]", document_text),
                f"local user path leaked in {document}",
            )
            for item in expected:
                self.assertIn(item["source_locator"], document_text)
            for obsolete_instruction in ("original absolute path", "记录原路径"):
                self.assertNotIn(obsolete_instruction, document_text)

    def test_valid_building_passes(self):
        validate_asset(valid_building())

    def test_empty_value_is_rejected(self):
        data = valid_building()
        data["palette"] = []
        with self.assertRaisesRegex(CatalogError, "empty"):
            validate_asset(data)

    def test_nested_empty_value_is_rejected(self):
        data = valid_building()
        data["source_provenance"]["source_kind"] = ""
        with self.assertRaisesRegex(CatalogError, "empty"):
            validate_asset(data)

    def test_missing_top_level_field_is_rejected(self):
        data = valid_building()
        del data["acceptance"]
        with self.assertRaisesRegex(CatalogError, "missing fields"):
            validate_asset(data)

    def test_required_views_are_exact(self):
        data = valid_building()
        data["generation"]["required_view_kinds"].append("random_extra")
        with self.assertRaisesRegex(CatalogError, "required_view_kinds"):
            validate_asset(data)

    def test_reference_image_views_are_exact(self):
        data = valid_building()
        data["reference_images"][1]["kind"] = "random_extra"
        with self.assertRaisesRegex(CatalogError, "reference_images"):
            validate_asset(data)

    def test_v004_budget_is_impossible(self):
        data = valid_building()
        data["generation"]["max_versions_per_view"] = 4
        with self.assertRaisesRegex(CatalogError, "max_versions_per_view"):
            validate_asset(data)

    def test_invalid_asset_id_is_rejected(self):
        data = valid_building()
        data["asset_id"] = "building with spaces"
        with self.assertRaisesRegex(CatalogError, "asset_id"):
            validate_asset(data)

    def test_unknown_category_is_rejected(self):
        data = valid_building()
        data["category"] = "boat"
        with self.assertRaisesRegex(CatalogError, "category"):
            validate_asset(data)

    def test_unknown_batch_is_rejected(self):
        data = valid_building()
        data["batch"] = "B4"
        with self.assertRaisesRegex(CatalogError, "batch"):
            validate_asset(data)

    def test_asset_call_budget_is_enforced(self):
        data = valid_building()
        data["generation"]["generation_calls_used"] = 7
        with self.assertRaisesRegex(CatalogError, "budget"):
            validate_asset(data)

    def test_production_gates_must_remain_false(self):
        for gate in ("tripo_allowed", "model_or_sprite_production_allowed", "ue_import_allowed"):
            with self.subTest(gate=gate):
                data = valid_building()
                data["workflow_gates"][gate] = True
                with self.assertRaisesRegex(CatalogError, "production gates"):
                    validate_asset(data)


class SchemaParityValidationTests(unittest.TestCase):
    def test_unknown_top_level_property_is_rejected(self):
        data = valid_building()
        data["unreviewed_extension"] = "not approved by the schema"

        with self.assertRaisesRegex(CatalogError, "unknown fields"):
            validate_asset(data)

    def test_unknown_generation_property_is_rejected(self):
        data = valid_building()
        data["generation"]["random_seed_pool"] = 99

        with self.assertRaisesRegex(CatalogError, "generation.*unknown fields"):
            validate_asset(data)

    def test_unknown_workflow_gate_property_is_rejected(self):
        data = valid_building()
        data["workflow_gates"]["skip_user_review"] = True

        with self.assertRaisesRegex(CatalogError, "workflow_gates.*unknown fields"):
            validate_asset(data)

    def test_unknown_reference_image_property_is_rejected(self):
        data = valid_building()
        data["reference_images"][0]["manual_hash_override"] = "forbidden"

        with self.assertRaisesRegex(CatalogError, "reference_images.*unknown fields"):
            validate_asset(data)

    def test_schema_object_types_raise_clean_catalog_errors(self):
        cases = {
            "source_provenance": 123,
            "paper2d": ["not", "an", "object"],
            "unreal": ["not", "an", "object"],
            "pcg": "not an object",
            "acceptance": [True],
        }
        for field, malformed in cases.items():
            with self.subTest(field=field):
                data = valid_building()
                data[field] = malformed
                with self.assertRaisesRegex(CatalogError, field):
                    validate_asset(data)

    def test_schema_scalar_and_list_types_are_enforced(self):
        cases = {
            "display_name": 123,
            "intended_zone": "town_interior",
            "target_dimensions_m": "large",
            "palette": "jade_green",
        }
        for field, malformed in cases.items():
            with self.subTest(field=field):
                data = valid_building()
                data[field] = malformed
                with self.assertRaisesRegex(CatalogError, field):
                    validate_asset(data)

    def test_generation_constants_are_enforced(self):
        cases = {
            "use_case": "photoreal-product-shot",
            "blocked_after_revisions": 3,
        }
        for field, malformed in cases.items():
            with self.subTest(field=field):
                data = valid_building()
                data["generation"][field] = malformed
                with self.assertRaisesRegex(CatalogError, field):
                    validate_asset(data)

    def test_workflow_gate_types_are_enforced(self):
        cases = {
            "style_locked": "false",
            "batch_approval_id": 123,
        }
        for field, malformed in cases.items():
            with self.subTest(field=field):
                data = valid_building()
                data["workflow_gates"][field] = malformed
                with self.assertRaisesRegex(CatalogError, field):
                    validate_asset(data)

    def test_nested_atlas_cell_count_is_a_strict_positive_integer(self):
        for malformed in ("7", 0, True):
            with self.subTest(malformed=malformed):
                data = _asset_for("paper2d_plant", "P2D_QS_BAD_ATLAS", "B1")
                data["paper2d"] = {"atlas_cell_count": malformed}
                with self.assertRaisesRegex(CatalogError, "paper2d.atlas_cell_count"):
                    validate_asset(data)


class CatalogValidationTests(unittest.TestCase):
    def test_valid_catalog_returns_summary(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            _build_catalog(root)

            summary = validate_catalog(root)

            self.assertEqual(
                summary,
                {
                    "ok": True,
                    "asset_count": 35,
                    "batch_counts": BATCH_COUNTS,
                    "plant_cells": 40,
                },
            )

    def test_catalog_requires_35_unique_ids(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            _build_catalog(root)
            manifest_path = root / "manifest.json"
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            manifest["asset_ids"][-1] = manifest["asset_ids"][0]
            _write_json(manifest_path, manifest)

            with self.assertRaisesRegex(CatalogError, "35 unique"):
                validate_catalog(root)

    def test_manifest_traversal_id_is_rejected_before_asset_read(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            _build_catalog(root)
            manifest_path = root / "manifest.json"
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            manifest["asset_ids"][0] = "../../outside"
            _write_json(manifest_path, manifest)
            outside = root / "outside" / "asset.json"
            outside.parent.mkdir()
            outside.write_text("this is deliberately invalid JSON", encoding="utf-8")

            with self.assertRaisesRegex(CatalogError, "manifest asset_id"):
                validate_catalog(root)

    def test_asset_symlink_escape_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            assets = _build_catalog(root)
            escaped = assets[0]
            asset_dir = root / "assets" / escaped["asset_id"]
            outside_dir = root / "outside-asset"
            _write_json(outside_dir / "asset.json", escaped)
            (asset_dir / "asset.json").unlink()
            asset_dir.rmdir()
            try:
                asset_dir.symlink_to(outside_dir, target_is_directory=True)
            except OSError as exc:
                if os.name != "nt":
                    self.skipTest(f"platform cannot create a test symlink: {exc}")
                junction = subprocess.run(
                    ["cmd", "/c", "mklink", "/J", str(asset_dir), str(outside_dir)],
                    capture_output=True,
                    text=True,
                    check=False,
                )
                if junction.returncode != 0:
                    self.skipTest(f"platform cannot create a test junction: {junction.stderr}")

            try:
                with self.assertRaisesRegex(CatalogError, "escapes assets root"):
                    validate_catalog(root)
            finally:
                if asset_dir.is_symlink():
                    asset_dir.unlink()
                elif asset_dir.exists():
                    os.rmdir(asset_dir)

    def test_catalog_enforces_exact_batch_counts(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            assets = _build_catalog(root)
            assets[-1]["batch"] = "B2"
            _write_asset(root, assets[-1])

            with self.assertRaisesRegex(CatalogError, "batch counts"):
                validate_catalog(root)

    def test_catalog_enforces_plant_atlas_total(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            assets = _build_catalog(root)
            plant = next(item for item in assets if item["category"] == "paper2d_plant")
            plant["paper2d"]["atlas_cell_count"] -= 1
            _write_asset(root, plant)

            with self.assertRaisesRegex(CatalogError, "total 40"):
                validate_catalog(root)

    def test_catalog_reports_malformed_plant_atlas_as_catalog_error(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            assets = _build_catalog(root)
            plant = next(item for item in assets if item["category"] == "paper2d_plant")
            plant["paper2d"]["atlas_cell_count"] = "seven"
            _write_asset(root, plant)

            with self.assertRaisesRegex(CatalogError, "paper2d.atlas_cell_count"):
                validate_catalog(root)

    def test_catalog_enforces_batch_call_cap(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            assets = _build_catalog(root)
            b1_assets = [item for item in assets if item["batch"] == "B1"]
            calls = [7, 7, 7, 7, 7, 7, 6, 1]
            for asset, used in zip(b1_assets, calls):
                asset["generation"]["generation_calls_used"] = used
                _write_asset(root, asset)

            with self.assertRaisesRegex(CatalogError, "B1 call cap"):
                validate_catalog(root)

    def test_catalog_rejects_wrong_per_asset_maximum(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            assets = _build_catalog(root)
            board = next(item for item in assets if item["batch"] == "B0")
            board["generation"]["max_generation_calls"] = 999
            _write_asset(root, board)

            with self.assertRaisesRegex(CatalogError, "max_generation_calls"):
                validate_catalog(root)

    def test_catalog_rejects_manifest_batch_cap_drift(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            assets = _build_catalog(root)
            drifted = dict(BATCH_CALL_CAPS)
            drifted["B0"] = 999
            _write_manifest(root, [item["asset_id"] for item in assets], drifted)

            with self.assertRaisesRegex(CatalogError, "batch_call_caps"):
                validate_catalog(root)

    def test_theoretical_asset_maxima_may_exceed_batch_cap(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            assets = _build_catalog(root)
            b1_maximum = sum(
                item["generation"]["max_generation_calls"]
                for item in assets
                if item["batch"] == "B1"
            )
            self.assertGreater(b1_maximum, BATCH_CALL_CAPS["B1"])

            self.assertTrue(validate_catalog(root)["ok"])


class OutputRegistrationTests(unittest.TestCase):
    def _root_with_asset(self, directory: str, asset: dict | None = None) -> tuple[Path, dict]:
        root = Path(directory)
        data = asset or valid_building()
        _write_asset(root, data)
        _write_manifest(root, [data["asset_id"]])
        return root, data

    def test_v004_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root, data = self._root_with_asset(directory)
            output = root / "concept.png"
            output.write_bytes(b"concept")

            with self.assertRaisesRegex(CatalogError, "v001"):
                register_output(root, data["asset_id"], "hero_3q", "v004", output)

    def test_registration_hashes_file_increments_budget_and_updates_only_matching_view(self):
        with tempfile.TemporaryDirectory() as directory:
            root, data = self._root_with_asset(directory)
            output = root / "concepts" / "inn.png"
            output.parent.mkdir()
            payload = b"deterministic image bytes"
            output.write_bytes(payload)
            untouched = copy.deepcopy(data["reference_images"][1])

            result = register_output(root, data["asset_id"], "hero_3q", "v001", output)

            stored = json.loads(
                (root / "assets" / data["asset_id"] / "asset.json").read_text(encoding="utf-8")
            )
            expected_hash = hashlib.sha256(payload).hexdigest()
            self.assertEqual(result["sha256"], expected_hash)
            self.assertEqual(result["generation_calls_used"], 1)
            self.assertEqual(stored["generation"]["generation_calls_used"], 1)
            self.assertEqual(stored["reference_images"][1], untouched)
            self.assertEqual(
                stored["reference_images"][0],
                {
                    "kind": "hero_3q",
                    "approval_state": "generated_pending_review",
                    "file_stub": "BLD_QS_M_A_INN__hero_3q",
                    "output_path": "concepts/inn.png",
                    "sha256": expected_hash,
                    "version": "v001",
                },
            )
            raw = (root / "assets" / data["asset_id"] / "asset.json").read_text(encoding="utf-8")
            self.assertTrue(raw.endswith("\n"))
            self.assertEqual(raw, json.dumps(stored, ensure_ascii=False, indent=2, sort_keys=True) + "\n")

    def test_unrequired_view_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root, data = self._root_with_asset(directory)
            output = root / "concept.png"
            output.write_bytes(b"concept")

            with self.assertRaisesRegex(CatalogError, "required view"):
                register_output(root, data["asset_id"], "rear", "v001", output)

    def test_exhausted_asset_budget_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            data = valid_building()
            data["generation"]["generation_calls_used"] = 6
            root, data = self._root_with_asset(directory, data)
            output = root / "concept.png"
            output.write_bytes(b"concept")

            with self.assertRaisesRegex(CatalogError, "asset generation budget exhausted"):
                register_output(root, data["asset_id"], "hero_3q", "v001", output)

    def test_exhausted_batch_budget_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            current = valid_building()
            current["generation"]["max_generation_calls"] = 6
            assets = [current]
            for index in range(8):
                other = _asset_for("building", f"BLD_QS_BATCH_CAP_{index}", "B1")
                other["generation"]["generation_calls_used"] = 6
                assets.append(other)
            for asset in assets:
                _write_asset(root, asset)
            _write_manifest(root, [item["asset_id"] for item in assets])
            output = root / "concept.png"
            output.write_bytes(b"concept")

            with self.assertRaisesRegex(CatalogError, "B1 generation budget exhausted"):
                register_output(root, current["asset_id"], "hero_3q", "v001", output)

    def test_stale_assets_outside_manifest_do_not_consume_batch_budget(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            current = valid_building()
            _write_asset(root, current)
            _write_manifest(root, [current["asset_id"]])
            for index in range(8):
                stale = _asset_for("building", f"BLD_QS_STALE_{index}", "B1")
                stale["generation"]["generation_calls_used"] = 6
                _write_asset(root, stale)
            output = root / "concept.png"
            output.write_bytes(b"manifest member output")

            result = register_output(root, current["asset_id"], "hero_3q", "v001", output)

            self.assertEqual(result["generation_calls_used"], 1)

    def test_asset_absent_from_manifest_cannot_be_registered(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            stale = valid_building()
            member = _asset_for("reference", "REF_QS_ONLY_MANIFEST_MEMBER", "B0")
            _write_asset(root, stale)
            _write_asset(root, member)
            _write_manifest(root, [member["asset_id"]])
            output = root / "concept.png"
            output.write_bytes(b"stale output")

            with self.assertRaisesRegex(CatalogError, "not present in manifest"):
                register_output(root, stale["asset_id"], "hero_3q", "v001", output)

    def test_unrelated_duplicate_manifest_id_blocks_registration(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            current = valid_building()
            duplicated = _asset_for("reference", "REF_QS_DUPLICATED_MEMBER", "B0")
            _write_asset(root, current)
            _write_asset(root, duplicated)
            _write_manifest(
                root,
                [current["asset_id"], duplicated["asset_id"], duplicated["asset_id"]],
            )
            output = root / "concept.png"
            output.write_bytes(b"must not register with duplicate manifest membership")

            with self.assertRaisesRegex(CatalogError, "manifest asset_ids must be unique"):
                register_output(root, current["asset_id"], "hero_3q", "v001", output)

    def test_existing_or_older_version_is_not_overwritten(self):
        with tempfile.TemporaryDirectory() as directory:
            data = valid_building()
            data["reference_images"][0].update(
                {
                    "approval_state": "generated_pending_review",
                    "output_path": "concepts/old.png",
                    "sha256": "a" * 64,
                    "version": "v002",
                }
            )
            data["generation"]["generation_calls_used"] = 1
            root, data = self._root_with_asset(directory, data)
            output = root / "concept.png"
            output.write_bytes(b"concept")

            with self.assertRaisesRegex(CatalogError, "newer than v002"):
                register_output(root, data["asset_id"], "hero_3q", "v002", output)


class BatchReportTests(unittest.TestCase):
    def test_b0_report_is_derived_from_registered_json(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            assets = [
                _asset_for("reference", f"REF_QS_REPORT_{index}", "B0")
                for index in range(4)
            ]
            _write_manifest(root, [item["asset_id"] for item in assets])
            expected: dict[str, tuple[str, str]] = {}
            for asset in assets:
                _write_asset(root, asset)
            for index, asset in enumerate(assets):
                image_path = root / "style" / "boards" / f"board-{index}.png"
                image_path.parent.mkdir(parents=True, exist_ok=True)
                payload = f"board {index}".encode("utf-8")
                image_path.write_bytes(payload)
                register_output(root, asset["asset_id"], "board", "v001", image_path)
                expected[asset["asset_id"]] = (
                    f"style/boards/board-{index}.png",
                    hashlib.sha256(payload).hexdigest(),
                )
            report_path = root / "batch0.md"

            write_batch_report(root, "B0", report_path)

            report = report_path.read_text(encoding="utf-8")
            for asset_id, (output_path, digest) in expected.items():
                self.assertIn(asset_id, report)
                self.assertIn(output_path, report)
                self.assertIn(digest, report)
            self.assertEqual(report.count("generated_pending_review"), 4)
            self.assertEqual(report.count("calls used 1/3"), 4)
            self.assertIn("tripo_allowed=false", report)
            self.assertIn("model_or_sprite_production_allowed=false", report)
            self.assertIn("ue_import_allowed=false", report)

    def test_report_rejects_manifest_asset_id_mismatch(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            assets = [
                _asset_for("reference", f"REF_QS_MISMATCH_{index}", "B0")
                for index in range(4)
            ]
            _write_manifest(root, [item["asset_id"] for item in assets])
            for asset in assets:
                _write_asset(root, asset)
            for index, asset in enumerate(assets):
                image_path = root / f"board-{index}.png"
                image_path.write_bytes(f"board {index}".encode("utf-8"))
                register_output(root, asset["asset_id"], "board", "v001", image_path)
            mismatched_path = root / "assets" / assets[0]["asset_id"] / "asset.json"
            mismatched = json.loads(mismatched_path.read_text(encoding="utf-8"))
            mismatched["asset_id"] = "REF_QS_DIFFERENT_ID"
            _write_json(mismatched_path, mismatched)

            with self.assertRaisesRegex(CatalogError, "asset_id mismatch"):
                write_batch_report(root, "B0", root / "report.md")

    def test_report_rejects_duplicate_manifest_id(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            assets = [
                _asset_for("reference", f"REF_QS_DUP_REPORT_{index}", "B0")
                for index in range(3)
            ]
            _write_manifest(root, [item["asset_id"] for item in assets])
            for asset in assets:
                _write_asset(root, asset)
            for index, asset in enumerate(assets):
                image_path = root / f"duplicate-report-{index}.png"
                image_path.write_bytes(f"board {index}".encode("utf-8"))
                register_output(root, asset["asset_id"], "board", "v001", image_path)
            _write_manifest(
                root,
                [
                    assets[0]["asset_id"],
                    assets[1]["asset_id"],
                    assets[2]["asset_id"],
                    assets[2]["asset_id"],
                ],
            )

            with self.assertRaisesRegex(CatalogError, "manifest asset_ids must be unique"):
                write_batch_report(root, "B0", root / "report.md")


class SchemaContractTests(unittest.TestCase):
    def test_schema_documents_core_contract(self):
        schema_path = (
            PROJECT_ROOT
            / "SourceAssets"
            / "TownPCG"
            / "QingshanEnvironment"
            / "schema"
            / "qingshan_environment_asset.schema.json"
        )
        schema = json.loads(schema_path.read_text(encoding="utf-8"))
        self.assertEqual(schema["$schema"], "https://json-schema.org/draft/2020-12/schema")
        self.assertFalse(schema["additionalProperties"])
        self.assertEqual(
            schema["properties"]["asset_id"]["pattern"],
            "^(REF|BLD|LMK|ENV|P2D|SRF|KIT|PROP)_QS_[A-Z0-9_]+$",
        )
        self.assertEqual(
            schema["properties"]["batch"]["enum"],
            ["REGISTRY", "B0", "B1", "B2", "B3"],
        )
        self.assertEqual(
            schema["properties"]["generation"]["properties"]["max_versions_per_view"]["const"],
            3,
        )
        self.assertFalse(schema["properties"]["generation"]["additionalProperties"])
        self.assertFalse(schema["properties"]["workflow_gates"]["additionalProperties"])


if __name__ == "__main__":
    unittest.main()
