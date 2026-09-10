"""Deterministic preview from independent art and current BattleBoard layout constants."""
import hashlib,json,shutil
from collections import deque
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw,ImageFont,ImageOps,ImageFilter

ROOT=Path(__file__).resolve().parents[1]
ART=ROOT/'SourceArt/Characters/gemstyle-discussion-20260910'
CAST=ROOT/'SourceArt/Characters/gemstyle-redesign-20260910'
FONT=ROOT/'Saved/FontPreview/20260904/fonts/ZiKuJiangHuGuFeng.ttf'
INK=(47,41,33,255)
SIZE=(1920,1080)
QI_ICON_RENDER_SCALE=1.30
SUBJECT_HEIGHTS={'hero':300,'hunter':275,'you_bai':200,'rooster':195,'goat':180,'moneyrat':265}

def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()

def flood(candidate):
    h,w=candidate.shape;data=bytearray(candidate.astype(np.uint8).tobytes());q=deque()
    def add(i):
        if data[i]==1:data[i]=2;q.append(i)
    for x in range(w):add(x);add((h-1)*w+x)
    for y in range(h):add(y*w);add(y*w+w-1)
    while q:
        i=q.popleft();x=i%w
        if x:add(i-1)
        if x+1<w:add(i+1)
        if i>=w:add(i-w)
        if i+w<len(data):add(i+w)
    return np.frombuffer(data,dtype=np.uint8).reshape(h,w)==2

def matte(source):
    im=Image.open(source).convert('RGB');small=im.resize((800,800),Image.Resampling.LANCZOS)
    c=np.array(small).astype(np.int16)
    bg=np.median(np.concatenate([c[:6].reshape(-1,3),c[-6:].reshape(-1,3),c[:,:6].reshape(-1,3),c[:,-6:].reshape(-1,3)]),axis=0)
    candidate=np.max(np.abs(c-bg),axis=2)<34
    mask=flood(candidate)
    # Enclosed bow/limb/tail gaps: only flat ivory components matching the backdrop.
    remaining=bytearray((candidate&~mask).astype(np.uint8).tobytes());w=800;h=800
    flat=c.reshape(-1,3)
    holes=0
    for initial in range(len(remaining)):
        if not remaining[initial]:continue
        component=[];q=deque([initial]);remaining[initial]=0
        while q:
            i=q.popleft();component.append(i);x=i%w
            for n in ((i-1 if x else -1),(i+1 if x+1<w else -1),(i-w if i>=w else -1),(i+w if i+w<w*h else -1)):
                if n>=0 and remaining[n]:remaining[n]=0;q.append(n)
        if len(component)<90:continue
        pixels=flat[component]
        if np.max(np.abs(np.mean(pixels,axis=0)-bg))<6 and np.max(np.std(pixels,axis=0))<4:
            mask.reshape(-1)[component]=True;holes+=1
    alpha=Image.fromarray((~mask*255).astype(np.uint8)).filter(ImageFilter.MinFilter(3)).resize(im.size,Image.Resampling.LANCZOS)
    result=im.convert('RGBA');result.putalpha(alpha)
    return result,holes

def old_identity_bounds(path):
    c=np.array(Image.open(path).convert('RGB')).astype(np.int16)
    key=(c[:,:,0]>145)&(c[:,:,2]>135)&(c[:,:,1]<135)&(np.minimum(c[:,:,0],c[:,:,2])-c[:,:,1]>65)
    im=Image.fromarray((~key*255).astype(np.uint8))
    return im.getbbox()

def prepare_units():
    jobs={j['slug']:j for j in json.loads((CAST/'art-jobs.json').read_text(encoding='utf-8'))['jobs']}
    units={};checks=[];out=ART/'preview-cutouts';out.mkdir(exist_ok=True)
    for slug in ('hero','hunter','you_bai','rooster','goat','moneyrat'):
        j=jobs[slug];source=ART/j['group']/(slug+'.png');clean,holes=matte(source)
        bounds=clean.getchannel('A').point(lambda a:255 if a>=64 else 0).getbbox()
        assert bounds
        old=old_identity_bounds(ROOT/j['identityReference'])
        assert old
        # User requested comparable human scale, smaller spirit/minions, boss mass.
        wanted_height=round(SUBJECT_HEIGHTS[slug]*1600/410)
        target=(1440,wanted_height)
        fit=ImageOps.contain(clean.crop(bounds),target,Image.Resampling.LANCZOS)
        stage=Image.new('RGBA',(1600,1600),(0,0,0,0))
        foot=round((315 if slug=='you_bai' else 335)*1600/410)
        pos=(round((1600-fit.width)/2),foot-fit.height)
        stage.alpha_composite(fit,pos)
        file=out/(slug+'.png');stage.save(file);units[slug]=stage
        checks.append({'slug':slug,'source':str(source.relative_to(ROOT)),'sourceSha256':digest(source),'cutout':str(file.relative_to(ART)),'sha256':digest(file),'oldIdentityBounds':old,'newArtworkBounds':bounds,'placementIn1600':pos,'normalizedArtworkSize':fit.size,'subjectStageHeight':SUBJECT_HEIGHTS[slug],'enclosedBackgroundRegionsRemoved':holes})
    review=Image.new('RGB',(1800,720),(100,119,119))
    for i,(slug,im) in enumerate(units.items()):
        thumb=im.resize((300,300),Image.Resampling.LANCZOS);review.paste(thumb,((i%6)*300,0),thumb)
        review.paste((229,115,187),((i%6)*300,340,(i%6+1)*300,640));review.paste(thumb,((i%6)*300,340),thumb)
    review.save(ART/'cutout-edge-review.png')
    return units,checks

