"""Isolated six-unit / twelve-clip Dreamina animation pilot. No UE asset writes."""
import argparse
import json
import shutil
import subprocess
from datetime import datetime,timezone
from pathlib import Path

from PIL import Image,ImageOps,ImageDraw,ImageFont
from compose_gemstyle_current_layout import matte
from package_project_design_characters import sha

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'SourceAssets/AnimationProduction/gemstyle-pilot-20260910'
CLI=ROOT/'Saved/Tools/JimengCLI/dreamina.exe'
UNITS=[
 ('hero','主角','left','SourceArt/Characters/project-design-gemstyle-all-20260910/characters/hero.png','小幅胸口呼吸，肩膀仅轻微上下，发髻系带、短衣摆和竹叶末端慢慢摆动，竹篓保持刚性，脚掌不动。','双脚钉在原位置，空着的手收拳后朝画面左前方打出一次短直拳，手臂伸出但不夸张，然后收回原姿势；另一手继续握背篓肩带，不生成兵器。'),
 ('hunter','弓手','left','SourceArt/Characters/project-design-gemstyle-all-20260910/characters/hunter.png','胸口轻微呼吸，短披肩的两角和头发末端轻轻摆动，双手保持原低位持木弓姿势，弓身和弓弦稳定，脚不动。','脚固定，沿原低位持弓方向向画面左轻轻抬弓，拉弦手小幅后拉，放出一次箭，再回到低位持弓原姿势。弓弦始终只有一根，连上下弓梢，拉弦点与箭尾一致；弓不能变形、不多弦，不转向观众。'),
 ('you_bai','幽白','left','SourceArt/Characters/project-design-gemstyle-all-20260910/characters/you_bai.png','整体仅上下缓浮约画面高度1%，顶部焰尖与下方卷尾缓缓摆动，两颗小火滴跟随小幅浮动；保持脸、火焰轮廓和蓝紫配色，不喷射、不大幅涨缩。','脸朝左，整团火焰短暂向后收拢一点，再朝画面左前方作一次很短的撞击探头动作，随后回到原位置与原轮廓。位移不超过画面宽度5%，不生成手脚、兵器或大火球。'),
 ('rooster','公鸡','right','SourceArt/Monsters/project-design-gemstyle-20260910/monsters/rooster.png','双脚钉在原位置，胸部小幅呼吸，冠尖与三片长尾羽末端稍微来回晃动，身体不跳、不走，翅膀收拢。','两脚固定，身体稍向后蓄力，头向画面右前方啄击一次；一侧翅膀顺势小幅抬起并向前挥一下帮助平衡，然后头和翅膀都回到原姿势。啄击和挥翅是同一次攻击，不能连打或旋转身体。'),
 ('goat','山羊','right','SourceArt/Monsters/project-design-gemstyle-20260910/monsters/goat.png','保持两足直立的胖山羊原姿势，圆肚子轻微呼吸，胡须和短尾轻轻摆动，两根角稳定，两只脚不动，表情保持半眯不屑。','两脚固定，头稍向后收，再低头朝画面右前方短促顶角一次，然后收回原姿势；一对角与头作为同一个刚体运动。身体位移很小，不跑跳，不转成四足动物，不多生肢体。'),
 ('moneyrat','金钱鼠','right','SourceArt/Monsters/project-design-gemstyle-20260910/monsters/moneyrat.png','Boss原地轻微呼吸，胡须、腰带末端和尾尖微动，背后钱串仅轻摆2至3度，胸前大铜钱保持硬挺并被双手握住，两脚不动。','两只脚固定，双手继续握住胸前大铜钱，将铜钱朝画面右前方短促推出一次作盾击，再收回原位置。身体只略前倾，铜钱不旋转不融化不离手，背后钱架保持完整，不抛撒钱币、不连续攻击。'),
]

