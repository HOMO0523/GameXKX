"""Transient real-PIE acceptance probe for the BlockShield status icon.

The probe stays inside the pure-2D Desktop Training flow.  It never saves and
uses the existing development-only card fixture to make one deterministic
self-targeting Block card playable in the current Training battle.
"""

from __future__ import annotations

import argparse
import json

import unreal


HUD_MAP_TOKEN = "L_DesktopTrainingHUD"
STAGE_ID = "Training.Normal.1-1"
BLOCK_CARD_ID = "Profession.Guard.BuDongRuShan"
BLOCK_ICON_ID = "BlockShield"
BLOCK_TEXTURE_PATH = (
    "/Game/GameXXK/UI/Battle/StatusIcons/"
    "T_BattleStatus_BlockShield.T_BattleStatus_BlockShield"
)


def _call(obj, name, *args):
    fn = getattr(obj, name, None) if obj is not None else None
    if not callable(fn):
        return None
    try:
        return fn(*args)
    except Exception as exc:  # pragma: no cover - executed inside Unreal
        return f"ERR:{exc}"


def _text(value) -> str:
    return "" if value is None else str(value)


def _world():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    return editor.get_game_world() if editor else None


def _objects():
    world = _world()
    controller = unreal.GameplayStatics.get_player_controller(world, 0) if world else None
    workbench = _call(controller, "get_desktop_training_workbench_widget_for_test")
    subsystem = _call(workbench, "get_mvp_subsystem")
    board = _call(controller, "get_battle_board_widget_for_test")
    return world, controller, workbench, subsystem, board


def _runtime_snapshot(subsystem):
    state = _call(subsystem, "get_runtime_state_copy")
    if state is None or isinstance(state, str):
        return {}, None
    battle = getattr(getattr(state, "card_run", None), "active_battle", None)
    hand = []
    for card in list(getattr(getattr(battle, "deck", None), "hand", []) or []):
        hand.append(
            {
                "instance_id": _text(getattr(card, "instance_id", "")),
                "card_id": _text(getattr(card, "card_id", "")),
                "owner_unit_id": _text(getattr(card, "owner_unit_id", "")),
            }
        )
    units = []
    for unit in list(getattr(battle, "units", []) or []):
        statuses = [
            {
                "status": _text(getattr(stack, "status", "")),
                "stacks": int(getattr(stack, "stacks", 0)),
            }
            for stack in list(getattr(unit, "statuses", []) or [])
            if int(getattr(stack, "stacks", 0)) > 0
        ]
        units.append(
            {
                "unit_id": _text(getattr(unit, "unit_id", "")),
                "side": _text(getattr(unit, "side", "")),
                "living": bool(getattr(unit, "is_living", getattr(unit, "b_living", True))),
                "armor": int(getattr(unit, "armor", 0)),
                "statuses": statuses,
            }
        )
    return {
        "screen": _text(getattr(state, "screen", "")),
        "quest_state": _text(getattr(state, "quest_state", "")),
        "has_active_card_battle": bool(
            getattr(getattr(state, "card_run", None), "has_active_card_battle", False)
        ),
        "hand": hand,
        "units": units,
    }, state


def _visibility(widget) -> str:
    value = _call(widget, "get_visibility")
    if value is None and widget is not None:
        try:
            value = widget.get_editor_property("visibility")
        except Exception:  # pragma: no cover - executed inside Unreal
            value = None
    return _text(value)


def _vector2(value):
    if value is None or isinstance(value, str):
        return None
    try:
        return {"x": float(value.x), "y": float(value.y)}
    except Exception:  # pragma: no cover - executed inside Unreal
        return None


def _geometry_snapshot(widget):
    geometry = _call(widget, "get_cached_geometry")
    if geometry is None or isinstance(geometry, str):
        return None
    local_size = _call(geometry, "get_local_size")
    absolute_origin = _call(geometry, "local_to_absolute", unreal.Vector2D(0.0, 0.0))
    absolute_max = None
    if local_size is not None and not isinstance(local_size, str):
        absolute_max = _call(geometry, "local_to_absolute", local_size)
    return {
        "local_size": _vector2(local_size),
        "absolute_origin": _vector2(absolute_origin),
        "absolute_max": _vector2(absolute_max),
    }


def _fixture_apply_result(value):
    """Normalize UE Python's bool/out-parameter return variants."""
    if value is None:
        return False, ""
    if isinstance(value, str):
        # UE 5.8 returns the sole FString out parameter directly.  Empty text
        # therefore means the bool return succeeded with no error.
        return True, value
    if isinstance(value, (tuple, list)):
        return (
            bool(value[0]) if value else False,
            "" if len(value) < 2 or value[1] is None else str(value[1]),
        )
    if isinstance(value, dict):
        return (
            bool(value.get("return_value", value.get("ok", False))),
            str(value.get("out_error", value.get("error", "")) or ""),
        )
    return bool(value), ""


