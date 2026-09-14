"""Inspect the live party-left HUD in an isolated, canonical 2D PIE session."""
import argparse
import json
from pathlib import Path

import unreal

parser = argparse.ArgumentParser()
parser.add_argument("phase", choices=["travel", "battle", "observe", "capture", "sample", "restore"])
parser.add_argument("--label", default="party-left")
parser.add_argument("--width", type=int, default=1920)
parser.add_argument("--height", type=int, default=1080)
args = parser.parse_args()

root = Path(str(unreal.Paths.project_dir())).resolve()
saved = str(unreal.Paths.project_saved_dir()).replace("\\", "/")
assert "/PartyLeft/PreviewUser/" in saved, "Use the isolated PartyLeft/PreviewUser profile"
out = root / "Saved/PartyLeft/Live"
out.mkdir(parents=True, exist_ok=True)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world and "L_DesktopTrainingHUD" in world.get_path_name(), "Use the canonical 2D PIE map"
pc = unreal.GameplayStatics.get_player_controller(world, 0)
instance = unreal.GameplayStatics.get_game_instance(world)
mvp = next(x for x in unreal.ObjectIterator(unreal.GameXXKMVPSubsystem) if x.get_outer() == instance)
dev = next(x for x in unreal.ObjectIterator(unreal.GameXXKDevToolsSubsystem) if x.get_outer() == instance)
workbench = pc.get_desktop_training_workbench_widget_for_test()


def command(name, parameters=None):
    result = json.loads(dev.execute_json(json.dumps({"command": name, "args": parameters or {}})))
    assert result.get("ok"), result
    return result


def belongs_to(widget, owner):
    parent = widget.get_outer()
    while parent:
        if parent == owner:
            return True
        parent = parent.get_outer()
    return False


report = {"phase": args.phase, "world": world.get_path_name(), "isolated_saved": saved}
if args.phase in ("travel", "restore"):
    if dev.is_session_active():
        report["restore"] = command("session.restore")
    report["travel_started"] = bool(mvp.start_training_travel(unreal.Name("Training.Normal.1-1")))
    pc.refresh_player_flow_widgets_for_test()
    workbench.open_workbench()
elif args.phase == "battle":
    if not dev.is_session_active():
        report["session"] = command("session.begin")
    report["battle"] = command("battle.start", {"stage": "Training.Normal.1-1", "encounter": 1, "seed": 20260914})
    report["auto"] = command("battle.auto", {"enabled": False})
    pc.refresh_player_flow_widgets_for_test()

board = pc.get_battle_board_widget_for_test()
report["battle_visible"] = bool(board and board.is_battle_board_visible())
report["targeting"] = bool(board and board.is_card_targeting_active())
report["dev_session"] = bool(dev.is_session_active())
report["travel_phase"] = workbench.get_travel_visual_phase_name_for_test()
rows = []
for widget in unreal.ObjectIterator(unreal.Widget):
    name = widget.get_name()
    if not (name.startswith(("TravelHero", "TravelCompanion", "TravelEnemy", "TravelBackgroundTile_", "TrainingWaveMarker_", "BattleUnitVisual_", "BattleUnitTargetProxy_"))
            or name in ("BattleUnitAtlasImage", "BattleEnemyIntentCardBox", "BattleTopRightToolbar", "TrainingWaveProgressFill")):
        continue
    if not (belongs_to(widget, workbench) or (board and belongs_to(widget, board))):
        continue
    transform = widget.get_editor_property("render_transform")
    row = {"name": name, "path": widget.get_path_name(), "visibility": str(widget.get_visibility()),
           "scale": [transform.scale.x, transform.scale.y],
           "translation": [transform.translation.x, transform.translation.y]}
    slot = widget.get_editor_property("slot")
    if isinstance(slot, unreal.CanvasPanelSlot):
        anchor = slot.get_anchors().minimum
        pos = slot.get_position()
        size = slot.get_size()
        row.update(anchor=[anchor.x, anchor.y], position=[pos.x, pos.y], size=[size.x, size.y])
    rows.append(row)
report["widgets"] = rows
state = mvp.get_runtime_state_copy()
report["units"] = [{"id": str(u.unit_id), "side": str(u.side), "hp": u.hp}
                   for u in state.card_run.active_battle.units]
report["hand"] = [{"id": str(c.instance_id), "card": str(c.card_id), "owner": str(c.owner_unit_id)}
                  for c in state.card_run.active_battle.deck.hand]
if args.phase == "sample":
    import time
    started = time.monotonic()
    samples = []
    handle = [None]
    captured = [False]
    last_cinematic = [None]
    visuals = [w for w in unreal.ObjectIterator(unreal.GameXXKBattleUnitVisualWidget)
               if board and belongs_to(w, board) and w.get_name().startswith("BattleUnitVisual_")]

    def sample_tick(delta):
        elapsed = time.monotonic() - started
        for visual in visuals:
            slot = visual.get_editor_property("slot")
            if not slot or visual.get_visibility() in (unreal.SlateVisibility.HIDDEN, unreal.SlateVisibility.COLLAPSED):
                continue
            if slot.get_size().x < 800:
                continue
            last_cinematic[0] = elapsed
            anchor = slot.get_anchors().minimum
            transform = visual.get_editor_property("render_transform")
            samples.append({"seconds": elapsed, "name": visual.get_name(), "anchor": [anchor.x, anchor.y],
                            "translation": [transform.translation.x, transform.translation.y]})
            if not captured[0]:
                captured[0] = bool(unreal.GameXXKEditorCaptureAutomationLibrary.capture_live_game_widget(
                    board, str(out / "battle-cinematic.png"), 1920, 1080))
        if elapsed >= 60 or (last_cinematic[0] is not None and elapsed - last_cinematic[0] >= 1):
            unreal.unregister_slate_post_tick_callback(handle[0])
            (out / "cinematic-samples.json").write_text(json.dumps(samples, indent=2), encoding="utf-8")

    handle[0] = unreal.register_slate_post_tick_callback(sample_tick)
    report["sampling"] = "next live close-up, with a 60-second timeout"
if args.phase == "capture":
    assert args.label.replace("-", "").replace("_", "").isalnum()
    widget = board if report["battle_visible"] else workbench
    path = out / (args.label + ".png")
    report["capture_ok"] = bool(unreal.GameXXKEditorCaptureAutomationLibrary.capture_live_game_widget(
        widget, str(path), args.width, args.height))
    report["capture"] = str(path)
    assert report["capture_ok"]
(out / (args.label + "-" + args.phase + ".json")).write_text(
    json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
print(json.dumps(report, ensure_ascii=False))