def write(path,data):
    path.parent.mkdir(parents=True,exist_ok=True)
    temp=path.with_suffix(path.suffix+'.tmp');temp.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8');temp.replace(path)

def read(path):return json.loads(path.read_text(encoding='utf-8-sig'))
def now():return datetime.now(timezone.utc).isoformat()

def prepare():
    if (OUT/'manifest.json').exists():
        print({'output':str(OUT),'status':'existing pilot retained; prompts, ledger and pause flag preserved'})
        return
    for directory in ['inputs','cutouts','prompts','tasks','raw','review']:(OUT/directory).mkdir(parents=True,exist_ok=True)
    jobs=[];sources=[]
    for slug,name,facing,relative,idle,attack in UNITS:
        original=ROOT/relative
        clean,holes=matte(original)
        box=clean.getchannel('A').point(lambda a:255 if a>=100 else 0).getbbox();assert box
        body=ImageOps.contain(clean.crop(box),(770,780),Image.Resampling.LANCZOS)
        canvas=Image.new('RGBA',(1024,1024),(0,0,0,0));x=(1024-body.width)//2;y=(1024-body.height)//2 if slug=='you_bai' else 884-body.height
        canvas.alpha_composite(body,(x,y));canvas.save(OUT/'cutouts'/(slug+'.png'))
        chroma=Image.new('RGB',(1024,1024),(255,0,255));chroma.paste(canvas,(0,0),canvas);chroma.save(OUT/'inputs'/(slug+'.png'))
        sources.append({'slug':slug,'name':name,'source':relative,'sourceSha256':sha(original),'input':f'inputs/{slug}.png','inputSha256':sha(OUT/'inputs'/(slug+'.png')),'size':[1024,1024],'subjectBounds':list(canvas.getchannel('A').getbbox()),'enclosedGapsRemoved':holes,'method':'existing approved matte + fixed scale/placement; original artwork untouched'})
        for action,motion in [('idle',idle),('attack',attack)]:
            timing=('只做一个缓慢微小的待机循环。0秒与4秒严格回到参考图同一姿态，中间自然呼吸；变化很小但不是静止图片。' if action=='idle' else '只做一次攻击：0至0.5秒保持初始姿态，0.5至1秒小幅蓄力，1至1.5秒一次清楚短促攻击，1.5至2.4秒收回，2.4至4秒稳定回到与参考图完全相同的待机姿态。禁止重复攻击。')
            prompt=f'这是2D游戏单体精灵帧动画素材，固定正交相机与固定画幅。只让参考图中的{name}动起来，不改变设计、体型、脸、服饰、道具、宝石icon手绘块面和深描边。整个身体保持3/4朝画面{("左" if facing=="left" else "右")}，不转身不朝向观众。{timing}\n动作：{motion}\n强制保持背景为纯平洋红#FF00FF，全程没有光影变化、地面、投影、文字、粒子、场景或其他生物。画面不推拉、不移动、不旋转、不抖动。所有肢体、角、尾和道具全程完整在画幅内，大小固定，脚底与身体基准位置固定；变化来自关节和小幅末端摆动，不能整张图片缩放、漂移或变形。首尾同一张图是动作循环的起止锚点，动态后必须归位。'
            jid=f'{slug}_{action}';(OUT/'prompts'/(jid+'.txt')).write_text(prompt,encoding='utf-8')
            jobs.append({'id':jid,'slug':slug,'name':name,'facing':facing,'action':action,'prompt':f'prompts/{jid}.txt','input':f'inputs/{slug}.png','duration':4,'resolution':'720p','model':'CLI default (seedance2.0_vip per current help)','maxSubmissions':1})
    write(OUT/'manifest.json',{'scope':'3 characters + 3 monsters; one idle and one attack each','jobCount':12,'unitCount':6,'tool':'Dreamina official Windows CLI 1.4.18','creditBefore':4079,'sourcePreparation':'deterministic chroma-key and safety framing; no art redesign','firstLastPolicy':'identical input images','phase':'two-job pilot before remaining ten','imported':False,'sources':sources,'jobs':jobs})
    review=Image.new('RGB',(1800,690),(239,234,217));d=ImageDraw.Draw(review);font=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',27)
    for i,s in enumerate(sources):
        im=Image.open(OUT/'cutouts'/(s['slug']+'.png')).resize((300,300),Image.Resampling.LANCZOS)
        review.paste((91,113,117),(i*300,40,(i+1)*300,340));review.paste(im,(i*300,40),im)
        review.paste((255,0,255),(i*300,350,(i+1)*300,650));review.paste(im,(i*300,350),im)
        d.text((i*300+150,20),s['name'],font=font,fill=(35,46,41),anchor='mm')
    review.save(OUT/'review'/'six-inputs.png');print({'prepared':12,'units':6,'output':str(OUT)})

