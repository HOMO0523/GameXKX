"""Review outputs for the isolated blade graphic-style and hair revision."""
import hashlib
import json
from pathlib import Path
from PIL import Image,ImageOps,ImageDraw,ImageFont
from package_project_design_characters import bounds

ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'SourceArt/Characters/female-guofeng-concepts-20260912'
OUT=BASE/'refinements/blade-gem-graphic-v3'
BG=(244,239,223)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
raw=Image.open(OUT/'raw.png').convert('RGB')
fit=ImageOps.contain(raw,(1360,1360),Image.Resampling.LANCZOS)
full=Image.new('RGB',(1600,1600),BG)
full.paste(fit,((1600-fit.width)//2,(1600-fit.height)//2))
box,area=bounds(full)
assert min(box[0],box[1],1600-box[2],1600-box[3])>=120
full.save(OUT/'female_blade_v3.png')
w,h=raw.size
crop=(round(w*.34),round(h*.008),round(w*.71),round(h*.285))
head=raw.crop(crop)
head.resize((740,round(740*head.height/head.width)),Image.Resampling.LANCZOS).save(OUT/'head-detail.png')
sheet=Image.new('RGB',(1400,850),BG);d=ImageDraw.Draw(sheet)
font=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',29)
d.text((25,17),'女刀客 · 宝石画风与轮廓调整',font=font,fill=(35,46,41))
for i,(path,title) in enumerate([(OUT/'before.png','上一版'),(OUT/'raw.png','厚描边 · 大色面 · 短束发')]):
    im=Image.open(path).convert('RGB').resize((680,680),Image.Resampling.LANCZOS)
    sheet.paste(im,(10+i*700,80))
    d.text((350+i*700,805),title,font=font,fill=(35,46,41),anchor='mm')
sheet.save(OUT/'comparison.png')
prompt=json.loads((OUT/'prompt.json').read_text(encoding='utf-8'))
prompt.update(generatedPath=str(OUT/'raw.png'),nativeSize=list(raw.size),reviewSize=[1600,1600],fullbodyFile='female_blade_v3.png',headDetailFile='head-detail.png',comparisonFile='comparison.png',foregroundBounds=box,sha256=sha(OUT/'female_blade_v3.png'),referenceSnapshot='before.png',visualReview={'braidRemoved':True,'hair':'short upswept tied tuft, broad swept fringe','rendering':'thicker dark contour, broad red/black value groups, simplified wraps','identityPreserved':['adult full figure','red/black palette','mischievous expression','left-facing stance','held curved dao']})
(OUT/'prompt.json').write_text(json.dumps(prompt,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
manifest=json.loads((BASE/'manifest.json').read_text(encoding='utf-8'))
for r in manifest['records']:assert sha(BASE/r['file'])==r['sha256']
print(json.dumps({'scope':'female blade only','nativeSize':list(raw.size),'reviewSize':[1600,1600],'safeBounds':box,'otherDraftsUnchanged':True},ensure_ascii=True))
