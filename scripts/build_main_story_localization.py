"""Build a reviewed main-story fragment; never overwrite the shared runtime dictionary."""
import argparse
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
FOLDER = ROOT/'Content/Localization/GameXXK/MainStory'
NAMESPACE = 'GameXXKMainStoryUI'
NAMES = {'narrator':'Narrator','hero':'Hero','you_bai':'Youbai','zhou_guang_zu':'Zhou Guangzu',
         'jin_gui':'Jin Gui','tusi':'Tusi Chief','song_jin_bao':'Song Jinbao','qiong_yao_er':'Qiong Yaoer',
         'driver':'Old Driver','mountain_man':'Mountain Traveler','abbot':'Guoqing Abbot',
         'innkeeper':'Odd Innkeeper','woodcutter':'Woodcutter','hunter':'Stranded Hunter',
         'willow_scholar':'Master Willow','boatman':'Boatman','villager':'Riverbend Villager',
         'elder':'Riverbend Elder','poor_traveler':'Old Traveler','porter':'Porter'}

def build(partial=False):
    campaign=json.loads((ROOT/'SourceAssets/Narrative/MainStory/campaign.json').read_text(encoding='utf-8'))
    rows=[];positions=[];seen={};chapters=[];compact_seen={}
    def add(key,zh,en):
        assert en and isinstance(en,str) and not re.search(r'[\u3400-\u9fff]',en), (key,en)
        positions.append({'key':key,'zh-Hans':zh,'en':en})
        if zh in seen:
            assert seen[zh]==en, ('Conflicting translation for the same source',key,zh,seen[zh],en)
            return
        seen[zh]=en
        rows.append({'namespace':NAMESPACE,'key':key,'zh-Hans':zh,'en':en})
    for actor,definition in campaign['characters'].items():
        add('Character.'+actor,definition['name'],NAMES[actor])
    for chapter in campaign['chapters']:
        path=FOLDER/(chapter['id']+'.en.json')
        if not path.exists():
            assert partial, 'Missing chapter '+chapter['id']
            continue
        en=json.loads(path.read_text(encoding='utf-8'))
        assert en['id']==chapter['id'] and set(en['nodes'])=={n['id'] for n in chapter['nodes']}
        chapters.append(chapter['id'])
        for field in ('title','summary'):add(chapter['id']+'.'+field,chapter[field],en[field])
        add(chapter['id']+'.location',chapter['title'].split('·')[0],en['location'])
        for node in chapter['nodes']:
            english=en['nodes'][node['id']];prefix=node['id']+'.'
            for field in ('title','summary','objective','result'):add(prefix+field,node[field],english[field])
            if 'short_title' in english:
                short=english['short_title'];assert short and not re.search(r'[\u3400-\u9fff]',short)
                if node['title'] in compact_seen:assert compact_seen[node['title']]==short
                else:
                    compact_seen[node['title']]=short
                    rows.append({'namespace':NAMESPACE,'key':prefix+'short_title','zh-Hans':node['title'],'en':short,'usage':'compact'})
            for field in ('lines','after_battle_lines'):
                source=node.get(field,[]);translated=english.get(field,[])
                assert len(source)==len(translated), (node['id'],field,len(source),len(translated))
                for i,(pair,text) in enumerate(zip(source,translated)):add(prefix+field+'.'+str(i),pair[1],text)
            hints=english.get('hints')
            if hints is None:
                assert node['hints']==['先听清这段交流，留意'+node['objective']+'。','这一步要做的是：'+node['objective']+'。',
                                       '把本段对话听完并确认结论：'+node['result']], (node['id'],'nonstandard hints need translation')
                hints=['Listen carefully. Focus: '+english['objective']+'.', 'Next step: '+english['objective']+'.',
                       'Finish the conversation and confirm: '+english['result']]
            assert len(hints)==len(node['hints'])
            for i,(zh,text) in enumerate(zip(node['hints'],hints)):add(prefix+'hint.'+str(i),zh,text)
            options=english.get('options',[])
            assert len(options)==len(node['options']), (node['id'],'options')
            for i,(source,pair) in enumerate(zip(node['options'],options)):
                assert len(pair)==2
                for field,text in zip(('text','feedback'),pair):add(prefix+'option.'+str(i)+'.'+field,source[field],text)
    ui=FOLDER/'UI.en.json'
    if ui.exists():
        for row in json.loads(ui.read_text(encoding='utf-8'))['entries']:
            add(row['key'],row['zh-Hans'],row['en'])
            if 'nativePattern' in row:rows[-1]['nativePattern']=row['nativePattern']
        for key,zh,en in [('Available','可进行','Available'),('Active','进行中','In progress'),
                         ('Completed','待领奖','Claim reward'),('Rewarded','已领奖','Claimed'),('Locked','未开放','Locked')]:
            add('Side.'+key,'支线 · '+zh,'Side · '+en)
        for chapter in campaign['chapters']:
            number=chapter['stage_number'];stage=f'{(number-1)//3+1}-{(number-1)%3+1}'
            add(chapter['id']+'.enter','进入'+stage,'Enter '+stage)
        for node in [n for c in campaign['chapters'] for n in c['nodes']]:
            reward=node['reward'];zh=f"报酬：{reward['gold']//10000}万金币";en=f"Reward: {reward['gold']:,} gold"
            if reward['advanced_boxes'] or reward['normal_boxes']:
                zh+=f" · {reward['box_level']}级高级箱{reward['advanced_boxes']}个、普通箱{reward['normal_boxes']}个"
                en+=f" · Lv. {reward['box_level']}: {reward['advanced_boxes']} Advanced, {reward['normal_boxes']} Normal Chests"
            add(node['id']+'.reward',zh,en)
    else:assert partial, 'Missing UI translations'
    out=ROOT/'Content/Localization/GameXXK/main-story.entries.json'
    out.write_text(json.dumps({'entries':rows},ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    report={'complete':len(chapters)==6 and ui.exists(),'chapters':chapters,'source_positions':len(positions),'unique_entries':len(rows)}
    target=ROOT/'Saved/StorySystem/Localization';target.mkdir(parents=True,exist_ok=True)
    (target/'coverage.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    (target/'bilingual-review.json').write_text(json.dumps(positions,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(report))

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--partial',action='store_true')
    build(parser.parse_args().partial)