def _resource_path(resource) -> str:
    if resource is None:
        return ""
    value = _call(resource, "get_path_name")
    return _text(value or resource)


def _brush_resource_path(image) -> str:
    if image is None:
        return ""
    brush = _call(image, "get_brush")
    if brush is None or isinstance(brush, str):
        try:
            brush = image.get_editor_property("brush")
        except Exception:  # pragma: no cover - executed inside Unreal
            return ""
    resource = _call(brush, "get_resource_object")
    if resource is None or isinstance(resource, str):
        try:
            resource = brush.get_editor_property("resource_object")
        except Exception:  # pragma: no cover - executed inside Unreal
            resource = None
    return _resource_path(resource)


def _rendered_block_badges(board, runtime):
    rendered = []
    if board is None or isinstance(board, str):
        return rendered
    for unit in runtime.get("units", []):
        unit_id = unit.get("unit_id", "")
        if not unit_id:
            continue
        hud = _call(board, "get_projected_unit_hud_for_test", unreal.Name(unit_id))
        status_widget = _call(hud, "get_status_effects_widget_for_test")
        count = _call(status_widget, "get_icon_count_for_test")
        try:
            count = int(count)
        except (TypeError, ValueError):
            count = 0
        row = _call(status_widget, "get_widget_from_name", "BattleUnitStatusEffectsRow")
        for index in range(max(0, count)):
            icon_id = _text(_call(status_widget, "get_icon_id_for_test", index))
            if icon_id.casefold() != BLOCK_ICON_ID.casefold():
                continue
            icon_widget = _call(row, "get_child_at", index)
            icon_image = _call(icon_widget, "get_widget_from_name", "BattleStatusIconImage")
            glyph = _call(icon_widget, "get_widget_from_name", "BattleStatusIconGlyph")
            rendered.append(
                {
                    "unit_id": unit_id,
                    "icon_id": icon_id,
                    "displayed_stack": _text(
                        _call(status_widget, "get_icon_displayed_stack_for_test", index)
                    ),
                    "image_visibility": _visibility(icon_image),
                    "glyph_visibility": _visibility(glyph),
                    "texture_path": _brush_resource_path(icon_image),
                    "texture_asset_loaded": unreal.load_asset(BLOCK_TEXTURE_PATH) is not None,
                }
            )
    return rendered


def _snapshot(phase: str, extra=None):
    world, controller, workbench, subsystem, board = _objects()
    runtime, _state = _runtime_snapshot(subsystem)
    map_name = _text(_call(world, "get_map_name") or _call(world, "get_name"))
    badges = _rendered_block_badges(board, runtime)
    central_status_icon = _call(board, "get_widget_from_name", "BattleCinematicStatusIcon")
    design_stage = _call(board, "get_battle_design_stage_for_test")
    targeting_pointer = _call(board, "get_targeting_pointer_position_for_test")
    targeting_pointer_absolute = None
    stage_geometry = _call(design_stage, "get_cached_geometry")
    if stage_geometry is not None and not isinstance(stage_geometry, str):
        targeting_pointer_absolute = _call(
            stage_geometry,
            "local_to_absolute",
            targeting_pointer,
        )
    errors = []
    if world is None:
        errors.append("pie_world_missing")
    if HUD_MAP_TOKEN not in map_name:
        errors.append("not_desktop_training_hud_map")
    payload = {
        "ok": not errors,
        "phase": phase,
        "map": map_name,
        "controller": _text(_call(controller, "get_name")),
        "workbench_present": workbench is not None and not isinstance(workbench, str),
        "workbench_visible": bool(_call(workbench, "is_workbench_visible_for_test")),
        "battle_board_present": board is not None and not isinstance(board, str),
        "battle_board_visible": bool(_call(board, "is_battle_board_visible")),
        "battle_board_geometry": _geometry_snapshot(board),
        "battle_design_stage_geometry": _geometry_snapshot(design_stage),
        "targeting_pointer": _vector2(targeting_pointer),
        "targeting_pointer_absolute": _vector2(targeting_pointer_absolute),
        "runtime": runtime,
        "block_badges": badges,
        "central_status_icon_visibility": _visibility(central_status_icon),
        "errors": errors,
    }
    if extra:
        payload.update(extra)
    return payload


def _start_challenge():
    _world_value, controller, workbench, subsystem, board = _objects()
    if workbench is None or isinstance(workbench, str):
        _call(controller, "set_desktop_training_workbench_enabled_for_test", True)
        workbench = _call(controller, "get_desktop_training_workbench_widget_for_test")
    selected = bool(_call(workbench, "select_stage_for_test", unreal.Name(STAGE_ID)))
    challenged = bool(_call(workbench, "click_challenge_for_test"))
    active = bool(_call(subsystem, "is_training_challenge_battle_active"))
    payload = _snapshot(
        "challenge",
        {
            "selected_stage": selected,
            "challenge_clicked": challenged,
            "training_challenge_battle_active": active,
        },
    )
    if not selected:
        payload["errors"].append("stage_selection_failed")
    if not challenged or not active:
        payload["errors"].append("challenge_did_not_enter_battle")
    payload["ok"] = not payload["errors"]
    return payload


