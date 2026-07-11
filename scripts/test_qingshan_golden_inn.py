from __future__ import annotations

import argparse
import ast
import copy
import hashlib
import io
import json
import os
import subprocess
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
BLENDER_EXE = Path("D:/Blender/blender.exe")


class BlenderBuilderStructureTests(unittest.TestCase):
    @staticmethod
    def _builder_tree() -> tuple[str, ast.Module]:
        builder_path = PROJECT_ROOT / "scripts" / "blender" / "build_qingshan_golden_inn.py"
        source = builder_path.read_text(encoding="utf-8")
        return source, ast.parse(source, filename=str(builder_path))

    def test_builder_exposes_required_blender_entry_points_and_cli_boundary(self):
        builder_path = PROJECT_ROOT / "scripts" / "blender" / "build_qingshan_golden_inn.py"
        self.assertTrue(builder_path.is_file(), f"missing Blender builder: {builder_path}")

        source, tree = self._builder_tree()
        functions = {
            node.name for node in ast.walk(tree) if isinstance(node, ast.FunctionDef)
        }
        self.assertTrue(
            {
                "create_tapered_box",
                "create_beam_between",
                "create_curved_roof",
                "create_chunky_window",
                "create_single_capital",
                "create_fixed_cameras",
                "audit_scene",
                "main",
            }.issubset(functions)
        )

        imported_modules = {
            alias.name
            for node in ast.walk(tree)
            if isinstance(node, ast.Import)
            for alias in node.names
        }
        imported_modules.update(
            node.module or ""
            for node in ast.walk(tree)
            if isinstance(node, ast.ImportFrom)
        )
        self.assertTrue({"argparse", "bpy", "qingshan_golden_inn"}.issubset(imported_modules))
        helper_imports = {
            alias.name
            for node in ast.walk(tree)
            if isinstance(node, ast.ImportFrom) and node.module == "qingshan_golden_inn"
            for alias in node.names
        }
        required_helpers = {
            "load_golden_contract",
            "build_component_plan",
            "output_paths",
            "sha256_file",
        }
        self.assertTrue(required_helpers.issubset(helper_imports))
        called_helpers = {
            node.func.id
            for node in ast.walk(tree)
            if isinstance(node, ast.Call) and isinstance(node.func, ast.Name)
        }
        self.assertTrue(required_helpers.issubset(called_helpers))
        self.assertFalse(
            any(
                module.lower().replace("_", "").split(".", maxsplit=1)[0]
                in {"ue", "ue5", "unreal", "unrealbridge"}
                for module in imported_modules
            )
        )
        self.assertTrue({"importlib", "subprocess"}.isdisjoint(imported_modules))
        self.assertTrue({"__import__", "eval", "exec"}.isdisjoint(called_helpers))

        string_literals = {
            node.value
            for node in ast.walk(tree)
            if isinstance(node, ast.Constant) and isinstance(node.value, str)
        }
        self.assertIn("--", string_literals, "builder must parse Blender args after `--`")

    def test_runtime_guard_rejects_foreground_and_pre_42_blender(self):
        _, tree = self._builder_tree()
        functions = {
            node.name: node for node in tree.body if isinstance(node, ast.FunctionDef)
        }
        self.assertIn("_require_supported_background_blender", functions)
        module = ast.Module(
            body=[functions["_require_supported_background_blender"]], type_ignores=[]
        )
        ast.fix_missing_locations(module)
        fake_bpy = mock.Mock()
        namespace = {"bpy": fake_bpy}
        exec(compile(module, "<blender-runtime-guard>", "exec"), namespace)
        guard = namespace["_require_supported_background_blender"]

        fake_bpy.app.background = False
        fake_bpy.app.version = (4, 2, 3)
        with self.assertRaisesRegex(RuntimeError, "background"):
            guard()

        fake_bpy.app.background = True
        fake_bpy.app.version = (4, 1, 9)
        with self.assertRaisesRegex(RuntimeError, "4.2"):
            guard()

        fake_bpy.app.version = (4, 2, 0)
        guard()

    def test_output_validation_rejects_escape_and_file_ancestor_before_reset(self):
        _, tree = self._builder_tree()
        functions = {
            node.name: node for node in tree.body if isinstance(node, ast.FunctionDef)
        }
        self.assertIn("_validate_output_destinations", functions)
        module = ast.Module(
            body=[functions["_validate_output_destinations"]], type_ignores=[]
        )
        ast.fix_missing_locations(module)
        namespace = {
            "Path": Path,
            "CANONICAL_OUTPUT_RELATIVE": Path(
                "SourceAssets/TownPCG/QingshanEnvironment/assets/"
                "BLD_QS_M_A_INN/source/blender"
            ),
        }
        exec(compile(module, "<output-containment>", "exec"), namespace)
        validate = namespace["_validate_output_destinations"]

        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir).resolve()
            paths = output_paths(root, "v001")
            resolved_root, validated = validate(root, paths)
            self.assertEqual(resolved_root, root)
            self.assertEqual(validated, paths)

            escaped = dict(paths)
            escaped["front"] = root.parent / paths["front"].name
            with self.assertRaisesRegex(RuntimeError, "canonical output"):
                validate(root, escaped)

        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir).resolve()
            (root / "SourceAssets").write_text("not a directory", encoding="utf-8")
            with self.assertRaisesRegex(RuntimeError, "ancestor.*directory"):
                validate(root, output_paths(root, "v001"))

        main_calls = [
            node
            for node in ast.walk(functions["main"])
            if isinstance(node, ast.Call)
        ]
        guard_line = next(
            node.lineno
            for node in main_calls
            if isinstance(node.func, ast.Name)
            and node.func.id == "_require_supported_background_blender"
        )
        validate_line = next(
            node.lineno
            for node in main_calls
            if isinstance(node.func, ast.Name)
            and node.func.id == "_validate_output_destinations"
        )
        reset_line = next(
            node.lineno
            for node in main_calls
            if isinstance(node.func, ast.Name) and node.func.id == "_reset_and_build"
        )
        mkdir_lines = [
            node.lineno
            for node in main_calls
            if isinstance(node.func, ast.Attribute) and node.func.attr == "mkdir"
        ]
        self.assertLess(guard_line, validate_line)
        self.assertLess(validate_line, reset_line)
        self.assertTrue(mkdir_lines)
        self.assertLess(validate_line, min(mkdir_lines))

    def test_output_validation_rejects_resolved_symlink_or_junction_redirect(self):
        _, tree = self._builder_tree()
        functions = {
            node.name: node for node in tree.body if isinstance(node, ast.FunctionDef)
        }
        self.assertIn("_validate_output_destinations", functions)
        module = ast.Module(
            body=[functions["_validate_output_destinations"]], type_ignores=[]
        )
        ast.fix_missing_locations(module)
        namespace = {
            "Path": Path,
            "CANONICAL_OUTPUT_RELATIVE": Path(
                "SourceAssets/TownPCG/QingshanEnvironment/assets/"
                "BLD_QS_M_A_INN/source/blender"
            ),
        }
        exec(compile(module, "<output-containment>", "exec"), namespace)
        validate = namespace["_validate_output_destinations"]

        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir).resolve()
            paths = output_paths(root, "v001")
            paths["front"].parent.mkdir(parents=True)
            paths["front"].write_bytes(b"old")
            external = root.parent / f"{root.name}-escaped-front.png"
            original_resolve = type(paths["front"]).resolve

            def redirected_resolve(candidate, *args, **kwargs):
                if candidate == paths["front"]:
                    return external
                return original_resolve(candidate, *args, **kwargs)

            with mock.patch.object(type(paths["front"]), "resolve", redirected_resolve):
                with self.assertRaisesRegex(RuntimeError, "redirect"):
                    validate(root, paths)

    def test_prior_outputs_are_invalidated_before_build_with_scoped_sidecars(self):
        _, tree = self._builder_tree()
        functions = {
            node.name: node for node in tree.body if isinstance(node, ast.FunctionDef)
        }
        self.assertIn("_invalidate_previous_outputs", functions)
        module = ast.Module(
            body=[functions["_invalidate_previous_outputs"]], type_ignores=[]
        )
        ast.fix_missing_locations(module)
        namespace = {"Path": Path}
        exec(compile(module, "<output-invalidation>", "exec"), namespace)
        invalidate = namespace["_invalidate_previous_outputs"]

        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir).resolve()
            paths = output_paths(root, "v001")
            for path in paths.values():
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b"stale")
            blend_backup = paths["blend"].with_name(f"{paths['blend'].name}1")
            report_temp = paths["report"].with_name(f"{paths['report'].name}.tmp")
            unrelated = paths["report"].parent / "preserve.txt"
            blend_backup.write_bytes(b"backup")
            report_temp.write_bytes(b"partial")
            unrelated.write_bytes(b"preserve")

            invalidate(paths)

            self.assertTrue(all(not path.exists() for path in paths.values()))
            self.assertFalse(blend_backup.exists())
            self.assertFalse(report_temp.exists())
            self.assertEqual(unrelated.read_bytes(), b"preserve")

        calls = [
            node
            for node in ast.walk(functions["main"])
            if isinstance(node, ast.Call) and isinstance(node.func, ast.Name)
        ]
        invalidate_line = next(
            node.lineno
            for node in calls
            if node.func.id == "_invalidate_previous_outputs"
        )
        reset_line = next(
            node.lineno for node in calls if node.func.id == "_reset_and_build"
        )
        self.assertLess(invalidate_line, reset_line)

    def test_render_and_save_results_must_finish_before_current_run_hashing(self):
        _, tree = self._builder_tree()
        functions = {
            node.name: node for node in tree.body if isinstance(node, ast.FunctionDef)
        }
        self.assertIn("_require_finished", functions)
        self.assertIn("_assert_current_run_outputs", functions)
        module = ast.Module(body=[functions["_require_finished"]], type_ignores=[])
        ast.fix_missing_locations(module)
        namespace: dict = {}
        exec(compile(module, "<operator-result>", "exec"), namespace)
        require_finished = namespace["_require_finished"]
        require_finished({"FINISHED"}, "render")
        with self.assertRaisesRegex(RuntimeError, "render.*CANCELLED"):
            require_finished({"CANCELLED"}, "render")

        render_calls = {
            node.func.id
            for node in ast.walk(functions["_render_view"])
            if isinstance(node, ast.Call) and isinstance(node.func, ast.Name)
        }
        self.assertIn("_require_finished", render_calls)

        main_calls = [
            node for node in ast.walk(functions["main"]) if isinstance(node, ast.Call)
        ]
        save_line = next(
            node.lineno
            for node in main_calls
            if isinstance(node.func, ast.Attribute) and node.func.attr == "save_as_mainfile"
        )
        require_lines = [
            node.lineno
            for node in main_calls
            if isinstance(node.func, ast.Name) and node.func.id == "_require_finished"
        ]
        current_line = next(
            node.lineno
            for node in main_calls
            if isinstance(node.func, ast.Name)
            and node.func.id == "_assert_current_run_outputs"
        )
        hash_line = next(
            node.lineno
            for node in main_calls
            if isinstance(node.func, ast.Name) and node.func.id == "sha256_file"
        )
        self.assertTrue(any(line > save_line for line in require_lines))
        self.assertLess(current_line, hash_line)

    def test_dense_curved_roof_enables_smooth_surface_shading(self):
        _, tree = self._builder_tree()
        functions = {
            node.name: node for node in tree.body if isinstance(node, ast.FunctionDef)
        }
        self.assertIn("create_curved_roof", functions)
        roof_function = functions["create_curved_roof"]
        self.assertTrue(
            any(
                isinstance(node, ast.Assign)
                and any(
                    isinstance(target, ast.Attribute) and target.attr == "use_smooth"
                    for target in node.targets
                )
                and isinstance(node.value, ast.Constant)
                and node.value.value is True
                for node in ast.walk(roof_function)
            ),
            "the visible high-resolution roof must not expose flat grid facets",
        )

    def test_main_renders_enable_dark_freestyle_ink_lines(self):
        _, tree = self._builder_tree()
        functions = {
            node.name: node for node in tree.body if isinstance(node, ast.FunctionDef)
        }
        configure = functions["_configure_scene"]
        freestyle_true_targets = [
            target
            for node in ast.walk(configure)
            if isinstance(node, ast.Assign)
            and isinstance(node.value, ast.Constant)
            and node.value.value is True
            for target in node.targets
            if isinstance(target, ast.Attribute) and target.attr == "use_freestyle"
        ]
        self.assertGreaterEqual(len(freestyle_true_targets), 2)
        configure_attributes = {
            node.attr for node in ast.walk(configure) if isinstance(node, ast.Attribute)
        }
        self.assertTrue(
            {
                "freestyle_settings",
                "linestyle",
                "color",
                "thickness",
                "select_silhouette",
                "select_crease",
            }.issubset(configure_attributes)
        )

    def test_block_primitives_apply_small_bevel_but_dense_roof_does_not(self):
        _, tree = self._builder_tree()
        functions = {
            node.name: node for node in tree.body if isinstance(node, ast.FunctionDef)
        }
        assignments = {
            target.id: node.value
            for node in tree.body
            if isinstance(node, ast.Assign)
            for target in node.targets
            if isinstance(target, ast.Name)
        }
        self.assertIn("SMALL_BEVEL_WIDTH_M", assignments)
        bevel_width = ast.literal_eval(assignments["SMALL_BEVEL_WIDTH_M"])
        self.assertGreaterEqual(bevel_width, 0.005)
        self.assertLessEqual(bevel_width, 0.015)
        self.assertIn("_apply_small_bevel", functions)

        bevel_calls = [
            node
            for node in ast.walk(functions["_apply_small_bevel"])
            if isinstance(node, ast.Call)
        ]
        self.assertTrue(
            any(
                isinstance(node.func, ast.Attribute)
                and node.func.attr == "modifier_apply"
                for node in bevel_calls
            )
        )
        self.assertTrue(
            any(
                isinstance(node.func, ast.Name) and node.func.id == "_require_finished"
                for node in bevel_calls
            )
        )
        mesh_factory_calls = {
            node.func.id
            for node in ast.walk(functions["_create_mesh_object"])
            if isinstance(node, ast.Call) and isinstance(node.func, ast.Name)
        }
        self.assertIn("_apply_small_bevel", mesh_factory_calls)

        for primitive_name in ("create_tapered_box", "create_beam_between"):
            mesh_calls = [
                node
                for node in ast.walk(functions[primitive_name])
                if isinstance(node, ast.Call)
                and isinstance(node.func, ast.Name)
                and node.func.id == "_create_mesh_object"
            ]
            self.assertEqual(len(mesh_calls), 1)
            self.assertIn("bevel_width", {keyword.arg for keyword in mesh_calls[0].keywords})

        roof_names = {
            node.id
            for node in ast.walk(functions["create_curved_roof"])
            if isinstance(node, ast.Name)
        }
        self.assertNotIn("SMALL_BEVEL_WIDTH_M", roof_names)

    def test_silhouette_is_emission_black_on_white_and_restores_render_state(self):
        _, tree = self._builder_tree()
        functions = {
            node.name: node for node in tree.body if isinstance(node, ast.FunctionDef)
        }
        self.assertIn("_make_emission_material", functions)
        emission_literals = {
            node.value
            for node in ast.walk(functions["_make_emission_material"])
            if isinstance(node, ast.Constant) and isinstance(node.value, str)
        }
        self.assertIn("ShaderNodeEmission", emission_literals)

        silhouette = functions["_render_silhouette"]
        called_names = {
            node.func.id
            for node in ast.walk(silhouette)
            if isinstance(node, ast.Call) and isinstance(node.func, ast.Name)
        }
        self.assertIn("_make_emission_material", called_names)
        self.assertNotIn("_make_material", called_names)
        tuple_literals = {
            tuple(ast.literal_eval(node))
            for node in ast.walk(silhouette)
            if isinstance(node, ast.Tuple)
            and all(isinstance(item, ast.Constant) for item in node.elts)
        }
        self.assertIn((0.0, 0.0, 0.0, 1.0), tuple_literals)
        self.assertIn((1.0, 1.0, 1.0, 1.0), tuple_literals)

        try_node = next(node for node in ast.walk(silhouette) if isinstance(node, ast.Try))
        final_names = {
            node.id for node in ast.walk(ast.Module(body=try_node.finalbody)) if isinstance(node, ast.Name)
        }
        self.assertTrue(
            {
                "saved_camera",
                "saved_filepath",
                "saved_resolution",
                "saved_film_transparent",
                "saved_freestyle",
                "saved_world",
                "saved_materials",
                "saved_view_settings",
            }.issubset(final_names)
        )

    def test_silhouette_pixel_validator_rejects_color_border_alpha_and_trivial_masks(self):
        _, tree = self._builder_tree()
        validator = next(
            node
            for node in tree.body
            if isinstance(node, ast.FunctionDef)
            and node.name == "_validate_silhouette_pixels"
        )
        isolated = copy.deepcopy(validator)
        isolated.returns = None
        for argument in (*isolated.args.posonlyargs, *isolated.args.args):
            argument.annotation = None
        namespace: dict[str, object] = {}
        module = ast.fix_missing_locations(ast.Module(body=[isolated], type_ignores=[]))
        exec(compile(module, "<silhouette-validator>", "exec"), namespace)
        validate = namespace["_validate_silhouette_pixels"]

        def flattened(rows: list[list[tuple[float, float, float, float]]]) -> list[float]:
            return [component for row in rows for pixel in row for component in pixel]

        white = (1.0, 1.0, 1.0, 1.0)
        black = (0.0, 0.0, 0.0, 1.0)
        gray = (0.45, 0.45, 0.45, 1.0)
        valid_rows = [[white for _ in range(5)] for _ in range(5)]
        valid_rows[1][1] = black
        valid_rows[1][2] = black
        valid_rows[2][1] = black
        valid_rows[2][2] = black
        valid_rows[3][3] = gray
        summary = validate(flattened(valid_rows), 5, 5)
        self.assertEqual(summary["pure_black"], 4)
        self.assertGreaterEqual(summary["pure_white"], 16)

        invalid_cases = []
        colored = copy.deepcopy(valid_rows)
        colored[2][2] = (0.0, 0.2, 0.8, 1.0)
        invalid_cases.append(colored)
        dirty_border = copy.deepcopy(valid_rows)
        dirty_border[0][2] = black
        invalid_cases.append(dirty_border)
        transparent = copy.deepcopy(valid_rows)
        transparent[2][2] = (0.0, 0.0, 0.0, 0.5)
        invalid_cases.append(transparent)
        invalid_cases.append([[white for _ in range(5)] for _ in range(5)])
        for rows in invalid_cases:
            with self.subTest(rows=rows), self.assertRaises(RuntimeError):
                validate(flattened(rows), 5, 5)

    def test_windows_use_two_closed_matte_warm_paper_panes(self):
        source, tree = self._builder_tree()
        functions = {
            node.name: node for node in tree.body if isinstance(node, ast.FunctionDef)
        }
        self.assertIn("_create_closed_window_pane", functions)
        self.assertIn('"MAT_XuanPaperWarm"', source)
        self.assertIn('materials["paper"]', source)
        self.assertIn('f"{name}_Pane_{pane_index + 1:02d}"', source)
        pane_function = functions["_create_closed_window_pane"]
        face_literals = [
            node
            for node in ast.walk(pane_function)
            if isinstance(node, (ast.Tuple, ast.List))
        ]
        self.assertTrue(face_literals, "pane helper must create explicit closed mesh faces")
        material_calls = [
            node
            for node in ast.walk(functions["_create_materials"])
            if isinstance(node, ast.Call)
            and isinstance(node.func, ast.Name)
            and node.func.id == "_make_material"
        ]
        self.assertGreaterEqual(len(material_calls), 4)
        audit_literals = {
            node.value
            for node in ast.walk(functions["audit_scene"])
            if isinstance(node, ast.Constant) and isinstance(node.value, str)
        }
        self.assertIn("_Pane_", audit_literals)

    def test_builder_validates_fixed_camera_framing_before_rendering(self):
        _, tree = self._builder_tree()
        functions = {
            node.name: node
            for node in tree.body
            if isinstance(node, ast.FunctionDef)
        }
        self.assertIn("validate_camera_framing", functions)
        called_names = {
            node.func.id
            for node in ast.walk(functions["_reset_and_build"])
            if isinstance(node, ast.Call) and isinstance(node.func, ast.Name)
        }
        self.assertIn("validate_camera_framing", called_names)

    def test_builder_cli_parses_only_arguments_after_blender_boundary(self):
        _, tree = self._builder_tree()
        functions = {
            node.name: node for node in tree.body if isinstance(node, ast.FunctionDef)
        }
        self.assertIn("_parse_blender_args", functions)
        module = ast.Module(body=[functions["_parse_blender_args"]], type_ignores=[])
        ast.fix_missing_locations(module)
        namespace = {
            "__doc__": "test builder",
            "argparse": argparse,
            "Path": Path,
            "sys": sys,
            "VERSIONS": ("v001", "v002", "v003"),
        }
        exec(compile(module, "<builder-cli>", "exec"), namespace)
        parse_args = namespace["_parse_blender_args"]

        for version in ("v001", "v002", "v003"):
            with self.subTest(version=version), mock.patch.object(
                sys,
                "argv",
                [
                    "blender",
                    "--background",
                    "--factory-startup",
                    "--",
                    "--project-root",
                    str(PROJECT_ROOT),
                    "--version",
                    version,
                ],
            ):
                parsed = parse_args()
                self.assertEqual(parsed.project_root, PROJECT_ROOT)
                self.assertEqual(parsed.version, version)

        with mock.patch.object(
            sys,
            "argv",
            ["blender", "--", "--project-root", str(PROJECT_ROOT), "--version", "v004"],
        ), mock.patch.object(sys, "stderr", io.StringIO()):
            with self.assertRaises(SystemExit):
                parse_args()

    def test_builder_saves_blend_before_hashing_outputs(self):
        _, tree = self._builder_tree()
        functions = {
            node.name: node for node in tree.body if isinstance(node, ast.FunctionDef)
        }
        self.assertIn("main", functions)
        calls = [node for node in ast.walk(functions["main"]) if isinstance(node, ast.Call)]
        save_lines = [
            node.lineno
            for node in calls
            if isinstance(node.func, ast.Attribute) and node.func.attr == "save_as_mainfile"
        ]
        hash_lines = [
            node.lineno
            for node in calls
            if isinstance(node.func, ast.Name) and node.func.id == "sha256_file"
        ]
        self.assertEqual(len(save_lines), 1)
        self.assertEqual(len(hash_lines), 1)
        self.assertLess(save_lines[0], hash_lines[0])

    def test_builder_disables_blend_backup_sidecars(self):
        _, tree = self._builder_tree()
        self.assertTrue(
            any(
                isinstance(node, ast.Assign)
                and any(
                    isinstance(target, ast.Attribute) and target.attr == "save_version"
                    for target in node.targets
                )
                and isinstance(node.value, ast.Constant)
                and node.value.value == 0
                for node in ast.walk(tree)
            )
        )

    def test_builder_removes_only_its_stale_blend_backup_sidecar(self):
        _, tree = self._builder_tree()
        functions = {
            node.name: node for node in tree.body if isinstance(node, ast.FunctionDef)
        }
        self.assertIn("_remove_stale_blend_backup", functions)
        module = ast.Module(body=[functions["_remove_stale_blend_backup"]], type_ignores=[])
        ast.fix_missing_locations(module)
        namespace = {"Path": Path}
        exec(compile(module, "<blend-backup-cleanup>", "exec"), namespace)

        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            blend = root / "golden.blend"
            backup = root / "golden.blend1"
            unrelated = root / "other.blend1"
            backup.write_bytes(b"stale")
            unrelated.write_bytes(b"preserve")

            namespace["_remove_stale_blend_backup"](blend)

            self.assertFalse(backup.exists())
            self.assertEqual(unrelated.read_bytes(), b"preserve")

        main_calls = {
            node.func.id
            for node in ast.walk(functions["main"])
            if isinstance(node, ast.Call) and isinstance(node.func, ast.Name)
        }
        self.assertIn("_remove_stale_blend_backup", main_calls)

    def test_scene_audit_checks_outward_signed_mesh_volume(self):
        _, tree = self._builder_tree()
        functions = {
            node.name: node for node in tree.body if isinstance(node, ast.FunctionDef)
        }
        self.assertIn("audit_scene", functions)
        volume_calls = [
            node
            for node in ast.walk(functions["audit_scene"])
            if isinstance(node, ast.Call)
            and isinstance(node.func, ast.Attribute)
            and node.func.attr == "calc_volume"
        ]
        self.assertEqual(len(volume_calls), 1)
        self.assertTrue(
            any(
                keyword.arg == "signed"
                and isinstance(keyword.value, ast.Constant)
                and keyword.value.value is True
                for keyword in volume_calls[0].keywords
            )
        )

    def test_scene_audit_measures_capital_layers_and_window_divisions(self):
        _, tree = self._builder_tree()
        functions = {
            node.name: node for node in tree.body if isinstance(node, ast.FunctionDef)
        }
        audit = functions["audit_scene"]
        report_return = next(
            node
            for node in ast.walk(audit)
            if isinstance(node, ast.Return) and isinstance(node.value, ast.Dict)
        )
        report_values = {
            key.value: value
            for key, value in zip(report_return.value.keys, report_return.value.values)
            if isinstance(key, ast.Constant) and isinstance(key.value, str)
        }
        self.assertIsInstance(report_values["capital_layers"], ast.Name)
        self.assertEqual(report_values["capital_layers"].id, "measured_capital_layers")
        self.assertIsInstance(report_values["window_divisions"], ast.Name)
        self.assertEqual(
            report_values["window_divisions"].id, "measured_window_divisions"
        )
        audit_literals = {
            node.value
            for node in ast.walk(audit)
            if isinstance(node, ast.Constant) and isinstance(node.value, str)
        }
        self.assertTrue({"Capital_", "Column_", "_Post_L", "_Division_"}.issubset(audit_literals))

    def test_builder_declares_readable_two_storey_large_form_contract(self):
        _, tree = self._builder_tree()
        assignments = {
            target.id: node.value
            for node in tree.body
            if isinstance(node, ast.Assign)
            for target in node.targets
            if isinstance(target, ast.Name)
        }
        self.assertIn("LARGE_FORM_CONTRACT", assignments)
        form = ast.literal_eval(assignments["LARGE_FORM_CONTRACT"])

        self.assertEqual(form["storeys"], 2)
        self.assertGreaterEqual(form["ground_floor_top_z_m"], 1.75)
        self.assertLessEqual(form["ground_floor_top_z_m"], 2.15)
        self.assertGreaterEqual(form["upper_floor_top_z_m"], 3.45)
        self.assertGreaterEqual(form["door_width_m"], 1.45)
        self.assertLessEqual(form["door_width_m"], 1.70)
        self.assertGreaterEqual(form["door_height_m"], 1.55)
        self.assertLessEqual(form["door_height_m"], 1.80)
        shift_ratio = form["upper_floor_shift_x_m"] / form["body_width_m"]
        self.assertGreaterEqual(shift_ratio, 0.05)
        self.assertLessEqual(shift_ratio, 0.10)
        self.assertEqual(form["window_count"], 3)
        self.assertEqual(form["window_divisions"], 2)
        self.assertGreaterEqual(form["min_window_width_m"], 1.15)
        self.assertGreaterEqual(form["capital_top_width_m"], 0.85)
        self.assertAlmostEqual(
            form["balcony_band_z_m"], form["ground_floor_top_z_m"], delta=0.15
        )

        string_literals = {
            node.value
            for node in ast.walk(tree)
            if isinstance(node, ast.Constant) and isinstance(node.value, str)
        }
        self.assertTrue(
            {"Body_Ground_Storey", "Body_Upper_Storey", "Balcony_Ledge_Front"}.issubset(
                string_literals
            )
        )
        consumed_form_keys = {
            node.slice.value
            for node in ast.walk(tree)
            if isinstance(node, ast.Subscript)
            and isinstance(node.value, ast.Name)
            and node.value.id == "LARGE_FORM_CONTRACT"
            and isinstance(node.slice, ast.Constant)
            and isinstance(node.slice.value, str)
        }
        self.assertEqual(consumed_form_keys, set(form))

    def test_column_lean_preserves_signed_values_from_component_plan(self):
        _, tree = self._builder_tree()
        column_top = next(
            node
            for node in tree.body
            if isinstance(node, ast.FunctionDef) and node.name == "_column_top"
        )
        called_names = {
            node.func.id
            for node in ast.walk(column_top)
            if isinstance(node, ast.Call) and isinstance(node.func, ast.Name)
        }
        self.assertNotIn("abs", called_names)
        self.assertIn(
            "lean_deg",
            {
                node.value
                for node in ast.walk(column_top)
                if isinstance(node, ast.Constant) and isinstance(node.value, str)
            },
        )

    @unittest.skipUnless(BLENDER_EXE.is_file(), "Blender 4.2+ is not installed")
    def test_blender_self_test_rejects_capital_and_divider_mutations(self):
        builder = PROJECT_ROOT / "scripts" / "blender" / "build_qingshan_golden_inn.py"
        asset_relative = Path(
            "SourceAssets/TownPCG/QingshanEnvironment/assets/BLD_QS_M_A_INN/asset.json"
        )
        with tempfile.TemporaryDirectory() as temp_dir:
            project_root = Path(temp_dir).resolve()
            temporary_asset = project_root / asset_relative
            temporary_asset.parent.mkdir(parents=True)
            temporary_asset.write_bytes((PROJECT_ROOT / asset_relative).read_bytes())
            command = [
                str(BLENDER_EXE),
                "--background",
                "--factory-startup",
                "--python",
                str(builder),
                "--",
                "--project-root",
                str(project_root),
                "--version",
                "v001",
                "--self-test-audit",
            ]
            result = subprocess.run(
                command,
                cwd=PROJECT_ROOT,
                capture_output=True,
                text=True,
                timeout=120,
            )
            combined = result.stdout + result.stderr
            self.assertEqual(result.returncode, 0, combined)
            self.assertIn('"capital_mutation_rejected": true', combined)
            self.assertIn('"divider_mutation_rejected": true', combined)
            self.assertIn('"silhouette_black_white_verified": true', combined)
            self.assertFalse((temporary_asset.parent / "source").exists())

            output_directory = temporary_asset.parent / "source" / "blender"
            output_directory.mkdir(parents=True)
            existing_blend = output_directory / "BLD_QS_M_A_INN__golden__v001.blend"
            existing_blend.write_bytes(b"existing-production-output")
            refusal = subprocess.run(
                command,
                cwd=PROJECT_ROOT,
                capture_output=True,
                text=True,
                timeout=120,
            )
            refusal_output = refusal.stdout + refusal.stderr
            self.assertNotEqual(refusal.returncode, 0, refusal_output)
            self.assertIn("self-test refuses existing canonical output", refusal_output)
            self.assertEqual(existing_blend.read_bytes(), b"existing-production-output")
            self.assertEqual(
                sorted(path.name for path in output_directory.iterdir()),
                [existing_blend.name],
            )


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
