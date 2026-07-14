"""Capture the current PIE viewport for occlusion material visual QA."""

from __future__ import annotations

import json

import unreal


def main():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor.get_game_world() if editor else None
    if world is None:
        raise RuntimeError("no active PIE world")
    unreal.SystemLibrary.execute_console_command(world, "HighResShot 1920x1080")
    report = {"requested": True, "world": str(world.get_outermost().get_path_name())}
    print(json.dumps(report, ensure_ascii=False))
    return report


if __name__ == "__main__":
    main()
