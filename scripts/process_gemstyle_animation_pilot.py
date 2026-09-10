"""Extract fixed-canvas frame animations and visual QA from the Dreamina pilot."""
import argparse
import hashlib
import json
import math
import zipfile
from pathlib import Path

import imageio_ffmpeg
import numpy as np
from PIL import Image,ImageDraw,ImageFont,ImageFilter

from dreamina_gemstyle_animation_pilot import OUT,ROOT,read,write

def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def key_magenta(rgb):
    c=rgb.astype(np.int16)
    key=(c[:,:,0]>130)&(c[:,:,2]>125)&(np.minimum(c[:,:,0],c[:,:,2])-c[:,:,1]>68)
    alpha=Image.fromarray((~key*255).astype('uint8')).filter(ImageFilter.MinFilter(3))
    alpha=alpha.filter(ImageFilter.GaussianBlur(.35))
    rgba=Image.fromarray(rgb).convert('RGBA');rgba.putalpha(alpha)
    return rgba

def process(jid):
    job=next(j for j in read(OUT/'manifest.json')['jobs'] if j['id']==jid)
    state=read(OUT/'tasks'/(jid+'.json'))
    assert state['status']=='success',state['status']
    videos=[p for p in (OUT/'raw'/jid).rglob('*') if p.suffix.lower() in ['.mp4','.mov','.webm']]
    assert len(videos)==1,(jid,[str(p) for p in videos]);video=videos[0]
    reader=imageio_ffmpeg.read_frames(str(video),pix_fmt='rgb24');meta=next(reader);w,h=meta['size']
    rgb_frames=[]
    try:
        for raw in reader:rgb_frames.append(np.frombuffer(raw,dtype='uint8').reshape(h,w,3).copy())
    finally:reader.close()
    assert len(rgb_frames)>=24,'short video'
    indices=np.rint(np.linspace(0,len(rgb_frames)-1,48)).astype(int)
    folder=OUT/'frames'/jid;folder.mkdir(parents=True,exist_ok=True)
    frames=[];boxes=[];centers=[];areas=[];bottoms=[];edge=[];color_deltas=[]
    prior=None
    for n,index in enumerate(indices):
        frame=key_magenta(rgb_frames[index]).resize((512,512),Image.Resampling.LANCZOS)
        array=np.array(frame);a=array[:,:,3];mask=a>128;yy,xx=np.where(mask)
        assert len(xx)>1000,(jid,n,'empty frame')
        box=[int(xx.min()),int(yy.min()),int(xx.max())+1,int(yy.max())+1]
        boxes.append(box);centers.append([float(xx.mean()),float(yy.mean())]);areas.append(len(xx))
        dense=np.where(np.count_nonzero(mask,axis=1)>=8)[0];bottoms.append(int(dense[-1]))
        edge.append(int(max(a[0].max(),a[-1].max(),a[:,0].max(),a[:,-1].max())))
        if prior is not None:color_deltas.append(float(np.mean(np.abs(array[:,:,:3].astype(float)-prior[:,:,:3].astype(float)))))
        prior=array;array[a==0,:3]=0;frame=Image.fromarray(array)
        frame.save(folder/f'{n:03d}.png');frames.append(frame)
    atlas=Image.new('RGBA',(4096,4096),(0,0,0,0))
    for i,frame in enumerate(frames):atlas.alpha_composite(frame,((i%8)*512,(i//8)*512))
    atlasdir=OUT/'atlases';atlasdir.mkdir(exist_ok=True);atlasfile=atlasdir/(jid+'.png');atlas.save(atlasfile)
    previews=OUT/'review';font=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',20)
    samples=[0,6,12,18,24,30,38,47];sheet=Image.new('RGB',(1200,680),(236,231,217));d=ImageDraw.Draw(sheet)
    d.text((15,12),job['name']+' · '+job['action']+' · 关键帧',font=font,fill=(35,46,41))
    for i,n in enumerate(samples):
        x=(i%4)*300;y=45+(i//4)*310
        bg=Image.new('RGB',(280,280),(76,95,98));im=frames[n].resize((280,280),Image.Resampling.LANCZOS);bg.paste(im,(0,0),im);sheet.paste(bg,(x+10,y))
        d.text((x+150,y+292),f'{n:02d} / {n/12:.2f}s',font=font,fill=(35,46,41),anchor='mm')
    sheet.save(previews/(jid+'-contact.png'))
    animations=[]
    for frame in frames:
        bg=Image.new('RGB',(384,384),(76,95,98));im=frame.resize((384,384),Image.Resampling.LANCZOS);bg.paste(im,(0,0),im);animations.append(bg)
    animations[0].save(previews/(jid+'.gif'),save_all=True,append_images=animations[1:],duration=83,loop=0,disposal=2)
    frames[0].save(previews/(jid+'.webp'),save_all=True,append_images=frames[1:],duration=83,loop=0,lossless=True)
    start=np.array(frames[0]).astype(float);end=np.array(frames[-1]).astype(float)
    center=np.array(centers);shift=float(np.max(np.linalg.norm(center-center[0],axis=1)))
    report={'id':jid,'name':job['name'],'action':job['action'],'sourceVideo':str(video.relative_to(OUT)),'sourceVideoSha256':digest(video),'sourceSize':[w,h],'sourceFrameCount':len(rgb_frames),'sourceFps':meta['fps'],'duration':meta['duration'],'selectedIndices':indices.tolist(),'frameCount':48,'fps':12,'frameSize':[512,512],'atlas':str(atlasfile.relative_to(OUT)),'atlasSha256':digest(atlasfile),'atlasGrid':{'columns':8,'rows':8,'cellWidth':512,'cellHeight':512},'loop':job['action']=='idle','transform':'whole video canvas scaled once to 512; no per-frame crop/recenter','frameBounds':boxes,'maxEdgeAlpha':max(edge),'areaRange':[min(areas),max(areas)],'centroidMaxShiftPixels':round(shift,3),'bottomRange': [min(bottoms),max(bottoms)],'firstLastRgbaMeanDelta':round(float(np.mean(np.abs(start-end))),4),'adjacentRgbMeanDeltas':[round(x,4) for x in color_deltas],'hasMotion':max(color_deltas)>.05,'visualReview':'pending','imported':False}
    write(atlasdir/(jid+'.json'),report)
    print(json.dumps({k:report[k] for k in ['id','frameCount','maxEdgeAlpha','bottomRange','centroidMaxShiftPixels','hasMotion']},ensure_ascii=False))

def gallery():
    manifest=read(OUT/'manifest.json');cards=[]
    for job in manifest['jobs']:
        jid=job['id'];p=OUT/'atlases'/(jid+'.json')
        if not p.exists():continue
        r=read(p)
        cards.append(f'<article><h2>{job["name"]} · {job["action"]}</h2><img src="{jid}.webp"><p>48帧 · 12fps · 512×512</p><a href="{jid}-contact.png">关键帧</a> · <a href="../atlases/{jid}.png">图集</a></article>')
    html='''<!doctype html><html lang="zh"><meta charset="utf-8"><title>6单位动画测试</title><style>body{margin:28px;background:#ece7d9;color:#263533;font:16px sans-serif}h1{font-size:27px}main{display:grid;grid-template-columns:repeat(3,minmax(280px,1fr));gap:20px}article{background:#faf7ee;padding:16px;border-radius:8px}h2{font-size:20px}img{width:100%;background:#4c5f62;border-radius:6px}a{color:#25636c}</style><h1>宝石画风 · 3角色＋3怪物帧动画测试</h1><p>每个单位一个小幅idle、一个单次攻击。原始视频来自即梦CLI；当前为离线预览，未导入游戏。</p><main>'''+''.join(cards)+'</main></html>'
    (OUT/'review'/'index.html').write_text(html,encoding='utf-8')
    print({'clips':len(cards),'gallery':str(OUT/'review'/'index.html')})

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('id');a=p.parse_args()
    if a.id=='gallery':gallery()
    else:process(a.id)
