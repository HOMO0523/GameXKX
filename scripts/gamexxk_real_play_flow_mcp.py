#!/usr/bin/env python3
"""Drive the real GameXXK MVP entry flow through UE MCP and OS input."""

from __future__ import annotations

import argparse
import base64
import ctypes
from contextlib import contextmanager
import struct
import zlib
import json
import math
import re
import struct
import sys
import time
from pathlib import Path
from typing import Any

from PIL import Image

from ue_mcp_client import EDITOR_TOOLSET, UnrealMCPClient


PROJECT_ROOT = Path(__file__).resolve().parents[1]
REPORT_DIR = PROJECT_ROOT / "Saved" / "HarnessReports"
SCREENSHOT_DIR = PROJECT_ROOT / "Saved" / "Codex"
SLATE_TOOLSET = "SlateInspectorToolset.SlateInspectorToolset"
SCENE_TOOLSET = "editor_toolset.toolsets.scene.SceneTools"
PROBE_SCRIPT = "Content/Python/gamexxk_probe_real_play_flow.py"
BATTLE_HUD_FIXTURE_SCRIPT = "Content/Python/gamexxk_apply_battle_hud_fixture.py"
ACTIVE_WIDGETS_PROBE_SCRIPT = "Content/Python/gamexxk_probe_active_widgets.py"
MAIN_MAP = "/Game/GameXXK/Maps/L_Main"
QINGSHAN_MAP_TOKEN = "L_Qingshan_AsianVillage_Demo"
ROUTE_MAP_TOKEN = "L_RouteMap"
BATTLE_MAP_TOKEN = "L_BattleTown"
BATTLE_PC_TOKEN = "GameXXKMVPPlayerController"
# After D releases, PIE must process and expose the remaining W-only motion
# before W releases.  A probe after this settle is intentional: a wall-clock
# delay alone can still leave the final release on the stale diagonal facing.
DIAGONAL_D_TO_W_VERTICAL_PROBE_SETTLE_SECONDS = 0.20
SLATE_WINDOW_PATTERN = re.compile(r'window "([^"]*GameXXK Preview[^"]*)"[^\n]*\[ref=([^\]]+)\]')
SLATE_BUTTON_PATTERN = re.compile(
    r'button(?: "(?P<label>[^"]*)")?(?P<disabled> \[disabled\])? \[pos=(?P<x>-?\d+),(?P<y>-?\d+) size=(?P<w>\d+),(?P<h>\d+)\] \[ref=(?P<ref>[^\]]+)\]'
)


def _png_size(data: bytes) -> tuple[int, int]:
    if len(data) < 24 or data[:8] != b"\x89PNG\r\n\x1a\n":
        return 0, 0
    return struct.unpack(">II", data[16:24])


def _slate_preview_window_ref(snapshot: str) -> str:
    """Return the last GameXXK PIE Slate window reference, if one exists."""
    preview_ref = ""
    for match in SLATE_WINDOW_PATTERN.finditer(str(snapshot or "")):
        preview_ref = match.group(2)
    return preview_ref


def _decode_slate_screenshot_png(payload: Any) -> bytes:
    """Validate the Slate MCP image transport before recording visual evidence."""
    if not isinstance(payload, dict) or str(payload.get("mimeType", "")).lower() != "image/png":
        raise RuntimeError(f"Slate screenshot did not return a PNG payload: {type(payload).__name__}")
    encoded = payload.get("data")
    if not isinstance(encoded, str) or not encoded:
        raise RuntimeError("Slate screenshot PNG payload has no base64 data")
    try:
        image = base64.b64decode(encoded, validate=True)
    except Exception as exc:
        raise RuntimeError("Slate screenshot PNG payload is not valid base64") from exc
    if _png_size(image) == (0, 0):
        raise RuntimeError("Slate screenshot payload is not a valid PNG image")
    return image


def _rgba_to_png(width: int, height: int, rgba: bytes) -> bytes:
    def chunk(kind: bytes, payload: bytes) -> bytes:
        return (
            struct.pack(">I", len(payload))
            + kind
            + payload
            + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF)
        )

    stride = width * 4
    raw = b"".join(b"\x00" + rgba[row * stride : (row + 1) * stride] for row in range(height))
    return (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(raw, 6))
        + chunk(b"IEND", b"")
    )


def _load_json_from_probe(result: dict[str, Any]) -> dict[str, Any]:
    stdout = str(result.get("stdout", "")).strip()
    if not stdout:
        return {}
    last_line = stdout.splitlines()[-1]
    return json.loads(last_line)


def _runtime_screen(probe: dict[str, Any]) -> str:
    return str(probe.get("probe", {}).get("runtime_state", {}).get("screen", ""))


def _screen_contains(probe: dict[str, Any], token: str) -> bool:
    normalized_screen = _runtime_screen(probe).replace("_", "").upper()
    normalized_token = token.replace("_", "").upper()
    return normalized_token in normalized_screen


def _runtime_state(probe: dict[str, Any]) -> dict[str, Any]:
    state = probe.get("probe", {}).get("runtime_state", {})
    return state if isinstance(state, dict) else {}


def _save_state(probe: dict[str, Any]) -> dict[str, Any]:
    state = probe.get("probe", {}).get("save_state", {})
    return state if isinstance(state, dict) else {}


def _visible_commands(probe: dict[str, Any]) -> list[dict[str, Any]]:
    commands = probe.get("probe", {}).get("hud", {}).get("visible_commands", [])
    return commands if isinstance(commands, list) else []


def _flow_widgets(probe: dict[str, Any]) -> dict[str, Any]:
    widgets = probe.get("probe", {}).get("player_controller", {}).get("flow_widgets", {})
    return widgets if isinstance(widgets, dict) else {}


def _route_node_visual_states(probe: dict[str, Any]) -> list[dict[str, Any]]:
    route_widget = _flow_widgets(probe).get("route_map", {})
    if not isinstance(route_widget, dict):
        return []
    states = route_widget.get("route_node_visual_states", [])
    return states if isinstance(states, list) else []


def _route_node_visual_state(probe: dict[str, Any], node_id: int) -> dict[str, Any]:
    for state in _route_node_visual_states(probe):
        if int(state.get("node_id", -1)) == int(node_id):
            return state
    return {}


def _widget_visible(probe: dict[str, Any], name: str) -> bool:
    widget = _flow_widgets(probe).get(name, {})
    if not isinstance(widget, dict):
        return False
    visibility = str(widget.get("visibility", "")).upper()
    return bool(widget.get("is_in_viewport")) and ("VISIBLE" in visibility or widget.get("is_town_overlay_visible") or widget.get("is_battle_board_visible"))


def _has_visible_command(probe: dict[str, Any], command_name: str, enabled: bool | None = None) -> bool:
    for command in _visible_commands(probe):
        if str(command.get("command_name", "")) != command_name:
            continue
        if enabled is None:
            return True
        return bool(command.get("b_enabled")) is enabled
    return False


def _map_name(probe: dict[str, Any]) -> str:
    payload = probe.get("probe") if isinstance(probe, dict) else None
    return str(payload.get("map_name", "")) if isinstance(payload, dict) else ""


def _pawn_location(probe: dict[str, Any]) -> dict[str, float]:
    location = probe.get("probe", {}).get("pawn", {}).get("location", {})
    return location if isinstance(location, dict) else {}


def _pawn_state(probe: dict[str, Any]) -> dict[str, Any]:
    pawn = probe.get("probe", {}).get("pawn", {})
    return pawn if isinstance(pawn, dict) else {}


def _current_flipbook(probe: dict[str, Any]) -> str:
    return str(_pawn_state(probe).get("current_flipbook", ""))


def _is_town_moving(probe: dict[str, Any]) -> bool:
    return bool(_pawn_state(probe).get("is_town_moving"))


def _move_input_is_ignored(probe: dict[str, Any]) -> bool:
    return bool(_pawn_state(probe).get("move_input_ignored"))


def _actors(probe: dict[str, Any]) -> list[dict[str, Any]]:
    payload = probe.get("probe") if isinstance(probe, dict) else None
    actors = payload.get("actors", []) if isinstance(payload, dict) else []
    return actors if isinstance(actors, list) else []


def _quest_npc(probe: dict[str, Any]) -> dict[str, Any]:
    for actor in _actors(probe):
        if str(actor.get("get_npc_role", "")).upper().endswith("QUEST"):
            return actor
    return {}


def _town_exit(probe: dict[str, Any]) -> dict[str, Any]:
    for actor in _actors(probe):
        label = str(actor.get("label", ""))
        class_name = str(actor.get("class", ""))
        if label == "QingshanInn_TownExit" or "TownExit" in class_name:
            return actor
    return {}


def _npc_by_role(probe: dict[str, Any], role: str) -> dict[str, Any]:
    role_token = role.upper()
    for actor in _actors(probe):
        if str(actor.get("get_npc_role", "")).upper().endswith(role_token):
            return actor
    return {}


def _battle_scene_counts(probe: dict[str, Any]) -> dict[str, int]:
    counts = {"presenters": 0, "units": 0, "enemies": 0, "party": 0, "visual_units": 0}
    for actor in _actors(probe):
        class_name = str(actor.get("class", ""))
        if "BattleScenePresenter" in class_name:
            counts["presenters"] += 1
        if "BattleSceneUnitActor" not in class_name:
            continue
        counts["units"] += 1
        visual = actor.get("battle_visual", {})
        if isinstance(visual, dict) and str(visual.get("flipbook", "")):
            counts["visual_units"] += 1
        if bool(actor.get("is_enemy_unit")):
            counts["enemies"] += 1
        else:
            counts["party"] += 1
    return counts


def _battle_hud_units(probe: dict[str, Any]) -> list[dict[str, Any]]:
    """Join scene identity to Board-owned HUD evidence by stable UnitId."""
    payload = probe.get("probe", {}) if isinstance(probe, dict) else {}
    board = payload.get("battle_board") if isinstance(payload, dict) else {}
    unit_huds = board.get("unit_huds") if isinstance(board, dict) else {}
    unit_huds = unit_huds if isinstance(unit_huds, dict) else {}
    units: list[dict[str, Any]] = []
    for actor in _actors(probe):
        if not isinstance(actor, dict) or "BattleSceneUnitActor" not in str(actor.get("class", "")):
            continue
        unit_id = str(actor.get("unit_id", "") or "")
        units.append(
            {
                "unit_id": unit_id,
                "unit_hud": unit_huds.get(unit_id),
            }
        )
    return units


def _finite_float(value: Any) -> float | None:
    try:
        number = float(value)
    except (TypeError, ValueError, OverflowError):
        return None
    return number if math.isfinite(number) else None


_FIXTURE_UNIT_SIDES = {"Player": "Party", "Enemy.Outer": "Enemy"}


def _unit_hud_side(unit_hud: Any) -> str:
    side = unit_hud.get("side") if isinstance(unit_hud, dict) else ""
    return str(side).strip()


def _viewport_size(viewport: Any) -> tuple[float, float] | None:
    if isinstance(viewport, dict):
        width = viewport.get("width")
        height = viewport.get("height")
    elif isinstance(viewport, (list, tuple)) and len(viewport) >= 2:
        width, height = viewport[0], viewport[1]
    else:
        return None
    width = _finite_float(width)
    height = _finite_float(height)
    return (width, height) if width is not None and height is not None and width > 0.0 and height > 0.0 else None


def _rect_values(rect: Any) -> tuple[float, float, float, float] | None:
    if not isinstance(rect, dict):
        return None
    try:
        left, top, right, bottom = (_finite_float(rect[key]) for key in ("left", "top", "right", "bottom"))
    except KeyError:
        return None
    if None in (left, top, right, bottom):
        return None
    return left, top, right, bottom


def _screen_rect_in_viewport(
    rect: Any,
    viewport_size: tuple[float, float],
    field_name: str = "resource_hud",
) -> tuple[bool, str]:
    values = _rect_values(rect)
    if values is None:
        return False, f"{field_name}_screen_rect_missing"
    left, top, right, bottom = values
    if right <= left or bottom <= top:
        return False, f"{field_name}_screen_rect_invalid"
    width, height = viewport_size
    if right <= 0.0 or bottom <= 0.0 or left >= width or top >= height:
        return False, f"{field_name}_offscreen"
    containment_tolerance = 1.0
    if (
        left < -containment_tolerance
        or top < -containment_tolerance
        or right > width + containment_tolerance
        or bottom > height + containment_tolerance
    ):
        return False, f"{field_name}_partially_offscreen"
    return True, ""


def _ue58_python_geometry_api_unavailable(summary: Any) -> bool:
    """True only for the known UE 5.8 Python binding gap, never for an absent widget."""
    if not isinstance(summary, dict):
        return False
    geometry = summary.get("geometry")
    if not isinstance(geometry, dict) or geometry.get("cached") is not True:
        return False
    for error in geometry.get("errors", []):
        if not isinstance(error, dict):
            continue
        if str(error.get("stage", "")) != "geometry.get_local_size":
            continue
        if "has no attribute 'get_local_size'" in str(error.get("exception", "")):
            return True
    return False


def _screen_rect_is_aggregated_geometry_diagnostic(summary: Any, expected_stage: str = "") -> bool:
    """The project probe intentionally collapses child-widget Geometry details into this stable diagnostic."""
    if not isinstance(summary, dict):
        return False
    for diagnostic in summary.get("diagnostics", []):
        if not isinstance(diagnostic, dict) or str(diagnostic.get("code", "")) != "screen_rect_unavailable":
            continue
        if not expected_stage or str(diagnostic.get("stage", "")) == expected_stage:
            return True
    return False


def _uses_ue58_fixed_slot_geometry_fallback(units: list[dict[str, Any]], board: Any) -> bool:
    """Require every relevant live widget to report the same unavailable Geometry API."""
    if not units or not isinstance(board, dict):
        return False
    board_summaries: list[Any] = [board.get(key) for key in ("unit_hud_layer", "party_qi", "hand_card_box", "end_turn_button")]
    if not all(_ue58_python_geometry_api_unavailable(summary) for summary in board_summaries):
        return False
    for unit in units:
        unit_hud = unit.get("unit_hud") if isinstance(unit, dict) else None
        if not isinstance(unit_hud, dict):
            return False
        resource = unit_hud.get("resource")
        status = unit_hud.get("status")
        if not (
            _ue58_python_geometry_api_unavailable(unit_hud)
            or _screen_rect_is_aggregated_geometry_diagnostic(unit_hud, "battle_unit_hud.screen_rect")
        ):
            return False
        if not (
            _ue58_python_geometry_api_unavailable(resource)
            or _screen_rect_is_aggregated_geometry_diagnostic(resource, "battle_unit_hud.resource.screen_rect")
            or _screen_rect_is_aggregated_geometry_diagnostic(unit_hud, "battle_unit_hud.resource.screen_rect")
        ):
            return False
        if not (
            _ue58_python_geometry_api_unavailable(status)
            or _screen_rect_is_aggregated_geometry_diagnostic(status, "battle_unit_hud.status.screen_rect")
            or _screen_rect_is_aggregated_geometry_diagnostic(unit_hud, "battle_unit_hud.status.screen_rect")
        ):
            return False
    return True


