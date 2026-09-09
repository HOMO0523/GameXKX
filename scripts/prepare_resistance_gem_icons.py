"""Prepare three generated resistance gems with the authorized background/layout pipeline."""
from pathlib import Path
import json, hashlib, shutil, zipfile
from PIL import Image, ImageDraw, ImageFont
from prepare_gem_type_icon_review import cleanup_background, normalize

ROOT=Path(__file__).resolve().parents[1]
FOLDER=ROOT/'SourceArt/UI/Items/Gems/resistance-review-20260909'
FONT=ROOT/'SourceArt/UI/Fonts/Readability/SourceHanSansCN-Bold.otf'

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def font(size):return ImageFont.truetype(str(FONT),size)

def main():
    generation=json.loads((FOLDER/'generation-records-v1.json').read_text(encoding='utf-8'))
    assert generation['background_cleanup_authorized'] and len(generation['records'])==3
    for directory in ['raw','clean','icons']: (FOLDER/directory).mkdir(parents=True,exist_ok=True)
    records=[]
    for source_record in generation['records']:
        record=dict(source_record);source=Path(record['generated_path']);name='T_Item_Gem_'+record['type']+'.png'
        raw=FOLDER/'raw'/name
        if raw.exists():assert sha(raw)==sha(source)
        else:shutil.copy2(source,raw)
        image,report=cleanup_background(raw);image.save(FOLDER/'clean'/name)
        icon=normalize(image);icon_path=FOLDER/'icons'/name;icon.save(icon_path)
        assert icon.mode=='RGBA' and icon.size==(512,512)
        alpha=icon.getchannel('A')
        for box in [(0,0,512,1),(0,511,512,512),(0,0,1,512),(511,0,512,512)]:assert alpha.crop(box).getextrema()==(0,0)
        records.append({**record,**report,'source_sha256':sha(raw),'normalized_png':str(icon_path.relative_to(ROOT)),
            'normalized_sha256':sha(icon_path),'size':[512,512],'mode':'RGBA','transparent_border':True,'percent_overlay':False})
    board=Image.new('RGB',(1488,704),(239,232,214));draw=ImageDraw.Draw(board)
    draw.text((48,22),'三系抗性宝石',font=font(31),fill=(48,44,36))
    draw.text((1007,36),'同系列 · 全品质共用',font=font(18),fill=(111,104,85))
    colors=[(172,57,26),(37,108,162),(158,107,22)]
    for i,record in enumerate(records):
        x=48+i*480;icon=Image.open(ROOT/record['normalized_png']).convert('RGBA')
        big=icon.resize((400,400),Image.Resampling.LANCZOS);board.paste(big,(x+16,88),big)
        draw.text((x+216,517),record['title'],anchor='mm',font=font(29),fill=colors[i])
        small=icon.resize((48,48),Image.Resampling.LANCZOS)
        board.paste(small,(x+127,567),small);draw.text((x+192,579),'48px',font=font(20),fill=(86,78,64))
        gray=small.convert('LA').convert('RGBA');board.paste(gray,(x+294,567),gray)
    draw.text((48,661),'火焰晶壳 / 雪晶护壁 / 雷纹晶甲',font=font(19),fill=(101,93,77))
    board_path=FOLDER/'resistance-gems-review-v1.png';board.save(board_path)
    manifest={'status':'review-only','count':3,'planned_gem_type_count':17,'quality_agnostic':True,'runtime_imported':False,
        'board':str(board_path.relative_to(ROOT)),'records':records}
    manifest_path=FOLDER/'manifest-v1.json';manifest_path.write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    bundle=FOLDER/'GameXXK_三抗性宝石图标_v1.zip'
    with zipfile.ZipFile(bundle,'w',zipfile.ZIP_DEFLATED) as archive:
        for record in records:archive.write(ROOT/record['normalized_png'],'icons/'+Path(record['normalized_png']).name)
        archive.write(board_path,board_path.name);archive.write(manifest_path,manifest_path.name)
    print(json.dumps({'ok':True,'icons':len(records),'size':[512,512],'alpha_borders':'clear','board':str(board_path),'zip':str(bundle)},ensure_ascii=False))

if __name__=='__main__':main()
