"""Contract tests for the Qingshan building concept prompt compiler."""

from __future__ import annotations

import hashlib
import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from scripts.qingshan_building_concepts import (
    ConceptError,
    canonical_json_bytes,
    compile_prompt_packet,
    load_building_style,
    validate_building_style,
    validate_golden_asset,
)


STYLE_ID = "QS_InkToon_Building_v2"
ASSET_ID = "BLD_QS_S_A_HOUSE"


def valid_style() -> dict:
    return {
        "schema_version": 1,
        "style_id": STYLE_ID,
        "inherits": "style/QS_InkToon_v1.json",
        "inherits_sha256": "0" * 64,
        "camera_contract": {
            "view": "fixed_high_three_quarter",
            "single_complete_object": True,
            "visible_base": True,
        },
        "composition_contract": {
            "background": "clean_warm_rice_paper",
            "subject_count": 1,
            "text_or_annotations": False,
        },
        "material_contract": {
            "wall": "warm_off_white_plaster",
            "window_fill": "warm_yellow_translucent_rice_paper",
            "outline": "dark_teal_variable_dry_brush",
        },
        "shape_contract": {
            "doors_windows": "few_large_thick_frames",
            "posts_beams": "chunky_single_readable_masses",
            "roof": "one_bold_bent_silhouette_no_tile_repetition",
            "exaggeration": "playful_asymmetric_moderate",
        },
        "detail_budget": {
            "forbidden": [
                "individual_roof_tiles",
                "repeated_window_lattices",
            ],
        },
        "negative_prompt": [
            "black_window_panes",
            "individual_roof_tiles",
            "fine_window_lattices",
            "triple_layer_fine_column_capitals",
            "over_fragmentation",
            "speckled_noise",
            "excessive_weirdness",
            "mirror_symmetry",
            "text",
            "annotations",
            "multiple_buildings",
            "scene_collage",
        ],
        "prompt_contract": {
            "positive": [
                "fixed high three-quarter camera",
                "one standalone building on a solid warm rice-paper background",
                "warm yellow xuan-paper windows",
                "bold doors, windows, beams, and columns",
                "expressive exaggeration without excessive distortion",
            ],
            "negative": [
                "text, labels, callouts, or dimension annotations",
                "individual roof tiles or tiny repeated lattice grids",
                "black window panes",
            ],
        },
        "reference_images": [
            {
                "path": "style/references/reference.jpeg",
                "role": "mass_and_outline",
                "sha256": "1" * 64,
            }
        ],
    }


def valid_asset() -> dict:
    return {
        "schema_version": 1,
        "asset_id": ASSET_ID,
        "style_profile": STYLE_ID,
        "generation": {
            "subject": (
                "one standalone small Qingshan house with warm yellow paper "
                "windows, a clearly visible base, and a front-right entrance"
            ),
            "subject_count": 1,
            "output_layout": "single standalone image",
            "window_treatment": "warm yellow paper windows",
            "roof_detail": "broad roof masses without repeated tiles",
            "window_detail": "broad frames without fine lattice",
            "visible_base": True,
            "entrance_direction": "front-right",
        },
        "negative_prompt": (
            "multiple buildings, black window panes, repeated individual roof "
            "tiles, fine window lattice, hidden base, ambiguous entrance"
        ),
        "expected_output_filename": "BLD_QS_S_A_HOUSE__golden__v001.png",
    }


def write_json(path: Path, data: dict) -> bytes:
    payload = (json.dumps(data, sort_keys=True, ensure_ascii=False, indent=2) + "\n").encode("utf-8")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(payload)
    return payload


