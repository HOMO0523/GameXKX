"""Prepare the generated six-frame fire/frost hits using the approved background/layout workflow."""
from pathlib import Path
import json, hashlib, shutil
from PIL import Image, ImageDraw
from prepare_gem_type_icon_review import cleanup_background

ROOT=Path(__file__).resolve().parents[1]
GENERATED=Path('C:/Users/shxuw/.codex/generated_images/01a081fe-c2ea-7c52-9b93-e50433252402')
OUT=ROOT/'SourceArt/UI/Battle/VFX/ElementalHits'
SPECS={
    'Fire':('exec-b301ae65-2441-40aa-a94e-a30beaeff1c5.png',[(256,474),(255,480),(261,480),(255,432),(256,432),(249,433)],'B02/039 ground flame burst'),
    'Frost':('exec-f4ce0875-06f1-4722-be4f-e07789f1a84f.png',[(256,505),(256,505),(256,508),(252,445),(254,448),(249,448)],'B02/042 descending ice and shards'),
}

def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    manifest={'frame_count':6,'cell':[256,280],'grid':[8,8],'duration_seconds':0.30,'ground_anchor':[128,250],
              'generated_with':'built-in imagegen','processing':'background alpha only; original RGB preserved before slicing/resizing; fixed global scale and explicit ground alignment','effects':[]}
    for kind,(name,anchors,reference) in SPECS.items():
        source=GENERATED/name;raw=OUT/(kind+'-generated-sheet.png')
        if raw.exists():assert digest(raw)==digest(source)
        else:shutil.copy2(source,raw)
        sheet,report=cleanup_background(raw);assert sheet.size==(1536,1024)
        sheet.save(OUT/(kind+'-clean-sheet.png'))
        frames=[];records=[]
        atlas=Image.new('RGBA',(2048,2240))
        for index,anchor in enumerate(anchors):
            col,row=index%3,index//3
            original=sheet.crop((col*512,row*512,(col+1)*512,(row+1)*512))
            # One scale for every stage: never normalize each frame to fill its box.
            scale=0.46;size=(round(512*scale),round(512*scale))
            scaled=original.resize(size,Image.Resampling.LANCZOS)
            position=(128-round(anchor[0]*scale),250-round(anchor[1]*scale))
            canvas=Image.new('RGBA',(256,280));canvas.alpha_composite(scaled,position)
            alpha=canvas.getchannel('A');bbox=alpha.point(lambda a:255 if a>=16 else 0).getbbox()
            assert bbox and bbox[0]>0 and bbox[2]<256 and bbox[1]>0 and bbox[3]<280,(kind,index,bbox)
            path=OUT/f'{kind}-frame-{index:02d}.png';canvas.save(path);frames.append(canvas)
            atlas.alpha_composite(canvas,(index*256,0))
            records.append({'frame':index,'source_cell':[col,row],'source_ground_anchor':anchor,'destination':position,'alpha_bbox':bbox,'sha256':digest(path)})
        atlas_path=OUT/f'T_{kind}Hit.png';atlas.save(atlas_path)
        assert atlas.crop((0,280,2048,2240)).getchannel('A').getextrema()==(0,0)
        assert atlas.crop((1536,0,2048,280)).getchannel('A').getextrema()==(0,0)
        preview=[]
        for frame in frames:
            paper=Image.new('RGBA',(384,420),(233,223,198,255));paper.alpha_composite(frame.resize((384,420)),(0,0));preview.append(paper.convert('RGB'))
        preview[0].save(OUT/f'{kind}-preview.gif',save_all=True,append_images=preview[1:],duration=[50]*5+[650],loop=0,disposal=2)
        manifest['effects'].append({'type':kind,'source':str(raw.relative_to(ROOT)),'reference':reference,'source_sha256':digest(raw),'cleanup':report,
             'texture':f'/Game/GameXXK/UI/Battle/VFX/ElementalHits/T_{kind}Hit.T_{kind}Hit','atlas':str(atlas_path.relative_to(ROOT)),'atlas_sha256':digest(atlas_path),'frames':records})
    (OUT/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'ok':True,'effects':2,'frames':12,'manifest':str(OUT/'manifest.json')}))

if __name__=='__main__':main()
