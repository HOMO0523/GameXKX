"""Repeat the observed desktop task/dialogue close regression in an isolated PIE save."""
import json
from pathlib import Path
import subprocess
import sys
import time
from ue_mcp_client import UnrealMCPClient

ROOT=Path(__file__).resolve().parents[1]
def ui(*args):
    return subprocess.check_output([sys.executable,'-B','-X','utf8',str(ROOT/'scripts/main_story_ui_check.py'),*args,'--native-input'],cwd=ROOT,text=True,encoding='utf-8')
def probe():
    return json.loads(ui('probe'))
def expect(text):
    snapshot=ui('snapshot')
    assert text in snapshot, (text,snapshot)
    return snapshot

before=probe()
assert any(x in before['saved_dir'] for x in ('ImageOptimization/PIEUser/','StorySystem/VisualV2User/','StorySystem/TalentTotalsUser/')) and not before['route_active']
catalog=json.loads((ROOT/'SourceAssets/Narrative/MainStory/campaign.json').read_text(encoding='utf-8'))
lines=catalog['chapters'][0]['nodes'][0]['lines']
client=UnrealMCPClient(timeout=30)
assert client.connect()
records=[]
for cycle in range(3):
    ui('click','任务');ui('click','天台山');ui('click','天台山脚')
    expect('回看对白')
    images=json.loads(client.run_project_python_file('Content/Python/gamexxk_probe_story_images.py')['stdout'])
    assert any(x['texture'] and 'S00_01' in x['texture'] for x in images), images
    ui('click','回看对白')
    start=probe()
    expect(lines[0][1])
    for _,text in lines[1:4]:
        ui('continue');expect(text)
        assert probe()['layout_builds']==start['layout_builds']
    ui('close');expect('天台山·祖传也会指错路')
    ui('close');expect('关闭背包与全部子界面')
    ui('click','天赋');expect('天赋修行')
    ui('click','编队');expect('出战三人')
    ui('probe','["gc"]');time.sleep(.3)
    ui('click','天赋');expect('天赋修行')
    ui('click','编队');expect('出战三人')
    after=probe()
    assert after['story']==before['story'], (before['story'],after['story'])
    records.append({'cycle':cycle+1,'dialogue_build_count':start['layout_builds'],
                    'after_build_count':after['layout_builds'],'story_unchanged':True,
                    'result_image_loaded':True,'clicks_advance_one_line':True,'navigation_after_gc':True})
    print(json.dumps(records[-1],ensure_ascii=False),flush=True)
(ROOT/'Saved/StorySystem/stable-ui-verification.json').write_text(json.dumps(records,ensure_ascii=False,indent=2),encoding='utf-8')
