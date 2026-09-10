"""Build a friend-facing image bundle from the latest reviewed cast manifests."""
import csv
import hashlib
import json
import shutil
import zipfile
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont

ROOT=Path(__file__).resolve().parents[1]
CHARACTERS=ROOT/'SourceArt/Characters/project-design-gemstyle-all-20260910'
MONSTERS=ROOT/'SourceArt/Monsters/project-design-gemstyle-20260910'
OUTPUT=ROOT/'SourceArt/ReviewPackages/20260910_新版角色与怪物'
BUNDLE=OUTPUT/'GameXXK_新版角色与怪物_分享图包'
OVERVIEWS=BUNDLE/'00_总览与分组'
BG=(244,239,223)
INK=(35,46,41)
TIERS={'normal':'普通','elite':'精英','boss':'首领'}

def read(path):return json.loads(path.read_text(encoding='utf-8-sig'))
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def sheet(path,records,columns,cell,title,subtitle):
    rows=(len(records)+columns-1)//columns
    im=Image.new('RGB',(columns*cell+48,120+rows*(cell+62)),BG)
    draw=ImageDraw.Draw(im)
    heading=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',36)
    font=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',26)
    small=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',19)
    draw.text((26,17),title,font=heading,fill=INK)
    draw.text((28,73),subtitle,font=small,fill=(88,94,80))
    for i,r in enumerate(records):
        x=24+(i%columns)*cell;y=116+(i//columns)*(cell+62)
        with Image.open(BUNDLE/r['file']) as source:
            thumbnail=source.convert('RGB').resize((cell,cell),Image.Resampling.LANCZOS)
        im.paste(thumbnail,(x,y))
        draw.text((x+cell/2,y+cell+20),r['name'],font=font,fill=INK,anchor='mm')
        if r['category']=='怪物':
            draw.text((x+cell/2,y+cell+48),f'第{r["chapter"]}章 · {r["tierLabel"]}',font=small,fill=(111,94,66),anchor='mm')
    im.save(path,quality=94,subsampling=0,optimize=True)

def build():
    OVERVIEWS.mkdir(parents=True,exist_ok=True)
    records=[]
    for category,source_root,expected in [('角色',CHARACTERS,13),('怪物',MONSTERS,21)]:
        manifest=read(source_root/'manifest.json')
        assert manifest['complete'] and manifest['count']==expected
        for i,r in enumerate(manifest['records'],1):
            source=source_root/r['file']
            assert sha(source)==r['sha256'],r['slug']+' differs from latest manifest'
            with Image.open(source) as im:assert im.size==(1600,1600)
            tier=TIERS.get(r.get('tier'),'')
            if category=='角色':
                rel=Path('01_角色单图')/f'{i:02d}_{r["name"]}.png'
            else:
                rel=Path('02_怪物单图')/f'第{r["chapter"]}章'/f'{i:02d}_{r["name"]}_{tier}.png'
            destination=BUNDLE/rel;destination.parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2(source,destination)
            assert sha(destination)==r['sha256']
            records.append({'number':i,'name':r['name'],'slug':r['slug'],'id':r['id'],'category':category,'chapter':r.get('chapter',''),'tierLabel':tier,'file':rel.as_posix(),'size':[1600,1600],'sha256':r['sha256'],'source':str(source.relative_to(ROOT))})
    chars=[r for r in records if r['category']=='角色'];monsters=[r for r in records if r['category']=='怪物']
    assert len(records)==34 and len({r['sha256'] for r in records})==34
    subtitle='项目原设计 · 宝石 icon 画风 · 2026.09.10 分享版'
    sheet(OVERVIEWS/'01_全部角色_13名.jpg',chars,5,400,'新版角色 · 13名',subtitle)
    sheet(OVERVIEWS/'02_全部怪物_21只.jpg',monsters,7,330,'新版怪物 · 21只',subtitle)
    sheet(OVERVIEWS/'03_主角与六种伙伴.jpg',[r for r in chars if not r['id'].startswith('Npc.')],4,470,'主角与六种伙伴',subtitle)
    sheet(OVERVIEWS/'04_六名剧情角色.jpg',[r for r in chars if r['id'].startswith('Npc.')],3,560,'六名剧情角色',subtitle)
    for chapter in [1,2,3]:
        sheet(OVERVIEWS/f'0{chapter+4}_第{chapter}章怪物.jpg',[r for r in monsters if r['chapter']==chapter],4,470,f'第{chapter}章怪物 · 普通 / 精英 / 首领',subtitle)
    sheet(OVERVIEWS/'08_三位首领.jpg',[r for r in monsters if r['tierLabel']=='首领'],3,560,'三位首领 · 金钱鼠 / 黑熊 / 老虎',subtitle)
    text='''GameXXK · 新版角色与怪物分享图包
2026.09.10

共34张单图：13名角色（主角＋6种伙伴＋6名剧情角色），21只怪物（三章各7只）。

建议先看“00_总览与分组”，再打开单张PNG放大查看。
“01_角色单图”按角色编号；“02_怪物单图”按章节分组，并注明普通、精英、首领。

本版保留项目原设计，采用宝石icon的图形与用色；角色朝左，怪物朝右。
怪物已根据最新反馈收拢过碎的毛发切面，并调整偏写实的眼睛。
单图为1600×1600米白背景审阅PNG，总览统一排版不代表游戏内真实尺寸比例。
图片用于风格和造型讨论；反馈时直接引用文件名即可。
'''
    (BUNDLE/'00_先读我.txt').write_text(text,encoding='utf-8-sig')
    with (BUNDLE/'图片目录.csv').open('w',encoding='utf-8-sig',newline='') as f:
        writer=csv.writer(f);writer.writerow(['分类','编号','名称','章节','类型','尺寸','图片位置'])
        for r in records:writer.writerow([r['category'],r['number'],r['name'],r['chapter'],r['tierLabel'],'1600×1600',r['file']])
    all_files=sorted(p for p in BUNDLE.rglob('*') if p.is_file())
    full=OUTPUT/'GameXXK_新版角色与怪物_34张分享图包_20260910.zip'
    quick=OUTPUT/'GameXXK_角色怪物_快速预览_20260910.zip'
    with zipfile.ZipFile(full,'w',zipfile.ZIP_DEFLATED) as z:
        for p in all_files:z.write(p,Path(BUNDLE.name)/p.relative_to(BUNDLE))
    with zipfile.ZipFile(quick,'w',zipfile.ZIP_DEFLATED) as z:
        for p in sorted(OVERVIEWS.glob('*.jpg')):z.write(p,p.name)
        z.write(BUNDLE/'00_先读我.txt','00_先读我.txt')
    for zip_path in [full,quick]:
        with zipfile.ZipFile(zip_path) as z:
            assert z.testzip() is None
            assert len(z.namelist())==len(set(z.namelist()))
            if zip_path==full:
                assert sum(n.endswith('.png') for n in z.namelist())==34
                for r in records:
                    arc=(Path(BUNDLE.name)/r['file']).as_posix()
                    assert hashlib.sha256(z.read(arc)).hexdigest()==r['sha256']
    report={'characters':13,'monsters':21,'singleImages':34,'previewSheets':8,'allPngsByteIdenticalToLatestMasters':True,'sourceDirectories':[str(CHARACTERS),str(MONSTERS)],'zipIntegrity':'passed','fullZip':str(full),'fullBytes':full.stat().st_size,'quickZip':str(quick),'quickBytes':quick.stat().st_size,'records':records}
    (OUTPUT/'packaging-report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k not in ['records','sourceDirectories']},ensure_ascii=False))

if __name__=='__main__':build()
