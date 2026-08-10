from __future__ import annotations

import argparse
import json
import sys

import unreal

from gamexxk_meta_shop_probe_utils import warehouse_ids_from_snapshot

from gamexxk_probe_real_play_flow import (
    _all_actors,
    _enum_name,
    _first_player_controller,
    _first_player_pawn,
    _get_game_world,
    _get_mvp_subsystem,
    _get_mvp_subsystem_from_player_controller,
    _object_path,
    _struct_get,
)


def _is_visible(widget) -> bool:
    if widget is None:
        return False
    try:
        return bool(widget.is_visible())
    except Exception:
        pass
    try:
        return "VISIBLE" in _enum_name(widget.get_visibility()).upper()
    except Exception:
        return False


def _warehouse_ids(subsystem) -> list[str]:
    try:
        value = subsystem.get_equipment_warehouse_snapshot()
    except Exception:
        return []
    return warehouse_ids_from_snapshot(value)


def _player_gold(subsystem) -> int:
    try:
        state = subsystem.get_runtime_state_copy()
        return int(_struct_get(state, "player_gold", "PlayerGold") or 0)
    except Exception:
        return 0


def _ensure_acceptance_funds(subsystem, minimum_gold: int) -> int:
    """Prepare only the transient PIE state so the real purchase path can be exercised."""
    current_gold = _player_gold(subsystem)
    if current_gold >= minimum_gold:
        return 0
    try:
        state = subsystem.get_runtime_state_copy()
        state.set_editor_property("player_gold", minimum_gold)
        subsystem.set_editor_property("runtime_state", state)
    except Exception:
        return 0
    return _player_gold(subsystem) - current_gold


def _merchant_actor(world):
    for actor in _all_actors(world):
        if not hasattr(actor, "apply_default_interaction") or not hasattr(actor, "get_npc_role"):
            continue
        try:
            if _enum_name(actor.get_npc_role()).upper().endswith("MERCHANT"):
                return actor
        except Exception:
            continue
    return None


def _command_visible(player_controller, command_name: str) -> bool:
    try:
        overlay = player_controller.get_town_overlay_widget_for_test()
        commands = overlay.build_town_actions_for_test() if overlay else []
    except Exception:
        return False
    for command in commands or []:
        if str(_struct_get(command, "command_name", "CommandName")) == command_name:
            return True
    return False


def _legacy_trade_visible(player_controller) -> bool:
    try:
        inventory = player_controller.get_inventory_window_widget_for_test()
        if not inventory:
            return False
        mode = _enum_name(inventory.get_window_mode_for_test()).upper()
        return "MERCHANT" in mode and "TRADE" in mode and _is_visible(inventory)
    except Exception:
        return False


def _close(player_controller) -> dict:
    if player_controller is None:
        return {"ok": False, "reason": "player_controller_missing"}
    try:
        closed = bool(player_controller.close_meta_shop_window())
    except Exception as exc:
        return {"ok": False, "reason": str(exc)}
    return {"ok": closed, "meta_shop_open": bool(player_controller.is_meta_shop_open_for_test())}


def probe_meta_shop() -> dict:
    world = _get_game_world()
    player_controller = _first_player_controller(world)
    pawn = _first_player_pawn(world)
    subsystem = _get_mvp_subsystem(world) or _get_mvp_subsystem_from_player_controller(player_controller)
    merchant = _merchant_actor(world)
    if not all((world, player_controller, pawn, subsystem, merchant)):
        return {
            "ok": False,
            "reason": "live_meta_shop_context_missing",
            "world": _object_path(world),
            "player_controller": _object_path(player_controller),
            "pawn": _object_path(pawn),
            "subsystem": _object_path(subsystem),
            "merchant": _object_path(merchant),
        }

    products = list(subsystem.get_meta_shop_products() or [])
    first_price = int(_struct_get(products[0], "price", "Price") or 0) if products else 0
    acceptance_funding_delta = _ensure_acceptance_funds(subsystem, first_price)
    gold_before = _player_gold(subsystem)
    warehouse_before = _warehouse_ids(subsystem)
    interaction_ok = bool(merchant.apply_default_interaction(pawn))
    widget = player_controller.get_meta_shop_widget_for_test()
    shop_open = bool(player_controller.is_meta_shop_open_for_test()) and _is_visible(widget)
    selected = requested = confirmed = False
    product_name = ""
    if widget and products:
        product_id = _struct_get(products[0], "product_id", "ProductId")
        product_name = _enum_name(product_id)
        selected = bool(widget.select_product(product_id))
        requested = bool(widget.request_purchase()) if selected else False
        confirmed = bool(widget.confirm_purchase()) if requested else False

    gold_after = _player_gold(subsystem)
    warehouse_after = _warehouse_ids(subsystem)
    evidence = {
        "merchant_interaction_opened_meta_shop": interaction_ok and shop_open,
        "product_count": len(products),
        "legacy_trade_visible": _legacy_trade_visible(player_controller),
        "equipment_purchase_gold_delta": gold_after - gold_before,
        "equipment_instance_delta": len(warehouse_after) - len(warehouse_before),
        "acceptance_funding_delta": acceptance_funding_delta,
        "old_buy_item_command_visible": _command_visible(player_controller, "BuyHealingPowder"),
        "selected_product": product_name,
        "selection_succeeded": selected,
        "purchase_request_succeeded": requested,
        "purchase_confirm_succeeded": confirmed,
        "gold_before": gold_before,
        "gold_after": gold_after,
        "warehouse_count_before": len(warehouse_before),
        "warehouse_count_after": len(warehouse_after),
        "widget": _object_path(widget),
        "merchant": _object_path(merchant),
    }
    evidence["ok"] = (
        evidence["merchant_interaction_opened_meta_shop"]
        and evidence["product_count"] == 7
        and not evidence["legacy_trade_visible"]
        and evidence["equipment_purchase_gold_delta"] == -100
        and evidence["equipment_instance_delta"] == 1
        and not evidence["old_buy_item_command_visible"]
        and selected
        and requested
        and confirmed
    )
    return evidence


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--close", action="store_true")
    args = parser.parse_args(argv)
    world = _get_game_world()
    player_controller = _first_player_controller(world)
    if args.close:
        result = {"meta_shop_close": _close(player_controller)}
    else:
        result = {"meta_shop": probe_meta_shop()}
        result["ok"] = bool(result["meta_shop"].get("ok"))
    print(json.dumps(result, ensure_ascii=False))
    return 0 if (result.get("ok") or result.get("meta_shop_close", {}).get("ok")) else 1


if __name__ == "__main__":
    main(sys.argv[1:])
