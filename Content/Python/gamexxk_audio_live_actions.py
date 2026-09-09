"""Run only after the editor coordinator hands back the canonical 2D PIE slot."""
import argparse
import json
import uuid
from pathlib import Path

import unreal

def objects():
    w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    assert w and "L_DesktopTrainingHUD" in w.get_path_name(), "Canonical 2D PIE required"
    pc = unreal.GameplayStatics.get_player_controller(w, 0)
    wb = pc.get_desktop_training_workbench_widget_for_test()
    mvp = wb.get_mvp_subsystem()
    gi = unreal.GameplayStatics.get_game_instance(w)
    dev = next(o for o in unreal.ObjectIterator(unreal.GameXXKDevToolsSubsystem) if o.get_outer() == gi)
    return w, pc, wb, mvp, pc.get_battle_board_widget_for_test(), dev

def diag(w):
    return json.loads(unreal.GameXXKSfxLibrary.get_diagnostics(w))

def snapshot():
    w, pc, wb, mvp, board, dev = objects()
    s = mvp.get_runtime_state_copy()
    b = s.card_run.active_battle
    return {
        "world": w.get_path_name(), "screen": str(s.screen), "muted": wb.is_muted_for_test(),
        "dev_session": dev.is_session_active(), "audio": diag(w),
        "active_battle": s.card_run.has_active_card_battle, "phase": str(b.phase) if s.card_run.has_active_card_battle else None,
        "hand": [{"id": str(c.instance_id), "card": str(c.card_id), "owner": str(c.owner_unit_id)} for c in b.deck.hand],
        "units": [{"id": str(u.unit_id), "side": str(u.side), "hp": u.hp, "max_hp": u.max_hp, "armor": u.armor} for u in b.units],
        "board": board.get_battle_board_debug_state_for_test() if board else None,
        "tool_inputs": wb.get_occupied_tool_slot_count_for_test(),
        "carrying": wb.is_carrying_item_for_test(),
        "first_equipment": wb.find_first_backpack_equipment_slot_for_test(),
        "tools_visible": wb.is_tools_panel_active_for_test(),
        "notices": [{"name": t.get_name(), "text": str(t.get_text())} for t in unreal.ObjectIterator(unreal.TextBlock) if "Notice" in t.get_name()],
        "backpack": [{"slot": i, "id": str(e.entry_id)} for i, e in enumerate(s.desktop_inventory.backpack_slots) if str(e.entry_id) != "None"],
    }

def dev_call(command, arguments):
    dev = objects()[-1]
    return json.loads(dev.execute_json(json.dumps({"schema": 1, "request_id": "audio-" + uuid.uuid4().hex,
                                                  "command": command, "args": arguments})))

def mute_check():
    w, pc, wb, mvp, board, dev = objects()
    original = wb.is_muted_for_test()
    before = diag(w)
    if not original: wb.handle_desktop_action_for_test(17)
    try:
        muted = wb.is_muted_for_test()
        emitted = unreal.GameXXKSfxLibrary.play_named(w, "HitLight")
        during = diag(w)
        assert muted and not emitted and before["played"] == during["played"]
    finally:
        if wb.is_muted_for_test() != original: wb.handle_desktop_action_for_test(17)
    return {"ok": True, "original_muted": original, "restored_muted": wb.is_muted_for_test(), "during": during}

def invalid_tool():
    w, pc, wb, mvp, board, dev = objects()
    assert dev.is_session_active(), "Begin a temporary Dev session first"
    assert wb.get_occupied_tool_slot_count_for_test() == 0, "Do not consume existing tool reservations"
    wb.set_tool_mode_for_test(unreal.GameXXKDesktopToolMode.DISMANTLE)
    before = diag(w)["played"].get("Tool", 0)
    accepted = wb.confirm_tool_for_test()
    after = diag(w)["played"].get("Tool", 0)
    assert not accepted and before == after
    return {"ok": True, "accepted": accepted, "before": before, "after": after}

