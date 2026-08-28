from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable


FORBIDDEN_SCENE_FIELDS = frozenset(
    {"map", "mappath", "worldlocation", "location", "transform", "x", "y", "z"}
)
STEP_TYPES = frozenset({"command", "wait", "dialogue", "branchOnOutcome", "end"})


@dataclass(frozen=True)
class NarrativeCatalogSnapshot:
    character_ids: frozenset[str]
    action_ids_by_character: dict[str, frozenset[str]]
    dialogue_ids: frozenset[str]
    slot_ids_by_stage: dict[str, frozenset[str]]
    command_types: frozenset[str]
    wait_types: frozenset[str]
    outcome_ids: frozenset[str]


def _nonempty(value: object) -> bool:
    return isinstance(value, str) and bool(value.strip())


def _forbidden_scene_errors(value: object, owner: str = "root") -> list[str]:
    errors: list[str] = []
    if isinstance(value, dict):
        for key, child in value.items():
            key_text = str(key)
            if key_text.lower() in FORBIDDEN_SCENE_FIELDS:
                errors.append(f"{owner}: forbidden scene field {key_text}")
            errors.extend(_forbidden_scene_errors(child, f"{owner}.{key_text}"))
    elif isinstance(value, list):
        for index, child in enumerate(value):
            errors.extend(_forbidden_scene_errors(child, f"{owner}[{index}]"))
    elif isinstance(value, str) and "/maps/" in value.lower().replace("\\", "/"):
        errors.append(f"{owner}: direct map path is forbidden")
    return errors


def validate_character_catalog(payload: dict) -> list[str]:
    errors = _forbidden_scene_errors(payload)
    if not isinstance(payload, dict) or payload.get("schemaVersion") != 1:
        errors.append("character catalog schemaVersion must be 1")
        return sorted(set(errors))
    characters = payload.get("characters")
    if not isinstance(characters, list) or not characters:
        errors.append("characters must be a non-empty array")
        return sorted(set(errors))
    seen_ids: set[str] = set()
    for index, entry in enumerate(characters):
        owner = f"characters[{index}]"
        if not isinstance(entry, dict):
            errors.append(f"{owner}: character must be an object")
            continue
        character_id = entry.get("characterId")
        if not _nonempty(character_id):
            errors.append(f"{owner}: characterId must not be empty")
        elif character_id in seen_ids:
            errors.append(f"{owner}: duplicate characterId {character_id}")
        else:
            assert isinstance(character_id, str)
            seen_ids.add(character_id)
        if not _nonempty(entry.get("displayName")):
            errors.append(f"{owner}: displayName must not be empty")
        actions = entry.get("actions", [])
        if not isinstance(actions, list) or any(not _nonempty(action) for action in actions):
            errors.append(f"{owner}: actions must contain non-empty strings")
        elif len(actions) != len(set(actions)):
            errors.append(f"{owner}: action IDs must be unique")
        for path_key in ("portraitPath", "animationLibraryPath"):
            path_value = entry.get(path_key)
            if path_value is not None and (
                not _nonempty(path_value) or not str(path_value).startswith("/Game/")
            ):
                errors.append(f"{owner}: {path_key} must be a /Game/ asset path")
    return sorted(set(errors))


def _step_targets(step: dict[str, Any]) -> set[str]:
    step_type = step.get("type")
    if step_type in {"command", "wait", "dialogue"}:
        target = step.get("next")
        return {target} if _nonempty(target) else set()
    if step_type == "branchOnOutcome" and isinstance(step.get("outcomes"), dict):
        return {target for target in step["outcomes"].values() if _nonempty(target)}
    return set()


def _reachable(entry: str, adjacency: dict[str, set[str]]) -> set[str]:
    reached: set[str] = set()
    pending = [entry]
    while pending:
        step_id = pending.pop()
        if step_id in reached or step_id not in adjacency:
            continue
        reached.add(step_id)
        pending.extend(adjacency[step_id] - reached)
    return reached


def _components(adjacency: dict[str, set[str]]) -> list[set[str]]:
    index = 0
    indices: dict[str, int] = {}
    low: dict[str, int] = {}
    stack: list[str] = []
    on_stack: set[str] = set()
    result: list[set[str]] = []

    def visit(step_id: str) -> None:
        nonlocal index
        indices[step_id] = index
        low[step_id] = index
        index += 1
        stack.append(step_id)
        on_stack.add(step_id)
        for target in sorted(adjacency[step_id]):
            if target not in adjacency:
                continue
            if target not in indices:
                visit(target)
                low[step_id] = min(low[step_id], low[target])
            elif target in on_stack:
                low[step_id] = min(low[step_id], indices[target])
        if low[step_id] != indices[step_id]:
            return
        component: set[str] = set()
        while stack:
            member = stack.pop()
            on_stack.remove(member)
            component.add(member)
            if member == step_id:
                break
        result.append(component)

    for step_id in sorted(adjacency):
        if step_id not in indices:
            visit(step_id)
    return result


def _immediate_cycle_errors(steps: dict[str, dict], adjacency: dict[str, set[str]]) -> list[str]:
    errors: list[str] = []
    for component in _components(adjacency):
        cyclic = len(component) > 1 or any(step in adjacency[step] for step in component)
        if not cyclic:
            continue
        if all(steps[step].get("type") == "branchOnOutcome" for step in component):
            has_exit = any(
                target not in component
                for step in component
                for target in adjacency[step]
            )
            if not has_exit:
                errors.append(f"exitless immediate cycle: {', '.join(sorted(component))}")
    return errors


