#!/usr/bin/env python3
"""Static/unit contract for the PIE-only PartyDeck acceptance extension.

The acceptance runner itself talks to UE only after a real PIE session already
exists.  These tests deliberately exercise the no-engine pure predicates so a
future gameplay/UI change cannot silently weaken the evidence it collects.
"""

from __future__ import annotations

import importlib.util
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
RUNNER_PATH = PROJECT_ROOT / "scripts" / "gamexxk_party_deck_play_acceptance.py"
PROBE_PATH = PROJECT_ROOT / "Content" / "Python" / "gamexxk_probe_party_deck_runtime.py"


def _load_runner():
    spec = importlib.util.spec_from_file_location("gamexxk_party_deck_play_acceptance", RUNNER_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Unable to load PartyDeck acceptance runner: {RUNNER_PATH}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class PartyDeckRealPlayAcceptanceTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.runner = _load_runner()

    def test_runtime_probe_is_isolated_from_assets_and_maps(self) -> None:
        self.assertTrue(PROBE_PATH.is_file())
        source = PROBE_PATH.read_text(encoding="utf-8")
        self.assertIn("PARTY_DECK_RUNTIME_PROBE_VERSION", source)
        self.assertIn("def _card_run_summary", source)
        self.assertIn("def _battle_board_summary", source)
        self.assertIn("def _route_encounter_summary", source)
        self.assertIn('"open-route-encounter-panel"', source)
        self.assertIn('"trigger-route-encounter-primary"', source)
        self.assertIn('"Npc.Event.NiuHuan"', source)
        self.assertIn('"pawn_interaction_binding"', source)
        self.assertIn('get_interaction_component', source)
        self.assertIn('get_focused_actor', source)
        self.assertIn('get_route_encounter_source_actor_for_test', source)
        self.assertIn('"is_route_encounter_panel_open_for_test"', source)
        self.assertNotIn('"open_route_encounter_panel"', source)
        self.assertNotIn('"route_scene_actor_interact"', source)
        self.assertNotIn('_call(actor, "interact", pawn)', source)
        self.assertIn("def _party_visual_summary", source)
        self.assertNotIn("apply_default_interaction", source)
        self.assertNotIn("EditorAssetLibrary.save", source)
        self.assertNotIn("load_level", source)
        self.assertNotIn("save_current_game", source)

    def test_runtime_probe_reads_card_targeting_evidence_from_public_test_getters(self) -> None:
        source = PROBE_PATH.read_text(encoding="utf-8")
        self.assertIn('"get_pending_card_instance_id_for_test"', source)
        self.assertIn('"get_targeting_source_position_for_test"', source)
        self.assertIn('"get_targeting_pointer_position_for_test"', source)
        self.assertNotIn('preview = _prop(board, "pending_card_preview", "PendingCardPreview")', source)

    def test_runtime_probe_cancels_targeting_through_the_controller_bridge(self) -> None:
        source = PROBE_PATH.read_text(encoding="utf-8")
        self.assertIn('"cancel_battle_targeting_for_test"', source)
        self.assertNotIn('bool(_call(board, "cancel_battle_targeting"))', source)

    def test_manual_target_card_selection_uses_stable_instance_and_energy(self) -> None:
        hand = [
            {"instance_id": "Hero.QingFengYiShi#7", "card_id": "Hero.QingFengYiShi", "owner_unit_id": "Hero"},
            {"instance_id": "Hero.FengShenBu#8", "card_id": "Hero.FengShenBu", "owner_unit_id": "Hero"},
        ]
        catalog = {
            "Hero.QingFengYiShi": {"target_mode": "SingleEnemy", "energy_cost": 1},
            "Hero.FengShenBu": {"target_mode": "Self", "energy_cost": 1},
        }
        selection = self.runner.select_manual_target_card(hand, catalog, shared_energy=1)
        self.assertEqual(selection["instance_id"], "Hero.QingFengYiShi#7")
        self.assertEqual(selection["target_mode"], "SingleEnemy")

    def test_static_catalog_reader_covers_the_approved_card_pool(self) -> None:
        catalog = self.runner.load_card_target_catalog()
        self.assertGreaterEqual(len(catalog), 174)
        self.assertEqual(catalog["Hero.QingFengYiShi"]["target_mode"], "SingleEnemy")
        self.assertEqual(catalog["Hero.QingFengYiShi"]["energy_cost"], 1)

    def test_targeting_evidence_requires_live_board_and_scene_highlight(self) -> None:
        snapshot = {
            "battle_board": {
                "is_in_viewport": True,
                "is_card_targeting_active": True,
                "pending_card_instance_id": "Hero.QingFengYiShi#7",
                "targeting_source_position": {"x": 12, "y": 30},
                "targeting_pointer_position": {"x": 520, "y": 360},
                "legal_target_unit_ids": ["Enemy.MoneyRat.0"],
            },
            "battle_units": [
                {"unit_id": "Enemy.MoneyRat.0", "is_enemy_unit": True, "is_card_target_highlighted": True, "is_card_target_outline_enabled": True},
                {"unit_id": "Hero", "is_enemy_unit": False, "is_card_target_highlighted": False, "is_card_target_outline_enabled": False},
            ],
        }
        verdict = self.runner.evaluate_targeting_state(snapshot, "Hero.QingFengYiShi#7", "Enemy.MoneyRat.0")
        self.assertTrue(verdict["ok"])
        self.assertFalse(verdict["owner_highlighted"])

    def test_targeting_evidence_reads_owner_from_the_real_nested_active_battle_hand(self) -> None:
        snapshot = {
            "card_run": {
                "active_battle": {
                    "hand": [{"instance_id": "Hero.QingFengYiShi#7", "owner_unit_id": "Hero"}],
                }
            },
            "battle_board": {
                "is_in_viewport": True,
                "is_card_targeting_active": True,
                "pending_card_instance_id": "Hero.QingFengYiShi#7",
                "targeting_source_position": {"x": 12, "y": 30},
                "targeting_pointer_position": {"x": 520, "y": 360},
                "legal_target_unit_ids": ["Enemy.MoneyRat.0"],
            },
            "battle_units": [
                {"unit_id": "Enemy.MoneyRat.0", "is_enemy_unit": True, "is_card_target_highlighted": True, "is_card_target_outline_enabled": True},
                {"unit_id": "Hero", "is_enemy_unit": False, "is_card_target_highlighted": False, "is_card_target_outline_enabled": False},
            ],
        }
        verdict = self.runner.evaluate_targeting_state(snapshot, "Hero.QingFengYiShi#7", "Enemy.MoneyRat.0")
        self.assertTrue(verdict["ok"])
        self.assertEqual(verdict["owner_unit_id"], "Hero")

    def test_targeting_evidence_allows_a_highlighted_owner_when_the_card_targets_allies(self) -> None:
        snapshot = {
            "card_run": {
                "active_battle": {
                    "hand": [{"instance_id": "Companion.Guard.SelfGuard#3", "owner_unit_id": "Companion"}],
                }
            },
            "battle_board": {
                "is_in_viewport": True,
                "is_card_targeting_active": True,
                "pending_card_instance_id": "Companion.Guard.SelfGuard#3",
                "targeting_source_position": {"x": 900, "y": 410},
                "targeting_pointer_position": {"x": 900, "y": 410},
                "legal_target_unit_ids": ["Companion", "Hero"],
            },
            "battle_units": [
                {"unit_id": "Companion", "is_enemy_unit": False, "is_card_target_highlighted": True, "is_card_target_outline_enabled": True},
                {"unit_id": "Hero", "is_enemy_unit": False, "is_card_target_highlighted": True, "is_card_target_outline_enabled": True},
            ],
        }
        verdict = self.runner.evaluate_targeting_state(snapshot, "Companion.Guard.SelfGuard#3", "Companion")
        self.assertTrue(verdict["ok"])
        self.assertTrue(verdict["owner_highlighted"])

    def test_reward_evidence_requires_three_choices_and_replacement_at_cap(self) -> None:
        snapshot = {
            "card_run": {
                "route_card_ids": [f"Route.General.{index}" for index in range(12)],
                "pending_reward": {
                    "card_ids": ["Route.General.A", "Route.General.B", "Route.General.C"],
                    "requires_route_card_replacement": True,
                },
            },
            "battle_board": {
                "has_pending_route_reward": True,
                "pending_route_reward_card_ids": ["Route.General.A", "Route.General.B", "Route.General.C"],
                "route_reward_replacement_card_ids": [f"Route.General.{index}" for index in range(12)],
            },
        }
        verdict = self.runner.evaluate_reward_offer(snapshot, require_replacement=True)
        self.assertTrue(verdict["ok"])
        self.assertEqual(verdict["replacement_count"], 12)

    def test_event_open_requires_visible_panel_identity_and_no_automatic_resolution(self) -> None:
        npc_open = self.runner.evaluate_event_panel_open(
            {
                "runtime_state": {"screen": "RouteEvent", "player_gold": 50},
                "card_run": {
                    "pending_event": {"event_npc_id": "Npc.YueBai"},
                    "party_selection": {"active_permanent_companion_instance_id": "Partner.1", "quest_npc": {"npc_id": "", "selected_card_ids": []}},
                },
                "route_panel": {
                    "class": "GameXXKRouteEncounterPanelWidget",
                    "is_in_viewport": True,
                    "is_open": True,
                    "speaker": "月白",
                    "primary_action": "AcceptTaskNpcSupport",
                    "primary_label": "邀请月白支援",
                    "secondary_action": "TakeHealingPowder",
                    "source_actor_path": "/Game/PIE/RouteEncounter_A",
                },
                "action": {
                    "ok": True,
                    "kind": "open_route_encounter_panel_only",
                    "input_path": "pawn_interaction_binding",
                    "rejected_when_unfocused": True,
                    "actor": "/Game/PIE/RouteEncounter_A",
                    "focused_actor_path": "/Game/PIE/RouteEncounter_A",
                    "source_actor_path": "/Game/PIE/RouteEncounter_A",
                    "source_last_interaction_successful_before_choice": False,
                },
            },
            event_kind="task_npc",
            event_npc_id_before_open="Npc.YueBai",
            player_gold_before_open=50,
        )
        self.assertTrue(npc_open["ok"])
        self.assertTrue(npc_open["unchanged_before_choice"])

        niu_huan_open = self.runner.evaluate_event_panel_open(
            {
                "runtime_state": {"screen": "RouteEvent", "player_gold": 50},
                "card_run": {
                    "pending_event": {"event_npc_id": "Npc.Event.NiuHuan"},
                    "party_selection": {"active_permanent_companion_instance_id": "Partner.1", "quest_npc": {"npc_id": "None", "selected_card_ids": []}},
                },
                "route_panel": {
                    "class": "GameXXKRouteEncounterPanelWidget",
                    "is_in_viewport": True,
                    "is_open": True,
                    "speaker": "牛欢",
                    "primary_action": "TakeGold",
                    "primary_label": "收下 12 金",
                    "secondary_action": "TakeHealingPowder",
                    "source_actor_path": "/Game/PIE/RouteEncounter_A",
                },
                "action": {
                    "ok": True,
                    "kind": "open_route_encounter_panel_only",
                    "input_path": "pawn_interaction_binding",
                    "rejected_when_unfocused": True,
                    "actor": "/Game/PIE/RouteEncounter_A",
                    "focused_actor_path": "/Game/PIE/RouteEncounter_A",
                    "source_actor_path": "/Game/PIE/RouteEncounter_A",
                    "source_last_interaction_successful_before_choice": False,
                },
            },
            event_kind="niu_huan",
            event_npc_id_before_open="Npc.Event.NiuHuan",
            player_gold_before_open=50,
        )
        self.assertTrue(niu_huan_open["ok"])

        self.assertFalse(self.runner.evaluate_event_panel_open(
            {
                "runtime_state": {"screen": "RouteEvent", "player_gold": 50},
                "card_run": {"pending_event": {"event_npc_id": "Npc.YueBai"}, "party_selection": {"quest_npc": {"npc_id": "", "selected_card_ids": []}}},
                "route_panel": {"class": "SomeOtherPanel", "is_in_viewport": True, "is_open": True, "speaker": "月白", "primary_action": "AcceptTaskNpcSupport", "primary_label": "邀请月白支援", "secondary_action": "TakeHealingPowder", "source_actor_path": "/Game/PIE/RouteEncounter_A"},
                "action": {
                    "ok": True,
                    "kind": "open_route_encounter_panel_only",
                    "input_path": "pawn_interaction_binding",
                    "rejected_when_unfocused": True,
                    "actor": "/Game/PIE/RouteEncounter_A",
                    "focused_actor_path": "/Game/PIE/RouteEncounter_A",
                    "source_actor_path": "/Game/PIE/RouteEncounter_A",
                    "source_last_interaction_successful_before_choice": False,
                },
            },
            event_kind="task_npc",
            event_npc_id_before_open="Npc.YueBai",
            player_gold_before_open=50,
        )["ok"])

        self.assertFalse(self.runner.evaluate_event_panel_open(
            {
                "runtime_state": {"screen": "RouteEvent", "player_gold": 50},
                "card_run": {"pending_event": {"event_npc_id": "Npc.YueBai"}, "party_selection": {"quest_npc": {"npc_id": "", "selected_card_ids": []}}},
                "route_panel": {"class": "GameXXKRouteEncounterPanelWidget", "is_in_viewport": True, "is_open": True, "speaker": "月白", "primary_action": "AcceptTaskNpcSupport", "primary_label": "邀请月白支援", "secondary_action": "TakeHealingPowder", "source_actor_path": "/Game/PIE/RouteEncounter_A"},
                "action": {
                    "ok": True,
                    "kind": "open_route_encounter_panel_only",
                    "input_path": "pawn_interaction_binding",
                    "rejected_when_unfocused": True,
                    "actor": "/Game/PIE/RouteEncounter_A",
                    "focused_actor_path": "/Game/PIE/RouteEncounter_B",
                    "source_actor_path": "/Game/PIE/RouteEncounter_A",
                    "source_last_interaction_successful_before_choice": False,
                },
            },
            event_kind="task_npc",
            event_npc_id_before_open="Npc.YueBai",
            player_gold_before_open=50,
        )["ok"])

    def test_event_primary_action_resolves_only_after_the_visible_panel_choice(self) -> None:
        npc = self.runner.evaluate_event_resolution(
            {
                "action": {"ok": True, "kind": "trigger-route-encounter-primary", "selected_action": "AcceptTaskNpcSupport", "source_actor_path_before_choice": "/Game/PIE/RouteEncounter_A", "source_last_interaction_successful_after_choice": True},
                "runtime_state": {"screen": "DungeonMap", "player_gold": 50},
                "route_panel": {"is_open": False, "source_actor_path": ""},
                "card_run": {
                    "active_temporary_quest_npc_id": "Npc.YueBai",
                    "pending_event": {"source_node_id": -1, "event_npc_id": ""},
                    "party_selection": {"quest_npc": {"npc_id": "Npc.YueBai", "selected_card_ids": ["A", "B", "C"]}},
                },
            },
            event_kind="task_npc",
            event_npc_id_before_open="Npc.YueBai",
            player_gold_before_open=50,
        )
        self.assertTrue(npc["ok"])

        niu_huan = self.runner.evaluate_event_resolution(
            {
                "action": {"ok": True, "kind": "trigger-route-encounter-primary", "selected_action": "TakeGold", "source_actor_path_before_choice": "/Game/PIE/RouteEncounter_A", "source_last_interaction_successful_after_choice": True},
                "runtime_state": {"screen": "DungeonMap", "player_gold": 75},
                "route_panel": {"is_open": False, "source_actor_path": ""},
                "card_run": {
                    "active_temporary_quest_npc_id": "None",
                    "pending_event": {"source_node_id": -1, "event_npc_id": ""},
                    "party_selection": {"active_permanent_companion_instance_id": "Partner.1", "quest_npc": {"npc_id": "", "selected_card_ids": []}},
                },
            },
            event_kind="niu_huan",
            event_npc_id_before_open="Npc.Event.NiuHuan",
            player_gold_before_open=50,
        )
        self.assertTrue(niu_huan["ok"])
        self.assertTrue(niu_huan["event_reward_increased_gold"])

        self.assertFalse(self.runner.evaluate_event_resolution(
            {
                "action": {"ok": True, "kind": "trigger-route-encounter-primary", "selected_action": "TakeGold", "source_actor_path_before_choice": "/Game/PIE/RouteEncounter_A", "source_last_interaction_successful_after_choice": True},
                "runtime_state": {"screen": "DungeonMap", "player_gold": 75},
                "route_panel": {"is_open": False, "source_actor_path": "/Game/PIE/RouteEncounter_A"},
                "card_run": {
                    "active_temporary_quest_npc_id": "None",
                    "pending_event": {"source_node_id": 1, "event_npc_id": "Npc.Event.NiuHuan"},
                    "party_selection": {"quest_npc": {"npc_id": "", "selected_card_ids": []}},
                },
            },
            event_kind="niu_huan",
            event_npc_id_before_open="Npc.Event.NiuHuan",
            player_gold_before_open=50,
        )["ok"])

    def test_panel_evidence_does_not_treat_chest_as_an_unnamed_event(self) -> None:
        chest = self.runner.evaluate_route_panel(
            {
                "runtime_state": {"screen": "RouteEvent", "pending_route_node_kind": "Chest"},
                "route_panel": {"kind": "Chest", "is_explicit": True, "is_in_viewport": True, "is_open": True, "primary_action": "TakeGold", "source_actor_path": "/Game/PIE/RouteEncounter_A"},
                "action": {
                    "ok": True,
                    "kind": "open_route_encounter_panel_only",
                    "input_path": "pawn_interaction_binding",
                    "rejected_when_unfocused": True,
                    "actor": "/Game/PIE/RouteEncounter_A",
                    "focused_actor_path": "/Game/PIE/RouteEncounter_A",
                    "source_actor_path": "/Game/PIE/RouteEncounter_A",
                    "source_last_interaction_successful_before_choice": False,
                },
            },
            expected_kind="Chest",
        )
        self.assertTrue(chest["ok"])
        ungated_chest = self.runner.evaluate_route_panel(
            {
                "runtime_state": {"screen": "RouteEvent", "pending_route_node_kind": "Chest"},
                "route_panel": {"kind": "Chest", "is_explicit": True, "is_in_viewport": True, "is_open": True, "primary_action": "TakeGold"},
            },
            expected_kind="Chest",
        )
        self.assertFalse(ungated_chest["ok"])
        generic_event = self.runner.evaluate_route_panel(
            {
                "runtime_state": {"screen": "RouteEvent", "pending_route_node_kind": "Chest"},
                "route_panel": {"kind": "Event", "is_explicit": False, "is_in_viewport": True, "is_open": True, "primary_action": "None"},
            },
            expected_kind="Chest",
        )
        self.assertFalse(generic_event["ok"])

    def test_route_panel_primary_action_requires_state_cleanup_and_source_completion(self) -> None:
        self.assertTrue(hasattr(self.runner, "evaluate_route_panel_resolution"))
        chest = self.runner.evaluate_route_panel_resolution(
            {
                "action": {
                    "ok": True,
                    "kind": "trigger-route-encounter-primary",
                    "selected_action": "TakeGold",
                    "source_actor_path_before_choice": "/Game/PIE/RouteEncounter_A",
                    "source_last_interaction_successful_after_choice": True,
                },
                "runtime_state": {
                    "screen": "DungeonMap",
                    "player_gold": 75,
                    "player_hp": 50,
                    "player_max_hp": 100,
                    "pending_route_node_id": -1,
                    "visited_route_node_ids": [0, 1],
                },
                "route_panel": {"is_open": False, "source_actor_path": ""},
                "card_run": {"pending_event": {"source_node_id": -1, "event_npc_id": ""}},
            },
            expected_kind="Chest",
            pending_node_id_before_open=1,
            player_gold_before_open=50,
            player_hp_before_open=50,
        )
        self.assertTrue(chest["ok"])

        camp = self.runner.evaluate_route_panel_resolution(
            {
                "action": {
                    "ok": True,
                    "kind": "trigger-route-encounter-primary",
                    "selected_action": "CampRest",
                    "source_actor_path_before_choice": "/Game/PIE/RouteEncounter_A",
                    "source_last_interaction_successful_after_choice": True,
                },
                "runtime_state": {
                    "screen": "DungeonMap",
                    "player_gold": 50,
                    "player_hp": 100,
                    "player_max_hp": 100,
                    "pending_route_node_id": -1,
                    "visited_route_node_ids": [0, 1],
                },
                "route_panel": {"is_open": False, "source_actor_path": ""},
                "card_run": {"pending_event": {"source_node_id": -1, "event_npc_id": ""}},
            },
            expected_kind="Camp",
            pending_node_id_before_open=1,
            player_gold_before_open=50,
            player_hp_before_open=50,
        )
        self.assertTrue(camp["ok"])

        merchant = self.runner.evaluate_route_panel_resolution(
            {
                "action": {
                    "ok": True,
                    "kind": "trigger-route-encounter-primary",
                    "selected_action": "MerchantLeave",
                    "source_actor_path_before_choice": "/Game/PIE/RouteEncounter_A",
                    "source_last_interaction_successful_after_choice": True,
                },
                "runtime_state": {
                    "screen": "DungeonMap",
                    "player_gold": 50,
                    "player_hp": 50,
                    "player_max_hp": 100,
                    "pending_route_node_id": -1,
                    "visited_route_node_ids": [0, 1],
                },
                "route_panel": {"is_open": False, "source_actor_path": ""},
                "card_run": {"pending_event": {"source_node_id": -1, "event_npc_id": ""}},
            },
            expected_kind="Merchant",
            pending_node_id_before_open=1,
            player_gold_before_open=50,
            player_hp_before_open=50,
        )
        self.assertTrue(merchant["ok"])

        stale_source = self.runner.evaluate_route_panel_resolution(
            {
                "action": {
                    "ok": True,
                    "kind": "trigger-route-encounter-primary",
                    "selected_action": "MerchantLeave",
                    "source_actor_path_before_choice": "/Game/PIE/RouteEncounter_A",
                    "source_last_interaction_successful_after_choice": True,
                },
                "runtime_state": {
                    "screen": "DungeonMap",
                    "player_gold": 50,
                    "player_hp": 50,
                    "player_max_hp": 100,
                    "pending_route_node_id": -1,
                    "visited_route_node_ids": [0, 1],
                },
                "route_panel": {"is_open": False, "source_actor_path": "/Game/PIE/RouteEncounter_A"},
                "card_run": {"pending_event": {"source_node_id": -1, "event_npc_id": ""}},
            },
            expected_kind="Merchant",
            pending_node_id_before_open=1,
            player_gold_before_open=50,
            player_hp_before_open=50,
        )
        self.assertFalse(stale_source["ok"])

    def test_party_evidence_enforces_hero_one_partner_one_task_npc_and_visuals(self) -> None:
        snapshot = {
            "card_run": {
                "party_selection": {
                    "active_permanent_companion_instance_id": "Partner.Blade.1",
                    "quest_npc": {"npc_id": "Npc.YueBai", "selected_card_ids": ["A", "B", "C"]},
                }
            },
            "party_visuals": [
                {"unit_id": "Hero", "role": "Hero", "has_visual": True},
                {"unit_id": "Partner.Blade.1", "role": "Blade", "has_visual": True},
                {"unit_id": "Npc.YueBai", "role": "QuestNpc", "has_visual": True},
            ],
        }
        verdict = self.runner.evaluate_party_cap_and_visuals(snapshot)
        self.assertTrue(verdict["ok"])
        self.assertEqual(verdict["combat_member_count"], 3)

    def test_automation_plan_covers_every_forced_branch_without_private_pie_mutation(self) -> None:
        plan = self.runner.get_automation_test_plan()
        flattened = {name for names in plan.values() for name in names}
        self.assertIn("GameXXK.Integration.CardBattle.BoardTargeting", flattened)
        self.assertIn("GameXXK.Integration.CardBattle.BoardRewardReplacement", flattened)
        self.assertIn("GameXXK.Integration.CardRoute.EventSupport", flattened)
        self.assertIn("GameXXK.Integration.CardRoute.Lifecycle", flattened)
        self.assertIn("GameXXK.MVP.RouteEncounter.Panel.EventIdentityAndExplicitChoice", flattened)
        self.assertIn("GameXXK.MVP.RouteEncounter.Panel.VisibleChoicesResolveOnlyOnClick", flattened)
        self.assertIn("GameXXK.MVP.Battle.PartyDeckVisualMapping", flattened)

    def test_runner_does_not_offer_a_nonexistent_transient_fixture_and_requires_one_live_panel_kind(self) -> None:
        source = RUNNER_PATH.read_text(encoding="utf-8")
        self.assertNotIn("--transient-fixture", source)
        self.assertIn("--expected-panel-kind", source)
        self.assertIn('choices=("Camp", "Merchant", "Chest")', source)


if __name__ == "__main__":
    unittest.main()
