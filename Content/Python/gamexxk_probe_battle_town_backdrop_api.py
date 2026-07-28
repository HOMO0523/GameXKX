"""Read-only UE Python capability probe for battle-town backdrop integration."""

from __future__ import annotations

import json

import unreal


def _members(value: object) -> list[str]:
    return sorted(
        name
        for name in dir(value)
        if any(token in name.lower() for token in ("level", "instance", "world_asset", "load"))
    )


def main() -> dict[str, object]:
    level_instance = getattr(unreal, "LevelInstance", None)
    subsystem_type = getattr(unreal, "LevelInstanceEditorSubsystem", None)
    subsystem = None
    if subsystem_type is not None:
        subsystem = unreal.get_editor_subsystem(subsystem_type)
    report = {
        "ok": True,
        "level_instance_class": str(level_instance),
        "level_instance_members": _members(level_instance) if level_instance is not None else [],
        "level_instance_editor_subsystem": str(subsystem_type),
        "level_instance_editor_subsystem_members": _members(subsystem) if subsystem is not None else [],
        "level_streaming_dynamic": str(getattr(unreal, "LevelStreamingDynamic", None)),
        "level_streaming_dynamic_members": _members(getattr(unreal, "LevelStreamingDynamic", None)),
        "set_world_asset_doc": str(getattr(level_instance, "set_world_asset", None).__doc__)
        if level_instance is not None
        else "",
        "load_level_instance_doc": str(getattr(level_instance, "load_level_instance", None).__doc__)
        if level_instance is not None
        else "",
    }
    unreal.log("[GameXXK][BattleTownBackdropApiProbe] " + json.dumps(report, ensure_ascii=False))
    return report


if __name__ == "__main__":
    print(json.dumps(main(), ensure_ascii=False))
