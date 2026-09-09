"""Restorable real-PIE shop and currency checks on the canonical desktop map."""
import json
from pathlib import Path
import sys
import unreal
import gamexxk_verify_route_node_art as route_probe
import gamexxk_probe_training_visual_mvp as base

OUT = Path(__file__).resolve().parents[2] / "Saved/RouteNodeArt"
MONEY = unreal.Name("Item.TravelMoney")

def balance(subsystem):
    state = subsystem.get_runtime_state_copy()
    return {"gold": state.player_gold, "backpack": state.inventory.get(MONEY, 0),
            "warehouse": state.desktop_inventory.warehouse_items.get(MONEY, 0),
            "legacy": state.card_run.route_travel_money}

def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "observe"
    world, controller, subsystem, dev = route_probe.context()
    _, _, workbench = base._controller_and_widget()
    if mode == "outside":
        assert not dev.is_session_active()
        route_probe.command(dev, "session.begin")
        unreal._gamexxk_money_ui_session = dev
        (OUT / "money-ui-before.json").write_text(json.dumps(route_probe.command(dev, "snapshot.export"), ensure_ascii=False), encoding="utf-8")
        grant = max(0, 1000000 - balance(subsystem)["gold"])
        if grant: route_probe.command(dev, "item.give", {"id": "Currency.Gold", "quantity": grant})
        subsystem.cancel_training_challenge_to_workbench()
        workbench.handle_desktop_action_for_test(1979)
    elif mode == "buy-bundle":
        assert getattr(unreal, "_gamexxk_money_ui_session", None) == dev
        before = balance(subsystem)
        workbench.handle_desktop_action_for_test(1961)
        after = balance(subsystem)
        assert after["gold"] == before["gold"] - 100000, (before, after)
        assert after["backpack"] == before["backpack"] + 10, (before, after)
        (OUT / "money-ui-outside-purchase.json").write_text(json.dumps({"before": before, "after": after}), encoding="utf-8")
    elif mode in ("route", "restart-route"):
        assert getattr(unreal, "_gamexxk_money_ui_session", None) == dev
        if mode == "restart-route":
            (OUT / "manual-preview-before-checks.json").write_text(json.dumps(route_probe.command(dev, "snapshot.export"), ensure_ascii=False), encoding="utf-8")
            original = json.loads((OUT / "money-ui-before.json").read_text(encoding="utf-8"))["data"]
            route_probe.command(dev, "snapshot.import", {"scene": original})
            route_probe.command(dev, "item.give", {"id": "Item.TravelMoney", "quantity": 10})
            grant = max(0, 1000000 - balance(subsystem)["gold"])
            if grant: route_probe.command(dev, "item.give", {"id": "Currency.Gold", "quantity": grant})
        subsystem.set_battle_auto_play_enabled(False)
        assert subsystem.start_training_challenge(unreal.Name("Training.Normal.1-1"))
        controller.refresh_player_flow_widgets_from_state()
    elif mode == "merchant":
        assert getattr(unreal, "_gamexxk_money_ui_session", None) == dev
        result = subsystem.apply_route_merchant_acceptance_fixture_for_test(True)
        assert "ROUTE_MERCHANT" in str(subsystem.get_runtime_state_copy().screen), result
    elif mode == "fund-merchant":
        assert getattr(unreal, "_gamexxk_money_ui_session", None) == dev
        scene = route_probe.command(dev, "snapshot.export")["data"]
        state = scene["state"]
        state["inventory"]["Item.TravelMoney"] = 3
        state["desktopInventory"]["warehouseItems"]["Item.TravelMoney"] = 100
        slots = state["desktopInventory"]["warehouseSlots"]
        slot = next(x for x in slots if x["entryId"] == "None")
        slot["entryId"] = "Item.TravelMoney"; slot["bEquipmentInstance"] = False
        route_probe.command(dev, "snapshot.import", {"scene": scene})
    elif mode in ("purchase", "poor", "refresh"):
        assert getattr(unreal, "_gamexxk_money_ui_session", None) == dev
        merchant = controller.get_route_merchant_widget_for_test()
        state = subsystem.get_runtime_state_copy()
        offers = [o for o in state.card_run.route_merchant.offers if not o.sold and not o.unavailable]
        before = balance(subsystem)
        if mode == "refresh":
            assert merchant.refresh_stock()
            price = 20
        else:
            offer = offers[0]
            price = offer.price
            success = merchant.purchase_offer(offer.offer_id)
            assert success == (mode == "purchase"), (mode, success)
        after = balance(subsystem)
        assert before["gold"] == after["gold"], (before, after)
        if mode == "poor": assert before == after
        else: assert after["backpack"] + after["warehouse"] == before["backpack"] + before["warehouse"] - price, (before, after, price)
        (OUT / ("money-ui-" + mode + ".json")).write_text(json.dumps({"before": before, "after": after, "price": price}), encoding="utf-8")
    elif mode == "restore":
        assert getattr(unreal, "_gamexxk_money_ui_session", None) == dev
        subsystem.clear_route_encounter_acceptance_fixture_for_test()
        route_probe.command(dev, "session.restore")
        del unreal._gamexxk_money_ui_session
        controller.refresh_player_flow_widgets_from_state()
    report = {"mode": mode, "balance": balance(subsystem), "session": dev.is_session_active(), "screen": str(subsystem.get_runtime_state_copy().screen)}
    route = controller.get_route_map_widget_for_test()
    if route:
        report.update(title=str(route.get_route_entry_title_for_test()), title_opacity=route.get_route_entry_title_opacity_for_test(), money_text=str(route.get_route_money_summary_text_for_test()))
    print(json.dumps(report, ensure_ascii=False))

main()
