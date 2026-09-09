import json
import sys

import unreal

sys.path.insert(0, r"D:\UE5 demo\GameXXK\Content\Python")
import gamexxk_probe_real_play_flow as probe  # noqa: E402


def main():
    world = probe._get_game_world()
    pc = probe._first_player_controller(world)
    board = pc.get_battle_board_widget_for_test()
    out = {}
    body = board.widget_tree.find_widget(unreal.Name("BattleHandCardDetailBody"))
    panel = board.widget_tree.find_widget(unreal.Name("BattleHandCardDetailPanel"))
    out["panel_visibility"] = str(panel.get_visibility()) if panel else "missing"
    if body is None:
        out["body"] = "missing"
        print(json.dumps(out, ensure_ascii=False))
        return
    rows = []
    children = body.get_all_children() if hasattr(body, "get_all_children") else []
    for row in children:
        cells = []
        for cell in (row.get_all_children() if hasattr(row, "get_all_children") else []):
            if hasattr(cell, "get_text"):
                cells.append(["text", str(cell.get_text())])
            else:
                content = cell.get_content() if hasattr(cell, "get_content") else None
                if content is not None and hasattr(content, "get_text"):
                    cells.append(["pill", str(content.get_text()), str(cell.get_visibility())])
                else:
                    cells.append(["other", str(cell.get_class().get_name())])
        rows.append(cells)
    out["rows"] = rows
    print(json.dumps(out, ensure_ascii=False, default=str))


if __name__ == "__main__":
    main()
