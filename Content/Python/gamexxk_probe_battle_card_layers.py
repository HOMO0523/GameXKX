"""Inspect the right-side vertical controls and card/actor paint order in live PIE."""
import json
from pathlib import Path

import unreal

assert "/PartyLeft/PreviewUser/" in str(unreal.Paths.project_saved_dir()).replace("\\", "/")
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world and "L_DesktopTrainingHUD" in world.get_path_name()
pc = unreal.GameplayStatics.get_player_controller(world, 0)
board = pc.get_battle_board_widget_for_test()
assert board and board.is_battle_board_visible()

def belongs(widget):
    parent = widget.get_outer()
    while parent and parent != board:
        parent = parent.get_outer()
    return parent == board

widgets = {w.get_name(): w for w in unreal.ObjectIterator(unreal.Widget) if belongs(w)}
toolbar = widgets["BattleTopRightToolbar"]
rail = widgets["BattleEnemyIntentCardBox"]
showcase = widgets["BattleEnemyIntentShowcaseCard"]
stage = rail.get_parent()
actors = [w for name, w in widgets.items() if name.startswith("BattleUnitVisual_")]
slot = toolbar.get_editor_property("slot")
pos, size = slot.get_position(), slot.get_size()
rail_z = rail.get_editor_property("slot").get_z_order()
showcase_z = showcase.get_editor_property("slot").get_z_order()
report = {"world": world.get_path_name(), "vertical": isinstance(toolbar, unreal.VerticalBox),
          "position": [pos.x, pos.y], "size": [size.x, size.y],
          "controls": [toolbar.get_child_at(i).get_name() for i in range(toolbar.get_children_count())],
          "card_parent": stage.get_name(), "rail_z": rail_z, "showcase_z": showcase_z,
          "actors": [{"name": a.get_name(), "same_parent": a.get_parent() == stage,
                      "z": a.get_editor_property("slot").get_z_order()} for a in actors]}
report["ok"] = (report["vertical"] and report["controls"] == ["BattleAutoPlaySize", "BattleCloseSize"]
                and pos.x > 1700 and pos.x + size.x <= 1920
                and showcase.get_parent() == stage and len(actors) == 6
                and all(a["same_parent"] and a["z"] > max(rail_z, showcase_z) for a in report["actors"]))
out = Path(str(unreal.Paths.project_dir())).resolve() / "Saved/BattleRightToolbar"
out.mkdir(parents=True, exist_ok=True)
(out / "live-layout.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
print(json.dumps(report, ensure_ascii=False))
assert report["ok"]
