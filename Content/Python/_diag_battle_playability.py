#!/usr/bin/env python3
"""Battle playability diagnostic (read-only, no battle mutation)."""
from __future__ import annotations

import json
import sys
from pathlib import Path

import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gamexxk_probe_real_play_flow as probe  # noqa: E402


def _struct_get(value, *names):
    return probe._struct_get(value, *names)


def _enum_name(value):
    return probe._enum_name(value)


def _call(obj, name, *args):
    fn = getattr(obj, name, None)
    if fn is None or not callable(fn):
        return None, "missing"
    try:
        return fn(*args), None
    except Exception as exc:
        return None, "err:" + str(exc)


def main() -> None:
    world = probe._get_game_world()
    pc = probe._first_player_controller(world)
    board, board_error = None, "no_pc"
    if pc:
        board, board_error = _call(pc, "get_battle_board_widget_for_test")
    subsystem = None
    if board:
        subsystem, _ = _call(board, "get_mvp_subsystem")

    out = {"board": None if board is None else str(board), "subsystem": subsystem is not None}

    battle = {}
    if subsystem is not None:
        try:
            raw = subsystem.get_runtime_state_copy()
            out["screen"] = _enum_name(_struct_get(raw, "screen", "Screen"))
            card_run = _struct_get(raw, "card_run", "CardRun")
            active = _struct_get(card_run, "active_battle", "ActiveBattle")
            battle["has_active_battle"] = bool(_struct_get(card_run, "b_has_active_card_battle", "bHasActiveCardBattle"))
            battle["phase"] = _enum_name(_struct_get(active, "phase", "Phase")) if active is not None else "no_active_battle"
            deck = _struct_get(active, "deck", "Deck") if active is not None else None
            battle["shared_energy"] = _struct_get(deck, "shared_energy", "SharedEnergy") if deck is not None else None
            battle["max_shared_energy"] = _struct_get(deck, "max_shared_energy", "MaxSharedEnergy") if deck is not None else None
            pending = _struct_get(deck, "pending_choice", "PendingChoice") if deck is not None else None
            battle["pending_choice_kind"] = _enum_name(_struct_get(pending, "kind", "Kind")) if pending is not None else None
            terrain = _struct_get(active, "terrain", "Terrain") if active is not None else None
            battle["terrain"] = str(terrain)
            units = _struct_get(active, "units", "Units") if active is not None else None
            enriched_units = {}
            try:
                entries = list(units) if units is not None else []
            except Exception:
                entries = []
            for unit in entries:
                unit_id = str(_struct_get(unit, "unit_id", "UnitId") or "")
                if not unit_id:
                    continue
                enriched_units[unit_id] = {
                    "side": _enum_name(_struct_get(unit, "side", "Side")),
                    "slot": _struct_get(unit, "battle_slot_number", "BattleSlotNumber"),
                    "living": bool(_struct_get(unit, "b_living", "living", "bLiving", "Living")),
                    "hp": _struct_get(unit, "hp", "HP"),
                    "max_hp": _struct_get(unit, "max_hp", "MaxHP"),
                    "mana": _struct_get(unit, "mana", "Mana"),
                    "max_mana": _struct_get(unit, "max_mana", "MaxMana"),
                }
            battle["units"] = enriched_units
            hand = _struct_get(deck, "hand", "Hand") if deck is not None else None
            try:
                hand_entries = list(hand) if hand is not None else []
            except Exception:
                hand_entries = []
            battle["hand"] = []
            for card in hand_entries:
                owner = str(_struct_get(card, "owner_unit_id", "OwnerUnitId") or "")
                battle["hand"].append({
                    "instance_id": str(_struct_get(card, "instance_id", "InstanceId") or ""),
                    "card_id": str(_struct_get(card, "card_id", "CardId") or ""),
                    "owner_unit_id": owner,
                    "owner_living": enriched_units.get(owner, {}).get("living"),
                })
        except Exception as exc:
            battle["state_error"] = str(exc)
    out["battle"] = battle

    board_info = {}
    if board is not None:
        for name in ("is_card_targeting_active", "is_card_targeting_for_test"):
            value, error = _call(board, name)
            board_info[name] = error if error else value
        value, error = _call(board, "get_pending_card_instance_id_for_test")
        board_info["pending_card_instance_id"] = error if error else str(value)
        highlighted = {}
        for unit_id in (battle.get("units") or {}).keys():
            value, error = _call(board, "is_target_unit_highlighted", unreal.Name(unit_id))
            highlighted[unit_id] = error if error else bool(value)
        board_info["highlighted_units"] = highlighted
        slots = []
        for index in range(10):
            btn, error = _call(board, "get_hand_card_button_for_test", index)
            if error or btn is None:
                continue
            slot = {"slot": index}
            enabled, enabled_error = _call(btn, "get_is_enabled")
            if enabled_error:
                enabled, enabled_error = _call(btn, "is_enabled")
            slot["enabled"] = enabled_error if enabled_error else bool(enabled)
            opacity, opacity_error = _call(btn, "get_render_opacity")
            slot["opacity"] = opacity_error if opacity_error else float(opacity)
            slots.append(slot)
        board_info["hand_buttons"] = slots
    else:
        board_info["error"] = board_error
    out["board"] = board_info

    print("DIAG_JSON_BEGIN")
    print(json.dumps(out, ensure_ascii=False, default=str))
    print("DIAG_JSON_END")


if __name__ == "__main__":
    main()
