"""Temporary, guarded scenes and real UI actions for essential audio requirement videos."""
import argparse
import builtins
import json
import time
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / "Saved/Codex/EssentialSfxRequirements-20260914"
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world and "L_DesktopTrainingHUD" in world.get_path_name()
pc = unreal.GameplayStatics.get_player_controller(world, 0)
wb = pc.get_desktop_training_workbench_widget_for_test()
mvp = wb.get_mvp_subsystem()
gi = unreal.GameplayStatics.get_game_instance(world)
dev = next(o for o in unreal.ObjectIterator(unreal.GameXXKDevToolsSubsystem) if o.get_outer() == gi)


def write(name, value):
    (OUT / name).write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding="utf-8")


def command(name, args=None):
    result = json.loads(dev.execute_json(json.dumps({"command":name,"args":args or {}},ensure_ascii=False)))
    assert result.get("ok"), result
    return result.get("data")


def snapshot():
    state = mvp.get_runtime_state_copy()
    battle = state.card_run.active_battle
    board = pc.get_battle_board_widget_for_test()
    return {"wall":time.time(),"mono":time.monotonic(),"screen":str(state.screen),"dev":dev.is_session_active(),"player_level":state.player_level,
            "audio":json.loads(unreal.GameXXKSfxLibrary.get_diagnostics(world)),
            "battle_active":state.card_run.has_active_card_battle,"phase":str(battle.phase) if state.card_run.has_active_card_battle else "Inactive",
            "hand":[{"instance":str(c.instance_id),"card":str(c.card_id),"owner":str(c.owner_unit_id)} for c in battle.deck.hand],
            "units":[{"id":str(u.unit_id),"side":str(u.side),"hp":u.hp,"max_hp":u.max_hp,"armor":u.armor} for u in battle.units],
            "board":board.get_battle_board_debug_state_for_test() if board else None}


def prepare(scenario, card="", variant=""):
    assert dev.is_session_active()
    mvp.clear_card_tooltip_fixture_for_test()
    mvp.clear_target_outcome_fixture_for_test()
    command("battle.restart")
    scene = command("snapshot.export")
    battle=scene["state"]["cardRun"]["activeBattle"]
    parties=[u for u in battle["units"] if u["side"]=="Party"]
    enemies=[u for u in battle["units"] if u["side"]=="Enemy"]
    for unit in parties+enemies:
        unit.update(hp=120,maxHP=120,armor=0,attack=20 if unit in parties else 10,defense=0)
        if unit in parties:unit.update(mana=100,maxMana=100)
    if variant=="heavy":parties[0]["attack"]=80
    if variant=="buff":parties[0]["defense"]=30
    if variant=="critical":
        battle["talentCriticalChancePercent"]=20
        battle["talentCriticalDamagePercent"]=50
        # Legal 20% talent rate; seed 273 produces agility roll 8, critical roll 19.
        battle["combatRandomState"]=273
    if variant=="ice":parties[0]["armor"]=40
    if variant=="victory":
        enemies[0]["hp"]=1
        for unit in enemies[1:]:unit.update(hp=0,bLiving=False)
    if variant=="defeat":
        for unit in parties:unit["hp"]=1
        for unit in enemies:unit["attack"]=500
    if scenario=="Outcome.ArmorBlocked":enemies[0]["armor"]=99
    if scenario=="Outcome.Lethal":enemies[0]["hp"]=10
    if scenario=="Outcome.Healing":parties[0]["hp"]=60
    selected_card=card or ("Hero.Generic.GuiYuanShu" if scenario=="Outcome.Healing" else "Hero.Generic.QingFengYiShi")
    if card and "Sorcerer" in card:
        owner=parties[1];owner["role"]="Sorcerer"
        deck=battle["deck"];deck["drawPile"]+=deck["hand"];deck["hand"]=[]
        owned=[c for zone in ("drawPile","discardPile","exhaustPile") for c in deck[zone] if c["ownerUnitId"]==owner["unitId"]]
        assert len(owned)==5,"Sorcerer sample requires the normal five-card companion loadout"
        pool=list(dict.fromkeys([card,"Profession.Sorcerer.LingHuoFu","Profession.Sorcerer.JuLing","Profession.Sorcerer.SheLingHuo","Profession.Sorcerer.ChiXiaoFenXing","Profession.Sorcerer.YanMuHuTi"]))[:5]
        for item,definition in zip(owned,pool):item["cardId"]=definition
        first=owned[0];deck["drawPile"]=[c for c in deck["drawPile"] if c["instanceId"]!=first["instanceId"]];deck["hand"]=[first]
    else:
        first=dict(battle["deck"]["hand"][0]);first.update(cardId=selected_card,ownerUnitId=parties[0]["unitId"])
        battle["deck"]["drawPile"] += battle["deck"]["hand"][1:]
        battle["deck"]["hand"]=[first]
    battle["deck"]["sharedEnergy"]=20
    if variant=="reject":battle["deck"]["sharedEnergy"]=0
    battle["phase"]="Player"
    s=scene["state"];s["playerMaxHP"]=max(s["playerMaxHP"],s["playerHP"]);s["playerMaxMP"]=max(s["playerMaxMP"],s["playerMP"])
    write("prepared-"+a.name+".json",scene)
    command("snapshot.import", {"scene":scene})
    pc.refresh_player_flow_widgets_for_test()
    write("scene-"+a.name+".json", command("snapshot.export"))


