"""Import the explicitly authorized S00 chapter-review art through project UE MCP."""
import json
from pathlib import Path
from ue_mcp_client import UnrealMCPClient

ROOT=Path(__file__).resolve().parents[1]
client=UnrealMCPClient(timeout=90)
assert client.connect()
reports=[]
for number in range(1,8):
    node=f'S00-{number:02d}'
    response=client.run_project_python_file('Content/Python/gamexxk_import_reviewed_story_node.py',[node,'--chapter-preview'])
    report=json.loads(response['stdout'].strip().splitlines()[-1])
    assert report['complete']
    reports.append(report)
    print(json.dumps({'node':node,'size':[report['audit']['width'],report['audit']['height']],'format':report['audit']['format']},ensure_ascii=False),flush=True)
path=ROOT/'Saved/StorySystem/S00-chapter-import.json'
path.write_text(json.dumps(reports,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(client.save_dirty_packages()))
