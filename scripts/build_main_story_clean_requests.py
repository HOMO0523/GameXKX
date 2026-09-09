"""Build dramatic keyframe requests; old rejected compositions are never inputs."""
import hashlib
import json
from pathlib import Path
from build_main_story_content import image_request
from main_story_authoring import ROOT, CHARACTERS

campaign=json.loads((ROOT/'SourceAssets/Narrative/MainStory/campaign.json').read_text(encoding='utf-8'))
requests=[]
for chapter in campaign['chapters']:
    for n in chapter['nodes']:
        if n.get('script_revision',1) < 2 or not n['art'].get('key_moment') or not n['art'].get('camera'):
            continue
        requests.append(image_request(n))
path=ROOT/'SourceAssets/Narrative/MainStory/clean-redraw-requests.json'
path.write_text(json.dumps(requests,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'requests':len(requests),'path':str(path)}))
