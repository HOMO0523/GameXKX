import ast
from pathlib import Path
import struct
import unittest
import zlib


ROOT = Path(__file__).resolve().parents[1]
IMPORTER = ROOT / "Content" / "Python" / "gamexxk_import_battle_status_icons.py"


def _paeth(left: int, above: int, upper_left: int) -> int:
    estimate = left + above - upper_left
    left_distance = abs(estimate - left)
    above_distance = abs(estimate - above)
    upper_left_distance = abs(estimate - upper_left)
    if left_distance <= above_distance and left_distance <= upper_left_distance:
        return left
    if above_distance <= upper_left_distance:
        return above
    return upper_left


def _read_rgba_alpha_rows(source: Path) -> tuple[int, int, list[bytearray]]:
    data = source.read_bytes()
    offset = 8
    width = height = bit_depth = color_type = 0
    compressed = bytearray()
    while offset + 8 <= len(data):
        length = struct.unpack(">I", data[offset : offset + 4])[0]
        chunk_type = data[offset + 4 : offset + 8]
        payload = data[offset + 8 : offset + 8 + length]
        offset += 12 + length
        if chunk_type == b"IHDR":
            width, height, bit_depth, color_type = struct.unpack(">IIBB", payload[:10])
        elif chunk_type == b"IDAT":
            compressed.extend(payload)
        elif chunk_type == b"IEND":
            break
    if bit_depth != 8 or color_type != 6:
        raise AssertionError(f"expected 8-bit RGBA PNG, got bit_depth={bit_depth}, color_type={color_type}")

    raw = zlib.decompress(bytes(compressed))
    stride = width * 4
    previous = bytearray(stride)
    alpha_rows: list[bytearray] = []
    raw_offset = 0
    for _ in range(height):
        filter_type = raw[raw_offset]
        raw_offset += 1
        current = bytearray(raw[raw_offset : raw_offset + stride])
        raw_offset += stride
        for index in range(stride):
            left = current[index - 4] if index >= 4 else 0
            above = previous[index]
            upper_left = previous[index - 4] if index >= 4 else 0
            if filter_type == 1:
                current[index] = (current[index] + left) & 0xFF
            elif filter_type == 2:
                current[index] = (current[index] + above) & 0xFF
            elif filter_type == 3:
                current[index] = (current[index] + ((left + above) // 2)) & 0xFF
            elif filter_type == 4:
                current[index] = (current[index] + _paeth(left, above, upper_left)) & 0xFF
            elif filter_type != 0:
                raise AssertionError(f"unsupported PNG filter type: {filter_type}")
        alpha_rows.append(bytearray(current[3:stride:4]))
        previous = current
    return width, height, alpha_rows


class BattleStatusIconImportTest(unittest.TestCase):
    def test_appended_status_sources_and_destinations_are_explicit(self) -> None:
        tree = ast.parse(IMPORTER.read_text(encoding="utf-8"))
        text = IMPORTER.read_text(encoding="utf-8")
        expected = {
            "T_BattleStatus_MedicineHerbs": "status_glyph_medicine_draft_v1_alpha.png",
            "T_BattleStatus_WeakBrokenBlade": "status_glyph_weak_drooping_broken_blade_draft_v1_alpha.png",
            "T_BattleStatus_WealthCoin": "status_glyph_wealth_square_coin_draft_v1_alpha.png",
            "T_BattleStatus_RageFlame": "status_glyph_rage_horn_flame_draft_v1_alpha.png",
            "T_BattleStatus_PreyTargetEye": "status_glyph_prey_ink_target_draft_v1_alpha.png",
            "T_BattleStatus_ChargeSpiralHorn": "status_glyph_charge_spiral_horn_draft_v1_alpha.png",
            "T_BattleStatus_CounterHookBlade": "status_glyph_counter_return_hook_blade_draft_v1_alpha.png",
        }
        self.assertIsNotNone(tree)
        for destination, source in expected.items():
            self.assertIn(destination, text)
            self.assertIn(source, text)
            self.assertTrue((ROOT / "SourceArt" / "Generated" / "Draft" / "V1" / "Status" / source).is_file())
        self.assertIn("--route-only", text)

    def test_block_shield_source_and_safe_single_asset_import_are_explicit(self) -> None:
        text = IMPORTER.read_text(encoding="utf-8")
        source = ROOT / "SourceArt" / "UI" / "Battle" / "StatusIcons" / "battle_status_block_shield_inkflat_v4.png"

        self.assertIn("T_BattleStatus_BlockShield", text)
        self.assertIn(source.name, text)
        self.assertIn("--block-only", text)
        self.assertIn("GAMEXXK_BATTLE_STATUS_IMPORT_MODE", text)
        self.assertIn("replace_existing=False", text)
        self.assertTrue(source.is_file())

        header = source.read_bytes()[:26]
        self.assertEqual(header[:8], b"\x89PNG\r\n\x1a\n")
        self.assertEqual(header[12:16], b"IHDR")
        self.assertEqual(struct.unpack(">II", header[16:24]), (1254, 1254))
        self.assertEqual(header[24], 8, "BlockShield source must use 8-bit PNG channels")
        self.assertEqual(header[25], 6, "BlockShield source must be RGBA, not opaque RGB")

        width, height, alpha_rows = _read_rgba_alpha_rows(source)
        corners = (alpha_rows[0][0], alpha_rows[0][-1], alpha_rows[-1][0], alpha_rows[-1][-1])
        transparent = sum(alpha == 0 for row in alpha_rows for alpha in row)
        translucent = sum(0 < alpha < 255 for row in alpha_rows for alpha in row)
        total = width * height
        self.assertEqual(corners, (0, 0, 0, 0))
        self.assertGreater(transparent, total // 5)
        self.assertGreater(total - transparent, total // 5)
        self.assertGreater(translucent, 0, "BlockShield source should preserve antialiased alpha edges")


if __name__ == "__main__":
    unittest.main()
