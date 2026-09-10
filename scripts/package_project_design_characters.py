"""Review-only packaging for the approved project-design gem-style character set."""
import argparse
import hashlib
import json
import shutil
import zipfile
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageOps

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'SourceArt/Characters/project-design-gemstyle-all-20260910'
APPROVED = ROOT / 'SourceArt/Characters/project-design-gemstyle-20260910'
BG = (244, 239, 223)
DETAILS = {
    'blade': '刀客：严格保留原图年轻锐利眉眼、红发绳的黑色高长马尾、青绿色交领短武衣与两枚系扣、灰米翻袖与尖分片衣摆、红色长腰带两片飘带、深棕裤、米色绑腿与棕布鞋。一手低持原宽弯刀向左下，另一手握拳自然放低，双腿稳立原姿势。刀的弧形、圆护手、刀柄小红穗保持，只有一把刀，完整露出；不要换成红衣或加盔甲。',
    'guard': '守卫：保留原图方脸、短下巴胡须和鬓角、黑色小髻、粗眉，宽肩壮硕短腿。棕黄色无袖护衣有几块大方形面，内穿灰靛蓝交领衣、深棕裤与米色绑腿、棕鞋。原图画面左侧的手握木杆长矛，画面右侧的臂持一面圆木盾，木盾中心仅一个灰铁圆盾脐；盾形必须是圆形而非方盾，矛杆从手上下延伸成一根直线，矛头为单个灰铁叶形尖。保留双脚稳立与原盾的位置，不添板甲、披风。',
    'healer': '药师：保留原年轻女子柔和笑脸、黑发单个后上发髻与两片大叶束饰，米白交领上衣和宽裤、橄榄草绿短披肩及两片向右后方飘的宽布尾、原绿腰结与布鞋。一臂抱原木棕色双腹药葫芦，葫芦下腹有单片叶形标记，另一手握肩带；背篓露两卷药卷和少量草叶。不新增手持药枝、不改成双髻、不换裙子。保留原站姿与服装结构，叶子和编织线做减法。',
    'sorcerer': '法师：保留原矮圆少年、调皮单眼眨眼微笑，蓝色圆帽带额前圆饰、帽顶小金扣、后侧短系带；原灰靛蓝宽袖长衣、米金色粗衣缘与大圆项圈，胸前一个简洁圆环纹、宽蓝裤和棕鞋。一只手低持短木法杆朝左下，另一手朝上，掌上恰好两张空白米色符纸与少量短金色弧线。不要改成尖帽欧美法师，不加法杖宝石、火球或大量符文，保持原两手动作。',
    'formation_master': '阵师：保留原黑发小髻与横发簪、半眯眼淡定浅笑，灰褐长外袍、浅米灰交领长衣、黑灰腰带和两条长带尾、原布鞋。画面左侧手向左下伸出持一把原折扇，另一手在身后；背后画面右侧保留一个竹木框的竖长方形米灰阵旗，横杆、竖杆和少数布环结构清晰。折扇和旗面无字，不加八卦地阵、漂浮石头或额外法器，保留原从容站姿。',
    'tusi_chief': '土司首领：保留原宽矮威严身形、黝暖肤色的方脸方鼻、黑色粗眉鬓角、两条巨大向上弯的象牙白胡须和白色尖山羊胡；原一边高翘的不对称大竹斗笠、黑色大衣领与赭红橘色宽厚锯齿下摆披风、米黄内衣、黑裤和棕鞋、胸口一个六角铜扣。一手竖握原木杖，杖头只有一个开口向上的U形月牙。保持短壮稳立，胡须必须像原图属于面部而不是额头的牛角，不改为野兽，不加羽毛铠甲。',
    'song_jin_bao': '宋金宝：保留原瘦高成年男子、尖鼻尖下巴、挑眉与露齿狡黠笑容，黑发、原高尖顶斜斗笠；朱橘红交领长短袍、深褐宽衣缘和腰带、米金色前襟大折角回纹、腰侧小方钱盒、棕色宽裤、细绑腿和尖头黑棕鞋。一手举恰好三张空白米色长纸牌在脸左前方，另一手叉后腰，原双腿姿势保持。必须保持瘦高四至五头身差异，不能画成主角一样短胖，不能用西方巫师帽代替原竹编斗笠。',
    'zhou_guang_zu': '周光祖：保留原宽肩结实体形、半眯眼老成脸、两边长弯黑胡须与尖下巴胡、小黑髻、巨大扁宽椭圆竹斗笠。黑灰交领长袍、米黄腰绳与宽带尾、墨绿宽裤和原棕鞋。肩头保留恰好一只橙色白下巴白尾尖的小狐狸，原样绕在肩后，画面右侧狐头和一条卷大尾巴能读清；不是第二个人，也不是狐狸披风。前手托原一束卷起的竹简斜向左上，后手收在背后；原屈膝稳立动作，简化竹纹与狐狸毛为大面。',
    'jin_gui': '金贵：保留原矮个淡漠男子、困倦半睁眼、面颊细长疤痕、深棕短发、原宽圆锥竹斗笠与顶端红叶/红布结。穿原淡青绿交领短衣、米白领缘、米黄细腰绳、绿宽裤、腰侧小棕袋、深绿布鞋。一手握原细长竹烟杆近嘴，杆身向左下伸，远端有一个小弯烟斗，另一手背在后面；前脚微抬的闲散原姿势保留。不增加烟雾、酒瓶或武器，不改成富贵金袍。',
    'qiong_yao_er': '琼幺儿：保留原年轻女子明亮笑眼、棕黑侧辫与原耳环、乳白圆厚民族帽沿及一排大圆珠、帽顶赭红小方块；朱橘红短上衣、米白交领和袖边、暖金黄长裙、白腰带与两条长白红渐变飘带、绑腿和原棕鞋。前手持原绿色花瓣形小铃灯/法铃，下接小穗，另一只向后舒展的手腕挂一个小金铃。保留轻盈扭身迈步的原动作线，但脸部与胸廓必须转成3/4面向画面左，看向左边而非原图的右边；两条飘带向画面右后延伸。保留高挑灵动比例，不改成小幼童，不新增头冠或莲花台。',
}
COMMON = '''Use case: style-transfer. Asset type: Chinese game full-body single character review illustration.
输入图片1是项目宝石icon，只提供块面、用色和线条画风；图片2是用户刚确认的三名角色风格样板，只参考绘制语言，绝不复制他们的服装或道具；图片3才是本次唯一要画的角色的项目原设计。
按图片3忠实重绘，保留原身份、性别年龄感、脸部特征、发型、身体比例、服装结构和原配色、标志道具、原动作线，只让画法达到图片2批准稿的统一水平。所有角色面部、目光和身体为3/4朝画面左。轮廓用干净有粗细层次的深墨描边；饱满鲜明的手绘卡通大体块，3至5组主要明暗，布褶整合成少数宽面，少量宽笔触。国风与中国民间江湖服饰的气质，保持各人的比例差异和个性。皮肤、布和毛发仍为有机材质，不把角色做成真正水晶、多边形低模或金属玩具。去掉原图纸纹、碎细线、灰脏纹理、毛糙断线，不加水墨晕染、不画欧美奇幻铠甲。
单张正方形，只画一个完整全身角色含所有原道具，四周至少8%安全留白，不裁帽子武器脚或飘带。无文字、无标签、无边框、无样板三人、无宝石图标、无场景、无地面投影。干净均匀浅暖米白#F4EFDF背景，不要洋红色或棋盘格。请精修双手与道具接触结构、整齐完整肢体，宁可减少装饰也要剪影清晰。
本角色锁定要点：'''