class BuildingStyleContractTests(unittest.TestCase):
    def assert_style_rejected(self, mutation, message: str) -> None:
        style = valid_style()
        mutation(style)
        with self.assertRaisesRegex(ConceptError, message):
            validate_building_style(style)

    def test_accepts_complete_shared_style_master(self) -> None:
        validate_building_style(valid_style())

    def test_requires_window_fill(self) -> None:
        self.assert_style_rejected(
            lambda data: data["material_contract"].pop("window_fill"),
            "window_fill",
        )

    def test_rejects_unknown_fields_at_every_level(self) -> None:
        cases = (
            ("top", lambda d: d.__setitem__("extra", True), "extra"),
            ("camera", lambda d: d["camera_contract"].__setitem__("fov", 35), "camera_contract.fov"),
            ("reference", lambda d: d["reference_images"][0].__setitem__("note", "x"), "reference_images\\[0\\].note"),
        )
        for label, mutation, message in cases:
            with self.subTest(label=label):
                self.assert_style_rejected(mutation, message)

    def test_rejects_bool_where_integer_is_required(self) -> None:
        self.assert_style_rejected(
            lambda data: data["composition_contract"].__setitem__("subject_count", True),
            "composition_contract.subject_count",
        )

    def test_rejects_wrong_container_and_scalar_types(self) -> None:
        cases = (
            (lambda d: d.__setitem__("camera_contract", []), "camera_contract"),
            (lambda d: d.__setitem__("reference_images", {}), "reference_images"),
            (lambda d: d["reference_images"][0].__setitem__("role", 7), "reference_images\\[0\\].role"),
            (lambda d: d["prompt_contract"].__setitem__("positive", "prompt"), "prompt_contract.positive"),
        )
        for mutation, message in cases:
            with self.subTest(message=message):
                self.assert_style_rejected(mutation, message)

    def test_locks_complete_negative_prompt_token_set(self) -> None:
        self.assert_style_rejected(
            lambda data: data["negative_prompt"].remove("scene_collage"),
            "negative_prompt",
        )

    def test_detail_budget_reports_extra_and_order_drift(self) -> None:
        cases = (
            (
                lambda data: data["detail_budget"]["forbidden"].append("extra_detail"),
                "detail_budget.forbidden.*exact canonical list",
            ),
            (
                lambda data: data["detail_budget"]["forbidden"].reverse(),
                "detail_budget.forbidden.*exact canonical list",
            ),
        )
        for mutation, message in cases:
            with self.subTest(message=message):
                self.assert_style_rejected(mutation, message)

    def test_rejects_shared_master_contract_drift(self) -> None:
        cases = (
            ("style id", lambda d: d.__setitem__("style_id", "QS_InkToon_v1"), "style_id"),
            ("schema", lambda d: d.__setitem__("schema_version", 2), "schema_version"),
            (
                "camera",
                lambda d: d["camera_contract"].__setitem__("view", "eye level"),
                "fixed high three-quarter",
            ),
            (
                "incomplete object",
                lambda d: d["camera_contract"].__setitem__("single_complete_object", False),
                "single complete object",
            ),
            (
                "hidden base",
                lambda d: d["camera_contract"].__setitem__("visible_base", False),
                "visible base",
            ),
            (
                "background",
                lambda d: d["composition_contract"].__setitem__("background", "white studio"),
                "warm rice paper",
            ),
            (
                "multiple subjects",
                lambda d: d["composition_contract"].__setitem__("subject_count", 2),
                "single subject",
            ),
            (
                "annotations",
                lambda d: d["composition_contract"].__setitem__("text_or_annotations", True),
                "text annotations",
            ),
            (
                "window material",
                lambda d: d["material_contract"].__setitem__("window_fill", "black glass"),
                "warm yellow xuan paper windows",
            ),
            (
                "thin openings",
                lambda d: d["shape_contract"].__setitem__("doors_windows", "many_thin_frames"),
                "bold doors and windows",
            ),
            (
                "thin structure",
                lambda d: d["shape_contract"].__setitem__("posts_beams", "thin_separate_parts"),
                "bold beams and columns",
            ),
            (
                "tiled roof",
                lambda d: d["shape_contract"].__setitem__("roof", "individual_repeated_tiles"),
                "one bold roof silhouette",
            ),
            (
                "distortion",
                lambda d: d["shape_contract"].__setitem__("exaggeration", "extreme_distortion"),
                "playful_asymmetric_moderate",
            ),
            (
                "roof tiles allowed",
                lambda d: d["detail_budget"]["forbidden"].remove("individual_roof_tiles"),
                "individual roof tiles",
            ),
            (
                "lattices allowed",
                lambda d: d["detail_budget"]["forbidden"].remove("repeated_window_lattices"),
                "repeated window lattices",
            ),
        )
        for label, mutation, message in cases:
            with self.subTest(label=label):
                self.assert_style_rejected(mutation, message)


