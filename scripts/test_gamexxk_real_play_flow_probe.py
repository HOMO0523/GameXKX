#!/usr/bin/env python3
"""Contract coverage for the real-PIE interaction preparation probe."""

from __future__ import annotations

from contextlib import redirect_stdout
import importlib.util
from io import StringIO
import json
import sys
import types
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
PROBE_PATH = PROJECT_ROOT / "Content" / "Python" / "gamexxk_probe_real_play_flow.py"
FIXTURE_ENTRYPOINT_PATH = PROJECT_ROOT / "Content" / "Python" / "gamexxk_apply_battle_hud_fixture.py"
META_SHOP_PROBE_PATH = PROJECT_ROOT / "Content" / "Python" / "gamexxk_probe_meta_shop_window.py"
REAL_FLOW_HARNESS_PATH = PROJECT_ROOT / "scripts" / "gamexxk_real_play_flow_mcp.py"


def _load_probe_module():
    module_name = "_gamexxk_real_play_flow_probe_test"
    original_unreal = sys.modules.get("unreal")
    sys.modules["unreal"] = types.ModuleType("unreal")
    try:
        spec = importlib.util.spec_from_file_location(module_name, PROBE_PATH)
        if spec is None or spec.loader is None:
            raise RuntimeError("Cannot load real play-flow probe for its pure helpers")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module
    finally:
        if original_unreal is None:
            sys.modules.pop("unreal", None)
        else:
            sys.modules["unreal"] = original_unreal


class _ViewportController:
    def __init__(self, viewport_size=None, error: Exception | None = None) -> None:
        self.viewport_size = viewport_size
        self.error = error
        self.calls = 0

    def get_viewport_size(self):
        self.calls += 1
        if self.error is not None:
            raise self.error
        return self.viewport_size


class _GeometryWithThrowingLocalSize:
    def get_local_size(self):
        raise RuntimeError("cached local size unavailable")

    def get_absolute_position(self):
        return types.SimpleNamespace(x=12.0, y=34.0)

    def get_absolute_size(self):
        return types.SimpleNamespace(x=320.0, y=158.0)


class _WidgetWithThrowingGeometryRead:
    def get_cached_geometry(self):
        return _GeometryWithThrowingLocalSize()

    def is_visible(self):
        return True

    def get_visibility(self):
        return types.SimpleNamespace(name="Visible")


