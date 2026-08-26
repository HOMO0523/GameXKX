"""Read and exercise the permanent-talent facade in the active PIE world."""

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
    subsystem = workbench.get_mvp_subsystem() if workbench else None
    if subsystem is None:
        raise RuntimeError("active PIE GameXXKMVPSubsystem was not found")

    views_before = subsystem.get_talent_node_views()
    root_before = next(
        (view for view in views_before if str(view.definition.id) == "Talent.Root"),
        None,
    )
    if root_before is None:
        raise RuntimeError("Talent.Root view was not returned")

    subsystem.purchase_talent_node("Talent.Root")
    views_after = subsystem.get_talent_node_views()
    root_after = next(
        (view for view in views_after if str(view.definition.id) == "Talent.Root"),
        None,
    )
    visible_after = [
        str(view.definition.id)
        for view in views_after
        if "HIDDEN" not in str(view.state)
    ]
    print(json.dumps({
        "ok": True,
        "root_rank_before": int(root_before.rank),
        "root_state_before": str(root_before.state),
        "root_price_before": int(root_before.next_price),
        "purchase_invoked": True,
        "root_rank_after": int(root_after.rank) if root_after else -1,
        "root_state_after": str(root_after.state) if root_after else "missing",
        "visible_after": visible_after,
    }, ensure_ascii=False))


if __name__ == "__main__":
    main()
