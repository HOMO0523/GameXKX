from __future__ import annotations

import copy
import hashlib
import importlib
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
    MAX_DIRECT_GENERATION_INPUTS,
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
            "batch_state": {
                "B0": "unlocked_pending_generation",
                "B1": "locked_pending_B0_approval",
                "B2": "locked_pending_B1_approval",
                "B3": "locked_pending_B2_approval",
                "REGISTRY": "existing_asset_registered",
            },
            "generation_calls_used": {batch: 0 for batch in BATCH_COUNTS},
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


EXPECTED_QINGSHAN_CATALOG = {
    "REGISTRY": ["BLD_QS_L_A_MARKET_SHOP"],
    "B0": [
        "REF_QS_ENV_STYLE_LOCK",
        "REF_QS_SCALE_LINEUP",
        "REF_QS_CAMERA_ROUTE",
        "REF_QS_SURFACE_PALETTE",
    ],
    "B1": [
        "BLD_QS_M_A_INN",
        "BLD_QS_S_A_HOUSE",
        "LMK_QS_GATE_NORTH",
        "LMK_QS_BRIDGE_MAIN",
        "LMK_QS_DOCK_SOUTH",
        "ENV_QS_MTN_NEAR_A_RIDGE",
        "P2D_QS_MTN_FAR_A_MIST",
        "ENV_QS_ROCK_KIT_A_FIELD",
        "P2D_QS_TREE_PINE_A",
        "P2D_QS_SHRUB_A",
        "SRF_QS_GROUND_EARTH_A",
        "SRF_QS_WATER_RIVER_A",
    ],
    "B2": [
        "BLD_QS_M_B_STREET_SHOP",
        "BLD_QS_S_B_WORKSHOP",
        "BLD_QS_S_C_RIVER_HUT",
        "ENV_QS_MTN_NEAR_B_CLIFF",
        "P2D_QS_MTN_FAR_B_SILHOUETTE",
        "ENV_QS_ROCK_KIT_B_RIVERBANK",
        "P2D_QS_TREE_BROADLEAF_A",
        "P2D_QS_TREE_BAMBOO_A",
        "P2D_QS_GRASS_TUFT_A",
        "P2D_QS_FLOWER_WILD_A",
        "SRF_QS_GROUND_GRASS_A",
        "SRF_QS_ROAD_STONE_A",
        "SRF_QS_RIVERBANK_BLEND_A",
    ],
    "B3": [
        "KIT_QS_FENCE_WOOD_A",
        "KIT_QS_PATH_STEPPING_STONE_A",
        "PROP_QS_MARKET_KIT_A",
        "PROP_QS_DOCK_KIT_A",
        "PROP_QS_STREET_KIT_A",
    ],
}


def _load_catalog_builder():
    try:
        return importlib.import_module("build_qingshan_environment_catalog")
    except ModuleNotFoundError as exc:
        if exc.name == "build_qingshan_environment_catalog":
            raise AssertionError("deterministic Qingshan catalog builder is missing") from exc
        raise


def _seed_catalog_builder_inputs(root: Path) -> None:
    for relative in (
        "source_metrics.json",
        "style/references/provenance.json",
    ):
        source = (
            PROJECT_ROOT
            / "SourceAssets"
            / "TownPCG"
            / "QingshanEnvironment"
            / relative
        )
        destination = root / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(source.read_bytes())


def _asset_jsons(root: Path) -> dict[str, dict]:
    return {
        path.parent.name: json.loads(path.read_text(encoding="utf-8"))
        for path in sorted((root / "assets").glob("*/asset.json"))
    }


def _planned_b0_prompt(asset_id: str) -> str:
    plan_path = (
        PROJECT_ROOT
        / "docs"
        / "superpowers"
        / "plans"
        / "2026-07-10-qingshan-environment-concept-batch0.md"
    )
    plan = plan_path.read_text(encoding="utf-8")
    section_start = plan.index(f"### Task {EXPECTED_QINGSHAN_CATALOG['B0'].index(asset_id) + 5}:")
    next_task = f"### Task {EXPECTED_QINGSHAN_CATALOG['B0'].index(asset_id) + 6}:"
    section_end = plan.find(next_task, section_start)
    section = plan[section_start:] if section_end == -1 else plan[section_start:section_end]
    match = re.search(r"```text\n(.*?)\n```", section, flags=re.DOTALL)
    if match is None:
        raise AssertionError(f"Batch 0 prompt missing from implementation plan for {asset_id}")
    return match.group(1)


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

    def test_generation_trace_metadata_is_complete_canonical_and_unique(self):
        trace = {
            "generation_input_paths": [
                "style/boards/REF_QS_ENV_STYLE_LOCK__board__v001.png",
                "style/references/style_env_day.jpeg",
            ],
            "generation_reference_lineage": [
                "style/references/style_env_day.jpeg",
                "style/references/style_env_night.jpeg",
            ],
            "generation_prompt": "Original prompt\nTargeted v002 revision.",
        }
        data = valid_building()
        data["reference_images"][0].update(trace)
        validate_asset(data)

        malformed = {
            "partial trace": {key: value for key, value in trace.items() if key != "generation_prompt"},
            "absolute direct input": {
                **trace,
                "generation_input_paths": ["C:/Users/example/reference.png"],
            },
            "noncanonical lineage": {
                **trace,
                "generation_reference_lineage": ["style/references/../secret.png"],
            },
            "duplicate lineage": {
                **trace,
                "generation_reference_lineage": [
                    "style/references/style_env_day.jpeg",
                    "style/references/style_env_day.jpeg",
                ],
            },
        }
        for label, metadata in malformed.items():
            with self.subTest(label=label):
                candidate = valid_building()
                candidate["reference_images"][0].update(metadata)
                with self.assertRaises(CatalogError):
                    validate_asset(candidate)

    def test_direct_tool_inputs_are_capped_at_five_but_lineage_can_exceed_five(self):
        data = valid_building()
        data["reference_images"][0].update(
            {
                "generation_input_paths": [f"style/references/direct-{index}.png" for index in range(5)],
                "generation_reference_lineage": [
                    f"style/references/lineage-{index}.png" for index in range(6)
                ],
                "generation_prompt": "Five direct tool inputs with six-source lineage",
            }
        )
        validate_asset(data)

        data["reference_images"][0]["generation_input_paths"].append(
            "style/references/direct-5.png"
        )
        with self.assertRaisesRegex(CatalogError, "at most 5"):
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


