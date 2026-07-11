from __future__ import annotations

import copy
import hashlib
import json
import os
import struct
import sys
import tempfile
import unittest
import zlib
from functools import lru_cache
from pathlib import Path
from unittest import mock


PROJECT_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT_ROOT / "scripts"))

from qingshan_golden_inn import (  # noqa: E402
    ASSET_ID,
    REQUIRED_VIEWS,
    VERSIONS,
    GoldenInnError,
    build_component_plan,
    load_golden_contract,
    output_paths,
    sha256_file,
    verify_gate1_report,
)


REQUIRED_MODULES = ["BODY", "ROOF_A", "DOOR_A", "WINDOW_A", "EAVE_A"]


def valid_asset_contract() -> dict:
    return {
        "asset_id": "BLD_QS_M_A_INN",
        "target_dimensions_m": [4.8, 5.6, 5.2],
        "building": {
            "golden_contract": {
                "workflow": "golden_sample_gate1",
                "blender_min_version": [4, 2, 0],
                "deformation_percent_range": [5, 10],
                "column_lean_degrees_range": [4, 7],
                "window_frame_ratio_range": [0.08, 0.12],
                "max_window_divisions": 3,
                "max_capital_layers": 2,
                "ground_contact_plane_z_m": 0.0,
                "required_render_views": ["hero_3q", "front", "player_camera"],
                "output_root": "assets/BLD_QS_M_A_INN/source/blender",
            }
        },
    }


def write_json(path: Path, payload: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload), encoding="utf-8")


PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def png_chunk(kind: bytes, data: bytes) -> bytes:
    checksum = zlib.crc32(kind + data) & 0xFFFFFFFF
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", checksum)


def ihdr_chunk(width: int, height: int, *, bit_depth: int = 8, color_type: int = 6) -> bytes:
    data = struct.pack(">IIBBBBB", width, height, bit_depth, color_type, 0, 0, 0)
    return png_chunk(b"IHDR", data)


@lru_cache(maxsize=None)
def minimal_png(
    width: int,
    height: int,
    *,
    bit_depth: int = 8,
    color_type: int = 6,
    include_idat: bool = True,
    include_iend: bool = True,
) -> bytes:
    bytes_per_pixel = 3 if color_type == 2 else 4
    scanline = b"\0" + (b"\0" * (width * bytes_per_pixel))
    chunks = [ihdr_chunk(width, height, bit_depth=bit_depth, color_type=color_type)]
    if include_idat:
        chunks.append(png_chunk(b"IDAT", zlib.compress(scanline * height)))
    if include_iend:
        chunks.append(png_chunk(b"IEND", b""))
    return PNG_SIGNATURE + b"".join(chunks)


BLENDER_DNA_PAYLOAD = b"SDNANAMETYPETLENSTRC"


def blender_header(
    *, pointer_marker: bytes = b"-", endian_marker: bytes = b"v", version: bytes = b"402"
) -> bytes:
    return b"BLENDER" + pointer_marker + endian_marker + version


def blender_bhead(
    code: bytes,
    payload: bytes,
    *,
    pointer_size: int = 8,
    endian: str = "<",
    declared_length: int | None = None,
    sdna_index: int = 0,
    count: int = 1,
) -> bytes:
    pointer_code = "Q" if pointer_size == 8 else "I"
    payload_length = len(payload) if declared_length is None else declared_length
    header = struct.pack(
        f"{endian}4si{pointer_code}ii",
        code,
        payload_length,
        0,
        sdna_index,
        count,
    )
    return header + payload


def minimal_blend(
    *,
    pointer_marker: bytes = b"-",
    endian_marker: bytes = b"v",
    version: bytes = b"402",
    include_dna: bool = True,
    include_endb: bool = True,
    dna_payload: bytes = BLENDER_DNA_PAYLOAD,
    block_pointer_size: int | None = None,
    block_endian: str | None = None,
    trailing: bytes = b"",
) -> bytes:
    pointer_size = block_pointer_size or (8 if pointer_marker == b"-" else 4)
    endian = block_endian or ("<" if endian_marker == b"v" else ">")
    data = blender_header(
        pointer_marker=pointer_marker,
        endian_marker=endian_marker,
        version=version,
    )
    if include_dna:
        data += blender_bhead(
            b"DNA1", dna_payload, pointer_size=pointer_size, endian=endian
        )
    if include_endb:
        data += blender_bhead(
            b"ENDB", b"", pointer_size=pointer_size, endian=endian, count=0
        )
    return data + trailing


