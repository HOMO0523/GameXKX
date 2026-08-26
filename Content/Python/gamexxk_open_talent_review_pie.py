"""Open the active desktop workbench and its Talents navigation button in PIE."""

from __future__ import annotations

import json

import unreal


def main() -> None:
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor.get_game_world() if editor else None
    controller = unreal.GameplayStatics.get_player_controller(world, 0) if world else None
    workbench = (
        controller.get_desktop_training_workbench_widget_for_test()
        if controller
        else None
    )
    if workbench is None:
        raise RuntimeError("active desktop workbench was not found")
    if not workbench.open_backpack():
        raise RuntimeError("failed to expand the desktop workbench")
    print(json.dumps({"ok": True, "backpack_expanded": True}, ensure_ascii=False))


if __name__ == "__main__":
    main()
