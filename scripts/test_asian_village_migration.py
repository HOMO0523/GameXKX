import importlib
import hashlib
import io
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from contextlib import redirect_stderr, redirect_stdout
from unittest import mock


PROJECT_ROOT = Path(__file__).resolve().parents[1]
GITIGNORE_RULE = "Content/Asian_Village/**"
CONTENT_NEGATIONS = (
    "!Content/**/*.uasset",
    "!Content/**/*.umap",
    "!Plugins/**/Content/**",
)


def migration_module():
    try:
        return importlib.import_module("scripts.asian_village_migration")
    except (ImportError, AttributeError) as exc:
        raise AssertionError(f"migration contract is unavailable: {exc}") from exc


def assert_gitignore_contract(test_case: unittest.TestCase, gitignore: Path) -> None:
    lines = gitignore.read_text(encoding="utf-8").splitlines()
    test_case.assertEqual(lines.count(GITIGNORE_RULE), 1)
    rule_index = lines.index(GITIGNORE_RULE)
    for negation in CONTENT_NEGATIONS:
        test_case.assertLess(lines.index(negation), rule_index)


def create_junction(test_case: unittest.TestCase, link: Path, target: Path) -> None:
    try:
        result = subprocess.run(
            ["cmd", "/c", "mklink", "/J", str(link), str(target)],
            capture_output=True,
            text=True,
        )
    except OSError as exc:
        test_case.skipTest(f"directory junctions unavailable: {exc}")
    if result.returncode != 0:
        test_case.skipTest(
            "directory junctions unavailable: "
            f"{result.stderr.strip() or result.stdout.strip()}"
        )


def make_asset_tree(root: Path) -> dict:
    files = {
        "Zeta/Map.umap": b"map-data",
        "alpha/mesh.uasset": b"mesh-data\x00",
        "alpha/readme.txt": "Eastern village\n".encode("utf-8"),
    }
    for relative, data in files.items():
        path = root / Path(relative)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    return files


def run_cli(*args: object) -> subprocess.CompletedProcess:
    return subprocess.run(
        [sys.executable, "-m", "scripts.asian_village_migration", *map(str, args)],
        cwd=PROJECT_ROOT,
        capture_output=True,
        text=True,
        encoding="utf-8",
    )


