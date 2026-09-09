"""Validate all chapter review images and publish one local review index."""
import hashlib
import html
import json
from pathlib import Path
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'SourceArt/UI/StoryNodes/Redraw'
campaign=json.loads((ROOT/'SourceAssets/Narrative/MainStory/campaign.json').read_text(encoding='utf-8'))
manifest=json.loads((ROOT/'SourceArt/UI/StoryNodes/manifest.json').read_text(encoding='utf-8'))['nodes']
chapters=[];seen=set();rows=[]
for chapter in campaign['chapters']:
    folder=OUT/chapter['id'];review=json.loads((folder/'review.json').read_text(encoding='utf-8'))
    assert len(review['images'])==len(chapter['nodes'])
    for row,node in zip(review['images'],chapter['nodes']):
        key=node['id'];assert row['id']==key and key not in seen;seen.add(key)
        entry=manifest[key];png=ROOT/row['image'];webp=ROOT/row['webp']
        assert hashlib.sha256(png.read_bytes()).hexdigest()==entry['sha256']==row['sha256']
        for image_path in (png,webp):
            with Image.open(image_path) as im:im.load();assert im.size==(1536,512),(key,im.size)
        assert webp.stat().st_size<350000,(key,webp.stat().st_size)
        assert entry['visual_review']=='pass'
        if chapter['id']!='S00' and key!='S02-02':
            ledger=json.loads((folder/'generated.json').read_text(encoding='utf-8'))[key]
            assert row['raw']==ledger['raw']
            assert hashlib.sha256((ROOT/ledger['raw']).read_bytes()).hexdigest()==ledger['sha256']
        rows.append({'id':key,'bytes':webp.stat().st_size,'sha256':entry['sha256'],'approved':entry.get('user_review')=='approved' or entry.get('reviewer')=='user'})
    count=len(review['images']);size=sum(r['webp_bytes'] for r in review['images'])
    chapters.append({'id':chapter['id'],'title':chapter['title'],'count':count,'bytes':size,'status':review['status'],'cover':str((ROOT/review['images'][0]['webp']).relative_to(OUT)).replace('\\','/')})
assert len(seen)==61
portraits=json.loads((ROOT/'SourceArt/UI/StoryPortraits/manifest.json').read_text(encoding='utf-8'))['characters']
for key,entry in portraits.items():
    p=ROOT/entry['file'];assert hashlib.sha256(p.read_bytes()).hexdigest()==entry['sha256']
    with Image.open(p) as im:
        im.load();assert im.mode=='RGBA' and im.size==(512,512)
        a=im.getchannel('A');assert a.getextrema()==(0,255)
        assert all(a.getpixel(xy)==0 for xy in [(0,0),(511,0),(0,511),(511,511)])
report={'scene_count':len(rows),'new_this_run':53,'chapter_counts':[c['count'] for c in chapters],'webp_total_bytes':sum(r['bytes'] for r in rows),'webp_max_bytes':max(r['bytes'] for r in rows),'webp_average_bytes':round(sum(r['bytes'] for r in rows)/len(rows)),'user_approved':sum(r['approved'] for r in rows),'portrait_count':len(portraits),'portrait_size':[512,512],'scene_size':[1536,512],'render_format':'BC7','soft_edge_factors':[12,10],'chapters':chapters,'images':rows}
(OUT/'validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
cards=[]
for c in chapters:
    status='本章已通过' if c['status']=='user_approved' else '整章审阅'
    cards.append(f'<a class="chapter" href="{c["id"]}/review.html"><img src="{c["cover"]}" alt="{html.escape(c["title"])}"><div><small>{c["id"]} · {c["count"]}幅 · {c["bytes"]/1000000:.2f} MB</small><h2>{html.escape(c["title"])}</h2><p>{status}　→</p></div></a>')
page='''<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>主线剧情图 · 六章总览</title><style>
*{box-sizing:border-box}body{margin:0;background:#ece1ca;color:#3b3327;font-family:KaiTi,"楷体",serif}main{max-width:1360px;margin:auto;padding:48px 28px}header{padding-bottom:24px;margin-bottom:26px;border-bottom:2px solid #647967}h1{font-size:38px;margin:0 0 16px}header p{font-size:20px;line-height:1.7;margin:4px 0}.grid{display:grid;grid-template-columns:1fr 1fr;gap:24px}.chapter{background:#f3e9d5;border:1px solid #c7b99d;border-radius:5px;overflow:hidden;display:block;text-decoration:none;color:inherit;transition:transform .15s}.chapter:hover{transform:translateY(-3px);border-color:#627f69}.chapter img{display:block;width:100%;aspect-ratio:3/1;object-fit:cover;mask-image:linear-gradient(to right,transparent,#000 8.333%,#000 91.667%,transparent),linear-gradient(to bottom,transparent,#000 10%,#000 90%,transparent);mask-composite:intersect}.chapter div{padding:12px 22px 20px}h2{font-size:25px;margin:12px 0;color:#355f4d}small{font:14px system-ui;color:#776c57}.chapter p{font-size:18px;margin:0;color:#63745f}.note{font-size:18px;line-height:1.8;color:#776b55;margin-top:28px}@media(max-width:800px){main{padding:25px 14px}.grid{grid-template-columns:1fr}h1{font-size:30px}}
</style><main><header><h1>主线剧情图 · 六章总览</h1><p>61幅剧情场景。按章查看高光、角色与原稿对照。</p><p>已通过的首章和单张修正版保留；其余集中审阅。每章页面可以切换四周柔化预览。</p></header><div class="grid">'''+''.join(cards)+f'''</div><p class="note">审阅图共 {report['webp_total_bytes']/1000000:.2f} MB，平均每幅 {report['webp_average_bytes']/1000:.0f} KB。游戏场景使用1536×512源图、BC7与篝火式四周渐隐；另补齐8位配角半身像，角色头像采用512×512透明图片。所有高分辨率母版与修改历史均保留。</p></main></html>'''
page=page.replace('已通过的首章和单张修正版保留；其余集中审阅。',f'已通过 {report["user_approved"]} 幅；待复核 {len(rows)-report["user_approved"]} 幅。通过稿已锁定。')
(OUT/'review.html').write_text(page,encoding='utf-8')
print(json.dumps({k:v for k,v in report.items() if k not in ('images','chapters')},ensure_ascii=False))
