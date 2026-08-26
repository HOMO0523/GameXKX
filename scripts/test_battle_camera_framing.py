#!/usr/bin/env python3
"""Keep authored and runtime battle-camera framing in sync."""

from __future__ import annotations

import importlib.util
import sys
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest import mock


PROJECT_ROOT = Path(__file__).resolve().parents[1]
MAP_AUTHORING_SCRIPT = PROJECT_ROOT / "Content" / "Python" / "gamexxk_ensure_route_encounter_maps.py"
PLAYER_CONTROLLER_CPP = PROJECT_ROOT / "Source" / "GameXXK" / "Private" / "MVP" / "GameXXKMVPPlayerController.cpp"
WIDE_BATTLE_FOV = 63.0


def load_map_authoring_module():
    spec = importlib.util.spec_from_file_location("gamexxk_ensure_route_encounter_maps", MAP_AUTHORING_SCRIPT)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"could not load {MAP_AUTHORING_SCRIPT}")
    module = importlib.util.module_from_spec(spec)
    # Importing the authoring file outside UE only needs Vector to materialize
    # its static camera contract; no editor operation is invoked by this test.
    unreal_stub = SimpleNamespace(Vector=lambda x, y, z: (x, y, z))
    with mock.patch.dict(sys.modules, {"unreal": unreal_stub}):
        spec.loader.exec_module(module)
    return module


class BattleCameraFramingTest(unittest.TestCase):
    def test_authored_battle_camera_uses_wider_fov_for_board_safe_space(self) -> None:
        module = load_map_authoring_module()

        self.assertEqual(float(module.BATTLE_SCENE_MAP["camera_fov"]), WIDE_BATTLE_FOV)

    @unittest.skip(
        "The runtime no longer owns a camera-FOV fallback; battle FOV is map-authored and checked above."
    )
    def test_runtime_fallback_matches_authored_wide_fov(self) -> None:
        source = PLAYER_CONTROLLER_CPP.read_text(encoding="utf-8")

        self.assertIn(
            "constexpr float BattleSceneCameraFallbackFOV = 63.0f;",
            source,
        )


if __name__ == "__main__":
    unittest.main()
