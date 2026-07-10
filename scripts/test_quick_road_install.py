from __future__ import annotations

import json
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
UPROJECT = ROOT / "GameXXK.uproject"
PLUGIN = ROOT / "Plugins" / "Quick_Road" / "Quick_Road.uplugin"


class QuickRoadInstallTests(unittest.TestCase):
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
