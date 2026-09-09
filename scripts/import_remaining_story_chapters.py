"""Import and reload-check the completed chapter previews through the project MCP."""
import hashlib
import json
from pathlib import Path
from ue_mcp_client import UnrealMCPClient

ROOT=Path(__file__).resolve().parents[1]
manifest=json.loads((ROOT/'SourceArt/UI/StoryNodes/manifest.json').read_text(encoding='utf-8'))['nodes']
campaign=json.loads((ROOT/'SourceAssets/Narrative/MainStory/campaign.json').read_text(encoding='utf-8'))
nodes=[n for c in campaign['chapters'][1:] for n in c['nodes'] if n['id']!='S02-02']
assert len(nodes)==53
for node in nodes:
    entry=manifest[node['id']]
    assert entry['visual_review']=='pass' and entry['size']==[1536,512]
    assert entry.get('review_batch')==node['id'][:3]
    assert hashlib.sha256((ROOT/entry['file']).read_bytes()).hexdigest()==entry['sha256']==entry['reviewed_sha256']
    contract=hashlib.sha256(json.dumps({'art':node['art'],'result':node['result']},ensure_ascii=False,sort_keys=True).encode('utf-8')).hexdigest()
    assert entry['contract_sha256']==contract
client=UnrealMCPClient(timeout=90)
assert client.connect()
out=ROOT/'Saved/StorySystem/RemainingChapterImports';out.mkdir(parents=True,exist_ok=True)
report_path=out/'report.json'
previous=json.loads(report_path.read_text(encoding='utf-8')).get('nodes',{}) if report_path.exists() else {}
reports={}
for node in nodes:
    node_id=node['id'];entry=manifest[node_id]
    prior=previous.get(node_id,{})
    if prior.get('import',{}).get('source_sha256')==entry['sha256'] and prior.get('reload',{}).get('reload_verified'):
        reports[node_id]=prior
    else:
        response=client.run_project_python_file('Content/Python/gamexxk_import_reviewed_story_node.py',[node_id,'--chapter-preview'])
        assert response.get('success'),response
        imported=json.loads(response['stdout'].strip().splitlines()[-1]);assert imported['complete']
        response=client.run_project_python_file('Content/Python/gamexxk_verify_reviewed_story_node.py',[node_id,'--chapter-preview'])
        assert response.get('success'),response
        reloaded=json.loads(response['stdout'].strip().splitlines()[-1]);assert reloaded['reload_verified']
        reports[node_id]={'import':imported,'reload':reloaded}
    temporary=report_path.with_suffix('.json.tmp')
    temporary.write_text(json.dumps({'complete':False,'nodes':reports},ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    temporary.replace(report_path)
    print(json.dumps({'node':node_id,'progress':len(reports),'total':len(nodes),'format':reports[node_id]['reload']['audit']['format']},ensure_ascii=False),flush=True)
saved=client.save_dirty_packages();assert saved.get('save_result') and not saved.get('dirty_after'),saved
report={'complete':True,'count':len(reports),'nodes':reports,'save':saved,'preserved':['S00-01','S00-02','S00-03','S00-04','S00-05','S00-06','S00-07','S02-02']}
report_path.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'complete':True,'count':len(reports),'report':str(report_path)},ensure_ascii=False),flush=True)
