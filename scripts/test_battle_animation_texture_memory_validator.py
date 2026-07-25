#!/usr/bin/env python3
"""Pure contracts for the UE battle-animation texture memory validator."""

from __future__ import annotations

import importlib.util
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
VALIDATOR_PATH = PROJECT_ROOT / "Content/Python/gamexxk_validate_battle_animation_texture_memory.py"


def load_validator():
    spec = importlib.util.spec_from_file_location(
        "gamexxk_validate_battle_animation_texture_memory",
        VALIDATOR_PATH,
    )
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module


class BattleAnimationTextureMemoryValidatorTests(unittest.TestCase):
    def test_uses_ue58_blueprint_memory_size_api(self) -> None:
        validator = load_validator()

        class FakeTexture:
            @staticmethod
            def blueprint_get_memory_size() -> int:
                return 16 * 1024 * 1024

        self.assertEqual(
            validator._resource_size_bytes(FakeTexture()),
            16 * 1024 * 1024,
        )

    def test_report_rejects_any_texture_that_drifted_from_bc7_policy(self) -> None:
        validator = load_validator()

        report = validator.build_report(
            [
                {
                    "asset_id": "hero",
                    "ok": True,
                    "resource_size_bytes": 16 * 1024 * 1024,
                },
                {
                    "asset_id": "rooster",
                    "ok": False,
                    "errors": ["compression=TC_EDITOR_ICON"],
                },
            ]
        )

        self.assertFalse(report["ok"])
        self.assertEqual(report["failed_asset_ids"], ["rooster"])

    def test_pilot_validation_contains_exactly_six_assets(self) -> None:
        validator = load_validator()

        self.assertEqual(len(validator.PILOT_ASSET_IDS), 6)
        self.assertIn("character_00_hero_attack", validator.PILOT_ASSET_IDS)
        self.assertIn("enemy_01_rooster_hit", validator.PILOT_ASSET_IDS)


if __name__ == "__main__":
    unittest.main()
