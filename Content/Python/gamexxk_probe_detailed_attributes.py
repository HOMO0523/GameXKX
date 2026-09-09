"""Observe the real desktop attribute page; run phases separately so Slate can tick."""

import json
import sys
from pathlib import Path

import unreal
import gamexxk_probe_training_visual_mvp as base


def walk(widget):
    yield widget
    if isinstance(widget, unreal.PanelWidget):
        for index in range(widget.get_children_count()):
            yield from walk(widget.get_child_at(index))


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "inspect"
    world, controller, workbench = base._controller_and_widget()
    if not world or not workbench or "L_DesktopTrainingHUD" not in world.get_name():
        raise RuntimeError("Detailed attributes require the canonical 2D desktop PIE world")
    if mode == "open":
        workbench.open_backpack()
    if mode == "character":
        assert workbench.select_backpack_character_for_test(unreal.Name(sys.argv[2]))
    tree = unreal.find_object(workbench, "DesktopTrainingWorkbenchWidgetTree") or unreal.find_object(workbench, "WidgetTree")
    root = unreal.find_object(tree, "DesktopTrainingReferenceCanvas")
    inventory = next((widget for widget in walk(root) if widget.get_name() == "EmbeddedApprovedBackpack"), None)
    if not inventory:
        raise RuntimeError("The actual embedded backpack is not mounted")
    if mode in ("open", "character"):
        assert inventory.open_character_backpack_tab_for_test(unreal.GameXXKCharacterBackpackTab.ATTRIBUTES)
    if mode == "detail":
        inventory.set_detailed_attributes_open(True)
    if mode == "back":
        inventory.set_detailed_attributes_open(False)
    inner_tree = unreal.find_object(inventory, "InventoryWindowWidgetTree") or unreal.find_object(inventory, "WidgetTree")
    inner_root = unreal.find_object(inner_tree, "InventoryWindowRoot")
    widgets = {widget.get_name(): widget for widget in walk(inner_root)}
    scroll = widgets["InventoryDetailedAttributesScrollBox"]
    if mode == "scroll":
        scroll.set_scroll_offset(float(sys.argv[2]) if len(sys.argv) > 2 else 900.0)
    names = ["InventoryDetailedAttributesButton", "InventoryDetailedAttributesBackButton", "InventoryDetailedAttributesPanel", "InventoryCharacterAttributeDetails", "InventoryDetailedAttributesScrollBox", "InventoryCharacterAttributeTitle", "InventoryCharacterIdentityText"]
    metrics = {}
    for name in names:
        widget = widgets[name]
        geometry = widget.get_cached_geometry()
        position = unreal.SlateLibrary.local_to_absolute(geometry, unreal.Vector2D(0, 0))
        size = unreal.SlateLibrary.get_absolute_size(geometry)
        metrics[name] = {"visibility": str(widget.get_visibility()), "position": [position.x, position.y], "size": [size.x, size.y]}
        if isinstance(widget, unreal.TextBlock):
            metrics[name]["text"] = str(widget.get_text())
    data = {"world": world.get_name(), "mode": mode, "character": str(inventory.get_configured_character_id_for_test()),
            "open": inventory.is_detailed_attributes_open_for_test(), "summary": str(inventory.get_character_tab_body_text_for_test()),
            "scroll": {"offset": scroll.get_scroll_offset(), "end": scroll.get_scroll_offset_of_end()}, "widgets": metrics}
    output = Path(unreal.Paths.get_project_file_path()).resolve().parent / "Saved/Diagnostics/CharacterDetailedAttributes/PIE"
    output.mkdir(parents=True, exist_ok=True)
    (output / (mode + ".json")).write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(data, ensure_ascii=False))


if __name__ == "__main__":
    main()
