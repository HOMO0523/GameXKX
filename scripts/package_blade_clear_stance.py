"""Package two blade stance repairs with reduced-size visual checks."""
import json, shutil, hashlib, zipfile
from pathlib import Path
from PIL import Image, ImageOps, ImageDraw, ImageFont
from package_blade_red_compact import sheet
from package_project_design_characters import bounds
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'SourceArt/Characters/female-blade-dual-proportions-20260913'
OUT=BASE/'refinements/clear-stance-v2'
TITLE='女刀客 B · 双腿轮廓修正'
SUBTITLE='双脚分开落地 · 衣摆让出后腿 · 两套比例'
ZIP_NAME='GameXXK_女刀客B_双比例站姿修正版.zip'
PREVIOUS=BASE
def main():
    for d in ['raw','characters','review']:(OUT/d).mkdir(exist_ok=True)
    records=[]
    for slug,label in [('idle_chibi','挂机条 · Q版'),('battle_tall','局内战斗 · 修长版')]:
        pf=OUT/'prompts'/(slug+'.json');p=json.loads(pf.read_text(encoding='utf-8'))
        raw=OUT/'raw'/(slug+'.png');shutil.copy2(p['generatedPath'],raw)
        with Image.open(raw) as im:
            fit=ImageOps.contain(im.convert('RGB'),(1360,1360),Image.Resampling.LANCZOS)
            full=Image.new('RGB',(1600,1600),(244,239,223));full.paste(fit,((1600-fit.width)//2,(1600-fit.height)//2))
        dest=OUT/'characters'/(slug+'_1600.png');full.save(dest)
        box,_=bounds(full);assert min(box[0],box[1],1600-box[2],1600-box[3])>=120
        records.append(dict(slug=slug,label=label,file=str(dest.relative_to(OUT)),size=[1600,1600],bounds=box,sha256=hashlib.sha256(dest.read_bytes()).hexdigest()))
    items=[(r['label'],OUT/r['file']) for r in records]
    sheet(items,OUT/'review/dual-proportions.png',TITLE,SUBTITLE,2,700)
    old=[('修正前 '+r['label'],PREVIOUS/'characters'/(r['slug']+'_1600.png')) for r in records]
    sheet(old+items,OUT/'review/before-after.png','站姿对照 · 上修正前 / 下修正后','检查后腿、膝盖方向、靴子间隙和衣摆遮挡',2,500)
    small=Image.new('RGB',(640,290),(244,239,223));d=ImageDraw.Draw(small)
    for i,(label,p) in enumerate(items):
        d.text((30+i*320,15),label,font=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',20),fill=(35,46,41))
        with Image.open(p) as im:
            box,_=bounds(im);crop=im.crop(box)
            for n,h in enumerate([100,180]):
                fit=ImageOps.contain(crop,(150,h),Image.Resampling.LANCZOS)
                small.paste(fit,(25+i*320+n*150,245-fit.height))
    small.save(OUT/'review/readability.png')
    (OUT/'manifest.json').write_text(json.dumps(dict(records=records,imported=False,phase='stance-repair-review'),ensure_ascii=False,indent=2),encoding='utf-8')
    archive=OUT/ZIP_NAME
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
        for folder in ['raw','characters','review','prompts']:
            for p in (OUT/folder).glob('*'):z.write(p,p.relative_to(OUT))
        z.write(OUT/'manifest.json','manifest.json')
    with zipfile.ZipFile(archive) as z:assert z.testzip() is None
    print(json.dumps(dict(count=2,dimensions=[1600,1600],safeBounds=True,zipIntegrity=True)))
if __name__=='__main__':main()