def _fixed_slot_hud_errors(unit_hud: dict[str, Any]) -> list[str]:
    """Validate the Board's runtime fixed-slot seam when Python cannot read Slate rectangles."""
    errors: list[str] = []
    anchor = unit_hud.get("projected_anchor")
    anchor_x = _finite_float(anchor.get("x")) if isinstance(anchor, dict) else None
    anchor_y = _finite_float(anchor.get("y")) if isinstance(anchor, dict) else None
    if anchor_x is None or anchor_y is None or not (0.0 <= anchor_x <= 1.0 and 0.0 <= anchor_y <= 1.0):
        errors.append("unit_hud_fixed_slot_anchor_invalid")
    projection = unit_hud.get("projection") if isinstance(unit_hud.get("projection"), dict) else {}
    applied_slot = projection.get("applied_slot") if isinstance(projection.get("applied_slot"), dict) else {}
    size = applied_slot.get("size") if isinstance(applied_slot.get("size"), dict) else {}
    width = _finite_float(size.get("x"))
    height = _finite_float(size.get("y"))
    if width is None or height is None or width < 24.0 or height < 24.0:
        errors.append("unit_hud_fixed_slot_size_invalid")
    if str(applied_slot.get("visibility", "")).upper() != "SELF_HIT_TEST_INVISIBLE":
        errors.append("unit_hud_fixed_slot_visibility_invalid")
    return errors


def _strict_rectangles_overlap(first: Any, second: Any) -> bool:
    first_values = _rect_values(first)
    second_values = _rect_values(second)
    if first_values is None or second_values is None:
        return False
    first_left, first_top, first_right, first_bottom = first_values
    second_left, second_top, second_right, second_bottom = second_values
    return (
        first_left < second_right
        and first_right > second_left
        and first_top < second_bottom
        and first_bottom > second_top
    )


def _battle_board_errors(
    probe: dict[str, Any],
    viewport_size: tuple[float, float],
    *,
    use_fixed_slot_geometry_fallback: bool = False,
) -> list[str]:
    payload = probe.get("probe", {}) if isinstance(probe, dict) else {}
    board = payload.get("battle_board") if isinstance(payload, dict) else None
    if not isinstance(board, dict):
        return ["battle_board_missing"]

    errors: list[str] = []
    unit_hud_layer = board.get("unit_hud_layer")
    if not isinstance(unit_hud_layer, dict):
        errors.append("unit_hud_layer_missing")
    else:
        if unit_hud_layer.get("visible") is not True:
            errors.append("unit_hud_layer_not_visible")
        if not use_fixed_slot_geometry_fallback:
            layer_ok, layer_error = _screen_rect_in_viewport(
                unit_hud_layer.get("screen_rect"), viewport_size, "unit_hud_layer"
            )
            if not layer_ok:
                errors.append(layer_error)
    if not isinstance(board.get("unit_huds"), dict):
        errors.append("unit_huds_missing")
    raw_shared_energy = _finite_float(board.get("shared_energy"))
    shared_energy = int(raw_shared_energy) if raw_shared_energy is not None else None
    if shared_energy is None:
        raw_value = board.get("shared_energy")
        errors.append("shared_energy_non_finite" if isinstance(raw_value, float) and not math.isfinite(raw_value) else "shared_energy_missing")

    party_qi = board.get("party_qi")
    if not isinstance(party_qi, dict):
        return errors + ["party_qi_missing"]
    if party_qi.get("visible") is not True:
        errors.append("party_qi_not_visible")
    raw_party_qi_value = _finite_float(party_qi.get("value"))
    party_qi_value = int(raw_party_qi_value) if raw_party_qi_value is not None else None
    if party_qi_value is None:
        raw_value = party_qi.get("value")
        errors.append("party_qi_value_non_finite" if isinstance(raw_value, float) and not math.isfinite(raw_value) else "party_qi_value_missing")
    if shared_energy is not None and party_qi_value is not None and party_qi_value != shared_energy:
        errors.append("party_qi_value_mismatch")
    party_qi_rect = party_qi.get("screen_rect")
    party_qi_rect_ok = False
    if not use_fixed_slot_geometry_fallback:
        party_qi_rect_ok, party_qi_rect_error = _screen_rect_in_viewport(party_qi_rect, viewport_size, "party_qi")
        if not party_qi_rect_ok:
            errors.append(party_qi_rect_error)

    controls: dict[str, Any] = {}
    for control_key in ("hand_card_box", "end_turn_button"):
        control = board.get(control_key)
        if not isinstance(control, dict):
            errors.append(f"{control_key}_missing")
            continue
        controls[control_key] = control
        if control.get("visible") is not True:
            errors.append(f"{control_key}_not_visible")
        if not use_fixed_slot_geometry_fallback:
            rect_ok, rect_error = _screen_rect_in_viewport(control.get("screen_rect"), viewport_size, control_key)
            if not rect_ok:
                errors.append(rect_error)

    if party_qi_rect_ok and not use_fixed_slot_geometry_fallback:
        for control_key, control in controls.items():
            if _strict_rectangles_overlap(party_qi_rect, control.get("screen_rect")):
                errors.append(f"party_qi_overlaps_{control_key}")
    return errors


def _fixture_vital_values(resource: dict[str, Any], vital_name: str) -> tuple[int, int] | None:
    rendered = resource.get("rendered") if isinstance(resource, dict) else {}
    value = rendered.get(f"{vital_name}_text") if isinstance(rendered, dict) else None
    match = re.search(r"(\d+)\s*/\s*(\d+)", str(value or ""))
    return (int(match.group(1)), int(match.group(2))) if match else None


def _rendered_text_matches_vitals(value: Any, current: int, maximum: int) -> bool:
    normalized = re.sub(r"\s+", "", str(value or ""))
    return bool(re.search(rf"(?<!\d){current}/{maximum}(?!\d)", normalized))


def _rendered_percent_matches_vitals(value: Any, current: int, maximum: int) -> bool:
    if maximum <= 0:
        return False
    try:
        rendered_percent = float(value)
    except (TypeError, ValueError):
        return False
    return math.isfinite(rendered_percent) and math.isclose(
        rendered_percent,
        float(current) / float(maximum),
        rel_tol=0.0,
        abs_tol=0.005,
    )


def _rendered_badge_present(status_hud: Any, icon_id: str, displayed_stack: str) -> bool:
    if not isinstance(status_hud, dict):
        return False
    rendered = status_hud.get("rendered")
    if not isinstance(rendered, dict):
        return False
    badges = rendered.get("badges")
    if not isinstance(badges, list):
        return False
    expected_icon = icon_id.casefold()
    expected_stack = str(displayed_stack)
    for badge in badges:
        if not isinstance(badge, dict):
            continue
        if str(badge.get("icon_id", "")).strip().casefold() != expected_icon:
            continue
        if str(badge.get("displayed_stack", "")).strip() == expected_stack:
            return True
    return False


def _fixture_party_rendered_errors(unit: dict[str, Any]) -> list[str]:
    unit_hud = unit.get("unit_hud") if isinstance(unit.get("unit_hud"), dict) else {}
    resource_hud = unit_hud.get("resource") if isinstance(unit_hud.get("resource"), dict) else {}
    errors: list[str] = []
    health = _fixture_vital_values(resource_hud, "health")
    mana = _fixture_vital_values(resource_hud, "mana")
    expected_health = (72, 100)
    expected_mana = (18, 30)
    if health is None:
        errors.append("fixture_party_hp_missing")
    elif health[0] < 0 or health[1] <= 0:
        errors.append("fixture_party_hp_invalid")
    elif health[0] >= health[1]:
        errors.append("fixture_party_hp_not_reduced")
    if mana is None:
        errors.append("fixture_party_mana_missing")
    elif mana[0] < 0 or mana[1] <= 0:
        errors.append("fixture_party_mana_invalid")
    elif mana[0] >= mana[1]:
        errors.append("fixture_party_mana_not_reduced")

    rendered = resource_hud.get("rendered") if isinstance(resource_hud.get("rendered"), dict) else {}
    if not _rendered_text_matches_vitals(rendered.get("health_text"), *expected_health):
        errors.append("fixture_party_health_text_mismatch")
    if not _rendered_text_matches_vitals(rendered.get("mana_text"), *expected_mana):
        errors.append("fixture_party_mana_text_mismatch")
    if not _rendered_percent_matches_vitals(rendered.get("health_percent"), *expected_health):
        errors.append("fixture_party_health_percent_mismatch")
    if not _rendered_percent_matches_vitals(rendered.get("mana_percent"), *expected_mana):
        errors.append("fixture_party_mana_percent_mismatch")

    status_hud = unit_hud.get("status")
    if not _rendered_badge_present(status_hud, "ArmorShield", "7"):
        errors.append("fixture_party_armor_badge_missing")
    return errors


def _fixture_enemy_rendered_errors(unit: dict[str, Any]) -> list[str]:
    unit_hud = unit.get("unit_hud") if isinstance(unit.get("unit_hud"), dict) else {}
    status_hud = unit_hud.get("status")
    errors: list[str] = []
    if not _rendered_badge_present(status_hud, "PoisonVial", "2"):
        errors.append("fixture_enemy_poison_badge_missing")
    if not _rendered_badge_present(status_hud, "BleedDrop", "3"):
        errors.append("fixture_enemy_bleed_badge_missing")
    return errors


