"""Validate shipped player translations, independent of Unreal/PIE and saves."""

import json
import re
import unittest
from collections import Counter
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / "Content/Localization/GameXXK/strings.json"
ARGUMENT = re.compile(r"\{([A-Za-z][A-Za-z0-9_]*|[0-9]+)\}")
CJK = re.compile(r"[\u3400-\u9fff]")


class LocalizationCatalogTests(unittest.TestCase):
    def catalog(self):
        self.assertTrue(CATALOG.is_file(), "The shipped zh-Hans/en catalogue is missing")
        return json.loads(CATALOG.read_text(encoding="utf-8-sig"))

    def test_only_approved_languages_and_complete_entries(self):
        data = self.catalog()
        self.assertEqual("zh-Hans", data["nativeCulture"])
        self.assertEqual(["zh-Hans", "en"], data["cultures"])
        entries = data["entries"]
        self.assertTrue(entries)
        identities = [(e.get("namespace", "GameXXK"), e["key"]) for e in entries]
        self.assertEqual(len(identities), len(set(identities)), "Duplicate translation identity")
        for entry in entries:
            with self.subTest(key=entry["key"]):
                self.assertTrue(entry["key"].strip())
                self.assertTrue(entry["zh-Hans"].strip())
                self.assertTrue(entry["en"].strip())
                if entry["key"] != "Language.Chinese":
                    self.assertIsNone(CJK.search(entry["en"]), "Untranslated Chinese in English copy")

    def test_translations_preserve_parameters_and_numbers(self):
        for entry in self.catalog()["entries"]:
            with self.subTest(key=entry["key"]):
                zh, en = entry["zh-Hans"], entry["en"]
                self.assertEqual(Counter(ARGUMENT.findall(zh)), Counter(ARGUMENT.findall(en)),
                                 "Translation dropped/duplicated a runtime argument")
                # Numbers authored into descriptions are mechanics, never approximate them in translation.
                def literals(value):
                    value = ARGUMENT.sub("", value)
                    # Natural English count words are equivalent to authored numeric limits.
                    value = re.sub(r"\bonce\b", "1", value, flags=re.I)
                    value = re.sub(r"\btwice\b", "2", value, flags=re.I)
                    return Counter(re.findall(r"[0-9]+(?:\.[0-9]+)?", value))
                self.assertEqual(literals(zh), literals(en), "Translation changed an authored quantity")

    def test_settings_and_navigation_have_player_copy(self):
        entries = {e["key"]: e for e in self.catalog()["entries"]}
        for key in ("Settings.Title", "Settings.Language", "Settings.Scale", "Settings.Saved",
                    "Language.Chinese", "Language.English", "Common.Close", "Common.Next",
                    "Common.Previous", "Common.Done", "Common.Skip", "Help.Title",
                    "Help.Desktop", "Help.Inventory", "Help.Formation", "Help.Deck",
                    "Help.Talents", "Help.Tools", "Help.Training", "Help.Story",
                    "Help.Academy", "Help.Shop", "Help.Route", "Help.Battle", "Help.Rewards"):
            with self.subTest(key=key):
                self.assertIn(key, entries, "A player surface has no bilingual reading entry")
        self.assertFalse(any("ResetCombatGuide" in key for key in entries),
                         "Retired reset-guide action must not return as player copy")


if __name__ == "__main__":
    unittest.main()
