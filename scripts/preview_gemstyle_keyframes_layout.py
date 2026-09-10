"""Offline composition of the curated poses at the existing discussion layout."""
import json
from pathlib import Path
import numpy as np
import imageio_ffmpeg
from PIL import Image,ImageDraw,ImageFont
from dreamina_gemstyle_animation_pilot import ROOT,OUT

def main():
    art=ROOT/'SourceArt/Characters/gemstyle-discussion-20260910'
    spec=json.loads((art/'layout-spec.json').read_text(encoding='utf-8'))
    background=Image.open(art/'background-stage-1920.png').convert('RGBA')
    overlay=Image.open(art/'ui-overlay.png').convert('RGBA')
    heights={'hero':300,'hunter':275,'you_bai':200,'rooster':195,'goat':180,'moneyrat':265}
    still={}
    for slug in heights:
        im=Image.open(OUT/'cutouts'/(slug+'.png')).convert('RGBA')
        box=im.getchannel('A').getbbox();im=im.crop(box)
        still[slug]=im.resize((round(im.width*heights[slug]/im.height),heights[slug]),Image.Resampling.LANCZOS)
    motions={}
    for unit in ['hero','rooster']:
        for action in ['idle','attack']:
            folder=OUT/'keyframe-pilot'/(unit+'_'+action)
            motions[(unit,action)]=[Image.open(p).convert('RGBA').resize((410,410),Image.Resampling.LANCZOS) for p in sorted(folder.glob('*.png'))]
    target=OUT/'keyframe-pilot'/'offline-battle-layout.mp4'
    writer=imageio_ffmpeg.write_frames(str(target),(1280,720),fps=12,codec='libx264',pix_fmt_in='rgb24',quality=8,macro_block_size=1,output_params=['-movflags','+faststart'])
    writer.send(None);font=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',20)
    try:
        for n in range(48):
            time=n/12;canvas=background.copy()
            for row in spec['units']:
                slug=row['slug'];cx,cy=row['visualCenter'];anchor=row['hudAnchor']
                if slug in ['hero','rooster']:
                    start=1.25 if slug=='hero' else 2.5
                    action='attack' if start<=time<start+1 else 'idle'
                    frames=motions[(slug,action)];frame=int((time-start)*10) if action=='attack' else int(time*4)%8
                    canvas.alpha_composite(frames[min(frame,len(frames)-1)],(round(cx-205),round(cy-205)))
                else:
                    im=still[slug];foot=anchor[1]-(30 if slug=='you_bai' else 10)
                    canvas.alpha_composite(im,(round(cx-im.width/2),round(foot-im.height)))
            canvas=Image.alpha_composite(canvas,overlay)
            d=ImageDraw.Draw(canvas);d.rounded_rectangle((1110,8,1900,42),radius=5,fill=(20,29,28,195))
            d.text((1505,25),'离线排版预览 · 主角与公鸡 · idle 8帧 / attack 10帧',font=font,anchor='mm',fill=(250,246,231))
            if n==22:canvas.convert('RGB').save(OUT/'keyframe-pilot'/'offline-battle-layout.png')
            writer.send(np.array(canvas.convert('RGB').resize((1280,720),Image.Resampling.LANCZOS)))
    finally:writer.close()
    print(target)

if __name__=='__main__':main()
