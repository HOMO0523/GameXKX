"""Validate the targeted 1K atlas set used by the compact Travel strip."""

from __future__ import annotations

import json

import gamexxk_validate_battle_animation_texture_memory as texture_validator


PARTY_BASES = (
    "character_00_hero",
    "character_01_blade",
    "character_07_tusi_chief",
)
CHAPTER_ONE_ENEMY_BASES = (
    "enemy_01_rooster",
    "enemy_02_goat",
    "enemy_03_weasel",
    "enemy_04_civet",
    "enemy_06_bluehorn",
)
ACTIONS = ("idle", "attack", "hit", "death")
ASSET_IDS = tuple(
    f"{base}_1k_{action}"
    for base in (*PARTY_BASES, *CHAPTER_ONE_ENEMY_BASES)
    for action in ACTIONS
)


def main() -> None:
    texture_validator.ATLAS_SIZE = 1024
    texture_validator.MAX_RESOURCE_SIZE_BYTES = 2 * 1024 * 1024
    records = [texture_validator.validate_texture(asset_id) for asset_id in ASSET_IDS]
    report = texture_validator.build_report(records)
    report["profile"] = "desktop_training_first_chapter_1k"
    print(json.dumps(report, ensure_ascii=False))


if __name__ == "__main__":
    main()