def action():
    assert dev.is_session_active()
    board = pc.get_battle_board_widget_for_test()
    before = snapshot()
    if a.action in ("play", "reject"):
        battle = mvp.get_runtime_state_copy().card_run.active_battle
        card = battle.deck.hand[0]
        accepted = board.click_card_in_hand(card.instance_id)
        if a.action == "reject":
            assert not accepted, "Expected invalid play to be rejected"
            write(a.name+"-action.json",{"before":before,"after":snapshot(),"action":a.action,"accepted":accepted})
            return
        assert accepted, "Card selection rejected"
        if board.is_card_targeting_active():
            target = next(u for u in battle.units if u.hp > 0 and (("ENEMY" in str(u.side)) != a.ally))
            assert board.confirm_targeting_unit(target.unit_id), "Card target rejected"
    elif a.action == "end-turn":
        assert board.end_card_player_phase()
    elif a.action == "level-up":
        # Invoke the same reward transaction while leaving the current property tab visible.
        mvp.collect_training_travel_rewards()
        assert mvp.get_runtime_state_copy().player_level>before["player_level"]
    else:
        wb.handle_desktop_action_for_test(a.id)
    write(a.name+"-action.json",{"before":before,"after":snapshot(),"action":a.action,"id":a.id})


def record_begin():
    assert dev.is_session_active()
    unreal.GameXXKSfxLibrary.reset_diagnostics(world)
    record = {"start":time.time(),"events":[],"last":{},"handle":None}
    unreal.AudioMixerLibrary.start_recording_output(world,90.0)
    def tick(delta):
        diag = json.loads(unreal.GameXXKSfxLibrary.get_diagnostics(world))
        if diag["played"] != record["last"]:
            record["events"].append({"wall":time.time(),"audio":diag})
            record["last"] = diag["played"]
    record["handle"] = unreal.register_slate_post_tick_callback(tick)
    setattr(builtins,"_sfx_requirements_record",record)
    write(a.name+"-start.json",{"wall":record["start"],"state":snapshot()})


p = argparse.ArgumentParser()
p.add_argument("phase",choices=["init","resume","state","prepare","action","record-start","record-stop","restore","import","leave-battle","reward-prepare","level-prepare","attributes","fresh-battle"])
p.add_argument("--scenario",default="Outcome.Single")
p.add_argument("--card",default="")
p.add_argument("--variant",default="")
p.add_argument("--name",default="probe")
p.add_argument("--action",choices=["play","reject","end-turn","desktop","level-up"],default="play")
p.add_argument("--id",type=int,default=600)
p.add_argument("--ally",action="store_true")
p.add_argument("--output",default="")
a = p.parse_args()
if a.output:
    OUT=(ROOT/a.output).resolve()
    assert OUT.is_relative_to(ROOT/'Saved/Codex')
    OUT.mkdir(parents=True,exist_ok=True)
