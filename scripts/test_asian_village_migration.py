import importlib
import os
import subprocess
import tempfile
import unittest
from pathlib import Path


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

    def test_resolve_target_rejects_symlink_escape(self):
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
                junction = subprocess.run(
                    ["cmd", "/c", "mklink", "/J", str(target), str(outside)],
                    capture_output=True,
                    text=True,
                )
                if junction.returncode != 0:
                    self.skipTest(
                        "directory symlinks and junctions unavailable: "
                        f"{exc}; {junction.stderr.strip()}"
                    )

            with self.assertRaisesRegex(
                module.MigrationError, r"Content[/\\]Asian_Village"
            ):
                module.resolve_target(target, project_root)

    def test_resolve_target_normalizes_path_errors(self):
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

            project_root = base / "project"
            (project_root / "Content").mkdir(parents=True)
            with self.assertRaisesRegex(
                module.MigrationError, r"Content[/\\]Asian_Village"
            ):
                module.resolve_target(Path("\0"), project_root)

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


if __name__ == "__main__":
    unittest.main()