def _battle_hud_verdict(probe: dict[str, Any], viewport: Any) -> dict[str, Any]:
    """Validate observed actor and Board HUDs only; this function never mutates the probe or PIE."""
    if not isinstance(probe, dict) or not isinstance(probe.get("probe"), dict):
        return {
            "ok": False,
            "reason": "probe_invalid",
            "unit_count": 0,
            "errors": {"__probe__": ["probe_invalid"]},
        }
    viewport_size = _viewport_size(viewport)
    if viewport_size is None:
        return {
            "ok": False,
            "reason": "viewport_invalid",
            "unit_count": 0,
            "errors": {"__viewport__": ["viewport_invalid"]},
        }

    units = _battle_hud_units(probe)
    if not units:
        return {
            "ok": False,
            "reason": "battle_units_missing",
            "unit_count": 0,
            "errors": {},
        }

    payload = probe.get("probe", {}) if isinstance(probe, dict) else {}
    board = payload.get("battle_board") if isinstance(payload, dict) else {}
    use_fixed_slot_geometry_fallback = _uses_ue58_fixed_slot_geometry_fallback(units, board)
    errors: dict[str, list[str]] = {}
    for index, unit in enumerate(units):
        unit_id = str(unit.get("unit_id", "") or "")
        error_id = unit_id or f"__missing_unit_id_{index}"
        unit_errors: list[str] = []
        unit_hud = unit.get("unit_hud")
        if not isinstance(unit_hud, dict):
            unit_errors.append("unit_hud_missing")
            unit_hud = {}
        if unit_hud.get("visible") is not True:
            unit_errors.append("unit_hud_not_visible")
        side = _unit_hud_side(unit_hud)
        if side not in {"Party", "Enemy"}:
            unit_errors.append("unit_hud_side_invalid")
        expected_side = _FIXTURE_UNIT_SIDES.get(unit_id)
        if expected_side is not None and side != expected_side:
            unit_errors.append("unit_hud_side_mismatch")
        if str(unit_hud.get("unit_id", "") or "") != unit_id:
            unit_errors.append("unit_hud_identity_mismatch")
        unit_rect_ok = False
        if not use_fixed_slot_geometry_fallback:
            unit_rect_ok, unit_rect_error = _screen_rect_in_viewport(
                unit_hud.get("screen_rect"), viewport_size, "unit_hud"
            )
            if not unit_rect_ok:
                unit_errors.append(unit_rect_error)
        else:
            unit_errors.extend(_fixed_slot_hud_errors(unit_hud))
        anchor = unit_hud.get("projected_anchor")
        unit_rect = _rect_values(unit_hud.get("screen_rect"))
        try:
            anchor_x = _finite_float(anchor["x"])
        except (KeyError, TypeError):
            anchor_x = None
            unit_errors.append("unit_hud_anchor_missing")
        if anchor_x is None:
            unit_errors.append("unit_hud_anchor_non_finite")
        if unit_rect is not None and anchor_x is not None:
            if abs(((unit_rect[0] + unit_rect[2]) / 2.0) - anchor_x) > 2.0:
                unit_errors.append("unit_hud_anchor_mismatch")

        resource_hud = unit_hud.get("resource")
        if not isinstance(resource_hud, dict):
            unit_errors.append("unit_hud_resource_missing")
            resource_hud = {}
        if resource_hud.get("visible") is not True:
            unit_errors.append("unit_hud_resource_not_visible")
        if not use_fixed_slot_geometry_fallback:
            rect_ok, rect_error = _screen_rect_in_viewport(
                resource_hud.get("screen_rect"), viewport_size, "unit_hud_resource"
            )
            if not rect_ok:
                unit_errors.append(rect_error)

        mana_row_visible = resource_hud.get("mana_row_visible")
        if side == "Enemy" and mana_row_visible is True:
            unit_errors.append("unit_hud_enemy_mana_row_visible")
        if side == "Party" and mana_row_visible is not True:
            unit_errors.append("unit_hud_party_mana_row_missing")

        status_hud = unit_hud.get("status")
        if not isinstance(status_hud, dict):
            unit_errors.append("unit_hud_status_missing")
            status_hud = {}
        status_rendered = status_hud.get("rendered") if isinstance(status_hud.get("rendered"), dict) else {}
        status_badges = status_rendered.get("badges") if isinstance(status_rendered.get("badges"), list) else []
        raw_icon_count = _finite_float(status_rendered.get("icon_count", 0))
        status_icon_count = int(raw_icon_count) if raw_icon_count is not None else -1
        if status_icon_count < 0:
            unit_errors.append("unit_hud_status_icon_count_invalid")
        if status_hud.get("visible") is not True:
            if not (
                status_hud.get("visible") is False
                and status_hud.get("screen_rect") is None
                and status_icon_count == 0
                and not status_badges
            ):
                unit_errors.append("unit_hud_status_not_visible")
        else:
            if not use_fixed_slot_geometry_fallback:
                status_rect = status_hud.get("screen_rect")
                status_rect_ok, status_rect_error = _screen_rect_in_viewport(
                    status_rect, viewport_size, "unit_hud_status"
                )
                if not status_rect_ok:
                    unit_errors.append(status_rect_error)
                else:
                    status_values = _rect_values(status_rect)
                    if status_values and status_values[2] - status_values[0] < 24.0:
                        unit_errors.append("unit_hud_status_width_too_small")

        if unit_errors:
            errors[error_id] = unit_errors

    unit_huds = board.get("unit_huds") if isinstance(board, dict) else {}
    unit_huds = unit_huds if isinstance(unit_huds, dict) else {}
    scene_ids = [str(unit.get("unit_id", "") or "") for unit in units if str(unit.get("unit_id", "") or "")]
    scene_id_set = set(scene_ids)
    if len(scene_id_set) != len(scene_ids):
        errors.setdefault("__board__", []).append("unit_hud_duplicate_scene_id")
    if len(unit_huds) != len(scene_ids):
        errors.setdefault("__board__", []).append("unit_hud_count_mismatch")
    for unit_id in unit_huds:
        if str(unit_id) not in scene_id_set:
            errors.setdefault("__board__", []).append("unit_hud_orphan")
    controls = [
        ("hand_card_box", board.get("hand_card_box") if isinstance(board, dict) else None),
        ("party_qi", board.get("party_qi") if isinstance(board, dict) else None),
        ("end_turn_button", board.get("end_turn_button") if isinstance(board, dict) else None),
    ]
    if not use_fixed_slot_geometry_fallback:
        for unit in units:
            unit_id = str(unit.get("unit_id", "") or "")
            unit_hud = unit.get("unit_hud") if isinstance(unit.get("unit_hud"), dict) else {}
            for control_name, control in controls:
                if isinstance(control, dict) and _strict_rectangles_overlap(unit_hud.get("screen_rect"), control.get("screen_rect")):
                    errors.setdefault(unit_id or "__missing_unit_id__", []).append(f"unit_hud_overlaps_{control_name}")

        visible_huds = [
            (str(unit.get("unit_id", "") or ""), unit.get("unit_hud"))
            for unit in units
            if isinstance(unit.get("unit_hud"), dict) and unit["unit_hud"].get("visible") is True
        ]
        for first_index, (first_id, first_hud) in enumerate(visible_huds):
            for second_id, second_hud in visible_huds[first_index + 1 :]:
                if _strict_rectangles_overlap(first_hud.get("screen_rect"), second_hud.get("screen_rect")):
                    errors.setdefault(first_id or "__missing_unit_id__", []).append("unit_hud_overlaps_unit_hud")
                    errors.setdefault(second_id or "__missing_unit_id__", []).append("unit_hud_overlaps_unit_hud")

    party_units = [unit for unit in units if _unit_hud_side(unit.get("unit_hud")) == "Party"]
    enemy_units = [unit for unit in units if _unit_hud_side(unit.get("unit_hud")) == "Enemy"]
    if not party_units:
        errors.setdefault("__fixture__", []).append("fixture_party_missing")
    else:
        primary_party = next(
            (
                unit
                for unit in party_units
                if str(unit.get("unit_id", "")).strip().casefold() in {"player", "hero"}
            ),
            party_units[0],
        )
        primary_party_id = str(primary_party.get("unit_id", "") or "__fixture_party__")
        party_errors = _fixture_party_rendered_errors(primary_party)
        if party_errors:
            errors.setdefault(primary_party_id, []).extend(party_errors)
    if not enemy_units:
        errors.setdefault("__fixture__", []).append("fixture_enemy_missing")
    else:
        primary_enemy = enemy_units[0]
        primary_enemy_id = str(primary_enemy.get("unit_id", "") or "__fixture_enemy__")
        enemy_errors = _fixture_enemy_rendered_errors(primary_enemy)
        if enemy_errors:
            errors.setdefault(primary_enemy_id, []).extend(enemy_errors)

    board_errors = _battle_board_errors(
        probe,
        viewport_size,
        use_fixed_slot_geometry_fallback=use_fixed_slot_geometry_fallback,
    )
    if board_errors:
        errors.setdefault("__board__", []).extend(board_errors)

    return {
        "ok": not errors,
        "reason": "" if not errors else "battle_hud_invalid",
        "unit_count": len(units),
        "errors": errors,
        "geometry_validation_mode": "fixed_slot_fallback" if use_fixed_slot_geometry_fallback else "screen_rect",
    }


def _battle_hud_observation(probe: dict[str, Any]) -> dict[str, Any]:
    """Persist a non-mutating verdict from the same probe that supplied the PIE viewport."""
    payload = probe.get("probe", {}) if isinstance(probe, dict) else {}
    observed_viewport = payload.get("pie_viewport") if isinstance(payload, dict) else None
    viewport_source = ""
    viewport: dict[str, float] | None = None
    if isinstance(observed_viewport, dict):
        viewport_source = str(observed_viewport.get("source", "") or "")
        viewport_size = _viewport_size(observed_viewport)
        if viewport_source == "player_controller.get_viewport_size" and viewport_size is not None:
            viewport = {
                "width": viewport_size[0],
                "height": viewport_size[1],
            }

    verdict = _battle_hud_verdict(probe, viewport)
    board = payload.get("battle_board") if isinstance(payload, dict) else {}
    unit_huds = board.get("unit_huds") if isinstance(board, dict) else {}

    return {
        "ok": bool(verdict.get("ok")),
        "map_name": _map_name(probe),
        "viewport": viewport,
        "viewport_source": viewport_source or None,
        "verification_status": "evaluated" if viewport is not None else "viewport_unavailable",
        "unit_huds": unit_huds if isinstance(unit_huds, dict) else {},
        "battle_board": board if isinstance(board, dict) else None,
        "battle_hud_verdict": verdict,
    }


def _rect_dict(left: float, top: float, right: float, bottom: float) -> dict[str, float]:
    return {"left": left, "top": top, "right": right, "bottom": bottom}


def _same_rect(first: Any, second: Any) -> bool:
    first_values = _rect_values(first)
    second_values = _rect_values(second)
    if first_values is None or second_values is None:
        return False
    return all(math.isclose(left, right, abs_tol=0.01) for left, right in zip(first_values, second_values))


def _window_geometry_is_stable(before: Any, after: Any) -> bool:
    return bool(
        isinstance(before, dict)
        and isinstance(after, dict)
        and before.get("hwnd") == after.get("hwnd")
        and _same_rect(before.get("window_screen_rect"), after.get("window_screen_rect"))
        and _same_rect(before.get("client_screen_rect"), after.get("client_screen_rect"))
    )


def _window_client_geometry_for_screenshot(screenshot_geometry: Any) -> dict[str, Any]:
    if not isinstance(screenshot_geometry, dict):
        raise RuntimeError("screenshot_window_geometry_missing")
    if "window_geometry_before" in screenshot_geometry or "window_geometry_after" in screenshot_geometry:
        before = screenshot_geometry.get("window_geometry_before")
        after = screenshot_geometry.get("window_geometry_after")
        if not isinstance(before, dict) or not isinstance(after, dict):
            raise RuntimeError("screenshot_window_geometry_missing")
        if screenshot_geometry.get("geometry_stable") is not True:
            raise RuntimeError("screenshot_window_geometry_unstable")
        if (
            before.get("hwnd") != after.get("hwnd")
            or not _same_rect(before.get("window_screen_rect"), after.get("window_screen_rect"))
            or not _same_rect(before.get("client_screen_rect"), after.get("client_screen_rect"))
        ):
            raise RuntimeError("screenshot_window_geometry_unstable")
        return before
    return screenshot_geometry


def _viewport_to_screenshot_transform(
    viewport: Any,
    image_size: tuple[int, int],
    screenshot_geometry: Any = None,
) -> dict[str, Any]:
    """Map PIE viewport pixels into a same-window screenshot using captured Win32 client bounds."""
    viewport_size = _viewport_size(viewport)
    if viewport_size is None:
        raise RuntimeError("screenshot_viewport_invalid")
    try:
        image_width = float(image_size[0])
        image_height = float(image_size[1])
    except (IndexError, TypeError, ValueError):
        image_width = 0.0
        image_height = 0.0
    if image_width <= 0.0 or image_height <= 0.0:
        raise RuntimeError("screenshot_image_size_invalid")

    viewport_width, viewport_height = viewport_size
    if math.isclose(viewport_width, image_width, abs_tol=0.01) and math.isclose(viewport_height, image_height, abs_tol=0.01):
        return {
            "source": "identity_image_equals_viewport",
            "scale_x": 1.0,
            "scale_y": 1.0,
            "viewport_width": viewport_width,
            "viewport_height": viewport_height,
            "image_width": image_width,
            "image_height": image_height,
            "client_image_rect": _rect_dict(0.0, 0.0, image_width, image_height),
        }

    geometry = _window_client_geometry_for_screenshot(screenshot_geometry)
    window_rect = _rect_values(geometry.get("window_screen_rect"))
    client_rect = _rect_values(geometry.get("client_screen_rect"))
    if window_rect is None or client_rect is None:
        raise RuntimeError("screenshot_window_geometry_missing")
    window_left, window_top, window_right, window_bottom = window_rect
    client_left, client_top, client_right, client_bottom = client_rect
    window_width = window_right - window_left
    window_height = window_bottom - window_top
    client_width = client_right - client_left
    client_height = client_bottom - client_top
    if window_width <= 0.0 or window_height <= 0.0 or client_width <= 0.0 or client_height <= 0.0:
        raise RuntimeError("screenshot_window_geometry_invalid")
    if (
        client_left < window_left
        or client_top < window_top
        or client_right > window_right
        or client_bottom > window_bottom
    ):
        raise RuntimeError("screenshot_window_client_geometry_invalid")

    window_to_image_scale_x = image_width / window_width
    window_to_image_scale_y = image_height / window_height
    client_image_left = (client_left - window_left) * window_to_image_scale_x
    client_image_top = (client_top - window_top) * window_to_image_scale_y
    client_image_right = (client_right - window_left) * window_to_image_scale_x
    client_image_bottom = (client_bottom - window_top) * window_to_image_scale_y
    client_image_width = client_image_right - client_image_left
    client_image_height = client_image_bottom - client_image_top
    if (
        client_image_width <= 0.0
        or client_image_height <= 0.0
        or client_image_left < 0.0
        or client_image_top < 0.0
        or client_image_right > image_width
        or client_image_bottom > image_height
    ):
        raise RuntimeError("screenshot_client_image_rect_invalid")

    return {
        "source": "win32_preview_window_client",
        "scale_x": client_image_width / viewport_width,
        "scale_y": client_image_height / viewport_height,
        "viewport_width": viewport_width,
        "viewport_height": viewport_height,
        "image_width": image_width,
        "image_height": image_height,
        "window_screen_rect": _rect_dict(*window_rect),
        "client_screen_rect": _rect_dict(*client_rect),
        "client_image_rect": _rect_dict(
            client_image_left,
            client_image_top,
            client_image_right,
            client_image_bottom,
        ),
        "window_to_image_scale_x": window_to_image_scale_x,
        "window_to_image_scale_y": window_to_image_scale_y,
    }


def _map_viewport_rect_to_screenshot_rect(rect: Any, transform: dict[str, Any]) -> dict[str, float]:
    values = _rect_values(rect)
    client_image_rect = _rect_values(transform.get("client_image_rect"))
    if values is None or client_image_rect is None:
        raise RuntimeError("screenshot_crop_mapping_invalid")
    try:
        scale_x = float(transform["scale_x"])
        scale_y = float(transform["scale_y"])
    except (KeyError, TypeError, ValueError):
        raise RuntimeError("screenshot_crop_mapping_invalid")
    if scale_x <= 0.0 or scale_y <= 0.0:
        raise RuntimeError("screenshot_crop_mapping_invalid")
    left, top, right, bottom = values
    client_left, client_top, _, _ = client_image_rect
    return _rect_dict(
        client_left + left * scale_x,
        client_top + top * scale_y,
        client_left + right * scale_x,
        client_top + bottom * scale_y,
    )


def _crop_screenshot_to_viewport_rect(
    screenshot_path: Path,
    image_size: tuple[int, int],
    viewport: Any,
    rect: Any,
    output_path: Path,
    screenshot_geometry: Any = None,
) -> Path:
    transform = _viewport_to_screenshot_transform(viewport, image_size, screenshot_geometry)
    values = _rect_values(rect)
    if values is None:
        raise RuntimeError("screenshot_crop_rect_invalid")
    left, top, right, bottom = values
    if right <= left or bottom <= top:
        raise RuntimeError("screenshot_crop_rect_invalid")

    viewport_width = transform["viewport_width"]
    viewport_height = transform["viewport_height"]
    if right <= 0.0 or bottom <= 0.0 or left >= viewport_width or top >= viewport_height:
        raise RuntimeError("screenshot_crop_rect_outside_viewport")
    if left < 0.0 or top < 0.0 or right > viewport_width or bottom > viewport_height:
        raise RuntimeError("screenshot_crop_rect_partially_offscreen")

    mapped = _map_viewport_rect_to_screenshot_rect(rect, transform)
    mapped_values = _rect_values(mapped)
    client_values = _rect_values(transform["client_image_rect"])
    if mapped_values is None or client_values is None:
        raise RuntimeError("screenshot_crop_mapping_invalid")
    mapped_left, mapped_top, mapped_right, mapped_bottom = mapped_values
    client_left, client_top, client_right, client_bottom = client_values
    if (
        mapped_left < client_left - 0.01
        or mapped_top < client_top - 0.01
        or mapped_right > client_right + 0.01
        or mapped_bottom > client_bottom + 0.01
    ):
        raise RuntimeError("screenshot_crop_mapping_outside_client")
    try:
        crop_box = (
            int(math.ceil(mapped_left)),
            int(math.ceil(mapped_top)),
            int(math.floor(mapped_right)),
            int(math.floor(mapped_bottom)),
        )
    except (ValueError, OverflowError):
        raise RuntimeError("screenshot_crop_rect_invalid")
    if crop_box[2] <= crop_box[0] or crop_box[3] <= crop_box[1]:
        raise RuntimeError("screenshot_crop_rect_too_small")
    with Image.open(screenshot_path) as screenshot:
        if screenshot.size != (int(image_size[0]), int(image_size[1])):
            raise RuntimeError(
                "screenshot_viewport_size_mismatch: "
                f"captured={screenshot.size} expected={image_size}"
            )
        output_path.parent.mkdir(parents=True, exist_ok=True)
        screenshot.crop(crop_box).save(output_path)
    return output_path


