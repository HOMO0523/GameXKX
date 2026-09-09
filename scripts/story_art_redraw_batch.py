"""Persist built-in imagegen outputs and prepare bounded chapter review assets."""
import argparse
import hashlib
import html
import json
import math
import os
import shutil
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
from build_main_story_content import image_request

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'SourceArt/UI/StoryNodes/Redraw'
GENERATED=Path('C:/Users/shxuw/.codex/generated_images/01a08172-043d-75e0-a149-f8895de07b04')
CAMPAIGN=ROOT/'SourceAssets/Narrative/MainStory/campaign.json'
MANIFEST=ROOT/'SourceArt/UI/StoryNodes/manifest.json'
LOCKED={'S02-02'}

def read(path):return json.loads(path.read_text(encoding='utf-8'))
def write(path,data):
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def requests():
    jobs=[]
    for chapter in read(CAMPAIGN)['chapters'][1:]:
        folder=OUT/chapter['id'];folder.mkdir(parents=True,exist_ok=True)
        old=read(MANIFEST)['nodes']
        if not (folder/'before-manifest.json').exists():write(folder/'before-manifest.json',{n['id']:old[n['id']] for n in chapter['nodes']})
        batch=[]
        for node in chapter['nodes']:
            if node['id'] in LOCKED:continue
            request=image_request(node)
            assert len(request['referenced_image_paths'])<=5,(node['id'],request['referenced_image_paths'])
            assert all(Path(p).exists() for p in request['referenced_image_paths'])
            batch.append(request)
        write(folder/'requests.json',batch);jobs.extend(batch)
    write(OUT/'remaining-requests.json',jobs)
    print(json.dumps({'count':len(jobs),'chapters':5}))

def ingest(node_id,filename,revision):
    assert node_id.startswith(('S01-','S02-','S03-','S04-','S05-')) and node_id not in LOCKED
    assert Path(filename).name==filename and filename.startswith('exec-') and filename.endswith('.png')
    folder=OUT/node_id[:3];raw=folder/'Raw';raw.mkdir(exist_ok=True)
    stem=node_id.lower().replace('-','_')+f'_v{revision}'
    source=GENERATED/filename;target=raw/(stem+'.png')
    if target.exists():assert sha(target)==sha(source),'A different version must not overwrite a retained master'
    else:shutil.copy2(source,target)
    with Image.open(target) as im:
        im.load();assert abs(im.width/im.height-3)<.02,(node_id,im.size)
        source_size=list(im.size)
    request=next(r for r in read(folder/'requests.json') if r['node_id']==node_id)
    (raw/(stem+'_prompt.txt')).write_text(request['prompt'],encoding='utf-8')
    ledger_path=folder/'generated.json';ledger=read(ledger_path) if ledger_path.exists() else {}
    previous=ledger.get(node_id,{})
    if previous.get('user_approved'):
        assert previous['sha256']==sha(target),'User-approved scene is locked'
    record={'raw':target.relative_to(ROOT).as_posix(),'generated_source':str(source),'sha256':sha(target),'source_size':source_size,'revision':revision,'contract_sha256':request['contract_sha256'],'prompt':(raw/(stem+'_prompt.txt')).relative_to(ROOT).as_posix(),'visual_review':'pending'}
    if previous.get('user_approved'):record['user_approved']=True
    if node_id in ledger and ledger[node_id]['sha256']!=record['sha256']:
        record['history']=ledger[node_id].get('history',[])+[{k:v for k,v in ledger[node_id].items() if k!='history'}]
    ledger[node_id]=record;write(ledger_path,ledger)
    print(json.dumps({'id':node_id,'raw':str(target),'size':source_size}))

