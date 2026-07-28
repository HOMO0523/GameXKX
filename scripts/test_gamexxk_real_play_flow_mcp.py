#!/usr/bin/env python3
"""Unit coverage for the real-flow screenshot transport fallback."""

from __future__ import annotations

import base64
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SCRIPTS_ROOT = PROJECT_ROOT / "scripts"
if str(SCRIPTS_ROOT) not in sys.path:
    sys.path.insert(0, str(SCRIPTS_ROOT))

import gamexxk_real_play_flow_mcp as flow


_ONE_PIXEL_PNG = base64.b64decode(
    "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVQIHWP4z8DwHwAFgAI/"
    "Y3D3VwAAAABJRU5ErkJggg=="
)


class SlateScreenshotFallbackTest(unittest.TestCase):
    def test_real_flow_targets_the_verified_battle_town_map(self) -> None:
        self.assertEqual("L_BattleTown", flow.BATTLE_MAP_TOKEN)

    def test_preview_ref_is_selected_from_the_slate_window_snapshot(self) -> None:
        snapshot = (
            'window "GameXXK - Unreal Editor" [ref=w1]\n'
            'window "GameXXK Preview [NetMode: Standalone 0]" [ref=w18]\n'
        )
        self.assertEqual("w18", flow._slate_preview_window_ref(snapshot))
        self.assertEqual("", flow._slate_preview_window_ref('window "GameXXK - Unreal Editor" [ref=w1]'))

    def test_slate_png_decoder_accepts_png_and_rejects_invalid_transport(self) -> None:
        payload = {"mimeType": "image/png", "data": base64.b64encode(_ONE_PIXEL_PNG).decode("ascii")}
        self.assertEqual(_ONE_PIXEL_PNG, flow._decode_slate_screenshot_png(payload))

        with self.assertRaises(RuntimeError):
            flow._decode_slate_screenshot_png({"mimeType": "image/jpeg", "data": payload["data"]})
        with self.assertRaises(RuntimeError):
            flow._decode_slate_screenshot_png({"mimeType": "image/png", "data": "not-base64"})


class _FakeProjectPythonClient:
    def __init__(self) -> None:
        self.timeout = 5.0
        self.calls: list[tuple[str, list[str]]] = []

    def run_project_python_file(self, relative_path: str, argv: list[str]) -> dict[str, str]:
        self.calls.append((relative_path, list(argv)))
        if argv and argv[0] == "--town-key":
            key = argv[1]
            state = argv[2]
            payload = {
                "town_key": {
                    "ok": True,
                    "key": key,
                    "state": state,
                    "pressed": state == "down",
                },
                "probe": {},
            }
        elif argv and argv[0] == "--town-interact":
            payload = {"town_interact": {"ok": True}, "probe": {}}
        else:
            raise AssertionError(f"Unexpected project Python call: {argv}")
        return {"stdout": json.dumps(payload)}


class _MalformedTownKeyReplyClient(_FakeProjectPythonClient):
    """Models a town-key dispatch that reaches PIE but loses its response."""

    def run_project_python_file(self, relative_path: str, argv: list[str]) -> dict[str, str]:
        self.calls.append((relative_path, list(argv)))
        if argv and argv[0] == "--town-key":
            if argv[1] == "D" and argv[2] == "down":
                return {"stdout": "{malformed town-key response"}
            return {
                "stdout": json.dumps(
                    {
                        "town_key": {
                            "ok": True,
                            "key": argv[1],
                            "state": argv[2],
                            "pressed": argv[2] == "down",
                        },
                        "probe": {},
                    }
                )
            }
        raise AssertionError(f"Unexpected project Python call: {argv}")


class _ReleaseFailureClient(_FakeProjectPythonClient):
    """Models a PIE key release that reaches MCP but is rejected by the probe."""

    def run_project_python_file(self, relative_path: str, argv: list[str]) -> dict[str, str]:
        self.calls.append((relative_path, list(argv)))
        if argv and argv[0] == "--town-key":
            key = argv[1]
            state = argv[2]
            return {
                "stdout": json.dumps(
                    {
                        "town_key": {
                            "ok": state != "up",
                            "key": key,
                            "state": state,
                            "pressed": state == "down",
                        },
                        "probe": {},
                    }
                )
            }
        raise AssertionError(f"Unexpected project Python call: {argv}")


class _RecordingTownInputClient(_FakeProjectPythonClient):
    """Records the actual project-Python key transport used by the harness."""

    def __init__(self, timeline: list[tuple[object, ...]]) -> None:
        super().__init__()
        self.timeline = timeline

    def run_project_python_file(self, relative_path: str, argv: list[str]) -> dict[str, str]:
        result = super().run_project_python_file(relative_path, argv)
        if argv and argv[0] == "--town-key":
            self.timeline.append(("key", argv[1], argv[2]))
        return result


class _ReadOnlyBattleHudClient:
    endpoint = "http://fake-mcp:18765/mcp"

    def __init__(self) -> None:
        self.runtime_actions: list[tuple[object, ...]] = []
        self.project_probe_calls: list[tuple[str, list[str]]] = []

    def connect(self) -> bool:
        self.runtime_actions.append(("connect",))
        return True

    def is_in_pie(self) -> bool:
        self.runtime_actions.append(("is_in_pie",))
        return True

    def run_project_python_file(self, relative_path: str, argv: list[str]) -> dict[str, str]:
        self.runtime_actions.append(("run_project_python_file", relative_path, list(argv)))
        self.project_probe_calls.append((relative_path, list(argv)))
        if relative_path != flow.PROBE_SCRIPT or argv:
            raise AssertionError(f"Observation attempted a non-read-only project command: {relative_path} {argv}")
        return {
            "stdout": json.dumps(
                {
                    "probe": {
                        "map_name": "L_BattleTown",
                        "pie_viewport": {
                            "width": 319.0,
                            "height": 617.0,
                            "source": "player_controller.get_viewport_size",
                        },
                        "actors": [
                            {
                                "class": "GameXXKBattleSceneUnitActor",
                                "unit_id": "Player",
                                "is_enemy_unit": False,
                            },
                        ],
                        "battle_board": {
                            "unit_hud_layer": {"visible": True, "screen_rect": {"left": 0, "top": 0, "right": 319, "bottom": 500}},
                            "unit_huds": {
                                "Player": {
                                    "unit_id": "Player", "visible": True, "projected_anchor": {"x": 100, "y": 100},
                                    "screen_rect": {"left": 50, "top": 50, "right": 150, "bottom": 150},
                                    "resource": {"visible": True, "screen_rect": None, "mana_row_visible": True, "rendered": {}},
                                    "status": {"visible": True, "screen_rect": {"left": 50, "top": 120, "right": 150, "bottom": 150}, "rendered": {}},
                                },
                            },
                        },
                    },
                }
            )
        }

    def start_pie(self, *args, **kwargs):
        raise AssertionError(f"Read-only observation attempted StartPIE: {args} {kwargs}")

    def stop_pie(self, *args, **kwargs):
        raise AssertionError(f"Read-only observation attempted StopPIE: {args} {kwargs}")

    def call_tool(self, *args, **kwargs):
        raise AssertionError(f"Read-only observation attempted a tool mutation/click: {args} {kwargs}")


