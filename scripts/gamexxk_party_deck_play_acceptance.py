#!/usr/bin/env python3
"""PIE-only acceptance extensions for the PartyDeck/route gameplay contract.

This runner deliberately *does not* start or stop Unreal, compile C++, save a
game, or change any asset/map.  It connects to an already-running UE MCP
session and asks ``gamexxk_probe_party_deck_runtime.py`` to inspect the live
state or invoke the same public UMG/Subsystem actions that a player uses.  It
never fabricates a reward, event, party, or route panel by overwriting private
runtime state.  Forced branches remain the responsibility of the named C++
automation tests printed by ``--print-automation-plan``.

Run the normal real-play flow first with ``--keep-pie``.  Then execute this
script against that same PIE session.  Its JSON report is intentionally
evidence-oriented: every success condition names the concrete widget/runtime
field it observed, rather than inferring behaviour from a map transition.
"""

from __future__ import annotations

import argparse
import importlib.util
import json
import re
import sys
import time
from pathlib import Path
from typing import Any, Callable


PROJECT_ROOT = Path(__file__).resolve().parents[1]
REPORT_DIR = PROJECT_ROOT / "Saved" / "HarnessReports"
RUNTIME_PROBE = "Content/Python/gamexxk_probe_party_deck_runtime.py"
CATALOG_PATH = PROJECT_ROOT / "docs" / "design" / "2026-08-11-full-card-catalog.md"
PARTY_DECK_ACCEPTANCE_VERSION = "2026-07-17.4"

MANUAL_TARGET_MODES = frozenset({"SingleEnemy", "SingleAlly", "OtherAlly", "AnyLivingUnit"})
TASK_NPC_PREFIX = "Npc."
EVENT_NIU_HUAN_IDS = frozenset({"Event.NiuHuan", "Npc.Event.NiuHuan", "Npc.NiuHuan", "NiuHuan"})
ROUTE_PANEL_PRIMARY_ACTIONS = {
    "Chest": "TakeGold",
    "Camp": "CampRest",
    "Merchant": "MerchantLeave",
}
TASK_NPC_DISPLAY_NAMES = {
    "Npc.TusiChief": "土司首领",
    "Npc.SongJinBao": "宋金宝",
    "Npc.YueBai": "月白",
    "Npc.ZhouGuangZu": "周光祖",
    "Npc.JinGui": "金贵",
    "Npc.QiongMeiEr": "琼么儿",
}

# These are the focused, existing deterministic fixtures for branches that a
# real player route cannot safely force on demand (for example a full twelve
# card reward list or both identities behind a random event node).  The PIE
# probe supplements them with visible/world-facing evidence; it does not
# overwrite RuntimeState to manufacture a green result.
AUTOMATION_TESTS_BY_EVIDENCE: dict[str, tuple[str, ...]] = {
    "card_targeting_arrow_cancel_commit": (
        "GameXXK.Integration.CardBattle.BoardTargeting",
        "GameXXK.Integration.CardBattle.ControllerInputBridge",
    ),
    "three_choice_reward_and_replacement": (
        "GameXXK.Integration.CardBattle.BoardRewards",
        "GameXXK.Integration.CardBattle.BoardRewardReplacement",
        "GameXXK.Integration.CardRoute.RewardChoice",
        "GameXXK.Integration.CardRoute.RewardGate",
    ),
    "event_npc_vs_niu_huan": (
        "GameXXK.Integration.CardRoute.EventOffer",
        "GameXXK.Integration.CardRoute.EventSupport",
        "GameXXK.Integration.CardRoute.QuestNpc",
        "GameXXK.MVP.RouteEncounter.SceneActorInteraction",
        "GameXXK.MVP.RouteEncounter.Panel.EventIdentityAndExplicitChoice",
        "GameXXK.MVP.RouteEncounter.Panel.NiuHuanChestCampMerchantChoices",
        "GameXXK.MVP.RouteEncounter.Panel.VisibleChoicesResolveOnlyOnClick",
    ),
    "camp_merchant_chest_route_panels": (
        "GameXXK.Integration.CardRoute.Lifecycle",
        "GameXXK.MVP.RouteEncounter.SceneActorInteraction",
        "GameXXK.MVP.RouteEncounter.Panel.NiuHuanChestCampMerchantChoices",
        "GameXXK.MVP.RouteEncounter.Panel.VisibleChoicesResolveOnlyOnClick",
    ),
    "hero_partner_task_npc_cap_and_visuals": (
        "GameXXK.Integration.CardRoute.CompanionBattleProgression",
        "GameXXK.Integration.CardRoute.QuestNpc",
        "GameXXK.MVP.Battle.PartyDeckVisualMapping",
        "GameXXK.MVP.Companion.Facade.TownOnlyConfiguration",
    ),
}


def get_automation_test_plan() -> dict[str, tuple[str, ...]]:
    """Return the immutable test names needed for deterministic fixture proof."""
    return {key: tuple(value) for key, value in AUTOMATION_TESTS_BY_EVIDENCE.items()}


