#!/usr/bin/env python3
"""Run the Training strip probe without blocking Unreal's game thread."""

from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path
from typing import Any


SCRIPT_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPT_DIR))

from ue_mcp_client import UnrealMCPClient  # noqa: E402


PROBE = "Content/Python/gamexxk_probe_training_visual_mvp.py"
LANE_TILE_WIDTH = 750.0


def _payload(result: dict[str, Any]) -> dict[str, Any]:
    stdout = str(result.get("stdout", "")).strip()
    if not stdout:
        raise RuntimeError(f"probe returned no stdout: {result}")
    parsed = json.loads(stdout.splitlines()[-1])
    if not isinstance(parsed, dict):
        raise RuntimeError(f"probe returned non-object JSON: {parsed!r}")
    return parsed


def _run_phase(client: UnrealMCPClient, phase: str, *extra: str) -> dict[str, Any]:
    result = client.run_project_python_file(PROBE, ["--phase", phase, *extra])
    payload = _payload(result)
    if not payload.get("ok"):
        raise RuntimeError(f"{phase} failed: {payload}")
    return payload


def _connect_or_launch(args: argparse.Namespace) -> UnrealMCPClient:
    client = UnrealMCPClient(host=args.mcp_host, port=args.mcp_port, timeout=45.0)
    if client.connect():
        return client
    if not args.launch_editor:
        raise RuntimeError(
            f"UE MCP is unavailable at {client.endpoint}; run with --launch-editor "
            "to launch the exact GameXXK.uproject"
        )

    from ue_tdd_pipeline import (  # noqa: E402
        build_project,
        kill_editor,
        launch_editor,
        save_running_editor_before_close,
        wait_for_mcp,
    )

    if not save_running_editor_before_close(host=args.mcp_host, port=args.mcp_port):
        raise RuntimeError("could not safely save the running GameXXK editor")
    if not kill_editor():
        raise RuntimeError("could not safely close the running GameXXK editor")
    if args.build and not build_project():
        raise RuntimeError("cold GameXXKEditor build failed")
    if launch_editor(mcp_port=args.mcp_port) is None:
        raise RuntimeError("failed to launch GameXXK.uproject")
    launched = wait_for_mcp(host=args.mcp_host, port=args.mcp_port, timeout=180.0)
    if launched is None:
        raise RuntimeError("GameXXK editor launched but UE MCP did not become ready")
    return launched


def _wrapped_scroll_delta(previous: float, current: float, width: float = LANE_TILE_WIDTH) -> float:
    delta = current - previous
    if delta < -width * 0.5:
        delta += width
    elif delta > width * 0.5:
        delta -= width
    return delta