class _BattleHudFixtureClient:
    def __init__(self) -> None:
        self.calls: list[tuple[str, list[str]]] = []

    def run_project_python_file(self, relative_path: str, argv: list[str]) -> dict[str, str]:
        self.calls.append((relative_path, list(argv)))
        if relative_path != flow.BATTLE_HUD_FIXTURE_SCRIPT:
            raise AssertionError(f"Fixture command used an unexpected script: {relative_path}")
        if argv == []:
            payload = {"battle_hud_fixture": {"ok": True}}
        elif argv == ["--clear"]:
            payload = {"battle_hud_fixture_clear": {"ok": True}}
        else:
            raise AssertionError(f"Unexpected fixture entrypoint arguments: {argv}")
        return {"stdout": json.dumps(payload)}


def _harness_with_client(client: _FakeProjectPythonClient):
    harness = object.__new__(flow.RealFlowHarness)
    harness.client = client
    harness.events = []
    return harness


def _town_visual_probe(state: str, direction: str) -> dict[str, object]:
    return {
        "probe": {
            "pawn": {
                "current_flipbook": f"/Game/Test/FB_Hero_{state}_{direction}.FB_Hero_{state}_{direction}",
                "is_town_moving": state == "Walk",
                "visual": {"relative_rotation": {"yaw": 90.0}},
            }
        }
    }


class BattleHudFixtureEntrypointTest(unittest.TestCase):
    def test_harness_uses_the_explicit_fixture_entrypoint_for_apply_and_clear(self) -> None:
        client = _BattleHudFixtureClient()
        harness = _harness_with_client(client)
        harness.battle_hud_fixture_may_be_applied = False

        harness.apply_battle_hud_fixture()
        harness.clear_battle_hud_fixture()

        self.assertEqual(
            [
                (flow.BATTLE_HUD_FIXTURE_SCRIPT, []),
                (flow.BATTLE_HUD_FIXTURE_SCRIPT, ["--clear"]),
            ],
            client.calls,
        )


class TownMcpInputBackendTest(unittest.TestCase):
    def test_town_key_sends_persistent_key_state_through_the_project_python_probe(self) -> None:
        client = _FakeProjectPythonClient()
        harness = _harness_with_client(client)

        harness.town_key("d", True)

        self.assertEqual(
            [(flow.PROBE_SCRIPT, ["--town-key", "D", "down"])],
            client.calls,
        )
        self.assertEqual("town_key", harness.events[-1]["name"])
        self.assertTrue(harness.events[-1]["ok"])
        self.assertEqual("mcp_project_python", harness.events[-1]["backend"])
        self.assertEqual(flow.PROBE_SCRIPT, harness.events[-1]["probe_script"])
        self.assertEqual("D", harness.events[-1]["key"])
        self.assertEqual("down", harness.events[-1]["state"])

    def test_town_interact_uses_the_project_python_pawn_interact_command(self) -> None:
        client = _FakeProjectPythonClient()
        harness = _harness_with_client(client)

        harness.town_interact()

        self.assertEqual([(flow.PROBE_SCRIPT, ["--town-interact"])], client.calls)
        self.assertEqual("town_interact", harness.events[-1]["name"])
        self.assertTrue(harness.events[-1]["ok"])
        self.assertEqual("mcp_project_python", harness.events[-1]["backend"])
        self.assertEqual(flow.PROBE_SCRIPT, harness.events[-1]["probe_script"])

    def test_walk_route_uses_key_holds_without_axis_or_win32_keyboard_injection(self) -> None:
        source = flow.__file__
        harness_source = Path(source).read_text(encoding="utf-8")
        start = harness_source.index("    def walk_to_world_location(")
        end = harness_source.index("    def run(self)", start)
        walk_source = harness_source[start:end]

        self.assertIn("hold_town_keys", walk_source)
        self.assertNotIn("town_axis", walk_source)
        self.assertNotIn("self.input.press_key(", walk_source)
        self.assertNotIn("self.input.key_down(", walk_source)

    def test_hold_town_keys_releases_every_maybe_pressed_key_in_reverse_order_after_mcp_error(self) -> None:
        client = _MalformedTownKeyReplyClient()
        harness = _harness_with_client(client)

        with self.assertRaises(json.JSONDecodeError):
            with harness.hold_town_keys("W", "D"):
                pass

        self.assertEqual(
            [
                (flow.PROBE_SCRIPT, ["--town-key", "W", "down"]),
                (flow.PROBE_SCRIPT, ["--town-key", "D", "down"]),
                (flow.PROBE_SCRIPT, ["--town-key", "D", "up"]),
                (flow.PROBE_SCRIPT, ["--town-key", "W", "up"]),
            ],
            client.calls,
        )

    def test_serialized_mcp_diagonal_release_probes_vertical_state_before_releasing_w(self) -> None:
        timeline: list[tuple[object, ...]] = []
        client = _RecordingTownInputClient(timeline)
        harness = _harness_with_client(client)
        probes = iter(
            [
                _town_visual_probe("Walk", "NorthEast"),
                _town_visual_probe("Walk", "North"),
                _town_visual_probe("Idle", "North"),
            ]
        )

        def probe() -> dict[str, object]:
            result = next(probes)
            pawn = result["probe"]["pawn"]
            timeline.append(("probe", pawn["current_flipbook"].rsplit("/", 1)[-1]))
            return result

        harness.probe = probe
        self.assertTrue(
            hasattr(harness, "run_serialized_mcp_d_to_w_release"),
            "The real-flow harness needs a behaviorally testable D->W release sequence.",
        )
        with patch.object(flow.time, "sleep", side_effect=lambda seconds: timeline.append(("sleep", seconds))):
            after_move = harness.run_serialized_mcp_d_to_w_release()

        self.assertEqual(
            [
                ("key", "W", "down"),
                ("key", "D", "down"),
                ("sleep", 0.25),
                ("probe", "FB_Hero_Walk_NorthEast.FB_Hero_Walk_NorthEast"),
                ("sleep", 0.25),
                ("key", "D", "up"),
                ("sleep", flow.DIAGONAL_D_TO_W_VERTICAL_PROBE_SETTLE_SECONDS),
                ("probe", "FB_Hero_Walk_North.FB_Hero_Walk_North"),
                ("key", "W", "up"),
                ("sleep", 0.25),
                ("probe", "FB_Hero_Idle_North.FB_Hero_Idle_North"),
            ],
            timeline,
        )
        self.assertTrue(flow._expect_visual_state(after_move, "Idle", "North")["ok"])

    def test_hold_town_keys_fails_when_successful_key_cannot_be_released(self) -> None:
        client = _ReleaseFailureClient()
        harness = _harness_with_client(client)

        with self.assertRaisesRegex(RuntimeError, "Town key release"):
            with harness.hold_town_keys("D"):
                pass

        self.assertEqual(
            [
                (flow.PROBE_SCRIPT, ["--town-key", "D", "down"]),
                (flow.PROBE_SCRIPT, ["--town-key", "D", "up"]),
            ],
            client.calls,
        )
        release_events = [event for event in harness.events if event["name"] == "town_key_release_failed"]
        self.assertEqual(1, len(release_events))
        self.assertEqual("mcp_project_python", release_events[0]["backend"])
        self.assertEqual(flow.PROBE_SCRIPT, release_events[0]["probe_script"])


