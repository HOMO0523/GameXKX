"""Transient probe: buy the CompanionPack product from the meta shop in PIE."""

import json
import sys

import unreal

from gamexxk_probe_open_meta_shop import _merchant_actor


def _world():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    return editor.get_game_world() if editor else None


def main(argv):
    world = _world()
    pc = unreal.GameplayStatics.get_player_controller(world, 0) if world else None
    if not pc:
        print(json.dumps({"ok": False, "reason": "pc_missing"}))
        return

    try:
        shop_open = bool(pc.is_meta_shop_open_for_test())
    except Exception:
        shop_open = False

    if not shop_open:
        pawn = unreal.GameplayStatics.get_player_pawn(world, 0) if world else None
        merchant = _merchant_actor(world) if world else None
        if not merchant:
            print(json.dumps({"ok": False, "reason": "merchant_missing"}))
            return
        try:
            interacted = bool(merchant.apply_default_interaction(pawn))
        except Exception as exc:
            print(json.dumps({"ok": False, "reason": f"interact_error: {exc}"[:200]}))
            return
        if not interacted:
            print(json.dumps({"ok": False, "reason": "interact_failed"}))
            return

    shop = None
    try:
        shop = pc.get_meta_shop_widget_for_test()
    except Exception as exc:
        print(json.dumps({"ok": False, "reason": f"shop_widget_error: {exc}"[:200]}))
        return
    if not shop:
        print(json.dumps({"ok": False, "reason": "shop_missing"}))
        return

    steps = {}
    try:
        product = unreal.GameXXKMetaShopProductId.COMPANION_PACK
        steps["select"] = bool(shop.select_product(product))
    except Exception as exc:
        print(json.dumps({"ok": False, "reason": f"select_error: {exc}"[:200]}))
        return
    try:
        steps["request"] = bool(shop.request_purchase())
    except Exception as exc:
        print(json.dumps({"ok": False, "reason": f"request_error: {exc}"[:200], "steps": steps}))
        return
    try:
        steps["confirm"] = bool(shop.confirm_purchase())
    except Exception as exc:
        print(json.dumps({"ok": False, "reason": f"confirm_error: {exc}"[:200], "steps": steps}))
        return
    steps["shop_open_after"] = bool(pc.is_meta_shop_open_for_test())
    print(json.dumps({"ok": True, "steps": steps}, ensure_ascii=False))


if __name__ == "__main__":
    main(sys.argv[1:])