class RealPlayFlowProbeTest(unittest.TestCase):
    def test_real_flow_wires_the_new_meta_shop_acceptance_contract(self) -> None:
        self.assertTrue(META_SHOP_PROBE_PATH.is_file(), "focused meta-shop PIE probe must exist")
        probe_source = META_SHOP_PROBE_PATH.read_text(encoding="utf-8")
        harness_source = REAL_FLOW_HARNESS_PATH.read_text(encoding="utf-8")

        for field in (
            "merchant_interaction_opened_meta_shop",
            "product_count",
            "legacy_trade_visible",
            "equipment_purchase_gold_delta",
            "equipment_instance_delta",
            "acceptance_funding_delta",
            "old_buy_item_command_visible",
        ):
            self.assertIn(field, probe_source)
        self.assertNotIn("raise SystemExit", probe_source)
        self.assertIn("_get_mvp_subsystem_from_player_controller", probe_source)
        self.assertIn("META_SHOP_PROBE_SCRIPT", harness_source)
        self.assertIn('result["meta_shop"]', harness_source)
        self.assertIn("probe_meta_shop", harness_source)

    def test_battle_side_name_normalizes_ue_enum_spellings_to_the_locked_payload_values(self) -> None:
        module = _load_probe_module()

        self.assertEqual("Party", module._battle_side_name(types.SimpleNamespace(name="PARTY")))
        self.assertEqual("Enemy", module._battle_side_name(types.SimpleNamespace(name="ENEMY")))
        self.assertEqual("Party", module._battle_side_name(types.SimpleNamespace(name="Party")))
        self.assertEqual("Enemy", module._battle_side_name("EGameXXKCardTargetSide::Enemy"))
        self.assertEqual("Invalid", module._battle_side_name(types.SimpleNamespace(name="Invalid")))

    def test_fixture_result_treats_single_empty_out_string_as_success(self) -> None:
        module = _load_probe_module()

        self.assertEqual((True, ""), module._fixture_apply_result(""))
        self.assertEqual((True, "detail"), module._fixture_apply_result("detail"))
        self.assertEqual((False, ""), module._fixture_apply_result(None))
        self.assertEqual((True, ""), module._fixture_apply_result((True, "")))
        self.assertEqual((False, "rejected"), module._fixture_apply_result((False, "rejected")))

    def test_open_quest_offer_handler_uses_the_live_quest_actor(self) -> None:
        module = _load_probe_module()

        class _Pawn:
            def get_path_name(self): return "/Game/Test/Hero"

        class _QuestActor:
            def get_npc_role(self): return types.SimpleNamespace(name="QUEST")
            def get_path_name(self): return "/Game/Test/QuestNpc"

        class _Controller:
            def __init__(self): self.calls = []
            def open_task_offer_panel_for_npc(self, actor, pawn):
                self.calls.append((actor, pawn))
                return True

        pawn = _Pawn()
        actor = _QuestActor()
        controller = _Controller()
        module._first_player_controller = lambda world: controller
        module._first_player_pawn = lambda world: pawn
        module._all_actors = lambda world: [actor]

        self.assertEqual(
            {"ok": True, "npc": "/Game/Test/QuestNpc"},
            module._handle_open_quest_offer(object()),
        )
        self.assertEqual([(actor, pawn)], controller.calls)

    def test_accept_task_offer_handler_passes_the_requested_task_id(self) -> None:
        module = _load_probe_module()

        class _Controller:
            def __init__(self): self.task_ids = []
            def accept_task_offer_by_id(self, task_id):
                self.task_ids.append(task_id)
                return True

        controller = _Controller()
        module.unreal.Name = lambda value: f"Name:{value}"
        module._first_player_controller = lambda world: controller

        self.assertEqual(
            {"ok": True, "task_id": "Quest_01"},
            module._handle_accept_task_offer(object(), "Quest_01"),
        )
        self.assertEqual(["Name:Quest_01"], controller.task_ids)

    def test_town_key_handler_normalizes_and_persists_fake_pawn_input(self) -> None:
        module = _load_probe_module()

        class _Pawn:
            def __init__(self): self.inputs = []
            def get_path_name(self): return "/Game/Test/Hero"
            def set_town_automation_key_state(self, key, pressed):
                self.inputs.append((key, pressed))
                return True

        pawn = _Pawn()
        module.unreal.Name = lambda value: f"Name:{value}"
        module._first_player_pawn = lambda world: pawn

        self.assertEqual(
            {
                "ok": True,
                "key": "D",
                "state": "down",
                "pressed": True,
                "pawn": "/Game/Test/Hero",
            },
            module._handle_town_key(object(), "d", "DOWN"),
        )
        self.assertEqual([("Name:D", True)], pawn.inputs)
        self.assertEqual(
            {"ok": False, "key": "Q", "state": "down", "reason": "unsupported_town_key"},
            module._handle_town_key(object(), "q", "down"),
        )

    def test_town_interact_handler_invokes_the_fake_pawn(self) -> None:
        module = _load_probe_module()

        class _Pawn:
            def __init__(self): self.interactions = 0
            def get_path_name(self): return "/Game/Test/Hero"
            def interact(self): self.interactions += 1

        pawn = _Pawn()
        module._first_player_pawn = lambda world: pawn

        self.assertEqual(
            {"ok": True, "pawn": "/Game/Test/Hero"},
            module._handle_town_interact(object()),
        )
        self.assertEqual(1, pawn.interactions)

    def test_probe_observes_board_owned_unit_huds_and_rejects_actor_component_sources(self) -> None:
        source = PROBE_PATH.read_text(encoding="utf-8")
        resource_header = (
            PROJECT_ROOT / "Source" / "GameXXK" / "Public" / "UI" / "GameXXKBattleUnitResourceWidget.h"
        ).read_text(encoding="utf-8")

        self.assertIn("def _screen_rect(world, widget):", source)
        self.assertIn("unreal.SlateBlueprintLibrary.local_to_viewport", source)
        self.assertNotIn('"battle_hud"', source)
        self.assertNotIn("get_resource_hud_widget_component_for_test", source)
        self.assertNotIn("get_status_effects_widget_component_for_test", source)
        self.assertNotIn("should_show_mana_for_test", source)
        self.assertIn('"unit_hud_layer"', source)
        self.assertIn('"unit_huds"', source)
        self.assertIn("get_battle_projected_unit_hud_layer_for_test", source)
        self.assertIn("get_projected_unit_hud_for_test", source)
        self.assertIn("get_projected_unit_hud_anchor_position_for_test", source)
        self.assertIn("get_battle_unit_screen_position_for_test", source)
        self.assertIn('"projection"', source)
        self.assertIn('"received_anchor"', source)
        self.assertIn('"latest_anchor"', source)
        self.assertIn('"transient_anchor"', source)
        self.assertIn('"latest_applied_anchor"', source)
        self.assertIn('"applied_slot"', source)
        self.assertIn('"canvas"', source)
        self.assertIn('"root_geometry"', source)
        self.assertIn('"layer_geometry"', source)
        self.assertIn('"layer_z_order"', source)
        self.assertIn('"offsets"', source)
        self.assertIn('"z_order"', source)
        self.assertIn('"viewport_dpi_revision"', source)
        self.assertIn("get_resource_widget_for_test", source)
        self.assertIn("get_status_effects_widget_for_test", source)
        self.assertIn("get_unit_id_for_test", source)
        self.assertIn("get_side_for_test", source)
        self.assertIn("get_slot_number_for_test", source)
        self.assertIn("def _battle_side_name(value):", source)
        self.assertIn('result["side"] = _battle_side_name(widget.get_side_for_test())', source)
        self.assertIn('"screen_rect"', source)
        self.assertIn('"rendered"', source)
        self.assertIn('"mana_row_visible"', source)
        self.assertIn("get_health_display_text_for_test", source)
        self.assertIn("get_mana_display_text_for_test", source)
        self.assertIn("get_health_percent_for_test", source)
        self.assertIn("get_mana_percent_for_test", source)
        self.assertIn("get_icon_count_for_test", source)
        self.assertIn("get_icon_id_for_test", source)
        self.assertIn("get_icon_displayed_stack_for_test", source)
        self.assertNotIn('"should_show_qi_for_test"', source)
        self.assertIn("def _battle_board_summary(world, player_controller, subsystem):", source)
        self.assertIn('"battle_board": _battle_board_summary(world, player_controller, subsystem)', source)
        self.assertIn("get_party_qi_widget_for_test", source)
        self.assertIn("get_hand_card_box_for_test", source)
        self.assertIn("get_end_turn_button_for_test", source)
        self.assertIn("get_shared_qi_for_test", source)
        self.assertIn('"shared_energy"', source)

        actors_start = source.index("def _actors_summary(world):")
        actors_end = source.index("\ndef ", actors_start + len("def _actors_summary(world):"))
        self.assertNotIn('"shared_energy"', source[actors_start:actors_end])
        self.assertIn("UFUNCTION(BlueprintPure, Category = \"GameXXK|Battle|Test\", meta = (DevelopmentOnly))\n\tbool IsManaRowVisibleForTest() const;", resource_header)

    def test_probe_retains_geometry_diagnostics_when_cached_geometry_read_raises(self) -> None:
        module = _load_probe_module()

        summary = module._widget_screen_summary(object(), _WidgetWithThrowingGeometryRead())

        self.assertIsNone(summary["screen_rect"])
        self.assertEqual(
            {"x": 12.0, "y": 34.0},
            summary["geometry"]["absolute_position"],
        )
        self.assertEqual(
            {"x": 320.0, "y": 158.0},
            summary["geometry"]["absolute_size"],
        )
        self.assertIsNone(summary["geometry"]["local_size"])
        self.assertEqual(
            [
                {
                    "stage": "geometry.get_local_size",
                    "exception": "RuntimeError: cached local size unavailable",
                }
            ],
            summary["geometry"]["errors"],
        )
        self.assertIn("screen_rect_unavailable", summary["errors"])

    def test_battle_hud_fixture_handlers_refresh_the_fake_controller(self) -> None:
        module = _load_probe_module()

        class _Controller:
            def __init__(self): self.refreshes = 0
            def get_path_name(self): return "/Game/Test/Controller"
            def refresh_player_flow_widgets_for_test(self): self.refreshes += 1

        class _Subsystem:
            def __init__(self): self.applies = 0; self.clears = 0
            def get_path_name(self): return "/Game/Test/Subsystem"
            def apply_battle_hud_fixture_for_test(self): self.applies += 1; return ""
            def clear_battle_hud_fixture_for_test(self): self.clears += 1

        controller = _Controller()
        subsystem = _Subsystem()
        module._first_player_controller = lambda world: controller
        module._get_mvp_subsystem = lambda world: subsystem
        module._get_mvp_subsystem_from_player_controller = lambda player_controller: None

        self.assertEqual(
            {
                "ok": True,
                "player_controller": "/Game/Test/Controller",
                "subsystem": "/Game/Test/Subsystem",
                "out_error": "",
            },
            module._handle_apply_battle_hud_fixture(object()),
        )
        self.assertEqual({"ok": True, "subsystem": "/Game/Test/Subsystem"}, module._handle_clear_battle_hud_fixture(object()))
        self.assertEqual(1, subsystem.applies)
        self.assertEqual(1, subsystem.clears)
        self.assertEqual(2, controller.refreshes)

    def test_fixture_entrypoint_is_a_small_explicit_project_python_command(self) -> None:
        source = FIXTURE_ENTRYPOINT_PATH.read_text(encoding="utf-8")

        self.assertIn('parser.add_argument("--clear", action="store_true")', source)
        self.assertIn("_handle_apply_battle_hud_fixture", source)
        self.assertIn("_handle_clear_battle_hud_fixture", source)
        self.assertIn('result["battle_hud_fixture"]', source)
        self.assertIn('result["battle_hud_fixture_clear"]', source)

    def test_probe_derives_pie_viewport_dimensions_from_a_nondefault_controller_result(self) -> None:
        module = _load_probe_module()
        controller = _ViewportController((319, 617))

        self.assertEqual(
            {"width": 319.0, "height": 617.0},
            module._viewport_dimensions((319, 617)),
        )
        self.assertEqual(
            {
                "width": 319.0,
                "height": 617.0,
                "source": "player_controller.get_viewport_size",
            },
            module._pie_viewport_summary(controller),
        )
        self.assertEqual(1, controller.calls)

    def test_probe_rejects_nonfinite_numeric_values_and_uses_strict_json(self) -> None:
        module = _load_probe_module()

        class _ResourceWidget:
            def get_health_display_text_for_test(self): return "气血 1 / 2"
            def get_mana_display_text_for_test(self): return "内力 1 / 2"
            def get_health_percent_for_test(self): return float("nan")
            def get_mana_percent_for_test(self): return float("inf")

        self.assertIsNone(module._viewport_dimensions((float("nan"), 617)))
        self.assertIsNone(module._viewport_dimensions((319, float("inf"))))
        self.assertIsNone(module._strict_vector2d_to_dict(types.SimpleNamespace(x=float("nan"), y=1.0)))
        self.assertEqual('{"ok": true}', module._strict_json_dumps({"ok": True}))
        self.assertEqual({"bad": None}, json.loads(module._strict_json_dumps({"bad": float("nan")})))
        result = {"errors": []}
        rendered = module._rendered_resource_summary(_ResourceWidget(), result)
        self.assertIsNone(rendered["health_percent"])
        self.assertIsNone(rendered["mana_percent"])
        self.assertEqual(["rendered_health_percent_invalid", "rendered_mana_percent_invalid"], result["errors"])

    def test_probe_rejects_infinite_runtime_shared_energy_without_raising(self) -> None:
        module = _load_probe_module()
        subsystem = types.SimpleNamespace(
            get_runtime_state_copy=lambda: types.SimpleNamespace(
                card_run=types.SimpleNamespace(
                    active_battle=types.SimpleNamespace(
                        deck=types.SimpleNamespace(shared_energy=float("inf"))
                    )
                )
            )
        )
        result = {"errors": []}

        self.assertIsNone(module._battle_board_shared_energy(subsystem, result))
        self.assertEqual(["shared_energy_invalid"], result["errors"])

    def test_probe_surfaces_viewport_read_failure_as_structured_diagnostics(self) -> None:
        module = _load_probe_module()
        controller = _ViewportController(error=RuntimeError("PIE transitioned"))
        diagnostics = []

        self.assertIsNone(module._pie_viewport_summary(controller, diagnostics))
        self.assertEqual(
            [
                {
                    "stage": "player_controller.get_viewport_size",
                    "exception": "RuntimeError: PIE transitioned",
                }
            ],
            diagnostics,
        )

        module._get_game_world = lambda: object()
        module._first_player_controller = lambda world: controller
        module._first_player_pawn = lambda world: None
        module._first_hud = lambda player_controller: None
        module._get_mvp_subsystem = lambda world: None
        module._get_mvp_subsystem_from_player_controller = lambda player_controller: None
        module._get_map_name = lambda world: "L_Test"
        module._runtime_state = lambda subsystem: {}
        module._save_state = lambda: {}
        module._player_controller_summary = lambda player_controller: {}
        module._hud_summary = lambda hud: {}
        module._hero_summary = lambda pawn: {}
        module._actors_summary = lambda world: []
        module._battle_board_summary = lambda world, player_controller, subsystem: {}

        report = module.probe()
        self.assertIsNone(report["pie_viewport"])
        self.assertEqual(diagnostics, report["pie_viewport_diagnostics"])

    def test_board_summary_keeps_legacy_error_and_adds_a_structured_diagnostic(self) -> None:
        module = _load_probe_module()

        class _Controller:
            def get_viewport_size(self): return (1280, 720)
            def get_battle_board_widget_for_test(self):
                raise RuntimeError("board rebuilding")

        module.unreal.SlateBlueprintLibrary = types.SimpleNamespace(get_viewport_scale=lambda world: 1.0)
        result = module._battle_board_summary(object(), _Controller(), None)

        self.assertEqual(["battle_board_unavailable:board rebuilding"], result["errors"])
        self.assertEqual(
            [
                {
                    "stage": "battle_board.get_battle_board_widget_for_test",
                    "exception": "RuntimeError: board rebuilding",
                }
            ],
            result["diagnostics"],
        )

    def test_unit_hud_resource_getter_failure_keeps_legacy_error_and_adds_a_structured_diagnostic(self) -> None:
        module = _load_probe_module()

        class _StatusWidget:
            def is_visible(self): return True
            def get_visibility(self): return types.SimpleNamespace(name="Visible")
            def get_cached_geometry(self): return None
            def get_icon_count_for_test(self): return 0

        class _UnitHud:
            def is_visible(self): return True
            def get_visibility(self): return types.SimpleNamespace(name="Visible")
            def get_cached_geometry(self): return None
            def get_unit_id_for_test(self): return "Party_01"
            def get_side_for_test(self): return types.SimpleNamespace(name="PARTY")
            def get_slot_number_for_test(self): return 1
            def get_resource_widget_for_test(self): raise RuntimeError("resource rebuilding")
            def get_status_effects_widget_for_test(self): return _StatusWidget()

        class _Board:
            def get_projected_unit_hud_for_test(self, unit_id): return _UnitHud()
            def has_battle_unit_screen_position_for_test(self, unit_id): return False
            def get_battle_unit_screen_position_for_test(self, unit_id): return None
            def has_projected_unit_hud_screen_position_for_test(self, unit_id): return False
            def get_projected_unit_hud_anchor_position_for_test(self, unit_id): return None

        module.unreal.Name = lambda value: value
        result = module._board_unit_hud_summary(None, _Board(), "Party_01")

        self.assertIn("resource_unavailable:resource rebuilding", result["errors"])
        self.assertIn(
            {
                "stage": "battle_unit_hud.get_resource_widget_for_test",
                "exception": "RuntimeError: resource rebuilding",
            },
            result.get("diagnostics", []),
        )

    def test_main_writes_strict_json_for_nonfinite_probe_values(self) -> None:
        module = _load_probe_module()
        module.probe = lambda: {"nan": float("nan"), "infinity": float("inf"), "nested": [float("-inf")]}
        stdout = StringIO()

        with redirect_stdout(stdout):
            module.main([])

        encoded = stdout.getvalue().strip()
        self.assertNotIn("NaN", encoded)
        self.assertNotIn("Infinity", encoded)
        self.assertEqual(
            {"probe": {"nan": None, "infinity": None, "nested": [None]}},
            json.loads(encoded),
        )

    def test_probe_has_no_actor_widget_component_summary_path(self) -> None:
        source = PROBE_PATH.read_text(encoding="utf-8")
        self.assertNotIn("def _widget_component_summary", source)
        self.assertNotIn("def _battle_hud_summary", source)
        actors_start = source.index("def _actors_summary(world):")
        actors_end = source.index("\ndef ", actors_start + len("def _actors_summary(world):"))
        self.assertNotIn('"battle_hud"', source[actors_start:actors_end])


if __name__ == "__main__":
    unittest.main()
