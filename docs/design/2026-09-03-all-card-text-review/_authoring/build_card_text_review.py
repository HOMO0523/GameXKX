"""Build the review artifact only; never writes runtime assets or gameplay code."""
from pathlib import Path
from fractions import Fraction
import re, json, html, math, runpy

HERE = Path(__file__).resolve().parent
ROOT = next(p for p in HERE.parents if (p/'Source/GameXXK/Private/GameXXKCardCatalog.cpp').is_file())
OUT = ROOT/'docs/design/2026-09-03-all-card-text-review'
OUT.mkdir(parents=True, exist_ok=True)
BASE = {c['id']:c for c in json.loads((HERE/'baseline-metadata.json').read_text(encoding='utf-8'))}
Q = [Fraction(1),Fraction(6,5),Fraction(7,5)]
QN = ['普通','稀有','史诗']
QUALITY_SOURCE = (ROOT/'Source/GameXXK/Private/GameXXKCardQualityRules.cpp').read_text(encoding='utf-8-sig')
for tier, name in [(1,'Rare'),(2,'Epic')]:
    body=QUALITY_SOURCE.split('Get'+name+'CardIds()')[1].split('return Ids;')[0]
    for ident in re.findall(r'TEXT\("([^"]+)"\)',body):
        BASE[ident]['quality']=QN[tier]

CARDS=[]
def ceil(x): return math.ceil(Fraction(x))
def fmt(x):
    x=Fraction(x)
    return str(x.numerator) if x.denominator==1 else f'{float(x):.2f}'.rstrip('0').rstrip('.')
def token(text,q,native):
    def repl(m):
        k,*a=m.group(1).split(':')
        if k=='v': return a[0].split('/')[q]
        if k=='q': return fmt(Q[q])
        if k=='m': return fmt(500*Q[q])
        if k=='bonus': return '' if q==0 else f'，品质加成{20*q}%'
        if k=='hquality':
            return ''
        if k in ('a','p','g'): return str(ceil(Fraction(a[0])*Q[q])+ (int(a[1]) if len(a)>1 else 0))
        if k in ('d','i','o'): return str(ceil(Fraction(a[0])*Q[q]*5))
        if k=='h':
            med=Fraction(a[2]) if len(a)>2 else Fraction(0)
            return str(ceil((Fraction(a[0])+med)*Q[q]*5))
        raise ValueError('Unknown token '+m.group(1))
    return re.sub(r'\{([^{}]+)\}',repl,text)

def add(ident,compact,detail=None,*,cost=None,target=None,note='',branches=None,status='已核对'):
    b=BASE[ident]
    native=QN.index(b['quality'])
    variants=[]
    for q in range(native,3):
        ec=cost if cost is not None else b['cost'].replace(' 气 / ','气／').replace(' 内','内')
        cost_text=token(ec,q,native)
        if '.Healer.' in ident:
            cost_text+='；首次开启药方另加1气'
        variants.append(dict(quality=QN[q],cost=cost_text,compact=token(compact,q,native),detail=token(detail or compact,q,native)))
    group='主角' if ident.startswith('Hero.') else ('伙伴·'+{'Blade':'刀客','Guard':'守卫','Healer':'药师','Hunter':'弓手','Sorcerer':'法师','FormationMaster':'阵师'}[ident.split('.')[1]] if ident.startswith('Profession.') else ('任务NPC' if ident.startswith('Npc.') else 'Boss牌'))
    if any(c['id']==ident for c in CARDS): raise ValueError('Duplicate '+ident)
    CARDS.append(dict(id=ident,name=b['name'],group=group,base_quality=b['quality'],target=target or b['target'],status=status,note=note,variants=variants,branches=branches or []))

def attack(c,target='',hits=1,extra=0):
    num='{a:'+str(c)+(f':{extra}' if extra else '')+'}'
    pct='{p:'+str(c)+'}'
    if hits==1: return (f'{target}造成{num}点伤害。',f'{target}造成{pct}%的攻击伤害。')
    return (f'{target}攻击{hits}次，每次{num}点伤害。',f'{target}攻击{hits}次，每次造成{pct}%的攻击伤害。')
def armor(c,who='获得'):
    return (f'{who}{{g:{c}}}点〔护甲〕。',f'{who}相当于{c}%防御的〔护甲〕{{bonus}}。')
def dot(c,status,who='施加'):
    return (f'{who}{{d:{c}}}点〔{status}〕。',f'{who}基础值{c}的〔{status}〕{{bonus}}，等级倍率5倍。')
def heal(c,ref='n',med=0,who='恢复'):
    return (f'{who}{{h:{c}:{ref}:{med}}}点生命，消耗全部〔药效〕。',f'{who}生命，治疗系数{c}{{hquality:{ref}}}；消耗全部〔药效〕，每点增加{{q}}系数；等级倍率5倍。')
def join(*parts): return ('\n'.join(p[0] if isinstance(p,tuple) else p for p in parts),'\n'.join(p[1] if isinstance(p,tuple) else p for p in parts))
def write(ident,*parts,**kw):
    c,d=join(*parts);add(ident,c,d,**kw)

