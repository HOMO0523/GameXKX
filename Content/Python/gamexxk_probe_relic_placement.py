"""Seed isolated relics and verify their shared-stage layout against the battle log."""
import json
import sys
from pathlib import Path

import unreal

assert "/PartyLeft/PreviewUser/" in str(unreal.Paths.project_saved_dir()).replace("\\", "/")
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world and "L_DesktopTrainingHUD" in world.get_path_name()
pc = unreal.GameplayStatics.get_player_controller(world, 0)
instance = unreal.GameplayStatics.get_game_instance(world)
dev = next(x for x in unreal.ObjectIterator(unreal.GameXXKDevToolsSubsystem) if x.get_outer() == instance)
board = pc.get_battle_board_widget_for_test()
assert board and board.is_battle_board_visible() and dev.is_session_active()
mode = sys.argv[1] if len(sys.argv) > 1 else "inspect"

def command(name, args):
    result = json.loads(dev.execute_json(json.dumps({"command": name, "args": args})))
    assert result.get("ok"), result
    return result

if mode == "seed":
    entries = command("catalog", {"category": "relic"})["data"]["entries"]
    for entry in entries[:13]:
        command("item.give", {"id": entry["id"], "quantity": 1})
    print(json.dumps({"seeded": 13}))
elif mode == "toggle_log":
    board.call_method("HandleBattleSettlementLogToggle")
    print(json.dumps({"toggled": True}))
elif mode == "play":
    mvp = next(x for x in unreal.ObjectIterator(unreal.GameXXKMVPSubsystem) if x.get_outer() == instance)
    state = mvp.get_runtime_state_copy()
    card = next(c for c in state.card_run.active_battle.deck.hand
                if str(c.card_id) == "Profession.Blade.JingHongChuQiao")
    assert board.click_card_in_hand(card.instance_id)
    assert board.is_card_targeting_active()
    assert board.confirm_targeting_unit(unreal.Name("TrainingEnemy.Rooster.1"))
    print(json.dumps({"played": str(card.card_id)}))
else:
    bar = pc.get_relic_bar_widget_for_test()
    assert bar and bar.get_rendered_relic_count_for_test() >= 13

    def find(owner, name):
        for widget in unreal.ObjectIterator(unreal.Widget):
            if widget.get_name() != name:
                continue
            parent = widget.get_outer()
            while parent and parent != owner:
                parent = parent.get_outer()
            if parent == owner:
                return widget
        raise RuntimeError(name + " not found")

    # Compare the shared design-space contract; cached tick geometry may be
    # empty for a non-ticking relic widget even while Slate paints it correctly.
    slot = find(bar, "RelicBarOverflow").get_editor_property("slot")
    start, size = slot.get_position(), slot.get_size()
    relic = [start.x, start.y, start.x + size.x, start.y + size.y]
    log_widget = find(board, "BattleSettlementLogPanel")
    log_size = log_widget.get_editor_property("slot").get_size()
    expanded = log_size.x > 650
    log = [24, 334, 724, 594] if expanded else [-96, 84, 246, 240]
    overlap = relic[0] < log[2] and relic[2] > log[0] and relic[1] < log[3] and relic[3] > log[1]
    report = {"mode": mode, "coordinate_space": "shared 1920x1080 safe stage", "expanded": expanded,
              "count": bar.get_rendered_relic_count_for_test(), "relic_rect": relic,
              "log_rect": log, "log_visible": log_widget.is_visible(), "overlap": overlap,
              "world": world.get_path_name()}
    out = Path(str(unreal.Paths.project_dir())).resolve() / "Saved/RelicBarPlacement"
    out.mkdir(parents=True, exist_ok=True)
    (out / (mode + ".json")).write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(report, ensure_ascii=False))
    assert not overlap and relic[2] > relic[0] and relic[3] > relic[1]
    if mode in ("compact", "expanded"):
        assert report["log_visible"] and expanded == (mode == "expanded")