def _union_screen_rects(rects: list[Any]) -> dict[str, float]:
    values = [value for value in (_rect_values(rect) for rect in rects) if value is not None]
    if not values:
        raise RuntimeError("screenshot_crop_rect_missing")
    return {
        "left": min(value[0] for value in values),
        "top": min(value[1] for value in values),
        "right": max(value[2] for value in values),
        "bottom": max(value[3] for value in values),
    }


def _unit_hud_crop_rect(probe: dict[str, Any], side: str) -> dict[str, float]:
    candidates = []
    for unit in _battle_hud_units(probe):
        unit_hud = unit.get("unit_hud") if isinstance(unit.get("unit_hud"), dict) else {}
        if _unit_hud_side(unit_hud) != side:
            continue
        rect = unit_hud.get("screen_rect")
        if _rect_values(rect) is not None:
            candidates.append((str(unit.get("unit_id", "") or ""), rect))
    preferred_ids = ("player", "hero") if side == "Party" else ("enemy",)
    for preferred_id in preferred_ids:
        for unit_id, rect in candidates:
            if unit_id.casefold() == preferred_id:
                return rect
    if candidates:
        return sorted(candidates, key=lambda candidate: candidate[0].casefold())[0][1]
    raise RuntimeError(f"screenshot_crop_unit_hud_missing:{side.casefold()}")


def _capture_battle_hud_crops(
    screenshot_path: Path,
    image_size: tuple[int, int],
    viewport: Any,
    probe: dict[str, Any],
    screenshot_geometry: Any = None,
) -> dict[str, Any]:
    transform = _viewport_to_screenshot_transform(viewport, image_size, screenshot_geometry)
    payload = probe.get("probe", {}) if isinstance(probe, dict) else {}
    board = payload.get("battle_board") if isinstance(payload, dict) else None
    party_qi = board.get("party_qi") if isinstance(board, dict) else None
    party_qi_rect = party_qi.get("screen_rect") if isinstance(party_qi, dict) else None
    stem = screenshot_path.stem
    suffix = screenshot_path.suffix or ".png"
    outputs = {"full": str(screenshot_path)}
    for output_key, rect in (
        ("hero", _unit_hud_crop_rect(probe, "Party")),
        ("enemy", _unit_hud_crop_rect(probe, "Enemy")),
        ("party_qi", party_qi_rect),
    ):
        output_path = screenshot_path.with_name(f"{stem}_{output_key}_hud{suffix}")
        outputs[output_key] = str(
            _crop_screenshot_to_viewport_rect(
                screenshot_path,
                image_size,
                viewport,
                rect,
                output_path,
                screenshot_geometry,
            )
        )
    return {"crops": outputs, "transform": transform}


def _should_capture_battle_hud_crops(battle_hud_verdict: Any) -> bool:
    """Only crop when UE Python supplied real Slate rectangles for the live widgets."""
    return not (
        isinstance(battle_hud_verdict, dict)
        and str(battle_hud_verdict.get("geometry_validation_mode", "")) == "fixed_slot_fallback"
    )


def _expect_npc_visual(actor: dict[str, Any], class_token: str, flipbook_token: str) -> dict[str, Any]:
    body = actor.get("body_character", {})
    if not isinstance(body, dict):
        body = {}
    visual = body or actor.get("visual_character", {})
    if not isinstance(visual, dict):
        visual = {}
    visual_component = visual.get("visual", {})
    if not isinstance(visual_component, dict):
        visual_component = {}
    class_text = " ".join(
        str(value)
        for value in (
            actor.get("class", ""),
            actor.get("name", ""),
            body.get("class", ""),
            body.get("path", ""),
            actor.get("visual_character_class", ""),
            visual.get("class", ""),
            visual.get("path", ""),
        )
    )
    flipbook_text = " ".join(
        str(value)
        for value in (
            visual.get("current_flipbook", ""),
            visual.get("get_default_town_flipbook_path_string", ""),
            visual_component.get("flipbook", ""),
        )
    )
    collision_text = str(visual.get("collision_enabled", ""))
    body_location = visual.get("location", {})
    if not isinstance(body_location, dict):
        body_location = {}
    grounded_root_z = float(body_location.get("z", 0.0) or 0.0)
    ok = bool(
        class_token in class_text
        and flipbook_token in flipbook_text
        and bool(visual.get("has_assigned_town_flipbook")) is True
        and bool(visual.get("is_town_moving")) is False
        and grounded_root_z >= 60.0
    )
    return {
        "ok": ok,
        "actor": actor.get("name", ""),
        "role": actor.get("get_npc_role", ""),
        "expected_class_token": class_token,
        "expected_flipbook_token": flipbook_token,
        "class_text": class_text,
        "flipbook_text": flipbook_text,
        "collision_enabled": collision_text,
        "actor_tick_enabled": visual.get("actor_tick_enabled"),
        "body_class": body.get("class", ""),
        "grounded_root_z": grounded_root_z,
    }


def _npc_visual_state(probe: dict[str, Any]) -> dict[str, Any]:
    checks = [
        _expect_npc_visual(_npc_by_role(probe, "QUEST"), "BP_NpcCharacter_C", "FB_Npc_Idle_South"),
        _expect_npc_visual(_npc_by_role(probe, "MERCHANT"), "BP_MerchantCharacter_C", "FB_Merchant_Idle_South"),
    ]
    return {
        "ok": all(bool(check.get("ok")) for check in checks),
        "checks": checks,
    }


def _quest_interacted(probe: dict[str, Any]) -> bool:
    npc = _quest_npc(probe)
    return bool(npc.get("was_last_interaction_successful") and npc.get("is_follower_active"))


def _task_offer_open(probe: dict[str, Any]) -> bool:
    task_panel = _flow_widgets(probe).get("task_panel", {})
    return bool(
        isinstance(task_panel, dict)
        and _widget_visible(probe, "task_panel")
        and task_panel.get("is_task_panel_open_for_test")
        and task_panel.get("is_showing_task_offers_for_test")
    )


def _expect_visual_state(probe: dict[str, Any], expected_state: str, expected_direction: str) -> dict[str, Any]:
    flipbook = _current_flipbook(probe)
    expected_token = f"/FB_Hero_{expected_state}_{expected_direction}."
    moving = _is_town_moving(probe)
    visual_rotation = ((_pawn_state(probe).get("visual") or {}).get("relative_rotation") or {})
    ok = expected_token in flipbook
    if expected_state == "Walk":
        ok = ok and moving
    if expected_state == "Idle":
        ok = ok and not moving
    try:
        ok = ok and abs(float(visual_rotation.get("yaw", 0.0)) - 90.0) <= 0.1
    except (TypeError, ValueError):
        ok = False
    return {
        "ok": bool(ok),
        "expected_state": expected_state,
        "expected_direction": expected_direction,
        "current_flipbook": flipbook,
        "is_town_moving": moving,
        "visual_yaw": visual_rotation.get("yaw"),
    }


def _has_play_session(probe: dict[str, Any]) -> bool:
    data = probe.get("probe", {})
    return bool(
        data.get("has_pie_world")
        and data.get("player_controller", {}).get("class", "").endswith("GameXXKMVPPlayerController")
        and data.get("hud", {}).get("class", "").endswith("GameXXKMVPHUD")
        and data.get("pawn", {}).get("class", "")
    )


def _topdown_camera_state(probe: dict[str, Any]) -> dict[str, Any]:
    pawn = probe.get("probe", {}).get("pawn", {})
    if not isinstance(pawn, dict):
        return {"ok": False, "reason": "pawn_missing"}
    camera = pawn.get("camera", {})
    spring_arm = pawn.get("spring_arm", {})
    if not isinstance(camera, dict) or not isinstance(spring_arm, dict):
        return {"ok": False, "reason": "camera_or_spring_arm_missing"}

    projection = str(camera.get("projection_mode", "")).upper()
    target_arm_length = float(spring_arm.get("target_arm_length") or 0.0)
    pitch = float((spring_arm.get("relative_rotation") or {}).get("pitch") or 0.0)
    camera_name = str(camera.get("name", ""))
    boom_name = str(spring_arm.get("name", ""))
    ok = bool(
        camera_name == "TopDownCamera"
        and boom_name == "CameraBoom"
        and "PERSPECTIVE" in projection
        # BP_HeroCharacter's camera was intentionally tuned in-editor. The
        # harness validates that current authored baseline rather than the
        # retired Ocean-style defaults.
        and 960.0 <= target_arm_length <= 1040.0
        and -35.0 <= pitch <= -25.0
        and bool(spring_arm.get("absolute_rotation")) is True
        and bool(spring_arm.get("do_collision_test")) is False
    )
    return {
        "ok": ok,
        "camera_name": camera_name,
        "boom_name": boom_name,
        "projection": projection,
        "target_arm_length": target_arm_length,
        "spring_arm_pitch": pitch,
        "absolute_rotation": bool(spring_arm.get("absolute_rotation")),
        "do_collision_test": bool(spring_arm.get("do_collision_test")),
    }


def _battle_camera_state(probe: dict[str, Any]) -> dict[str, Any]:
    player_controller = probe.get("probe", {}).get("player_controller", {})
    if not isinstance(player_controller, dict):
        return {"ok": False, "reason": "player_controller_missing"}
    view_target = player_controller.get("view_target", {})
    if not isinstance(view_target, dict):
        return {"ok": False, "reason": "view_target_missing"}
    player_camera = player_controller.get("player_camera", {})
    if not isinstance(player_camera, dict):
        player_camera = {}
    rotation = view_target.get("rotation", {})
    if not isinstance(rotation, dict):
        rotation = {}
    player_camera_rotation = player_camera.get("rotation", {})
    if not isinstance(player_camera_rotation, dict):
        player_camera_rotation = {}
    camera = view_target.get("camera", {})
    if not isinstance(camera, dict):
        camera = {}
    tags = " ".join(str(tag) for tag in view_target.get("tags", []))
    identity = " ".join(
        str(value)
        for value in (
            view_target.get("name", ""),
            view_target.get("label", ""),
            view_target.get("class", ""),
            tags,
        )
    )
    projection = str(camera.get("projection_mode", "")).upper()
    view_target_pitch = float(rotation.get("pitch") or rotation.get("roll") or 0.0)
    actual_pitch = float(player_camera_rotation.get("pitch") or view_target_pitch)
    ok = bool(
        "GameXXK_BattleScene_Camera" in identity
        and "CameraActor" in str(view_target.get("class", ""))
        and "PERSPECTIVE" in projection
        and -65.0 <= actual_pitch <= -55.0
    )
    return {
        "ok": ok,
        "identity": identity,
        "projection": projection,
        "pitch": actual_pitch,
        "view_target_pitch": view_target_pitch,
        "location": view_target.get("location", {}),
        "player_camera": player_camera,
        "field_of_view": camera.get("field_of_view"),
    }


def _distance(a: dict[str, float], b: dict[str, float]) -> float:
    return math.sqrt(sum((float(a.get(axis, 0.0)) - float(b.get(axis, 0.0))) ** 2 for axis in ("x", "y", "z")))


