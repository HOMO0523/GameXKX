from __future__ import annotations

import argparse
import json
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable


REGISTERED_NODE_TYPES = frozenset({"line", "choice", "end"})
REGISTERED_PRESENTATIONS = frozenset({"bubble", "dialogue"})
REGISTERED_CONDITION_TYPES = frozenset(
    {
        "flag",
        "tutorialState",
        "taskState",
        "itemAtLeast",
        "goldAtLeast",
        "companionUnlocked",
        "optionSelected",
        "nodeSeen",
    }
)


@dataclass(frozen=True)
class CatalogSnapshot:
    speakers: frozenset[str]
    roles: frozenset[str]
    outcomes: frozenset[str]


def _is_nonempty_string(value: object) -> bool:
    return isinstance(value, str) and bool(value.strip())


def _condition_errors(owner_id: str, conditions: object) -> list[str]:
    if conditions is None:
        return []
    if not isinstance(conditions, dict):
        return [f"{owner_id}: conditions must be an object"]
    errors: list[str] = []
    for condition_type, value in conditions.items():
        if condition_type not in REGISTERED_CONDITION_TYPES:
            errors.append(f"{owner_id}: unknown condition {condition_type}")
            continue
        if not _is_nonempty_string(value):
            errors.append(f"{owner_id}: condition {condition_type} requires a non-empty string")
            continue
        assert isinstance(value, str)
        if condition_type == "tutorialState" and value not in {
            "NotStarted",
            "Active",
            "Completed",
        }:
            errors.append(f"{owner_id}: invalid tutorialState {value}")
        elif condition_type in {"taskState", "itemAtLeast"} and not re.fullmatch(
            r"[^:]+:[0-9]+", value
        ):
            errors.append(f"{owner_id}: {condition_type} must use Id:nonnegative-integer")
        elif condition_type == "goldAtLeast" and not value.isdecimal():
            errors.append(f"{owner_id}: goldAtLeast must be a nonnegative integer")
    return errors


def _node_targets(node: dict[str, Any]) -> set[str]:
    node_type = node.get("type")
    if node_type == "line":
        target = node.get("next")
        return {target} if isinstance(target, str) and target else set()
    if node_type == "choice":
        options = node.get("options")
        if not isinstance(options, list):
            return set()
        return {
            option.get("next")
            for option in options
            if isinstance(option, dict)
            and isinstance(option.get("next"), str)
            and option["next"]
        }
    return set()


def _reachable_nodes(entry: str, adjacency: dict[str, set[str]]) -> set[str]:
    reached: set[str] = set()
    pending = [entry]
    while pending:
        node_id = pending.pop()
        if node_id in reached or node_id not in adjacency:
            continue
        reached.add(node_id)
        pending.extend(adjacency[node_id] - reached)
    return reached


def _strongly_connected_components(adjacency: dict[str, set[str]]) -> list[set[str]]:
    next_index = 0
    indices: dict[str, int] = {}
    lowlinks: dict[str, int] = {}
    stack: list[str] = []
    on_stack: set[str] = set()
    components: list[set[str]] = []

    def visit(node_id: str) -> None:
        nonlocal next_index
        indices[node_id] = next_index
        lowlinks[node_id] = next_index
        next_index += 1
        stack.append(node_id)
        on_stack.add(node_id)

        for target in sorted(adjacency[node_id]):
            if target not in adjacency:
                continue
            if target not in indices:
                visit(target)
                lowlinks[node_id] = min(lowlinks[node_id], lowlinks[target])
            elif target in on_stack:
                lowlinks[node_id] = min(lowlinks[node_id], indices[target])

        if lowlinks[node_id] != indices[node_id]:
            return
        component: set[str] = set()
        while stack:
            member = stack.pop()
            on_stack.remove(member)
            component.add(member)
            if member == node_id:
                break
        components.append(component)

    for node_id in sorted(adjacency):
        if node_id not in indices:
            visit(node_id)
    return components


def _exitless_cycle_errors(
    nodes: dict[str, dict[str, Any]], adjacency: dict[str, set[str]]
) -> list[str]:
    errors: list[str] = []
    for component in _strongly_connected_components(adjacency):
        cyclic = len(component) > 1 or any(
            node_id in adjacency[node_id] for node_id in component
        )
        if not cyclic:
            continue
        has_choice_exit = any(
            nodes[node_id].get("type") == "choice"
            and any(target not in component for target in adjacency[node_id])
            for node_id in component
        )
        if not has_choice_exit:
            errors.append(f"exitless cycle: {', '.join(sorted(component))}")
    return errors