class MigrationContractTests(unittest.TestCase):
    def test_module_exposes_exact_constants(self):
        module = migration_module()

        self.assertEqual(
            module.SOURCE_ASSET_DIR,
            Path(r"D:\UE5 demo\zzz\我的项目\Content\Asian_Village"),
        )
        self.assertEqual(
            module.TARGET_RELATIVE_DIR, Path("Content") / "Asian_Village"
        )
        self.assertEqual(
            module.EXPECTED_COUNTS, {"files": 505, "uasset": 503, "umap": 2}
        )
        self.assertTrue(issubclass(module.MigrationError, RuntimeError))

    def test_resolve_target_accepts_only_exact_project_destination(self):
        module = migration_module()
        with tempfile.TemporaryDirectory() as directory:
            project_root = Path(directory)
            (project_root / "Content").mkdir()
            target = project_root / module.TARGET_RELATIVE_DIR

            self.assertEqual(module.resolve_target(target, project_root), target)

    def test_resolve_target_rejects_other_directories_and_traversal(self):
        module = migration_module()
        with tempfile.TemporaryDirectory() as directory:
            project_root = Path(directory)
            (project_root / "Content").mkdir()
            invalid_targets = (
                project_root / "Content" / "Other_Village",
                project_root / "Asian_Village",
                project_root
                / "Content"
                / ".."
                / "Content"
                / "Asian_Village",
                Path("Content") / "Asian_Village",
            )

            for target in invalid_targets:
                with self.subTest(target=target):
                    with self.assertRaisesRegex(
                        module.MigrationError, r"Content[/\\]Asian_Village"
                    ):
                        module.resolve_target(target, project_root)

    def test_resolve_target_rejects_target_symlink_escape(self):
        module = migration_module()
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            project_root = base / "project"
            content = project_root / "Content"
            outside = base / "outside"
            content.mkdir(parents=True)
            outside.mkdir()
            target = content / "Asian_Village"
            try:
                os.symlink(outside, target, target_is_directory=True)
            except OSError as exc:
                self.skipTest(f"directory symlinks unavailable: {exc}")

            with self.assertRaisesRegex(
                module.MigrationError, r"Content[/\\]Asian_Village"
            ):
                module.resolve_target(target, project_root)

    def test_resolve_target_rejects_target_junction_escape(self):
        module = migration_module()
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            project_root = base / "project"
            content = project_root / "Content"
            outside = base / "outside"
            content.mkdir(parents=True)
            outside.mkdir()
            target = content / "Asian_Village"
            create_junction(self, target, outside)

            with self.assertRaisesRegex(
                module.MigrationError, r"Content[/\\]Asian_Village"
            ):
                module.resolve_target(target, project_root)

    def test_resolve_target_rejects_content_junction_escape(self):
        module = migration_module()
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            project_root = base / "project"
            outside_content = base / "outside-content"
            project_root.mkdir()
            outside_content.mkdir()
            create_junction(self, project_root / "Content", outside_content)
            target = project_root / module.TARGET_RELATIVE_DIR

            with self.assertRaisesRegex(
                module.MigrationError, r"Content[/\\]Asian_Village"
            ):
                module.resolve_target(target, project_root)

    def test_resolve_target_normalizes_missing_root_error(self):
        module = migration_module()
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            missing_root = base / "missing-project"
            with self.assertRaisesRegex(
                module.MigrationError, r"Content[/\\]Asian_Village"
            ):
                module.resolve_target(
                    missing_root / module.TARGET_RELATIVE_DIR, missing_root
                )

    def test_resolve_target_normalizes_nul_root_error_with_cause(self):
        module = migration_module()
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory) / module.TARGET_RELATIVE_DIR
            with self.assertRaisesRegex(
                module.MigrationError, r"Content[/\\]Asian_Village"
            ) as raised:
                module.resolve_target(target, Path("\0"))

            self.assertIsInstance(raised.exception.__cause__, (OSError, ValueError))

    def test_real_gitignore_keeps_vendor_rule_last(self):
        assert_gitignore_contract(self, PROJECT_ROOT / ".gitignore")

    def test_temporary_gitignore_contract_detects_duplicates_and_order(self):
        with tempfile.TemporaryDirectory() as directory:
            gitignore = Path(directory) / ".gitignore"
            gitignore.write_text(
                "\n".join((*CONTENT_NEGATIONS, GITIGNORE_RULE, "")),
                encoding="utf-8",
            )
            assert_gitignore_contract(self, gitignore)

            gitignore.write_text(
                "\n".join((GITIGNORE_RULE, *CONTENT_NEGATIONS, GITIGNORE_RULE)),
                encoding="utf-8",
            )
            with self.assertRaises(AssertionError):
                assert_gitignore_contract(self, gitignore)

    def test_gitignore_semantics_only_ignore_vendor_content(self):
        cases = (
            ("Content/Asian_Village/example.uasset", 0),
            ("Content/GameXXK/Test.uasset", 1),
            ("Content/GameXXK/Test.umap", 1),
            ("Plugins/Test/Content/Test.uasset", 1),
        )
        with tempfile.TemporaryDirectory() as directory:
            repository = Path(directory)
            initialized = subprocess.run(
                ["git", "init", "--quiet"],
                cwd=repository,
                capture_output=True,
                text=True,
            )
            self.assertEqual(initialized.returncode, 0, initialized.stderr)
            (repository / ".gitignore").write_text(
                (PROJECT_ROOT / ".gitignore").read_text(encoding="utf-8"),
                encoding="utf-8",
            )

            for path, expected_returncode in cases:
                with self.subTest(path=path):
                    checked = subprocess.run(
                        ["git", "check-ignore", "-q", "--no-index", "--", path],
                        cwd=repository,
                        capture_output=True,
                        text=True,
                    )
                    self.assertEqual(
                        checked.returncode,
                        expected_returncode,
                        checked.stderr or checked.stdout,
                    )

    def test_local_dependency_policy_records_collaboration_boundaries(self):
        policy = PROJECT_ROOT / "docs" / "production" / "asian-village-local-dependency.md"
        self.assertTrue(policy.is_file(), f"missing dependency policy: {policy}")
        text = policy.read_text(encoding="utf-8")
        required_text = (
            "Stylized Eastern Village",
            "https://www.fab.com/listings/9841fee2-683f-4e68-adb8-bafec270a251",
            "UE 5.4",
            "Content/Asian_Village",
            "local-only",
            "Git ignored",
            "Tripo",
            "GPT",
            "external AI",
            "scripts",
            "reports",
            "maps",
            "adaptation assets",
        )
        for phrase in required_text:
            with self.subTest(phrase=phrase):
                self.assertIn(phrase, text)


