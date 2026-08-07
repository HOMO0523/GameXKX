from __future__ import annotations

import unreal


def main() -> None:
    world = unreal.EditorLevelLibrary.get_editor_world()
    game_instance = unreal.GameplayStatics.get_game_instance(world)

    subsystem = unreal.GameXXKMVPSubsystem()
    subsystem.ensure_qingshan_town_runtime_for_direct_map()

    widget_class = unreal.load_object(None, "/Script/GameXXK.GameXXKCompanionRosterWidget")
    widget = unreal.new_object(widget_class, game_instance)
    widget.set_mvp_subsystem(subsystem)
    widget.take_widget()
    widget.refresh_from_state()

    # Force a layout pass so cached geometry exists.
    if hasattr(widget, "force_layout_prepass"):
        widget.force_layout_prepass()

    tree = widget.widget_tree
    if not tree:
        print("no widget tree")
        return

    scroll = unreal.WidgetBlueprintLibrary.cast_to_scroll_box(
        tree.find_widget("CompanionRosterEquipmentScrollBox"))
    print("scrollbox found:", scroll is not None)

    grid = unreal.WidgetBlueprintLibrary.cast_to_uniform_grid_panel(
        tree.find_widget("CompanionRosterEquipmentGrid"))
    print("grid found:", grid is not None)
    if grid:
        print("grid children:", grid.get_children_count())

    root = tree.root_widget
    if root:
        geo = root.get_cached_geometry()
        print("root size:", geo.get_local_size())

    # Report every warehouse slot's cached position to see the actual layout.
    for idx in range(20):
        name = "CompanionEquipmentWarehouseSlot_%03d" % idx
        w = tree.find_widget(name)
        if w:
            g = w.get_cached_geometry()
            print(name, "pos=", g.get_absolute_position(), "size=", g.get_local_size())
        else:
            print(name, "MISSING")


if __name__ == "__main__":
    main()