def _seed_block_card():
    _world_value, _controller, _workbench, subsystem, _board = _objects()
    result = _call(
        subsystem,
        "apply_card_tooltip_fixture_for_test",
        unreal.Name(BLOCK_CARD_ID),
    )
    applied, error = _fixture_apply_result(result)
    payload = _snapshot(
        "seed-block-card",
        {"fixture_applied": applied, "fixture_error": error, "card_id": BLOCK_CARD_ID},
    )
    if not applied:
        payload["errors"].append("block_card_fixture_failed")
    if not any(card.get("card_id") == BLOCK_CARD_ID for card in payload["runtime"].get("hand", [])):
        payload["errors"].append("block_card_not_in_hand")
    payload["ok"] = not payload["errors"]
    return payload


def _play_block_card():
    _world_value, _controller, _workbench, subsystem, board = _objects()
    runtime_before, _state = _runtime_snapshot(subsystem)
    card = next(
        (item for item in runtime_before.get("hand", []) if item.get("card_id") == BLOCK_CARD_ID),
        None,
    )
    played = False
    if card and board is not None and not isinstance(board, str):
        played = bool(
            _call(board, "click_card_in_hand", unreal.Name(card.get("instance_id", "")))
        )
    payload = _snapshot(
        "play-block-card",
        {
            "card": card,
            "card_played": played,
            "presentation_active": bool(_call(board, "is_battle_presentation_active_for_test")),
            "status_presentation_active": bool(
                _call(board, "is_battle_status_presentation_active_for_test")
            ),
            "active_status_icon_id": _text(
                _call(board, "get_active_battle_status_icon_id_for_test")
            ),
        },
    )
    if not card:
        payload["errors"].append("block_card_missing_before_play")
    if not played:
        payload["errors"].append("block_card_play_failed")
    payload["ok"] = not payload["errors"]
    return payload


def _observe():
    payload = _snapshot("observe")
    block_stacks = []
    for unit in payload["runtime"].get("units", []):
        for status in unit.get("statuses", []):
            if "BLOCK" in status.get("status", "").upper():
                block_stacks.append(
                    {"unit_id": unit.get("unit_id", ""), "stacks": status.get("stacks", 0)}
                )
    payload["block_runtime_stacks"] = block_stacks
    valid_badges = [
        badge
        for badge in payload["block_badges"]
        if badge.get("texture_asset_loaded") is True
        and (
            not badge.get("texture_path")
            or badge.get("texture_path") == BLOCK_TEXTURE_PATH
        )
        and (
            not badge.get("glyph_visibility")
            or "COLLAPSED" in badge.get("glyph_visibility", "").upper()
        )
        and (
            not badge.get("image_visibility")
            or "COLLAPSED" not in badge.get("image_visibility", "").upper()
        )
    ]
    payload["validated_block_badges"] = valid_badges
    if not block_stacks:
        payload["errors"].append("runtime_block_status_missing")
    if not payload["block_badges"]:
        payload["errors"].append("rendered_block_badge_missing")
    if not valid_badges:
        payload["errors"].append("block_badge_texture_or_fallback_invalid")
    payload["ok"] = not payload["errors"]
    return payload


def _clear():
    _world_value, _controller, _workbench, subsystem, _board = _objects()
    active_before = bool(_call(subsystem, "is_card_tooltip_fixture_active_for_test"))
    if active_before:
        _call(subsystem, "clear_card_tooltip_fixture_for_test")
    return _snapshot("clear", {"fixture_was_active": active_before})


def _return_workbench():
    _world_value, controller, _workbench, subsystem, _board = _objects()
    returned = bool(_call(subsystem, "cancel_training_challenge_to_workbench"))
    _call(controller, "refresh_player_flow_widgets_for_test")
    payload = _snapshot("return-workbench", {"returned": returned})
    if not returned:
        payload["errors"].append("challenge_return_failed")
    if not payload.get("workbench_visible"):
        payload["errors"].append("workbench_not_restored")
    payload["ok"] = not payload["errors"]
    return payload


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--phase",
        choices=(
            "challenge",
            "seed-block-card",
            "play-block-card",
            "observe",
            "clear",
            "return-workbench",
        ),
        required=True,
    )
    args = parser.parse_args(argv)
    handlers = {
        "challenge": _start_challenge,
        "seed-block-card": _seed_block_card,
        "play-block-card": _play_block_card,
        "observe": _observe,
        "clear": _clear,
        "return-workbench": _return_workbench,
    }
    print(json.dumps(handlers[args.phase](), ensure_ascii=False, sort_keys=True))


if __name__ == "__main__":
    main()