def valid_tool():
    w, pc, wb, mvp, board, dev = objects()
    assert dev.is_session_active(), "Only temporary Dev state may be consumed"
    assert wb.get_occupied_tool_slot_count_for_test() == 0
    wb.open_backpack()
    wb.set_tool_mode_for_test(unreal.GameXXKDesktopToolMode.DISMANTLE)
    setup = json.loads((Path(__file__).resolve().parents[2] / "Saved/Codex/EssentialSfx-20260909/dev-setup.json").read_text())
    created = set(setup["equipment"]["data"]["created_ids"])
    slots = mvp.get_runtime_state_copy().desktop_inventory.backpack_slots
    slot = next((i for i, entry in enumerate(slots) if str(entry.entry_id) in created), -1)
    assert slot >= 0, "Create a temporary equipment item first"
    picked = wb.pick_up_backpack_slot_for_test(slot)
    if not picked: return {"ok": False, "step": "pickup", "slot": slot, "snapshot": snapshot()}
    wb.handle_desktop_action_for_test(300)
    if wb.get_occupied_tool_slot_count_for_test() != 1:
        return {"ok": False, "step": "drop", "snapshot": snapshot()}
    before = diag(w)["played"].get("Tool", 0)
    accepted = wb.confirm_tool_for_test()
    after = diag(w)["played"].get("Tool", 0)
    assert accepted and after == before + 1, (accepted, before, after)
    return {"ok": True, "slot": slot, "accepted": accepted, "before": before, "after": after}

def seed(card, outcome=False):
    w, pc, wb, mvp, board, dev = objects()
    assert dev.is_session_active()
    result = mvp.apply_target_outcome_fixture_for_test(card) if outcome else mvp.apply_card_tooltip_fixture_for_test(card)
    return {"result": str(result), "snapshot": snapshot()}

def play(card, ally):
    w, pc, wb, mvp, board, dev = objects()
    assert dev.is_session_active()
    s = mvp.get_runtime_state_copy()
    b = s.card_run.active_battle
    choice = next(c for c in b.deck.hand if not card or str(c.card_id) == card)
    accepted = board.click_card_in_hand(choice.instance_id)
    targeted = None
    if accepted and board.is_card_targeting_active():
        target = next(u for u in b.units if u.hp > 0 and (("ENEMY" in str(u.side).upper()) != ally))
        targeted = board.confirm_targeting_unit(target.unit_id)
    return {"accepted": accepted, "targeted": targeted, "card": str(choice.card_id), "snapshot": snapshot()}

def invalid_card():
    w, pc, wb, mvp, board, dev = objects()
    assert dev.is_session_active() and board
    before = diag(w)["played"]
    accepted = board.click_card_in_hand("Audio.DoesNotExist")
    after = diag(w)["played"]
    assert not accepted and before == after
    return {"ok": True, "accepted": accepted, "before": before, "after": after}

if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("action", choices=["state", "dev", "mute-check", "tool-invalid", "tool-valid", "tool-prepare", "seed", "outcome", "play", "invalid-card", "end-turn", "clear-fixture"])
    p.add_argument("--id", default="")
    p.add_argument("--args", default="{}")
    p.add_argument("--ally", action="store_true")
    a = p.parse_args()
    if a.action == "state": result = snapshot()
    elif a.action == "dev": result = dev_call(a.id, json.loads(a.args))
    elif a.action == "mute-check": result = mute_check()
    elif a.action == "tool-invalid": result = invalid_tool()
    elif a.action == "tool-valid": result = valid_tool()
    elif a.action == "tool-prepare":
        w, pc, wb, mvp, board, dev = objects()
        assert dev.is_session_active()
        grant = dev_call("item.give", {"id": "Currency.Gold", "quantity": 1000000})
        root_purchase = mvp.purchase_talent_node("Talent.Root")
        purchase = mvp.purchase_talent_node("Talent.Entry.Tools")
        wb.handle_desktop_action_for_test(3)
        wb.handle_desktop_action_for_test(4)
        wb.handle_desktop_action_for_test(3)
        result = {"grant": grant["ok"], "root_purchase": str(root_purchase), "purchase": str(purchase), "unlocked": mvp.get_talent_projection().tools_unlocked, "snapshot": snapshot()}
    elif a.action in ("seed", "outcome"): result = seed(a.id, a.action == "outcome")
    elif a.action == "play": result = play(a.id, a.ally)
    elif a.action == "invalid-card": result = invalid_card()
    elif a.action == "end-turn": result = {"accepted": objects()[4].end_card_player_phase()}
    else:
        objects()[3].clear_card_tooltip_fixture_for_test()
        result = snapshot()
    print(json.dumps(result, ensure_ascii=False))
