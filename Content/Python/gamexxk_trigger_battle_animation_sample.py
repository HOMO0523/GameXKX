from __future__ import annotations

import json
import sys


PREFERRED_HERO_ATTACK_CARD_IDS = (
    "Route.General.PoJiaTuCi",
    "Hero.HeYuZhan",
    "Hero.Generic.SuiYanJi",
    "Hero.Generic.QingFengYiShi",
)


def trigger_card_attack(board, cards: list[dict], target_unit_id: str, name_factory=None) -> dict:
    """Play a real hero attack card and commit its stable runtime target."""
    make_name = name_factory or (lambda value: value)
    if not board:
        return {"ok": False, "reason": "battle_board_missing"}
    selected = next(
        (
            card
            for card_id in PREFERRED_HERO_ATTACK_CARD_IDS
            for card in cards
            if card.get("card_id") == card_id and card.get("owner_unit_id") == "Player"
        ),
        None,
    )
    if not selected:
        return {"ok": False, "reason": "preferred_hero_attack_missing", "cards": cards}
    instance_id = str(selected.get("instance_id") or "")
    if not instance_id or not bool(board.click_card_in_hand(make_name(instance_id))):
        return {"ok": False, "reason": "click_card_failed", "card": selected}
    if not bool(board.is_card_targeting_active()):
        return {"ok": False, "reason": "manual_targeting_not_started", "card": selected}
    if not bool(board.confirm_targeting_unit(make_name(target_unit_id))):
        return {"ok": False, "reason": "confirm_target_failed", "card": selected, "target": target_unit_id}
    return {
        "ok": True,
        "card_id": selected["card_id"],
        "card_instance_id": instance_id,
        "target_unit_id": target_unit_id,
    }


def trigger_basic_attack(board, enemy_index: int = 1, vector_factory=None) -> dict:
    """Drive the same Board targeting path used by player clicks."""
    make_vector = vector_factory or (lambda x, y: (x, y))
    if not board:
        return {"ok": False, "reason": "battle_board_missing"}
    if not bool(board.open_command_menu_for_party_unit(
        0,
        make_vector(1020.0, 380.0),
        make_vector(1220.0, 390.0),
    )):
        return {"ok": False, "reason": "open_command_menu_failed"}
    if not bool(board.execute_basic_attack_action()):
        return {"ok": False, "reason": "begin_basic_attack_failed"}
    if not bool(board.confirm_targeting_enemy(int(enemy_index))):
        return {"ok": False, "reason": "confirm_enemy_failed", "enemy_index": int(enemy_index)}
    return {"ok": True, "enemy_index": int(enemy_index)}


def main(argv: list[str]) -> int:
    import unreal
    from gamexxk_probe_party_deck_runtime import (
        _first_controller,
        _get_game_world,
        _get_mvp_subsystem,
        _instance_summary,
        _prop,
        _state,
    )

    target_unit_id = argv[0] if argv else "Enemy.Rooster.P3"
    world = _get_game_world()
    player_controller = _first_controller(world)
    board = player_controller.get_battle_board_widget_for_test() if player_controller else None
    subsystem = _get_mvp_subsystem(world, player_controller)
    state = _state(subsystem)
    card_run = _prop(state, "card_run", "CardRun")
    battle = _prop(card_run, "active_battle", "ActiveBattle")
    deck = _prop(battle, "deck", "Deck")
    cards = [_instance_summary(item) for item in (_prop(deck, "hand", "Hand") or [])]
    result = trigger_card_attack(board, cards, target_unit_id, unreal.Name)
    print(json.dumps(result, ensure_ascii=False))
    return 0 if result.get("ok") else 1


if __name__ == "__main__":
    main(sys.argv[1:])
