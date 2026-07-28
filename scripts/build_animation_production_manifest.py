#!/usr/bin/env python3
"""Select the affordable four-unit-action plus three-effect production scope."""

from __future__ import annotations

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PROMPT_MANIFEST = (
    ROOT / "SourceAssets/AnimationProduction/prompts_v1/generation_manifest.json"
)
OUTPUT_ROOT = ROOT / "SourceAssets/AnimationProduction/production_v1"
PILOT_ROOT = ROOT / "SourceAssets/AnimationProduction/pilot_v1"
SELECTED_ACTIONS = {"idle", "attack", "hit", "death"}

GENERIC_EFFECTS = (
    ("status_buff_generic", "status_buff_generic.txt"),
    ("status_debuff_generic", "status_debuff_generic.txt"),
    ("impact_ink_generic", "impact_ink_generic.txt"),
)


def build() -> dict[str, object]:
    source = json.loads(PROMPT_MANIFEST.read_text(encoding="utf-8"))
    entries: list[dict[str, object]] = []
    for source_entry in source["entries"]:
        if source_entry["action"] not in SELECTED_ACTIONS:
            continue
        entry = dict(source_entry)
        entry["kind"] = "unit_action"
        entry["submission_count"] = 0
        entry["status"] = "not_submitted"
        entries.append(entry)

    blank = "SourceAssets/AnimationProduction/pilot_v1/inputs/magenta_blank_720.png"
    for effect_id, prompt_name in GENERIC_EFFECTS:
        entries.append(
            {
                "sequence": len(entries) + 1,
                "asset_id": effect_id,
                "kind": "generic_effect",
                "side": "neutral",
                "facing": "center",
                "action": "effect",
                "first_frame": blank,
                "last_frame": blank,
                "prompt_file": (
                    f"SourceAssets/AnimationProduction/pilot_v1/prompts/{prompt_name}"
                ),
                "model": "seedance1.5pro",
                "resolution": "720p",
                "duration_seconds": 5,
                "expected_credits": 40,
                "max_submissions": 1,
                "automatic_retry": False,
                "submission_count": 0,
                "status": "not_submitted",
            }
        )

    for index, entry in enumerate(entries, start=1):
        entry["sequence"] = index

    credit_total = sum(int(entry["expected_credits"]) for entry in entries)
    manifest: dict[str, object] = {
        "schema_version": 1,
        "production_id": "battle_animation_production_v1",
        "buff_runtime_policy": "idle_plus_generic_overlay",
        "unit_actions": ["idle", "attack", "hit", "death"],
        "unit_count": 34,
        "entry_count": len(entries),
        "expected_credit_total": credit_total,
        "max_submissions_per_asset": 1,
        "automatic_retry": False,
        "entries": entries,
    }
    OUTPUT_ROOT.mkdir(parents=True, exist_ok=True)
    (OUTPUT_ROOT / "manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    ledger: dict[str, object] = {
        "schema_version": 1,
        "production_id": manifest["production_id"],
        "starting_credit_snapshot": 6870,
        "reserved_credit_total": credit_total,
        "projected_credit_after_single_pass": 6870 - credit_total,
        "spent_credit_total": 0,
        "submitted_count": 0,
        "successful_count": 0,
        "failed_count": 0,
        "pending_count": 0,
        "assets": {
            entry["asset_id"]: {
                "sequence": entry["sequence"],
                "status": "not_submitted",
                "submission_count": 0,
                "expected_credits": entry["expected_credits"],
            }
            for entry in entries
        },
    }
    (OUTPUT_ROOT / "ledger.json").write_text(
        json.dumps(ledger, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    return manifest


if __name__ == "__main__":
    result = build()
    print(
        json.dumps(
            {
                "entry_count": result["entry_count"],
                "expected_credit_total": result["expected_credit_total"],
            },
            indent=2,
        )
    )
