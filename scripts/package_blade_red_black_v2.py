"""Package one reference-led blade revision without replacing prior drafts."""
import hashlib
import json
from pathlib import Path
from PIL import Image,ImageOps
from package_project_design_characters import bounds

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'SourceArt/Characters/female-guofeng-concepts-20260912/refinements/blade-red-black-v2'
im=Image.open(OUT/'raw.png').convert('RGB')
native=list(im.size)
fit=ImageOps.contain(im,(1360,1360),Image.Resampling.LANCZOS)
canvas=Image.new('RGB',(1600,1600),(244,239,223))
canvas.paste(fit,((1600-fit.width)//2,(1600-fit.height)//2))
box,area=bounds(canvas)
assert min(box[0],box[1],1600-box[2],1600-box[3])>=120
canvas.save(OUT/'female_blade_v2.png')
# A magnified inspection crop of the generated face, not a separate drawing.
w,h=im.size
crop=(round(w*.38),round(h*.085),round(w*.60),round(h*.235))
face=im.crop(crop)
face.resize((792,round(792*face.height/face.width)),Image.Resampling.LANCZOS).save(OUT/'face-detail.png')
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
prompt=json.loads((OUT/'prompt.json').read_text(encoding='utf-8'))
prompt.update(generatedPath=str(OUT/'raw.png'),nativeSize=native,fullbodyFile='female_blade_v2.png',reviewSize=[1600,1600],faceDetailFile='face-detail.png',faceCropInRaw=crop,foregroundBounds=box,sha256=sha(OUT/'female_blade_v2.png'),referenceSnapshot='user-reference.png',visualReview='red/black palette, side braid, stylized narrow eyes, asymmetric grin, sparse costume accents; ready for user review')
(OUT/'prompt.json').write_text(json.dumps(prompt,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
old=ROOT/'SourceArt/Characters/female-guofeng-concepts-20260912/characters/female_blade.png'
manifest=json.loads((ROOT/'SourceArt/Characters/female-guofeng-concepts-20260912/manifest.json').read_text(encoding='utf-8'))
assert sha(old)==next(r['sha256'] for r in manifest['records'] if r['slug']=='female_blade')
print(json.dumps({'nativeSize':native,'reviewSize':[1600,1600],'safeBounds':box,'priorDraftUnchanged':True,'output':str(OUT)},ensure_ascii=True))
