"""Capture the real failing battle-save payload without committing a player battle."""
import hashlib
import json
from pathlib import Path
import unreal

root=Path(__file__).resolve().parents[2];out=root/'Saved/StorySystem/GateSaveFix';out.mkdir(parents=True,exist_ok=True)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world and 'L_DesktopTrainingHUD' in world.get_name()
instance=unreal.GameplayStatics.get_game_instance(world)
mvp=next(x for x in unreal.ObjectIterator(unreal.GameXXKMVPSubsystem) if x.get_outer()==instance)
dev=next(x for x in unreal.ObjectIterator(unreal.GameXXKDevToolsSubsystem) if x.get_outer()==instance)
def command(name,args=None):
    r=json.loads(dev.execute_json(json.dumps({'schema':1,'command':name,'args':args or {}})))
    assert r['ok'],r
    return r
before=command('snapshot.export');assert not before['session_active']
(out/'player-before.json').write_text(json.dumps(before,ensure_ascii=False,indent=2),encoding='utf-8')
protected=[p for p in (root/'Saved/SaveGames').glob('*.sav') if 'SaveSlot' in p.name or 'DesktopTraining' in p.name]
hashes={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in protected}
original=mvp.get_runtime_state_copy();report={}
try:
    command('snapshot.import',{'scene':before['data']})
    gates=set(original.narrative_progress.main_story.gate_node_ids)
    gate=next(x for x in original.reachable_route_node_ids if x in gates)
    assert mvp.select_training_challenge_route_node(gate)
    candidate=mvp.get_runtime_state_copy();assert candidate.card_run.has_active_card_battle
    captured=command('snapshot.export');(out/'battle-candidate.json').write_text(json.dumps(captured,ensure_ascii=False,indent=2),encoding='utf-8')
    raw=unreal.GameplayStatics.create_save_game_object(unreal.GameXXKSaveGame)
    raw.set_editor_property('save_state',unreal.GameXXKMVPRules.make_save_state(candidate))
    assert unreal.GameplayStatics.save_game_to_slot(raw,'GameXXK_StoryGateRawDiagnostic',0)
    loaded=unreal.GameplayStatics.load_game_from_slot('GameXXK_StoryGateRawDiagnostic',0)
    report['raw_engine_roundtrip']=loaded is not None
    if hasattr(raw.get_editor_property('save_state'),'export_text'):
        (out/'state-before.txt').write_text(raw.get_editor_property('save_state').export_text(),encoding='utf-8')
        (out/'state-after.txt').write_text(loaded.get_editor_property('save_state').export_text(),encoding='utf-8')
    fixture=unreal.new_object(unreal.GameXXKMVPSubsystem,outer=unreal.new_object(unreal.GameInstance))
    fixture.set_editor_property('runtime_state',original)
    report['map_sealed_write']=fixture.save_current_game('GameXXK_StoryMapSealedDiagnostic',0)
    report['map_error']=str(fixture.get_last_save_load_error())
    fixture.set_editor_property('runtime_state',candidate)
    report['battle_sealed_write']=fixture.save_current_game('GameXXK_StoryGateSealedDiagnostic',0)
    report['battle_error']=str(fixture.get_last_save_load_error())
finally:
    report['restore']=command('session.restore')
    after=command('snapshot.export')
    report['player_state_restored']=after['data']['state']==before['data']['state']
    report['player_saves_unchanged']=all(Path(p).exists() and hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in hashes.items())
    (out/'reproduction.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    assert report['player_state_restored'] and report['player_saves_unchanged'],report
print(json.dumps(report,ensure_ascii=False))
