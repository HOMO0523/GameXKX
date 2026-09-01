from __future__ import annotations

import argparse
import json
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable


@dataclass(frozen=True)
class GuideCatalogSnapshot:
    target_ids: frozenset[str]
    trigger_event_ids: frozenset[str]
    completion_event_ids: frozenset[str]
    action_ids: frozenset[str]


DEFAULT_CATALOGS = GuideCatalogSnapshot(
    target_ids=frozenset(
        {
            "Route.Tutorial.NextNode",
            "Route.Settlement.Confirm",
            "Battle.Hud.PartyQi",
            "Battle.Hand.FirstPlayableTargetedCard",
            "Battle.Enemy.FirstLegalTarget",
            "Battle.EndTurn",
            "Route.Merchant.CardRow",
            "Route.Merchant.RelicRow",
            "Route.Merchant.Leave",
            "Route.Event.ValidChoiceGroup",
            "Route.Camp.Heal",
            "Route.Camp.Gold",
            "Route.Chest.Open",
            "Desktop.Settings.ResetCombatGuide",
            "Desktop.Tab",
            "Desktop.Training",
            "Desktop.Training.Difficulty.Normal",
            "Desktop.Training.Stage.Normal.1-1",
            "Desktop.Training.Travel",
            "Desktop.Training.TravelStrip",
        }
    ),
    trigger_event_ids=frozenset(
        {
            "Event.Route.Opened",
            "Event.RouteMap.Opened",
            "Event.Battle.Opened",
            "Event.Merchant.Opened",
            "Event.Route.EventOpened",
            "Event.Route.CampOpened",
            "Event.Route.ChestOpened",
            "Event.Boss.Opened",
            "Event.Settlement.Opened",
            "Event.Desktop.FirstJourney.Started",
            "Event.Desktop.Tab.Expanded",
            "Event.Desktop.Training.Opened",
            "Event.Desktop.Training.Difficulty.NormalSelected",
            "Event.Desktop.Training.Stage.Normal.1-1.Selected",
            "Event.Desktop.Training.Travel.Started",
        }
    ),
    completion_event_ids=frozenset(
        {
            "Event.Route.NextNodeSelected",
            "Event.Battle.TargetedCardSelected",
            "Event.Battle.LegalTargetSelected",
            "Event.Battle.CardResolved",
            "Event.Battle.EndTurnResolved",
            "Event.Merchant.CardPurchased",
            "Event.Merchant.RelicPurchased",
            "Event.Merchant.Left",
            "Event.Route.EventChoiceResolved",
            "Event.Route.CampHealResolved",
            "Event.Route.CampGoldResolved",
            "Event.Route.CampResolved",
            "Event.Route.ChestRewardResolved",
            "Event.Boss.Completed",
            "Event.Settlement.Confirmed",
            "Event.Guide.Done",
            "Event.Desktop.Tab.Expanded",
            "Event.Desktop.Training.Opened",
            "Event.Desktop.Training.Difficulty.NormalSelected",
            "Event.Desktop.Training.Stage.Normal.1-1.Selected",
            "Event.Desktop.Training.Travel.Started",
            "Event.Desktop.Training.EncounterCompleted",
        }
    ),
    action_ids=frozenset(
        {
            "Action.Route.SelectNext",
            "Action.Battle.SelectTargetedCard",
            "Action.Battle.SelectLegalTarget",
            "Action.Battle.CommitCard",
            "Action.Battle.EndTurn",
            "Action.Merchant.PurchaseCard",
            "Action.Merchant.PurchaseRelic",
            "Action.Merchant.Leave",
            "Action.Route.EventChoose",
            "Action.Route.CampHeal",
            "Action.Route.CampGold",
            "Action.Route.ChestOpen",
            "Action.Route.SettlementConfirm",
            "Action.Desktop.ResetCombatGuide",
            "Action.Desktop.Tab",
            "Action.Desktop.Training",
            "Action.Desktop.Training.Difficulty.Normal",
            "Action.Desktop.Training.Stage.Normal.1-1",
            "Action.Desktop.Training.Travel",
        }
    ),
)


def _nonempty(value: object) -> bool:
    return isinstance(value, str) and bool(value.strip())


def _reachable(entry: str, adjacency: dict[str, str | None]) -> set[str]:
    reached: set[str] = set()
    cursor: str | None = entry
    while cursor is not None and cursor in adjacency and cursor not in reached:
        reached.add(cursor)
        cursor = adjacency[cursor]
    return reached


