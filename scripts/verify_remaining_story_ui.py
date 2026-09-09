"""Bounded default-2D art/portrait replay check; restore the public Dev session."""
import argparse
import json
from pathlib import Path
import re
import subprocess
import sys
import time
from ue_mcp_client import UnrealMCPClient

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Saved/StorySystem/RemainingChapterImports'
campaign=json.loads((ROOT/'SourceAssets/Narrative/MainStory/campaign.json').read_text(encoding='utf-8'))
parser=argparse.ArgumentParser()
parser.add_argument('--port',type=int,default=18765)
parser.add_argument('--native-coordinate-scale',type=float,default=1.0)
parser.add_argument('--resume',action='store_true')
args=parser.parse_args()
client=UnrealMCPClient(timeout=60,port=args.port)
assert client.connect() and not client.is_in_pie(),'Requires an explicitly handed-over stopped editor'
scale_path=ROOT/'Saved/Config/GameXXKDesktopHudSettings.ini'
scale_text=scale_path.read_text(encoding='utf-8-sig') if scale_path.exists() else ''
match=re.search(r'^HudScalePercent=(\d+)',scale_text,re.M)
original_scale=int(match.group(1)) if match else 100
assert original_scale in (50,75,100)
report={'complete':False,'original_scale':original_scale,'chapter_screenshots':[],'portraits':[],'scope':'Restorable art replay only, not progression acceptance'}
if args.resume:
    previous=json.loads((OUT/'ui-check.json').read_text(encoding='utf-8'))
    assert previous.get('restore',{}).get('ok') and not previous.get('restore',{}).get('session_active')
    report['chapter_screenshots']=previous['chapter_screenshots']
    report['portraits']=previous['portraits']
    report['resumed_after_external_window_occlusion']=True

def py(path,args):
    result=client.run_project_python_file(path,args)
    assert result.get('success'),result
    return json.loads(result['stdout'].strip().splitlines()[-1])

def probe(args):
    result=py('Content/Python/gamexxk_main_story_probe.py',args)
    assert result.get('ok'),result
    return result

def check(action,value=''):
    command=[sys.executable,'-B','-X','utf8',str(ROOT/'scripts/main_story_ui_check.py'),action]
    if value:command.append(value)
    command+=['--window',window,'--port',str(args.port),
              '--native-coordinate-scale',str(args.native_coordinate_scale)]
    if action in ('click','close','continue'):command.append('--native-input')
    result=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,encoding='utf-8')
    if result.returncode:raise RuntimeError(result.stdout+'\n'+result.stderr)
    return result.stdout.strip()

def capture(name):
    time.sleep(.35)
    result=check('capture',name)
    path=ROOT/'Saved/StorySystem'/(name+'.png')
    assert path.exists() and path.stat().st_size>10000,result
    return str(path)

def focus_node(node_id):
    chapter=next(c for c in campaign['chapters'] if c['id']==node_id[:3]);depth={}
    for _ in chapter['nodes']:
        for n in chapter['nodes']:
            depth[n['id']]=max([depth.get(p,0)+1 for p in n['requires_all']+n['requires_any']] or [0])
    py('Content/Python/gamexxk_story_art_preview_fixture.py',['scroll',str(max(0,22+depth[node_id]*306-50))])
    time.sleep(.25)

actors={
    'S01':[('woodcutter','S01-06'),('hunter','S01-04'),('willow_scholar','S01-10')],
    'S02':[('boatman','S02-01'),('poor_traveler','S02-03'),('villager','S02-04'),('elder','S02-06')],
    'S03':[('porter','S03-03')],
}
started=False;fixture=False
try:
    client.start_pie(warmup_seconds=2,play_mode='PlayMode_InEditorFloating');started=True
    baseline=probe(['observe'])
    assert baseline['world'].find('L_DesktopTrainingHUD')>=0
    assert not baseline['route_active'] and not baseline['card_battle']
    report['baseline']={k:baseline[k] for k in ('world','saved_dir','screen','route_active','card_battle','gold','layout_builds')}
    result=py('Content/Python/gamexxk_story_art_preview_fixture.py',['show-chapter','S01']);fixture=True
    assert result['writes_suppressed']
    probe(['ui-action','656'])
    time.sleep(.5)
    roots=client.call_tool('Snapshot',{'ref':'','maxDepth':0},toolset_name='SlateInspectorToolset.SlateInspectorToolset')
    window='GameXXKDesktopOverlay' if 'GameXXKDesktopOverlay' in roots else 'GameXXK Preview'
    report['window']=window
    for index in range(1,6):
        chapter_id=f'S{index:02d}'
        if index!=1:
            probe(['chapter',chapter_id]);probe(['ui-action',str(2100+index)])
        py('Content/Python/gamexxk_story_art_preview_fixture.py',['scroll','0'])
        if not any(row['chapter']==chapter_id for row in report['chapter_screenshots']):
            report['chapter_screenshots'].append({'chapter':chapter_id,'path':capture('approved-'+chapter_id+'-tree')})
        for actor,node_id in actors.get(chapter_id,[]):
            if any(row['actor']==actor for row in report['portraits']):continue
            node=next(n for c in campaign['chapters'] for n in c['nodes'] if n['id']==node_id)
            assert node['lines'][0][0]==actor
            focus_node(node_id)
            check('click',node['title']);time.sleep(.2)
            check('click','回看对白');time.sleep(.25)
            text=check('snapshot')
            name=campaign['characters'][actor]['name']
            assert name in text,(actor,'speaker name missing from visible Slate tree')
            screenshot=capture('approved-portrait-'+actor)
            check('close');time.sleep(.25)
            report['portraits'].append({'actor':actor,'name':name,'node':node_id,'path':screenshot,'native_replay_clicked':True,'native_close_clicked':True})
            (OUT/'ui-check.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
        print(json.dumps({'chapter':chapter_id,'portrait_count':len(report['portraits'])},ensure_ascii=False),flush=True)
    before=probe(['observe']);assert before['backpack_open']
    probe(['ui-action','63']);probe(['ui-action','60']);time.sleep(.25)
    after=probe(['observe']);assert not after['backpack_open']
    report['backpack_close']={'before_layout_builds':before['layout_builds'],'after_layout_builds':after['layout_builds'],'closed':True,'path':capture('approved-story-backpack-closed')}
    report['complete']=len(report['portraits'])==8 and len(report['chapter_screenshots'])==5
finally:
    if started:
        if fixture:
            report['restore']=py('Content/Python/gamexxk_story_art_preview_fixture.py',['restore'])
        probe(['ui-action',str({50:651,75:656,100:650}[original_scale])])
        report['final_runtime']=probe(['observe'])
        client.stop_pie();assert client.wait_for_pie_state(False)
        report['save']=client.save_dirty_packages()
        assert report['save'].get('save_result') and not report['save'].get('dirty_after')
    (OUT/'ui-check.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'complete':report['complete'],'portraits':len(report['portraits']),'chapters':len(report['chapter_screenshots']),'report':str(OUT/'ui-check.json')},ensure_ascii=False))