def validate_sequence(payload: dict, catalogs: NarrativeCatalogSnapshot) -> list[str]:
    errors = _forbidden_scene_errors(payload)
    if not isinstance(payload, dict):
        return sorted(set(errors + ["sequence root must be an object"]))
    if payload.get("schemaVersion") != 1:
        errors.append("schemaVersion must be 1")
    if not _nonempty(payload.get("sequenceId")):
        errors.append("sequenceId must not be empty")
    if not isinstance(payload.get("sequenceVersion"), int) or payload["sequenceVersion"] <= 0:
        errors.append("sequenceVersion must be a positive integer")
    stage_id = payload.get("stageContractId")
    if stage_id not in catalogs.slot_ids_by_stage:
        errors.append(f"unknown stage contract {stage_id}")
    stage_slots = catalogs.slot_ids_by_stage.get(stage_id, frozenset())

    roles = payload.get("roles")
    if not isinstance(roles, dict):
        errors.append("roles must be an object")
        roles = {}
    for role, character_id in roles.items():
        if not _nonempty(role) or not _nonempty(character_id):
            errors.append("role and character IDs must not be empty")
        elif character_id not in catalogs.character_ids:
            errors.append(f"role {role}: unknown character {character_id}")

    steps = payload.get("steps")
    if not isinstance(steps, dict) or not steps:
        return sorted(set(errors + ["steps must be a non-empty object"]))
    entry = payload.get("entryStep")
    if entry not in steps:
        errors.append(f"entry step does not exist: {entry!r}")
    adjacency = {str(step_id): set() for step_id in steps}
    command_ids: set[str] = set()

    for step_id, step in steps.items():
        if not _nonempty(step_id) or not isinstance(step, dict):
            errors.append("step IDs must be non-empty and step values objects")
            continue
        step_type = step.get("type")
        if step_type not in STEP_TYPES:
            errors.append(f"{step_id}: unknown step type {step_type!r}")
            continue
        targets = _step_targets(step)
        adjacency[step_id].update(targets)
        for target in targets:
            if target not in steps:
                errors.append(f"{step_id}: missing target {target}")

        if step_type == "command":
            command_id = step.get("commandId")
            if not _nonempty(command_id):
                errors.append(f"{step_id}: commandId must not be empty")
            elif command_id in command_ids:
                errors.append(f"{step_id}: duplicate command id {command_id}")
            else:
                assert isinstance(command_id, str)
                command_ids.add(command_id)
            command_type = step.get("commandType")
            if command_type not in catalogs.command_types:
                errors.append(f"{step_id}: unknown command type {command_type}")
            arguments = step.get("arguments", {})
            if not isinstance(arguments, dict):
                errors.append(f"{step_id}: arguments must be an object")
                arguments = {}
            role = arguments.get("role")
            if role is not None and role not in roles:
                errors.append(f"{step_id}: unknown role {role}")
            character_id = arguments.get("characterId")
            if character_id is not None and character_id not in catalogs.character_ids:
                errors.append(f"{step_id}: unknown character {character_id}")
            slot_id = arguments.get("slotId")
            if slot_id is not None and slot_id not in stage_slots:
                errors.append(f"{step_id}: unknown slot {slot_id}")
            action_id = arguments.get("actionId")
            if action_id is not None:
                resolved_character = roles.get(role) if role is not None else character_id
                supported = catalogs.action_ids_by_character.get(resolved_character, frozenset())
                if action_id not in supported:
                    errors.append(
                        f"{step_id}: character {resolved_character} does not support action {action_id}"
                    )
            if not _nonempty(step.get("next")):
                errors.append(f"{step_id}: command next must not be empty")

        elif step_type == "wait":
            if step.get("waitType") not in catalogs.wait_types:
                errors.append(f"{step_id}: unknown wait type {step.get('waitType')}")
            if not _nonempty(step.get("next")):
                errors.append(f"{step_id}: wait next must not be empty")

        elif step_type == "dialogue":
            if step.get("dialogueId") not in catalogs.dialogue_ids:
                errors.append(f"{step_id}: unknown dialogue {step.get('dialogueId')}")
            if not _nonempty(step.get("next")):
                errors.append(f"{step_id}: dialogue next must not be empty")

        elif step_type == "branchOnOutcome":
            outcomes = step.get("outcomes")
            if not isinstance(outcomes, dict) or not outcomes:
                errors.append(f"{step_id}: outcomes must be a non-empty object")
            else:
                for outcome_id, target in outcomes.items():
                    if outcome_id not in catalogs.outcome_ids:
                        errors.append(f"{step_id}: unknown outcome {outcome_id}")
                    if not _nonempty(target):
                        errors.append(f"{step_id}: outcome target must not be empty")

    if isinstance(entry, str) and entry in steps:
        unreachable = set(steps) - _reachable(entry, adjacency)
        errors.extend(f"unreachable step: {step_id}" for step_id in sorted(unreachable))
    typed_steps = {key: value for key, value in steps.items() if isinstance(value, dict)}
    errors.extend(_immediate_cycle_errors(typed_steps, adjacency))
    return sorted(set(errors))


def canonicalize_sequence(payload: dict) -> dict:
    return json.loads(json.dumps(payload, ensure_ascii=False, sort_keys=True))


def _reject_duplicate_keys(pairs: Iterable[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def validate_file(path: Path, catalogs: NarrativeCatalogSnapshot) -> dict:
    payload = json.loads(
        path.read_text(encoding="utf-8"), object_pairs_hook=_reject_duplicate_keys
    )
    errors = validate_sequence(payload, catalogs)
    if errors:
        raise ValueError("\n".join(errors))
    return canonicalize_sequence(payload)