# Tutorial 0-1 vocabulary is Guide-owned. Keep it in the Guide catalog snapshot
# instead of borrowing the Dialogue-only runtime-catalog.json.
DEFAULT_CATALOGS = GuideCatalogSnapshot(
    target_ids=DEFAULT_CATALOGS.target_ids
    | frozenset(
        {
            "Battle.Unit.Hero.Health",
            "Battle.Unit.Hero.Mana",
            "Battle.Enemy.Intent",
            "Battle.Hand.HengJianShouShi",
            "Battle.Hand.SuiYanJi",
            "Battle.Hand.FengShenBu",
            "Battle.Unit.Hero.Target",
            "Battle.Unit.Enemy.Target",
            "Battle.Unit.YueBai.Visual",
            "Battle.Pending.ForcedDiscard",
            "Battle.AutoBattle",
        }
    ),
    trigger_event_ids=DEFAULT_CATALOGS.trigger_event_ids,
    completion_event_ids=DEFAULT_CATALOGS.completion_event_ids
    | frozenset(
        {
            "Event.Tutorial01.Continue",
            "Event.Tutorial01.HengJianResolved",
            "Event.Tutorial01.SuiYanResolved",
            "Event.Tutorial01.FengShenForcedDiscardOpened",
            "Event.Tutorial01.ForcedDiscardResolved",
            "Event.Tutorial01.PlayerTurnReady",
            "Event.Tutorial01.AutoBattleEnabled",
        }
    ),
    action_ids=DEFAULT_CATALOGS.action_ids
    | frozenset(
        {
            "Action.Guide.Continue",
            "Action.Battle.SelectCard.HengJianShouShi",
            "Action.Battle.SelectCard.SuiYanJi",
            "Action.Battle.SelectCard.FengShenBu",
            "Action.Battle.SelectTarget.Hero",
            "Action.Battle.SelectTarget.Enemy",
            "Action.Battle.SubmitForcedDiscard",
            "Action.Battle.EnableAuto",
        }
    ),
)


def validate_guide(payload: dict, catalogs: GuideCatalogSnapshot) -> list[str]:
    if not isinstance(payload, dict):
        return ["guide root must be an object"]

    errors: list[str] = []
    allowed_root = {"schemaVersion", "guideId", "guideVersion", "entryStep", "steps"}
    for key in payload:
        if key not in allowed_root:
            errors.append(f"unknown root field {key}")
    if payload.get("schemaVersion") != 1:
        errors.append("schemaVersion must be 1")
    if not _nonempty(payload.get("guideId")):
        errors.append("guideId must not be empty")
    if not isinstance(payload.get("guideVersion"), int) or payload["guideVersion"] <= 0:
        errors.append("guideVersion must be a positive integer")

    steps = payload.get("steps")
    if not isinstance(steps, dict) or not steps:
        return sorted(set(errors + ["steps must be a non-empty object"]))
    entry = payload.get("entryStep")
    if not _nonempty(entry) or entry not in steps:
        errors.append(f"entry step does not exist: {entry!r}")

    allowed_step_fields = {
        "triggerEvent",
        "target",
        "additionalTargets",
        "bubbleAnchor",
        "missingTargetPolicy",
        "inputPolicy",
        "text",
        "allowedActions",
        "completionEvent",
        "next",
    }
    adjacency: dict[str, str | None] = {}
    terminal_count = 0
    for step_id, step in steps.items():
        if not _nonempty(step_id) or not isinstance(step, dict):
            errors.append("step IDs must be non-empty and step values objects")
            continue
        for key in step:
            if key not in allowed_step_fields:
                errors.append(f"{step_id}: unknown field {key}")

        trigger_event = step.get("triggerEvent")
        if trigger_event not in catalogs.trigger_event_ids:
            errors.append(f"{step_id}: unknown trigger event {trigger_event}")
        completion_event = step.get("completionEvent")
        if completion_event not in catalogs.completion_event_ids:
            errors.append(f"{step_id}: unknown completion event {completion_event}")

        policy = step.get("inputPolicy")
        if policy not in {"soft", "forced"}:
            errors.append(f"{step_id}: inputPolicy must be soft or forced")
        target = step.get("target")
        if _nonempty(target) and target not in catalogs.target_ids:
            errors.append(f"{step_id}: unknown target {target}")
        if not isinstance(target, str):
            errors.append(f"{step_id}: target must be a string")

        additional_targets = step.get("additionalTargets", [])
        if not isinstance(additional_targets, list):
            errors.append(f"{step_id}: additionalTargets must be an array")
            additional_targets = []
        elif any(not _nonempty(value) for value in additional_targets):
            errors.append(
                f"{step_id}: additionalTargets must contain non-empty IDs"
            )
        elif len(additional_targets) != len(set(additional_targets)):
            errors.append(f"{step_id}: additionalTargets must be unique")
        if _nonempty(target) and target in additional_targets:
            errors.append(f"{step_id}: focus targets must be unique")
        for additional_target in additional_targets:
            if (
                _nonempty(additional_target)
                and additional_target not in catalogs.target_ids
            ):
                errors.append(
                    f"{step_id}: unknown additional target {additional_target}"
                )

        bubble_anchor = step.get("bubbleAnchor")
        if bubble_anchor is not None and not _nonempty(bubble_anchor):
            errors.append(f"{step_id}: bubbleAnchor must be a non-empty ID")
        elif _nonempty(bubble_anchor) and bubble_anchor not in catalogs.target_ids:
            errors.append(f"{step_id}: unknown bubble anchor {bubble_anchor}")

        missing_target_policy = step.get("missingTargetPolicy", "skip")
        if missing_target_policy not in {"skip", "abort"}:
            errors.append(
                f"{step_id}: missingTargetPolicy must be skip or abort"
            )
        if policy == "forced" and not _nonempty(target) and not additional_targets:
            errors.append(f"{step_id}: forced target must not be empty")
        if not _nonempty(step.get("text")):
            errors.append(f"{step_id}: text must not be empty")

        actions = step.get("allowedActions")
        if not isinstance(actions, list):
            errors.append(f"{step_id}: allowedActions must be an array")
            actions = []
        elif any(not _nonempty(action) for action in actions):
            errors.append(f"{step_id}: allowedActions must contain non-empty IDs")
        elif len(actions) != len(set(actions)):
            errors.append(f"{step_id}: allowedActions must be unique")
        for action_id in actions:
            if _nonempty(action_id) and action_id not in catalogs.action_ids:
                errors.append(f"{step_id}: unknown allowed action {action_id}")
        if policy == "forced" and not actions:
            errors.append(f"{step_id}: forced step requires an allowed action")

        next_step = step.get("next")
        if next_step is None:
            adjacency[step_id] = None
            terminal_count += 1
        elif not _nonempty(next_step):
            errors.append(f"{step_id}: next must be a non-empty step ID")
            adjacency[step_id] = None
        else:
            adjacency[step_id] = next_step
            if next_step not in steps:
                errors.append(f"{step_id}: missing target {next_step}")

    if terminal_count != 1:
        errors.append("guide must contain exactly one terminal step")
    if isinstance(entry, str) and entry in adjacency:
        reachable = _reachable(entry, adjacency)
        for step_id in sorted(set(steps) - reachable):
            errors.append(f"unreachable step: {step_id}")
        cursor: str | None = entry
        visited: set[str] = set()
        while cursor is not None and cursor in adjacency:
            if cursor in visited:
                errors.append(f"guide contains a cycle at step {cursor}")
                break
            visited.add(cursor)
            cursor = adjacency[cursor]

    return sorted(set(errors))


