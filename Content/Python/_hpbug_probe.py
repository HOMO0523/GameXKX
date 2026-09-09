import json
import time

import unreal

import gamexxk_probe_real_play_flow as probe

world = probe._get_game_world()
pc = probe._first_player_controller(world)
board = pc.get_battle_board_widget_for_test()
subsystem = probe._get_mvp_subsystem(world) or probe._get_mvp_subsystem_from_player_controller(pc)


def runtime():
    if subsystem is None:
        return {}
    return probe._runtime_state(subsystem)


def dump(label):
    state = runtime()
    out = {"label": label}
    out["units"] = {
        str(uid): {
            "hp": u.get("hp"),
            "max": u.get("max_hp"),
            "armor": u.get("armor"),
            "living": u.get("living"),
        }
        for uid, u in state.get("battle_units", {}).items()
    }
    out["hand"] = [
        {"instance": c.get("instance_id"), "card": c.get("card_id")}
        for c in state.get("battle_hand", [])
    ]
    out["shared_energy"] = state.get("shared_energy")
    print(json.dumps(out, ensure_ascii=False))


dump("before")
