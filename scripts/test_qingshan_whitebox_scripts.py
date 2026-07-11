import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
IMPLEMENTATION = ROOT / "Source/GameXXKEditor/Private/GameXXKTownPCGAutomationLibrary.cpp"
EXPECTED_ROOTS = {
    "VerticalSliceGraphRoot": "/Game/GameXXK/Environment/TownPCG/VerticalSlice/",
    "B0RGraphRoot": "/Game/GameXXK/Environment/TownPCG/B0R/",
    "PrototypeMapRoot": "/Game/GameXXK/Maps/Prototype/",
    "B0RMapRoot": "/Game/GameXXK/Maps/Dev/",
}
EXPECTED_HELPERS = {
    "IsManagedGraphPath": ("VerticalSliceGraphRoot", "B0RGraphRoot"),
    "IsManagedPrototypeMapPath": ("PrototypeMapRoot", "B0RMapRoot"),
}


def _parse_root_constants(source):
    return dict(
        re.findall(
            r'constexpr TCHAR (\w+)\[\] = TEXT\("([^"]+)"\);',
            source,
        )
    )


def _helper_has_exact_logic(source, helper_name, root_names):
    first_root, second_root = map(re.escape, root_names)
    helper_pattern = re.compile(
        rf"bool {re.escape(helper_name)}\(const FString& Path\)\s*"
        rf"\{{\s*return Path\.StartsWith\({first_root}, ESearchCase::CaseSensitive\)\s*"
        rf"\|\| Path\.StartsWith\({second_root}, ESearchCase::CaseSensitive\);\s*\}}"
    )
    return helper_pattern.search(source) is not None


def _has_safe_root_contract(source):
    constants = _parse_root_constants(source)
    if {name: constants.get(name) for name in EXPECTED_ROOTS} != EXPECTED_ROOTS:
        return False
    return all(
        _helper_has_exact_logic(source, helper_name, root_names)
        for helper_name, root_names in EXPECTED_HELPERS.items()
    )


def _managed_path_is_accepted(source, helper_name, path):
    constants = _parse_root_constants(source)
    return any(path.startswith(constants[root_name]) for root_name in EXPECTED_HELPERS[helper_name])


class QingshanWhiteboxRootSafetyTests(unittest.TestCase):
    def test_editor_automation_has_the_exact_approved_root_contract(self):
        source = IMPLEMENTATION.read_text(encoding="utf-8")
        self.assertTrue(_has_safe_root_contract(source))

    def test_root_contract_rejects_broad_constants_used_by_helpers(self):
        source = IMPLEMENTATION.read_text(encoding="utf-8")
        broad_root_mutations = (
            ("VerticalSliceGraphRoot", "/Game/"),
            ("B0RGraphRoot", "/Game/GameXXK/"),
            ("PrototypeMapRoot", "/Game/GameXXK/Maps/"),
            ("VerticalSliceGraphRoot", "/Game/GameXXK/Environment/TownPCG/"),
        )
        for constant_name, broad_root in broad_root_mutations:
            with self.subTest(constant=constant_name, broad_root=broad_root):
                mutated = source.replace(
                    f'{constant_name}[] = TEXT("{EXPECTED_ROOTS[constant_name]}")',
                    f'{constant_name}[] = TEXT("{broad_root}")',
                )
                self.assertNotEqual(mutated, source)
                self.assertFalse(_has_safe_root_contract(mutated))

    def test_root_contract_rejects_sibling_prefix_acceptance(self):
        source = IMPLEMENTATION.read_text(encoding="utf-8")
        sibling_paths = (
            ("IsManagedGraphPath", "/Game/GameXXK/Environment/TownPCG/VerticalSliceSibling/Graph"),
            ("IsManagedGraphPath", "/Game/GameXXK/Environment/TownPCG/B0Rogue/Graph"),
            ("IsManagedPrototypeMapPath", "/Game/GameXXK/Maps/PrototypeSibling/Map"),
            ("IsManagedPrototypeMapPath", "/Game/GameXXK/Maps/Developer/Map"),
        )
        for helper_name, sibling_path in sibling_paths:
            with self.subTest(helper=helper_name, path=sibling_path):
                self.assertFalse(_managed_path_is_accepted(source, helper_name, sibling_path))

        for constant_name, approved_root in EXPECTED_ROOTS.items():
            with self.subTest(constant=constant_name):
                mutated = source.replace(
                    f'{constant_name}[] = TEXT("{approved_root}")',
                    f'{constant_name}[] = TEXT("{approved_root.rstrip("/")}")',
                )
                self.assertNotEqual(mutated, source)
                self.assertFalse(_has_safe_root_contract(mutated))

    def test_root_rejection_diagnostics_name_both_managed_scopes(self):
        source = IMPLEMENTATION.read_text(encoding="utf-8")
        self.assertIn("managed town PCG graph roots", source)
        self.assertIn("managed prototype/Dev maps", source)
        self.assertIn("managed prototype/Dev map level", source)
        self.assertNotIn("managed town PCG VerticalSlice root", source)
        self.assertNotIn("restricted to prototype maps", source)
        self.assertNotIn("not a prototype map level", source)


if __name__ == "__main__":
    unittest.main()