def canonicalize_guide(payload: dict) -> dict:
    return json.loads(json.dumps(payload, ensure_ascii=False, sort_keys=True))


def _reject_duplicate_keys(pairs: Iterable[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def validate_file(path: Path, catalogs: GuideCatalogSnapshot = DEFAULT_CATALOGS) -> dict:
    payload = json.loads(
        path.read_text(encoding="utf-8"), object_pairs_hook=_reject_duplicate_keys
    )
    errors = validate_guide(payload, catalogs)
    if errors:
        raise ValueError("\n".join(errors))
    return canonicalize_guide(payload)


def load_catalog(path: Path) -> GuideCatalogSnapshot:
    payload = json.loads(path.read_text(encoding="utf-8"))
    return GuideCatalogSnapshot(
        target_ids=frozenset(payload.get("targetIds", [])),
        trigger_event_ids=frozenset(payload.get("triggerEventIds", [])),
        completion_event_ids=frozenset(payload.get("completionEventIds", [])),
        action_ids=frozenset(payload.get("actionIds", [])),
    )


def _collect_sources(paths: list[Path]) -> list[Path]:
    sources: list[Path] = []
    for path in paths:
        sources.extend(sorted(path.rglob("*.guide.json")) if path.is_dir() else [path])
    return sorted(set(sources))


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate GameXXK semantic guide JSON.")
    parser.add_argument("paths", nargs="+", type=Path)
    parser.add_argument("--catalog", type=Path)
    args = parser.parse_args()
    catalogs = load_catalog(args.catalog) if args.catalog else DEFAULT_CATALOGS
    results = []
    for path in _collect_sources(args.paths):
        payload = validate_file(path, catalogs)
        results.append({"path": str(path), "guideId": payload["guideId"], "ok": True})
    print(json.dumps({"ok": True, "validated": results}, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
