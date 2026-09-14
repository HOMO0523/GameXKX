"""Create and package reference-led cast art review, without runtime imports."""
import argparse,hashlib,json,shutil,zipfile
from pathlib import Path
from PIL import Image,ImageOps
from package_project_design_characters import DETAILS,bounds
from package_blade_red_compact import sheet
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'SourceArt/Characters/cast-unified-20260913'
OLD=ROOT/'SourceArt/Characters/project-design-gemstyle-all-20260910'
FEMALE=ROOT/'SourceArt/Characters/female-guofeng-concepts-20260912'
APPROVED=Path('C:/Users/shxuw/AppData/Local/Temp/codex-clipboard-f18b0d9e-0d10-49a8-bb8d-c32ec742b420.png')
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def write(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
COMMON='''Use case: identity-preserving style transfer. Image1 is the user's FINAL APPROVED STYLE AND PROPORTION reference, a female blade fighter. Image2 is the identity/design of the ONE character to redraw. Do not copy the blade woman's face, gender, clothes, red palette, weapon or hairstyle onto other characters. Render the subject of image2 in the EXACT game art language of image1: bold crisp dark contour with line-weight hierarchy, coherent broad colored planes, simple graphic eyes with at most one highlight, restrained handpainted shading; no tiny polygon mosaic, noisy folds, photoreal eyes or 3D gloss. Human crown-to-sole ratio close to image1 approximately four heads excluding hats/ponytails; retain intentional stocky/slender identity variation within this compact range. Adult faces retain age and character. Women are graceful capable adult women with natural fuller feminine curves, tapered waists, but avoid thick barrel torsos, swollen thighs or muscular chest plates. Men retain each original masculine age, physique and facial hair. Preserve unique original palette and meaningful accessories. Chinese wuxia/folk costume, strong silhouette, detail concentrated at one or two focal areas and large quiet cloth areas. Body, face and gaze three-quarter SCREEN LEFT. Natural slight asymmetric weight shift rather than parallel stiff legs; clearly separate two legs and two feet, coherent hip/knee/ankle anatomy, robe must not hide second foot. Hands and equipment mechanically correct. Entire full body including weapons, hats, hair and feet inside square canvas with 8 percent empty margin. Plain uniform warm ivory #F4EFDF background, no ground shadow, no text, labels, panel or extra characters. Keep the identity and clothing cuts of image2 with proportion/drawing refinement, not a new costume. SPECIFIC IDENTITY: '''
def init():
    for d in ['references','raw','characters','prompts','review']:(OUT/d).mkdir(parents=True,exist_ok=True)
    shutil.copy2(APPROVED,OUT/'references/approved_blade.png')
    shutil.copy2(APPROVED,OUT/'raw/blade.png')
    women={j['slug']:j for j in read(FEMALE/'design-brief.json')['jobs']}
    jobs=[]
    for r in read(OLD/'manifest.json')['records']:
        slug=r['slug'];name=r['name'];ref=OLD/r['file'];detail=DETAILS.get(slug,'')
        if slug=='blade':
            jobs.append(dict(slug=slug,name='女刀客',group='职业伙伴',approved=True,reference=str(OUT/'references/approved_blade.png'),sourceHash=sha(APPROVED)));continue
        if slug=='hero':detail='男主角：保留年轻成年旅人，黑色小髻、棕褐短外衣、灰蓝交领内搭与束腿裤、米白绑腿、棕布靴；背竹篓露竹竿宽叶，腰挂圆编织水壶，一手握肩带另一手自然放松。眉眼坚毅亲和，简化编织成少数宽线。';name='男主角'
        if slug in ['hunter','sorcerer']:
            key='female_archer' if slug=='hunter' else 'female_taoist';ref=FEMALE/'characters'/(key+'.png');detail=women[key]['details'];name='女弓手' if slug=='hunter' else '女道士'
            detail+=' 本次体态以样板的干练自然曲线为准，不要加宽加胖。'
        if slug=='hunter':detail+=' 弓弦结构优先：恰好一根连续绷紧弦，从上弓梢到箭尾扣弦点再到下弓梢；箭杆一根直线，箭尾与弦交汇，握弓手握木弓把，另一手轻持箭尾。保留低持弓姿态，不满弓拉射，不多出交叉弦。'
        if slug=='you_bai':detail='幽白是纯蓝紫灵火精灵，不是人。保留面朝左的机灵图形眼睛、蓝紫脸和大卷云火焰尾，只有少数大火焰分叉和两个小火星。没有人类躯干、手脚或衣服，不应用人类四头身。匹配样板的描边、眼睛与大块明暗，火焰有流动图形感但不碎。'
        group='主角' if slug=='hero' else ('职业伙伴' if slug in ['guard','healer','hunter','sorcerer','formation_master'] else '剧情伙伴')
        jobs.append(dict(slug=slug,name=name,group=group,approved=False,reference=str(ref),sourceHash=sha(ref),prompt=COMMON+detail,references=[str(OUT/'references/approved_blade.png'),str(ref)]))
    ref=FEMALE/'characters/female_hero.png'
    jobs.insert(1,dict(slug='female_hero',name='女主角',group='主角',approved=False,reference=str(ref),sourceHash=sha(ref),prompt=COMMON+women['female_hero']['details']+' 保留有女性特征但干练的自然曲线，不加粗腰胯和腿；让亲和机灵的脸区别于冷艳刀客。',references=[str(OUT/'references/approved_blade.png'),str(ref)]))
    write(OUT/'jobs.json',dict(count=14,new=13,approvedReused=1,phase='one-proportion-set-for-review',imported=False,jobs=jobs))
    print(json.dumps(dict(count=14,new=13,approvedReused=1)))
def package():
    jobs=read(OUT/'jobs.json')['jobs'];records=[]
    for j in jobs:
        raw=OUT/'raw'/(j['slug']+'.png')
        if not j['approved']:
            pf=OUT/'prompts'/(j['slug']+'.json');p=read(pf);source=Path(p['generatedPath'])
            if source.resolve()!=raw.resolve():shutil.copy2(source,raw)
            p.setdefault('originalGeneratedPath',str(source));p['generatedPath']=str(raw);write(pf,p)
        with Image.open(raw) as im:
            native=list(im.size);fit=ImageOps.contain(im.convert('RGB'),(1360,1360),Image.Resampling.LANCZOS)
            full=Image.new('RGB',(1600,1600),(244,239,223));full.paste(fit,((1600-fit.width)//2,(1600-fit.height)//2))
        p=OUT/'characters'/(j['slug']+'.png');full.save(p);box,_=bounds(full)
        assert min(box[0],box[1],1600-box[2],1600-box[3])>=120
        assert sha(Path(j['reference']))==j['sourceHash']
        records.append(dict(slug=j['slug'],name=j['name'],group=j['group'],file=str(p.relative_to(OUT)),raw=str(raw.relative_to(OUT)),nativeSize=native,size=[1600,1600],sha256=sha(p),rawSha256=sha(raw),bounds=box,approved=j['approved']))
    assert len(records)==14 and len({r['rawSha256'] for r in records})==14
    assert sha(OUT/'raw/blade.png')==sha(OUT/'references/approved_blade.png')
    items=lambda rr:[(r['name'],OUT/r['file']) for r in rr]
    sheet(items(records),OUT/'review/all-cast.png','主角与全伙伴 · 统一画风核对','14张 / 刀客沿用确认图 / 其余角色独立重设计 / 尚未导入',5,390)
    for group,slug in [('主角','heroes'),('职业伙伴','role-companions'),('剧情伙伴','story-companions')]:
        rr=[r for r in records if r['group']==group]
        sheet(items(rr),OUT/'review'/(slug+'.png'),group+' · 单组核对','保留各自身份、配色、道具和性格',min(len(rr),3),560)
    women=[r for r in records if r['slug'] in ['female_hero','blade','healer','hunter','sorcerer','qiong_yao_er']]
    sheet(items(women),OUT/'review/female-cast.png','女性角色 · 差异与统一核对','旅人 / 冷艳刀客 / 温和药师 / 轻快弓手 / 沉静道士 / 明快铃舞',3,560)
    sheet(items(records),OUT/'review/readability.png','小尺寸辨识检查','同展示框等比缩放 · 非实际局内身高',7,180)
    write(OUT/'manifest.json',dict(count=14,imported=False,phase='awaiting-user-review',approvedBladeExact=True,records=records))
    (OUT/'README.txt').write_text('全伙伴与主角 · 统一画风核对\n\n共14张：男、女主角；女刀客、守卫、药师、女弓手、女道士、阵师；土司首领、宋金宝、幽白、周光祖、金贵、琼幺儿。\n刀客直接使用本次用户确认附件，raw字节一致。其余13张按用户要求重新设计服装剪裁、图形方向、姿态与性格表达，保留个人身份、主题与配色；统一参考刀客的画风和紧凑比例。\ncharacters为1600×1600审阅画布，原生生成尺寸见manifest和raw。\n先看review总览与分组，逐个反馈名字及要改之处。用户本轮确定先做一套核对，后续再扩展挂机Q版；未导入、未做动画。\n称呼统一为道士；本轮不改运行时代码。\n',encoding='utf-8-sig')
    archive=OUT/'GameXXK_主角与全伙伴_14张统一画风核对包.zip'
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
        for d in ['characters','raw','review','prompts','references']:
            for p in sorted((OUT/d).glob('*')):
                if p.is_file():z.write(p,p.relative_to(OUT))
        for n in ['README.txt','jobs.json','manifest.json','redesign-directions.json']:z.write(OUT/n,n)
    with zipfile.ZipFile(archive) as z:assert z.testzip() is None
    print(json.dumps(dict(count=14,safeBounds=True,unique=True,sourceHashesUnchanged=True,approvedBladeExact=True,zipIntegrity=True,zipBytes=archive.stat().st_size)))
if __name__=='__main__':
    a=argparse.ArgumentParser();a.add_argument('action',choices=['init','package']);args=a.parse_args()
    init() if args.action=='init' else package()