def run(args: argparse.Namespace) -> dict[str, Any]:
    client = _connect_or_launch(args)
    if client.is_in_pie():
        client.stop_pie()
        if not client.wait_for_pie_state(False):
            raise RuntimeError("existing PIE session did not stop")

    prepared = _run_phase(client, "prepare-map")
    client.start_pie(warmup_seconds=1.0)
    if not client.wait_for_pie_state(True):
        raise RuntimeError("HUD-map PIE session did not start")

    try:
        started = _run_phase(client, "start-travel", "--stage", args.stage)

        # Sampling waits intentionally stay outside UE. While this process
        # sleeps, Slate and the workbench NativeTick keep advancing normally.
        sample_interval = max(0.05, args.sample_interval)
        sample_count = max(1, args.samples)
        samples: list[dict[str, Any]] = []
        previous = started
        capture_payload: dict[str, Any] | None = None
        capture_phases = {"EncounterIdle", "HeroAttack", "EnemyHit", "EnemyDeath"}
        for sample_index in range(sample_count):
            time.sleep(sample_interval)
            sample = _run_phase(client, "observe")
            sample["sample_index"] = sample_index
            sample["elapsed_seconds"] = round((sample_index + 1) * sample_interval, 4)
            sample["scroll_delta"] = _wrapped_scroll_delta(
                float(previous.get("scroll_offset", 0.0)),
                float(sample.get("scroll_offset", 0.0)),
            )
            sample["native_tick_delta"] = int(sample.get("native_tick_count", 0)) - int(
                previous.get("native_tick_count", 0)
            )
            samples.append(sample)
            previous = sample
            if (
                args.capture
                and capture_payload is None
                and str(sample.get("visual_phase", "")) in capture_phases
            ):
                capture_payload = _run_phase(client, "observe", "--capture")

        observed = samples[-1]
        if args.capture and capture_payload is None:
            capture_payload = _run_phase(client, "observe", "--capture")

        errors: list[str] = []
        if not started.get("selected_stage"):
            errors.append("stage_not_selected")
        if not started.get("travel_started"):
            errors.append("travel_not_started")
        if not observed.get("strip"):
            errors.append("visual_strip_missing")
        if int(observed.get("native_tick_count", 0)) <= int(started.get("native_tick_count", 0)):
            errors.append("native_tick_did_not_advance")
        walking_samples = [sample for sample in samples if sample.get("visual_phase") == "Walking"]
        if not any(float(sample.get("scroll_delta", 0.0)) > 0.01 for sample in walking_samples):
            errors.append("walking_scroll_did_not_advance")
        if not any(int(sample.get("walk_frame", 0)) != int(started.get("walk_frame", 0)) for sample in walking_samples):
            errors.append("walk_frame_did_not_advance")
        zero_walking_run = 0
        maximum_zero_walking_run = 0
        for sample in walking_samples:
            if abs(float(sample.get("scroll_delta", 0.0))) <= 0.01:
                zero_walking_run += 1
                maximum_zero_walking_run = max(maximum_zero_walking_run, zero_walking_run)
            else:
                zero_walking_run = 0
        if maximum_zero_walking_run >= 3:
            errors.append("walking_scroll_has_unexplained_zero_plateau")
        visual_phases = {str(sample.get("visual_phase", "")) for sample in samples}
        hero_actions = {str(sample.get("hero_action", "")) for sample in samples}
        party_actions = [
            {
                str((sample.get("party_actions") or ["", "", ""])[party_index])
                for sample in samples
                if len(sample.get("party_actions") or []) > party_index
            }
            for party_index in range(3)
        ]
        enemy_actions = {str(sample.get("enemy_action", "")) for sample in samples}
        if "EncounterIdle" not in visual_phases:
            errors.append("encounter_idle_not_observed")
        if "HeroAttack" not in visual_phases or "Attack" not in hero_actions:
            errors.append("hero_attack_not_observed")
        for party_index, actions in enumerate(party_actions):
            if "Attack" not in actions:
                errors.append(f"party_{party_index}_attack_not_observed")
        if "EnemyHit" not in visual_phases or "Hit" not in enemy_actions:
            errors.append("enemy_hit_not_observed")
        if "EnemyDeath" not in visual_phases or "Death" not in enemy_actions:
            errors.append("enemy_death_not_observed")
        if not any(bool(sample.get("enemy_visible")) for sample in samples):
            errors.append("enemy_never_visible")
        if any(
            bool(sample.get("enemy_visible")) and not str(sample.get("enemy_id", ""))
            for sample in samples
        ):
            errors.append("visible_enemy_missing_identity")
        if any(
            not 0.0 <= float(sample.get(key, 0.0)) <= 1.0
            for sample in samples
            for key in ("hero_hp_fraction", "enemy_hp_fraction")
        ):
            errors.append("health_fraction_out_of_range")
        if any(
            len(sample.get("party_hp_fractions") or []) != 3
            or any(not 0.0 <= float(value) <= 1.0 for value in sample.get("party_hp_fractions", []))
            for sample in samples
        ):
            errors.append("party_health_fraction_out_of_range")
        if not observed.get("atlas"):
            errors.append("walk_atlas_missing")
        if not observed.get("background"):
            errors.append("loop_background_missing")

        advanced = _run_phase(client, "advance")
        return {
            "ok": not errors,
            "probe": PROBE,
            "prepared": prepared,
            "started": started,
            "observed": observed,
            "samples": samples,
            "sample_interval": sample_interval,
            "maximum_zero_walking_run": maximum_zero_walking_run,
            "observed_visual_phases": sorted(visual_phases),
            "observed_hero_actions": sorted(hero_actions),
            "observed_party_actions": [sorted(actions) for actions in party_actions],
            "observed_enemy_actions": sorted(enemy_actions),
            "capture": capture_payload,
            "advanced": advanced,
            "errors": errors,
        }
    finally:
        if not args.keep_pie and client.is_in_pie():
            client.stop_pie()
            client.wait_for_pie_state(False)


def main(argv=None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--mcp-host", default="127.0.0.1")
    parser.add_argument("--mcp-port", type=int, default=18765)
    parser.add_argument("--stage", default="Training.Normal.1-1")
    parser.add_argument("--observe-seconds", type=float, default=0.35, help=argparse.SUPPRESS)
    parser.add_argument("--samples", type=int, default=60)
    parser.add_argument("--sample-interval", type=float, default=0.1)
    parser.add_argument("--launch-editor", action="store_true")
    parser.add_argument("--build", action="store_true")
    parser.add_argument("--capture", action="store_true")
    parser.add_argument("--keep-pie", action="store_true")
    parser.add_argument("--output")
    args = parser.parse_args(argv)

    try:
        report = run(args)
    except Exception as exc:
        report = {"ok": False, "probe": PROBE, "errors": [str(exc)]}
    rendered = json.dumps(report, ensure_ascii=False, indent=2, sort_keys=True)
    if args.output:
        output_path = Path(args.output).resolve()
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(rendered + "\n", encoding="utf-8")
        report["output"] = str(output_path)
        rendered = json.dumps(report, ensure_ascii=False, indent=2, sort_keys=True)
    print(rendered)
    return 0 if report.get("ok") else 1


if __name__ == "__main__":
    raise SystemExit(main())
