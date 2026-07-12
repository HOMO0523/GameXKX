"""Contract tests for the Qingshan building concept prompt compiler."""

from __future__ import annotations

import hashlib
import tempfile
import unittest
from pathlib import Path

from scripts.qingshan_building_concepts import (
    ConceptError,
    canonical_json_bytes,
    compile_prompt_packet,
    validate_building_style,
    validate_golden_asset,
)


STYLE_ID = "QS_InkToon_Building_v2"
ASSET_ID = "BLD_QS_S_A_HOUSE"


def valid_style() -> dict:
    return {
        "schema_version": 1,
        "style_id": STYLE_ID,
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
            "window_fill": "warm_yellow_translucent_rice_paper",
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
    payload = canonical_json_bytes(data)
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
