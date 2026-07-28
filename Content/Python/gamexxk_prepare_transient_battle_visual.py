"""Prepare a non-saving 3v3 battle visual session inside the active PIE world.

This is deliberately a runtime-only verifier: it starts an in-memory new game,
enters the first combat node, and travels the current PIE session to the
battle map.  It never calls a save API or deletes a save slot.
"""

from __future__ import annotations

import json
import sys

import unreal

from gamexxk_probe_real_play_flow import (
    _get_game_world,
    _get_mvp_subsystem,
    _get_mvp_subsystem_from_player_controller,
    _first_player_controller,
    _runtime_state,
)


BATTLE_MAP = "/Game/GameXXK/Maps/L_BattleTown"


def _node_kind(name):
    enum_type = unreal.GameXXKNodeKind
    for candidate in (name, name.upper(), "GAME_XXK_NODE_KIND_" + name.upper()):
        value = getattr(enum_type, candidate, None)
        if value is not None:
            return value
    raise RuntimeError("node_kind_unavailable:" + name)


def main(_argv):
    world = _get_game_world()
    player_controller = _first_player_controller(world)
    subsystem = _get_mvp_subsystem(world) or _get_mvp_subsystem_from_player_controller(player_controller)
    result = {
        "ok": False,
        "persistent_save_api_called": False,
        "battle_map": BATTLE_MAP,
    }

    if not world:
        result["reason"] = "pie_world_missing"
    elif not subsystem:
        result["reason"] = "mvp_subsystem_missing"
    else:
        result["start_game"] = bool(subsystem.start_game())
        result["select_qingshan"] = bool(subsystem.select_world_region(unreal.Name("Region.Qingshan")))
        result["accept_quest"] = bool(subsystem.accept_quest())
        result["open_dungeon"] = bool(subsystem.open_dungeon_from_town_exit())
        result["select_start"] = bool(subsystem.select_dungeon_node(_node_kind("START")))
        result["select_battle"] = bool(subsystem.select_dungeon_node(_node_kind("BATTLE")))
        result["runtime_before_travel"] = _runtime_state(subsystem)
        result["ok"] = all(
            result[key]
            for key in ("start_game", "select_qingshan", "accept_quest", "open_dungeon", "select_start", "select_battle")
        )
        if result["ok"]:
            unreal.GameplayStatics.open_level(world, unreal.Name(BATTLE_MAP))
            result["travel_requested"] = True
        else:
            result["reason"] = "transient_flow_step_failed"

    print(json.dumps(result, ensure_ascii=False, sort_keys=True))


if __name__ == "__main__":
    main(sys.argv[1:])
