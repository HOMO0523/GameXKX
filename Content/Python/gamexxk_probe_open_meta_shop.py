"""Open the meta shop via the town merchant NPC interaction (open only — no purchase).

For visual review: after this probe succeeds the meta shop window stays open in PIE
and can be captured with a Slate screenshot.
"""

from __future__ import annotations

import json
import sys

from gamexxk_probe_meta_shop_window import _merchant_actor
from gamexxk_probe_real_play_flow import (
    _first_player_controller,
    _first_player_pawn,
    _get_game_world,
    _object_path,
)


def main(argv: list[str]) -> dict:
    world = _get_game_world()
    player_controller = _first_player_controller(world)
    pawn = _first_player_pawn(world)
    merchant = _merchant_actor(world)
    if not all((world, player_controller, pawn, merchant)):
        return {
            "ok": False,
            "reason": "live_meta_shop_context_missing",
            "world": _object_path(world),
            "player_controller": _object_path(player_controller),
            "pawn": _object_path(pawn),
            "merchant": _object_path(merchant),
        }
    interaction_ok = bool(merchant.apply_default_interaction(pawn))
    shop_open = bool(player_controller.is_meta_shop_open_for_test())
    return {
        "ok": interaction_ok and shop_open,
        "interaction_ok": interaction_ok,
        "shop_open": shop_open,
        "merchant": _object_path(merchant),
    }


if __name__ == "__main__":
    print(json.dumps(main(sys.argv[1:]), ensure_ascii=False))