class GoldenContractTests(unittest.TestCase):
    def test_loads_contract_and_builds_deterministic_component_plan(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            contract_path = Path(temp_dir) / "asset.json"
            asset = valid_asset_contract()
            write_json(contract_path, asset)

            contract = load_golden_contract(contract_path)
            plan = build_component_plan(contract)

        self.assertEqual(ASSET_ID, "BLD_QS_M_A_INN")
        self.assertEqual(VERSIONS, ("v001", "v002", "v003"))
        self.assertEqual(REQUIRED_VIEWS, ("hero_3q", "front", "player_camera"))
        self.assertEqual(contract, {"asset": asset, "golden": asset["building"]["golden_contract"]})
        self.assertEqual(
            plan,
            {
                "asset_id": "BLD_QS_M_A_INN",
                "dimensions_m": [4.8, 5.6, 5.2],
                "ground_z_m": 0.0,
                "asymmetry": 0.075,
                "facade_offset_m": 0.16,
                "roof_ridge_offset_m": -0.22,
                "window_frame_ratio": 0.10,
                "window_divisions": 2,
                "capital_layers": 1,
                "modules": REQUIRED_MODULES,
                "columns": [
                    {"name": "Column_FL", "x_m": -2.0, "y_m": 2.45, "lean_deg": -5.5},
                    {"name": "Column_FR", "x_m": 1.86, "y_m": 2.45, "lean_deg": 4.5},
                    {"name": "Column_BL", "x_m": -2.0, "y_m": -2.4, "lean_deg": -4.0},
                    {"name": "Column_BR", "x_m": 1.86, "y_m": -2.4, "lean_deg": 6.0},
                ],
            },
        )

    def test_loader_rejects_field_specific_contract_drift(self):
        mutations = [
            ("asset_id", lambda asset: asset.__setitem__("asset_id", "BLD_WRONG")),
            (
                "building.golden_contract",
                lambda asset: asset["building"].__setitem__("golden_contract", []),
            ),
            (
                "target_dimensions_m",
                lambda asset: asset.__setitem__("target_dimensions_m", [4.8, 5.6, 5.1]),
            ),
            (
                "max_window_divisions",
                lambda asset: asset["building"]["golden_contract"].__setitem__(
                    "max_window_divisions", 4
                ),
            ),
            (
                "max_capital_layers",
                lambda asset: asset["building"]["golden_contract"].__setitem__(
                    "max_capital_layers", 3
                ),
            ),
            (
                "deformation_percent_range",
                lambda asset: asset["building"]["golden_contract"].__setitem__(
                    "deformation_percent_range", [4, 10]
                ),
            ),
            (
                "column_lean_degrees_range",
                lambda asset: asset["building"]["golden_contract"].__setitem__(
                    "column_lean_degrees_range", [7, 4]
                ),
            ),
            (
                "window_frame_ratio_range",
                lambda asset: asset["building"]["golden_contract"].__setitem__(
                    "window_frame_ratio_range", [0.12, float("inf")]
                ),
            ),
            (
                "required_render_views",
                lambda asset: asset["building"]["golden_contract"].__setitem__(
                    "required_render_views", ["front", "hero_3q", "player_camera"]
                ),
            ),
            (
                "blender_min_version",
                lambda asset: asset["building"]["golden_contract"].__setitem__(
                    "blender_min_version", [4, 1, 9]
                ),
            ),
            (
                "output_root",
                lambda asset: asset["building"]["golden_contract"].__setitem__(
                    "output_root", "../outside"
                ),
            ),
            (
                "workflow",
                lambda asset: asset["building"]["golden_contract"].__setitem__(
                    "workflow", "not_gate1"
                ),
            ),
        ]
        with tempfile.TemporaryDirectory() as temp_dir:
            contract_path = Path(temp_dir) / "asset.json"
            for expected_field, mutate in mutations:
                with self.subTest(field=expected_field):
                    asset = valid_asset_contract()
                    mutate(asset)
                    write_json(contract_path, asset)
                    with self.assertRaisesRegex(GoldenInnError, expected_field.replace(".", r"\.")):
                        load_golden_contract(contract_path)

    def test_loader_reports_invalid_json_as_golden_inn_error(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            contract_path = Path(temp_dir) / "asset.json"
            contract_path.write_text("{not-json", encoding="utf-8")
            with self.assertRaisesRegex(GoldenInnError, "JSON"):
                load_golden_contract(contract_path)

    def test_loader_reports_non_utf8_json_as_golden_inn_error(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            contract_path = Path(temp_dir) / "asset.json"
            contract_path.write_bytes(b"\xff")
            with self.assertRaisesRegex(GoldenInnError, "JSON"):
                load_golden_contract(contract_path)

    def test_component_plan_rejects_contract_ranges_that_exclude_plan_values(self):
        asset = valid_asset_contract()
        asset["building"]["golden_contract"]["column_lean_degrees_range"] = [6, 7]
        contract = {"asset": asset, "golden": asset["building"]["golden_contract"]}
        with self.assertRaisesRegex(GoldenInnError, "Column_FL.*column_lean_degrees_range"):
            build_component_plan(contract)

    def test_loads_committed_golden_inn_contract(self):
        contract_path = (
            PROJECT_ROOT
            / "SourceAssets/TownPCG/QingshanEnvironment/assets/BLD_QS_M_A_INN/asset.json"
        )
        contract = load_golden_contract(contract_path)
        plan = build_component_plan(contract)
        self.assertEqual(contract["asset"]["asset_id"], ASSET_ID)
        self.assertEqual(contract["golden"]["workflow"], "golden_sample_gate1")
        self.assertEqual(plan["dimensions_m"], [4.8, 5.6, 5.2])

    def test_huge_contract_numbers_raise_golden_inn_error(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            contract_path = Path(temp_dir) / "asset.json"
            asset = valid_asset_contract()
            asset["building"]["golden_contract"]["column_lean_degrees_range"] = [
                4,
                10**400,
            ]
            write_json(contract_path, asset)
            with self.assertRaisesRegex(GoldenInnError, "column_lean_degrees_range"):
                load_golden_contract(contract_path)

    def test_contract_path_type_error_is_normalized(self):
        with self.assertRaisesRegex(GoldenInnError, "contract path"):
            load_golden_contract(None)  # type: ignore[arg-type]
        with self.assertRaisesRegex(GoldenInnError, "contract path"):
            load_golden_contract("\0")


class OutputPathTests(unittest.TestCase):
    def test_output_paths_are_exact_and_project_relative(self):
        project_root = Path("D:/Example/GameXXK")
        base = (
            project_root
            / "SourceAssets/TownPCG/QingshanEnvironment/assets/BLD_QS_M_A_INN/source/blender"
        )
        self.assertEqual(
            output_paths(project_root, "v002"),
            {
                "blend": base / "BLD_QS_M_A_INN__golden__v002.blend",
                "hero_3q": base / "BLD_QS_M_A_INN__hero_3q__v002.png",
                "front": base / "BLD_QS_M_A_INN__front__v002.png",
                "player_camera": base / "BLD_QS_M_A_INN__player_camera__v002.png",
                "silhouette_128": base / "BLD_QS_M_A_INN__silhouette_128__v002.png",
                "report": base / "BLD_QS_M_A_INN__gate1_report__v002.json",
            },
        )

    def test_output_paths_reject_unknown_or_unsafe_versions(self):
        for version in ("v004", "../v001", "v001/../../escape", "C:/escape/v001"):
            with self.subTest(version=version):
                with self.assertRaisesRegex(GoldenInnError, "version"):
                    output_paths(Path("D:/Example/GameXXK"), version)


class GateOneReportTests(unittest.TestCase):
    def _seed_outputs(self, root: Path, version: str = "v001") -> dict[str, Path]:
        paths = output_paths(root, version)
        for key, path in paths.items():
            if key != "report":
                path.parent.mkdir(parents=True, exist_ok=True)
                if key == "blend":
                    path.write_bytes(minimal_blend())
                elif key == "silhouette_128":
                    path.write_bytes(minimal_png(128, 128))
                else:
                    path.write_bytes(minimal_png(1536, 1024))
        return paths

    def _valid_report(self, paths: dict[str, Path], version: str = "v001") -> dict:
        return {
            "asset_id": ASSET_ID,
            "version": version,
            "blender_version": [4, 2, 0],
            "render_views": list(REQUIRED_VIEWS),
            "ground_min_z_m": 0.0005,
            "object_count": 12,
            "mesh_object_count": 10,
            "faces": {"triangles": 0, "quads": 32_000, "ngons": 0},
            "non_manifold_edges": 0,
            "negative_scale_objects": [],
            "unapplied_transform_objects": [],
            "module_material_counts": {
                module: 2 if module in {"BODY", "ROOF_A"} else 1
                for module in REQUIRED_MODULES
            },
            "capital_layers": 1,
            "window_divisions": 2,
            "bounds_m": {
                "min": [-2.4, -2.8, 0.0005],
                "max": [2.4, 2.8, 5.2005],
                "size": [4.8, 5.6, 5.2],
            },
            "output_sha256": {
                key: sha256_file(path) for key, path in paths.items() if key != "report"
            },
        }

    def test_valid_report_returns_normalized_summary_and_verifies_hashes(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            paths = self._seed_outputs(Path(temp_dir))
            report = self._valid_report(paths)
            write_json(paths["report"], report)

            summary = verify_gate1_report(paths["report"], paths)

        self.assertEqual(
            summary,
            {
                "ok": True,
                "asset_id": ASSET_ID,
                "version": "v001",
                "views": list(REQUIRED_VIEWS),
                "modules": REQUIRED_MODULES,
                "bounds_m": [4.8, 5.6, 5.2],
                "sha256_verified": True,
            },
        )

    def test_blend_parser_accepts_declared_32bit_big_endian_blocks(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            paths = self._seed_outputs(Path(temp_dir))
            paths["blend"].write_bytes(
                minimal_blend(pointer_marker=b"_", endian_marker=b"V")
            )
            write_json(paths["report"], self._valid_report(paths))
            self.assertTrue(verify_gate1_report(paths["report"], paths)["ok"])

    def test_missing_output_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            paths = self._seed_outputs(Path(temp_dir))
            write_json(paths["report"], self._valid_report(paths))
            paths["front"].unlink()
            with self.assertRaisesRegex(GoldenInnError, "missing output.*front"):
                verify_gate1_report(paths["report"], paths)

    def test_missing_report_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            paths = self._seed_outputs(Path(temp_dir))
            with self.assertRaisesRegex(GoldenInnError, "missing report"):
                verify_gate1_report(paths["report"], paths)

    def test_report_verifier_rejects_canonical_names_outside_output_root(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            canonical = output_paths(Path(temp_dir), "v001")
            flat_root = Path(temp_dir) / "escaped"
            paths = {key: flat_root / path.name for key, path in canonical.items()}
            for key, path in paths.items():
                if key != "report":
                    path.parent.mkdir(parents=True, exist_ok=True)
                    if key == "blend":
                        path.write_bytes(minimal_blend())
                    elif key == "silhouette_128":
                        path.write_bytes(minimal_png(128, 128))
                    else:
                        path.write_bytes(minimal_png(1536, 1024))
            write_json(paths["report"], self._valid_report(paths))
            with self.assertRaisesRegex(GoldenInnError, "canonical output_root"):
                verify_gate1_report(paths["report"], paths)

    def test_canonical_output_root_uses_platform_native_case_rules(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            canonical = self._seed_outputs(root)
            lowercase_parent = (
                root
                / "sourceassets/TownPCG/QingshanEnvironment/assets/"
                "BLD_QS_M_A_INN/source/blender"
            )
            paths = {key: lowercase_parent / path.name for key, path in canonical.items()}
            for key, path in paths.items():
                if key != "report":
                    content = canonical[key].read_bytes()
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_bytes(content)
            write_json(paths["report"], self._valid_report(paths))
            with mock.patch.object(os.path, "normcase", side_effect=lambda value: value):
                with self.assertRaisesRegex(GoldenInnError, "canonical output_root"):
                    verify_gate1_report(paths["report"], paths)

    def test_bad_gate_fields_are_rejected(self):
        mutations = [
            ("asset_id", lambda report: report.__setitem__("asset_id", "BLD_WRONG")),
            ("version", lambda report: report.__setitem__("version", "v002")),
            (
                "render_views",
                lambda report: report.__setitem__("render_views", ["hero_3q", "front"]),
            ),
            ("ground_min_z_m", lambda report: report.__setitem__("ground_min_z_m", 0.01)),
            (
                "non_manifold_edges",
                lambda report: report.__setitem__("non_manifold_edges", 1),
            ),
            (
                "negative_scale_objects",
                lambda report: report.__setitem__("negative_scale_objects", ["BODY"]),
            ),
            (
                "unapplied_transform_objects",
                lambda report: report.__setitem__("unapplied_transform_objects", ["ROOF_A"]),
            ),
            (
                "missing modules",
                lambda report: report["module_material_counts"].pop("EAVE_A"),
            ),
            (
                "module_material_counts.*WINDOW_A",
                lambda report: report["module_material_counts"].__setitem__("WINDOW_A", 3),
            ),
            (
                "module_material_counts.*EAVE_A",
                lambda report: report["module_material_counts"].__setitem__("EAVE_A", 0),
            ),
            ("capital_layers", lambda report: report.__setitem__("capital_layers", 2)),
            ("capital_layers", lambda report: report.__setitem__("capital_layers", 1.0)),
            ("window_divisions", lambda report: report.__setitem__("window_divisions", 3)),
            ("window_divisions", lambda report: report.__setitem__("window_divisions", 2.0)),
            (
                "faces.*quads",
                lambda report: report["faces"].__setitem__("quads", 29_999),
            ),
            (
                "faces.*quads",
                lambda report: report["faces"].__setitem__("quads", 35_001),
            ),
            (
                "faces.*quad-dominant",
                lambda report: report["faces"].__setitem__("triangles", 32_000),
            ),
            (
                "bounds_m",
                lambda report: report["bounds_m"].__setitem__("size", [4.8, 0.0, 5.2]),
            ),
            (
                "bounds_m",
                lambda report: report["bounds_m"]["max"].__setitem__(0, 2.5),
            ),
            (
                "bounds_m",
                lambda report: (
                    report["bounds_m"]["min"].__setitem__(2, 0.0004),
                    report["bounds_m"]["max"].__setitem__(2, 5.2004),
                ),
            ),
            (
                "bottom_center",
                lambda report: (
                    report["bounds_m"]["min"].__setitem__(0, -2.38),
                    report["bounds_m"]["max"].__setitem__(0, 2.42),
                ),
            ),
            (
                "output_sha256.*blend",
                lambda report: report["output_sha256"].__setitem__("blend", "0" * 64),
            ),
        ]
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            for expected_error, mutate in mutations:
                with self.subTest(field=expected_error):
                    paths = self._seed_outputs(root)
                    report = copy.deepcopy(self._valid_report(paths))
                    mutate(report)
                    write_json(paths["report"], report)
                    with self.assertRaisesRegex(GoldenInnError, expected_error):
                        verify_gate1_report(paths["report"], paths)

    def test_hashes_are_required_and_must_cover_every_output(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            paths = self._seed_outputs(Path(temp_dir))
            report = self._valid_report(paths)
            report.pop("output_sha256")
            write_json(paths["report"], report)
            with self.assertRaisesRegex(GoldenInnError, "output_sha256.*required"):
                verify_gate1_report(paths["report"], paths)

            report["output_sha256"] = {"blend": hashlib.sha256(paths["blend"].read_bytes()).hexdigest()}
            write_json(paths["report"], report)
            with self.assertRaisesRegex(GoldenInnError, "output_sha256.*missing"):
                verify_gate1_report(paths["report"], paths)

    def test_artifacts_require_nonempty_valid_blender_and_png_headers(self):
        valid_main_png = minimal_png(1536, 1024)
        idat_length = struct.unpack(">I", valid_main_png[33:37])[0]
        idat_data = valid_main_png[41 : 41 + idat_length]
        trailing_zlib_stream = (
            PNG_SIGNATURE
            + ihdr_chunk(1536, 1024)
            + png_chunk(b"IDAT", idat_data + zlib.compress(b"extra stream"))
            + png_chunk(b"IEND", b"")
        )
        split = len(idat_data) // 2
        nonconsecutive_idat = (
            PNG_SIGNATURE
            + ihdr_chunk(1536, 1024)
            + png_chunk(b"IDAT", idat_data[:split])
            + png_chunk(b"tEXt", b"interrupt")
            + png_chunk(b"IDAT", idat_data[split:])
            + png_chunk(b"IEND", b"")
        )
        bad_crc = bytearray(minimal_png(1536, 1024))
        bad_crc[-1] ^= 1
        duplicate_ihdr = (
            minimal_png(1536, 1024)[:33]
            + ihdr_chunk(1536, 1024)
            + minimal_png(1536, 1024)[33:]
        )
        duplicate_dna = (
            blender_header()
            + blender_bhead(b"DNA1", BLENDER_DNA_PAYLOAD)
            + blender_bhead(b"DNA1", BLENDER_DNA_PAYLOAD)
            + blender_bhead(b"ENDB", b"", count=0)
        )
        negative_block_length = (
            blender_header()
            + blender_bhead(b"TEST", b"", declared_length=-1)
            + blender_bhead(b"DNA1", BLENDER_DNA_PAYLOAD)
            + blender_bhead(b"ENDB", b"", count=0)
        )
        nonzero_endb = (
            blender_header()
            + blender_bhead(b"DNA1", BLENDER_DNA_PAYLOAD)
            + blender_bhead(b"ENDB", b"x", count=0)
        )
        mutations = [
            ("blend.*empty", "blend", b""),
            ("blend.*12-byte", "blend", b"BLENDER"),
            ("blend.*block", "blend", blender_header()),
            ("blend.*block", "blend", b"BLENDER-v402x"),
            ("blend.*version.*4.2", "blend", minimal_blend(version=b"401")),
            ("blend.*pointer", "blend", b"BLENDER?v300payload"),
            ("blend.*endian", "blend", b"BLENDER-x402payload"),
            ("blend.*version", "blend", b"BLENDER-v3x0payload"),
            ("blend.*BLENDER", "blend", b"not-a-blend"),
            ("blend.*DNA1", "blend", minimal_blend(include_dna=False)),
            ("blend.*ENDB", "blend", minimal_blend(include_endb=False)),
            ("blend.*truncated", "blend", minimal_blend()[:-3]),
            ("blend.*negative", "blend", negative_block_length),
            (
                "blend.*(framing|DNA1)",
                "blend",
                minimal_blend(block_pointer_size=4),
            ),
            (
                "blend.*framing",
                "blend",
                minimal_blend(block_endian=">"),
            ),
            ("blend.*DNA1.*exactly once", "blend", duplicate_dna),
            (
                "blend.*DNA1.*SDNA",
                "blend",
                minimal_blend(dna_payload=b"not-sdna"),
            ),
            (
                "blend.*DNA1.*NAME.*TYPE.*TLEN.*STRC",
                "blend",
                minimal_blend(dna_payload=b"SDNASTRCTLENTYPENAME"),
            ),
            ("blend.*ENDB.*zero", "blend", nonzero_endb),
            ("blend.*ENDB.*terminal", "blend", minimal_blend(trailing=b"x")),
            ("front.*PNG signature", "front", b"not-a-png"),
            ("hero_3q.*IHDR", "hero_3q", b"\x89PNG\r\n\x1a\ntruncated"),
            ("player_camera.*dimensions", "player_camera", minimal_png(0, 1024)),
            ("player_camera.*1536x1024", "player_camera", minimal_png(1, 1)),
            ("silhouette_128.*128x128", "silhouette_128", minimal_png(127, 128)),
            (
                "front.*IDAT",
                "front",
                minimal_png(1536, 1024, include_idat=False),
            ),
            (
                "front.*IEND",
                "front",
                minimal_png(1536, 1024, include_iend=False),
            ),
            ("hero_3q.*CRC", "hero_3q", bytes(bad_crc)),
            ("front.*truncated", "front", minimal_png(1536, 1024)[:-2]),
            ("hero_3q.*IHDR.*exactly once", "hero_3q", duplicate_ihdr),
            (
                "front.*IHDR format",
                "front",
                minimal_png(1536, 1024, bit_depth=16),
            ),
            ("front.*terminal", "front", minimal_png(1536, 1024) + b"trailing"),
            ("front.*IDAT.*compressed", "front", trailing_zlib_stream),
            ("front.*IDAT.*consecutive", "front", nonconsecutive_idat),
        ]
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            for expected_error, key, content in mutations:
                with self.subTest(key=key, expected_error=expected_error):
                    paths = self._seed_outputs(root)
                    paths[key].write_bytes(content)
                    report = self._valid_report(paths)
                    write_json(paths["report"], report)
                    with self.assertRaisesRegex(GoldenInnError, expected_error):
                        verify_gate1_report(paths["report"], paths)

    def test_resolved_artifact_symlink_cannot_escape_output_root(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            paths = self._seed_outputs(root)
            external = root / "external-front.png"
            external.write_bytes(minimal_png(1536, 1024))
            external_resolved = external.resolve()
            paths["front"].unlink()
            resolve_patch = None
            try:
                paths["front"].symlink_to(external)
            except OSError:
                # Windows without Developer Mode cannot create symlinks. Simulate
                # exactly the escaped result Path.resolve() would return.
                paths["front"].write_bytes(external.read_bytes())
                path_type = type(paths["front"])
                original_resolve = path_type.resolve

                def escaped_resolve(candidate, *args, **kwargs):
                    if candidate == paths["front"]:
                        return external_resolved
                    return original_resolve(candidate, *args, **kwargs)

                resolve_patch = mock.patch.object(path_type, "resolve", escaped_resolve)
            report = self._valid_report(paths)
            write_json(paths["report"], report)
            if resolve_patch is None:
                with self.assertRaisesRegex(GoldenInnError, "front.*escapes canonical output_root"):
                    verify_gate1_report(paths["report"], paths)
            else:
                with resolve_patch:
                    with self.assertRaisesRegex(
                        GoldenInnError, "front.*escapes canonical output_root"
                    ):
                        verify_gate1_report(paths["report"], paths)

    def test_resolved_canonical_ancestor_cannot_escape_project_root(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            paths = self._seed_outputs(root)
            write_json(paths["report"], self._valid_report(paths))
            suffix = Path(
                "SourceAssets/TownPCG/QingshanEnvironment/assets/"
                "BLD_QS_M_A_INN/source/blender"
            )
            escaped_parent = root.parent / f"{root.name}-escaped" / suffix
            canonical_parent = paths["blend"].parent
            escaped_files = {path: escaped_parent / path.name for path in paths.values()}
            path_type = type(paths["blend"])
            original_resolve = path_type.resolve

            def escaped_ancestor_resolve(candidate, *args, **kwargs):
                if candidate == canonical_parent:
                    return escaped_parent
                if candidate in escaped_files:
                    return escaped_files[candidate]
                return original_resolve(candidate, *args, **kwargs)

            with mock.patch.object(path_type, "resolve", escaped_ancestor_resolve):
                with self.assertRaisesRegex(GoldenInnError, "escapes resolved project_root"):
                    verify_gate1_report(paths["report"], paths)

    def test_public_path_type_errors_are_normalized(self):
        class BadPathLike:
            def __fspath__(self):
                raise OSError("broken path-like")

        with self.assertRaisesRegex(GoldenInnError, "project_root"):
            output_paths(None, "v001")  # type: ignore[arg-type]
        with self.assertRaisesRegex(GoldenInnError, "project_root"):
            output_paths(BadPathLike(), "v001")  # type: ignore[arg-type]
        with tempfile.TemporaryDirectory() as temp_dir:
            paths = self._seed_outputs(Path(temp_dir))
            write_json(paths["report"], self._valid_report(paths))
            paths["front"] = object()  # type: ignore[assignment]
            with self.assertRaisesRegex(GoldenInnError, "paths.front"):
                verify_gate1_report(paths["report"], paths)

            paths = self._seed_outputs(Path(temp_dir))
            write_json(paths["report"], self._valid_report(paths))
            paths[object()] = paths["front"]  # type: ignore[index]
            with self.assertRaisesRegex(GoldenInnError, "paths.*unexpected"):
                verify_gate1_report(paths["report"], paths)

        with self.assertRaisesRegex(GoldenInnError, "hash path"):
            sha256_file(None)  # type: ignore[arg-type]
        with self.assertRaisesRegex(GoldenInnError, "hash path"):
            sha256_file("\0")
        with self.assertRaisesRegex(GoldenInnError, "project_root"):
            output_paths("\0", "v001")

    def test_huge_report_numbers_raise_golden_inn_error(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            paths = self._seed_outputs(Path(temp_dir))
            report = self._valid_report(paths)
            report["ground_min_z_m"] = 10**400
            write_json(paths["report"], report)
            with self.assertRaisesRegex(GoldenInnError, "ground_min_z_m"):
                verify_gate1_report(paths["report"], paths)


if __name__ == "__main__":
    unittest.main()
