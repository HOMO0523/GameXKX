"""Host-side source contract for the Qingshan town PCG editor library."""

from __future__ import annotations

from pathlib import Path
import re
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[1]
HEADER = PROJECT_ROOT / "Source" / "GameXXKEditor" / "Public" / "GameXXKTownPCGAutomationLibrary.h"
IMPLEMENTATION = (
    PROJECT_ROOT
    / "Source"
    / "GameXXKEditor"
    / "Private"
    / "GameXXKTownPCGAutomationLibrary.cpp"
)
BUILD_RULES = PROJECT_ROOT / "Source" / "GameXXKEditor" / "GameXXKEditor.Build.cs"


class QingshanTownPCGScriptsTests(unittest.TestCase):
    def test_editor_library_declares_required_blueprint_operations(self):
        self.assertTrue(HEADER.is_file(), f"missing editor library header: {HEADER}")
        self.assertTrue(
            IMPLEMENTATION.is_file(),
            f"missing editor library implementation: {IMPLEMENTATION}",
        )

        header = HEADER.read_text(encoding="utf-8")
        implementation = IMPLEMENTATION.read_text(encoding="utf-8")
        self.assertIn("UGameXXKTownPCGAutomationLibrary", header)
        self.assertIn("UBlueprintFunctionLibrary", header)
        for operation in (
            "CreateOrUpdateTownPCGGraph",
            "AttachTownPCGGraph",
            "GenerateTownPCG",
            "ClearTownPCG",
        ):
            with self.subTest(operation=operation):
                self.assertRegex(header, rf"\b{operation}\s*\(")
                self.assertRegex(implementation, rf"\b{operation}\s*\(")

    def test_editor_module_declares_pcg_dependencies(self):
        build_rules = BUILD_RULES.read_text(encoding="utf-8")
        public_match = re.search(
            r"PublicDependencyModuleNames\.AddRange\s*\(.*?\{(?P<body>.*?)\}\s*\)",
            build_rules,
            re.DOTALL,
        )
        self.assertIsNotNone(public_match, "missing public dependency declaration")
        public_dependencies = set(re.findall(r'"([A-Za-z0-9_]+)"', public_match.group("body")))
        self.assertIn("PCG", public_dependencies)

    def test_generated_component_count_uses_public_pcg_component_tags(self):
        implementation = IMPLEMENTATION.read_text(encoding="utf-8")
        self.assertNotIn("GetManagedResources", implementation)
        self.assertIn("PCGHelpers::DefaultPCGTag", implementation)

    def test_mutating_operations_are_transactional_and_generation_completes_before_counting(self):
        implementation = IMPLEMENTATION.read_text(encoding="utf-8")
        self.assertIn("FScopedTransaction", implementation)
        self.assertIn("GenerateLocalGetTaskId", implementation)
        self.assertIn("IsGenerating()", implementation)
        self.assertIn("OnPCGGraphGeneratedDelegate", implementation)
        self.assertIn("OnPCGGraphCancelledDelegate", implementation)
        self.assertNotIn("GenerateLocal(true)", implementation)

    def test_graph_authoring_rejects_an_existing_object_of_the_wrong_class(self):
        implementation = IMPLEMENTATION.read_text(encoding="utf-8")
        self.assertIn("LoadAssetByPackagePath<UObject>", implementation)
        self.assertIn("FindObject<UObject>", implementation)
        self.assertIn("requested graph object exists but is not a PCG graph", implementation)


if __name__ == "__main__":
    unittest.main()