class BattleHudVerdictTest(unittest.TestCase):
    viewport = {"width": 1280, "height": 720}

    @staticmethod
    def _battle_actor(
        unit_id: str,
        *,
        enemy: bool,
        screen_rect: dict[str, float] | None = None,
        component_visible: bool = True,
        widget_visible: bool = True,
        mana_row_visible: bool | None = None,
        armor: int | None = None,
        status_text: str = "",
        status_rect: dict[str, float] | None = None,
        hp_current: int | None = None,
        hp_max: int = 100,
        mana_current: int | None = None,
        mana_max: int = 30,
        rendered_status_badges: list[dict[str, str]] | None = None,
    ) -> dict[str, object]:
        if screen_rect is None:
            screen_rect = {
                "left": 800.0 if enemy else 100.0,
                "top": 100.0,
                "right": 1000.0 if enemy else 300.0,
                "bottom": 160.0,
            }
        if mana_row_visible is None:
            mana_row_visible = not enemy
        if hp_current is None:
            hp_current = 32 if enemy else 72
        if mana_current is None:
            mana_current = 0 if enemy else 18
        if armor is None:
            armor = 0 if enemy else 7
        if status_rect is None:
            status_rect = {
                "left": 800.0 if enemy else 100.0,
                "top": 165.0,
                "right": 880.0 if enemy else 180.0,
                "bottom": 190.0,
            }
        if rendered_status_badges is None:
            rendered_status_badges = (
                [
                    {"icon_id": "BleedDrop", "displayed_stack": "3"},
                    {"icon_id": "PoisonVial", "displayed_stack": "2"},
                ]
                if enemy
                else [{"icon_id": "ArmorShield", "displayed_stack": str(armor)}]
            )
        return {
            "class": "GameXXKBattleSceneUnitActor",
            "unit_id": unit_id,
            "is_enemy_unit": enemy,
            "board_hud": {
                "unit_id": unit_id,
                "side": "Enemy" if enemy else "Party",
                "slot": 1,
                "projected_anchor": {"x": (screen_rect["left"] + screen_rect["right"]) / 2.0, "y": 130.0},
                "screen_rect": {"left": screen_rect["left"], "top": screen_rect["top"], "right": screen_rect["right"], "bottom": status_rect["bottom"]},
                "visible": component_visible and widget_visible,
                "resource": {
                    "visible": component_visible and widget_visible,
                    "screen_rect": screen_rect,
                    "mana_row_visible": mana_row_visible,
                    "rendered": {
                        "health_text": f"气血 {hp_current} / {hp_max}",
                        "mana_text": f"内力 {mana_current} / {mana_max}",
                        "health_percent": float(hp_current) / float(hp_max),
                        "mana_percent": float(mana_current) / float(mana_max),
                    },
                },
                "status": {
                    "visible": True,
                    "screen_rect": status_rect,
                    "rendered": {"icon_count": len(rendered_status_badges), "badges": rendered_status_badges},
                },
            },
        }

    @classmethod
    def _probe(cls, *actors: dict[str, object]) -> dict[str, object]:
        unit_huds = {str(actor["unit_id"]): actor.pop("board_hud") for actor in actors}
        return {
            "probe": {
                "actors": list(actors),
                "battle_board": {
                    "shared_energy": 2,
                    "party_qi": {
                        "visible": True,
                        "value": 2,
                        "screen_rect": {"left": 515.0, "top": 548.0, "right": 705.0, "bottom": 620.0},
                    },
                    "hand_card_box": {
                        "visible": True,
                        "screen_rect": {"left": 80.0, "top": 640.0, "right": 720.0, "bottom": 710.0},
                    },
                    "end_turn_button": {
                        "visible": True,
                        "screen_rect": {"left": 990.0, "top": 640.0, "right": 1160.0, "bottom": 710.0},
                    },
                    "unit_hud_layer": {
                        "visible": True,
                        "screen_rect": {"left": 0.0, "top": 0.0, "right": 1280.0, "bottom": 640.0},
                    },
                    "unit_huds": unit_huds,
                },
            },
        }

    def _valid_probe(self) -> dict[str, object]:
        return self._probe(
            self._battle_actor("Player", enemy=False),
            self._battle_actor("Enemy.Outer", enemy=True),
        )

    @staticmethod
    def _mark_geometry_api_unavailable(summary: dict[str, object]) -> None:
        """Model UE 5.8 Python, where cached geometry exists but its vector getters are not bound."""
        summary["screen_rect"] = None
        summary["geometry"] = {
            "cached": True,
            "errors": [
                {
                    "stage": "geometry.get_local_size",
                    "exception": "AttributeError: 'Geometry' object has no attribute 'get_local_size'",
                }
            ],
        }

    def _probe_with_ue58_geometry_api_unavailable(self) -> dict[str, object]:
        probe = self._valid_probe()
        board = probe["probe"]["battle_board"]
        for key in ("unit_hud_layer", "party_qi", "hand_card_box", "end_turn_button"):
            self._mark_geometry_api_unavailable(board[key])
        for unit_hud in board["unit_huds"].values():
            self._mark_geometry_api_unavailable(unit_hud)
            self._mark_geometry_api_unavailable(unit_hud["resource"])
            self._mark_geometry_api_unavailable(unit_hud["status"])
            unit_hud["projection"] = {
                "applied_slot": {
                    "size": {"x": 272.0, "y": 142.0},
                    "visibility": "SELF_HIT_TEST_INVISIBLE",
                }
            }
            unit_hud["projected_anchor"] = {"x": 0.75, "y": 0.52}
        return probe

    def _probe_with_real_ue58_aggregated_unit_diagnostics(self) -> dict[str, object]:
        """Mirror the project probe: Board retains Geometry errors; unit summaries retain only the aggregate diagnostic."""
        probe = self._probe_with_ue58_geometry_api_unavailable()
        board = probe["probe"]["battle_board"]
        for unit_hud in board["unit_huds"].values():
            for summary, stage in (
                (unit_hud, "battle_unit_hud.screen_rect"),
                (unit_hud["resource"], "battle_unit_hud.resource.screen_rect"),
                (unit_hud["status"], "battle_unit_hud.status.screen_rect"),
            ):
                summary.pop("geometry", None)
                summary["diagnostics"] = [{"code": "screen_rect_unavailable", "stage": stage}]
        return probe

    def test_battle_hud_verdict_reads_outer_probe_actors_and_accepts_visible_in_viewport_resources(self) -> None:
        verdict = flow._battle_hud_verdict(self._valid_probe(), self.viewport)

        self.assertTrue(verdict["ok"])
        self.assertEqual(2, verdict["unit_count"])
        self.assertEqual({}, verdict["errors"])

    def test_battle_hud_verdict_uses_fixed_slot_evidence_when_ue58_python_geometry_access_is_unavailable(self) -> None:
        verdict = flow._battle_hud_verdict(self._probe_with_ue58_geometry_api_unavailable(), self.viewport)

        self.assertTrue(verdict["ok"])
        self.assertEqual("fixed_slot_fallback", verdict["geometry_validation_mode"])

    def test_battle_hud_verdict_recognizes_the_real_probe_aggregated_geometry_diagnostics(self) -> None:
        verdict = flow._battle_hud_verdict(self._probe_with_real_ue58_aggregated_unit_diagnostics(), self.viewport)

        self.assertTrue(verdict["ok"])
        self.assertEqual("fixed_slot_fallback", verdict["geometry_validation_mode"])

    def test_battle_hud_verdict_recognizes_child_geometry_diagnostics_aggregated_on_the_parent_unit_hud(self) -> None:
        probe = self._probe_with_real_ue58_aggregated_unit_diagnostics()
        for unit_hud in probe["probe"]["battle_board"]["unit_huds"].values():
            unit_hud["diagnostics"] = [
                {"code": "screen_rect_unavailable", "stage": "battle_unit_hud.screen_rect"},
                {"code": "screen_rect_unavailable", "stage": "battle_unit_hud.resource.screen_rect"},
                {"code": "screen_rect_unavailable", "stage": "battle_unit_hud.status.screen_rect"},
            ]
            unit_hud["resource"].pop("diagnostics", None)
            unit_hud["status"].pop("diagnostics", None)

        verdict = flow._battle_hud_verdict(probe, self.viewport)

        self.assertTrue(verdict["ok"])
        self.assertEqual("fixed_slot_fallback", verdict["geometry_validation_mode"])

    def test_battle_hud_crop_is_skipped_only_for_the_ue58_fixed_slot_fallback(self) -> None:
        self.assertFalse(flow._should_capture_battle_hud_crops({"geometry_validation_mode": "fixed_slot_fallback"}))
        self.assertTrue(flow._should_capture_battle_hud_crops({"geometry_validation_mode": "screen_rect"}))

    def test_battle_hud_verdict_rejects_missing_or_orphan_board_unit_huds(self) -> None:
        missing = self._valid_probe()
        del missing["probe"]["battle_board"]["unit_huds"]["Player"]
        missing_verdict = flow._battle_hud_verdict(missing, self.viewport)
        self.assertIn("unit_hud_missing", missing_verdict["errors"]["Player"])

        orphan = self._valid_probe()
        orphan["probe"]["battle_board"]["unit_huds"]["Orphan"] = dict(
            orphan["probe"]["battle_board"]["unit_huds"]["Player"], unit_id="Orphan"
        )
        orphan_verdict = flow._battle_hud_verdict(orphan, self.viewport)
        self.assertIn("unit_hud_orphan", orphan_verdict["errors"]["__board__"])

    def test_battle_hud_verdict_rejects_a_stale_board_anchor(self) -> None:
        probe = self._valid_probe()
        probe["probe"]["battle_board"]["unit_huds"]["Player"]["projected_anchor"]["x"] += 3.0
        verdict = flow._battle_hud_verdict(probe, self.viewport)
        self.assertIn("unit_hud_anchor_mismatch", verdict["errors"]["Player"])

    def test_board_unit_hud_crop_target_uses_board_unit_hud_rect(self) -> None:
        probe = self._valid_probe()
        self.assertEqual(
            probe["probe"]["battle_board"]["unit_huds"]["Player"]["screen_rect"],
            flow._unit_hud_crop_rect(probe, "Party"),
        )

    def test_board_hud_crop_prefers_player_over_earlier_party_entries(self) -> None:
        probe = self._probe(
            self._battle_actor("Partner", enemy=False, screen_rect={"left": 350.0, "top": 100.0, "right": 550.0, "bottom": 160.0}),
            self._battle_actor("Player", enemy=False),
            self._battle_actor("Enemy.Outer", enemy=True),
        )

        self.assertEqual(
            probe["probe"]["battle_board"]["unit_huds"]["Player"]["screen_rect"],
            flow._unit_hud_crop_rect(probe, "Party"),
        )

    def test_battle_hud_verdict_is_total_for_malformed_roots_and_nonfinite_values(self) -> None:
        for malformed in (None, [], {}, {"probe": None}, {"probe": []}):
            with self.subTest(malformed=repr(malformed)):
                verdict = flow._battle_hud_verdict(malformed, self.viewport)
                self.assertFalse(verdict["ok"])
                self.assertEqual("probe_invalid", verdict["reason"])
        observation = flow._battle_hud_observation([])
        self.assertFalse(observation["ok"])
        self.assertEqual("probe_invalid", observation["battle_hud_verdict"]["reason"])

        viewport_verdict = flow._battle_hud_verdict(self._valid_probe(), {"width": float("nan"), "height": 720})
        self.assertEqual("viewport_invalid", viewport_verdict["reason"])

        energy = self._valid_probe()
        energy["probe"]["battle_board"]["shared_energy"] = float("inf")
        self.assertIn("shared_energy_non_finite", flow._battle_hud_verdict(energy, self.viewport)["errors"]["__board__"])

        percent = self._valid_probe()
        percent["probe"]["battle_board"]["unit_huds"]["Player"]["resource"]["rendered"]["health_percent"] = float("nan")
        self.assertIn("fixture_party_health_percent_mismatch", flow._battle_hud_verdict(percent, self.viewport)["errors"]["Player"])

        rect = self._valid_probe()
        rect["probe"]["battle_board"]["unit_huds"]["Player"]["resource"]["screen_rect"]["left"] = float("nan")
        self.assertIn("unit_hud_resource_screen_rect_missing", flow._battle_hud_verdict(rect, self.viewport)["errors"]["Player"])

        anchor = self._valid_probe()
        anchor["probe"]["battle_board"]["unit_huds"]["Player"]["projected_anchor"]["x"] = float("inf")
        self.assertIn("unit_hud_anchor_non_finite", flow._battle_hud_verdict(anchor, self.viewport)["errors"]["Player"])

        icon_count = self._valid_probe()
        icon_count["probe"]["battle_board"]["unit_huds"]["Player"]["status"]["rendered"]["icon_count"] = float("inf")
        self.assertIn("unit_hud_status_icon_count_invalid", flow._battle_hud_verdict(icon_count, self.viewport)["errors"]["Player"])

    def test_harness_uses_board_side_not_actor_side_for_fixture_and_crop_policy(self) -> None:
        probe = self._valid_probe()
        probe["probe"]["actors"][0]["is_enemy_unit"] = True
        probe["probe"]["actors"][1]["is_enemy_unit"] = False

        verdict = flow._battle_hud_verdict(probe, self.viewport)

        self.assertTrue(verdict["ok"])
        self.assertEqual(
            probe["probe"]["battle_board"]["unit_huds"]["Enemy.Outer"]["screen_rect"],
            flow._unit_hud_crop_rect(probe, "Enemy"),
        )

    def test_battle_hud_verdict_rejects_board_side_mismatching_fixture_expectation(self) -> None:
        probe = self._valid_probe()
        probe["probe"]["battle_board"]["unit_huds"]["Player"]["side"] = "Enemy"

        verdict = flow._battle_hud_verdict(probe, self.viewport)

        self.assertIn("unit_hud_side_mismatch", verdict["errors"]["Player"])

    def test_battle_hud_verdict_rejects_duplicate_scene_identity_and_board_identity_mismatch(self) -> None:
        duplicate = self._valid_probe()
        duplicate["probe"]["actors"].append(dict(duplicate["probe"]["actors"][0]))
        duplicate_verdict = flow._battle_hud_verdict(duplicate, self.viewport)
        self.assertIn("unit_hud_duplicate_scene_id", duplicate_verdict["errors"]["__board__"])

        mismatch = self._valid_probe()
        mismatch["probe"]["battle_board"]["unit_huds"]["Player"]["unit_id"] = "Other"
        mismatch_verdict = flow._battle_hud_verdict(mismatch, self.viewport)
        self.assertIn("unit_hud_identity_mismatch", mismatch_verdict["errors"]["Player"])

    def test_battle_hud_verdict_rejects_overlapping_visible_board_huds(self) -> None:
        probe = self._valid_probe()
        enemy = probe["probe"]["battle_board"]["unit_huds"]["Enemy.Outer"]
        enemy["screen_rect"] = dict(probe["probe"]["battle_board"]["unit_huds"]["Player"]["screen_rect"])

        verdict = flow._battle_hud_verdict(probe, self.viewport)

        self.assertIn("unit_hud_overlaps_unit_hud", verdict["errors"]["Player"])

    def test_battle_hud_verdict_allows_a_collapsed_empty_status_row_for_a_nonfixture_partner(self) -> None:
        probe = self._probe(
            self._battle_actor("Player", enemy=False),
            self._battle_actor("Enemy.Outer", enemy=True),
            self._battle_actor(
                "Partner",
                enemy=False,
                screen_rect={"left": 350.0, "top": 100.0, "right": 550.0, "bottom": 160.0},
                status_rect={"left": 350.0, "top": 165.0, "right": 430.0, "bottom": 190.0},
            ),
        )
        partner = probe["probe"]["battle_board"]["unit_huds"]["Partner"]
        partner["status"] = {"visible": False, "screen_rect": None, "rendered": {"icon_count": 0, "badges": []}}

        verdict = flow._battle_hud_verdict(probe, self.viewport)

        self.assertTrue(verdict["ok"])

    def test_battle_hud_verdict_requires_fixture_values_from_the_actual_rendered_widgets(self) -> None:
        probe = self._valid_probe()
        player = probe["probe"]["battle_board"]["unit_huds"]["Player"]
        enemy = probe["probe"]["battle_board"]["unit_huds"]["Enemy.Outer"]
        player["resource"]["rendered"]["health_text"] = "气血 100 / 100"
        player["resource"]["rendered"]["mana_percent"] = 1.0
        player["status"]["rendered"]["badges"] = []
        enemy["status"]["rendered"]["badges"] = [
            {"icon_id": "PoisonVial", "displayed_stack": "1"},
        ]

        verdict = flow._battle_hud_verdict(probe, self.viewport)

        self.assertFalse(verdict["ok"])
        self.assertIn("fixture_party_health_text_mismatch", verdict["errors"]["Player"])
        self.assertIn("fixture_party_mana_percent_mismatch", verdict["errors"]["Player"])
        self.assertIn("fixture_party_armor_badge_missing", verdict["errors"]["Player"])
        self.assertIn("fixture_enemy_poison_badge_missing", verdict["errors"]["Enemy.Outer"])
        self.assertIn("fixture_enemy_bleed_badge_missing", verdict["errors"]["Enemy.Outer"])

    def test_battle_hud_verdict_requires_reduced_fixture_party_health_and_mana(self) -> None:
        probe = self._valid_probe()
        player = probe["probe"]["battle_board"]["unit_huds"]["Player"]
        player["resource"]["rendered"]["health_text"] = "气血 100 / 100"
        player["resource"]["rendered"]["mana_text"] = "内力 30 / 30"
        player["resource"]["rendered"]["health_percent"] = 1.0
        player["resource"]["rendered"]["mana_percent"] = 1.0

        verdict = flow._battle_hud_verdict(probe, self.viewport)

        self.assertFalse(verdict["ok"])
        self.assertIn("fixture_party_hp_not_reduced", verdict["errors"]["Player"])
        self.assertIn("fixture_party_mana_not_reduced", verdict["errors"]["Player"])

    def test_battle_hud_verdict_rejects_null_resource_screen_rect_by_stable_unit_id(self) -> None:
        probe = self._valid_probe()
        probe["probe"]["battle_board"]["unit_huds"]["Player"]["resource"]["screen_rect"] = None

        verdict = flow._battle_hud_verdict(probe, self.viewport)

        self.assertFalse(verdict["ok"])
        self.assertIn("unit_hud_resource_screen_rect_missing", verdict["errors"]["Player"])

    def test_battle_hud_observation_persists_null_rect_verdict_from_the_real_pie_viewport(self) -> None:
        probe = self._valid_probe()
        probe["probe"]["pie_viewport"] = {
            "width": 1280.0,
            "height": 722.0,
            "source": "player_controller.get_viewport_size",
        }
        probe["probe"]["battle_board"]["unit_huds"]["Player"]["resource"]["screen_rect"] = None
        probe["probe"]["battle_board"]["unit_huds"]["Enemy.Outer"]["resource"]["screen_rect"] = None

        observation = flow._battle_hud_observation(probe)

        self.assertEqual({"width": 1280.0, "height": 722.0}, observation["viewport"])
        self.assertEqual("player_controller.get_viewport_size", observation["viewport_source"])
        self.assertEqual("evaluated", observation["verification_status"])
        self.assertFalse(observation["battle_hud_verdict"]["ok"])
        self.assertIn("unit_hud_resource_screen_rect_missing", observation["battle_hud_verdict"]["errors"]["Player"])
        self.assertIn("unit_hud_resource_screen_rect_missing", observation["battle_hud_verdict"]["errors"]["Enemy.Outer"])

    def test_battle_hud_verdict_rejects_offscreen_resource_rect_by_stable_unit_id(self) -> None:
        probe = self._valid_probe()
        probe["probe"]["battle_board"]["unit_huds"]["Player"]["resource"]["screen_rect"] = {
            "left": 1290.0,
            "top": 100.0,
            "right": 1400.0,
            "bottom": 160.0,
        }

        verdict = flow._battle_hud_verdict(probe, self.viewport)

        self.assertFalse(verdict["ok"])
        self.assertIn("unit_hud_resource_offscreen", verdict["errors"]["Player"])

    def test_screen_rect_requires_full_containment_with_only_one_pixel_tolerance(self) -> None:
        within_tolerance, _ = flow._screen_rect_in_viewport(
            {"left": -1.0, "top": 0.0, "right": 1281.0, "bottom": 720.0},
            (1280.0, 720.0),
            "resource_hud",
        )
        partial, reason = flow._screen_rect_in_viewport(
            {"left": -1.01, "top": 0.0, "right": 1281.0, "bottom": 720.0},
            (1280.0, 720.0),
            "resource_hud",
        )

        self.assertTrue(within_tolerance)
        self.assertFalse(partial)
        self.assertEqual("resource_hud_partially_offscreen", reason)

    def test_battle_hud_verdict_rejects_partially_offscreen_actor_status_and_board_rectangles(self) -> None:
        cases = (
            ("resource", "Player", "unit_hud_resource_partially_offscreen"),
            ("status", "Player", "unit_hud_status_partially_offscreen"),
            ("party_qi", "__board__", "party_qi_partially_offscreen"),
            ("hand_card_box", "__board__", "hand_card_box_partially_offscreen"),
            ("end_turn_button", "__board__", "end_turn_button_partially_offscreen"),
        )
        for key, error_owner, expected_error in cases:
            with self.subTest(key=key):
                probe = self._valid_probe()
                if key in {"resource", "status"}:
                    probe["probe"]["battle_board"]["unit_huds"]["Player"][key]["screen_rect"]["right"] = 1282.0
                else:
                    probe["probe"]["battle_board"][key]["screen_rect"]["right"] = 1282.0

                verdict = flow._battle_hud_verdict(probe, self.viewport)

                self.assertFalse(verdict["ok"])
                self.assertIn(expected_error, verdict["errors"][error_owner])

    def test_battle_hud_verdict_rejects_enemy_mana_row_by_stable_unit_id(self) -> None:
        probe = self._valid_probe()
        probe["probe"]["battle_board"]["unit_huds"]["Enemy.Outer"]["resource"]["mana_row_visible"] = True

        verdict = flow._battle_hud_verdict(probe, self.viewport)

        self.assertFalse(verdict["ok"])
        self.assertIn("unit_hud_enemy_mana_row_visible", verdict["errors"]["Enemy.Outer"])

    def test_battle_hud_verdict_rejects_party_without_mana_row_by_stable_unit_id(self) -> None:
        probe = self._valid_probe()
        probe["probe"]["battle_board"]["unit_huds"]["Player"]["resource"]["mana_row_visible"] = False

        verdict = flow._battle_hud_verdict(probe, self.viewport)

        self.assertFalse(verdict["ok"])
        self.assertIn("unit_hud_party_mana_row_missing", verdict["errors"]["Player"])

    def test_battle_hud_verdict_requires_visible_board_qi_to_match_card_runtime_shared_energy(self) -> None:
        probe = self._valid_probe()
        probe["probe"]["battle_board"]["party_qi"]["value"] = 1

        verdict = flow._battle_hud_verdict(probe, self.viewport)

        self.assertFalse(verdict["ok"])
        self.assertIn("party_qi_value_mismatch", verdict["errors"]["__board__"])

    def test_battle_hud_verdict_allows_edge_contact_but_rejects_qi_overlap_with_hand_or_end_turn(self) -> None:
        probe = self._valid_probe()
        probe["probe"]["battle_board"]["party_qi"]["screen_rect"] = {
            "left": 500.0,
            "top": 560.0,
            "right": 640.0,
            "bottom": 640.0,
        }
        edge_contact = flow._battle_hud_verdict(probe, self.viewport)
        self.assertTrue(edge_contact["ok"])

        probe["probe"]["battle_board"]["party_qi"]["screen_rect"]["bottom"] = 641.0
        overlap = flow._battle_hud_verdict(probe, self.viewport)
        self.assertFalse(overlap["ok"])
        self.assertIn("party_qi_overlaps_hand_card_box", overlap["errors"]["__board__"])

    def test_battle_hud_verdict_requires_a_wide_visible_status_rect_for_fixture_armor_or_statuses(self) -> None:
        probe = self._probe(
            self._battle_actor("Player", enemy=False, armor=7, status_rect={"left": 100.0, "top": 165.0, "right": 115.0, "bottom": 190.0}),
            self._battle_actor("Enemy.Outer", enemy=True, status_text="毒 2 · 流 3", status_rect={"left": 800.0, "top": 165.0, "right": 830.0, "bottom": 190.0}),
        )

        verdict = flow._battle_hud_verdict(probe, self.viewport)

        self.assertFalse(verdict["ok"])
        self.assertIn("unit_hud_status_width_too_small", verdict["errors"]["Player"])
        self.assertNotIn("unit_hud_status_width_too_small", verdict["errors"].get("Enemy.Outer", []))

    def test_battle_hud_verdict_rejects_an_absent_or_nonbattle_probe(self) -> None:
        verdict = flow._battle_hud_verdict({"probe": {"actors": []}}, self.viewport)

        self.assertFalse(verdict["ok"])
        self.assertEqual("battle_units_missing", verdict["reason"])

    def test_harness_gates_actor_hud_verdict_on_real_battle_preconditions(self) -> None:
        harness_source = Path(flow.__file__).read_text(encoding="utf-8")
        start = harness_source.index("        battle_scene_counts = _battle_scene_counts(after_battle)")
        end = harness_source.index("        result = {", start)
        battle_section = harness_source[start:end]

        self.assertIn("battle_preconditions_ok", battle_section)
        self.assertIn("if battle_preconditions_ok:", battle_section)
        self.assertIn("apply_battle_hud_fixture", battle_section)
        self.assertIn("_battle_hud_observation(after_battle)", battle_section)
        self.assertIn("capture_battle_hud_crops", battle_section)
        self.assertIn('battle_hud_capture["transform"]', battle_section)
        self.assertIn("screenshot_context", battle_section)

    def test_read_only_battle_hud_observation_mode_keeps_existing_pie_and_writes_its_verdict(self) -> None:
        harness_source = Path(flow.__file__).read_text(encoding="utf-8")

        self.assertIn('parser.add_argument("--battle-hud-observation", action="store_true")', harness_source)
        self.assertIn("keep_pie=args.keep_pie or args.battle_hud_observation", harness_source)
        self.assertIn("harness.observe_battle_actor_hud()", harness_source)
        self.assertIn('"battle_hud_verdict": verdict', harness_source)

    def test_observe_battle_actor_hud_only_uses_the_zero_argument_project_probe(self) -> None:
        client = _ReadOnlyBattleHudClient()
        harness = _harness_with_client(client)

        observation = harness.observe_battle_actor_hud()

        self.assertEqual([(flow.PROBE_SCRIPT, [])], client.project_probe_calls)
        self.assertEqual(
            [
                ("connect",),
                ("is_in_pie",),
                ("run_project_python_file", flow.PROBE_SCRIPT, []),
            ],
            client.runtime_actions,
        )
        self.assertFalse(observation["ok"])
        self.assertIn("unit_hud_resource_screen_rect_missing", observation["battle_hud_verdict"]["errors"]["Player"])


