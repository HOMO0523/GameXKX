
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
out = {"gold": str(prop(state, "player_gold", "PlayerGold")), "level": str(prop(state, "player_level", "PlayerLevel"))}
run = prop(state, "card_run", "CardRun")
roster = prop(run, "companion_roster", "CompanionRoster")
out["recruit_seed"] = str(prop(roster, "recruit_sequence_seed", "RecruitSequenceSeed"))
order = prop(roster, "pending_recruit_order", "PendingRecruitOrder")
out["pending_order"] = str(prop(order, "b_has_pending_order", "bHasPendingOrder"))
shop = prop(state, "meta_shop", "MetaShop")
out["shop_seed"] = str(prop(shop, "seed", "Seed"))
out["purchase_ordinal"] = str(prop(shop, "next_purchase_ordinal", "NextPurchaseOrdinal"))
print(json.dumps(out, ensure_ascii=False))
