import json

import unreal

import gamexxk_probe_desktop_story_flow as story
import gamexxk_probe_real_play_flow as real


controller = story._controller()
out = {"controller": str(controller) if controller else ""}
try:
    workbench = controller.get_desktop_training_workbench_widget_for_test()
    out["workbench"] = str(workbench)
    tree = workbench.get_editor_property("widget_tree")
    out["tree"] = str(tree)
    for name in (
        "CombatTutorialBeginActivityButton",
        "CombatTutorialMerchantPurchaseButton",
        "CombatTutorialMerchantLeaveButton",
        "CombatTutorialSettlementConfirmButton",
    ):
        widget = tree.find_widget(name) if tree else None
        out[name] = {
            "exists": bool(widget),
            "enabled": bool(widget.get_is_enabled()) if widget else False,
            "visibility": str(widget.get_visibility()) if widget else "",
        }
except Exception as exc:
    out["error"] = str(exc)
try:
    out["actionButtons"] = [
        {
            "name": str(widget.get_name()),
            "path": str(widget.get_path_name()),
            "enabled": bool(widget.get_is_enabled()),
            "visibility": str(widget.get_visibility()),
        }
        for widget in unreal.ObjectIterator()
        if isinstance(widget, unreal.GameXXKDesktopTrainingActionButton)
        and "CombatTutorial" in str(widget.get_name())
        and "Default__" not in str(widget.get_path_name())
    ]
except Exception as exc:
    out["objectError"] = str(exc)
try:
    subsystem = workbench.get_mvp_subsystem()
    runtime = subsystem.get_runtime_state_copy()
    card_run = real._struct_get(runtime, "card_run", "CardRun")
    selected = real._struct_get(
        card_run, "hero_selected_card_ids", "HeroSelectedCardIds"
    )
    out["heroSelectedCards"] = [str(value) for value in list(selected or [])]
    roster = real._struct_get(card_run, "companion_roster", "CompanionRoster")
    companions = real._struct_get(
        roster, "permanent_companions", "PermanentCompanions"
    )
    out["companionInstanceIds"] = sorted(
        str(real._struct_get(value, "instance_id", "InstanceId"))
        for value in list(companions or [])
    )
    out["activePermanentCompanion"] = str(
        real._struct_get(
            roster,
            "active_permanent_companion_instance_id",
            "ActivePermanentCompanionInstanceId",
        )
    )
    out["activeTemporaryQuestNpc"] = str(
        real._struct_get(
            card_run,
            "active_temporary_quest_npc_id",
            "ActiveTemporaryQuestNpcId",
        )
    )
    formation = real._struct_get(card_run, "ordered_formation", "OrderedFormation")
    out["formation"] = str(formation)
    out["subsystemMethods"] = [
        name
        for name in dir(subsystem)
        if "combat_basics_tutorial" in name
    ]
    if hasattr(subsystem, "get_combat_basics_tutorial_route_progress_copy"):
        route = subsystem.get_combat_basics_tutorial_route_progress_copy()
        out["route"] = {
            "current": str(real._struct_get(route, "current_node_id", "CurrentNodeId")),
            "reachable": [
                str(value)
                for value in list(
                    real._struct_get(route, "reachable_node_ids", "ReachableNodeIds")
                    or []
                )
            ],
            "completed": [
                str(value)
                for value in list(
                    real._struct_get(route, "completed_node_ids", "CompletedNodeIds")
                    or []
                )
            ],
        }
except Exception as exc:
    out["runtimeError"] = str(exc)
print(json.dumps(out, ensure_ascii=False, indent=2))
