#!/usr/bin/env python3
"""PIE flow for the Formation Master terrain backdrop: main menu -> town -> route -> battle.

Verifies the backdrop texture follows the active battle terrain, then plays the first
Formation Master terrain-switch card in hand and verifies the backdrop switches with it.
Keeps PIE open afterwards for the user's visual review. Prints JSON on the last line.
"""

from __future__ import annotations

import json
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from gamexxk_real_play_flow_mcp import (
    RealFlowHarness,
    SCENE_TOOLSET,
    PROJECT_ROOT,
    _has_play_session,
    _is_qingshan_town,
    _load_json_from_probe,
    _widget_visible,
)
from ue_mcp_client import EDITOR_TOOLSET, UnrealMCPClient

BATTLE_PROBE = "Content/Python/_probe_battle_action.py"
TERRAIN_PROBE = "Content/Python/_probe_terrain_backdrop_pie.py"

# Formation Master terrain-switch cards and their destination terrains.
FORMATION_TERRAIN_CARDS = {
    "GuanShi": "Plain",
    "DingZhen": "Village",
    "YinShuiHuiYuan": "WaterShore",
    "KunZhen": "Cave",
    "LinYingMiZong": "Forest",
    "JieShanWeiZhang": "Cliff",
}


def _probe(client: UnrealMCPClient, script: str, argv: list[str]) -> dict:
    return _load_json_from_probe(client.run_project_python_file(script, argv))


def main() -> int:
    harness = RealFlowHarness(timeout=30.0, keep_pie=True)
    harness.preserve_default_save()
    harness.connect()
    client: UnrealMCPClient = harness.client
    result: dict = {"ok": False}

    if client.is_in_pie():
        client.stop_pie()
        if not client.wait_for_pie_state(False):
            raise RuntimeError("PIE did not stop before the terrain backdrop flow")

    client.call_tool(
        "load_level",
        {"level_path": "/Game/GameXXK/Maps/L_Main"},
        toolset_name=SCENE_TOOLSET,
        timeout=60.0,
    )
    harness.probe("--delete-default-save")

    client.call_tool(
        "StartPIE",
        {"options": {"bSimulate": False, "playMode": 1, "warmupSeconds": 1.0}},
        toolset_name=EDITOR_TOOLSET,
        timeout=60.0,
    )
    harness.wait_for("PIE play session", _has_play_session)
    harness.preview_window = harness.input.find_preview_window()

    harness.wait_for("main menu visible", lambda p: _widget_visible(p, "main_menu"), timeout=15.0, interval=0.5)
    client.run_project_python_file("Content/Python/gamexxk_probe_town_hud_pie.py", ["--setres"])
    time.sleep(1.5)

    harness.click_main_menu_start()
    harness.wait_for("StartGame opens the Qingshan town map", _is_qingshan_town, timeout=10.0, interval=0.5)
    time.sleep(1.0)

    accept = _probe(client, BATTLE_PROBE, ["accept_quest"])
    harness.event("accept_quest_probe", **accept)
    if not accept.get("ok"):
        result["error"] = f"accept_quest failed: {accept}"
        print(json.dumps(result, ensure_ascii=False))
        return 1

    entered = _probe(client, TERRAIN_PROBE, ["enter_route"])
    harness.event("enter_route_probe", **entered)
    if not entered.get("ok"):
        result["error"] = f"enter_route failed: {entered}"
        print(json.dumps(result, ensure_ascii=False))
        return 1
    time.sleep(1.0)

    started = _probe(client, TERRAIN_PROBE, ["select_start"])
    harness.event("select_start_probe", **started)
    if not started.get("ok"):
        result["error"] = f"select_start failed: {started}"
        print(json.dumps(result, ensure_ascii=False))
        return 1
    time.sleep(0.5)

    battle_open = _probe(client, BATTLE_PROBE, ["select_battle"])
    harness.event("select_battle_probe", **battle_open)
    if not battle_open.get("ok"):
        nodes = _probe(client, BATTLE_PROBE, ["route_nodes"])
        battle_nodes = [n for n in nodes.get("nodes", []) if "Battle" in str(n.get("node_kind")) and n.get("enabled")]
        if battle_nodes:
            walked = _probe(client, BATTLE_PROBE, ["select_route", str(battle_nodes[0].get("node_id"))])
            harness.event("select_route_probe", **walked)
            battle_open = _probe(client, BATTLE_PROBE, ["select_battle"])
    if not battle_open.get("ok"):
        result["error"] = f"battle did not open: {battle_open}"
        print(json.dumps(result, ensure_ascii=False))
        return 1
    time.sleep(1.5)

    baseline_terrain = _probe(client, TERRAIN_PROBE, ["terrain"])
    baseline_backdrop = _probe(client, TERRAIN_PROBE, ["backdrop"])
    harness.event("battle_baseline", terrain=baseline_terrain, backdrop=baseline_backdrop)
    result["baseline"] = {"terrain": baseline_terrain, "backdrop": baseline_backdrop}

    formation = _probe(client, TERRAIN_PROBE, ["hand_formation"])
    harness.event("hand_formation_probe", **formation)
    result["formation_cards"] = formation.get("formation_cards", [])
    cards = formation.get("formation_cards", [])

    if not cards:
        result["error"] = "no Formation Master cards in hand; backdrop-per-terrain at battle open verified only"
        print(json.dumps(result, ensure_ascii=False))
        return 1

    played = None
    for card in cards:
        short = card["card_id"].rsplit(".", 1)[-1]
        if short in FORMATION_TERRAIN_CARDS:
            played = card
            played["destination_terrain"] = FORMATION_TERRAIN_CARDS[short]
            break
    if not played:
        result["error"] = f"hand has Formation cards but no terrain-switch card: {cards}"
        print(json.dumps(result, ensure_ascii=False))
        return 1

    clicked = _probe(client, BATTLE_PROBE, ["click_card", played["instance_id"]])
    harness.event("click_terrain_card", **clicked)
    if not clicked.get("ok"):
        result["error"] = f"terrain card click failed: {clicked}"
        print(json.dumps(result, ensure_ascii=False))
        return 1
    time.sleep(0.5)
    targeting = _probe(client, BATTLE_PROBE, ["targeting_active"])
    if targeting.get("ok") and targeting.get("targeting_active"):
        confirmed = _probe(client, BATTLE_PROBE, ["confirm", "Hero"])
        harness.event("confirm_terrain_card", **confirmed)
    time.sleep(1.5)

    after_terrain = _probe(client, TERRAIN_PROBE, ["terrain"])
    after_backdrop = _probe(client, TERRAIN_PROBE, ["backdrop"])
    harness.event("battle_after_switch", terrain=after_terrain, backdrop=after_backdrop)

    expected_texture = f"T_BattleArena_{played['destination_terrain']}_GeneratedV2"
    result["played"] = played
    result["after"] = {"terrain": after_terrain, "backdrop": after_backdrop}
    result["terrain_switched"] = after_terrain.get("terrain_name") == played["destination_terrain"]
    result["backdrop_switched"] = expected_texture in str(after_backdrop.get("texture", ""))
    result["ok"] = bool(result["terrain_switched"] and result["backdrop_switched"])

    print(json.dumps(result, ensure_ascii=False))
    return 0 if result["ok"] else 1


if __name__ == "__main__":
    sys.exit(main())