class ManifestTests(unittest.TestCase):
    def test_build_manifest_is_sorted_and_records_hashes_sizes_and_counts(self):
        module = migration_module()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            files = make_asset_tree(root)

            manifest = module.build_manifest(root)

            self.assertEqual(manifest["schema_version"], 1)
            self.assertEqual(
                manifest["counts"], {"files": 3, "uasset": 1, "umap": 1}
            )
            self.assertEqual(
                [item["path"] for item in manifest["files"]], sorted(files)
            )
            for item in manifest["files"]:
                data = files[item["path"]]
                self.assertEqual(item["bytes"], len(data))
                self.assertEqual(item["sha256"], hashlib.sha256(data).hexdigest())
                self.assertRegex(item["sha256"], r"^[0-9a-f]{64}$")

    def test_canonical_json_and_write_manifest_are_utf8_lf_and_no_overwrite(self):
        module = migration_module()
        value = {"z": "村", "a": [1]}
        expected = '{\n  "a": [\n    1\n  ],\n  "z": "村"\n}\n'.encode("utf-8")
        self.assertEqual(module.canonical_json_bytes(value), expected)
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "manifest.json"
            module.write_manifest(output, value)
            self.assertEqual(output.read_bytes(), expected)
            with self.assertRaises(module.MigrationError):
                module.write_manifest(output, value)
            module.write_manifest(output, {"replacement": True}, replace=True)
            self.assertIn(b"replacement", output.read_bytes())
            self.assertFalse(output.with_name(output.name + ".tmp").exists())

    def test_build_manifest_rejects_symlink(self):
        module = migration_module()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            original = root / "asset.uasset"
            original.write_bytes(b"asset")
            link = root / "link.uasset"
            try:
                os.symlink(original, link)
            except OSError as exc:
                self.skipTest(f"file symlinks unavailable: {exc}")
            with self.assertRaisesRegex(module.MigrationError, "link.uasset"):
                module.build_manifest(root)

    def test_build_manifest_rejects_file_changed_while_hashing(self):
        module = migration_module()
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "changing.uasset"
            path.write_bytes(b"asset")
            real_fstat = os.fstat
            calls = 0

            def changing_fstat(fd):
                nonlocal calls
                stat = real_fstat(fd)
                calls += 1
                if calls == 2:
                    path.write_bytes(b"changed")
                return stat

            with mock.patch.object(module.os, "fstat", side_effect=changing_fstat):
                with self.assertRaisesRegex(module.MigrationError, "changing.uasset"):
                    module.sha256_file(path)

    def test_build_manifest_rejects_directory_junction(self):
        module = migration_module()
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            source = base / "source"
            outside = base / "outside"
            source.mkdir()
            outside.mkdir()
            (outside / "asset.uasset").write_bytes(b"asset")
            junction = source / "linked-assets"
            create_junction(self, junction, outside)
            with self.assertRaisesRegex(module.MigrationError, "linked-assets"):
                module.build_manifest(source)

    def test_verify_manifest_strictly_validates_schema_paths_order_and_content(self):
        module = migration_module()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            make_asset_tree(root)
            manifest = module.build_manifest(root)
            result = module.verify_manifest(root, manifest)
            self.assertEqual(result["status"], "verified")
            self.assertEqual(result["counts"], manifest["counts"])

            bad_values = []
            for invalid in (True, "1", 2):
                bad = dict(manifest, schema_version=invalid)
                bad_values.append(bad)
            for path in (
                "/absolute.uasset",
                "back\\slash.uasset",
                "../escape.uasset",
                "dir/../escape.uasset",
                "file.uasset:stream",
                "bad\x00name.uasset",
                "bad\x1fname.uasset",
                "taildot./asset.uasset",
                "trail /asset.uasset",
                "CON.uasset",
                "dir/LPT1.txt",
            ):
                bad = json.loads(json.dumps(manifest))
                bad["files"][0]["path"] = path
                bad_values.append(bad)
            bad = json.loads(json.dumps(manifest))
            bad["files"].append(dict(bad["files"][0]))
            bad["counts"]["files"] += 1
            bad_values.append(bad)
            bad = json.loads(json.dumps(manifest))
            bad["files"] = list(reversed(bad["files"]))
            bad_values.append(bad)
            bad = json.loads(json.dumps(manifest))
            bad["files"][0]["sha256"] = "A" * 64
            bad_values.append(bad)
            bad = json.loads(json.dumps(manifest))
            bad["files"][0]["bytes"] = True
            bad_values.append(bad)

            for bad in bad_values:
                with self.subTest(bad=bad):
                    with self.assertRaises(module.MigrationError):
                        module.verify_manifest(root, bad)

            changed = root / manifest["files"][0]["path"]
            changed.write_bytes(b"wrong")
            with self.assertRaisesRegex(module.MigrationError, str(changed.name)):
                module.verify_manifest(root, manifest)


class StageAndPromoteTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.base = Path(self.temporary.name)
        self.project = self.base / "project"
        (self.project / "Content").mkdir(parents=True)
        self.source = self.base / "source"
        self.source.mkdir()
        make_asset_tree(self.source)
        self.target = self.project / "Content" / "Asian_Village"
        self.staging = (
            self.project / "Saved" / "MigrationStaging" / "Asian_Village"
        )
        self.manifest = migration_module().build_manifest(self.source)

    def call(self, **kwargs):
        return migration_module().stage_and_promote(
            self.source, self.target, self.staging, self.manifest, **kwargs
        )

    def test_rejects_existing_target_or_staging_without_modifying_source(self):
        module = migration_module()
        self.target.mkdir()
        with self.assertRaises(module.MigrationError):
            self.call()
        shutil.rmtree(self.target)
        self.staging.mkdir(parents=True)
        with self.assertRaises(module.MigrationError):
            self.call()
        self.assertTrue((self.source / "alpha" / "mesh.uasset").exists())
        self.assertTrue(self.staging.exists())

    def test_copy_failure_or_hash_mismatch_cleans_only_safe_staging(self):
        module = migration_module()

        def failing_copier(source, destination):
            shutil.copy2(source, destination)
            raise OSError("injected copy failure")

        with self.assertRaisesRegex(module.MigrationError, "copy failure"):
            self.call(copier=failing_copier)
        self.assertFalse(self.target.exists())
        self.assertFalse(self.staging.exists())
        self.assertTrue(self.source.exists())

        wrong = json.loads(json.dumps(self.manifest))
        wrong["files"][0]["sha256"] = "0" * 64
        with self.assertRaisesRegex(module.MigrationError, "verification"):
            module.stage_and_promote(
                self.source, self.target, self.staging, wrong
            )
        self.assertFalse(self.target.exists())
        self.assertFalse(self.staging.exists())

    def test_source_change_during_copy_is_rejected_and_staging_removed(self):
        module = migration_module()
        changed = False

        def mutating_copier(source, destination):
            nonlocal changed
            result = shutil.copy2(source, destination)
            if not changed:
                Path(source).write_bytes(b"changed-after-copy")
                changed = True
            return result

        with self.assertRaises(module.MigrationError):
            self.call(copier=mutating_copier)
        self.assertFalse(self.target.exists())
        self.assertFalse(self.staging.exists())
        self.assertTrue(self.source.exists())

    def test_promote_failure_cleans_staging_and_success_promotes(self):
        module = migration_module()
        original_replace = Path.replace

        def fail_staging_replace(path, target):
            if Path(path) == self.staging:
                raise OSError("injected promote failure")
            return original_replace(path, target)

        with mock.patch.object(Path, "replace", fail_staging_replace):
            with self.assertRaisesRegex(module.MigrationError, "promote failure"):
                self.call()
        self.assertFalse(self.target.exists())
        self.assertFalse(self.staging.exists())

        result = self.call()
        self.assertEqual(result["status"], "promoted")
        self.assertEqual(result["counts"], self.manifest["counts"])
        self.assertTrue(self.target.is_dir())
        self.assertFalse(self.staging.exists())

    def test_final_verification_failure_keeps_promoted_target(self):
        module = migration_module()
        verified = {"status": "verified", "counts": self.manifest["counts"]}
        with mock.patch.object(
            module,
            "verify_manifest",
            side_effect=[
                verified,
                verified,
                verified,
                module.MigrationError("terminal verify failure"),
            ],
        ):
            with self.assertRaisesRegex(module.MigrationError, "terminal"):
                self.call()
        self.assertTrue(self.target.exists())
        self.assertFalse(self.staging.exists())
        self.assertTrue(self.source.exists())

    def test_rejects_noncanonical_staging_and_target(self):
        module = migration_module()
        for target, staging in (
            (self.project / "Content" / "Elsewhere", self.staging),
            (self.target, self.project / "Saved" / "other"),
            (self.target, self.base / "Asian_Village"),
        ):
            with self.subTest(target=target, staging=staging):
                with self.assertRaises(module.MigrationError):
                    module.stage_and_promote(
                        self.source, target, staging, self.manifest
                    )


