
import unreal, json
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world = editor.get_game_world()
pc = unreal.GameplayStatics.get_player_controller(world, 0)
board = pc.get_battle_board_widget_for_test()
sub = board.get_mvp_subsystem()
state = sub.get_runtime_state_copy()
def prop(t, *names):
    for n in names:
        try: return getattr(t, n)
        except Exception: pass
        try: return t.get_editor_property(n)
        except Exception: pass
    return None
run = prop(state, "card_run", "CardRun")
roster = prop(run, "companion_roster", "CompanionRoster")
print(json.dumps({
    "active_id": str(prop(roster, "active_permanent_companion_instance_id", "ActivePermanentCompanionInstanceId")),
    "active_role": str(prop(roster, "active_permanent_companion_role", "ActivePermanentCompanionRole")),
}, ensure_ascii=False))
