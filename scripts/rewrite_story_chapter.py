"""Apply an authored dialogue/keyframe revision without changing task topology or rewards."""
import argparse
import json
from pathlib import Path
import re

ROOT=Path(__file__).resolve().parents[1]

def render(chapter):
    out=['from main_story_authoring import n, chapter, option', '', '# Plot revision 2: preserve stable task IDs and dependencies; illustrate the dramatic beat.',
         f'CHAPTER = chapter({chapter["id"]!r}, {chapter["title"]!r}, {chapter["summary"]!r}, {chapter["mainline_end"]!r}, [']
    for node in chapter['nodes']:
        out.append(f'    n({node["id"]!r}, {node["title"]!r}, {node["summary"]!r}, {node["objective"]!r}, [')
        out.extend(f'        ({speaker!r}, {line!r}),' for speaker,line in node['lines'])
        out.append(f'    ], {node["result"]!r},')
        out.append(f'    {node["art"]["scene"]!r},')
        out.append(f'    kind={node["kind"]!r}, all={node["requires_all"]!r}, any={node["requires_any"]!r}, optional={node["optional"]!r}, inside={node["inside_journey"]!r},')
        options=', '.join(f'option({o["text"]!r}, {o["correct"]!r}, {o["feedback"]!r})' for o in node['options'])
        out.append(f'    options=[{options}], hints={node["hints"]!r},')
        out.append(f'    cast={node["art"]["cast"]!r}, mood={node["art"]["mood"]!r}, facts={node["art"]["facts"]!r},')
        out.append(f'    highlight={node["art"]["key_moment"]!r}, camera={node["art"]["camera"]!r}),')
    out.append('])')
    return '\n'.join(out)+'\n'

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('revision')
    args=parser.parse_args()
    patch=json.loads(Path(args.revision).read_text(encoding='utf-8'))
    assert re.fullmatch(r'S0[0-5]',patch['id'])
    data=json.loads((ROOT/'SourceAssets/Narrative/MainStory/campaign.json').read_text(encoding='utf-8'))
    chapter=next(c for c in data['chapters'] if c['id']==patch['id'])
    assert set(patch['nodes'])=={n['id'] for n in chapter['nodes']}
    chapter['title']=patch['title'];chapter['summary']=patch['summary']
    for node in chapter['nodes']:
        change=patch['nodes'][node['id']]
        lines=[line.split('|',1) for line in change['lines'].strip().splitlines()]
        assert len(lines)>=len(node['lines']) and all(s in data['characters'] and t.strip() for s,t in lines)
        node['summary']=change['summary'];node['lines']=lines
        for field in ('scene','key_moment','camera'):
            node['art'][field]=change[field]
        node['art']['mood']=change.get('mood',node['art']['mood'])
        node['art']['cast']=list(dict.fromkeys(node['art']['cast']+[s for s,_ in lines if s!='narrator']))
        node['options']=change.get('options',node['options'])
        node['hints']=change.get('hints',node['hints'])
    path=ROOT/'scripts'/('main_story_'+patch['id'].lower()+'.py')
    path.write_text(render(chapter),encoding='utf-8')
    print(json.dumps({'chapter':patch['id'],'nodes':len(chapter['nodes']),'file':str(path)},ensure_ascii=False))

if __name__=='__main__':main()
