from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace

from Content.Python.gamexxk_audit_asian_village import (
    ASSET_ROOT,
    AuditError,
    canonical_report_bytes,
    classify_dependency,
    collect_audit,
    summarize_assets,
    validate_report,
    write_report,
)


class PureAuditTests(unittest.TestCase):
    def test_dependency_classification(self):
        self.assertEqual(classify_dependency(ASSET_ROOT + "/meshes/X"), {"kind": "internal", "blocking": False})
        self.assertEqual(classify_dependency("/Engine/BasicShapes/Cube"), {"kind": "engine", "blocking": False})
        self.assertEqual(classify_dependency("/Script/Engine"), {"kind": "engine", "blocking": False})
        self.assertEqual(classify_dependency("/Game/Private/X"), {"kind": "external_game", "blocking": True})
        self.assertEqual(classify_dependency("/SomePlugin/X"), {"kind": "plugin_or_unknown", "blocking": False})

    def test_summary_and_canonical_bytes(self):
        summary = summarize_assets([{"class": "StaticMesh"}, {"class": "Material"}, {"class": "StaticMesh"}])
        self.assertEqual(summary, {"asset_count": 3, "class_counts": {"Material": 1, "StaticMesh": 2}})
        first = canonical_report_bytes({"z": "青山", "a": 1})
        second = canonical_report_bytes({"a": 1, "z": "青山"})
        self.assertEqual(first, second)
        self.assertNotIn(b"\r\n", first)

    def test_validate_report_rejects_missing_and_bool_count(self):
        with self.assertRaisesRegex(AuditError, "missing"):
            validate_report({}, "source-readonly")
        report = valid_report()
        report["asset_count"] = True
        with self.assertRaisesRegex(AuditError, "integer"):
            validate_report(report, "source-readonly")


def valid_report() -> dict:
    return {
        "schema_version": 1,
        "engine_version": "5.8",
        "project_dir": "D:/Project",
        "mode": "source-readonly",
        "asset_root": ASSET_ROOT,
        "asset_count": 0,
        "class_counts": {},
        "packages": [],
        "external_dependencies": [],
        "load_failures": [],
        "blueprint_failures": [],
        "material_failures": [],
        "map_failures": [],
        "save_failures": [],
        "ok": True,
    }


class FakeRegistry:
    def __init__(self, assets, dependencies):
        self.assets = assets
        self.dependencies = dependencies
        self.searched = False

    def search_all_assets(self, synchronous):
        self.searched = synchronous

    def get_assets_by_path(self, root, recursive=True):
        return self.assets

    def get_dependencies(self, package, options):
        return self.dependencies.get(str(package), [])


class FakeEditorAssets:
    loaded = {}
    save_result = True

    @classmethod
    def load_asset(cls, path):
        return cls.loaded.get(path)

    @classmethod
    def save_directory(cls, root, only_if_is_dirty=False, recursive=True):
        return cls.save_result


class FakeBlueprintLibrary:
    fail = False

    @classmethod
    def compile_blueprint(cls, asset):
        if cls.fail:
            raise RuntimeError("blueprint compile failed")


class FakeMaterialLibrary:
    fail = False

    @classmethod
    def recompile_material(cls, asset):
        if cls.fail:
            raise RuntimeError("material compile failed")


def fake_unreal(assets, dependencies=None):
    registry = FakeRegistry(assets, dependencies or {})
    return SimpleNamespace(
        AssetRegistryHelpers=SimpleNamespace(get_asset_registry=lambda: registry),
        AssetRegistryDependencyOptions=lambda **kwargs: kwargs,
        EditorAssetLibrary=FakeEditorAssets,
        BlueprintEditorLibrary=FakeBlueprintLibrary,
        MaterialEditingLibrary=FakeMaterialLibrary,
        EditorLoadingAndSavingUtils=SimpleNamespace(load_map=lambda path: object()),
        SystemLibrary=SimpleNamespace(
            get_engine_version=lambda: "5.8-test",
            execute_console_command=lambda world, command: None,
        ),
        Paths=SimpleNamespace(project_dir=lambda: "D:/Project/"),
    )


class UnrealAuditTests(unittest.TestCase):
    def setUp(self):
        FakeEditorAssets.loaded = {}
        FakeEditorAssets.save_result = True
        FakeBlueprintLibrary.fail = False
        FakeMaterialLibrary.fail = False

    def test_source_audit_reports_blocking_external_and_load_failure_without_saving(self):
        data = SimpleNamespace(
            package_name=ASSET_ROOT + "/meshes/X",
            asset_name="X",
            asset_class="StaticMesh",
            object_path=ASSET_ROOT + "/meshes/X.X",
        )
        module = fake_unreal([data], {str(data.package_name): ["/Game/Private/X", "/Engine/EngineMaterials/DefaultMaterial"]})
        report = collect_audit(module, "source-readonly")
        self.assertFalse(report["ok"])
        self.assertEqual(report["load_failures"], [data.object_path])
        self.assertTrue(any(x["blocking"] for x in report["external_dependencies"]))

    def test_target_upgrade_compiles_and_saves(self):
        blueprint = SimpleNamespace(package_name=ASSET_ROOT + "/blueprints/BP", asset_name="BP", asset_class="Blueprint", object_path=ASSET_ROOT + "/blueprints/BP.BP")
        material = SimpleNamespace(package_name=ASSET_ROOT + "/materials/M", asset_name="M", asset_class="Material", object_path=ASSET_ROOT + "/materials/M.M")
        FakeEditorAssets.loaded = {blueprint.object_path: object(), material.object_path: object()}
        report = collect_audit(fake_unreal([material, blueprint]), "target-upgrade")
        self.assertTrue(report["ok"])
        self.assertEqual(report["blueprint_failures"], [])
        self.assertEqual(report["material_failures"], [])
        self.assertEqual(report["save_failures"], [])

    def test_target_upgrade_records_compile_and_save_failures(self):
        blueprint = SimpleNamespace(package_name=ASSET_ROOT + "/blueprints/BP", asset_name="BP", asset_class="Blueprint", object_path=ASSET_ROOT + "/blueprints/BP.BP")
        FakeEditorAssets.loaded = {blueprint.object_path: object()}
        FakeBlueprintLibrary.fail = True
        FakeEditorAssets.save_result = False
        report = collect_audit(fake_unreal([blueprint]), "target-upgrade")
        self.assertFalse(report["ok"])
        self.assertEqual(len(report["blueprint_failures"]), 1)
        self.assertEqual(len(report["save_failures"]), 1)

    def test_write_report_restricts_output_and_refuses_overwrite(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            module = SimpleNamespace(Paths=SimpleNamespace(project_dir=lambda: str(root)))
            report = valid_report()
            output = root / "Saved" / "AsianVillageAudit" / "report.json"
            write_report(output, report, module)
            self.assertEqual(json.loads(output.read_text(encoding="utf-8"))["ok"], True)
            with self.assertRaisesRegex(AuditError, "already exists"):
                write_report(output, report, module)
            with self.assertRaisesRegex(AuditError, "escaped"):
                write_report(root / "outside.json", report, module)


if __name__ == "__main__":
    unittest.main()
