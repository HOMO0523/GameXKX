"""Capture the real BattleBoard targeting paint at reproducible stage-local pointers."""
import json
import sys
from pathlib import Path

import unreal

assert "/PartyLeft/PreviewUser/" in str(unreal.Paths.project_saved_dir()).replace("\\", "/")
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world and "L_DesktopTrainingHUD" in world.get_path_name()
pc = unreal.GameplayStatics.get_player_controller(world, 0)
instance = unreal.GameplayStatics.get_game_instance(world)
mvp = next(x for x in unreal.ObjectIterator(unreal.GameXXKMVPSubsystem) if x.get_outer() == instance)
dev = next(x for x in unreal.ObjectIterator(unreal.GameXXKDevToolsSubsystem) if x.get_outer() == instance)
assert dev.is_session_active()
board = pc.get_battle_board_widget_for_test()
assert board and board.is_battle_board_visible()
if not board.is_card_targeting_active():
    card = next(c for c in mvp.get_runtime_state_copy().card_run.active_battle.deck.hand
                if str(c.card_id) == "Profession.Blade.JingHongChuQiao")
    assert board.click_card_in_hand(card.instance_id)

mode = sys.argv[1] if len(sys.argv) > 1 else "right"
targets = {"right": (1450,600), "upper-right": (1510,320), "left": (70,600)}
point = unreal.Vector2D(*targets[mode])
board.update_targeting_pointer(point)
start = board.get_targeting_source_position_for_test()
end = board.get_targeting_pointer_position_for_test()
material = unreal.load_asset("/Game/GameXXK/UI/Battle/Materials/M_BattleTargetArrowHead_GemV1")
texture = unreal.load_asset("/Game/GameXXK/UI/Battle/Textures/T_BattleTargetArrowHead_GemV1")
assert material and texture
out = Path(str(unreal.Paths.project_dir())).resolve() / "Saved/GemTargetArrow/Live"
out.mkdir(parents=True, exist_ok=True)
path = out / (mode + ".png")
assert unreal.GameXXKEditorCaptureAutomationLibrary.capture_live_game_widget(board, str(path), 1920, 1080)
report = {"world": world.get_path_name(), "targeting": True, "mode": mode,
          "source": [start.x,start.y], "pointer": [end.x,end.y],
          "material": material.get_path_name(), "texture": texture.get_path_name(), "capture": str(path),
          "capture_method": "live widget render with a stage-local diagnostic pointer"}
report["texture_audit"] = json.loads(unreal.GameXXKTextureAuditLibrary.inspect_texture(texture, False))
(out / (mode + ".json")).write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
print(json.dumps(report, ensure_ascii=False))