class PreviewInput:
    def __init__(self) -> None:
        self.user32 = ctypes.windll.user32
        self.gdi32 = ctypes.windll.gdi32
        self.user32.WindowFromPoint.restype = ctypes.c_void_p
        try:
            self.user32.SetProcessDPIAware()
        except Exception:
            pass

    def find_preview_window(self) -> dict[str, Any]:
        enum_proc_type = ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.c_void_p, ctypes.c_void_p)

        class Rect(ctypes.Structure):
            _fields_ = [
                ("left", ctypes.c_long),
                ("top", ctypes.c_long),
                ("right", ctypes.c_long),
                ("bottom", ctypes.c_long),
            ]

        matches: list[dict[str, Any]] = []

        def enum_proc(hwnd, _lparam):
            if not self.user32.IsWindowVisible(hwnd):
                return True
            length = self.user32.GetWindowTextLengthW(hwnd)
            if length <= 0:
                return True
            buffer = ctypes.create_unicode_buffer(length + 1)
            self.user32.GetWindowTextW(hwnd, buffer, length + 1)
            title = buffer.value
            if "GameXXK Preview" in title:
                rect = Rect()
                self.user32.GetWindowRect(hwnd, ctypes.byref(rect))
                matches.append({
                    "hwnd": int(hwnd),
                    "title": title,
                    "rect": [rect.left, rect.top, rect.right, rect.bottom],
                })
            return True

        self.user32.EnumWindows(enum_proc_type(enum_proc), 0)
        if not matches:
            raise RuntimeError("GameXXK Preview window was not found")
        return matches[0]

    def preview_window_geometry(self, window: dict[str, Any]) -> dict[str, Any]:
        """Capture outer and client screen bounds from the exact HWND used for a Slate screenshot."""
        class Rect(ctypes.Structure):
            _fields_ = [
                ("left", ctypes.c_long),
                ("top", ctypes.c_long),
                ("right", ctypes.c_long),
                ("bottom", ctypes.c_long),
            ]

        class Point(ctypes.Structure):
            _fields_ = [("x", ctypes.c_long), ("y", ctypes.c_long)]

        hwnd_value = int(window.get("hwnd", 0) or 0)
        if hwnd_value <= 0:
            raise RuntimeError("GameXXK Preview HWND was invalid")
        hwnd = ctypes.c_void_p(hwnd_value)
        window_rect = Rect()
        client_rect = Rect()
        client_origin = Point(0, 0)
        if not self.user32.GetWindowRect(hwnd, ctypes.byref(window_rect)):
            raise RuntimeError("GetWindowRect failed for GameXXK Preview")
        if not self.user32.GetClientRect(hwnd, ctypes.byref(client_rect)):
            raise RuntimeError("GetClientRect failed for GameXXK Preview")
        if not self.user32.ClientToScreen(hwnd, ctypes.byref(client_origin)):
            raise RuntimeError("ClientToScreen failed for GameXXK Preview")
        client_width = int(client_rect.right - client_rect.left)
        client_height = int(client_rect.bottom - client_rect.top)
        if client_width <= 0 or client_height <= 0:
            raise RuntimeError("GameXXK Preview client bounds were invalid")
        return {
            "hwnd": hwnd_value,
            "window_screen_rect": _rect_dict(
                float(window_rect.left),
                float(window_rect.top),
                float(window_rect.right),
                float(window_rect.bottom),
            ),
            "client_screen_rect": _rect_dict(
                float(client_origin.x),
                float(client_origin.y),
                float(client_origin.x + client_width),
                float(client_origin.y + client_height),
            ),
        }

    def capture_window_png(self, window: dict[str, Any]) -> tuple[bytes, tuple[int, int]]:
        class BitmapInfoHeader(ctypes.Structure):
            _fields_ = [
                ("biSize", ctypes.c_uint32),
                ("biWidth", ctypes.c_int32),
                ("biHeight", ctypes.c_int32),
                ("biPlanes", ctypes.c_uint16),
                ("biBitCount", ctypes.c_uint16),
                ("biCompression", ctypes.c_uint32),
                ("biSizeImage", ctypes.c_uint32),
                ("biXPelsPerMeter", ctypes.c_int32),
                ("biYPelsPerMeter", ctypes.c_int32),
                ("biClrUsed", ctypes.c_uint32),
                ("biClrImportant", ctypes.c_uint32),
            ]

        class BitmapInfo(ctypes.Structure):
            _fields_ = [("bmiHeader", BitmapInfoHeader), ("bmiColors", ctypes.c_uint32 * 3)]

        left, top, right, bottom = [int(v) for v in window["rect"]]
        width = max(0, right - left)
        height = max(0, bottom - top)
        if width <= 0 or height <= 0:
            raise RuntimeError(f"Cannot capture GameXXK Preview: invalid rect={window['rect']}")

        hwnd = ctypes.c_void_p(int(window["hwnd"]))
        self.focus(window)
        source_dc = self.user32.GetWindowDC(hwnd)
        if not source_dc:
            raise RuntimeError("GetWindowDC failed for GameXXK Preview")
        memory_dc = self.gdi32.CreateCompatibleDC(source_dc)
        bitmap = self.gdi32.CreateCompatibleBitmap(source_dc, width, height)
        previous = self.gdi32.SelectObject(memory_dc, bitmap)
        try:
            try:
                self.user32.PrintWindow.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_uint]
                self.user32.PrintWindow.restype = ctypes.c_bool
                ok = bool(self.user32.PrintWindow(hwnd, memory_dc, 0x00000002))
            except Exception:
                ok = False
            if not ok:
                ok = bool(self.gdi32.BitBlt(memory_dc, 0, 0, width, height, source_dc, 0, 0, 0x00CC0020))
            if not ok:
                raise RuntimeError("Window capture failed for GameXXK Preview")
            info = BitmapInfo()
            info.bmiHeader.biSize = ctypes.sizeof(BitmapInfoHeader)
            info.bmiHeader.biWidth = width
            info.bmiHeader.biHeight = -height
            info.bmiHeader.biPlanes = 1
            info.bmiHeader.biBitCount = 32
            info.bmiHeader.biCompression = 0
            buffer = (ctypes.c_ubyte * (width * height * 4))()
            lines = self.gdi32.GetDIBits(source_dc, bitmap, 0, height, ctypes.byref(buffer), ctypes.byref(info), 0)
            if lines != height:
                raise RuntimeError(f"GetDIBits captured {lines}/{height} lines")
            bgra = bytes(buffer)
            rgba = bytearray(len(bgra))
            rgba[0::4] = bgra[2::4]
            rgba[1::4] = bgra[1::4]
            rgba[2::4] = bgra[0::4]
            rgba[3::4] = b"\xff" * (width * height)
            return _rgba_to_png(width, height, bytes(rgba)), (width, height)
        finally:
            if previous:
                self.gdi32.SelectObject(memory_dc, previous)
            if bitmap:
                self.gdi32.DeleteObject(bitmap)
            if memory_dc:
                self.gdi32.DeleteDC(memory_dc)
            self.user32.ReleaseDC(hwnd, source_dc)

    def focus(self, window: dict[str, Any]) -> None:
        hwnd = ctypes.c_void_p(int(window["hwnd"]))
        self.user32.ShowWindow(hwnd, 5)
        self.user32.SetForegroundWindow(hwnd)
        time.sleep(0.2)

    def focus_keyboard_input(self, window: dict[str, Any]) -> None:
        left, top, right, bottom = [int(v) for v in window["rect"]]
        self.focus(window)
        self.user32.SetCursorPos((left + right) // 2, (top + bottom) // 2)
        time.sleep(0.05)
        self.user32.mouse_event(0x0002, 0, 0, 0, 0)
        time.sleep(0.03)
        self.user32.mouse_event(0x0004, 0, 0, 0, 0)
        time.sleep(0.1)

    def click_image_point(self, window: dict[str, Any], image_size: tuple[int, int], image_x: int, image_y: int) -> dict[str, int]:
        left, top, right, bottom = [int(v) for v in window["rect"]]
        width = max(1, right - left)
        height = max(1, bottom - top)
        image_width, image_height = image_size
        x = left + int(image_x * width / max(1, image_width))
        y = top + int(image_y * height / max(1, image_height))
        self.focus(window)
        self.user32.SetCursorPos(x, y)
        time.sleep(0.1)
        self.user32.mouse_event(0x0002, 0, 0, 0, 0)
        time.sleep(0.05)
        self.user32.mouse_event(0x0004, 0, 0, 0, 0)
        return {"x": x, "y": y}

    def click_screen_point(self, window: dict[str, Any], screen_x: int, screen_y: int) -> dict[str, int]:
        class Rect(ctypes.Structure):
            _fields_ = [
                ("left", ctypes.c_long),
                ("top", ctypes.c_long),
                ("right", ctypes.c_long),
                ("bottom", ctypes.c_long),
            ]

        class Point(ctypes.Structure):
            _fields_ = [
                ("x", ctypes.c_long),
                ("y", ctypes.c_long),
            ]

        hwnd = ctypes.c_void_p(int(window["hwnd"]))
        client_rect = Rect()
        if not self.user32.GetClientRect(hwnd, ctypes.byref(client_rect)):
            raise RuntimeError("GetClientRect failed for GameXXK Preview")
        width = max(1, int(client_rect.right - client_rect.left))
        height = max(1, int(client_rect.bottom - client_rect.top))
        local_x = max(0, min(width - 1, int(screen_x)))
        local_y = max(0, min(height - 1, int(screen_y)))
        point = Point(local_x, local_y)
        if not self.user32.ClientToScreen(hwnd, ctypes.byref(point)):
            raise RuntimeError("ClientToScreen failed for GameXXK Preview")
        x = int(point.x)
        y = int(point.y)
        self.click_window_message(window, x, y)
        return {"x": x, "y": y, "local_x": local_x, "local_y": local_y}

    def click_absolute_point(self, window: dict[str, Any], screen_x: int, screen_y: int) -> dict[str, int]:
        class Point(ctypes.Structure):
            _fields_ = [
                ("x", ctypes.c_long),
                ("y", ctypes.c_long),
            ]

        x = int(screen_x)
        y = int(screen_y)
        self.focus(window)
        self.user32.SetCursorPos(x, y)
        time.sleep(0.1)
        self.user32.mouse_event(0x0002, 0, 0, 0, 0)
        time.sleep(0.05)
        self.user32.mouse_event(0x0004, 0, 0, 0, 0)
        time.sleep(0.1)

        screen_point = Point(x, y)
        target_hwnd_value = self.user32.WindowFromPoint(screen_point)
        target_hwnd = ctypes.c_void_p(int(target_hwnd_value) if target_hwnd_value else int(window["hwnd"]))
        local_point = Point(x, y)
        local_x = 0
        local_y = 0
        if self.user32.ScreenToClient(target_hwnd, ctypes.byref(local_point)):
            local_x = int(local_point.x)
            local_y = int(local_point.y)
        return {
            "x": x,
            "y": y,
            "local_x": local_x,
            "local_y": local_y,
            "desktop_click": True,
            "target_hwnd": int(target_hwnd.value or 0),
            "root_hwnd": int(window["hwnd"]),
        }

    def click_window_message(self, window: dict[str, Any], screen_x: int, screen_y: int) -> dict[str, int]:
        class Point(ctypes.Structure):
            _fields_ = [
                ("x", ctypes.c_long),
                ("y", ctypes.c_long),
            ]

        root_hwnd = ctypes.c_void_p(int(window["hwnd"]))
        screen_point = Point(int(screen_x), int(screen_y))

        self.focus(window)
        self.user32.SetCursorPos(int(screen_x), int(screen_y))
        target_hwnd_value = self.user32.WindowFromPoint(screen_point)
        target_hwnd = ctypes.c_void_p(int(target_hwnd_value) if target_hwnd_value else int(window["hwnd"]))

        point = Point(int(screen_x), int(screen_y))
        if not self.user32.ScreenToClient(target_hwnd, ctypes.byref(point)):
            raise RuntimeError("ScreenToClient failed for GameXXK Preview")
        local_x = int(point.x)
        local_y = int(point.y)
        lparam = (local_y << 16) | (local_x & 0xFFFF)

        time.sleep(0.08)
        self.user32.SendMessageW(target_hwnd, 0x0200, 0, lparam)
        time.sleep(0.03)
        self.user32.SendMessageW(target_hwnd, 0x0201, 0x0001, lparam)
        time.sleep(0.05)
        self.user32.SendMessageW(target_hwnd, 0x0202, 0, lparam)
        return {"local_x": local_x, "local_y": local_y, "target_hwnd": int(target_hwnd.value or 0), "root_hwnd": int(root_hwnd.value or 0)}

    def press_key(self, window: dict[str, Any], virtual_key: int, hold_seconds: float = 0.0) -> None:
        self.focus_keyboard_input(window)
        self.user32.keybd_event(virtual_key, 0, 0, 0)
        time.sleep(max(0.0, hold_seconds))
        self.user32.keybd_event(virtual_key, 0, 0x0002, 0)

    def key_down(self, window: dict[str, Any], virtual_key: int, *, refocus: bool = True) -> None:
        if refocus:
            self.focus_keyboard_input(window)
        self.user32.keybd_event(virtual_key, 0, 0, 0)

    def key_up(self, virtual_key: int) -> None:
        self.user32.keybd_event(virtual_key, 0, 0x0002, 0)


class RealFlowHarness:
    def __init__(self, timeout: float, keep_pie: bool) -> None:
        self.client = UnrealMCPClient(timeout=timeout)
        self.input = PreviewInput()
        self.keep_pie = keep_pie
        self.events: list[dict[str, Any]] = []
        self.battle_hud_fixture_may_be_applied = False
        self._screenshot_contexts: dict[str, dict[str, Any]] = {}

    def event(self, name: str, **payload: Any) -> None:
        item = {"name": name, **payload}
        self.events.append(item)
        print(json.dumps(item, ensure_ascii=False), flush=True)

    def connect(self) -> None:
        if not self.client.connect():
            raise RuntimeError(f"Cannot connect to UE MCP at {self.client.endpoint}")
        self.event("mcp_connected", endpoint=self.client.endpoint)

    def probe(self, *args: str) -> dict[str, Any]:
        result = self.client.run_project_python_file(PROBE_SCRIPT, list(args))
        parsed = _load_json_from_probe(result)
        self.event("probe", screen=_runtime_screen(parsed), map=_map_name(parsed))
        return parsed

    def screenshot_context_for(self, screenshot_path: Path) -> dict[str, Any]:
        return dict(getattr(self, "_screenshot_contexts", {}).get(str(screenshot_path), {}))

    def apply_battle_hud_fixture(self) -> dict[str, Any]:
        """Apply the non-saving HUD fixture through the project Python/MCP boundary."""
        self.battle_hud_fixture_may_be_applied = True
        payload = _load_json_from_probe(self.client.run_project_python_file(BATTLE_HUD_FIXTURE_SCRIPT, []))
        fixture = payload.get("battle_hud_fixture", {})
        if not isinstance(fixture, dict):
            fixture = {}
        self.event("battle_hud_fixture_apply", detail=fixture)
        if not fixture.get("ok"):
            raise RuntimeError(f"Battle HUD fixture apply failed: {fixture}")
        return payload

    def clear_battle_hud_fixture(self) -> dict[str, Any]:
        """Restore raw battle presentation after a fixture capture, even when PIE stays open."""
        payload = _load_json_from_probe(
            self.client.run_project_python_file(BATTLE_HUD_FIXTURE_SCRIPT, ["--clear"])
        )
        clear_result = payload.get("battle_hud_fixture_clear", {})
        if not isinstance(clear_result, dict):
            clear_result = {}
        self.event("battle_hud_fixture_clear", detail=clear_result)
        if not clear_result.get("ok"):
            raise RuntimeError(f"Battle HUD fixture clear failed: {clear_result}")
        self.battle_hud_fixture_may_be_applied = False
        return payload

    def observe_battle_actor_hud(self) -> dict[str, Any]:
        """Read the current PIE battle HUD without starting, stopping, or clicking PIE."""
        self.connect()
        if not self.client.is_in_pie():
            raise RuntimeError("Battle actor HUD observation requires an active PIE session")
        observation = _battle_hud_observation(self.probe())
        observation["mcp_endpoint"] = self.client.endpoint
        observation["pie_running"] = True
        self.event(
            "battle_actor_hud_observation",
            map_name=observation.get("map_name"),
            viewport=observation.get("viewport"),
            viewport_source=observation.get("viewport_source"),
            verdict=observation.get("battle_hud_verdict"),
        )
        observation["events"] = list(self.events)
        return observation

    def hud_command(self, command_name: str) -> dict[str, Any]:
        result = self.client.run_project_python_file(PROBE_SCRIPT, ["--hud-command", command_name])
        parsed = _load_json_from_probe(result)
        command_result = parsed.get("hud_command", {})
        if not isinstance(command_result, dict):
            command_result = {}
        self.event("hud_command", command=command_name, ok=bool(command_result.get("ok")), detail=command_result)
        if not command_result.get("ok"):
            raise RuntimeError(f"HUD command {command_name} failed: {command_result}")
        return parsed

    def town_command(self, command_name: str) -> dict[str, Any]:
        result = self.client.run_project_python_file(PROBE_SCRIPT, ["--town-command", command_name])
        parsed = _load_json_from_probe(result)
        command_result = parsed.get("town_command", {})
        if not isinstance(command_result, dict):
            command_result = {}
        self.event("town_command", command=command_name, ok=bool(command_result.get("ok")), detail=command_result)
        if not command_result.get("ok"):
            raise RuntimeError(f"Town widget command {command_name} failed: {command_result}")
        return parsed

    def town_key(self, key: str, pressed: bool) -> dict[str, Any]:
        key = str(key).upper()
        state = "down" if bool(pressed) else "up"
        result = self.client.run_project_python_file(
            PROBE_SCRIPT,
            ["--town-key", key, state],
        )
        parsed = _load_json_from_probe(result)
        key_result = parsed.get("town_key", {})
        if not isinstance(key_result, dict):
            key_result = {}
        self.event(
            "town_key",
            backend="mcp_project_python",
            probe_script=PROBE_SCRIPT,
            key=key,
            state=state,
            ok=bool(key_result.get("ok")),
            detail=key_result,
        )
        if not key_result.get("ok"):
            raise RuntimeError(f"Town key command {key} {state} failed: {key_result}")
        return parsed

    @contextmanager
    def hold_town_keys(self, *keys: str):
        """Hold PIE town keys and always best-effort release them in reverse order."""
        held_keys: list[str] = []
        primary_error: BaseException | None = None
        try:
            for key in keys:
                normalized_key = str(key).upper()
                # Register before transport so a lost MCP reply still receives a
                # best-effort key-up in case PIE already consumed the key-down.
                held_keys.append(normalized_key)
                self.town_key(normalized_key, True)
            yield
        except BaseException as error:
            primary_error = error
            raise
        finally:
            release_error: Exception | None = None
            for key in reversed(held_keys):
                try:
                    self.town_key(key, False)
                except Exception as error:
                    self.event(
                        "town_key_release_failed",
                        backend="mcp_project_python",
                        probe_script=PROBE_SCRIPT,
                        key=key,
                        state="up",
                        error=str(error),
                    )
                    if release_error is None:
                        release_error = error
            if release_error is not None and primary_error is None:
                raise RuntimeError(f"Town key release failed after holding {held_keys}") from release_error

    def run_serialized_mcp_d_to_w_release(self) -> dict[str, Any]:
        """Prove W-only motion after D-up before releasing W for the final idle state."""
        with self.hold_town_keys("W"):
            with self.hold_town_keys("D"):
                time.sleep(0.25)
                while_diagonal_down = self.probe()
                diagonal_walk_state = _expect_visual_state(while_diagonal_down, "Walk", "NorthEast")
                self.event("diagonal_walk_state_probe", **diagonal_walk_state)
                if not diagonal_walk_state.get("ok"):
                    raise RuntimeError(f"Town diagonal keys did not switch hero to Walk_NorthEast while held: {diagonal_walk_state}")
                time.sleep(0.25)
            time.sleep(DIAGONAL_D_TO_W_VERTICAL_PROBE_SETTLE_SECONDS)
            while_vertical_only = self.probe()
            vertical_walk_state = _expect_visual_state(while_vertical_only, "Walk", "North")
            self.event("diagonal_vertical_state_probe", **vertical_walk_state)
            if not vertical_walk_state.get("ok"):
                raise RuntimeError(
                    f"Town D-up must leave W-only Walk_North before W release: {vertical_walk_state}"
                )
        time.sleep(0.25)
        return self.probe()

    def town_interact(self) -> dict[str, Any]:
        result = self.client.run_project_python_file(PROBE_SCRIPT, ["--town-interact"])
        parsed = _load_json_from_probe(result)
        interact_result = parsed.get("town_interact", {})
        if not isinstance(interact_result, dict):
            interact_result = {}
        self.event(
            "town_interact",
            backend="mcp_project_python",
            probe_script=PROBE_SCRIPT,
            ok=bool(interact_result.get("ok")),
            detail=interact_result,
        )
        if not interact_result.get("ok"):
            raise RuntimeError(f"Town interact command failed: {interact_result}")
        return parsed

    def route_node(self, node_index: int) -> dict[str, Any]:
        result = self.client.run_project_python_file(PROBE_SCRIPT, ["--route-node", str(node_index)])
        parsed = _load_json_from_probe(result)
        command_result = parsed.get("route_node", {})
        if not isinstance(command_result, dict):
            command_result = {}
        self.event("route_node", node_index=node_index, ok=bool(command_result.get("ok")), detail=command_result)
        if not command_result.get("ok"):
            raise RuntimeError(f"Route widget node {node_index} failed: {command_result}")
        return parsed

    def slate_preview_snapshot(self) -> str:
        root_snapshot = str(self.client.call_tool(
            "Snapshot",
            {"ref": "", "maxDepth": 3, "bIncludeSourceLocations": False},
            toolset_name=SLATE_TOOLSET,
            timeout=self.client.timeout,
        ))
        preview_ref = _slate_preview_window_ref(root_snapshot)
        if not preview_ref:
            raise RuntimeError(f"GameXXK Preview Slate window was not found in snapshot: {root_snapshot[:1000]}")

        self.client.call_tool(
            "Observe",
            {"ref": preview_ref, "maxDepth": 80},
            toolset_name=SLATE_TOOLSET,
            timeout=self.client.timeout,
        )
        time.sleep(0.15)
        return str(self.client.call_tool(
            "Snapshot",
            {"ref": preview_ref, "maxDepth": 80, "bIncludeSourceLocations": False},
            toolset_name=SLATE_TOOLSET,
            timeout=self.client.timeout,
        ))

    def slate_screenshot(self, name: str) -> tuple[Path, tuple[int, int]]:
        """Capture the PIE Slate window without relying on a visible Win32 title bar."""
        root_snapshot = str(self.client.call_tool(
            "Snapshot",
            {"ref": "", "maxDepth": 3, "bIncludeSourceLocations": False},
            toolset_name=SLATE_TOOLSET,
            timeout=self.client.timeout,
        ))
        preview_ref = _slate_preview_window_ref(root_snapshot)
        screenshot_context: dict[str, Any] = {
            "transport": "slate",
            "preview_ref": preview_ref,
        }
        preview_window = None
        try:
            preview_window = self.input.find_preview_window()
            screenshot_context["window_geometry_before"] = self.input.preview_window_geometry(preview_window)
        except Exception as exc:
            screenshot_context["geometry_error_before"] = type(exc).__name__
        payload = self.client.call_tool(
            "Screenshot",
            {"ref": preview_ref},
            toolset_name=SLATE_TOOLSET,
            timeout=self.client.timeout,
        )
        data = _decode_slate_screenshot_png(payload)
        size = _png_size(data)
        if preview_window is not None:
            try:
                screenshot_context["window_geometry_after"] = self.input.preview_window_geometry(preview_window)
            except Exception as exc:
                screenshot_context["geometry_error_after"] = type(exc).__name__
        screenshot_context["geometry_stable"] = _window_geometry_is_stable(
            screenshot_context.get("window_geometry_before"),
            screenshot_context.get("window_geometry_after"),
        )
        if size[0] <= 0 or size[1] <= 0:
            raise RuntimeError(f"Captured invalid Slate screenshot size for {name}: {size}")
        SCREENSHOT_DIR.mkdir(parents=True, exist_ok=True)
        path = SCREENSHOT_DIR / name
        path.write_bytes(data)
        self._screenshot_contexts[str(path)] = screenshot_context
        self.event(
            "screenshot",
            path=str(path),
            size=list(size),
            transport="slate",
            geometry_stable=screenshot_context["geometry_stable"],
        )
        return path, size

    def slate_button_for_route_node(self, node_state: dict[str, Any]) -> dict[str, Any]:
        target_position = node_state.get("screen_hit_box_position", {})
        target_size = node_state.get("hit_box_size", {})
        if not isinstance(target_position, dict) or not isinstance(target_size, dict):
            raise RuntimeError(f"Route node visual state does not expose Slate hit box data: {node_state}")

        target_x = float(target_position.get("x", 0.0))
        target_y = float(target_position.get("y", 0.0))
        target_w = float(target_size.get("x", 0.0))
        target_h = float(target_size.get("y", 0.0))
        snapshot = self.slate_preview_snapshot()
        candidates: list[dict[str, Any]] = []
        for match in SLATE_BUTTON_PATTERN.finditer(snapshot):
            candidate = {
                "ref": match.group("ref"),
                "disabled": bool(match.group("disabled")),
                "x": int(match.group("x")),
                "y": int(match.group("y")),
                "w": int(match.group("w")),
                "h": int(match.group("h")),
            }
            if candidate["disabled"]:
                continue
            candidate["distance"] = (
                abs(candidate["x"] - target_x)
                + abs(candidate["y"] - target_y)
                + abs(candidate["w"] - target_w)
                + abs(candidate["h"] - target_h)
            )
            candidates.append(candidate)

        if not candidates:
            raise RuntimeError(f"No enabled Slate buttons were found in the route map snapshot: {snapshot[:1200]}")

        best = min(candidates, key=lambda item: float(item["distance"]))
        if float(best["distance"]) > 8.0:
            raise RuntimeError(
                f"No Slate button matched route node {node_state.get('node_id')} "
                f"target=({target_x},{target_y},{target_w},{target_h}); best={best}; snapshot={snapshot[:1200]}"
            )
        return best

    def slate_button_for_visible_text(self, label: str) -> dict[str, Any]:
        snapshot = self.slate_preview_snapshot()
        named_candidates = [
            {
                "ref": match.group("ref"),
                "disabled": bool(match.group("disabled")),
                "x": int(match.group("x")),
                "y": int(match.group("y")),
                "w": int(match.group("w")),
                "h": int(match.group("h")),
            }
            for match in SLATE_BUTTON_PATTERN.finditer(snapshot)
            if match.group("label") == label and not bool(match.group("disabled"))
        ]
        if named_candidates:
            return named_candidates[-1]

        label_index = snapshot.rfind(f'"{label}"')
        if label_index < 0:
            raise RuntimeError(f"Slate text '{label}' was not present in the preview snapshot: {snapshot[:2400]}")

        candidates = [
            {
                "ref": match.group("ref"),
                "disabled": bool(match.group("disabled")),
                "x": int(match.group("x")),
                "y": int(match.group("y")),
                "w": int(match.group("w")),
                "h": int(match.group("h")),
            }
            for match in SLATE_BUTTON_PATTERN.finditer(snapshot[:label_index])
        ]
        candidates = [candidate for candidate in candidates if not candidate["disabled"]]
        if not candidates:
            raise RuntimeError(f"No enabled Slate button precedes dialog text '{label}': {snapshot[:2400]}")
        return candidates[-1]

    def click_main_menu_start(self) -> None:
        """Exercise the authored visible Start/New Game button, not the C++ API."""
        button = self.slate_button_for_visible_text("开始游戏")
        click_ok = bool(self.client.call_tool(
            "Click",
            {"ref": button["ref"], "button": "left", "doubleClick": False},
            toolset_name=SLATE_TOOLSET,
            timeout=self.client.timeout,
        ))
        self.event("main_menu_start_slate_click", click_ok=click_ok, button=button, label="开始游戏")
        if not click_ok:
            raise RuntimeError(f"Slate click failed for the main-menu Start/New Game button: {button}")
        time.sleep(0.45)

    def click_world_map_qingshan(self) -> None:
        """Exercise the authored Qingshan town marker after New Game opens the world map."""
        button = self.slate_button_for_visible_text("青山镇")
        click_ok = bool(self.client.call_tool(
            "Click",
            {"ref": button["ref"], "button": "left", "doubleClick": False},
            toolset_name=SLATE_TOOLSET,
            timeout=self.client.timeout,
        ))
        self.event("world_map_qingshan_slate_click", click_ok=click_ok, button=button, label="青山镇")
        if not click_ok:
            raise RuntimeError(f"Slate click failed for the world-map Qingshan town marker: {button}")
        time.sleep(0.45)

    def click_route_node(self, probe: dict[str, Any], node_id: int) -> dict[str, Any]:
        node_state = _route_node_visual_state(probe, node_id)
        if not node_state:
            raise RuntimeError(f"Route node {node_id} visual state was not found: {_route_node_visual_states(probe)}")
        if not bool(node_state.get("b_enabled")):
            raise RuntimeError(f"Route node {node_id} is not enabled for screen click: {node_state}")
        button = self.slate_button_for_route_node(node_state)
        click_ok = bool(self.client.call_tool(
            "Click",
            {"ref": button["ref"], "button": "left", "doubleClick": False},
            toolset_name=SLATE_TOOLSET,
            timeout=self.client.timeout,
        ))
        self.event("route_node_slate_click", node_id=node_id, click_ok=click_ok, button=button, node_state=node_state)
        if not click_ok:
            raise RuntimeError(f"Slate click failed for route node {node_id}: button={button}")
        time.sleep(0.45)
        return self.probe()

    def click_quest_offer_accept(self) -> dict[str, Any]:
        """Accept strictly through the new available-task panel's visible action."""
        label = "接取"
        button = self.slate_button_for_visible_text(label)
        click_ok = bool(self.client.call_tool(
            "Click",
            {"ref": button["ref"], "button": "left", "doubleClick": False},
            toolset_name=SLATE_TOOLSET,
            timeout=self.client.timeout,
        ))
        self.event("task_offer_slate_click", click_ok=click_ok, button=button, label=label)
        if not click_ok:
            raise RuntimeError(f"Slate click failed for quest offer accept button: {button}")
        time.sleep(0.45)
        return self.probe()

    def click_task_panel_back(self) -> dict[str, Any]:
        """Click the visible task panel's authored back-arrow button through Slate."""
        snapshot = self.slate_preview_snapshot()
        candidates = [
            {
                "ref": match.group("ref"),
                "disabled": bool(match.group("disabled")),
                "x": int(match.group("x")),
                "y": int(match.group("y")),
                "w": int(match.group("w")),
                "h": int(match.group("h")),
            }
            for match in SLATE_BUTTON_PATTERN.finditer(snapshot)
            if not bool(match.group("disabled"))
            and int(match.group("w")) == 50
            and int(match.group("h")) == 27
        ]
        if len(candidates) != 1:
            raise RuntimeError(
                "Task-panel back button was not uniquely identified by its authored 50x27 Slate hit box: "
                f"{candidates}; snapshot={snapshot[:2400]}"
            )
        button = candidates[0]
        click_ok = bool(self.client.call_tool(
            "Click",
            {"ref": button["ref"], "button": "left", "doubleClick": False},
            toolset_name=SLATE_TOOLSET,
            timeout=self.client.timeout,
        ))
        self.event("task_panel_back_slate_click", click_ok=click_ok, button=button)
        if not click_ok:
            raise RuntimeError(f"Slate click failed for task-panel back button: {button}")
        time.sleep(0.45)
        return self.probe()

    def screenshot(self, name: str) -> tuple[Path, tuple[int, int]]:
        try:
            return self.slate_screenshot(name)
        except Exception as slate_error:
            self.event("slate_screenshot_failed", error=str(slate_error))
        window = self.input.find_preview_window()
        screenshot_context: dict[str, Any] = {"transport": "win32"}
        try:
            screenshot_context["window_geometry_before"] = self.input.preview_window_geometry(window)
        except Exception as exc:
            screenshot_context["geometry_error_before"] = type(exc).__name__
        data, size = self.input.capture_window_png(window)
        try:
            screenshot_context["window_geometry_after"] = self.input.preview_window_geometry(window)
        except Exception as exc:
            screenshot_context["geometry_error_after"] = type(exc).__name__
        screenshot_context["geometry_stable"] = _window_geometry_is_stable(
            screenshot_context.get("window_geometry_before"),
            screenshot_context.get("window_geometry_after"),
        )
        SCREENSHOT_DIR.mkdir(parents=True, exist_ok=True)
        path = SCREENSHOT_DIR / name
        path.write_bytes(data)
        if size[0] <= 0 or size[1] <= 0:
            raise RuntimeError(f"Captured invalid screenshot size for {name}: {size}")
        self._screenshot_contexts[str(path)] = screenshot_context
        self.event(
            "screenshot",
            path=str(path),
            size=list(size),
            transport="win32",
            geometry_stable=screenshot_context["geometry_stable"],
        )
        return path, size

    def wait_for(self, label: str, predicate, timeout: float = 10.0, interval: float = 0.5) -> dict[str, Any]:
        deadline = time.monotonic() + timeout
        last_probe: dict[str, Any] = {}
        while time.monotonic() < deadline:
            last_probe = self.probe()
            if predicate(last_probe):
                self.event("wait_ok", label=label)
                return last_probe
            time.sleep(interval)
        raise RuntimeError(f"Timed out waiting for {label}; last probe={json.dumps(last_probe, ensure_ascii=False)}")

    def walk_to_world_location(self, start_probe: dict[str, Any], target: dict[str, float]) -> dict[str, Any]:
        speed = 260.0
        current = _pawn_location(start_probe)
        if not current:
            raise RuntimeError("Cannot walk: pawn location missing")

        def press_for_axis(delta: float, positive_key: str, negative_key: str) -> None:
            if abs(delta) < 24.0:
                return
            duration = min(8.0, max(0.05, abs(delta) / speed))
            key = positive_key if delta > 0.0 else negative_key
            with self.hold_town_keys(key):
                self.event("town_key_hold", key=key, duration_seconds=duration)
                time.sleep(duration)
            time.sleep(0.15)

        press_for_axis(float(target.get("x", 0.0)) - float(current.get("x", 0.0)), "W", "S")
        mid_probe = self.probe()
        mid = _pawn_location(mid_probe)
        press_for_axis(float(target.get("y", 0.0)) - float(mid.get("y", 0.0)), "D", "A")
        after_probe = self.probe()

        for _ in range(2):
            current = _pawn_location(after_probe)
            if _distance(current, target) <= 96.0:
                break
            press_for_axis(float(target.get("x", 0.0)) - float(current.get("x", 0.0)), "W", "S")
            after_probe = self.probe()
            current = _pawn_location(after_probe)
            press_for_axis(float(target.get("y", 0.0)) - float(current.get("y", 0.0)), "D", "A")
            after_probe = self.probe()

        self.event("walk_probe", distance_to_target=_distance(_pawn_location(after_probe), target))
        return after_probe

    def run(self) -> dict[str, Any]:
        self.connect()
        if self.client.is_in_pie():
            self.client.stop_pie()
            if not self.client.wait_for_pie_state(False):
                raise RuntimeError("PIE did not stop before the real-play flow loaded a fresh level")
            self.event("stopped_existing_pie")
        self.client.call_tool("load_level", {"level_path": MAIN_MAP}, toolset_name=SCENE_TOOLSET, timeout=60.0)
        self.event("loaded_main_map", map=MAIN_MAP)
        self.probe("--delete-default-save")

        self.client.call_tool(
            "StartPIE",
            {"options": {"bSimulate": False, "playMode": "PlayMode_InEditorFloating", "warmupSeconds": 1.0}},
            toolset_name=EDITOR_TOOLSET,
            timeout=60.0,
        )
        self.event("started_pie")
        self.wait_for("PIE play session", _has_play_session)

        before_start_probe = self.probe()
        main_menu_probe = {
            "ok": _widget_visible(before_start_probe, "main_menu"),
            "widgets": _flow_widgets(before_start_probe),
        }
        self.event("main_menu_widget_probe", **main_menu_probe)
        if not main_menu_probe["ok"]:
            raise RuntimeError(f"L_Main PIE did not show the player main menu: {main_menu_probe}")

        before_start_path, _ = self.screenshot("real_flow_before_start.png")
        self.click_main_menu_start()
        after_world_map = self.wait_for(
            "StartGame opens the world map",
            lambda probe: _map_name(probe) == "L_Main" and _screen_contains(probe, "WorldMap"),
            timeout=10.0,
            interval=0.5,
        )
        if bool(_save_state(after_world_map).get("exists")):
            raise RuntimeError(f"Start click should not create a save before manual Save; probe={json.dumps(after_world_map, ensure_ascii=False)}")
        world_map_probe = {
            "ok": (
                _screen_contains(after_world_map, "WorldMap")
                and _has_visible_command(after_world_map, "SelectQingshan", True)
                and not _widget_visible(after_world_map, "main_menu")
            ),
            "widgets": _flow_widgets(after_world_map),
            "visible_commands": _visible_commands(after_world_map),
        }
        self.event("world_map_widget_probe", **world_map_probe)
        if not world_map_probe["ok"]:
            raise RuntimeError(f"Start/New Game did not expose the playable Qingshan marker on the world map: {world_map_probe}")

        self.click_world_map_qingshan()
        after_qingshan = self.wait_for(
            "Qingshan world-map marker opens the town map",
            lambda probe: QINGSHAN_MAP_TOKEN in _map_name(probe) and _screen_contains(probe, "Town"),
            timeout=10.0,
            interval=0.5,
        )
        town_widget_probe = {
            "ok": _widget_visible(after_qingshan, "town_overlay") and not _widget_visible(after_qingshan, "main_menu"),
            "widgets": _flow_widgets(after_qingshan),
        }
        self.event("town_widget_probe", **town_widget_probe)
        if not town_widget_probe["ok"]:
            raise RuntimeError(f"PlayerController town UI was not visible after Start OpenLevel: {town_widget_probe}")
        camera_state = _topdown_camera_state(after_qingshan)
        self.event("topdown_camera_probe", **camera_state)
        if not camera_state.get("ok"):
            raise RuntimeError(f"BP_HeroCharacter top-down camera is not active/configured: {camera_state}")
        initial_idle_state = _expect_visual_state(after_qingshan, "Idle", "South")
        self.event("initial_idle_probe", **initial_idle_state)
        if not initial_idle_state.get("ok"):
            raise RuntimeError(f"BP_HeroCharacter did not enter Idle_South after town load: {initial_idle_state}")
        npc_visual_state = _npc_visual_state(after_qingshan)
        self.event("npc_visual_probe", **npc_visual_state)
        if not npc_visual_state.get("ok"):
            raise RuntimeError(f"Town NPC visual blueprints are not attached or idle: {npc_visual_state}")
        after_qingshan_path, _ = self.screenshot("real_flow_after_qingshan.png")

        before_move = _pawn_location(after_qingshan)
        with self.hold_town_keys("D"):
            time.sleep(0.25)
            while_d_down = self.probe()
            walk_state = _expect_visual_state(while_d_down, "Walk", "East")
            self.event("walk_state_probe", **walk_state)
            if not walk_state.get("ok"):
                raise RuntimeError(f"Town horizontal key did not switch hero to Walk_East while held: {walk_state}")
            time.sleep(0.5)
        time.sleep(0.25)
        after_move = self.probe()
        released_idle_state = _expect_visual_state(after_move, "Idle", "East")
        self.event("released_idle_probe", **released_idle_state)
        if not released_idle_state.get("ok"):
            raise RuntimeError(f"Town key release did not switch hero back to Idle_East: {released_idle_state}")
        after_move = self.run_serialized_mcp_d_to_w_release()
        diagonal_released_idle_state = _expect_visual_state(after_move, "Idle", "North")
        self.event(
            "diagonal_released_idle_probe",
            release_context="serialized MCP D->W release",
            expected_idle="Idle_North",
            **diagonal_released_idle_state,
        )
        if not diagonal_released_idle_state.get("ok"):
            raise RuntimeError(
                f"Serialized MCP D->W release did not switch hero back to Idle_North: {diagonal_released_idle_state}"
            )
        with self.hold_town_keys("D"):
            with self.hold_town_keys("W"):
                time.sleep(0.25)
                delayed_diagonal_down = self.probe()
                delayed_diagonal_walk_state = _expect_visual_state(delayed_diagonal_down, "Walk", "NorthEast")
                self.event("delayed_diagonal_walk_state_probe", **delayed_diagonal_walk_state)
                if not delayed_diagonal_walk_state.get("ok"):
                    raise RuntimeError(f"Town diagonal keys did not switch hero to Walk_NorthEast before delayed release: {delayed_diagonal_walk_state}")
            time.sleep(0.08)
        time.sleep(0.25)
        after_move = self.probe()
        delayed_diagonal_released_idle_state = _expect_visual_state(after_move, "Idle", "East")
        self.event("delayed_diagonal_released_idle_probe", **delayed_diagonal_released_idle_state)
        if not delayed_diagonal_released_idle_state.get("ok"):
            raise RuntimeError(f"Town vertical then delayed horizontal-key release should switch hero back to Idle_East: {delayed_diagonal_released_idle_state}")
        movement_distance = _distance(before_move, _pawn_location(after_move))
        self.event("movement_probe", distance=movement_distance)
        if movement_distance < 10.0:
            raise RuntimeError(f"D key did not move the hero enough; distance={movement_distance:.2f} cm")

        quest_npc = _quest_npc(after_move)
        quest_location = quest_npc.get("location") if isinstance(quest_npc.get("location"), dict) else {}
        if not quest_location:
            raise RuntimeError("Quest NPC was not found in the playable town map")
        near_quest = self.walk_to_world_location(after_move, quest_location)
        offer_probe: dict[str, Any] = {}
        interact_probe: dict[str, Any] = near_quest
        for attempt in range(3):
            self.town_interact()
            self.event("town_interact_attempt", target="quest_npc", attempt=attempt + 1)
            deadline = time.monotonic() + 2.0
            while time.monotonic() < deadline:
                interact_probe = self.probe()
                if _task_offer_open(interact_probe) and not _quest_interacted(interact_probe):
                    offer_probe = interact_probe
                    self.event("wait_ok", label="Town interact opens the new task offer without accepting the quest", attempt=attempt + 1, offer_kind="task_offer")
                    break
                time.sleep(0.35)
            if offer_probe:
                break
        if not offer_probe:
            raise RuntimeError(f"Timed out waiting for town interact to open the new task offer without accepting; last probe={json.dumps(interact_probe, ensure_ascii=False)}")
        quest_offer_probe = {
            "ok": _task_offer_open(offer_probe) and not _quest_interacted(offer_probe),
            "kind": "task_offer",
            "widgets": _flow_widgets(offer_probe),
            "quest_npc": _quest_npc(offer_probe),
        }
        self.event("quest_offer_probe", **quest_offer_probe)
        if not quest_offer_probe["ok"]:
            raise RuntimeError(f"Quest offer did not hold the confirmation state: {quest_offer_probe}")
        quest_offer_path, _ = self.screenshot("real_flow_quest_offer.png")

        after_interact = self.click_quest_offer_accept()
        after_interact = self.wait_for(
            "quest offer accept button confirms quest and activates follower",
            lambda probe: _quest_interacted(probe) and _task_offer_open(probe),
            timeout=5.0,
            interval=0.35,
        )

        # The confirmed task-offer UX deliberately remains open after accepting.
        # Close it through its authored visible back-arrow rather than OS Escape:
        # Escape is intercepted by the PIE host/editor before the game can receive it.
        after_interact = self.click_task_panel_back()
        after_interact = self.wait_for(
            "task-panel back button closes the accepted offer and releases movement",
            lambda probe: _quest_interacted(probe) and not _task_offer_open(probe) and not _move_input_is_ignored(probe),
            timeout=5.0,
            interval=0.35,
        )
        task_offer_close_probe = {
            "ok": (
                _quest_interacted(after_interact)
                and not _task_offer_open(after_interact)
                and not _move_input_is_ignored(after_interact)
            ),
            "widgets": _flow_widgets(after_interact),
            "move_input_ignored": _move_input_is_ignored(after_interact),
        }
        self.event("quest_offer_close_probe", **task_offer_close_probe)
        if not task_offer_close_probe["ok"]:
            raise RuntimeError(f"Accepted task offer did not close before movement verification: {task_offer_close_probe}")

        quest_after_interact = _quest_npc(after_interact)
        quest_location_after_interact = quest_after_interact.get("location") if isinstance(quest_after_interact.get("location"), dict) else {}
        if not quest_location_after_interact:
            raise RuntimeError("Quest NPC location missing after accepting quest")
        with self.hold_town_keys("D"):
            time.sleep(0.75)
            after_follower_input_move = self.probe()
        time.sleep(0.15)
        quest_after_follower_input_move = _quest_npc(after_follower_input_move)
        quest_location_after_follower_input_move = (
            quest_after_follower_input_move.get("location")
            if isinstance(quest_after_follower_input_move.get("location"), dict)
            else {}
        )
        quest_body_after_follower_input_move = quest_after_follower_input_move.get("body_character", {})
        if not isinstance(quest_body_after_follower_input_move, dict):
            quest_body_after_follower_input_move = {}
        quest_body_flipbook = str(quest_body_after_follower_input_move.get("current_flipbook", ""))
        quest_body_moving = bool(quest_body_after_follower_input_move.get("is_town_moving"))
        save_after_follower_input_move = _save_state(after_follower_input_move)
        quest_follower_input_distance = _distance(quest_location_after_interact, quest_location_after_follower_input_move)
        quest_follower_input_probe = {
            "ok": (
                quest_follower_input_distance >= 10.0
                and quest_body_moving
                and "/FB_Npc_Walk_East." in quest_body_flipbook
                and not bool(save_after_follower_input_move.get("exists"))
            ),
            "distance": quest_follower_input_distance,
            "before": quest_location_after_interact,
            "after": quest_location_after_follower_input_move,
            "body_is_town_moving": quest_body_moving,
            "body_current_flipbook": quest_body_flipbook,
            "save_exists_before_manual_save": save_after_follower_input_move.get("exists"),
        }
        self.event("quest_follower_range_chase_probe", **quest_follower_input_probe)
        if not quest_follower_input_probe["ok"]:
            raise RuntimeError(f"Quest follower did not chase with walk animation or saved before manual save: {quest_follower_input_probe}")

        after_manual_save = self.town_command("SaveSlot1")
        quest_after_manual_save = _quest_npc(after_manual_save)
        quest_location_after_manual_save = (
            quest_after_manual_save.get("location")
            if isinstance(quest_after_manual_save.get("location"), dict)
            else {}
        )
        save_after_manual = _save_state(after_manual_save)
        manual_saved_location = save_after_manual.get("quest_npc_location", {})
        if not isinstance(manual_saved_location, dict):
            manual_saved_location = {}
        manual_saved_distance = _distance(manual_saved_location, quest_location_after_manual_save)
        player_location_after_manual_save = _pawn_location(after_manual_save)
        manual_saved_player_location = save_after_manual.get("player_location", {})
        if not isinstance(manual_saved_player_location, dict):
            manual_saved_player_location = {}
        manual_saved_player_distance = _distance(manual_saved_player_location, player_location_after_manual_save)
        manual_save_probe = {
            "ok": (
                bool(save_after_manual.get("exists"))
                and bool(save_after_manual.get("b_has_player_location"))
                and bool(save_after_manual.get("b_follower_joined"))
                and bool(save_after_manual.get("b_has_quest_npc_location"))
                and manual_saved_player_distance <= 5.0
                and manual_saved_distance <= 5.0
            ),
            "saved_has_player_location": save_after_manual.get("b_has_player_location"),
            "saved_player_location": manual_saved_player_location,
            "player_location": player_location_after_manual_save,
            "saved_player_location_distance": manual_saved_player_distance,
            "saved_follower_joined": save_after_manual.get("b_follower_joined"),
            "saved_has_quest_npc_location": save_after_manual.get("b_has_quest_npc_location"),
            "saved_quest_npc_location": manual_saved_location,
            "quest_npc_location": quest_location_after_manual_save,
            "saved_quest_npc_location_distance": manual_saved_distance,
        }
        self.event("manual_save_probe", **manual_save_probe)
        if not manual_save_probe["ok"]:
            raise RuntimeError(f"Manual SaveGame did not persist player, follower state, and moved NPC location: {manual_save_probe}")

        town_exit = _town_exit(after_manual_save)
        town_exit_location = town_exit.get("location") if isinstance(town_exit.get("location"), dict) else {}
        if not town_exit_location:
            raise RuntimeError(f"Town route entrance QingshanInn_TownExit was not present in the real town actors: {_actors(after_manual_save)}")
        town_exit_approach_location = dict(town_exit_location)
        town_exit_approach_location["y"] = float(town_exit_approach_location.get("y", 0.0) or 0.0) - 320.0
        near_town_exit = self.walk_to_world_location(after_manual_save, town_exit_approach_location)
        self.event(
            "town_exit_walk_probe",
            town_exit=town_exit,
            approach_location=town_exit_approach_location,
            distance_to_approach=_distance(_pawn_location(near_town_exit), town_exit_approach_location),
        )

        after_route_map: dict[str, Any] = {}
        town_exit_interact_probe: dict[str, Any] = near_town_exit
        for attempt in range(4):
            self.town_interact()
            self.event("town_interact_attempt", target="town_exit", attempt=attempt + 1)
            time.sleep(0.35)
            town_exit_interact_probe = self.probe()
            if _screen_contains(town_exit_interact_probe, "DungeonMap") and ROUTE_MAP_TOKEN in _map_name(town_exit_interact_probe):
                after_route_map = town_exit_interact_probe
                self.event("wait_ok", label="Town interact enters route map through QingshanInn_TownExit", attempt=attempt + 1)
                break
        if not after_route_map:
            raise RuntimeError(f"Timed out waiting for town interact to enter the Slay-the-Spire route map through QingshanInn_TownExit; last probe={json.dumps(town_exit_interact_probe, ensure_ascii=False)}")
        route_runtime = _runtime_state(after_route_map)
        route_node_states = _route_node_visual_states(after_route_map)
        route_start_state = _route_node_visual_state(after_route_map, 0)
        route_map_probe = {
            "ok": (
                _screen_contains(after_route_map, "DungeonMap")
                and ROUTE_MAP_TOKEN in _map_name(after_route_map)
                and bool(route_runtime.get("b_dungeon_active"))
                and _widget_visible(after_route_map, "route_map")
                and bool(route_node_states)
                and bool(route_start_state.get("b_enabled"))
                and isinstance(route_start_state.get("viewport_hit_box_center"), dict)
            ),
            "map": _map_name(after_route_map),
            "screen": route_runtime.get("screen"),
            "b_dungeon_active": route_runtime.get("b_dungeon_active"),
            "dungeon_node_index": route_runtime.get("dungeon_node_index"),
            "visible_commands": _visible_commands(after_route_map),
            "widgets": _flow_widgets(after_route_map),
            "route_node_visual_states": route_node_states,
            "town_exit": town_exit,
            "town_exit_approach_location": town_exit_approach_location,
            "town_exit_approach_distance": _distance(_pawn_location(near_town_exit), town_exit_approach_location),
        }
        self.event("route_map_probe", **route_map_probe)
        if not route_map_probe["ok"]:
            raise RuntimeError(f"Town exit interact did not open the Slay-the-Spire route map screen: {route_map_probe}")
        after_route_map_path, _ = self.screenshot("real_flow_after_route_map.png")

        after_start_node = self.click_route_node(after_route_map, 0)
        start_node_runtime = _runtime_state(after_start_node)
        start_node_probe = {
            "ok": _screen_contains(after_start_node, "DungeonMap") and int(start_node_runtime.get("dungeon_node_index") or 0) >= 1,
            "screen": start_node_runtime.get("screen"),
            "dungeon_node_index": start_node_runtime.get("dungeon_node_index"),
        }
        self.event("route_start_node_probe", **start_node_probe)
        if not start_node_probe["ok"]:
            raise RuntimeError(f"Route start node did not advance route map: {start_node_probe}")

        battle_click_probe = self.click_route_node(after_start_node, 1)
        after_battle: dict[str, Any] = battle_click_probe
        for attempt in range(8):
            if BATTLE_MAP_TOKEN in _map_name(after_battle):
                self.event("wait_ok", label="Battle route node opens GameXXK battle scene", attempt=attempt + 1)
                break
            time.sleep(0.35)
            after_battle = self.probe()
        battle_runtime = _runtime_state(after_battle)
        active_widgets_probe = _load_json_from_probe(self.client.run_project_python_file(ACTIVE_WIDGETS_PROBE_SCRIPT))
        active_player_controller = active_widgets_probe.get("player_controller", {})
        if not isinstance(active_player_controller, dict):
            active_player_controller = {}
        battle_scene_counts = _battle_scene_counts(after_battle)
        battle_camera = _battle_camera_state(after_battle)
        battle_preconditions_ok = (
            BATTLE_MAP_TOKEN in _map_name(after_battle)
            and _screen_contains(after_battle, "Battle")
            and BATTLE_PC_TOKEN in str(active_player_controller.get("class_name", ""))
            and battle_scene_counts["enemies"] >= 1
            and battle_scene_counts["party"] >= 1
            and battle_scene_counts["visual_units"] == battle_scene_counts["units"]
            and bool(battle_camera.get("ok"))
        )
        battle_viewport: dict[str, float] = {}
        battle_hud = {
            "ok": False,
            "reason": "battle_preconditions_missing",
            "unit_count": 0,
            "errors": {},
        }
        battle_fixture: dict[str, Any] = {"ok": False, "reason": "battle_preconditions_missing"}
        battle_hud_capture: dict[str, Any] = {}
        if battle_preconditions_ok:
            try:
                fixture_apply = self.apply_battle_hud_fixture()
                battle_fixture = fixture_apply.get("battle_hud_fixture", {})
                if not isinstance(battle_fixture, dict):
                    battle_fixture = {"ok": False, "reason": "fixture_reply_invalid"}
                after_battle = self.probe()
                battle_runtime = _runtime_state(after_battle)
                battle_scene_counts = _battle_scene_counts(after_battle)
                battle_camera = _battle_camera_state(after_battle)
                battle_observation = _battle_hud_observation(after_battle)
                observed_viewport = battle_observation.get("viewport")
                if isinstance(observed_viewport, dict):
                    battle_viewport = dict(observed_viewport)
                battle_hud = battle_observation.get("battle_hud_verdict", {})
                if not isinstance(battle_hud, dict):
                    battle_hud = {
                        "ok": False,
                        "reason": "battle_hud_verdict_missing",
                        "unit_count": 0,
                        "errors": {"__harness__": ["battle_hud_verdict_missing"]},
                    }

                after_battle_path, battle_screenshot_size = self.screenshot("real_flow_after_battle.png")
                screenshot_context = self.screenshot_context_for(after_battle_path)
                battle_hud_capture = {
                    "full": str(after_battle_path),
                    "image_size": list(battle_screenshot_size),
                    "viewport": battle_viewport,
                    "screenshot_context": screenshot_context,
                }
                if _should_capture_battle_hud_crops(battle_hud):
                    try:
                        capture = _capture_battle_hud_crops(
                            after_battle_path,
                            battle_screenshot_size,
                            battle_viewport,
                            after_battle,
                            screenshot_context,
                        )
                        battle_hud_capture["crops"] = capture["crops"]
                        battle_hud_capture["transform"] = capture["transform"]
                    except RuntimeError as crop_error:
                        battle_hud_capture["crop_error"] = str(crop_error)
                        battle_hud = dict(battle_hud)
                        battle_hud_errors = battle_hud.get("errors", {})
                        if not isinstance(battle_hud_errors, dict):
                            battle_hud_errors = {}
                        screenshot_errors = list(battle_hud_errors.get("__screenshot__", []))
                        screenshot_errors.append(str(crop_error).split(":", 1)[0])
                        battle_hud_errors["__screenshot__"] = screenshot_errors
                        battle_hud["errors"] = battle_hud_errors
                        battle_hud["ok"] = False
                        battle_hud["reason"] = "battle_hud_invalid"
                else:
                    battle_hud_capture["crop_skipped"] = "ue58_geometry_api_unavailable_fixed_slot_evidence"
            finally:
                if self.battle_hud_fixture_may_be_applied:
                    self.clear_battle_hud_fixture()
        battle_probe = {
            "ok": battle_preconditions_ok and bool(battle_hud.get("ok")),
            "map": _map_name(after_battle),
            "screen": battle_runtime.get("screen"),
            "battle_scene_counts": battle_scene_counts,
            "battle_camera": battle_camera,
            "battle_hud": battle_hud,
            "battle_viewport": battle_viewport,
            "battle_fixture": battle_fixture,
            "battle_hud_capture": battle_hud_capture,
            "widgets": _flow_widgets(after_battle),
            "active_player_controller": active_player_controller,
            "onegame_route_widgets": active_widgets_probe.get("onegame_route_widgets", []),
            "click_probe_map": _map_name(battle_click_probe),
        }
        self.event("battle_scene_probe", **battle_probe)
        if not battle_probe["ok"]:
            raise RuntimeError(f"Route battle node did not open the GameXXK battle scene: {battle_probe}")

        result = {
            "ok": True,
            "screenshots": {
                "before_start": str(before_start_path),
                "after_qingshan": str(after_qingshan_path),
                "quest_dialog": str(quest_offer_path),
                "after_route_map": str(after_route_map_path),
                "after_battle": str(after_battle_path),
                "battle_hud": battle_hud_capture,
            },
            "topdown_camera": camera_state,
            "npc_visuals": npc_visual_state,
            "movement_distance_cm": movement_distance,
            "quest_distance_cm": _distance(_pawn_location(near_quest), quest_location),
            "quest_follower_input_distance_cm": quest_follower_input_distance,
            "manual_save": manual_save_probe,
            "route_map": route_map_probe,
            "battle": battle_probe,
            "final_probe": after_battle,
            "events": self.events,
        }
        return result

    def close(self) -> None:
        if getattr(self, "battle_hud_fixture_may_be_applied", False):
            try:
                if self.client.session_id and self.client.is_in_pie():
                    self.clear_battle_hud_fixture()
            except Exception as exc:
                self.event("battle_hud_fixture_cleanup_failed", error=str(exc))
        if self.keep_pie:
            return
        try:
            if self.client.session_id and self.client.is_in_pie():
                self.client.stop_pie()
                self.event("stopped_pie")
        except Exception as exc:
            self.event("stop_pie_failed", error=str(exc))
        try:
            if self.client.session_id:
                cleanup_result = self.client.run_project_python_file(PROBE_SCRIPT, ["--delete-default-save"])
                cleanup_payload = _load_json_from_probe(cleanup_result)
                self.event("deleted_default_save_after_real_flow", result=cleanup_payload.get("delete_default_save"))
        except Exception as exc:
            self.event("delete_default_save_after_real_flow_failed", error=str(exc))


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--timeout", type=float, default=30.0)
    parser.add_argument("--keep-pie", action="store_true")
    parser.add_argument("--battle-hud-observation", action="store_true")
    parser.add_argument("--report", type=Path, default=None)
    args = parser.parse_args(argv)

    harness = RealFlowHarness(timeout=args.timeout, keep_pie=args.keep_pie or args.battle_hud_observation)
    try:
        if args.battle_hud_observation:
            result = harness.observe_battle_actor_hud()
            return_code = 0 if bool(result.get("ok")) else 1
        else:
            result = harness.run()
            return_code = 0
    except Exception as exc:
        result = {"ok": False, "error": str(exc), "events": harness.events}
        print(json.dumps(result, ensure_ascii=False, indent=2), flush=True)
        return_code = 1
    else:
        print(json.dumps(result, ensure_ascii=False, indent=2), flush=True)
    finally:
        harness.close()

    REPORT_DIR.mkdir(parents=True, exist_ok=True)
    report_path = args.report or REPORT_DIR / f"gamexxk-real-play-flow-{time.strftime('%Y%m%d-%H%M%S')}.json"
    report_path.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps({"report": str(report_path), "ok": bool(result.get("ok"))}, ensure_ascii=False), flush=True)
    return return_code


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
