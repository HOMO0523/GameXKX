from __future__ import annotations

import argparse
import json
import sys

from gamexxk_probe_real_play_flow import (
    _get_game_world,
    _handle_apply_battle_hud_fixture,
    _handle_clear_battle_hud_fixture,
)


def main(argv):
    parser = argparse.ArgumentParser()
    parser.add_argument("--clear", action="store_true")
    args = parser.parse_args(argv)

    world = _get_game_world()
    result = {}
    if args.clear:
        result["battle_hud_fixture_clear"] = _handle_clear_battle_hud_fixture(world)
    else:
        result["battle_hud_fixture"] = _handle_apply_battle_hud_fixture(world)
    print(json.dumps(result, ensure_ascii=False, sort_keys=True))


if __name__ == "__main__":
    main(sys.argv[1:])