def card_base(raw_name='card-base-raw.png',original_source='SourceArt/UI/PSD/gamexxk-v4/ui-master/RuntimeApproved/T_MasterV2_CardFrame.png',output_name='card-base.png'):
    original=Image.open(ROOT/original_source).convert('RGBA')
    target=original.getchannel('A').getbbox();size=original.size
    im=Image.open(ART/raw_name).convert('RGBA');a=np.array(im);c=a[:,:,:3].astype(np.int16)
    mask=(c[:,:,0]>145)&(c[:,:,2]>130)&(c[:,:,1]<120)&(np.minimum(c[:,:,0],c[:,:,2])-c[:,:,1]>65)
    for _ in range(2):
        edge=mask.copy();edge[1:]|=mask[:-1];edge[:-1]|=mask[1:];edge[:,1:]|=mask[:,:-1];edge[:,:-1]|=mask[:,1:]
        mask|=edge&(np.minimum(c[:,:,0],c[:,:,2])-c[:,:,1]>20)
    a[:,:,3][mask]=0;clean=Image.fromarray(a);bounds=clean.getchannel('A').getbbox();assert bounds
    fit=clean.crop(bounds).resize((target[2]-target[0],target[3]-target[1]),Image.Resampling.LANCZOS)
    final=Image.new('RGBA',size,(0,0,0,0));final.alpha_composite(fit,(target[0],target[1]));final.save(ART/output_name)
    assert final.size==original.size and final.getchannel('A').getbbox()==target
    return final,{'originalSource':original_source,'file':output_name,'originalSize':list(size),'newSize':list(final.size),'originalAlphaBounds':list(target),'newAlphaBounds':list(final.getchannel('A').getbbox()),'exactCanvasAndMarginsMatch':True,'sha256':digest(ART/output_name)}

def font(px):return ImageFont.truetype(str(FONT),px)
def text(layer,xy,s,px,fill=INK,anchor='mm',stroke=0):ImageDraw.Draw(layer).text(xy,s,font=font(px),fill=fill,anchor=anchor,stroke_width=stroke,stroke_fill=(39,29,21,255))
def paste(layer,image,xy,size=None):
    if size:image=image.resize(tuple(map(round,size)),Image.Resampling.LANCZOS)
    layer.alpha_composite(image,tuple(map(round,xy)))