if a.phase == "init":
    assert not dev.is_session_active() and not (OUT/"runtime-before.json").exists()
    command("session.begin")
    write("runtime-before.json",command("snapshot.export"))
    write("ui-before.json",{"muted":wb.is_muted_for_test(),"never_disable":unreal.SystemLibrary.get_console_variable_int_value("au.NeverDisableSubmixes")})
    unreal.SystemLibrary.execute_console_command(world,"au.NeverDisableSubmixes 1")
    if wb.is_muted_for_test():wb.handle_desktop_action_for_test(17)
    command("battle.start",{"stage":"Training.Normal.1-1","encounter":1,"seed":20260914})
elif a.phase == "resume":
    assert a.output and not dev.is_session_active()
    command("session.begin")
    write("runtime-resume-"+a.name+".json",command("snapshot.export"))
    unreal.SystemLibrary.execute_console_command(world,"au.NeverDisableSubmixes 1")
    wb.open_backpack()
elif a.phase == "prepare":prepare(a.scenario,a.card,a.variant)
elif a.phase == "leave-battle":
    assert dev.is_session_active()
    mvp.cancel_training_challenge_to_workbench()
    pc.refresh_player_flow_widgets_for_test()
elif a.phase == "fresh-battle":
    command("battle.start",{"stage":"Training.Normal.1-1","encounter":1,"seed":20260915})
elif a.phase == "reward-prepare":
    assert dev.is_session_active()
    assert mvp.start_game()
    scene=command("snapshot.export")
    scene["state"]["training"]["ownedChestTokens"]=[{"tier":"NormalChest","sourceStageId":"Training.Normal.1-1","sourceItemLevel":5,"acquisitionOrdinal":1}]
    scene["state"]["training"]["nextChestAcquisitionOrdinal"]=1
    scene["state"]["training"]["nextChestOpenOrdinal"]=0
    scene["state"]["training"]["bTravelActive"]=False
    scene["state"]["training"]["activeTravelEncounterIndex"]=-1
    command("snapshot.import",{"scene":scene})
    wb.open_backpack()
    pc.refresh_player_flow_widgets_for_test()
elif a.phase == "level-prepare":
    assert dev.is_session_active()
    scene=command("snapshot.export")
    state=scene["state"]
    state["playerXP"]=state["playerLevel"]*100-1
    state["training"].update(pendingTravelExperience=2,pendingTravelGold=0,pendingTravelNormalChestCount=0,pendingTravelAdvancedChestCount=0,pendingTravelHuntChestCount=0,pendingTravelCompletedEncounters=1,pendingTravelCompletedStages=0,bTravelActive=False,activeTravelEncounterIndex=-1)
    command("snapshot.import",{"scene":scene})
    wb.open_backpack()
    pc.refresh_player_flow_widgets_for_test()
elif a.phase == "attributes":
    widgets=[o for o in unreal.ObjectIterator(unreal.GameXXKInventoryWindowWidget) if o.get_owning_player()==pc and o.is_visible()]
    assert len(widgets)==1,len(widgets)
    assert widgets[0].open_character_backpack_tab_for_test(unreal.GameXXKCharacterBackpackTab.ATTRIBUTES)
elif a.phase == "action":action()
elif a.phase == "record-start":record_begin()
elif a.phase == "record-stop":
    record=getattr(builtins,"_sfx_requirements_record")
    unreal.unregister_slate_post_tick_callback(record["handle"])
    unreal.AudioMixerLibrary.stop_recording_output(world,unreal.AudioRecordingExportType.WAV_FILE,a.name,str(OUT))
    write(a.name+"-stop.json",{"wall":time.time(),"events":record["events"],"state":snapshot()})
    if a.output:write(a.name+"-scene-after.json",command("snapshot.export"))
elif a.phase == "restore":
    mvp.clear_card_tooltip_fixture_for_test();mvp.clear_target_outcome_fixture_for_test()
    command("session.restore")
    ui=json.loads((OUT/"ui-before.json").read_text(encoding="utf-8"))
    if wb.is_muted_for_test()!=ui["muted"]:wb.handle_desktop_action_for_test(17)
    unreal.SystemLibrary.execute_console_command(world,"au.NeverDisableSubmixes "+str(ui["never_disable"]))
elif a.phase == "import":
    scene=json.loads((OUT/(a.name+"-scene.json")).read_text(encoding="utf-8"))
    command("snapshot.import",{"scene":scene})
result=snapshot();write("latest.json",result);print(json.dumps(result,ensure_ascii=False))
