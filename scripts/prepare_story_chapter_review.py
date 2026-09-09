"""Prepare the first chapter's bounded-size images and a chapter-level review gallery."""
import hashlib
import html
import json
import os
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'SourceArt/UI/StoryNodes/Redraw/S00'
VERSIONS={'S00-01':5,'S00-02':4,'S00-03':5,'S00-04':3,'S00-05':3,'S00-06':3,'S00-07':3}

def relative(path):
    return os.path.relpath(path,OUT).replace('\\','/')

def main():
    campaign=json.loads((ROOT/'SourceAssets/Narrative/MainStory/campaign.json').read_text(encoding='utf-8'))
    chapter=campaign['chapters'][0]
    manifest_path=ROOT/'SourceArt/UI/StoryNodes/manifest.json'
    manifest=json.loads(manifest_path.read_text(encoding='utf-8'))
    backup=OUT/'before-manifest.json'
    if not backup.exists():
        backup.write_text(json.dumps({n['id']:manifest['nodes'][n['id']] for n in chapter['nodes']},ensure_ascii=False,indent=2),encoding='utf-8')
    before=json.loads(backup.read_text(encoding='utf-8'))
    rows=[]
    for node in chapter['nodes']:
        node_id=node['id'];stem=node_id.lower().replace('-','_');revision=VERSIONS[node_id]
        raw=OUT/'Raw'/f'{stem}_v{revision}.png'
        png=OUT/(stem+'_1536.png');webp=OUT/(stem+'_1536.webp')
        with Image.open(raw) as original:
            original.load()
            assert original.width*512==original.height*1536,(node_id,original.size)
            image=original.convert('RGB').resize((1536,512),Image.Resampling.LANCZOS)
            image.save(png,optimize=True);image.save(webp,quality=90,method=6)
        for path in (png,webp):
            with Image.open(path) as check:check.load();assert check.size==(1536,512)
        digest=hashlib.sha256(png.read_bytes()).hexdigest()
        contract=hashlib.sha256(json.dumps({'art':node['art'],'result':node['result']},ensure_ascii=False,sort_keys=True).encode('utf-8')).hexdigest()
        entry=manifest['nodes'][node_id]
        approved=entry.get('sha256')==digest and entry.get('reviewer')=='user' and entry.get('user_review')=='approved'
        revision_no=entry.get('revision',1)+(entry.get('sha256')!=digest)
        entry.update(file=png.relative_to(ROOT).as_posix(),generated_source=raw.relative_to(ROOT).as_posix(),
                     sha256=digest,reviewed_sha256=digest,size=[1536,512],mode='RGB',revision=revision_no,
                     contract_sha256=contract,visual_review='pass',reviewer='user' if approved else 'agent',user_review='approved' if approved else 'pending',
                     review_batch='S00',visual_style='project_graphic_painterly_v3',
                     review_notes='首章整章预览稿；已处理角色差异、留白/纵深、姿势与持物、鞋印和桥梁结构等反馈，等待用户整章验收。')
        rows.append({'id':node_id,'title':node['title'],'highlight':node['art']['key_moment'],'result':node['result'],
                     'raw':raw.relative_to(ROOT).as_posix(),'image':png.relative_to(ROOT).as_posix(),
                     'webp':webp.relative_to(ROOT).as_posix(),'webp_bytes':webp.stat().st_size,'sha256':digest,
                     'prompt':str(raw.with_name(raw.stem+'_prompt.txt').relative_to(ROOT)).replace('\\','/'),
                     'user_review':'approved' if approved else 'pending'})
    manifest_path.write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    portraits=json.loads((ROOT/'SourceArt/UI/StoryPortraits/manifest.json').read_text(encoding='utf-8'))['characters']
    actors=['driver','mountain_man','abbot','innkeeper']
    cards=[]
    for row in rows:
        old=relative(ROOT/before[row['id']]['file'])
        cards.append(f'''<article id="{row['id']}"><div class="caption"><span>{row['id']}</span><h2>{html.escape(row['title'])}</h2></div>
<a href="{relative(ROOT/row['image'])}" target="_blank"><img class="scene" src="{relative(ROOT/row['webp'])}" alt="{html.escape(row['title'])}"></a>
<p>{html.escape(row['highlight'])}</p><small>{round(row['webp_bytes']/1000)} KB · 点击查看大图</small>
<details><summary>查看原图对照</summary><img class="scene" src="{old}" alt="原运行图"></details></article>''')
    role_cards=[];role_data={}
    for actor in actors:
        name=campaign['characters'][actor]['name'];portrait=portraits[actor]
        line=next(text for n in chapter['nodes'] for speaker,text in n['lines'] if speaker==actor)
        url=relative(ROOT/portrait['file'])
        role_data[actor]={'name':name,'image':url,'line':line}
        role_cards.append(f'<button class="role" data-role="{actor}"><img src="{url}" alt="{name}"><strong>{name}</strong></button>')
    page='''<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>天台山 · 剧情图与对白半身像审阅</title><style>
*{box-sizing:border-box}body{margin:0;background:#e9ddc6;color:#392f23;font-family:KaiTi,"楷体",serif}main{max-width:1200px;margin:auto;padding:44px 28px}header{margin-bottom:36px;border-bottom:2px solid #596f62;padding-bottom:22px}h1{font-size:36px;margin:0 0 12px}header p{font-size:19px;color:#655c4e}article{margin:30px 0 44px;padding:20px 22px;background:#f0e5cf;border:1px solid #cabfa9;border-radius:5px}.caption{display:flex;align-items:baseline;gap:14px}.caption span{color:#60776a;font:15px system-ui}h2{font-size:27px;margin:0 0 12px}.scene{display:block;width:100%;height:auto;mask-image:linear-gradient(to right,transparent,#000 8.333%,#000 91.667%,transparent),linear-gradient(to bottom,transparent,#000 10%,#000 90%,transparent);mask-composite:intersect}article p{font-size:21px;line-height:1.6;margin:8px 0}small,summary{color:#796e5d;font-size:15px}details{margin-top:12px}summary{cursor:pointer}.roles{display:grid;grid-template-columns:repeat(4,1fr);gap:14px}.role{border:1px solid #b8aa90;border-radius:5px;background:#f1e5cb;padding:12px;cursor:pointer;color:#443625;font:21px KaiTi}.role img{display:block;width:100%;height:auto}.role.active{outline:3px solid #577361}.dialogue{margin:25px auto 42px;background:#f0e3c9;padding:18px 24px;display:flex;align-items:center;gap:28px;max-width:924px;min-height:220px;border:1px solid #b6a688;box-shadow:0 3px 8px #55482f20}.dialogue img{width:184px;height:184px;object-fit:contain}.dialogue h3{font-size:27px;color:#456b5b;margin:0 0 14px}.dialogue p{font-size:24px;line-height:1.6;margin:0}.note{font-size:16px;color:#796e5d}@media(max-width:700px){main{padding:24px 12px}.roles{grid-template-columns:repeat(2,1fr)}.dialogue{gap:12px;padding:12px}.dialogue img{width:130px;height:150px}.dialogue p{font-size:20px}}
</style><main><header><h1>第一章 · 天台山</h1><p>七个剧情高光，四位配角半身像。本章集中审阅，其他章节尚未重绘完成。</p><label><input id="feather" type="checkbox" checked> 查看游戏使用的四周柔化效果</label></header>
<h2>对白角色</h2><p class="note">点击角色切换半身像与本章台词。此处是排版预览，实机检查另有截图。</p><div class="roles">'''+''.join(role_cards)+'''</div><div class="dialogue"><img id="portrait"><div><h3 id="speaker"></h3><p id="line"></p></div></div>
'''+''.join(cards)+'''<p class="note">每张保留高分辨率母版；审阅WebP与游戏PNG均为1536×512。游戏使用BC7和既有篝火式边缘渐隐。</p></main><script>
const roles='''+json.dumps(role_data,ensure_ascii=False)+''';function select(id){const r=roles[id];document.getElementById('portrait').src=r.image;document.getElementById('speaker').textContent=r.name;document.getElementById('line').textContent=r.line;document.querySelectorAll('.role').forEach(x=>x.classList.toggle('active',x.dataset.role===id))}document.querySelectorAll('.role').forEach(x=>x.onclick=()=>select(x.dataset.role));document.getElementById('feather').onchange=e=>document.querySelectorAll('.scene').forEach(x=>x.style.maskImage=e.target.checked?'':'none');select('driver');</script></html>'''
    all_approved=all(row['user_review']=='approved' for row in rows)
    if all_approved:page=page.replace('本章集中审阅，其他章节尚未重绘完成。','本章已通过，其他章节继续按章制作。')
    (OUT/'review.html').write_text(page,encoding='utf-8')
    report={'chapter':'S00','status':'user_approved' if all_approved else 'chapter_review_pending','images':rows,'portraits':actors,'remaining_other_scene_redraws':53}
    (OUT/'review.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    # A comparison/contact sheet is a review artifact; it does not replace the artwork.
    sheet=Image.new('RGB',(1600,1640),'#efe3cc');draw=ImageDraw.Draw(sheet)
    font=ImageFont.truetype('C:/Windows/Fonts/simkai.ttf',27)
    small_font=ImageFont.truetype('C:/Windows/Fonts/simkai.ttf',22)
    draw.text((30,14),'天台山 · 七张剧情图与四位对白角色',font=font,fill='#3b3529')
    for i,row in enumerate(rows):
        x=20+(i%2)*790;y=64+(i//2)*290
        with Image.open(ROOT/row['webp']) as im:sheet.paste(im.resize((770,257),Image.Resampling.LANCZOS),(x,y+28))
        draw.text((x,y),row['id']+'  '+row['title'],font=small_font,fill='#365c4c')
    for i,actor in enumerate(actors):
        x=28+i*395;y=1240
        with Image.open(ROOT/portraits[actor]['file']).convert('RGBA') as im:
            im=im.resize((330,330),Image.Resampling.LANCZOS);sheet.paste(im,(x,y),im)
        draw.text((x+110,y+330),campaign['characters'][actor]['name'],font=small_font,fill='#365c4c')
    sheet.save(OUT/'contact-sheet.png',optimize=True)
    print(json.dumps({'images':len(rows),'webp_bytes':sum(x['webp_bytes'] for x in rows),'gallery':str(OUT/'review.html'),'contact_sheet':str(OUT/'contact-sheet.png')},ensure_ascii=False))

if __name__=='__main__':main()