def hero():
    p='Hero.Generic.'
    write(p+'QingFengYiShi',attack(100),'下一张主动牌气力消耗−1。')
    write(p+'HeYuZhan',attack(160),'触发目标数值最高的〔流血〕、〔中毒〕或〔灼烧〕1次。')
    write(p+'FengShenBu','目标获得2层〔灵动〕；抽{v:2/3/4}张，弃1张。\n〔消耗〕')
    write(p+'SuiYanJi',attack(150),'施加3层〔破绽〕、1层〔标记〕。')
    write(p+'GuiYuanShu',heal(15,0),'清除目标全部〔流血〕、〔中毒〕和〔灼烧〕。\n目标的下一张主动牌气力消耗−1。')
    write(p+'HengJianShouShi',armor(80,'目标获得'),'目标获得2层〔标记〕、1次〔格挡〕。')
    write(p+'NingShenTuNa','获得2层〔气势〕，回复10点〔内力〕。\n〔消耗〕')
    write(p+'GuanXi','抽3张，弃1张。\n〔消耗〕')
    write(p+'PoYunYiShan',attack(160),('有〔灵动〕时，消耗1层，追加{a:100}点伤害并抽1张。','有〔灵动〕时，消耗1层，追加{p:100}%的攻击伤害并抽1张。'))
    write(p+'XingQiHuiHuan','抽{v:2/3/4}张，回复1点气力。\n〔消耗〕')
    write(p+'JianYiGuanHong',attack(260),('消耗全部〔气势〕，每层使本段伤害增加20点；消耗至少3层时回复1点气力。','消耗全部〔气势〕，每层使本段攻击倍率增加20个百分点；消耗至少3层时回复1点气力。'),note='示例气势0。气势本身的战斗结算加伤归目标预览，不混入卡牌产出；消耗加成不再乘品质。')
    write(p+'GuiYuanFanZhao',heal(15,0,who='全体友方各恢复'),armor(50,'全体友方各获得'),'清除全体友方全部〔流血〕、〔中毒〕和〔灼烧〕；抽2张。')
    p='Hero.Blade.'
    write(p+'TongFengYinShi','抽1张；目标获得{v:2/3/4}层〔气势〕。\n〔冲锋〕下一张主动牌的基础效果重放1次。\n〔收招〕下回合首张主动牌结算后，重放本牌基础效果。',cost='1气／0内')
    write(p+'XueLuXiangCheng',attack(150),dot(6,'流血'),'〔冲锋〕下一次主动攻击流血目标时，额外触发1次〔流血〕。\n〔收招〕下回合首次主动攻击流血目标时，抽2张并回复1点气力。')
    write(p+'YingFengHuanBu','获得2层〔标记〕、2层〔灵动〕、1次〔反击〕。\n〔冲锋〕下一张主动牌结算前，获得2层〔灵动〕、1次〔反击〕。\n〔收招〕下回合开始时，获得2层〔标记〕、1次〔反击〕。')
    write(p+'TongPaoJuShi','目标获得2层〔气势〕。\n其下次主动攻击消耗全部〔气势〕，每层增加10点伤害。\n〔冲锋〕下一张主动牌结算前，获得2层〔气势〕。\n〔收招〕目标下回合的下一张主动牌气力消耗−1。',note='对应选定友方的增伤和减费，不转给卡牌持有者。')
    CARDS[-1]['variants']=[dict(v,detail=v['detail'].replace('每层增加10点伤害','每层使攻击倍率增加10个百分点')) for v in CARDS[-1]['variants']]
    p='Hero.Guard.'
    write(p+'TieBiTongShou',armor(80,'目标获得'),'目标获得2次〔格挡〕。')
    write(p+'JieJiaHuanFeng',('造成{a:100}点伤害，另加当前〔护甲〕等量伤害，护甲保留。','造成{p:100}%的攻击伤害，另加当前〔护甲〕等量伤害，不消耗护甲。'),armor(40,'该友方再获得'),'该友方获得1次〔格挡〕。',note='内部取值使用护甲最高友方；不在攻击详述追加出手对象。')
    write(p+'LieZhenChengFeng',armor(140,'全体友方各获得'),'全体友方各获得1次〔格挡〕。')
    write(p+'XuanJiaZhenYue',('消耗目标全部〔护甲〕，对全体敌人造成{a:200}点伤害；每消耗1护甲增加1点伤害。','消耗目标全部〔护甲〕，对全体敌人造成{p:200}%的攻击伤害；每消耗1护甲，攻击倍率增加1个百分点。'))
    p='Hero.Healer.'
    write(p+'YiXueCuiFang','全体友方各失去1点生命，最低保留1点；每实际扣血1人，获得{v:2/3/4}点〔药效〕；抽1张。\n〔药方〕每名友方每回合首次实际失去生命时，获得1点药效，每回合最多3点。',note='首次开启药方额外支付1气；三名友方实际扣血时本牌基础获得6/9/12药效。')
    write(p+'HuiChunNiMai',('友方恢复{h:25:0}生命；敌人受到{h:25:0}点反转伤害。消耗全部〔药效〕。','对友方治疗，对敌人造成〔治疗反转〕；治疗系数25{hquality:0}，每点药效增加{q}系数，等级倍率5倍，消耗全部药效。'),'友方目标清除全部〔流血〕、〔中毒〕、〔灼烧〕和〔蚀伤〕。\n〔药方〕每回合首次治疗或反转消耗至少6点药效时，抽1张。')
    write(p+'DuHuoTongLu',attack(130),dot(6,'中毒'),dot(2,'灼烧'),'触发1次〔毒爆〕，获得6点〔药效〕。\n〔药方〕一次毒爆触发至少2种持续伤害时，获得2点药效，每回合最多2次。')
    write(p+'BaiCaoJiZhen',heal(20,0,who='全体友方各恢复'),dot(1,'中毒','全体敌人各获得'),dot(1,'灼烧','全体敌人各获得'),'〔药方〕每回合首次在一次行动中有效治疗至少2名友方时，回复1点气力。')
    p='Hero.Hunter.'
    write(p+'FengYanDingXian','抽2张，弃1张；获得2层〔灵动〕、3点〔蓄力〕。',cost='0气／{v:3/2/1}内')
    write(p+'LieYuLianShi',attack(140),dot(8,'流血'),('〔重箭〕每消耗1点蓄力，追加1次{a:50}点伤害。','〔重箭〕每消耗1点蓄力，追加1次{p:50}%的攻击伤害。'))
    write(p+'CuiDuChuanXin',attack(130),dot(6,'中毒'),'触发1次〔毒爆〕。\n〔重箭〕每消耗1点蓄力，再触发1次毒爆。')
    write(p+'HuiFengGuanRi',attack(150),('〔重箭〕每消耗1点蓄力，本段伤害增加{a:40}点并抽1张；消耗至少3点时回复1点气力。','〔重箭〕每消耗1点蓄力，本段攻击倍率增加{p:40}个百分点并抽1张；消耗至少3点时回复1点气力。'))
    p='Hero.Mage.'
    write(p+'YanXuLiaoYuan',attack(100,'对全体敌人'),dot(4,'灼烧','全体敌人各获得'),'〔检索〕1张未完成的主角法师牌。',('〔阵赏〕全体敌人各获得{d:6}点〔灼烧〕，再触发灼烧1次。','〔阵赏〕全体敌人各获得6点〔灼烧〕，{m}%增幅倍率，再触发灼烧1次。'))
    write(p+'HanXuNingChuan',armor(40),('回复6点〔内力〕，本次溢出获得{o:6}点〔护甲〕。','回复6点〔内力〕；仅实际溢出部分按100%转甲{bonus}，等级倍率5倍。'),('〔阵赏〕消耗全部护甲，进行〔冰爆〕。','〔阵赏〕消耗全部护甲，对全体敌人造成{p:100}%的攻击伤害；每消耗1护甲，攻击倍率增加1个百分点。'),cost='1气／0内',note='示例主角内力30/30，回复6全部溢出；普通基础40护甲加30溢出护甲。阵赏在重放后重新读护甲，不预填未来消耗值。')
    write(p+'LeiXuYinTing','全体敌人各获得3层〔标记〕。',('每个敌人按标记数受到等次数攻击，每次{a:50}点伤害。','按每个敌人开始时的标记数攻击，每次造成{p:50}%的攻击伤害。'),'〔检索〕1张未完成的主角法师牌。',('〔阵赏〕全体敌人各获得3层标记，再按标记数攻击，每次{a:60}点伤害。','〔阵赏〕全体敌人各获得3层标记，再按各自标记数攻击，每次造成{p:60}%的攻击伤害。'))
    write(p+'GuiXuTongXuan','抽2张，弃1张；回复{v:0/2/4}点〔内力〕。\n〔阵赏〕抽2张，回复1点气力；下一张主角牌气力消耗−1。')
    p='Hero.Formation.'
    write(p+'GuanShiLuoZi',attack(80),'触发当前〔地势〕收益1次；抽1张。',cost='1气／3内')
    write(p+'YiZhenHuiXiang','触发当前〔地势〕收益1次；本回合实际换过地势时，改为2次。',target='无需选择')
    write(p+'LianYingBuShi','下一次〔地势〕收益改为触发{v:2/3/4}次，仅生效1次。',target='无需选择')
    write(p+'LiuHeGuiYi','依次触发平原、断崖、山林、水岸、村镇、洞窟的〔地势〕收益各1次。',target='无需选择')

if __name__=='__main__':
    hero()
    for fn in sorted(HERE.glob('card_text_review_part_*.py')):
        exec(compile(fn.read_text(encoding='utf-8'),str(fn),'exec'),globals())
    if '--partial' in __import__('sys').argv:
        print('Prepared',len(CARDS),'cards');raise SystemExit(0)
    exec(compile((HERE/'card_text_review_render.py').read_text(encoding='utf-8'),'review_render','exec'),globals())
