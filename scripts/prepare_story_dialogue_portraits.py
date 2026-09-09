"""Cut the user-requested story busts to genuine alpha; keep generated masters unchanged."""
from pathlib import Path
import hashlib
import json
import shutil
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parents[1]
FOLDER = ROOT / 'SourceArt/UI/StoryPortraits'
GENERATED = Path('C:/Users/shxuw/.codex/generated_images/01a08172-043d-75e0-a149-f8895de07b04')
SOURCES = {
    'driver': ('exec-dfb0200a-49f2-4a3b-b922-661f76a72b4e.png', 'magenta'),
    'mountain_man': ('exec-9fb21753-a088-4d5d-bf3b-8e5063d56e5e.png', 'magenta'),
    'abbot': ('exec-49d05149-7415-4222-a5b0-904ac40ff847.png', 'magenta', 4),
    'innkeeper': ('exec-1773fcea-b360-406f-8649-60c62b399163.png', 'magenta'),
}
REVISION = 2

def cut(source, key):
    rgb = np.asarray(Image.open(source).convert('RGB')).astype(np.int16)
    if key == 'neutral':
        background = (rgb.max(2) - rgb.min(2) <= 10) & (rgb.min(2) >= 150)
    else:
        background = (rgb[:,:,0] - rgb[:,:,1] > 90) & (rgb[:,:,2] - rgb[:,:,1] > 90)
    # Only remove connected background. Enclosed white beard/eye highlights
    # remain opaque rather than being removed by a global grey threshold.
    connected = Image.fromarray(np.where(background, 0, 255).astype(np.uint8)).copy()
    for seed in ((0,0), (connected.width-1,0), (0,connected.height-1), (connected.width-1,connected.height-1)):
        if connected.getpixel(seed) == 0:
            ImageDraw.floodfill(connected, seed, 128)
    removed = background if key == 'magenta' else np.asarray(connected)==128
    alpha = Image.fromarray(np.where(removed, 0, 255).astype(np.uint8))
    # Remove the one source-pixel anti-alias fringe before downsampling.
    alpha = alpha.filter(ImageFilter.MinFilter(3))
    out = Image.fromarray(rgb.astype(np.uint8)).convert('RGBA')
    out.putalpha(alpha)
    bbox = alpha.getbbox()
    assert bbox
    subject = out.crop(bbox)
    scale = min(480/subject.width, 480/subject.height)
    size = (round(subject.width*scale), round(subject.height*scale))
    subject = subject.convert('RGBa').resize(size, Image.Resampling.LANCZOS).convert('RGBA')
    canvas = Image.new('RGBA', (512,512), (0,0,0,0))
    canvas.alpha_composite(subject, ((512-size[0])//2, 496-size[1]))
    return canvas, bbox

def main():
    remaining=FOLDER/'remaining-sources.json'
    if remaining.exists():
        SOURCES.update(json.loads(remaining.read_text(encoding='utf-8')))
    (FOLDER/'Raw').mkdir(parents=True, exist_ok=True)
    manifest_path=FOLDER/'manifest.json'
    manifest=json.loads(manifest_path.read_text(encoding='utf-8')) if manifest_path.exists() else {'schema':1,'characters':{}}
    for actor,source_entry in SOURCES.items():
        filename,key=source_entry[:2]
        revision=source_entry[2] if len(source_entry)>2 else REVISION
        source=GENERATED/filename
        master=FOLDER/'Raw'/(actor+f'_v{revision}.png')
        if not master.exists():shutil.copy2(source,master)
        assert hashlib.sha256(master.read_bytes()).digest()==hashlib.sha256(source.read_bytes()).digest()
        image,bbox=cut(master,key)
        output=FOLDER/(actor+f'_bust_v{revision}.png')
        image.save(output,optimize=True)
        alpha=np.asarray(image.getchannel('A'))
        assert image.mode=='RGBA' and image.size==(512,512)
        assert int(alpha.min())==0 and int(alpha.max())==255
        assert int(((alpha>0)&(alpha<255)).sum())>300, 'A cut silhouette needs an antialiased boundary, not a padded opaque rectangle'
        assert int((alpha==0).sum())>512*512*.2, 'Background must actually be removed'
        assert all(int(alpha[y,x])==0 for x,y in ((0,0),(511,0),(0,511),(511,511)))
        entry={'file':output.relative_to(ROOT).as_posix(),'reference_for_story':master.relative_to(ROOT).as_posix(),
               'source_generated':str(source),'sha256':hashlib.sha256(output.read_bytes()).hexdigest(),
               'size':[512,512],'source_crop':bbox,'alpha_bbox':image.getchannel('A').getbbox(),
               'transparent_pixels':int((alpha==0).sum()),'soft_edge_pixels':int(((alpha>0)&(alpha<255)).sum()),
               'bytes':output.stat().st_size,'key':key,'revision':revision,'review':'chapter_review_pending','imported':False,
               'texture':'/Game/GameXXK/UI/StoryPortraits/T_StoryPortrait_'+actor+'.T_StoryPortrait_'+actor}
        previous=manifest['characters'].get(actor)
        if previous and previous.get('sha256')!=entry['sha256']:
            manifest.setdefault('history',{}).setdefault(actor,[]).append(previous)
        elif previous:
            entry['review']=previous.get('review',entry['review'])
            entry['imported']=previous.get('imported',False)
        manifest['characters'][actor]=entry
    manifest_path.write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(manifest,ensure_ascii=False,indent=2))

if __name__=='__main__':main()
