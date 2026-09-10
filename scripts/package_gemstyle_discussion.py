"""Assemble the user-requested 3+3 cast and one battle concept for discussion."""
import hashlib,json,shutil,zipfile
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
import package_gemstyle_cast_review as cast

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'SourceArt/Characters/gemstyle-discussion-20260910'
SLUGS=['hero','hunter','you_bai','rooster','goat','moneyrat']
FEATURES=['棕褐灰蓝 · 竹篓旅人','原设披肩 · 低位持弓','蓝紫卷焰 · 古卷之灵','圆身红冠 · 短尾扇','低伏厚角 · 顶撞','朱红短褂 · 方孔铜钱']

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    jobs=json.loads((cast.ART/'art-jobs.json').read_text(encoding='utf-8'))['jobs']
    by={j['slug']:j for j in jobs};rows=[]
    for slug in SLUGS:
        j=by[slug];r=cast.package_one(j)
        target=OUT/j['group']/(slug+'.png');target.parent.mkdir(exist_ok=True)
        shutil.copy2(ROOT/r['reviewPng'],target)
        prompt=OUT/'prompts'/(slug+'.json');prompt.parent.mkdir(exist_ok=True)
        shutil.copy2(cast.ART/'prompts-v2'/(slug+'.json'),prompt)
        r['file']=str(target.relative_to(OUT)).replace('\\','/')
        r['facing']='left' if j['group']=='characters' else 'right'
        r['view']='three-quarter';r['reviewFlipHorizontal']=bool(j.get('reviewFlipHorizontal'))
        rows.append(r)
    font=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',29)
    small=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',20)
    title=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',36)
    sheet=Image.new('RGB',(1560,1230),cast.BG);d=ImageDraw.Draw(sheet)
    d.text((30,17),'国风新画风讨论 · 3个角色 × 3个怪物',font=title,fill=(30,45,41))
    d.text((30,68),'上排角色朝左，下排怪物朝右 · 3/4侧面 · 单体PNG均为1600×1600',font=small,fill=(70,85,73))
    for i,r in enumerate(rows):
        x=30+(i%3)*510;y=110+(i//3)*550
        im=Image.open(OUT/r['file']).convert('RGB').resize((480,480),Image.Resampling.LANCZOS)
        sheet.paste(im,(x,y))
        d.text((x+240,y+492),r['name'],font=font,anchor='mm',fill=(30,45,41))
        d.text((x+240,y+530),FEATURES[i],font=small,anchor='mm',fill=(70,85,73))
    sheet.save(OUT/'six-subjects.png')
    scene=OUT/'battle-scene.png'
    result={'countCharacters':3,'countMonsters':3,'tool':'built-in image_gen','phase':'discussion-awaiting-user',
            'userApproved':False,'imported':False,'records':rows,'sceneComplete':scene.exists()}
    if scene.exists():
        im=Image.open(scene);result['scene']={'file':'layout-preview.png' if (OUT/'layout-spec.json').exists() else 'battle-scene.png','size':list(im.size),'sha256':hashlib.sha256(scene.read_bytes()).hexdigest(),'kind':'deterministic offline composition from independent assets; not an in-engine screenshot'}
    if (OUT/'layout-spec.json').exists():
        result['layoutSpec']='layout-spec.json'
        result['independentBackground']='background-pure.png'
        result['independentCardBase']='card-base.png'
        result['fullCatalogGenerationPaused']=True
        result['cardBaseCanvasMatchesOriginal']=json.loads((OUT/'layout-spec.json').read_text(encoding='utf-8'))['cardBaseCheck']
        result['uiBaseChecks']=json.loads((OUT/'layout-spec.json').read_text(encoding='utf-8')).get('uiBaseChecks',[])
    (OUT/'manifest.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    if scene.exists():
        with zipfile.ZipFile(OUT/'GameXXK_国风新画风讨论包.zip','w',zipfile.ZIP_DEFLATED) as z:
            for directory in ['characters','monsters','preview-cutouts']:
                if not (OUT/directory).exists():continue
                for p in sorted((OUT/directory).glob('*')):z.write(p,p.relative_to(OUT))
            for file in ['layout-preview.png','layout-art-only.png','background-pure.png','card-base.png','qi-base.png','end-turn-base.png','ui-bases-preview.png','ui-overlay.png','unit-layer.png','six-subjects.png','layout-spec.json','manifest.json','README.md']:
                if (OUT/file).exists():z.write(OUT/file,file)
            for slug in SLUGS+['background-brighter-distance','card-base-simple','qi-base-v3-soul-cloud','end-turn-base-v2-gemstyle']:
                p=OUT/'prompts'/(slug+'.json')
                if p.exists():z.write(p,p.relative_to(OUT))
    print(json.dumps({'characters':3,'monsters':3,'scene':scene.exists(),'output':str(OUT)},ensure_ascii=False))

if __name__=='__main__':main()