def prepare(chapter_id):
    campaign=read(CAMPAIGN);chapter=next(c for c in campaign['chapters'] if c['id']==chapter_id)
    folder=OUT/chapter_id;ledger=read(folder/'generated.json');manifest=read(MANIFEST)
    rows=[]
    for node in chapter['nodes']:
        key=node['id'];entry=manifest['nodes'][key];stem=key.lower().replace('-','_')
        if key in LOCKED:
            assert entry.get('reviewer')=='user' and entry.get('visual_review')=='pass' and sha(ROOT/entry['file'])==entry['sha256']
            entry['user_review']='approved'
            png=ROOT/entry['file'];webp=png.with_suffix('.webp');digest=entry['sha256'];raw=entry['generated_source']
        else:
            record=ledger[key];source=ROOT/record['raw'];assert sha(source)==record['sha256']
            contract=hashlib.sha256(json.dumps({'art':node['art'],'result':node['result']},ensure_ascii=False,sort_keys=True).encode('utf-8')).hexdigest()
            assert contract==record['contract_sha256'],key+' narrative changed during generation'
            png=folder/(stem+'_1536.png');webp=png.with_suffix('.webp')
            with Image.open(source) as im:
                im=im.convert('RGB').resize((1536,512),Image.Resampling.LANCZOS);im.save(png,optimize=True);im.save(webp,quality=88,method=6)
            assert webp.stat().st_size<350000,(key,'Review image is unexpectedly heavy')
            digest=sha(png);raw=record['raw']
            entry.update(file=png.relative_to(ROOT).as_posix(),generated_source=raw,sha256=digest,reviewed_sha256=digest,size=[1536,512],mode='RGB',revision=entry.get('revision',1)+(entry.get('sha256')!=digest),contract_sha256=contract,visual_review='pass',reviewer='agent',user_review='pending',review_batch=chapter_id,visual_style='project_graphic_painterly_v3',review_notes='后续章节整章审阅稿；按节点高光、角色身份、自然姿势与简洁纵深制作。')
            record['visual_review']='pass'
            if record.get('user_approved'):
                entry.update(reviewer='user',user_review='approved',review_notes='用户明确通过并锁定当前版本。')
        for path in (png,webp):
            with Image.open(path) as check:check.load();assert check.size==(1536,512)
        rows.append({'id':key,'title':node['title'],'highlight':node['art']['key_moment'],'result':node['result'],'image':png.relative_to(ROOT).as_posix(),'webp':webp.relative_to(ROOT).as_posix(),'webp_bytes':webp.stat().st_size,'sha256':digest,'raw':raw,'user_review':entry['user_review']})
    write(MANIFEST,manifest);write(folder/'generated.json',ledger)
    portraits=read(ROOT/'SourceArt/UI/StoryPortraits/manifest.json')['characters']
    actors=list(dict.fromkeys(s for n in chapter['nodes'] for s,_ in n['lines'] if s in portraits))
    all_approved=all(row['user_review']=='approved' for row in rows)
    write(folder/'review.json',{'chapter':chapter_id,'status':'user_approved' if all_approved else 'chapter_review_pending','images':rows,'portraits':actors})
    rel=lambda p:os.path.relpath(ROOT/p,folder).replace('\\','/')
    cards=[]
    old=read(folder/'before-manifest.json')
    for row in rows:
        cards.append('<article id="'+row['id']+'"><h2>'+row['id']+' · '+html.escape(row['title'])+'</h2><a target="_blank" href="'+rel(row['image'])+'"><img class="scene" src="'+rel(row['webp'])+'"></a><p>'+html.escape(row['highlight'])+'</p><small>'+str(round(row['webp_bytes']/1000))+' KB</small><details><summary>原图对照</summary><img class="scene" src="'+rel(old[row['id']]['file'])+'"></details></article>')
    role_cards=['<figure><img src="'+rel(portraits[a]['file'])+'"><figcaption>'+campaign['characters'][a]['name']+'</figcaption></figure>' for a in actors]
    css='*{box-sizing:border-box}body{margin:0;background:#e9ddc6;color:#392f23;font-family:KaiTi,serif}main{max-width:1260px;margin:auto;padding:36px 24px}h1{font-size:34px}h2{font-size:26px;color:#3c6557}article{margin:32px 0;padding:20px;background:#f1e5cf;border:1px solid #cbbda4;border-radius:5px}article p{font-size:22px;line-height:1.6}.scene{display:block;width:100%;mask-image:linear-gradient(to right,transparent,#000 8.333%,#000 91.667%,transparent),linear-gradient(to bottom,transparent,#000 10%,#000 90%,transparent);mask-composite:intersect}small,summary{color:#796e5d}summary{cursor:pointer}.roles{display:flex;flex-wrap:wrap;justify-content:center}figure{width:210px;margin:8px}figure img{width:100%}figcaption{text-align:center;font-size:23px}a{color:#3c6557}'
    page='<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>'+chapter['title']+'</title><style>'+css+'</style><main><a href="../review.html">全部章节</a><h1>'+chapter_id+' · '+chapter['title']+'</h1><p>按章审阅 · '+str(len(rows))+' 张剧情图。点击图查看大图。</p><label><input id="feather" type="checkbox" checked>篝火式四周柔化预览</label><div class="roles">'+''.join(role_cards)+'</div>'+''.join(cards)+'</main><script>document.getElementById("feather").onchange=e=>document.querySelectorAll(".scene").forEach(x=>x.style.maskImage=e.target.checked?"":"none")</script></html>'
    if all_approved:page=page.replace('<p>按章审阅 · ','<p>本章已通过 · ')
    (folder/'review.html').write_text(page,encoding='utf-8')
    height=70+math.ceil(len(rows)/2)*292+(380 if actors else 0)
    sheet=Image.new('RGB',(1600,height),'#efe3cc');draw=ImageDraw.Draw(sheet)
    font=ImageFont.truetype('C:/Windows/Fonts/simkai.ttf',26);small=ImageFont.truetype('C:/Windows/Fonts/simkai.ttf',22)
    draw.text((25,15),chapter_id+' · '+chapter['title'],font=font,fill='#3b3529')
    for i,row in enumerate(rows):
        x=20+(i%2)*790;y=64+(i//2)*292
        with Image.open(ROOT/row['webp']) as im:sheet.paste(im.resize((770,257),Image.Resampling.LANCZOS),(x,y+28))
        draw.text((x,y),row['id']+'  '+row['title'],font=small,fill='#365c4c')
    for i,actor in enumerate(actors):
        x=25+i*min(390,1550//max(1,len(actors)));y=70+math.ceil(len(rows)/2)*292
        with Image.open(ROOT/portraits[actor]['file']).convert('RGBA') as im:
            im=im.resize((320,320),Image.Resampling.LANCZOS);sheet.paste(im,(x,y),im)
        draw.text((x+80,y+325),campaign['characters'][actor]['name'],font=small,fill='#365c4c')
    sheet.save(folder/'contact-sheet.png',optimize=True)
    print(json.dumps({'chapter':chapter_id,'images':len(rows),'webp_bytes':sum(r['webp_bytes'] for r in rows),'gallery':str(folder/'review.html')},ensure_ascii=False))

if __name__=='__main__':
    parser=argparse.ArgumentParser();sub=parser.add_subparsers(dest='mode',required=True)
    sub.add_parser('requests');p=sub.add_parser('ingest');p.add_argument('node');p.add_argument('filename');p.add_argument('--revision',type=int,default=1)
    p=sub.add_parser('prepare');p.add_argument('chapter')
    args=parser.parse_args()
    if args.mode=='requests':requests()
    elif args.mode=='ingest':ingest(args.node,args.filename,args.revision)
    else:prepare(args.chapter)
