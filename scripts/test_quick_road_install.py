from __future__ import annotations

import json
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
UPROJECT = ROOT / "GameXXK.uproject"
PLUGIN = ROOT / "Plugins" / "Quick_Road" / "Quick_Road.uplugin"
LOCALIZATION_CPP = (
    ROOT / "Plugins" / "Quick_Road" / "Source" / "Quick_Road" / "Private"
    / "Quick_RoadLocalization.cpp"
)
LOCALIZATION_H = (
    ROOT / "Plugins" / "Quick_Road" / "Source" / "Quick_Road" / "Public"
    / "Quick_RoadLocalization.h"
)


class QuickRoadInstallTests(unittest.TestCase):
    def test_plugin_descriptor_stays_strict_utf8_and_runtime_never_rewrites_it(self):
        raw = PLUGIN.read_bytes()
        self.assertFalse(raw.startswith((b"\xff\xfe", b"\xfe\xff")))
        descriptor = json.loads(raw.decode("utf-8"))
        self.assertIn("Description", descriptor)
        localization_source = LOCALIZATION_CPP.read_text(encoding="utf-8")
        localization_header = LOCALIZATION_H.read_text(encoding="utf-8")
        self.assertNotIn("UpdateDescriptor", localization_source)
        self.assertNotIn("ApplyLocalizedPluginDescriptor", localization_source)
        self.assertNotIn("ApplyLocalizedPluginDescriptor", localization_header)

    def test_quick_road_plugin_and_dependencies_are_enabled(self):
        self.assertTrue(PLUGIN.is_file(), f"QuickRoad descriptor must exist: {PLUGIN}")

        descriptor = json.loads(PLUGIN.read_text(encoding="utf-8-sig"))
        self.assertEqual(descriptor.get("FriendlyName"), "Quick Road")
        self.assertEqual(descriptor.get("EngineVersion"), "5.8.0")
        self.assertIs(descriptor.get("CanContainContent"), True)

        project = json.loads(UPROJECT.read_text(encoding="utf-8-sig"))
        plugins = {entry["Name"]: entry for entry in project.get("Plugins", [])}

        for plugin_name in ("Quick_Road", "PCG", "ProceduralMeshComponent"):
            self.assertIn(plugin_name, plugins)
            self.assertIs(plugins[plugin_name].get("Enabled"), True)

        self.assertEqual(plugins["Quick_Road"].get("TargetAllowList"), ["Editor"])


if __name__ == "__main__":
    unittest.main()
