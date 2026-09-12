"""Validate shipped player translations, independent of Unreal/PIE and saves."""

import json
import re
import unittest
from collections import Counter
from pathlib import Path
from decimal import Decimal


ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / "Content/Localization/GameXXK/strings.json"
ARGUMENT = re.compile(r"\{([A-Za-z][A-Za-z0-9_]*|[0-9]+)\}")
CJK = re.compile(r"[\u3400-\u9fff]")

def numeric_literals(value, narrative=False):
    value = ARGUMENT.sub("", value)
    # 10万 and 100,000 are the same amount; commas are not two extra quantities.
    value = re.sub(r"\d{1,3}(?:,\d{3})+(?:\.\d+)?", lambda m: m.group(0).replace(",", ""), value)
    def scaled(match):
        number = Decimal(match.group(1)) * {"万": 10000, "亿": 100000000}[match.group(2)]
        return format(number, "f").rstrip("0").rstrip(".") if number != number.to_integral() else str(int(number))
    value = re.sub(r"(\d+(?:\.\d+)?)(万|亿)", scaled, value)
    if not narrative:
        # In rules these describe a numeric limit. In prose, "once you arrive"
        # is a conjunction and must not invent a reward/count absent in the source.
        value = re.sub(r"\bonce\b", "1", value, flags=re.I)
        value = re.sub(r"\btwice\b", "2", value, flags=re.I)
    return Counter(re.findall(r"[0-9]+(?:\.[0-9]+)?", value))


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

    def test_namespaced_sources_survive_merges(self):
        identities={(e.get('namespace','GameXXK'),e['key']) for e in self.catalog()['entries']}
        for source in (ROOT/'docs/design/2026-09-10-ui-localization').glob('*-translations.json'):
            for entry in json.loads(source.read_text(encoding='utf-8'))['entries']:
                with self.subTest(source=source.name,key=entry['key']):
                    self.assertIn((entry.get('namespace','GameXXK'),entry['key']),identities,
                                  'A key shared by different namespaces must not erase a text identity')

    def test_chinese_card_faces_keep_native_abbreviations(self):
        entries={(e.get('namespace','GameXXK'),e['key']):e for e in self.catalog()['entries']}
        for key,zh,en in [('Legacy.1dfbeaf7ed056ffd','{0}气\n{1}内','{0} AP\n{1} MP'),
                          ('Legacy.d91ae0e39961bbd8','{0}气','{0} AP'),('Legacy.796d0bcac9801b08','{0}内','{0} MP')]:
            self.assertEqual(entries['GameXXK',key]['zh-Hans'],zh)
            self.assertEqual(entries['GameXXK',key]['en'],en)

    def test_translations_preserve_parameters_and_numbers(self):
        for entry in self.catalog()["entries"]:
            with self.subTest(key=entry["key"]):
                zh, en = entry["zh-Hans"], entry["en"]
                self.assertEqual(Counter(ARGUMENT.findall(zh)), Counter(ARGUMENT.findall(en)),
                                 "Translation dropped/duplicated a runtime argument")
                # Numbers authored into descriptions are mechanics, never approximate them in translation.
                narrative = entry.get("namespace", "").startswith("GameXXKMainStory")
                self.assertEqual(numeric_literals(zh,narrative), numeric_literals(en,narrative), "Translation changed an authored quantity")

    def test_all_card_names_use_at_most_two_words(self):
        cards=[entry for entry in self.catalog()['entries'] if entry.get('cardId')]
        self.assertTrue(cards)
        names=[]
        for entry in cards:
            with self.subTest(card=entry['cardId']):
                self.assertLessEqual(len(entry['en'].split()),2)
                names.append(entry['en'])
        self.assertEqual(len(names),len(set(names)), 'Short card names must remain distinct')

    def test_number_normalization_preserves_actual_amounts(self):
        self.assertEqual(numeric_literals("10万金币、10普通箱",True),numeric_literals("100,000 Gold and 10 Normal Chests",True))
        self.assertNotEqual(numeric_literals("10万金币",True),numeric_literals("10,000 Gold",True))
        self.assertEqual(numeric_literals("每回合1次"),numeric_literals("once per round"))
        self.assertEqual(numeric_literals("到达后",True),numeric_literals("Once you arrive",True))
        self.assertNotEqual(numeric_literals("2张牌"),numeric_literals("1 card"))

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


    def test_shipped_presentation_never_uses_reflected_enum_names(self):
        """Reflected enum display names are an editor-only truth.

        CoreUObject's UEnum::GetDisplayNameTextByIndex reads the UMETA(DisplayName)
        metadata inside `#if WITH_EDITOR` and otherwise falls through to
        `FText::FromString(GetNameStringByIndex(...))`, i.e. the raw C++ entry name.
        A packaged build therefore renders "Common"/"Epic"/"Legendary" where the
        editor showed the approved label, which is exactly how the equipment
        tooltip shipped an English quality name under zh-Hans.

        Presentation must resolve through the catalogue instead, e.g.
        FGameXXKEquipmentQualityRules::GetDisplayName(). The editor module may still
        use reflection freely, and test fixtures are ignored.
        """
        call = re.compile(r"GetDisplayNameTextBy(Value|Index)\s*\(|\bGetValueOrBitfieldAsDisplayNameText\s*\(")
        offenders = []
        runtime = ROOT / "Source/GameXXK"
        for path in sorted(list(runtime.rglob("*.cpp")) + list(runtime.rglob("*.h"))):
            if "Tests" in path.parts:
                continue
            source = path.read_text(encoding="utf-8", errors="replace")
            source = re.sub(r"/\*.*?\*/", "", source, flags=re.S)   # block comments
            source = re.sub(r"//[^\n]*", "", source)                 # line comments
            for number, line in enumerate(source.splitlines(), 1):
                if call.search(line):
                    offenders.append(f"{path.relative_to(ROOT).as_posix()}:{number}")
        self.assertEqual(
            [], offenders,
            "Reflected enum display names degrade to raw C++ entry names in a packaged build; "
            "resolve player-facing text through the localization catalogue instead")


if __name__ == "__main__":
    unittest.main()
