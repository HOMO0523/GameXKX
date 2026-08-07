"""Load the Qingshan town map and capture the PIE town HUD viewport.

Two modes driven through UE MCP:
  --load     Load the town map in the editor (caller then starts PIE via MCP).
  --capture  Issue HighResShot so the town HUD UMG overlay is baked to disk.
"""

from __future__ import annotations

import json
import sys

import unreal


TOWN_MAP = "/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo"


def _load() -> dict:
    unreal.EditorLevelLibrary.load_level(TOWN_MAP)
    return {"step": "loaded"}


def _capture() -> dict:
    editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor_subsystem.get_game_world() if editor_subsystem else None
    if world is None:
        return {"step": "capture", "error": "pie_world_unavailable"}
    controller = unreal.GameplayStatics.get_player_controller(world, 0)
    if controller is None:
        return {"step": "capture", "error": "pie_controller_unavailable"}
    unreal.SystemLibrary.execute_console_command(controller, "HighResShot 1920x1080")
    return {"step": "capture", "captured": True}


def _setres() -> dict:
    editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor_subsystem.get_game_world() if editor_subsystem else None
    if world is None:
        return {"step": "setres", "error": "pie_world_unavailable"}
    controller = unreal.GameplayStatics.get_player_controller(world, 0)
    if controller is None:
        return {"step": "setres", "error": "pie_controller_unavailable"}
    unreal.SystemLibrary.execute_console_command(controller, "r.SetRes 1920x1080")
    return {"step": "setres", "sent": True}


def main(argv: list[str]) -> dict:
    if "--load" in argv:
        return _load()
    if "--capture" in argv:
        return _capture()
    if "--setres" in argv:
        return _setres()
    return {"error": "expected --load, --capture, or --setres"}


if __name__ == "__main__":
    print(json.dumps(main(sys.argv[1:]), ensure_ascii=False))