def validate_dialogue(payload: dict, catalogs: CatalogSnapshot) -> list[str]:
    errors: list[str] = []
    if not isinstance(payload, dict):
        return ["dialogue root must be an object"]
    if payload.get("schemaVersion") != 1:
        errors.append("schemaVersion must be 1")
    if not _is_nonempty_string(payload.get("dialogueId")):
        errors.append("dialogueId must not be empty")
    if not isinstance(payload.get("dialogueVersion"), int) or payload["dialogueVersion"] <= 0:
        errors.append("dialogueVersion must be a positive integer")

    nodes = payload.get("nodes")
    if not isinstance(nodes, dict) or not nodes:
        return sorted(set(errors + ["nodes must be a non-empty object"]))
    if not all(_is_nonempty_string(node_id) and isinstance(node, dict) for node_id, node in nodes.items()):
        errors.append("node IDs must be non-empty and node values must be objects")

    entry = payload.get("entryNode")
    if entry not in nodes:
        errors.append(f"entry node does not exist: {entry!r}")

    adjacency: dict[str, set[str]] = {str(node_id): set() for node_id in nodes}
    seen_option_ids: set[str] = set()
    seen_outcome_ids: set[str] = set()

    def validate_outcome(owner_id: str, outcome: object) -> None:
        if not _is_nonempty_string(outcome):
            errors.append(f"{owner_id}: outcome id must not be empty")
            return
        assert isinstance(outcome, str)
        if outcome in seen_outcome_ids:
            errors.append(f"{owner_id}: duplicate outcome id {outcome}")
        seen_outcome_ids.add(outcome)
        if outcome not in catalogs.outcomes:
            errors.append(f"{owner_id}: unknown outcome {outcome}")

    for node_id, raw_node in nodes.items():
        if not isinstance(node_id, str) or not isinstance(raw_node, dict):
            continue
        node = raw_node
        node_type = node.get("type")
        if node_type not in REGISTERED_NODE_TYPES:
            errors.append(f"{node_id}: unknown node type {node_type!r}")
            continue
        targets = _node_targets(node)
        adjacency[node_id].update(targets)
        for target in sorted(targets):
            if target not in nodes:
                errors.append(f"{node_id}: missing target {target}")
        errors.extend(_condition_errors(node_id, node.get("conditions")))

        if node_type == "line":
            presentation = node.get("presentation")
            if presentation not in REGISTERED_PRESENTATIONS:
                errors.append(f"{node_id}: invalid presentation {presentation!r}")
            speaker = node.get("speaker")
            if speaker not in catalogs.speakers and speaker not in catalogs.roles:
                errors.append(f"{node_id}: unknown speaker or role {speaker}")
            if not _is_nonempty_string(node.get("textId")):
                errors.append(f"{node_id}: textId must not be empty")
            if not _is_nonempty_string(node.get("text")):
                errors.append(f"{node_id}: text must not be empty")
            if not _is_nonempty_string(node.get("next")):
                errors.append(f"{node_id}: line next must not be empty")
            text = node.get("text")
            if presentation == "bubble" and isinstance(text, str) and len(text.splitlines()) > 2:
                errors.append(f"{node_id}: bubble text exceeds two lines")

        elif node_type == "choice":
            presentation = node.get("presentation")
            if presentation not in REGISTERED_PRESENTATIONS:
                errors.append(f"{node_id}: invalid presentation {presentation!r}")
            options = node.get("options")
            if not isinstance(options, list) or not 1 <= len(options) <= 4:
                errors.append(f"{node_id}: choice must contain one to four options")
                continue
            for index, raw_option in enumerate(options):
                owner_id = f"{node_id}.options[{index}]"
                if not isinstance(raw_option, dict):
                    errors.append(f"{owner_id}: option must be an object")
                    continue
                option_id = raw_option.get("optionId")
                if not _is_nonempty_string(option_id):
                    errors.append(f"{owner_id}: option id must not be empty")
                elif option_id in seen_option_ids:
                    errors.append(f"{owner_id}: duplicate option id {option_id}")
                else:
                    assert isinstance(option_id, str)
                    seen_option_ids.add(option_id)
                if not _is_nonempty_string(raw_option.get("textId")):
                    errors.append(f"{owner_id}: textId must not be empty")
                if not _is_nonempty_string(raw_option.get("text")):
                    errors.append(f"{owner_id}: text must not be empty")
                if not _is_nonempty_string(raw_option.get("next")):
                    errors.append(f"{owner_id}: option next must not be empty")
                validate_outcome(owner_id, raw_option.get("outcomeId"))
                errors.extend(_condition_errors(owner_id, raw_option.get("conditions")))

        elif node_type == "end":
            validate_outcome(node_id, node.get("outcomeId"))

    if isinstance(entry, str) and entry in nodes:
        unreachable = set(nodes) - _reachable_nodes(entry, adjacency)
        errors.extend(f"unreachable node: {node_id}" for node_id in sorted(unreachable))
    typed_nodes = {
        node_id: node
        for node_id, node in nodes.items()
        if isinstance(node_id, str) and isinstance(node, dict)
    }
    errors.extend(_exitless_cycle_errors(typed_nodes, adjacency))
    return sorted(set(errors))


def canonicalize_dialogue(payload: dict) -> dict:
    return json.loads(json.dumps(payload, ensure_ascii=False, sort_keys=True))


def _reject_duplicate_keys(pairs: Iterable[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def validate_file(path: Path, catalogs: CatalogSnapshot) -> dict:
    payload = json.loads(
        path.read_text(encoding="utf-8"),
        object_pairs_hook=_reject_duplicate_keys,
    )
    errors = validate_dialogue(payload, catalogs)
    if errors:
        raise ValueError("\n".join(errors))
    return canonicalize_dialogue(payload)


def _load_catalog(path: Path) -> CatalogSnapshot:
    payload = json.loads(path.read_text(encoding="utf-8"))
    return CatalogSnapshot(
        speakers=frozenset(payload.get("speakers", [])),
        roles=frozenset(payload.get("roles", [])),
        outcomes=frozenset(payload.get("outcomes", [])),
    )


def _iter_dialogue_paths(paths: list[Path]) -> list[Path]:
    result: list[Path] = []
    for path in paths:
        if path.is_dir():
            result.extend(sorted(path.rglob("*.dialogue.json")))
        else:
            result.append(path)
    return sorted(set(result))


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate GameXXK dialogue JSON sources.")
    parser.add_argument("paths", nargs="+", type=Path)
    parser.add_argument("--catalog", required=True, type=Path)
    args = parser.parse_args()
    catalogs = _load_catalog(args.catalog)
    failed = False
    for path in _iter_dialogue_paths(args.paths):
        try:
            validate_file(path, catalogs)
            print(f"PASS {path}")
        except (OSError, ValueError, json.JSONDecodeError) as exc:
            failed = True
            print(f"FAIL {path}: {exc}")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
