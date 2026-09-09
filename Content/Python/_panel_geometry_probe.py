import json
import sys

import unreal

sys.path.insert(0, r"D:\UE5 demo\GameXXK\Content\Python")
import gamexxk_probe_real_play_flow as probe  # noqa: E402


def find_widget_by_name(root, name):
    if root is None:
        return None
    if str(root.get_name()) == name:
        return root
    for child in root.get_all_children():
        found = find_widget_by_name(child, name)
        if found is not None:
            return found
    return None


def main():
    world = probe._get_game_world()
    pc = probe._first_player_controller(world)
    board = pc.get_battle_board_widget_for_test()
    out = {}
    root = board.get_root_widget()
    out["root"] = str(root)
    panel = find_widget_by_name(root, "BattleHandCardDetailPanel")
    if panel is None:
        out["panel"] = "missing"
        print(json.dumps(out, ensure_ascii=False, default=str))
        return
    out["panel_visibility"] = str(panel.get_visibility())
    slot = unreal.WidgetLayoutLibrary.slot_as_canvas_slot(panel)
    out["slot_type"] = str(slot.get_class().get_name()) if slot else "none"
    if slot:
        out["auto_size"] = slot.get_auto_size()
        off = slot.get_offsets()
        out["offsets"] = [off.left, off.top, off.right, off.bottom]
    try:
        d = panel.get_desired_size()
        out["desired_size"] = [d.x, d.y]
    except Exception as exc:  # noqa: BLE001
        out["desired_size_error"] = str(exc)
    sizebox = find_widget_by_name(root, "BattleHandCardDetailSizeBox")
    if sizebox is not None:
        out["sizebox_height_override"] = sizebox.height_override
        out["sizebox_width_override"] = sizebox.width_override
    print(json.dumps(out, ensure_ascii=False, default=str))


if __name__ == "__main__":
    main()
