"""Curate a few clean poses and make isolated 2K runtime pilot atlases."""
import json
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
from dreamina_gemstyle_animation_pilot import OUT,ROOT,read,write
from package_project_design_characters import sha

PICKS={
 'hero_idle':[0,6,12,18,24,30,36,42],
 'hero_attack':[0,8,12,16,18,29,32,35,39,47],
 'rooster_idle':[0,6,12,18,24,30,36,42],
 'rooster_attack':[0,8,12,17,20,24,29,34,39,47],
}
TARGET=OUT/'keyframe-pilot'

def main():
    TARGET.mkdir(exist_ok=True);records=[];font=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',20)
    for jid,indices in PICKS.items():
        unit,action=jid.split('_');idle=Image.open(OUT/'frames'/(unit+'_idle')/'000.png').convert('RGBA')
        box=idle.getchannel('A').point(lambda a:255 if a>128 else 0).getbbox();assert box
        target_height=375 if unit=='hero' else 422
        foot=419 if unit=='hero' else 471
        scale=target_height/(box[3]-box[1]);ox=256-scale*(box[0]+box[2])/2;oy=foot-scale*box[3]
        target=TARGET/jid;target.mkdir(exist_ok=True)
        atlas=Image.new('RGBA',(2048,2048),(0,0,0,0));frames=[]
        sheet=Image.new('RGB',(len(indices)*200,260),(236,231,217));draw=ImageDraw.Draw(sheet)
        for i,n in enumerate(indices):
            source=Image.open(OUT/'frames'/jid/f'{n:03d}.png').convert('RGBA')
            im=source.transform((512,512),Image.Transform.AFFINE,(1/scale,0,-ox/scale,0,1/scale,-oy/scale),Image.Resampling.BICUBIC)
            alpha=im.getchannel('A');assert alpha.getbbox();assert alpha.crop((0,0,512,2)).getextrema()[1]==0
            im.save(target/f'{i:02d}.png');frames.append(im)
            atlas.alpha_composite(im.resize((256,256),Image.Resampling.LANCZOS),((i%8)*256,(i//8)*256))
            bg=Image.new('RGB',(200,200),(76,95,98));thumb=im.resize((200,200),Image.Resampling.LANCZOS);bg.paste(thumb,(0,0),thumb);sheet.paste(bg,(i*200,25));draw.text((i*200+100,241),f'关键帧{i} ← 原帧{n}',font=font,anchor='mm',fill=(35,46,41))
        name=f'T_Gemstyle_{unit.title()}_{action.title()}';atlasfile=TARGET/(name+'.png');atlas.save(atlasfile)
        sheet.save(TARGET/(jid+'-poses.png'))
        previews=[]
        for frame in frames:
            bg=Image.new('RGB',(384,384),(76,95,98));thumb=frame.resize((384,384),Image.Resampling.LANCZOS);bg.paste(thumb,(0,0),thumb);previews.append(bg)
        fps=4 if action=='idle' else 10
        previews[0].save(TARGET/(jid+'.gif'),save_all=True,append_images=previews[1:],duration=round(1000/fps),loop=0,disposal=2)
        records.append({'id':jid,'textureName':name,'assetPath':f'/Game/GameXXK/BattleAnimations/GemstylePilot/{name}','atlas':str(atlasfile.relative_to(ROOT)),'sha256':sha(atlasfile),'frameCount':len(indices),'fps':fps,'columns':8,'rows':8,'cellSize':256,'atlasSize':2048,'sourceFrameIndices':indices,'fixedTransform':{'scale':scale,'offset':[ox,oy],'referenceIdleBounds':box,'targetHeightAt512':target_height,'footAt512':foot},'note':'Hero attack omits generated pink effect frames; no extra generation.' if jid=='hero_attack' else ('Rooster restored to production idle alpha height 0.824219 and original 41px foot margin.' if unit=='rooster' else 'Curated poses from existing successful clip.')})
    write(TARGET/'manifest.json',{'count':4,'units':['hero','rooster'],'enabledBy':'GameXXK.BattleAnimation.GemstyleKeyframes 1','disableBy':'GameXXK.BattleAnimation.GemstyleKeyframes 0','records':records})
    print({'clips':4,'idleFrames':8,'attackFrames':10,'atlas':[2048,2048],'frameCells':[256,256]})

if __name__=='__main__':main()