class BattleHudScreenshotCropTest(unittest.TestCase):
    viewport = {"width": 1280.0, "height": 722.0}

    @staticmethod
    def _window_client_geometry() -> dict[str, object]:
        return {
            "hwnd": 42,
            "window_screen_rect": {"left": 100.0, "top": 200.0, "right": 1388.0, "bottom": 970.0},
            "client_screen_rect": {"left": 104.0, "top": 244.0, "right": 1384.0, "bottom": 966.0},
        }

    def test_crop_transform_rejects_unknown_or_missing_window_client_geometry(self) -> None:
        with self.assertRaisesRegex(RuntimeError, "screenshot_viewport_invalid"):
            flow._viewport_to_screenshot_transform(None, (1280, 722))
        with self.assertRaisesRegex(RuntimeError, "screenshot_window_geometry_missing"):
            flow._viewport_to_screenshot_transform(self.viewport, (1288, 770))

    def test_window_client_mapping_maps_the_known_outer_slate_image_to_client_content(self) -> None:
        transform = flow._viewport_to_screenshot_transform(
            self.viewport,
            (1288, 770),
            self._window_client_geometry(),
        )
        mapped = flow._map_viewport_rect_to_screenshot_rect(
            {"left": 0.0, "top": 0.0, "right": 1280.0, "bottom": 722.0},
            transform,
        )

        self.assertEqual("win32_preview_window_client", transform["source"])
        self.assertEqual(
            {"left": 4.0, "top": 44.0, "right": 1284.0, "bottom": 766.0},
            mapped,
        )
        self.assertEqual(1.0, transform["scale_x"])
        self.assertEqual(1.0, transform["scale_y"])
        self.assertEqual(self._window_client_geometry()["window_screen_rect"], transform["window_screen_rect"])
        self.assertEqual(self._window_client_geometry()["client_screen_rect"], transform["client_screen_rect"])

    def test_window_client_mapping_carries_dpi_scaling_through_outer_and_client_transforms(self) -> None:
        geometry = {
            "hwnd": 42,
            "window_screen_rect": {"left": 10.0, "top": 20.0, "right": 1010.0, "bottom": 520.0},
            "client_screen_rect": {"left": 20.0, "top": 40.0, "right": 1000.0, "bottom": 500.0},
        }
        transform = flow._viewport_to_screenshot_transform(
            {"width": 100.0, "height": 50.0},
            (2000, 1000),
            geometry,
        )
        mapped = flow._map_viewport_rect_to_screenshot_rect(
            {"left": 0.0, "top": 0.0, "right": 100.0, "bottom": 50.0},
            transform,
        )

        self.assertAlmostEqual(20.0, mapped["left"])
        self.assertAlmostEqual(40.0, mapped["top"])
        self.assertAlmostEqual(1980.0, mapped["right"])
        self.assertAlmostEqual(960.0, mapped["bottom"])
        self.assertAlmostEqual(19.6, transform["scale_x"])
        self.assertAlmostEqual(18.4, transform["scale_y"])
        self.assertAlmostEqual(2.0, transform["window_to_image_scale_x"])
        self.assertAlmostEqual(2.0, transform["window_to_image_scale_y"])

    def test_window_client_mapping_rejects_resize_between_capture_boundaries(self) -> None:
        geometry = {
            "geometry_stable": False,
            "window_geometry_before": self._window_client_geometry(),
            "window_geometry_after": {
                **self._window_client_geometry(),
                "window_screen_rect": {"left": 100.0, "top": 200.0, "right": 1400.0, "bottom": 970.0},
            },
        }

        with self.assertRaisesRegex(RuntimeError, "screenshot_window_geometry_unstable"):
            flow._viewport_to_screenshot_transform(self.viewport, (1288, 770), geometry)

    def test_identity_mapping_needs_no_window_geometry_when_image_equals_viewport(self) -> None:
        transform = flow._viewport_to_screenshot_transform(self.viewport, (1280, 722))
        mapped = flow._map_viewport_rect_to_screenshot_rect(
            {"left": 0.0, "top": 0.0, "right": 1280.0, "bottom": 722.0},
            transform,
        )

        self.assertEqual("identity_image_equals_viewport", transform["source"])
        self.assertEqual({"left": 0.0, "top": 0.0, "right": 1280.0, "bottom": 722.0}, mapped)

    def test_crop_helper_rejects_a_partially_offscreen_rect_instead_of_clamping_evidence(self) -> None:
        image = flow.Image.new("RGBA", (1280, 722), (20, 40, 60, 255))
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            source = root / "battle.png"
            crop = root / "hero.png"
            image.save(source)

            with self.assertRaisesRegex(RuntimeError, "screenshot_crop_rect_partially_offscreen"):
                flow._crop_screenshot_to_viewport_rect(
                    source,
                    (1280, 722),
                    self.viewport,
                    {"left": -10.0, "top": 700.0, "right": 25.0, "bottom": 750.0},
                    crop,
                )

    def test_crop_helper_rejects_an_invalid_or_fully_offscreen_rect(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            source = root / "battle.png"
            flow.Image.new("RGBA", (1280, 722), (20, 40, 60, 255)).save(source)

            with self.assertRaisesRegex(RuntimeError, "screenshot_crop_rect_invalid"):
                flow._crop_screenshot_to_viewport_rect(source, (1280, 722), self.viewport, {"left": 3.0, "top": 1.0, "right": 2.0, "bottom": 8.0}, root / "bad.png")
            with self.assertRaisesRegex(RuntimeError, "screenshot_crop_rect_outside_viewport"):
                flow._crop_screenshot_to_viewport_rect(source, (1280, 722), self.viewport, {"left": 1500.0, "top": 1.0, "right": 1600.0, "bottom": 8.0}, root / "outside.png")
            with self.assertRaisesRegex(RuntimeError, "screenshot_crop_rect_invalid"):
                flow._crop_screenshot_to_viewport_rect(source, (1280, 722), self.viewport, {"left": float("nan"), "top": 1.0, "right": 20.0, "bottom": 8.0}, root / "nan.png")


if __name__ == "__main__":
    unittest.main(verbosity=2)