def _load_ue_mcp_client():
    """Load the project-local MCP client without requiring the scripts package."""
    client_path = PROJECT_ROOT / "scripts" / "ue_mcp_client.py"
    spec = importlib.util.spec_from_file_location("gamexxk_party_deck_ue_mcp_client", client_path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Cannot load UE MCP client from {client_path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules.setdefault(spec.name, module)
    spec.loader.exec_module(module)
    return module.UnrealMCPClient


def _last_json(result: dict[str, Any]) -> dict[str, Any]:
    stdout = str(result.get("stdout", "")).strip()
    if not stdout:
        raise RuntimeError(f"PartyDeck runtime probe produced no JSON: {result}")
    try:
        parsed = json.loads(stdout.splitlines()[-1])
    except json.JSONDecodeError as exc:
        raise RuntimeError(f"PartyDeck runtime probe emitted invalid JSON: {stdout[-1200:]}") from exc
    if not isinstance(parsed, dict):
        raise RuntimeError(f"PartyDeck runtime probe must return an object: {parsed!r}")
    return parsed


def _as_dict(value: Any) -> dict[str, Any]:
    return value if isinstance(value, dict) else {}


def _as_list(value: Any) -> list[Any]:
    return value if isinstance(value, list) else []


def _name(value: Any) -> str:
    return str(value or "").strip()


def _card_id(value: dict[str, Any]) -> str:
    return _name(value.get("card_id"))


def _instance_id(value: dict[str, Any]) -> str:
    return _name(value.get("instance_id"))


def _active_battle(card_run: dict[str, Any]) -> dict[str, Any]:
    return _as_dict(card_run.get("active_battle"))


def _active_hand(card_run: dict[str, Any]) -> list[dict[str, Any]]:
    """Read the hand from the runtime's real ``CardRun.ActiveBattle.Deck`` shape.

    A small direct-hand fallback keeps offline evidence fixtures readable, but
    the live probe always emits the nested representation.
    """
    nested_hand = _as_list(_active_battle(card_run).get("hand"))
    raw_hand = nested_hand if nested_hand else _as_list(card_run.get("hand"))
    return [item for item in raw_hand if isinstance(item, dict)]


def load_card_target_catalog(path: Path = CATALOG_PATH) -> dict[str, dict[str, Any]]:
    """Parse immutable identity/cost/target fields from the verified catalog.

    ``GameXXK.Data.CardDocumentation`` verifies this generated Markdown against
    ``FGameXXKCardCatalog::GetAllCardDefinitions``. Reading that stable table
    avoids coupling the live runner to C++ helper names such as ``AddHero`` or
    ``AddHealer`` while still avoiding a rule reimplementation.
    """
    source = path.read_text(encoding="utf-8")
    pattern = re.compile(
        r"^\|\s*\d+\s*\|\s*[^|]+\|\s*`(?P<card_id>[^`]+)`\s*\|"
        r"\s*[^|]+\|\s*(?P<energy>\d+)\s*气\s*/\s*(?P<mana>\d+)\s*内\s*\|"
        r".*?TargetMode=(?P<target_mode>[A-Za-z0-9_]+)",
        re.MULTILINE,
    )
    result: dict[str, dict[str, Any]] = {}
    for match in pattern.finditer(source):
        card_id = match.group("card_id")
        result[card_id] = {
            "target_mode": match.group("target_mode"),
            "energy_cost": int(match.group("energy")),
            "mana_cost": int(match.group("mana")),
        }
    if len(result) != 198:
        raise RuntimeError(f"Card catalog parser read {len(result)} cards; expected the approved 198")
    return result


def select_manual_target_card(
    hand: list[dict[str, Any]],
    catalog: dict[str, dict[str, Any]],
    shared_energy: int,
) -> dict[str, Any]:
    """Return the first legal manual-target card by *stable instance ID*.

    No UI index is accepted or returned.  A missing result is a useful test
    failure: it means the caller should end the real player phase and draw a
    new hand, never guess a card slot.
    """
    for instance in hand:
        if not isinstance(instance, dict):
            continue
        card_id = _card_id(instance)
        instance_id = _instance_id(instance)
        definition = _as_dict(catalog.get(card_id))
        target_mode = _name(definition.get("target_mode"))
        cost = int(definition.get("energy_cost", 0) or 0)
        if instance_id and target_mode in MANUAL_TARGET_MODES and cost <= int(shared_energy):
            return {
                "instance_id": instance_id,
                "card_id": card_id,
                "owner_unit_id": _name(instance.get("owner_unit_id")),
                "target_mode": target_mode,
                "energy_cost": cost,
            }
    return {}


def evaluate_targeting_state(snapshot: dict[str, Any], card_instance_id: str, target_unit_id: str) -> dict[str, Any]:
    board = _as_dict(snapshot.get("battle_board"))
    units = [item for item in _as_list(snapshot.get("battle_units")) if isinstance(item, dict)]
    target = next((item for item in units if _name(item.get("unit_id")) == target_unit_id), {})
    card_run = _as_dict(snapshot.get("card_run"))
    owner = _name(next((item.get("owner_unit_id") for item in _active_hand(card_run) if _name(item.get("instance_id")) == card_instance_id), ""))
    owner_unit = next((item for item in units if _name(item.get("unit_id")) == owner), {})
    legal_ids = {_name(value) for value in _as_list(board.get("legal_target_unit_ids"))}
    pointer = _as_dict(board.get("targeting_pointer_position"))
    source = _as_dict(board.get("targeting_source_position"))
    pointer_has_coordinates = "x" in pointer and "y" in pointer
    source_has_coordinates = "x" in source and "y" in source
    target_highlighted = bool(target.get("is_card_target_highlighted")) and bool(target.get("is_card_target_outline_enabled"))
    owner_highlighted = bool(owner_unit.get("is_card_target_highlighted")) if owner_unit else False
    # Enemy cards must not accidentally turn the casting party unit into a
    # legal target.  Ally cards, by contrast, may correctly outline the owner
    # together with other legal allies (and can explicitly target self).
    owner_highlight_is_valid = not bool(target.get("is_enemy_unit")) or not owner_highlighted
    ok = bool(
        board.get("is_in_viewport")
        and board.get("is_card_targeting_active")
        and _name(board.get("pending_card_instance_id")) == card_instance_id
        and target_unit_id in legal_ids
        and target_highlighted
        and owner_highlight_is_valid
        and pointer_has_coordinates
        and source_has_coordinates
    )
    return {
        "ok": ok,
        "pending_card_instance_id": _name(board.get("pending_card_instance_id")),
        "target_unit_id": target_unit_id,
        "target_highlighted": target_highlighted,
        "owner_unit_id": owner,
        "owner_highlighted": owner_highlighted,
        "owner_highlight_is_valid": owner_highlight_is_valid,
        "legal_target_unit_ids": sorted(legal_ids),
        "targeting_source_position": source,
        "targeting_pointer_position": pointer,
    }


def evaluate_reward_offer(snapshot: dict[str, Any], require_replacement: bool) -> dict[str, Any]:
    card_run = _as_dict(snapshot.get("card_run"))
    pending = _as_dict(card_run.get("pending_reward"))
    board = _as_dict(snapshot.get("battle_board"))
    runtime_offer = [_name(value) for value in _as_list(pending.get("card_ids")) if _name(value)]
    board_offer = [_name(value) for value in _as_list(board.get("pending_route_reward_card_ids")) if _name(value)]
    replacements = [_name(value) for value in _as_list(board.get("route_reward_replacement_card_ids")) if _name(value)]
    route_cards = [_name(value) for value in _as_list(card_run.get("route_card_ids")) if _name(value)]
    replacement_requested = bool(pending.get("requires_route_card_replacement"))
    offer_matches = runtime_offer == board_offer and len(runtime_offer) == 3 and len(set(runtime_offer)) == 3
    replacement_ok = (not require_replacement) or (
        replacement_requested and len(route_cards) == 12 and len(replacements) == 12 and set(replacements) == set(route_cards)
    )
    return {
        "ok": bool(board.get("has_pending_route_reward")) and offer_matches and replacement_ok,
        "offer_card_ids": runtime_offer,
        "board_offer_card_ids": board_offer,
        "route_card_count": len(route_cards),
        "replacement_requested": replacement_requested,
        "replacement_count": len(replacements),
        "replacement_card_ids": replacements,
    }


def _is_empty_identity(value: Any) -> bool:
    return _name(value) in {"", "None", "NAME_None"}


def _evaluate_focused_route_panel_open(action: dict[str, Any], panel: dict[str, Any]) -> dict[str, Any]:
    """Verify that a visible route panel came from the player interaction gate.

    A scene actor lookup alone is not player evidence: a matching actor must be
    the interaction component's focused actor, must become the controller's
    saved source, and must remain unresolved until a visible choice is clicked.
    """
    actor_path = _name(action.get("actor"))
    focused_actor_path = _name(action.get("focused_actor_path"))
    action_source_actor_path = _name(action.get("source_actor_path"))
    panel_source_actor_path = _name(panel.get("source_actor_path"))
    opened_by_pawn_binding = (
        bool(action.get("ok"))
        and _name(action.get("kind")) == "open_route_encounter_panel_only"
        and _name(action.get("input_path")) == "pawn_interaction_binding"
    )
    focus_rejected_first = action.get("rejected_when_unfocused") is True
    source_not_resolved = action.get("source_last_interaction_successful_before_choice") is False
    same_actor_context = bool(
        actor_path
        and actor_path == focused_actor_path
        and actor_path == action_source_actor_path
        and actor_path == panel_source_actor_path
    )
    return {
        "ok": opened_by_pawn_binding and focus_rejected_first and source_not_resolved and same_actor_context,
        "opened_by_pawn_binding": opened_by_pawn_binding,
        "focus_rejected_first": focus_rejected_first,
        "source_not_resolved_before_choice": source_not_resolved,
        "same_actor_context": same_actor_context,
        "actor_path": actor_path,
        "focused_actor_path": focused_actor_path,
        "action_source_actor_path": action_source_actor_path,
        "panel_source_actor_path": panel_source_actor_path,
    }


def evaluate_event_panel_open(
    snapshot: dict[str, Any],
    event_kind: str,
    event_npc_id_before_open: str,
    player_gold_before_open: int,
) -> dict[str, Any]:
    """Verify that the F-equivalent action opened a specific panel only.

    This predicate intentionally checks that the pending identity and route
    state survived opening the UI. Resolution is proved separately after a
    visible panel button is triggered.
    """
    card_run = _as_dict(snapshot.get("card_run"))
    pending_event = _as_dict(card_run.get("pending_event"))
    party = _as_dict(card_run.get("party_selection"))
    quest_npc = _as_dict(party.get("quest_npc"))
    runtime = _as_dict(snapshot.get("runtime_state"))
    panel = _as_dict(snapshot.get("route_panel"))
    action = _as_dict(snapshot.get("action"))
    event_npc_id = _name(pending_event.get("event_npc_id"))
    active_npc_id = _name(quest_npc.get("npc_id"))
    primary_action = _name(panel.get("primary_action"))
    secondary_action = _name(panel.get("secondary_action"))
    speaker = _name(panel.get("speaker"))
    primary_label = _name(panel.get("primary_label"))
    panel_open = bool(panel.get("is_in_viewport")) and bool(panel.get("is_open"))
    panel_class_ok = "GameXXKRouteEncounterPanelWidget" in _name(panel.get("class"))
    focus_gate = _evaluate_focused_route_panel_open(action, panel)
    opened_by_scene_interaction = bool(focus_gate["ok"])
    unchanged_before_choice = (
        _name(runtime.get("screen")) == "RouteEvent"
        and event_npc_id == event_npc_id_before_open
        and int(runtime.get("player_gold", 0) or 0) == int(player_gold_before_open)
        and _is_empty_identity(active_npc_id)
    )

    if event_kind == "task_npc":
        expected_speaker = TASK_NPC_DISPLAY_NAMES.get(event_npc_id_before_open, event_npc_id_before_open)
        identity_ok = event_npc_id_before_open.startswith(TASK_NPC_PREFIX) and event_npc_id_before_open not in EVENT_NIU_HUAN_IDS
        action_ok = primary_action == "AcceptTaskNpcSupport" and secondary_action == "TakeHealingPowder"
        presentation_ok = speaker == expected_speaker and expected_speaker in primary_label
        ok = panel_open and panel_class_ok and opened_by_scene_interaction and unchanged_before_choice and identity_ok and action_ok and presentation_ok
    elif event_kind == "niu_huan":
        identity_ok = event_npc_id_before_open in EVENT_NIU_HUAN_IDS
        action_ok = primary_action == "TakeGold" and secondary_action == "TakeHealingPowder"
        presentation_ok = speaker == "牛欢"
        ok = panel_open and panel_class_ok and opened_by_scene_interaction and unchanged_before_choice and identity_ok and action_ok and presentation_ok
    else:
        raise ValueError(f"Unsupported event kind: {event_kind}")
    return {
        "ok": ok,
        "event_kind": event_kind,
        "event_npc_id": event_npc_id,
        "event_npc_id_before_open": event_npc_id_before_open,
        "panel_open": panel_open,
        "panel_class_ok": panel_class_ok,
        "opened_by_scene_interaction": opened_by_scene_interaction,
        "unchanged_before_choice": unchanged_before_choice,
        "speaker": speaker,
        "primary_action": primary_action,
        "secondary_action": secondary_action,
        "primary_label": primary_label,
        "active_temporary_npc_id_before_choice": active_npc_id,
        "focus_gate": focus_gate,
    }


def evaluate_event_resolution(
    snapshot: dict[str, Any],
    event_kind: str,
    event_npc_id_before_open: str,
    player_gold_before_open: int,
) -> dict[str, Any]:
    """Verify the result after the explicit visible primary panel action."""
    card_run = _as_dict(snapshot.get("card_run"))
    pending_event = _as_dict(card_run.get("pending_event"))
    party = _as_dict(card_run.get("party_selection"))
    quest_npc = _as_dict(party.get("quest_npc"))
    runtime = _as_dict(snapshot.get("runtime_state"))
    panel = _as_dict(snapshot.get("route_panel"))
    action = _as_dict(snapshot.get("action"))
    active_npc_id = _name(quest_npc.get("npc_id"))
    active_temporary_npc_id = _name(card_run.get("active_temporary_quest_npc_id"))
    selected_cards = [_name(value) for value in _as_list(quest_npc.get("selected_card_ids")) if _name(value)]
    try:
        pending_event_source_node_id = int(pending_event.get("source_node_id", -1))
    except (TypeError, ValueError):
        pending_event_source_node_id = -1
    pending_event_cleared = (
        _is_empty_identity(_name(pending_event.get("event_npc_id")))
        and pending_event_source_node_id < 0
    )
    panel_closed = not bool(panel.get("is_open"))
    source_context_cleared = _is_empty_identity(panel.get("source_actor_path"))
    route_completed = _name(runtime.get("screen")) == "DungeonMap"
    action_triggered = bool(action.get("ok")) and _name(action.get("kind")) == "trigger-route-encounter-primary"
    source_actor_path_before_choice = _name(action.get("source_actor_path_before_choice"))
    source_completed_after_choice = action.get("source_last_interaction_successful_after_choice") is True
    pending_route_node_cleared = int(runtime.get("pending_route_node_id", -1) or -1) < 0

    if event_kind == "task_npc":
        expected_action = "AcceptTaskNpcSupport"
        final_state_ok = (
            active_npc_id == event_npc_id_before_open
            and active_temporary_npc_id == event_npc_id_before_open
            and len(selected_cards) == 3
        )
        event_identity_ok = event_npc_id_before_open in TASK_NPC_DISPLAY_NAMES
        event_reward_increased_gold = False
    elif event_kind == "niu_huan":
        expected_action = "TakeGold"
        final_state_ok = (
            _is_empty_identity(active_npc_id)
            and _is_empty_identity(active_temporary_npc_id)
            and not selected_cards
        )
        event_identity_ok = event_npc_id_before_open in EVENT_NIU_HUAN_IDS
        event_reward_increased_gold = int(runtime.get("player_gold", 0) or 0) > int(player_gold_before_open)
    else:
        raise ValueError(f"Unsupported event kind: {event_kind}")

    action_matches = _name(action.get("selected_action")) == expected_action
    ok = (
        action_triggered
        and action_matches
        and panel_closed
        and source_context_cleared
        and source_actor_path_before_choice
        and source_completed_after_choice
        and route_completed
        and pending_route_node_cleared
        and pending_event_cleared
        and event_identity_ok
        and final_state_ok
    )
    if event_kind == "niu_huan":
        ok = bool(ok and event_reward_increased_gold)
    return {
        "ok": ok,
        "event_kind": event_kind,
        "event_npc_id_before_open": event_npc_id_before_open,
        "selected_action": _name(action.get("selected_action")),
        "action_triggered": action_triggered,
        "action_matches": action_matches,
        "panel_closed": panel_closed,
        "source_context_cleared": source_context_cleared,
        "source_actor_path_before_choice": source_actor_path_before_choice,
        "source_completed_after_choice": source_completed_after_choice,
        "route_completed": route_completed,
        "pending_route_node_cleared": pending_route_node_cleared,
        "pending_event_cleared": pending_event_cleared,
        "pending_event_source_node_id": pending_event_source_node_id,
        "active_temporary_npc_id": active_npc_id,
        "active_temporary_quest_npc_id": active_temporary_npc_id,
        "selected_card_count": len(selected_cards),
        "player_gold_before_open": int(player_gold_before_open),
        "player_gold_after_action": int(runtime.get("player_gold", 0) or 0),
        "event_reward_increased_gold": event_reward_increased_gold,
    }


def evaluate_route_panel(snapshot: dict[str, Any], expected_kind: str) -> dict[str, Any]:
    runtime = _as_dict(snapshot.get("runtime_state"))
    panel = _as_dict(snapshot.get("route_panel"))
    action = _as_dict(snapshot.get("action"))
    focus_gate = _evaluate_focused_route_panel_open(action, panel)
    observed_kind = _name(panel.get("kind"))
    pending_kind = _name(runtime.get("pending_route_node_kind"))
    expected_screen = "RouteEvent" if expected_kind == "Chest" else f"Route{expected_kind}"
    ok = bool(
        panel.get("is_in_viewport")
        and panel.get("is_open")
        and panel.get("is_explicit")
        and _name(panel.get("primary_action")) not in {"", "None"}
        and observed_kind == expected_kind
        and pending_kind == expected_kind
        and _name(runtime.get("screen")) == expected_screen
        and bool(focus_gate["ok"])
    )
    return {
        "ok": ok,
        "expected_kind": expected_kind,
        "observed_kind": observed_kind,
        "pending_route_node_kind": pending_kind,
        "screen": _name(runtime.get("screen")),
        "is_explicit": bool(panel.get("is_explicit")),
        "is_in_viewport": bool(panel.get("is_in_viewport")),
        "focus_gate": focus_gate,
    }


def evaluate_route_panel_resolution(
    snapshot: dict[str, Any],
    expected_kind: str,
    pending_node_id_before_open: int,
    player_gold_before_open: int,
    player_hp_before_open: int,
) -> dict[str, Any]:
    """Verify one visible primary action completes its exact route node cleanly."""
    runtime = _as_dict(snapshot.get("runtime_state"))
    panel = _as_dict(snapshot.get("route_panel"))
    action = _as_dict(snapshot.get("action"))
    card_run = _as_dict(snapshot.get("card_run"))
    pending_event = _as_dict(card_run.get("pending_event"))
    expected_action = ROUTE_PANEL_PRIMARY_ACTIONS.get(expected_kind, "")
    selected_action = _name(action.get("selected_action"))
    action_triggered = bool(action.get("ok")) and _name(action.get("kind")) == "trigger-route-encounter-primary"
    source_actor_path_before_choice = _name(action.get("source_actor_path_before_choice"))
    source_completed_after_choice = action.get("source_last_interaction_successful_after_choice") is True
    panel_closed = not bool(panel.get("is_open"))
    source_context_cleared = _is_empty_identity(panel.get("source_actor_path"))
    route_completed = _name(runtime.get("screen")) == "DungeonMap"
    pending_node_cleared = int(runtime.get("pending_route_node_id", -1) or -1) < 0
    visited_node_ids = {int(value) for value in _as_list(runtime.get("visited_route_node_ids"))}
    original_node_visited = int(pending_node_id_before_open) in visited_node_ids
    pending_event_cleared = (
        _is_empty_identity(pending_event.get("event_npc_id"))
        and int(pending_event.get("source_node_id", -1) or -1) < 0
    )
    player_gold_after_action = int(runtime.get("player_gold", 0) or 0)
    player_hp_after_action = int(runtime.get("player_hp", 0) or 0)
    player_max_hp_after_action = int(runtime.get("player_max_hp", 0) or 0)
    if expected_kind == "Chest":
        effect_ok = player_gold_after_action > int(player_gold_before_open)
    elif expected_kind == "Camp":
        effect_ok = (
            player_max_hp_after_action > 0
            and player_hp_after_action == player_max_hp_after_action
            and player_hp_after_action >= int(player_hp_before_open)
        )
    elif expected_kind == "Merchant":
        effect_ok = player_gold_after_action == int(player_gold_before_open)
    else:
        raise ValueError(f"Unsupported route panel kind: {expected_kind}")
    ok = bool(
        expected_action
        and action_triggered
        and selected_action == expected_action
        and source_actor_path_before_choice
        and source_completed_after_choice
        and panel_closed
        and source_context_cleared
        and route_completed
        and pending_node_cleared
        and original_node_visited
        and pending_event_cleared
        and effect_ok
    )
    return {
        "ok": ok,
        "expected_kind": expected_kind,
        "expected_action": expected_action,
        "selected_action": selected_action,
        "action_triggered": action_triggered,
        "source_actor_path_before_choice": source_actor_path_before_choice,
        "source_completed_after_choice": source_completed_after_choice,
        "panel_closed": panel_closed,
        "source_context_cleared": source_context_cleared,
        "route_completed": route_completed,
        "pending_node_cleared": pending_node_cleared,
        "original_node_visited": original_node_visited,
        "pending_event_cleared": pending_event_cleared,
        "effect_ok": effect_ok,
        "player_gold_after_action": player_gold_after_action,
        "player_hp_after_action": player_hp_after_action,
        "player_max_hp_after_action": player_max_hp_after_action,
    }


def evaluate_party_cap_and_visuals(snapshot: dict[str, Any]) -> dict[str, Any]:
    card_run = _as_dict(snapshot.get("card_run"))
    party = _as_dict(card_run.get("party_selection"))
    active_partner = _name(party.get("active_permanent_companion_instance_id"))
    quest = _as_dict(party.get("quest_npc"))
    active_npc = _name(quest.get("npc_id"))
    raw_visuals = [item for item in _as_list(snapshot.get("party_visuals")) if isinstance(item, dict)]
    member_ids = {"Hero"}
    if active_partner and active_partner not in {"None", "NAME_None"}:
        member_ids.add(active_partner)
    if active_npc and active_npc not in {"None", "NAME_None"}:
        member_ids.add(active_npc)
    matching_visuals = [item for item in raw_visuals if _name(item.get("unit_id")) in member_ids]
    visual_ids = {_name(item.get("unit_id")) for item in matching_visuals if bool(item.get("has_visual"))}
    no_extra_party = all(_name(item.get("unit_id")) in member_ids for item in raw_visuals)
    count = len(member_ids)
    ok = count <= 3 and "Hero" in visual_ids and member_ids.issubset(visual_ids) and no_extra_party
    return {
        "ok": ok,
        "hero_present": "Hero" in member_ids,
        "active_permanent_companion_instance_id": active_partner,
        "active_temporary_npc_id": active_npc,
        "combat_member_count": count,
        "visual_unit_ids": sorted(visual_ids),
        "unexpected_visual_unit_ids": sorted({_name(item.get("unit_id")) for item in raw_visuals} - member_ids),
    }


class PartyDeckAcceptanceHarness:
    """Thin MCP client that never owns a PIE lifecycle."""

    def __init__(self, timeout: float) -> None:
        self.client = _load_ue_mcp_client()(timeout=timeout)
        self.events: list[dict[str, Any]] = []

    def event(self, name: str, **payload: Any) -> None:
        record = {"name": name, **payload}
        self.events.append(record)
        print(json.dumps(record, ensure_ascii=False), flush=True)

    def connect(self) -> None:
        if not self.client.connect():
            raise RuntimeError(f"Cannot connect to UE MCP at {self.client.endpoint}; start the editor and PIE first")
        self.event("mcp_connected", endpoint=self.client.endpoint)
        if not self.client.is_in_pie():
            raise RuntimeError("PartyDeck acceptance requires an already-running PIE session; this runner never starts one")

    def probe(self, action: str = "snapshot", **args: Any) -> dict[str, Any]:
        command = ["--action", action]
        for key, value in args.items():
            if value is None:
                continue
            command.extend([f"--{key.replace('_', '-')}", str(value)])
        result = self.client.run_project_python_file(RUNTIME_PROBE, command)
        parsed = _last_json(result)
        action_result = _as_dict(parsed.get("action"))
        self.event("party_deck_probe", action=action, ok=bool(parsed.get("ok")), action_ok=action_result.get("ok"), screen=_as_dict(parsed.get("runtime_state")).get("screen"))
        return parsed

    def require_action(self, action: str, **args: Any) -> dict[str, Any]:
        payload = self.probe(action, **args)
        action_result = _as_dict(payload.get("action"))
        if not action_result.get("ok"):
            raise RuntimeError(f"PIE PartyDeck action failed: {action}: {action_result}")
        return payload

    def assert_targeting(self) -> dict[str, Any]:
        catalog = load_card_target_catalog()
        snapshot = self.probe()
        card_run = _as_dict(snapshot.get("card_run"))
        battle = _active_battle(card_run)
        hand = _active_hand(card_run)
        selection = select_manual_target_card(hand, catalog, int(battle.get("shared_energy", 0) or 0))
        if not selection:
            raise RuntimeError("No playable manual-target card is in the live hand; end a real card phase and draw again before retrying")
        selected = self.require_action("click-card", card_instance_id=selection["instance_id"])
        board = _as_dict(selected.get("battle_board"))
        legal_ids = [_name(value) for value in _as_list(board.get("legal_target_unit_ids")) if _name(value)]
        if not legal_ids:
            raise RuntimeError(f"Selected manual-target card exposed no legal targets: {selection}; board={board}")
        target_id = legal_ids[0]
        pointer = next((item.get("screen_position") for item in _as_list(selected.get("battle_units")) if isinstance(item, dict) and _name(item.get("unit_id")) == target_id), {"x": 512, "y": 320})
        pointer_dict = _as_dict(pointer)
        after_pointer = self.require_action("update-pointer", x=int(pointer_dict.get("x", 512)), y=int(pointer_dict.get("y", 320)))
        preview = evaluate_targeting_state(after_pointer, selection["instance_id"], target_id)
        if not preview["ok"]:
            raise RuntimeError(f"Card preview did not produce arrow/highlighter evidence: {preview}")
        after_cancel = self.require_action("cancel-targeting")
        cancelled_board = _as_dict(after_cancel.get("battle_board"))
        cancelled_units = [item for item in _as_list(after_cancel.get("battle_units")) if isinstance(item, dict)]
        cancel_ok = (not bool(cancelled_board.get("is_card_targeting_active"))) and not any(bool(unit.get("is_card_target_highlighted")) for unit in cancelled_units)
        if not cancel_ok:
            raise RuntimeError(f"Card cancel left a targeting/highlight residue: board={cancelled_board}; units={cancelled_units}")
        self.require_action("click-card", card_instance_id=selection["instance_id"])
        committed = self.require_action("confirm-target", unit_id=target_id)
        committed_board = _as_dict(committed.get("battle_board"))
        post_hand = [_name(item.get("instance_id")) for item in _as_list(_as_dict(committed.get("card_run")).get("hand")) if isinstance(item, dict)]
        committed_ok = not bool(committed_board.get("is_card_targeting_active")) and selection["instance_id"] not in post_hand
        if not committed_ok:
            raise RuntimeError(f"Card commit did not clear targeting or move the stable instance: {committed_board}; hand={post_hand}")
        return {"ok": True, "selection": selection, "targeting": preview, "cancel_ok": cancel_ok, "committed_hand_instance_ids": post_hand}

    def assert_reward(self, require_replacement: bool) -> dict[str, Any]:
        snapshot = self.probe()
        verdict = evaluate_reward_offer(snapshot, require_replacement=require_replacement)
        if not verdict["ok"]:
            raise RuntimeError(f"Pending reward does not meet the three-choice/replacement contract: {verdict}")
        offer_id = verdict["offer_card_ids"][0]
        if require_replacement:
            replace_id = verdict["replacement_card_ids"][0]
            self.require_action("select-replacement", route_card_id=replace_id)
            resolved = self.require_action("choose-reward", reward_card_id=offer_id, replace_route_card_id=replace_id)
        else:
            resolved = self.require_action("choose-reward", reward_card_id=offer_id)
        post_run = _as_dict(resolved.get("card_run"))
        post_route_cards = [_name(value) for value in _as_list(post_run.get("route_card_ids")) if _name(value)]
        route_ok = len(post_route_cards) <= 12 and (not require_replacement or (len(post_route_cards) == 12 and offer_id in post_route_cards))
        if not route_ok:
            raise RuntimeError(f"Reward resolution broke the route card cap or did not add the selected reward: {post_route_cards}")
        return {"ok": True, "offer": verdict, "chosen_reward_card_id": offer_id, "route_card_ids_after": post_route_cards}

    def assert_event_support(self, event_kind: str) -> dict[str, Any]:
        before = self.probe()
        before_run = _as_dict(before.get("card_run"))
        before_event = _as_dict(before_run.get("pending_event"))
        before_runtime = _as_dict(before.get("runtime_state"))
        before_event_npc_id = _name(before_event.get("event_npc_id"))
        if event_kind == "task_npc" and (not before_event_npc_id.startswith(TASK_NPC_PREFIX) or before_event_npc_id in EVENT_NIU_HUAN_IDS):
            raise RuntimeError(f"Current route event is not a named task-NPC offer: {before_event_npc_id}")
        if event_kind == "niu_huan" and before_event_npc_id not in EVENT_NIU_HUAN_IDS:
            raise RuntimeError(f"Current route event is not the event-only NiuHuan branch: {before_event_npc_id}")
        player_gold_before_open = int(before_runtime.get("player_gold", 0) or 0)
        opened_snapshot = self.require_action("open-route-encounter-panel")
        open_verdict = evaluate_event_panel_open(
            opened_snapshot,
            event_kind=event_kind,
            event_npc_id_before_open=before_event_npc_id,
            player_gold_before_open=player_gold_before_open,
        )
        if not open_verdict["ok"]:
            raise RuntimeError(f"F-equivalent event interaction did not open the required explicit panel: {open_verdict}")

        resolved_snapshot = self.require_action("trigger-route-encounter-primary")
        resolution_verdict = evaluate_event_resolution(
            resolved_snapshot,
            event_kind=event_kind,
            event_npc_id_before_open=before_event_npc_id,
            player_gold_before_open=player_gold_before_open,
        )
        if not resolution_verdict["ok"]:
            raise RuntimeError(f"Visible route-event choice did not resolve the required support/reward branch: {resolution_verdict}")
        return {
            "ok": True,
            "opened_panel": open_verdict,
            "resolved_choice": resolution_verdict,
        }

    def assert_route_panel(self, expected_kind: str) -> dict[str, Any]:
        """Open and resolve one real camp, merchant, or chest panel.

        Camp, merchant, and chest are separate player route states.  The caller
        must navigate to each in a normal run and invoke this assertion once
        per state; the runner never swaps maps or injects a fake node.  This
        assertion deliberately consumes the primary visible choice, so its
        evidence covers the interaction gate and the post-choice cleanup.
        """
        before = self.probe()
        before_runtime = _as_dict(before.get("runtime_state"))
        pending_node_id_before_open = int(before_runtime.get("pending_route_node_id", -1) or -1)
        if pending_node_id_before_open < 0:
            raise RuntimeError(f"Route {expected_kind} has no pending node to open: {before_runtime}")
        player_gold_before_open = int(before_runtime.get("player_gold", 0) or 0)
        player_hp_before_open = int(before_runtime.get("player_hp", 0) or 0)
        opened_snapshot = self.require_action("open-route-encounter-panel")
        open_verdict = evaluate_route_panel(opened_snapshot, expected_kind=expected_kind)
        if not open_verdict["ok"]:
            raise RuntimeError(f"Route {expected_kind} panel did not open through the focused player interaction gate: {open_verdict}")
        resolved_snapshot = self.require_action("trigger-route-encounter-primary")
        resolution_verdict = evaluate_route_panel_resolution(
            resolved_snapshot,
            expected_kind=expected_kind,
            pending_node_id_before_open=pending_node_id_before_open,
            player_gold_before_open=player_gold_before_open,
            player_hp_before_open=player_hp_before_open,
        )
        if not resolution_verdict["ok"]:
            raise RuntimeError(f"Route {expected_kind} primary choice did not cleanly resolve its node: {resolution_verdict}")
        return {
            "ok": True,
            "opened_panel": open_verdict,
            "resolved_choice": resolution_verdict,
        }

    def assert_party(self) -> dict[str, Any]:
        snapshot = self.probe()
        verdict = evaluate_party_cap_and_visuals(snapshot)
        if not verdict["ok"]:
            raise RuntimeError(f"Party selection/visual cap is invalid: {verdict}")
        return verdict


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--timeout", type=float, default=30.0)
    parser.add_argument(
        "--scenario",
        choices=("audit", "targeting", "reward", "replacement", "event-npc", "event-niu-huan", "panel", "party"),
        default="audit",
    )
    parser.add_argument(
        "--expected-panel-kind",
        choices=("Camp", "Merchant", "Chest"),
        default=None,
        help="Required with --scenario panel; run once after navigating to each real route panel.",
    )
    parser.add_argument("--report", type=Path, default=None)
    parser.add_argument("--print-automation-plan", action="store_true", help="Print focused deterministic automation-test names and exit without contacting UE.")
    args = parser.parse_args(argv)

    if args.print_automation_plan:
        print(json.dumps({"version": PARTY_DECK_ACCEPTANCE_VERSION, "automation_tests": get_automation_test_plan()}, ensure_ascii=False, indent=2))
        return 0

    harness = PartyDeckAcceptanceHarness(timeout=args.timeout)
    result: dict[str, Any]
    try:
        harness.connect()
        if args.scenario == "audit":
            result = {"ok": True, "snapshot": harness.probe(), "events": harness.events}
        elif args.scenario == "targeting":
            result = {"ok": True, "targeting": harness.assert_targeting(), "events": harness.events}
        elif args.scenario in {"reward", "replacement"}:
            result = {"ok": True, "reward": harness.assert_reward(require_replacement=args.scenario == "replacement"), "events": harness.events}
        elif args.scenario in {"event-npc", "event-niu-huan"}:
            result = {"ok": True, "event": harness.assert_event_support("task_npc" if args.scenario == "event-npc" else "niu_huan"), "events": harness.events}
        elif args.scenario == "panel":
            if not args.expected_panel_kind:
                raise RuntimeError("--scenario panel requires --expected-panel-kind Camp, Merchant, or Chest")
            result = {"ok": True, "panel": harness.assert_route_panel(args.expected_panel_kind), "events": harness.events}
        elif args.scenario == "party":
            result = {"ok": True, "party": harness.assert_party(), "events": harness.events}
    except Exception as exc:
        result = {"ok": False, "error": str(exc), "events": harness.events}
        code = 1
    else:
        code = 0

    REPORT_DIR.mkdir(parents=True, exist_ok=True)
    report_path = args.report or REPORT_DIR / f"gamexxk-party-deck-acceptance-{time.strftime('%Y%m%d-%H%M%S')}.json"
    report_path.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps({"ok": bool(result.get("ok")), "report": str(report_path)}, ensure_ascii=False), flush=True)
    return code


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