def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))

def write(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def init():
    for folder in ['raw', 'characters', 'prompts', 'review']:
        (OUT/folder).mkdir(parents=True, exist_ok=True)
    roster = [j for j in read(ROOT/'SourceArt/Characters/gemstyle-redesign-20260910/art-jobs.json')['jobs'] if j['group']=='characters']
    approved = {r['slug']:r for r in read(APPROVED/'manifest.json')['records']}
    jobs = []
    for item in roster:
        slug = item['slug']
        j = {k:item[k] for k in ['slug','name','id','identityReference','identitySourceSha256']}
        assert sha(ROOT/j['identityReference'])==j['identitySourceSha256']
        j.update(facing='left', view='three-quarter', userApproved=slug in approved)
        if slug in approved:
            for sub in ['characters','raw','prompts']:
                ext = '.json' if sub=='prompts' else '.png'
                source=APPROVED/sub/(slug+ext)
                target=OUT/sub/source.name
                if not target.exists(): shutil.copy2(source,target)
            j['approvedSha256']=approved[slug]['sha256']
            j['status']='approved-reused-exactly'
        else:
            j.update(details=DETAILS[slug], prompt=COMMON+DETAILS[slug], status='pending-generation')
            j['references']=[str(ROOT/'SourceArt/UI/Items/Gems/contour-review-20260909/gem-types-review-v5.png'),str(APPROVED/'three-characters.png'),str(ROOT/j['identityReference'])]
        jobs.append(j)
    write(OUT/'jobs.json', {'count':13,'newDrawings':10,'approvedReused':3,'tool':'built-in image_gen','scope':'all 13 project characters; monsters not included','approvalGate':'全批确认后再切透明底、导入或制作动画','jobs':jobs})
    print(json.dumps({'output':str(OUT),'total':13,'new':10,'approved':3}))

def bounds(image):
    a=np.array(image.convert('RGB')).astype(np.int16)
    bg=np.median(np.concatenate((a[:8].reshape(-1,3),a[-8:].reshape(-1,3),a[:,:8].reshape(-1,3),a[:,-8:].reshape(-1,3))),axis=0)
    mask=np.max(np.abs(a-bg),axis=2)>55
    yy,xx=np.where(mask)
    assert len(xx)>15000,'empty subject'
    return [int(xx.min()),int(yy.min()),int(xx.max())+1,int(yy.max())+1],len(xx)/mask.size

def package(complete=False):
    jobs=read(OUT/'jobs.json')['jobs']; records=[]
    for j in jobs:
        slug=j['slug']; promptfile=OUT/'prompts'/(slug+'.json')
        if not promptfile.exists(): continue
        raw=OUT/'raw'/(slug+'.png'); target=OUT/'characters'/(slug+'.png')
        if not j['userApproved']:
            prompt=read(promptfile); source=Path(prompt['generatedPath'])
            if source.resolve()!=raw.resolve(): shutil.copy2(source,raw)
            prompt.setdefault('originalGeneratedPath',str(source));prompt['generatedPath']=str(raw)
            write(promptfile,prompt)
            with Image.open(raw) as native:
                image=ImageOps.contain(native.convert('RGB'),(1600,1600),Image.Resampling.LANCZOS)
                canvas=Image.new('RGB',(1600,1600),BG)
                canvas.paste(image,((1600-image.width)//2,(1600-image.height)//2))
                canvas.save(target)
        assert sha(ROOT/j['identityReference'])==j['identitySourceSha256']
        if j['userApproved']: assert sha(target)==j['approvedSha256']
        with Image.open(target) as im:
            assert im.size==(1600,1600)
            box,area=bounds(im)
        with Image.open(raw) as im: native_size=list(im.size)
        assert min(box[0],box[1],1600-box[2],1600-box[3])>=12,(slug,box)
        records.append({**{k:j[k] for k in ['slug','name','id','identityReference','identitySourceSha256','facing','view','userApproved']},'file':f'characters/{slug}.png','sha256':sha(target),'size':[1600,1600],'raw':f'raw/{slug}.png','rawSha256':sha(raw),'nativeSize':native_size,'resampledReviewCanvas':native_size!=[1600,1600],'foregroundBounds':box,'foregroundFraction':round(area,4),'background':'warm-ivory-review','imported':False})
    if complete:
        assert len(records)==13 and len({r['id'] for r in records})==13
        assert len({r['rawSha256'] for r in records})==13
    write(OUT/'manifest.json',{'count':len(records),'expected':13,'complete':len(records)==13,'phase':'awaiting-full-set-visual-approval','tool':'built-in image_gen','nativeResolutionNote':'原始生成分辨率逐张记录；1600×1600为统一审阅画布','imported':False,'records':records})
    plan=read(OUT/'jobs.json')
    present={r['slug'] for r in records}
    for j in plan['jobs']:
        if j['slug'] in present and not j['userApproved']:j['status']='generated-awaiting-user-review'
    write(OUT/'jobs.json',plan)
    sheets(records)
    if complete:
        (OUT/'README.md').write_text('''# 项目原角色设计 × 宝石 icon 画风：13名角色

用户已确认主角、弓手、幽白的三人样板，本包原样保留三张批准图片，新增其余10名角色的独立重绘。
全部以项目 safe_frame_1600 的原设计为身份依据，保留服装、发型、道具、体型和动作特征，统一3/4朝左。

- characters：13张1600×1600米白背景全身审阅PNG；并非透明贴图。
- raw：原始生成PNG，原生尺寸见manifest；审阅稿按比例缩放至1600画布。
- review：全角色总览、职业/剧情角色分组大图、原设计对照。
- prompts：每个角色的图像工具提示词和来源。
- jobs.json / manifest.json：名单、身份源哈希、图像哈希、尺寸及批准状态。

使用内置image_gen逐名绘制。主角、弓手、幽白已获样板批准；新增10名角色待确认。
这是离线美术审阅包，尚未切透明底、导入UE、替换运行贴图或制作动画。原项目角色源图未修改。
''',encoding='utf-8')
        with zipfile.ZipFile(OUT/'GameXXK_全13角色_宝石画风审阅包.zip','w',zipfile.ZIP_DEFLATED) as z:
            for folder in ['characters','raw','prompts','review']:
                for p in sorted((OUT/folder).glob('*')):
                    if p.is_file():z.write(p,p.relative_to(OUT))
            for f in ['README.md','jobs.json','manifest.json','visual-review.json']:
                if not (OUT/f).exists():continue
                z.write(OUT/f,f)
        with zipfile.ZipFile(OUT/'GameXXK_全13角色_宝石画风审阅包.zip') as z: assert z.testzip() is None
    checks=['source hashes','approved image hashes','size','nonempty','safe bounds']
    if complete:checks+=['unique images','ZIP integrity']
    print(json.dumps({'count':len(records),'complete':len(records)==13,'approvedReused':sum(r['userApproved'] for r in records),'checks':checks},ensure_ascii=False))

def sheets(records):
    font=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',30)
    small=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',20)
    title=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',36)
    sets=[('all-characters.png',records,5,400,'全13名角色 · 项目原设计 × 宝石 icon 画风'),('roles.png',[r for r in records if r['id'].startswith(('Role.','Character.'))],4,500,'主角与六种职业 · 宝石 icon 画风'),('story-characters.png',[r for r in records if r['id'].startswith('Npc.')],3,600,'六名剧情角色 · 宝石 icon 画风')]
    for filename,rs,columns,cell,label in sets:
        rows=(len(rs)+columns-1)//columns
        sheet=Image.new('RGB',(columns*cell+40,rows*(cell+68)+126),BG)
        d=ImageDraw.Draw(sheet);d.text((24,20),label,font=title,fill=(35,46,41))
        d.text((26,72),'保留原服装、发型、体型与道具 · 3/4朝左 · 新增角色待确认',font=small,fill=(82,89,75))
        for i,r in enumerate(rs):
            x=20+(i%columns)*cell;y=116+(i//columns)*(cell+68)
            im=Image.open(OUT/r['file']).convert('RGB').resize((cell,cell),Image.Resampling.LANCZOS)
            sheet.paste(im,(x,y))
            d.text((x+cell//2,y+cell+22),r['name'],anchor='mm',font=font,fill=(35,46,41))
            if r['userApproved']:d.text((x+cell//2,y+cell+53),'已确认样板',anchor='mm',font=small,fill=(99,116,84))
        sheet.save(OUT/'review'/filename)
    for pageno in range((len(records)+3)//4):
        rs=records[pageno*4:pageno*4+4]
        sheet=Image.new('RGB',(1760,110+len(rs)*440),BG);d=ImageDraw.Draw(sheet)
        d.text((24,18),'原设计对照 · 原图 / 重绘',font=title,fill=(35,46,41))
        d.text((25,70),'核对服装、道具、比例与各自特征',font=small,fill=(82,89,75))
        for i,r in enumerate(rs):
            y=104+i*440
            for x,path in [(220,ROOT/r['identityReference']),(820,OUT/r['file'])]:
                im=Image.open(path).convert('RGB').resize((420,420),Image.Resampling.LANCZOS)
                sheet.paste(im,(x,y))
            d.text((32,y+190),r['name'],font=font,fill=(35,46,41))
            d.text((1290,y+180),'保留项目设计',font=font,fill=(35,46,41))
            d.text((1290,y+228),'块面、线条与用色优化',font=small,fill=(82,89,75))
        sheet.save(OUT/'review'/f'design-check-{pageno+1}.png')

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--init',action='store_true');parser.add_argument('--complete',action='store_true');args=parser.parse_args()
    if args.init:init()
    else:package(args.complete)