def main():
    assert FONT.exists()
    units,checks=prepare_units();base,card_check=card_base()
    orb,qi_check=card_base('qi-base-raw.png','SourceArt/UI/Battle/PartyQi/battle_party_qi_soul_orb_v1.png','qi-base.png')
    button,button_check=card_base('end-turn-base-raw.png','docs/ui/main_menu/source_art/ink_button_base.png','end-turn-base.png')
    background=Image.open(ART/'background-pure.png').convert('RGBA').resize(SIZE,Image.Resampling.LANCZOS)
    unit_layer=Image.new('RGBA',SIZE,(0,0,0,0));ui=Image.new('RGBA',SIZE,(0,0,0,0))
    # Exact current TryResolveFixedUnitHudLayout anchors + the visual's -140px offset.
    entries=[('rooster','公鸡',True,1,.095,.60,46,0),('moneyrat','金钱鼠',True,2,.245,.52,240,0),('goat','山羊',True,3,.395,.44,58,0),('you_bai','幽白',False,3,.605,.44,84,34),('hero','主角',False,2,.755,.52,100,30),('hunter','弓手',False,1,.905,.60,90,28)]
    positions=[]
    for slug,name,enemy,slot,x,y,hp,mana in entries:
        anchor=(1920*x,1080*(y+.025));center=(anchor[0],anchor[1]-140)
        paste(unit_layer,units[slug],(center[0]-205,center[1]-205),(410,410))
        positions.append({'slug':slug,'side':'enemy' if enemy else 'party','slot':slot,'visualCenter':center,'visualSize':[410,410],'hudAnchor':anchor})
        text(ui,(anchor[0],anchor[1]+14),f'{"敌" if enemy else "我"}{slot}P · {name}',24)
        for index,(value,label,color) in enumerate([(hp,'气血',(196,81,53,255))]+([] if enemy else [(mana,'内力',(112,165,139,255))])):
            bx,by=round(anchor[0]-126),round(anchor[1]+30+index*29)
            d=ImageDraw.Draw(ui);d.rounded_rectangle((bx,by,bx+252,by+27),radius=4,fill=(46,40,33,255));d.rectangle((bx+3,by+4,bx+249,by+23),fill=color)
            text(ui,(anchor[0],by+14),f'{label} {value}/{value}',22,(255,248,224,255),stroke=1)
    def face(x,y,slug,title,cost=None,intent=None):
        paste(ui,base,(x,y),(206,285))
        cropped=units[slug].crop(units[slug].getchannel('A').point(lambda a:255 if a>64 else 0).getbbox())
        # Runtime portrait slot is 190x228 at (8,48); use a bust for characters.
        if slug in ('hero','hunter'):cropped=cropped.crop((0,0,cropped.width,round(cropped.height*.69)))
        fit=ImageOps.contain(cropped,(190,216),Image.Resampling.LANCZOS)
        paste(ui,fit,(x+8+(190-fit.width)/2,y+58+(216-fit.height)))
        text(ui,(x+103,y+31),title,32)
        if cost:
            text(ui,(x+18,y+99),cost[0]+'气',31,(255,244,212,255),anchor='lm',stroke=2)
            text(ui,(x+18,y+133),cost[1]+'内',31,(255,244,212,255),anchor='lm',stroke=2)
        if intent:text(ui,(x+103,y+102),intent,29,(255,244,212,255),stroke=2)
    hand=[('hero','青锋一式',('1','1')),('hunter','蓄力',('0','1')),('hunter','重箭',('1','3')),('you_bai','青焰点灯',('0','3')),('you_bai','山河残图',('0','6'))]
    hand_rects=[]
    for i,(slug,title,cost) in enumerate(hand):
        x=375+4+i*214;y=775
        face(x,y,slug,title,cost=cost);hand_rects.append([x,y,206,285])
    intent_rects=[]
    for i,(slug,title,info) in enumerate([('rooster','啄击','伤害 12'),('moneyrat','敛财','蓄势'),('goat','顶角','伤害 10')]):
        left=1920*.32-822*.5+i*274
        text(ui,(left+28,41),'敌方',23);text(ui,(left+28,76),f'{i+1}P',26)
        x=left+60;face(x,24,slug,title,intent=info);intent_rects.append([x,24,206,285])
    for x,label in ((1430,'自动战斗：关'),(1628,'关闭')):
        paste(ui,button,(x,86),(186,60));text(ui,(x+93,116),label,27,(255,248,227,255))
    paste(ui,button,(1656,990),(224,76));text(ui,(1768,1028),'结束回合',40,(255,248,227,255))
    qi_render_size=round(140*QI_ICON_RENDER_SCALE)
    qi_render_origin=(1768-qi_render_size/2,906-qi_render_size/2)
    paste(ui,orb,qi_render_origin,(qi_render_size,qi_render_size));text(ui,(1768,906),'3',75)
    text(ui,(89,47),'地势·平原',27)
    art_only=Image.alpha_composite(background,unit_layer)
    final=Image.alpha_composite(art_only,ui)
    background.convert('RGB').save(ART/'background-stage-1920.png')
    unit_layer.save(ART/'unit-layer.png');ui.save(ART/'ui-overlay.png')
    art_only.convert('RGB').save(ART/'layout-art-only.png')
    # Replace the preview atomically so an open image viewer cannot block writing it.
    next_preview=ART/'layout-preview.next.png'
    final.convert('RGB').save(next_preview)
    next_preview.replace(ART/'layout-preview.png')
    legacy=ART/'battle-scene.png'
    if legacy.exists() and not (ART/'revisions/ai-composed-screen-rejected.png').exists():shutil.copy2(legacy,ART/'revisions/ai-composed-screen-rejected.png')
    shutil.copy2(ART/'layout-preview.png',legacy)
    report={'kind':'deterministic offline layout preview; not an in-engine screenshot','stage':list(SIZE),'editorAvailable':False,'layoutSource':'Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp','visualSource':'Source/GameXXK/Private/UI/GameXXKBattleUnitVisualWidget.cpp','font':str(FONT.relative_to(ROOT)),'sampleValuesNotLiveSave':True,'units':positions,'cutoutChecks':checks,'cardBaseCheck':card_check,'handRects':hand_rects,'enemyIntentRects':intent_rects,'handRowRect':[375,775,1170,287],'toolbarRect':[1430,86,384,60],'endTurnRect':[1656,990,224,76],'qiRect':[1698,836,140,140],'previewSha256':digest(ART/'layout-preview.png'),'backgroundSha256':digest(ART/'background-pure.png')}
    report['uiBaseChecks']=[card_check,qi_check,button_check]
    report['qiIconRenderScale']=QI_ICON_RENDER_SCALE
    report['qiIconRenderedRect']=[*qi_render_origin,qi_render_size,qi_render_size]
    report['userProportions']={'subjectHeights':SUBJECT_HEIGHTS,'bossEnemySlot':2,'foregroundBrightness':'unchanged','distance':'lighter mountains and background pine/bamboo; lower contrast'}
    (ART/'layout-spec.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'ok':True,'independentUnits':6,'canvas':list(SIZE),'cardBase':card_check,'layoutPreview':str(ART/'layout-preview.png')},ensure_ascii=False))

if __name__=='__main__':main()
