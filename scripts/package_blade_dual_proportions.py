"""Package approved chibi blade and new tall battle design for review."""
import json
import shutil
import hashlib
import zipfile
from pathlib import Path
from PIL import Image, ImageOps
from package_blade_red_compact import sheet
from package_project_design_characters import bounds

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'SourceArt/Characters/female-blade-dual-proportions-20260913'
SOURCE=ROOT/'SourceArt/Characters/female-blade-options-20260912/refinements/B-no-scabbard-20260913'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    for folder in ['raw','characters','review']:(OUT/folder).mkdir(exist_ok=True)
    prompt=json.loads((OUT/'battle-prompt.json').read_text(encoding='utf-8'))
    shutil.copy2(SOURCE/'raw.png',OUT/'raw/idle_chibi.png')
    shutil.copy2(SOURCE/'B_no_scabbard_1600.png',OUT/'characters/idle_chibi_1600.png')
    shutil.copy2(prompt['generatedPath'],OUT/'raw/battle_tall.png')
    with Image.open(OUT/'raw/battle_tall.png') as im:
        fit=ImageOps.contain(im.convert('RGB'),(1360,1360),Image.Resampling.LANCZOS)
        full=Image.new('RGB',(1600,1600),(244,239,223))
        full.paste(fit,((1600-fit.width)//2,(1600-fit.height)//2))
        full.save(OUT/'characters/battle_tall_1600.png')
    records=[]
    for slug,label in [('idle_chibi','挂机条 · Q版'),('battle_tall','局内战斗 · 修长版')]:
        p=OUT/'characters'/(slug+'_1600.png')
        with Image.open(p) as im:
            assert im.size==(1600,1600)
            box,_=bounds(im)
        assert min(box[0],box[1],1600-box[2],1600-box[3])>=120
        records.append(dict(slug=slug,label=label,path=str(p.relative_to(OUT)),sha256=sha(p),bounds=box))
    assert sha(OUT/'characters/idle_chibi_1600.png')==sha(SOURCE/'B_no_scabbard_1600.png')
    sheet([(r['label'],OUT/r['path']) for r in records],OUT/'review/dual-proportions.png','女刀客 B · 两套头身比例','同一角色 / 同一宝石画风 · 等高展示用于比较比例，非局内尺寸',2,700)
    manifest=dict(records=records,chibiPreservedExactly=True,imported=False,phase='tall-variant-review',canvasSize=[1600,1600])
    (OUT/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
    (OUT/'README.txt').write_text('女刀客 B · 两套头身比例\n\n挂机条：沿用已确认的去刀鞘Q版，单图保持字节一致。\n局内战斗：新增修长比例，保留红白服装、脸部身份、发型、云纹、单刀与宝石块面画风。\n两版1600×1600为审阅画布，原生生成图见raw。对照图按相同展示高度排版，非实际游戏显示尺寸。\n当前尚未导入游戏；此包仅为女刀客B的两版，不表示全角色已完成双比例。\n',encoding='utf-8-sig')
    archive=OUT/'GameXXK_女刀客B_战斗与挂机双比例.zip'
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
        for folder in ['raw','characters','review']:
            for p in sorted((OUT/folder).glob('*')):z.write(p,p.relative_to(OUT))
        for name in ['manifest.json','README.txt','battle-prompt.json']:z.write(OUT/name,name)
    with zipfile.ZipFile(archive) as z:assert z.testzip() is None
    print(json.dumps(dict(count=2,size=[1600,1600],chibiExact=True,safeBounds=True,zipIntegrity=True,zipBytes=archive.stat().st_size)))
if __name__=='__main__':main()