class CliTests(unittest.TestCase):
    def test_inventory_subprocess_writes_manifest_and_canonical_stdout(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "source"
            root.mkdir()
            make_asset_tree(root)
            output = Path(directory) / "manifest.json"
            result = run_cli("inventory", "--source", root, "--output", output)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stderr, "")
            self.assertEqual(result.stdout.encode("utf-8"), output.read_bytes())
            self.assertEqual(json.loads(result.stdout)["counts"]["files"], 3)

            again = run_cli("inventory", "--source", root, "--output", output)
            self.assertNotEqual(again.returncode, 0)
            self.assertEqual(again.stdout, "")
            self.assertNotIn("Traceback", again.stderr)

    def test_verify_subprocess_supports_output_and_rejects_duplicate_keys(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "source"
            root.mkdir()
            make_asset_tree(root)
            manifest_path = Path(directory) / "manifest.json"
            output = Path(directory) / "verification.json"
            module = migration_module()
            module.write_manifest(manifest_path, module.build_manifest(root))

            result = run_cli(
                "verify", "--root", root, "--manifest", manifest_path,
                "--output", output,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stdout.encode("utf-8"), output.read_bytes())
            self.assertEqual(json.loads(result.stdout)["status"], "verified")

            duplicate = Path(directory) / "duplicate.json"
            duplicate.write_text(
                '{"schema_version":1,"schema_version":1,"counts":{},"files":[]}',
                encoding="utf-8",
            )
            failed = run_cli("verify", "--root", root, "--manifest", duplicate)
            self.assertNotEqual(failed.returncode, 0)
            self.assertEqual(failed.stdout, "")
            self.assertIn("duplicate", failed.stderr.lower())
            self.assertNotIn("Traceback", failed.stderr)

    def test_copy_subprocess_promotes_to_fixed_project_paths(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            source = base / "source"
            source.mkdir()
            make_asset_tree(source)
            project = base / "project"
            (project / "Content").mkdir(parents=True)
            manifest_path = base / "manifest.json"
            module = migration_module()
            module.write_manifest(manifest_path, module.build_manifest(source))

            result = subprocess.run(
                [
                    sys.executable,
                    "-c",
                    (
                        "import runpy, subprocess\n"
                        "from unittest.mock import patch\n"
                        "no_editor = subprocess.CompletedProcess([], 0, "
                        "'INFO: No tasks are running', '')\n"
                        "with patch('subprocess.run', return_value=no_editor):\n"
                        "    runpy.run_module('scripts.asian_village_migration', "
                        "run_name='__main__')\n"
                    ),
                    "copy", "--source", str(source), "--project-root", str(project),
                    "--manifest", str(manifest_path),
                ],
                cwd=PROJECT_ROOT,
                capture_output=True,
                text=True,
                encoding="utf-8",
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            payload = json.loads(result.stdout)
            self.assertEqual(payload["status"], "promoted")
            self.assertTrue((project / "Content" / "Asian_Village").is_dir())
            self.assertFalse(
                (project / "Saved" / "MigrationStaging" / "Asian_Village").exists()
            )

    def test_copy_cli_rejects_unreal_editor_via_injected_process_check(self):
        module = migration_module()
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            source = base / "source"
            source.mkdir()
            make_asset_tree(source)
            project = base / "project"
            (project / "Content").mkdir(parents=True)
            manifest_path = base / "manifest.json"
            module.write_manifest(manifest_path, module.build_manifest(source))
            stdout = io.StringIO()
            stderr = io.StringIO()
            with redirect_stdout(stdout), redirect_stderr(stderr):
                code = module.main(
                    [
                        "copy", "--source", str(source), "--project-root",
                        str(project), "--manifest", str(manifest_path),
                    ],
                    process_checker=lambda: True,
                )
            self.assertNotEqual(code, 0)
            self.assertEqual(stdout.getvalue(), "")
            self.assertIn("UnrealEditor.exe", stderr.getvalue())
            self.assertNotIn("Traceback", stderr.getvalue())


if __name__ == "__main__":
    unittest.main()
