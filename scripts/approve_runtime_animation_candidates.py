#!/usr/bin/env python3
"""Record the user's approval for the candidate animations that are live in runtime.

The candidate manifests under
``SourceAssets/AnimationProduction/upgrade_20260827_corrected/`` were generated
with ``runtimeReplacementStatus: forbidden_pending_user_confirmation`` and a
pending review status. Commit 9b60b0a2 wired some of those animations into
runtime anyway, so the manifests and the shipped build disagreed.

On 2026-09-16 the user confirmed the runtime-wired candidates. This script
records that decision and nothing more:

* a candidate that is wired into runtime becomes approved;
* a candidate that is still unused keeps its pending status and gets an explicit
  note saying why, so the remaining gap stays visible instead of being papered
  over by a blanket approval;
* ``note`` fields written by this script are removed before re-adding, so the
  script is idempotent.

There are TWO manifest shapes and they must be handled differently:

* the aggregate ``upgrade_20260827_corrected/manifest.json`` lists every
  candidate under ``candidates`` (status ``review_pending``);
* each ``<candidate>/candidate_atlas/manifest.json`` describes ONE candidate
  (status ``candidate_review_only``) and has no ``candidates`` array.

Only the keys this script owns are touched. It never touches a .uasset, runtime
code, or any other manifest field.

Run with ``--check`` to report the intended change without writing.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CANDIDATE_ROOT = ROOT / "SourceAssets/AnimationProduction/upgrade_20260827_corrected"
AGGREGATE = CANDIDATE_ROOT / "manifest.json"

APPROVED_BY = "user"
APPROVED_AT = "2026-09-16"

# Candidate ids that are live in runtime, i.e. GameXXKBattleAnimationPresentation
# resolves them and its CorrectedClipTimings table carries a measured frame rate.
# Keyed by candidateId, not by replacementTargetSuggestion: the punch and kick
# manifests both suggest the generic target "character_00_hero_attack", which
# cannot distinguish them, and the hero idle candidate suggests
# "character_00_hero_idle" while the runtime names that same art
# "character_00_hero" idle. Keying on the candidate id keeps this unambiguous.
LIVE_CANDIDATE_IDS = {
    "character_00_hero_attack_punch_candidate",
    "character_00_hero_attack_kick_candidate",
    "character_00_hero_combat_idle_candidate",
    "candidate_yue_fire_idle",
    "enemy_01_rooster_idle_candidate",
    "enemy_01_rooster_attack_candidate",
    "enemy_03_weasel_idle_candidate",
    "enemy_03_weasel_attack_candidate",
    "enemy_05_ironfeather_idle_candidate",
    "enemy_07_graywolf_idle_candidate",
    "enemy_11_graymane_attack_candidate",
    "enemy_16_toad_idle_candidate",
    "enemy_18_deer_idle_candidate",
    "enemy_18_deer_attack_candidate",
}

# Pending vocabulary is shape-specific: the aggregate uses review_pending, the
# per-candidate manifests use candidate_review_only.
AGGREGATE_PENDING_STATUS = "review_pending"
CANDIDATE_PENDING_STATUS = "candidate_review_only"

# A shared battle-circle effect lives in this tree but is not one of the 27
# animation candidates the aggregate manifest governs, so this script leaves it
# entirely alone rather than guessing at its review state.
IGNORED_CANDIDATE_IDS = {"battle_circle_effect_01"}

PENDING_NOTE = (
    "Not wired into runtime yet, so the runtime replacement authorisation is "
    "still unconfirmed for this candidate."
)


def apply_approval(target: dict) -> None:
    target.pop("note", None)
    target["status"] = "approved"
    target["atlasStatus"] = "built_and_imported"
    target["runtimeReplacementStatus"] = "approved"
    target["approvedBy"] = APPROVED_BY
    target["approvedAt"] = APPROVED_AT


def apply_pending(target: dict, pending_status: str) -> None:
    target.pop("note", None)
    target["status"] = pending_status
    target["runtimeReplacementStatus"] = "forbidden_pending_user_confirmation"
    target["note"] = PENDING_NOTE


def process_aggregate(path: Path, write: bool) -> tuple[int, int]:
    data = json.loads(path.read_text(encoding="utf-8"))
    candidates = data.get("candidates")
    if not isinstance(candidates, list):
        raise SystemExit(f"{path} has no candidates array; refusing to touch it")

    approved = pending = 0
    for candidate in candidates:
        if candidate.get("candidateId", "") in LIVE_CANDIDATE_IDS:
            apply_approval(candidate)
            approved += 1
        else:
            apply_pending(candidate, AGGREGATE_PENDING_STATUS)
            pending += 1

    # The aggregate's own status only becomes approved when nothing is pending.
    data.pop("note", None)
    if pending == 0 and approved > 0:
        apply_approval(data)
    else:
        apply_pending(data, AGGREGATE_PENDING_STATUS)
        data["note"] = (
            f"{approved} of {approved + pending} candidates are approved and live in "
            "runtime; the rest are still unwired, so the overall replacement "
            "authorisation stays unconfirmed."
        )

    if write:
        path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return approved, pending


def process_candidate_manifest(path: Path, write: bool) -> tuple[int, int]:
    data = json.loads(path.read_text(encoding="utf-8"))
    if data.get("candidateId", "") in LIVE_CANDIDATE_IDS:
        apply_approval(data)
        approved, pending = 1, 0
    else:
        apply_pending(data, CANDIDATE_PENDING_STATUS)
        approved, pending = 0, 1

    if write:
        path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return approved, pending


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true", help="report without writing")
    args = parser.parse_args()
    write = not args.check
    verb = "would approve" if args.check else "approved"

    if not AGGREGATE.is_file():
        raise SystemExit(f"missing aggregate manifest: {AGGREGATE}")

    agg_approved, agg_pending = process_aggregate(AGGREGATE, write)
    print(f"{verb}: aggregate candidates approved={agg_approved} pending={agg_pending}")

    per_approved = per_pending = per_skipped = 0
    for path in sorted(CANDIDATE_ROOT.rglob("candidate_atlas/manifest.json")):
        data = json.loads(path.read_text(encoding="utf-8"))
        if data.get("candidateId", "") in IGNORED_CANDIDATE_IDS:
            per_skipped += 1
            continue
        a, p = process_candidate_manifest(path, write)
        per_approved += a
        per_pending += p
    print(f"{verb}: per-candidate manifests approved={per_approved} pending={per_pending}"
          + (f" skipped={per_skipped}" if per_skipped else ""))

    if per_approved != agg_approved or per_pending != agg_pending:
        print("WARNING: aggregate and per-candidate counts disagree "
              f"({agg_approved}/{agg_pending} vs {per_approved}/{per_pending})")
    else:
        print(f"consistent: {agg_approved} approved, {agg_pending} pending")

    if args.check:
        print("(check mode: nothing written)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