class DeterministicCatalogBuilderTests(unittest.TestCase):
    def _build(self, directory: str):
        root = Path(directory)
        _seed_catalog_builder_inputs(root)
        builder = _load_catalog_builder()
        summary = builder.write_catalog(root)
        return root, builder, summary, _asset_jsons(root)

    def test_catalog_json_files_are_pinned_to_lf_for_byte_stable_checkouts(self):
        attributes = (
            PROJECT_ROOT
            / "SourceAssets"
            / "TownPCG"
            / "QingshanEnvironment"
            / ".gitattributes"
        )
        self.assertTrue(attributes.is_file(), "catalog-local .gitattributes is missing")
        self.assertEqual(attributes.read_text(encoding="utf-8"), "*.json text eol=lf\n")

    def test_builder_writes_the_exact_ordered_catalog_and_batch_contract(self):
        with tempfile.TemporaryDirectory() as directory:
            root, _builder, summary, assets = self._build(directory)
            expected_ids = [
                asset_id
                for batch in BATCH_COUNTS
                for asset_id in EXPECTED_QINGSHAN_CATALOG[batch]
            ]
            manifest = json.loads((root / "manifest.json").read_text(encoding="utf-8"))

            self.assertEqual(manifest["catalog"], EXPECTED_QINGSHAN_CATALOG)
            self.assertEqual(manifest["asset_ids"], expected_ids)
            self.assertEqual(manifest["batch_call_caps"], BATCH_CALL_CAPS)
            self.assertEqual(set(assets), set(expected_ids))
            self.assertEqual(summary, validate_catalog(root))
            self.assertEqual(
                summary,
                {
                    "ok": True,
                    "asset_count": 35,
                    "batch_counts": BATCH_COUNTS,
                    "plant_cells": 40,
                },
            )

            for asset_id, asset in assets.items():
                expected_views = list(EXPECTED_VIEWS[asset["category"]])
                self.assertEqual(asset["generation"]["required_view_kinds"], expected_views)
                self.assertEqual(
                    [reference["kind"] for reference in asset["reference_images"]],
                    expected_views,
                )
                expected_calls = 0 if asset["category"] == "registry" else len(expected_views) * 3
                self.assertEqual(asset["generation"]["max_generation_calls"], expected_calls)
                self.assertEqual(asset["generation"]["generation_calls_used"], 0)
                for gate in (
                    "tripo_allowed",
                    "model_or_sprite_production_allowed",
                    "ue_import_allowed",
                ):
                    self.assertIs(asset["workflow_gates"][gate], False, f"{asset_id}: {gate}")

            for asset_id in EXPECTED_QINGSHAN_CATALOG["B0"]:
                self.assertIs(assets[asset_id]["workflow_gates"]["batch_unlocked"], True)
                self.assertIs(
                    assets[asset_id]["workflow_gates"]["concept_generation_allowed"], True
                )
            for batch in ("B1", "B2", "B3"):
                for asset_id in EXPECTED_QINGSHAN_CATALOG[batch]:
                    self.assertIs(assets[asset_id]["workflow_gates"]["batch_unlocked"], False)
                    self.assertIs(
                        assets[asset_id]["workflow_gates"]["concept_generation_allowed"],
                        False,
                    )

    def test_builder_records_exact_b0_prompts_and_stable_reference_paths(self):
        with tempfile.TemporaryDirectory() as directory:
            _root, _builder, _summary, assets = self._build(directory)
            stable_prefix = "style/references/"

            for asset_id, asset in assets.items():
                stable_references = asset["source_provenance"]["stable_reference_images"]
                self.assertGreater(len(stable_references), 0, asset_id)
                self.assertTrue(
                    all(reference.startswith(stable_prefix) for reference in stable_references),
                    asset_id,
                )
                self.assertTrue(
                    set(stable_references).issubset(asset["generation"]["input_images"]),
                    asset_id,
                )
                self.assertEqual(
                    len(asset["generation"]["input_images"]),
                    len(asset["generation"]["input_image_roles"]),
                )

            for asset_id in EXPECTED_QINGSHAN_CATALOG["B0"]:
                self.assertEqual(assets[asset_id]["generation"]["prompt"], _planned_b0_prompt(asset_id))

    def test_generated_palettes_do_not_repeat_color_tokens(self):
        with tempfile.TemporaryDirectory() as directory:
            _root, _builder, _summary, assets = self._build(directory)

            for asset_id, asset in assets.items():
                self.assertEqual(len(asset["palette"]), len(set(asset["palette"])), asset_id)
                generation_palette = asset["generation"]["palette"]
                self.assertEqual(
                    len(generation_palette), len(set(generation_palette)), asset_id
                )

    def test_buildings_use_measured_largest_anchor_and_distinct_staggered_families(self):
        with tempfile.TemporaryDirectory() as directory:
            root, _builder, _summary, assets = self._build(directory)
            metrics = json.loads((root / "source_metrics.json").read_text(encoding="utf-8"))
            registry = assets["BLD_QS_L_A_MARKET_SHOP"]
            expected = {
                "BLD_QS_M_A_INN": (
                    [4.8, 5.6, 5.2], "asymmetric_xieshan", "warm_red_brown", [30000, 35000], 35000
                ),
                "BLD_QS_S_A_HOUSE": (
                    [3.6, 4.2, 3.8], "double_slope", "indigo", [15000, 20000], 20000
                ),
                "BLD_QS_M_B_STREET_SHOP": (
                    [4.2, 5, 4.6], "deep_eave_half_storey", "ink_green", [25000, 30000], 30000
                ),
                "BLD_QS_S_B_WORKSHOP": (
                    [4, 4.8, 3.6], "hard_gable", "ochre", [12000, 18000], 18000
                ),
                "BLD_QS_S_C_RIVER_HUT": (
                    [3.8, 4.5, 3.5], "low_eave", "teal", [15000, 20000], 20000
                ),
            }

            self.assertEqual(registry["target_dimensions_m"], metrics["bounds_size_m"])
            self.assertIn("existing_quad_faces_target", registry["registry"])
            self.assertEqual(registry["registry"]["existing_quad_faces_target"], 50000)
            registry_volume = 1
            for dimension in registry["target_dimensions_m"]:
                registry_volume *= dimension
            roof_families = set()
            roof_colors = set()
            for asset_id, (dimensions, roof_form, roof_color, approved_range, target) in expected.items():
                asset = assets[asset_id]
                self.assertEqual(asset["target_dimensions_m"], dimensions)
                self.assertEqual(asset["roof"]["form"], roof_form)
                self.assertEqual(asset["roof"]["primary_color"], roof_color)
                self.assertEqual(asset["retopo_target_quads"], target)
                self.assertEqual(asset["building"]["model_pipeline"]["source"], "Tripo_high_precision")
                self.assertEqual(asset["building"]["model_pipeline"]["topology"], "quad")
                self.assertEqual(
                    asset["building"]["model_pipeline"]["approved_quad_face_range"],
                    approved_range,
                )
                self.assertEqual(
                    asset["building"]["model_pipeline"]["target_quad_faces"], target
                )
                self.assertEqual(
                    asset["building"]["model_pipeline"]["material_stage"],
                    "after_retopology",
                )
                self.assertEqual(asset["pcg"]["placement_pattern"], "staggered_not_row")
                volume = 1
                for dimension in dimensions:
                    volume *= dimension
                self.assertLess(volume, registry_volume)
                roof_families.add(roof_form)
                roof_colors.add(roof_color)
            self.assertEqual(len(roof_families), 5)
            self.assertEqual(len(roof_colors), 5)

    def test_non_b0_assets_define_category_specific_required_elements_per_view(self):
        with tempfile.TemporaryDirectory() as directory:
            _root, _builder, _summary, assets = self._build(directory)
            required = {
                "registry": {
                    "existing_asset_registry": {
                        "source_mesh_path", "measured_bounds_m", "material_slot_count",
                        "pivot_and_forward_axis", "scale_lineup_relationship",
                        "existing_50k_quad_registration",
                    }
                },
                "building": {
                    "hero_3q": {
                        "complete_uncropped_silhouette", "visible_base", "entrance_direction",
                        "roof_readability", "fixed_high_three_quarter_camera",
                        "single_asset_clean_warm_background",
                    },
                    "structure_sheet": {
                        "entrance_front", "side_and_rear", "top_down_footprint",
                        "player_scale_reference", "material_regions",
                    },
                },
                "landmark": {
                    "route_3q": {
                        "player_route_primary_view", "entry_and_exit", "traversal_clearance",
                        "fixed_high_three_quarter_camera",
                    },
                    "structure_connection_sheet": {
                        "front_and_side_structure", "top_down_passage_dimensions",
                        "connection_interfaces", "material_regions", "simple_collision_clearance",
                    },
                },
                "near_mountain": {
                    "module_lineup": {
                        "variant_module_lineup", "join_interfaces", "complete_base",
                        "distinct_silhouette",
                    },
                    "assembly_player_scale": {
                        "assembled_edge_example", "player_and_building_scale",
                        "north_gate_gap_preserved", "lower_center_open",
                    },
                },
                "far_mountain": {
                    "silhouette_layers": {
                        "two_joinable_variants", "clean_silhouette_band",
                        "separated_depth_layer_shapes",
                    },
                    "player_camera_composite": {
                        "fixed_player_camera", "near_mountain_relationship",
                        "north_gate_gap_visible", "depth_layer_readability",
                    },
                },
                "rock_kit": {
                    "variant_lineup": {
                        "S_M_L_variants", "complete_separated_silhouettes", "flat_bases",
                        "material_state",
                    },
                    "silhouette_scale": {
                        "black_silhouette", "player_and_building_scale", "placement_cluster",
                    },
                },
                "paper2d_plant": {
                    "neutral_variants": {
                        "separate_A_B_C", "stable_root_or_trunk", "warm_plain_background",
                    },
                    "silhouette_scale_cluster": {
                        "black_silhouette", "project_character_and_building_scale_ruler",
                        "cluster_of_3_to_5",
                    },
                    "wind_poses": {
                        "only_variant_A_animated", "B_and_C_static", "explicit_frame_sequence",
                        "stable_root_or_trunk",
                    },
                },
                "surface": {
                    "seamless_transition_sheet": {
                        "no_perspective_tile_sample", "boundary_transition_strips",
                        "non_repeating_shape_check",
                    },
                    "player_camera_material_sheet": {
                        "sloped_material_example", "fixed_player_camera_samples_5m_10m_20m",
                    },
                },
                "linear_kit": {
                    "variant_lineup": {
                        "three_named_variants", "complete_separated_silhouettes",
                        "connection_pivots",
                    },
                    "usage_scale_sheet": {
                        "project_character_and_building_scale", "placement_examples",
                        "connection_clearance",
                    },
                },
                "prop_kit": {
                    "variant_lineup": {
                        "three_named_variants", "complete_separated_silhouettes",
                        "connection_pivots",
                    },
                    "usage_scale_sheet": {
                        "project_character_and_building_scale", "placement_examples",
                        "connection_clearance",
                    },
                },
            }

            for asset_id, asset in assets.items():
                if asset["batch"] == "B0":
                    continue
                self.assertIn("view_contracts", asset["generation"]["prompt_sections"], asset_id)
                contracts = asset["generation"]["prompt_sections"]["view_contracts"]
                self.assertEqual(set(contracts), set(EXPECTED_VIEWS[asset["category"]]), asset_id)
                references = {item["kind"]: item for item in asset["reference_images"]}
                for view_kind, expected_elements in required[asset["category"]].items():
                    contract = contracts[view_kind]
                    self.assertTrue(contract["instruction"].strip(), f"{asset_id}:{view_kind}")
                    self.assertTrue(
                        expected_elements.issubset(contract["required_elements"]),
                        f"{asset_id}:{view_kind}",
                    )
                    self.assertEqual(
                        references[view_kind]["required_annotations"],
                        contract["required_elements"],
                    )

    def test_plant_wind_view_records_exact_frame_order_and_surfaces_record_distance_and_water_layers(self):
        with tempfile.TemporaryDirectory() as directory:
            _root, _builder, _summary, assets = self._build(directory)
            five_frame_order = [
                "neutral", "left_small", "left_large", "right_small", "right_large"
            ]
            four_frame_order = ["neutral", "left", "right", "slight_return"]
            for asset_id, asset in assets.items():
                if asset["category"] == "paper2d_plant":
                    self.assertIn("view_contracts", asset["generation"]["prompt_sections"], asset_id)
                    wind = asset["generation"]["prompt_sections"]["view_contracts"]["wind_poses"]
                    expected = five_frame_order if asset["wind_frames"] == 5 else four_frame_order
                    self.assertEqual(wind["frame_sequence"], expected, asset_id)

            surface_base = {
                "no_perspective_tile_sample", "boundary_transition_strips",
                "non_repeating_shape_check",
            }
            camera_base = {"sloped_material_example", "fixed_player_camera_samples_5m_10m_20m"}
            for asset_id, asset in assets.items():
                if asset["category"] != "surface":
                    continue
                views = asset["generation"]["prompt_sections"]["view_contracts"]
                self.assertTrue(surface_base.issubset(views["seamless_transition_sheet"]["required_elements"]))
                self.assertTrue(camera_base.issubset(views["player_camera_material_sheet"]["required_elements"]))
            water_views = assets["SRF_QS_WATER_RIVER_A"]["generation"]["prompt_sections"]["view_contracts"]
            water_layers = {"flow_direction", "bank_line", "shallow_deep_bands", "foam_layer"}
            for contract in water_views.values():
                self.assertTrue(water_layers.issubset(contract["required_elements"]))

    def test_style_profile_records_route_terrain_material_and_adjustable_performance_policy(self):
        with tempfile.TemporaryDirectory() as directory:
            root, _builder, _summary, assets = self._build(directory)
            profile = json.loads(
                (root / "style" / "QS_InkToon_v1.json").read_text(encoding="utf-8")
            )
            layout = profile["town_layout_contract"]

            self.assertEqual(layout["playable_bounds_m"], [160, 100])
            self.assertEqual(layout["road"]["provider"], "QuickRoad")
            self.assertEqual(layout["road"]["shape"], "gentle_horizontal_S_right_to_left")
            self.assertEqual(layout["north_gate"]["screen_side"], "right")
            self.assertEqual(
                layout["north_gate"]["player_default_facing_relation"],
                "not_facing_gate_at_start",
            )
            self.assertEqual(layout["bridge"]["crosses"], "river")
            self.assertEqual(layout["edge_terrain"]["enclosure"], "hybrid_mountain_ring")
            self.assertEqual(layout["edge_terrain"]["lower_center"], "open_for_gameplay")
            self.assertEqual(
                profile["unreal_material_contract"]["bsdf_style"],
                "UE5.8_Substrate_BSDF_ink_toon",
            )
            self.assertEqual(profile["performance_policy"]["mode"], "conservative_initial")
            self.assertIs(profile["performance_policy"]["adjustable_after_profile"], True)
            self.assertEqual(assets["LMK_QS_GATE_NORTH"]["pcg"]["screen_anchor"], "right")
            self.assertEqual(assets["LMK_QS_BRIDGE_MAIN"]["pcg"]["road_provider"], "QuickRoad")
            self.assertEqual(assets["SRF_QS_ROAD_STONE_A"]["pcg"]["road_provider"], "QuickRoad")

    def test_plant_families_have_only_a_animated_and_exact_40_cell_budget(self):
        with tempfile.TemporaryDirectory() as directory:
            _root, _builder, _summary, assets = self._build(directory)
            expected = {
                "P2D_QS_TREE_PINE_A": (5, 7),
                "P2D_QS_SHRUB_A": (5, 7),
                "P2D_QS_TREE_BROADLEAF_A": (5, 7),
                "P2D_QS_TREE_BAMBOO_A": (5, 7),
                "P2D_QS_GRASS_TUFT_A": (4, 6),
                "P2D_QS_FLOWER_WILD_A": (4, 6),
            }
            self.assertEqual(
                sum(assets[asset_id]["paper2d"]["atlas_cell_count"] for asset_id in expected),
                40,
            )
            for asset_id, (frames, cells) in expected.items():
                asset = assets[asset_id]
                self.assertEqual(asset["paper2d"]["variants"], ["A", "B", "C"])
                self.assertEqual(asset["animated_variants"], ["A"])
                self.assertEqual(asset["wind_frames"], frames)
                self.assertEqual(asset["atlas_cell_count"], cells)
                self.assertEqual(asset["paper2d"]["atlas_cell_count"], cells)

    def test_plant_ppu_content_boxes_reconstruct_each_target_world_size(self):
        with tempfile.TemporaryDirectory() as directory:
            _root, _builder, _summary, assets = self._build(directory)
            expected = {
                "P2D_QS_TREE_PINE_A": ("height", [256, 448], 0.64),
                "P2D_QS_SHRUB_A": ("height", [240, 180], 1.2),
                "P2D_QS_TREE_BROADLEAF_A": ("height", [360, 432], 0.72),
                "P2D_QS_TREE_BAMBOO_A": ("height", [288, 432], 0.72),
                "P2D_QS_GRASS_TUFT_A": ("height", [192, 192], 2.4),
                "P2D_QS_FLOWER_WILD_A": ("height", [168, 216], 2.4),
            }
            for asset_id, (basis, content_box, ppu) in expected.items():
                asset = assets[asset_id]
                paper2d = asset["paper2d"]
                for field in (
                    "ppu_basis_dimension", "content_box_px", "import_scale_override",
                    "unreal_units_per_meter", "ppu_definition",
                ):
                    self.assertIn(field, paper2d, f"{asset_id}:{field}")
                self.assertEqual(paper2d["ppu_basis_dimension"], basis, asset_id)
                self.assertEqual(paper2d["content_box_px"], content_box, asset_id)
                self.assertEqual(paper2d["import_scale_override"], 1.0, asset_id)
                self.assertEqual(paper2d["unreal_units_per_meter"], 100, asset_id)
                self.assertEqual(
                    paper2d["ppu_definition"], "pixels_per_unreal_unit_centimeter", asset_id
                )
                self.assertAlmostEqual(asset["pixels_per_unreal_unit"], ppu, places=8)
                self.assertTrue(
                    all(
                        content <= cell
                        for content, cell in zip(content_box, asset["sprite_cell_px"])
                    ),
                    asset_id,
                )
                reconstructed_m = [
                    pixels / (ppu * 100.0) * paper2d["import_scale_override"]
                    for pixels in content_box
                ]
                for actual, target in zip(reconstructed_m, asset["card_world_size_m"]):
                    self.assertAlmostEqual(actual, target, places=6, msg=asset_id)

    def test_generated_board_inputs_and_top_level_dependencies_have_the_same_asset_edges(self):
        with tempfile.TemporaryDirectory() as directory:
            _root, _builder, _summary, assets = self._build(directory)
            for asset_id, asset in assets.items():
                generated_inputs = {
                    match.group(1)
                    for path in asset["generation"]["input_images"]
                    if (
                        match := re.fullmatch(
                            r"style/boards/(REF_QS_[A-Z0-9_]+)__board__v001\.png", path
                        )
                    )
                }
                generated_dependencies = {
                    dependency
                    for dependency in asset["dependencies"]
                    if dependency.startswith("REF_QS_")
                }
                self.assertEqual(generated_inputs, generated_dependencies, asset_id)

            self.assertEqual(
                {
                    dependency
                    for dependency in assets["BLD_QS_L_A_MARKET_SHOP"]["dependencies"]
                    if dependency.startswith("REF_QS_")
                },
                set(),
            )
            for asset_id in (
                "REF_QS_SCALE_LINEUP", "REF_QS_CAMERA_ROUTE", "REF_QS_SURFACE_PALETTE"
            ):
                self.assertIn("REF_QS_ENV_STYLE_LOCK", assets[asset_id]["dependencies"])

    def test_check_mode_detects_drift_without_rewriting(self):
        with tempfile.TemporaryDirectory() as directory:
            root, builder, _summary, _assets = self._build(directory)
            drift_path = root / "assets" / "BLD_QS_M_A_INN" / "asset.json"
            drifted = json.loads(drift_path.read_text(encoding="utf-8"))
            drifted["display_name"] = "intentional drift"
            _write_json(drift_path, drifted)
            before = drift_path.read_bytes()

            with self.assertRaisesRegex(builder.CatalogBuildError, "drift"):
                builder.check_catalog(root)

            self.assertEqual(drift_path.read_bytes(), before)

    def test_check_mode_preserves_registered_output_evidence(self):
        with tempfile.TemporaryDirectory() as directory:
            root, builder, _summary, _assets = self._build(directory)
            output = root / "style" / "boards" / "style-board.png"
            output.parent.mkdir(parents=True, exist_ok=True)
            output.write_bytes(b"registered Batch 0 board")
            inputs = [
                "style/references/style_env_day.jpeg",
                "style/references/style_character_scale.jpeg",
            ]
            lineage = inputs + ["style/references/style_env_night.jpeg"]
            for relative_path in lineage:
                source = root / relative_path
                source.parent.mkdir(parents=True, exist_ok=True)
                source.write_bytes(relative_path.encode("utf-8"))
            register_output(
                root,
                "REF_QS_ENV_STYLE_LOCK",
                "board",
                "v001",
                output,
                generation_input_paths=inputs,
                generation_reference_lineage=lineage,
                generation_prompt="Traceable Batch 0 style-board prompt",
            )
            asset_path = root / "assets" / "REF_QS_ENV_STYLE_LOCK" / "asset.json"
            before = asset_path.read_bytes()

            try:
                result = builder.check_catalog(root)
            except builder.CatalogBuildError as exc:
                self.fail(f"registered evidence must not be treated as catalog drift: {exc}")
            self.assertEqual(result, {"ok": True, "unchanged": 37})
            self.assertEqual(asset_path.read_bytes(), before)

    def test_builder_preserves_trace_only_as_a_complete_versioned_group(self):
        with tempfile.TemporaryDirectory() as directory:
            root, builder, _summary, assets = self._build(directory)
            asset_id = "REF_QS_ENV_STYLE_LOCK"
            asset_path = root / "assets" / asset_id / "asset.json"
            trace = {
                "generation_input_paths": ["style/references/style_env_day.jpeg"],
                "generation_reference_lineage": ["style/references/style_env_day.jpeg"],
                "generation_prompt": "version-bound prompt",
            }

            for removed in ("generation_prompt", "version"):
                current = copy.deepcopy(assets[asset_id])
                current_reference = current["reference_images"][0]
                current_reference.update({"version": "v002", **trace})
                current_reference.pop(removed)
                _write_json(asset_path, current)
                rebuilt = copy.deepcopy(assets[asset_id])

                builder._overlay_registered_output(root, rebuilt)

                rebuilt_reference = rebuilt["reference_images"][0]
                for trace_field in trace:
                    self.assertNotIn(trace_field, rebuilt_reference, removed)

    def test_registering_all_b0_boards_updates_manifest_and_remains_deterministic(self):
        with tempfile.TemporaryDirectory() as directory:
            root, builder, _summary, _assets = self._build(directory)
            b0_ids = EXPECTED_QINGSHAN_CATALOG["B0"]
            for index, asset_id in enumerate(b0_ids, start=1):
                output = root / "style" / "boards" / f"{asset_id}__board__v001.png"
                output.parent.mkdir(parents=True, exist_ok=True)
                output.write_bytes(f"project-local board {index}".encode("utf-8"))

                result = register_output(root, asset_id, "board", "v001", output)

                manifest = json.loads((root / "manifest.json").read_text(encoding="utf-8"))
                self.assertEqual(manifest["generation_calls_used"]["B0"], index)
                expected_state = "generated_pending_review" if index == 4 else "generation_in_progress"
                self.assertEqual(manifest["batch_state"]["B0"], expected_state)
                asset = json.loads(
                    (root / "assets" / asset_id / "asset.json").read_text(encoding="utf-8")
                )
                self.assertEqual(asset["generation"]["generation_calls_used"], 1)
                self.assertEqual(result["generation_calls_used"], 1)
                self.assertEqual(builder.check_catalog(root), {"ok": True, "unchanged": 37})

            manifest = json.loads((root / "manifest.json").read_text(encoding="utf-8"))
            self.assertEqual(manifest["generation_calls_used"]["B0"], 4)
            self.assertEqual(manifest["batch_call_caps"]["B0"], 12)
            self.assertEqual(manifest["batch_state"]["B0"], "generated_pending_review")

    def test_register_failure_rolls_back_asset_and_manifest_together(self):
        with tempfile.TemporaryDirectory() as directory:
            root, _builder, _summary, _assets = self._build(directory)
            asset_id = "REF_QS_ENV_STYLE_LOCK"
            output = root / "style" / "boards" / f"{asset_id}__board__v001.png"
            output.parent.mkdir(parents=True, exist_ok=True)
            output.write_bytes(b"transaction failure board")
            asset_path = root / "assets" / asset_id / "asset.json"
            manifest_path = root / "manifest.json"
            before = {asset_path: asset_path.read_bytes(), manifest_path: manifest_path.read_bytes()}
            calls = 0

            def fail_second_replace(source, destination):
                nonlocal calls
                calls += 1
                if calls == 2:
                    raise OSError("injected second replace failure")
                os.replace(source, destination)

            try:
                register_output(
                    root, asset_id, "board", "v001", output, replace_file=fail_second_replace
                )
            except TypeError as exc:
                self.fail(f"register_output must expose controlled replace injection: {exc}")
            except CatalogError as exc:
                self.assertIn("injected second replace failure", str(exc))
            else:
                self.fail("injected registration failure unexpectedly succeeded")

            self.assertEqual({path: path.read_bytes() for path in before}, before)
            self.assertFalse(list(root.rglob("*.tmp")))
            self.assertFalse(list(root.rglob("*.bak")))

    def test_register_rejects_external_and_symlink_escape_and_stores_relative_posix_path(self):
        with tempfile.TemporaryDirectory() as directory, tempfile.TemporaryDirectory() as outside_directory:
            root, _builder, _summary, _assets = self._build(directory)
            outside = Path(outside_directory)
            external = outside / "external.png"
            external.write_bytes(b"external evidence")

            with self.assertRaisesRegex(CatalogError, "inside catalog root"):
                register_output(root, "REF_QS_ENV_STYLE_LOCK", "board", "v001", external)
            with self.assertRaisesRegex(CatalogError, "inside catalog root"):
                register_output(
                    root,
                    "REF_QS_ENV_STYLE_LOCK",
                    "board",
                    "v001",
                    root / ".." / Path(outside_directory).name / "external.png",
                )

            link = root / "style" / "boards"
            if link.exists():
                link.rmdir()
            try:
                link.symlink_to(outside, target_is_directory=True)
            except OSError as exc:
                if os.name != "nt":
                    self.skipTest(f"platform cannot create a test symlink: {exc}")
                junction = subprocess.run(
                    ["cmd", "/c", "mklink", "/J", str(link), str(outside)],
                    capture_output=True,
                    text=True,
                    check=False,
                )
                if junction.returncode != 0:
                    self.skipTest(f"platform cannot create a test junction: {junction.stderr}")
            try:
                with self.assertRaisesRegex(CatalogError, "inside catalog root"):
                    register_output(
                        root,
                        "REF_QS_ENV_STYLE_LOCK",
                        "board",
                        "v001",
                        link / "external.png",
                    )
            finally:
                if link.is_symlink():
                    link.unlink()
                elif link.exists():
                    os.rmdir(link)

            valid = root / "style" / "boards" / "valid.png"
            valid.parent.mkdir(parents=True, exist_ok=True)
            valid.write_bytes(b"valid project evidence")
            result = register_output(root, "REF_QS_ENV_STYLE_LOCK", "board", "v001", valid)
            self.assertEqual(result["output_path"], "style/boards/valid.png")
            self.assertNotIn("Users", result["output_path"])
            self.assertNotIn("\\", result["output_path"])

    def test_builder_check_rejects_deleted_or_tampered_registered_evidence(self):
        for mutation in ("delete", "tamper"):
            with self.subTest(mutation=mutation), tempfile.TemporaryDirectory() as directory:
                root, builder, _summary, _assets = self._build(directory)
                output = root / "style" / "boards" / "evidence.png"
                output.parent.mkdir(parents=True, exist_ok=True)
                output.write_bytes(b"original evidence")
                register_output(root, "REF_QS_ENV_STYLE_LOCK", "board", "v001", output)
                if mutation == "delete":
                    output.unlink()
                else:
                    output.write_bytes(b"tampered evidence")

                with self.assertRaisesRegex(builder.CatalogBuildError, "evidence|SHA|missing"):
                    builder.check_catalog(root)

    def test_catalog_write_transaction_rolls_back_all_37_files_on_replace_failure(self):
        with tempfile.TemporaryDirectory() as directory:
            root, builder, _summary, _assets = self._build(directory)
            paths = [root / "manifest.json", root / "style" / "QS_InkToon_v1.json"]
            paths.extend(sorted((root / "assets").glob("*/asset.json")))
            before = {path: path.read_bytes() for path in paths}
            calls = 0

            def fail_tenth_replace(source, destination):
                nonlocal calls
                calls += 1
                if calls == 10:
                    raise OSError("injected catalog replace failure")
                os.replace(source, destination)

            try:
                builder.write_catalog(root, replace_file=fail_tenth_replace)
            except TypeError as exc:
                self.fail(f"write_catalog must expose controlled replace injection: {exc}")
            except builder.CatalogBuildError as exc:
                self.assertIn("injected catalog replace failure", str(exc))
            else:
                self.fail("injected catalog failure unexpectedly succeeded")

            self.assertEqual({path: path.read_bytes() for path in paths}, before)
            self.assertFalse(list(root.rglob("*.tmp")))
            self.assertFalse(list(root.rglob("*.bak")))

    def test_catalog_write_rejects_asset_directory_escape_without_touching_external_file(self):
        with tempfile.TemporaryDirectory() as directory, tempfile.TemporaryDirectory() as outside_directory:
            root, builder, _summary, _assets = self._build(directory)
            asset_id = "BLD_QS_M_A_INN"
            asset_dir = root / "assets" / asset_id
            asset_path = asset_dir / "asset.json"
            asset_path.unlink()
            asset_dir.rmdir()
            outside = Path(outside_directory)
            external_asset = outside / "asset.json"
            sentinel = b"external file must remain unchanged"
            external_asset.write_bytes(sentinel)
            try:
                asset_dir.symlink_to(outside, target_is_directory=True)
            except OSError as exc:
                if os.name != "nt":
                    self.skipTest(f"platform cannot create a test symlink: {exc}")
                junction = subprocess.run(
                    ["cmd", "/c", "mklink", "/J", str(asset_dir), str(outside)],
                    capture_output=True,
                    text=True,
                    check=False,
                )
                if junction.returncode != 0:
                    self.skipTest(f"platform cannot create a test junction: {junction.stderr}")
            try:
                with self.assertRaisesRegex(builder.CatalogBuildError, "escapes catalog root"):
                    builder.write_catalog(root)
                self.assertEqual(external_asset.read_bytes(), sentinel)
                self.assertFalse(list(outside.glob("*.tmp")))
                self.assertFalse(list(outside.glob("*.bak")))
            finally:
                if asset_dir.is_symlink():
                    asset_dir.unlink()
                elif asset_dir.exists():
                    os.rmdir(asset_dir)

    def test_builder_is_byte_stable_and_checks_all_37_generated_json_files(self):
        with tempfile.TemporaryDirectory() as directory:
            root, builder, _summary, _assets = self._build(directory)
            generated_paths = [root / "manifest.json", root / "style" / "QS_InkToon_v1.json"]
            generated_paths.extend(sorted((root / "assets").glob("*/asset.json")))
            before = {path: path.read_bytes() for path in generated_paths}

            self.assertEqual(builder.check_catalog(root), {"ok": True, "unchanged": 37})
            builder.write_catalog(root)

            self.assertEqual({path: path.read_bytes() for path in generated_paths}, before)
            for path, payload in before.items():
                self.assertTrue(payload.endswith(b"\n"), path)
                parsed = json.loads(payload)
                self.assertEqual(list(parsed), sorted(parsed), path)

    def test_builder_refuses_missing_source_metrics_or_provenance(self):
        builder = _load_catalog_builder()
        for missing in ("source_metrics.json", "style/references/provenance.json"):
            with self.subTest(missing=missing), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                _seed_catalog_builder_inputs(root)
                (root / missing).unlink()

                with self.assertRaisesRegex(builder.CatalogBuildError, "missing"):
                    builder.write_catalog(root)


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

    def test_registration_records_actual_generation_trace(self):
        with tempfile.TemporaryDirectory() as directory:
            root, data = self._root_with_asset(directory)
            output = root / "concepts" / "inn-v002.png"
            output.parent.mkdir()
            output.write_bytes(b"revised concept")
            input_paths = [
                "style/boards/prior-v001.png",
                "style/references/style-env.jpeg",
            ]
            lineage = [
                "style/references/style-env.jpeg",
                "style/references/style-night.jpeg",
            ]
            for relative_path in set(input_paths + lineage):
                source = root / relative_path
                source.parent.mkdir(parents=True, exist_ok=True)
                source.write_bytes(relative_path.encode("utf-8"))
            prompt = "Original prompt\nTargeted v002 revision."

            result = register_output(
                root,
                data["asset_id"],
                "hero_3q",
                "v001",
                output,
                generation_input_paths=input_paths,
                generation_reference_lineage=lineage,
                generation_prompt=prompt,
            )

            stored = json.loads(
                (root / "assets" / data["asset_id"] / "asset.json").read_text(encoding="utf-8")
            )
            reference = stored["reference_images"][0]
            self.assertEqual(reference["generation_input_paths"], input_paths)
            self.assertEqual(reference["generation_reference_lineage"], lineage)
            self.assertEqual(reference["generation_prompt"], prompt)
            self.assertEqual(result["generation_input_paths"], input_paths)

    def test_registration_rejects_a_missing_actual_generation_input(self):
        with tempfile.TemporaryDirectory() as directory:
            root, data = self._root_with_asset(directory)
            output = root / "concept.png"
            output.write_bytes(b"concept")

            with self.assertRaisesRegex(CatalogError, "generation input.*does not exist"):
                register_output(
                    root,
                    data["asset_id"],
                    "hero_3q",
                    "v001",
                    output,
                    generation_input_paths=["style/references/missing.jpeg"],
                    generation_reference_lineage=["style/references/missing.jpeg"],
                    generation_prompt="Traceable prompt",
                )

    def test_registration_cli_accepts_generation_trace_flags(self):
        with tempfile.TemporaryDirectory() as directory:
            root, data = self._root_with_asset(directory)
            output = root / "concept.png"
            output.write_bytes(b"concept")
            direct = "style/boards/prior-v001.png"
            lineage = "style/references/style_env_day.jpeg"
            for relative_path in (direct, lineage):
                source = root / relative_path
                source.parent.mkdir(parents=True, exist_ok=True)
                source.write_bytes(relative_path.encode("utf-8"))
            prompt = "Original prompt\nTargeted v002 revision."

            completed = subprocess.run(
                [
                    sys.executable,
                    str(PROJECT_ROOT / "scripts" / "qingshan_environment_assets.py"),
                    "--root",
                    str(root),
                    "--register-output",
                    data["asset_id"],
                    "hero_3q",
                    "v001",
                    str(output),
                    "--generation-input-path",
                    direct,
                    "--generation-reference-lineage",
                    lineage,
                    "--generation-prompt",
                    prompt,
                ],
                capture_output=True,
                text=True,
                check=False,
            )

            self.assertEqual(completed.returncode, 0, completed.stderr)
            result = json.loads(completed.stdout)
            self.assertEqual(result["generation_input_paths"], [direct])
            stored = json.loads(
                (root / "assets" / data["asset_id"] / "asset.json").read_text(encoding="utf-8")
            )
            self.assertEqual(stored["reference_images"][0]["generation_prompt"], prompt)

    def test_revision_requires_fresh_trace_and_rejection_is_byte_stable(self):
        with tempfile.TemporaryDirectory() as directory:
            root, data = self._root_with_asset(directory)
            direct = ["style/references/style-env.jpeg"]
            lineage = ["style/references/style-env.jpeg"]
            source = root / direct[0]
            source.parent.mkdir(parents=True, exist_ok=True)
            source.write_bytes(b"stable reference")
            first = root / "concept-v001.png"
            first.write_bytes(b"first concept")
            register_output(
                root,
                data["asset_id"],
                "hero_3q",
                "v001",
                first,
                generation_input_paths=direct,
                generation_reference_lineage=lineage,
                generation_prompt="v001 generation prompt",
            )
            asset_path = root / "assets" / data["asset_id"] / "asset.json"
            manifest_path = root / "manifest.json"
            before = {
                asset_path: asset_path.read_bytes(),
                manifest_path: manifest_path.read_bytes(),
            }
            revision = root / "concept-v002.png"
            revision.write_bytes(b"second concept")

            with self.assertRaisesRegex(CatalogError, "targeted revision.*fresh generation trace"):
                register_output(root, data["asset_id"], "hero_3q", "v002", revision)

            self.assertEqual(
                {path: path.read_bytes() for path in before},
                before,
            )
            result = register_output(
                root,
                data["asset_id"],
                "hero_3q",
                "v002",
                revision,
                generation_input_paths=direct,
                generation_reference_lineage=lineage,
                generation_prompt="fresh v002 generation prompt",
            )
            stored = json.loads(asset_path.read_text(encoding="utf-8"))
            self.assertEqual(result["version"], "v002")
            self.assertEqual(stored["reference_images"][0]["version"], "v002")
            self.assertEqual(
                stored["reference_images"][0]["generation_prompt"],
                "fresh v002 generation prompt",
            )

    def test_first_registration_above_v001_requires_trace(self):
        with tempfile.TemporaryDirectory() as directory:
            root, data = self._root_with_asset(directory)
            output = root / "concept-v002.png"
            output.write_bytes(b"second version without first")
            asset_path = root / "assets" / data["asset_id"] / "asset.json"
            manifest_path = root / "manifest.json"
            before = {
                asset_path: asset_path.read_bytes(),
                manifest_path: manifest_path.read_bytes(),
            }

            with self.assertRaisesRegex(CatalogError, "targeted revision.*fresh generation trace"):
                register_output(root, data["asset_id"], "hero_3q", "v002", output)

            self.assertEqual(
                {path: path.read_bytes() for path in before},
                before,
            )

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
                image_path = root / "style" / "boards" / f"board-{index}.png"
                image_path.parent.mkdir(parents=True, exist_ok=True)
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
                image_path = root / "style" / "boards" / f"duplicate-report-{index}.png"
                image_path.parent.mkdir(parents=True, exist_ok=True)
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
        reference_item = schema["properties"]["reference_images"]["items"]
        for field in (
            "generation_input_paths",
            "generation_reference_lineage",
            "generation_prompt",
        ):
            self.assertIn(field, reference_item["properties"])
            self.assertEqual(
                sorted(reference_item["dependentRequired"][field]),
                sorted(
                    {
                        "generation_input_paths",
                        "generation_reference_lineage",
                        "generation_prompt",
                    }
                    - {field}
                ),
            )
        direct_inputs = reference_item["properties"]["generation_input_paths"]
        lineage = reference_item["properties"]["generation_reference_lineage"]
        self.assertEqual(direct_inputs["maxItems"], MAX_DIRECT_GENERATION_INPUTS)
        self.assertIn("direct tool inputs", direct_inputs["description"])
        self.assertNotIn("maxItems", lineage)
        self.assertIn("not limited to five", lineage["description"])


if __name__ == "__main__":
    unittest.main()