def cli(arguments,log):
    result=subprocess.run([str(CLI),*arguments],cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=180,check=False)
    write(log,{'arguments':arguments,'exitCode':result.returncode,'stdout':result.stdout,'stderr':result.stderr,'at':now()})
    start=result.stdout.find('{');end=result.stdout.rfind('}')
    if start<0 or end<start:raise RuntimeError((result.stderr or result.stdout)[-1600:])
    parsed=json.loads(result.stdout[start:end+1])
    if result.returncode!=0:raise RuntimeError(json.dumps(parsed,ensure_ascii=False))
    return parsed

def submit(jid):
    m=read(OUT/'manifest.json');j=next(j for j in m['jobs'] if j['id']==jid)
    if m.get('submissionsPaused'):raise RuntimeError('Generation paused by user; explicit resume is required before new submissions')
    statepath=OUT/'tasks'/(jid+'.json')
    if statepath.exists():raise RuntimeError('Already attempted: '+jid+'; query existing task instead of resubmitting')
    state={'id':jid,'status':'attempting','submissionCount':1,'at':now()};write(statepath,state)
    args=['frames2video',f'--first={OUT/j["input"]}',f'--last={OUT/j["input"]}',f'--prompt={(OUT/j["prompt"]).read_text(encoding="utf-8")}',f'--video_resolution={j["resolution"]}',f'--duration={j["duration"]}','--poll=0']
    try:
        r=cli(args,OUT/'tasks'/(jid+'.submit-log.json'))
        state.update(result=r,status=r.get('gen_status','unknown'),submit_id=r.get('submit_id'),credit_count=r.get('credit_count'))
        if not state['submit_id']:raise RuntimeError(json.dumps(r,ensure_ascii=False))
    except Exception as e:
        state.update(status='submission-error-review-required',error=str(e));write(statepath,state);raise
    write(statepath,state);print(json.dumps({'id':jid,'status':state['status'],'submit_id':state['submit_id'],'credit_count':state['credit_count']},ensure_ascii=False))

def query(jid):
    path=OUT/'tasks'/(jid+'.json');state=read(path)
    if not state.get('submit_id'):raise RuntimeError('No submit_id: '+jid)
    directory=OUT/'raw'/jid;directory.mkdir(parents=True,exist_ok=True)
    r=cli(['query_result',f'--submit_id={state["submit_id"]}',f'--download_dir={directory}'],OUT/'tasks'/(jid+'.query-log.json'))
    state.update(status=r.get('gen_status','unknown'),queryResult=r,queriedAt=now());write(path,state)
    files=[str(p.relative_to(OUT)) for p in directory.glob('*') if p.is_file()]
    print(json.dumps({'id':jid,'status':state['status'],'failure':r.get('fail_reason'),'files':files,'resultKeys':list(r)},ensure_ascii=False))

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('command',choices=['prepare','submit','query']);p.add_argument('id',nargs='?');a=p.parse_args()
    if a.command=='prepare':prepare()
    elif a.command=='submit':submit(a.id)
    else:query(a.id)