class BuildingStyleLoadingTests(unittest.TestCase):
    def make_catalog(self, root: Path) -> dict:
        inherited = b'{"style_id":"QS_InkToon_v1"}\n'
        reference = b"stable reference image bytes"
        (root / "style" / "references").mkdir(parents=True)
        (root / "style" / "QS_InkToon_v1.json").write_bytes(inherited)
        (root / "style" / "references" / "reference.jpeg").write_bytes(reference)
        style = valid_style()
        style["inherits_sha256"] = hashlib.sha256(inherited).hexdigest()
        style["reference_images"][0]["sha256"] = hashlib.sha256(reference).hexdigest()
        write_json(root / "style" / f"{STYLE_ID}.json", style)
        return style

    def assert_load_rejected(self, mutation, message: str) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            style = self.make_catalog(root)
            mutation(root, style)
            write_json(root / "style" / f"{STYLE_ID}.json", style)
            with self.assertRaisesRegex(ConceptError, message):
                load_building_style(root)

    def test_loads_style_when_inheritance_and_reference_hashes_match(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            expected = self.make_catalog(root)
            self.assertEqual(load_building_style(root), expected)

    def test_loads_checked_in_building_style_master(self) -> None:
        root = Path(__file__).resolve().parents[1] / "SourceAssets" / "TownPCG" / "QingshanEnvironment"
        style = load_building_style(root)
        self.assertEqual(style["style_id"], STYLE_ID)

    def test_rejects_invalid_catalog_style_paths(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            self.make_catalog(root)
            for style_path in ("/absolute/style.json", "style\\master.json", "style/../style/master.json"):
                with self.subTest(style_path=style_path):
                    with self.assertRaisesRegex(ConceptError, "style_path"):
                        load_building_style(root, style_path)

    def test_rejects_windows_unsafe_path_components(self) -> None:
        unsafe_paths = (
            "style/master.json:stream",
            "style/CON.txt",
            "style/master.json.",
            "style/master.json ",
            "style/\x00master.json",
        )
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            self.make_catalog(root)
            for style_path in unsafe_paths:
                with self.subTest(style_path=repr(style_path)):
                    with self.assertRaisesRegex(ConceptError, "style_path.*Windows-safe"):
                        load_building_style(root, style_path)

    def test_rejects_nul_in_reference_path_without_leaking_system_error(self) -> None:
        self.assert_load_rejected(
            lambda _root, data: data["reference_images"][0].__setitem__(
                "path", "style/references/\x00reference.jpeg"
            ),
            "reference_images\\[0\\].path.*Windows-safe",
        )

    def test_wraps_root_resolution_errors(self) -> None:
        with self.assertRaisesRegex(ConceptError, "root"):
            load_building_style(Path("\x00invalid-root"))

    def test_rejects_invalid_inheritance_paths(self) -> None:
        cases = (
            ("/absolute/base.json", "inherits"),
            ("style\\base.json", "inherits"),
            ("style/../base.json", "inherits"),
        )
        for path, message in cases:
            with self.subTest(path=path):
                self.assert_load_rejected(lambda _root, d, value=path: d.__setitem__("inherits", value), message)

    def test_rejects_missing_inheritance_file(self) -> None:
        self.assert_load_rejected(
            lambda root, _data: (root / "style" / "QS_InkToon_v1.json").unlink(),
            "inherits.*file",
        )

    def test_rejects_invalid_reference_paths(self) -> None:
        cases = (
            ("C:/absolute/reference.jpeg", "reference_images\\[0\\].path"),
            ("style\\references\\reference.jpeg", "reference_images\\[0\\].path"),
            ("style/references/../reference.jpeg", "reference_images\\[0\\].path"),
            ("style/references/missing.jpeg", "reference_images\\[0\\].path.*file"),
        )
        for path, message in cases:
            with self.subTest(path=path):
                self.assert_load_rejected(
                    lambda _root, d, value=path: d["reference_images"][0].__setitem__("path", value),
                    message,
                )

    def test_rejects_inheritance_and_reference_digest_mismatches(self) -> None:
        cases = (
            (lambda _root, d: d.__setitem__("inherits_sha256", "f" * 64), "inherits_sha256"),
            (lambda _root, d: d["reference_images"][0].__setitem__("sha256", "f" * 64), "reference_images\\[0\\].sha256"),
        )
        for mutation, message in cases:
            with self.subTest(message=message):
                self.assert_load_rejected(mutation, message)

    def test_rejects_symlink_escape(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory, tempfile.TemporaryDirectory() as outside_directory:
            root = Path(temporary_directory)
            style = self.make_catalog(root)
            outside_root = Path(outside_directory)
            outside = outside_root / "outside.jpeg"
            outside.write_bytes(b"outside")
            link = root / "style" / "references_escape"
            try:
                os.symlink(outside_root, link, target_is_directory=True)
            except OSError:
                result = subprocess.run(
                    ["cmd", "/c", "mklink", "/J", str(link), str(outside_root)],
                    capture_output=True,
                    text=True,
                )
                if result.returncode:
                    self.skipTest(f"symlinks and junctions unavailable: {result.stderr or result.stdout}")
            style["reference_images"][0]["path"] = "style/references_escape/outside.jpeg"
            style["reference_images"][0]["sha256"] = hashlib.sha256(outside.read_bytes()).hexdigest()
            write_json(root / "style" / f"{STYLE_ID}.json", style)
            with self.assertRaisesRegex(ConceptError, "reference_images\\[0\\].path.*catalog root"):
                load_building_style(root)

    def test_rejects_duplicate_json_fields(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            self.make_catalog(root)
            style_file = root / "style" / f"{STYLE_ID}.json"
            payload = style_file.read_text(encoding="utf-8").replace(
                '"schema_version": 1,',
                '"schema_version": 1,\n  "schema_version": 1,',
                1,
            )
            style_file.write_text(payload, encoding="utf-8", newline="\n")
            with self.assertRaisesRegex(ConceptError, "duplicate.*schema_version"):
                load_building_style(root)

    def test_wraps_dependency_disappearance_after_precheck(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            self.make_catalog(root)
            reference = root / "style" / "references" / "reference.jpeg"
            real_open = Path.open

            def disappear_then_open(path: Path, *args, **kwargs):
                if path == reference and reference.exists():
                    reference.unlink()
                return real_open(path, *args, **kwargs)

            with mock.patch.object(Path, "open", new=disappear_then_open):
                with self.assertRaisesRegex(ConceptError, "reference_images\\[0\\].path"):
                    load_building_style(root)


class GoldenAssetContractTests(unittest.TestCase):
    def assert_asset_rejected(self, mutation, message: str) -> None:
        asset = valid_asset()
        mutation(asset)
        with self.assertRaisesRegex(ConceptError, message):
            validate_golden_asset(asset, valid_style())

    def test_accepts_complete_standalone_s_a_asset(self) -> None:
        validate_golden_asset(valid_asset(), valid_style())

    def test_rejects_black_windows_in_subject(self) -> None:
        self.assert_asset_rejected(
            lambda data: data["generation"].__setitem__(
                "subject", data["generation"]["subject"] + " with black window panes"
            ),
            "warm yellow paper windows",
        )

    def test_rejects_asset_contract_drift(self) -> None:
        cases = (
            (
                "wrong style",
                lambda d: d.__setitem__("style_profile", "QS_InkToon_v1"),
                "style_profile",
            ),
            (
                "wrong asset id",
                lambda d: d.__setitem__("asset_id", "BLD_QS_S_B_WORKSHOP"),
                "asset_id",
            ),
            (
                "wrong schema",
                lambda d: d.__setitem__("schema_version", 2),
                "schema_version",
            ),
            (
                "multiple subjects",
                lambda d: d["generation"].__setitem__("subject_count", 2),
                "single subject",
            ),
            (
                "black windows",
                lambda d: d["generation"].__setitem__("window_treatment", "black window panes"),
                "warm yellow paper windows",
            ),
            (
                "repeated tiles",
                lambda d: d["generation"].__setitem__("roof_detail", "repeated individual roof tiles"),
                "individual roof tiles",
            ),
            (
                "fine lattice",
                lambda d: d["generation"].__setitem__("window_detail", "fine window lattice"),
                "fine window lattice",
            ),
            (
                "hidden base",
                lambda d: d["generation"].__setitem__("visible_base", False),
                "visible base",
            ),
            (
                "missing entrance",
                lambda d: d["generation"].pop("entrance_direction"),
                "entrance_direction",
            ),
            (
                "not standalone",
                lambda d: d["generation"].__setitem__("output_layout", "contact sheet"),
                "S-A.*single standalone image",
            ),
        )
        for label, mutation, message in cases:
            with self.subTest(label=label):
                self.assert_asset_rejected(mutation, message)


class PromptPacketContractTests(unittest.TestCase):
    def test_canonical_json_is_deterministic_utf8_with_lf(self) -> None:
        first = canonical_json_bytes({"z": "青山", "a": [2, 1]})
        second = canonical_json_bytes({"a": [2, 1], "z": "青山"})

        self.assertEqual(first, second)
        self.assertEqual(first, b'{\n  "a": [\n    2,\n    1\n  ],\n  "z": "\xe9\x9d\x92\xe5\xb1\xb1"\n}\n')
        self.assertNotIn(b"\r\n", first)

    def test_compiler_records_sources_hashes_prompts_contract_and_output(self) -> None:
        style = valid_style()
        asset = valid_asset()
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            style_path = Path("style") / f"{STYLE_ID}.json"
            asset_path = Path("assets") / ASSET_ID / "asset.json"
            style_bytes = write_json(root / style_path, style)
            asset_bytes = write_json(root / asset_path, asset)

            packet = compile_prompt_packet(root, ASSET_ID, "v001")

        self.assertEqual(packet["style_path"], style_path.as_posix())
        self.assertEqual(packet["style_sha256"], hashlib.sha256(style_bytes).hexdigest())
        self.assertEqual(packet["asset_path"], asset_path.as_posix())
        self.assertEqual(packet["asset_sha256"], hashlib.sha256(asset_bytes).hexdigest())
        self.assertEqual(
            packet["positive_prompt"],
            " ".join(style["prompt_contract"]["positive"] + [asset["generation"]["subject"]]),
        )
        self.assertEqual(
            packet["negative_prompt"],
            " ".join(style["prompt_contract"]["negative"] + [asset["negative_prompt"]]),
        )
        self.assertEqual(packet["composition_contract"], style["composition_contract"])
        self.assertEqual(packet["expected_output_filename"], asset["expected_output_filename"])

    def test_same_sources_compile_to_identical_packet_bytes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            write_json(root / "style" / f"{STYLE_ID}.json", valid_style())
            write_json(root / "assets" / ASSET_ID / "asset.json", valid_asset())

            first = canonical_json_bytes(compile_prompt_packet(root, ASSET_ID, "v001"))
            second = canonical_json_bytes(compile_prompt_packet(root, ASSET_ID, "v001"))

        self.assertEqual(first, second)


if __name__ == "__main__":
    unittest.main()
