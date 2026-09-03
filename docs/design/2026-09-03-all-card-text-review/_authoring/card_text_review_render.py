from collections import Counter
from datetime import datetime
import hashlib, shutil

exec(compile((HERE/'card_text_review_shared.py').read_text(encoding='utf-8'),'review_shared','exec'),globals())
exec(compile((HERE/'card_text_review_multiplier.py').read_text(encoding='utf-8'),'review_multiplier','exec'),globals())
exec(compile((HERE/'card_text_review_targets.py').read_text(encoding='utf-8'),'review_targets','exec'),globals())
exec(compile((HERE/'card_text_review_pill_help.py').read_text(encoding='utf-8'),'review_pill_help','exec'),globals())
SPELL_COMMON='〔法术任务〕所需不同牌各主动使用1次，依序免费重放，再领取首牌阵赏；每个玩家回合最多完成1次。'
FORMULA_COMMON='〔药方〕首次打出额外消耗1气，开启本场持续效果；不同角色分别记录，药方效果不会触发任何药方。'
UNIVERSALS={'YanMuHuTi','LieFu','XingHuoHuiShou','ChiYanFengJie'}
PILL_NOUNS=sorted({n for n,k,t,e in GLOSSARY if k in ('状态','资源','持续伤害','地势')}|{'毒爆','冰爆','治疗反转','无视防御'},key=len,reverse=True)
PILL_RE=re.compile('|'.join(map(re.escape,PILL_NOUNS)))
def normalize_pills(text):
    for name in ('气力','内力'):text=text.replace('〔'+name+'〕',name)
    return ''.join(p if p.startswith('〔') else PILL_RE.sub(lambda m:'〔'+m.group(0)+'〕',p) for p in re.split(r'(〔[^〕]+〕)',text))
for card in CARDS:
    ident=card['id']
    card.update(describe_card_target(card))
    common=[]
    required=4 if ident.startswith('Hero.Mage.') else (5 if ident.startswith('Profession.Sorcerer.') else (3 if ident.startswith(('Npc.YueBai.','Npc.SongJinBao.')) else 0))
    if required: common.append(SPELL_COMMON.replace('所需不同牌',f'{required}张不同牌'))
    if '.Healer.' in ident: common.append(FORMULA_COMMON)
    if ident.startswith('Profession.Sorcerer.') and ident.split('.')[-1] in UNIVERSALS:
        common.append('〔通用〕第二张记录牌决定阵赏分支。')
    card['common_rules']=[normalize_pills(t) for t in common]
    for v in card['variants']:
        v['target_heading']=visible_target_heading(card)
        for key in ('compact','detail'):
            t=v[key].replace('按第二张记录牌确定分支，见下方分支表。','第二张记录牌决定普通、火、冰或雷阵赏。')
            t=t.replace('；回复0点〔内力〕','').replace('自身回复0点内力。','')
            t=t.replace('，品质加成0%，','，')
            if ident.startswith('Profession.Sorcerer.') and ident.split('.')[-1] in UNIVERSALS:
                t+='\n'+('〔自动入手〕首牌使其余任务牌入手；非首牌随任务开启入手。每张每场1次。' if key=='compact' else
                    '〔自动入手〕作为首牌时，其余未完成的携带法师牌从抽牌堆或弃牌堆入手；作为非首牌时，任务开启后本牌从抽牌堆或弃牌堆入手。每张每场限1次。')
            if key=='detail' and not ident.startswith('Npc.') and '〔重箭〕' in t:
                t=t.replace('〔重箭〕','〔重箭〕消耗打出前的全部蓄力；',1)
            if key=='detail':
                word_q=0 if ident in {'Profession.FormationMaster.'+s[0] for s in switches} else QN.index(v['quality'])
                t=multiplier_copy(t,word_q,QN.index(card['base_quality']))
            v[key]=target_body_note(card)+normalize_pills(standardize_recipients(t.strip()))
    for b in card['branches']:
        b.update(describe_reward_target(card,b))
        b['target_heading']=visible_target_heading(b)
        for key in ('compact','detail'):
            t=b[key]
            if key=='detail': t=multiplier_copy(t,QN.index(b['quality']),QN.index(card['base_quality']))
            b[key]=normalize_pills(standardize_recipients(t))
    text_all='\n'.join([v[k] for v in card['variants'] for k in ('compact','detail')]+common+[b[k] for b in card['branches'] for k in ('compact','detail')])
    card['pills']=list(dict.fromkeys(re.findall(r'〔([^〕]+)〕',text_all)))
    card['source_rule']='按既有卡牌效果数据取值，攻击句不追加出手对象。'
    special={
        'Hero.Guard.JieJiaHuanFeng':'攻击和所加伤害护甲取护甲最高友方；生成护甲取主角防御。',
        'Hero.Guard.XuanJiaZhenYue':'攻击和被消耗护甲取所选友方。',
        'Npc.TusiChief.ZhaiZhuHaoLing':'攻击取攻击最高友方；生成护甲取土司防御。',
        'Npc.TusiChief.TuSiJunLing':'攻击取攻击最高友方；生成护甲取土司防御。',
        'Npc.TusiChief.MengZhaiShiYue':'逐名存活友方使用各自攻击，生成护甲取土司防御。',
        'Npc.SongJinBao.ShangQianGuWu':'攻击取所选友方，敌方目标按优先规则自动选择。',
        'Npc.SongJinBao.ErMuMiBao':'阵赏逐名存活友方使用各自攻击。',
        'Npc.JinGui.ShiJingErMu':'蓄力和重箭攻击取攻击最高友方。',
        'Npc.JinGui.ZaYiChouBei':'蓄力、重箭攻击和重箭回复内力均属于攻击最高友方。',
        'Npc.QiongMeiEr.TengQiaoFeiDu':'蓄力和重箭攻击取攻击最高友方。',
    }
    card['source_rule']=special.get(ident,card['source_rule'])
    card['implementation_note']='此处为目标文案，不能据此判断卡牌运行时已完成。'

prepare_pill_help()
write_pill_help_table()
ids=[c['id'] for c in CARDS]
assert len(ids)==173 and len(set(ids))==173
assert set(ids)==set(BASE), (set(BASE)-set(ids),set(ids)-set(BASE))
for ident in ids:
    assert ('TEXT("'+ident+'")') in (ROOT/'Source/GameXXK/Private/GameXXKCardCatalog.cpp').read_text(encoding='utf-8-sig')
expected={'主角':36,'伙伴·刀客':18,'伙伴·守卫':18,'伙伴·药师':18,'伙伴·弓手':18,'伙伴·法师':18,'伙伴·阵师':18,'任务NPC':24,'Boss牌':5}
assert dict(Counter(c['group'] for c in CARDS))==expected
known={g[0] for g in GLOSSARY}
for c in CARDS:
    assert c['target_description']
    assert set(c['target_labels'])<=set(TARGET_LABELS)
    assert len(c['variants'])==3-QN.index(c['base_quality'])
    assert set(c['pills'])<=known, (c['name'],set(c['pills'])-known)
    assert not set(c['pills'])&{'气力','内力'}
    for v in c['variants']+c['branches']:
        assert v['target_heading'] and not any(x in v['target_heading'] for x in ('：','。','；','仅自身'))
        names=[item['name'] for item in v['pill_descriptions']]
        assert len(names)==len(set(names))
        members=[name for item in v['pill_descriptions'] for name in item['members']]
        assert len(members)==len(set(members))
        assert set(members)<=set(c['pills'])
        assert not set(members)&(set(TARGET_LABELS)|{'气力','内力'})
        assert v['pill_shared_note']==shared_note(v['pill_descriptions'])
        if {'蓄力','重箭'}<=set(members):
            assert sum(set(item['members'])=={'蓄力','重箭'} for item in v['pill_descriptions'])==1
        for k in ('compact','detail'):
            assert v[k].strip(),(c['id'],k)
            assert not re.search(r'\{[^{}]*\}',v[k]),(c['id'],v[k])
            assert not re.search(r'(?:主角|法师|角色|当前)攻击[：:]|出手对象[：:]|取值对象[：:]|×|\*|=|层〔(?:流血|中毒|灼烧|蚀伤)〕',v[k]),(c['id'],v[k])
assert sum(len(c['variants']) for c in CARDS)==419
assert sum(len(c['branches']) for c in CARDS)==36
assert token('{h:25:1:0}',1,1)=='150'
assert token('{h:25:1:5}',1,1)=='180'
assert token('{h:25:1:5}',2,1)=='210'
assert [token('{d:6}',q,0) for q in range(3)]==['30','36','42']
assert [token('{o:4}',q,0) for q in range(3)]==['20','24','28']
assert [token('{g:80}',q,0) for q in range(3)]==['80','96','112']
assert token('{h:25:2:6}',2,2)=='217'
card_map={c['id']:c for c in CARDS}
assert card_map['Hero.Generic.QingFengYiShi']['variants'][1]['target_heading']=='单体敌方'
assert card_map['Hero.Generic.QingFengYiShi']['variants'][1]['compact'].startswith('造成120点伤害。')
assert card_map['Hero.Generic.QingFengYiShi']['variants'][1]['detail'].startswith('造成120%的攻击伤害。')
assert card_map['Profession.Healer.HuiChunLu']['target_labels']==['单体友方/敌方','全体友方','全体敌方']
assert card_map['Profession.Guard.TieBi']['target_description']=='对象：〔单体友方〕；仅自身。'
assert card_map['Hero.Guard.XuanJiaZhenYue']['target_labels']==['单体友方','全体敌方']
assert card_map['Profession.FormationMaster.GuanShi']['target_labels']==['全体敌方']
assert '每次102点伤害' in card_map['Profession.Blade.JiYuLianZhan']['variants'][1]['compact']
assert '首次开启药方另加1气' in card_map['Profession.Healer.HuiChunLu']['variants'][0]['cost']
assert not card_map['Hero.Generic.QingFengYiShi']['variants'][0]['pill_descriptions']
for suffix in UNIVERSALS:
    for variant in card_map['Profession.Sorcerer.'+suffix]['variants']:
        assert '〔自动入手〕' in variant['compact'] and '〔自动入手〕' in variant['detail']
for card in CARDS:
    if not card['id'].startswith('Npc.'):
        for variant in card['variants']:
            if '〔重箭〕' in variant['detail']:
                assert '消耗打出前的全部〔蓄力〕' in variant['detail'],(card['id'],variant['quality'])

meta=dict(status='完整文案审阅稿，待逐批确认；治疗公式同步更新，游戏tooltip尚未修改',card_count=173,quality_variant_count=419,universal_reward_variant_count=36,example=dict(attack=100,defense=100,team_max_level=100,party_living_count=3,armor=0,medicine=0,charge=0,momentum=0,enemy_dot=0,enemy_mark=0,hero_mana='30/30',sorcerer_mana='34/34'),sources=['Source/GameXXK/Private/GameXXKCardCatalog.cpp','Source/GameXXK/Private/GameXXKCardQualityRules.cpp','Source/GameXXK/Private/GameXXKCardRules.cpp','docs/superpowers/specs/2026-09-03-card-monster-progression-rebalance-design.md','docs/design/2026-09-03-sorcerer-card-quick-reference.md'],pending=[c['id'] for c in CARDS if c['status']!='文案待审'])
payload=dict(meta=meta,target_labels=list(TARGET_LABELS),cards=CARDS,glossary=[dict(name=a,kind=b,text=c,example=d) for a,b,c,d in GLOSSARY],aliases=ALIASES,state_texts=STATE_TEXTS)
meta['tooltip_interaction']=dict(default='compact',shift='hold_detail',right_button='click_toggle_pills',keep_pills_on_mouseup=True,pill_scope='current_card_quality_and_active_branch')
(OUT/'card-texts.json').write_text(json.dumps(payload,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')

def cell(x): return str(x).replace('|','\\|').replace('\n','<br>')
def card_md(c):
    z=[f'## {c["name"]}','',f'`{c["id"]}`','',f'原生品质：{c["base_quality"]}。状态：{c["status"]}。',f'目标入口：{c["target"]}。',f'Pill顺序：'+(' → '.join('〔'+x+'〕' for x in c['pills']) or '无'),'', '| 品质／费用 | 简述 | 详述 |','|---|---|---|']
    for v in c['variants']:
        heading='**'+v['target_heading']+'**<br>'
        z.append('| '+cell(v['quality']+'；'+v['cost'])+' | '+heading+cell(v['compact'])+' | '+heading+cell(v['detail'])+' |')
    if c['branches']:
        z+=['','阵赏分支文案：分支未定时详述列出四条；锁定后只展示所选分支。','', '| 品质／分支 | 简述 | 详述 |','|---|---|---|']
        for b in c['branches']:
            heading='**'+b['target_heading']+'**<br>'
            z.append('| '+b['quality']+'／'+b['branch']+' | '+heading+cell(b['compact'])+' | '+heading+cell(b['detail'])+' |')
    if c['common_rules']:
        z+=['','机制核对资料，不作为Tooltip正文：','']+[f'- {t}' for t in c['common_rules']]
    z+=['','[本牌各品质的右键pill说明](11-card-pill-descriptions.md#'+c['name']+')。']
    z+=['',f'取值核对，仅供评审：{c["source_rule"]}']
    if c['note']: z += [f'核对备注：{c["note"]}']
    return '\n'.join(z)+'\n'

files=[]
for n,(group,count) in enumerate(expected.items(),1):
    filename=f'{n:02d}-'+{'主角':'hero','伙伴·刀客':'blade','伙伴·守卫':'guard','伙伴·药师':'healer','伙伴·弓手':'hunter','伙伴·法师':'sorcerer','伙伴·阵师':'formation','任务NPC':'npc','Boss牌':'boss'}[group]+'.md'
    selection=[c for c in CARDS if c['group']==group]
    head=f'# {group}：{count}张完整文案\n\n[返回总表](README.md) · [Pill与状态说明](10-shared-tooltip-text.md)\n\n所有数字均为总表注明的统一示例。表内覆盖全部合法品质；未修改游戏代码。\n\n'
    (OUT/filename).write_text(head+'\n'.join(card_md(c) for c in selection),encoding='utf-8')
    files.append((group,count,filename,sum(len(c['variants']) for c in selection)))

shared=['# Pill、状态与交互提示完整文案','', '[返回总表](README.md)','', '这是待审文案。下表“示例／动态字段”不是固定写死的游戏数值；实际界面从当前状态取值。','', '## Pill与状态说明','', '| 名称 | 类别 | 说明文本 | 示例／动态字段 |','|---|---|---|---|']
shared+=['| '+' | '.join(cell(x) for x in g)+' |' for g in GLOSSARY]
shared+=['','## 术语统一建议','', '| 当前称谓 | 拟采用 | 影响 |','|---|---|---|']
shared+=['| '+' | '.join(cell(x) for x in g)+' |' for g in ALIASES]
shared+=['','## 交互、任务、药方、目标与阶段提示','', '| 场景 | 拟用文本 | 取值／显示条件 |','|---|---|---|']
shared+=['| '+' | '.join(cell(x) for x in g)+' |' for g in STATE_TEXTS]
shared+=['','普通卡牌详述不加入怪物防御、状态修正、护甲吸收或最终生命损失。这些规则可以在状态自身说明中解释，具体对象结算只在目标tooltip显示。','']
(OUT/'10-shared-tooltip-text.md').write_text('\n'.join(shared),encoding='utf-8')

readme=['# 全部卡牌描述文本审阅稿','', '**173张卡、419个合法品质版本、36条通用阵赏品质分支，以及全部共享pill和状态提示。** 当前是文案设计稿，游戏tooltip和pill样式尚未修改；本轮另按用户要求统一治疗计算。','', '[打开可搜索的文案阅读页](review.html) · [数据全文](card-texts.json) · [共享提示](10-shared-tooltip-text.md)','', '## 本次采用的表达','', '- 攻击简述写计算后的数字，例如“造成120点伤害。”；攻击详述写“造成120%的攻击伤害。”，不追加出手对象、当前攻击值或算式。', '- 取值角色保留在内部核对字段，不塞进攻击详述。目标、群体范围、条件、次数和先后顺序保留。', '- 护甲等简述显示产出数字。详述用自然句说明比例或系数、实际生效的品质与等级加成；没有加成时省略。', '- 每张牌的基础、条件、冲锋／收招、重箭、药方、阵赏均列出。公共机制放在独立说明中，同一术语解释一次。', '- 分支未定的通用阵赏列出全部分支；分支确定后只显示已锁分支。尚未演算完整任务重放时，不伪造未来护甲与阵赏总伤害。', '- 怪物的防御、状态修正、护甲吸收、最终损血由鼠标移到怪物后的tooltip负责。','', '## 数值示例基准','', '每张卡独立计算，示例攻击100、防御100、队伍最高等级100、3名存活友方。初始护甲、药效、蓄力、气势、敌方标记及持续伤害均为0；主角内力30/30、伙伴法师34/34。只计算该效果的自身产出，不把怪物防御、附加持续伤害及反应链合并为攻击数字。','', '这是供看文案的示例，不是实际角色面板或已验证的完整战斗预览。实际UI须从同一条运行时数值管线实时填数；当前属性或卡牌条件改变后，简述数值也改变。多段攻击逐段显示；未来条件奖励保留规则关系。有关血势、药效先获得后治疗、内力先付费后溢出等特殊顺序，已在对应卡下写出。','', '品质按原生及可升级品质逐个列出。所有治疗系数都是基础值。25点治疗在百级稀有使用600%增幅，零药效为150、药效5时为180；史诗700%增幅则为175／210。等级与品质统一合入最终增幅倍率，600%表示6倍。复制、护甲翻倍及返还不再次缩放。','', '## 分组文本表','', '| 分组 | 卡数 | 品质版本 | 完整表 |','|---|---:|---:|---|']
readme += [f'| {g} | {n} | {v} | [{g}]({f}) |' for g,n,f,v in files]
readme.insert(8,'- 对象名称在简述、详述中独占一行并加粗，只显示“单体友方”“单体敌方”“单体友方/敌方”“全体敌方”“全体友方”，不加“对象：”前缀或把解释挤在同一行。限定自身等规则在正文中说明；混合效果的范围沿用各自说明。无单位对象的牌写“无需选择对象”。')
readme.insert(9,'- 交互：默认简述；按住Shift显示详述；右键单击开启本卡pill说明，松开鼠标仍保持，再次右键关闭。Shift临时显示详述，松开回当前模式；移出预览后收起。只解释本卡有的pill，每种一次。阅读页每个品质下的“交互预览”可试用；[逐卡右键说明表](11-card-pill-descriptions.md)列出完整文本。')
readme.insert(10,'- 右键只解释关键词：气力、内力改为普通文字，说明放资源栏；同时出现的蓄力与重箭合并解释。持续伤害共性仅补一遍。自动入手条件与重箭先后顺序写回对应卡牌详述，满手等情况用临场提示。中毒采用最新确认：任意一方回合结束时，失去等同中毒值的生命。')
readme += ['', '## 单独待复核的语义','', '| 项目 | 当前处理 |','|---|---|', '| 雷走八方 | 单次重击、保留余标记与费用已确认；120／180／220／240等倍率、编序窗口及阵赏仍按候选列出。 |', '| 后巷脱身 | 总规格写低血敌方标记、金贵护甲；旧卡表和代码写低血友方获得标记／护甲／格挡。本稿单独标记，应用前统一目标。 |', '| 阵师已标品质的护甲 | 依据总规则“标稀有／史诗即已含品质”，村寨援阵稀有50%、水镜折光稀有80%、万象史诗200%、四象史诗120%按终值拟稿；这几项不再额外乘品质。 |', '| 封喉／浪断的不消耗流血效果 | 旧机制与全局非消耗规则重复，文案如实保留；未自行替换为新收益。 |', '', '## 文案覆盖检查','', '- 逐ID对照有效目录：173张，无遗漏、无重复、无25张退役路线卡。', '- 普通／稀有／史诗的419个合法版本均有费用、完整简述、详述。', '- 四张通用法师牌的16种阵赏按合法品质展开为36条。', '- 所有正文pill都有共享定义；药方首次额外1气、NPC重箭锁定时点、4／5／3张任务、每回合上限均有文本。', '- 已检查数值生成的关键例子及禁止的攻击参数列、括号算式和旧DOT层数单位。', '', '范围：本次“全部”指173张有效玩家卡及它们关联的卡牌／pill／状态／任务／目标提示，不重写剧情、菜单或所有敌人意图文案。', '', '依据：[整体规格](../../superpowers/specs/2026-09-03-card-monster-progression-rebalance-design.md)、[法师速查](../2026-09-03-sorcerer-card-quick-reference.md)以及当前卡牌定义、品质规则和效果实现。后续先逐批审阅本表，再实现相应文案；完整运行时重平衡仍有未完成项。','']
(OUT/'README.md').write_text('\n'.join(readme),encoding='utf-8')

page=r'''<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>GameXXK 全卡牌文案审阅</title>
<style>*{box-sizing:border-box}body{margin:0;background:#f7f5ef;color:#242a2a;font:15px/1.7 "Microsoft YaHei",sans-serif}header,main{max-width:1400px;margin:auto;padding:24px}h1{font-size:26px;margin:0 0 8px}h2{margin:0;font-size:20px}p{margin:6px 0;color:#57615f}.tools{position:sticky;top:0;background:#f7f5eff5;border-bottom:1px solid #d4dbd5;padding:12px 24px;z-index:2;display:flex;gap:12px;flex-wrap:wrap;align-items:center}input,select{font:inherit;border:1px solid #b7c3bd;border-radius:5px;padding:7px 12px;background:white}input{flex:1;min-width:220px}.count{margin-left:auto;color:#53675e}article{background:white;border:1px solid #dbe0da;border-radius:8px;margin-bottom:20px;padding:20px}article.candidate{border-left:5px solid #b77e25}.meta{font-size:13px;color:#68726b;margin:5px 0 12px}.pair{display:grid;grid-template-columns:1fr 1fr;gap:18px}.text{white-space:pre-wrap;overflow-wrap:anywhere}.label{font-size:12px;color:#6b7770;letter-spacing:1px;margin-bottom:6px}.panel{background:#f7f9f5;padding:14px;border-radius:5px}.panel.detail{background:#f4f6f8}.pill{display:inline-block;background:#e1e9df;border:1px solid #c7d4c4;color:#3b5945;line-height:1.4;border-radius:4px;padding:0 4px;margin:0 2px}details{margin-top:12px;border-top:1px solid #e7ebe4;padding-top:10px}summary{cursor:pointer;color:#53645a}code{font-size:12px;overflow-wrap:anywhere}.variant{margin:12px 0 20px}.cost{font-size:14px;font-weight:bold;margin:5px 0}.rule{font-size:13px;padding:8px 0;white-space:pre-wrap}.review-note{background:#fcf7e9;padding:8px 12px;font-size:13px}.gloss{display:grid;grid-template-columns:repeat(auto-fit,minmax(320px,1fr));gap:14px}.gloss article{margin:0}.tab{display:inline-block;border:1px solid #c8d1ca;border-radius:4px;padding:6px 13px;color:#365844;text-decoration:none}.hidden{display:none}@media(max-width:760px){.pair{grid-template-columns:1fr}header,main{padding:16px}.tools{padding:10px 16px}article{padding:14px}}@media print{.tools{position:static}article{break-inside:avoid}.hidden{display:none}}.target-line{display:block;font-size:15px;font-weight:700;color:#314c3c;margin:0 0 8px;line-height:1.6}.interaction-demo{margin-top:12px}.interactive-preview{border:1px solid #bccabd;border-radius:5px;background:#fffef8;padding:12px;margin-top:8px;outline-offset:3px}.interaction-mode{font-size:12px;color:#677969;margin-bottom:8px}.interaction-content{height:290px;overflow:auto;padding-right:8px}.pill-explanation{margin:0 0 10px}.pill-explanation strong{display:block;color:#345a42;margin-bottom:2px}.pill-shared-note{font-size:13px;color:#5f6c61;border-top:1px solid #dce3da;padding-top:8px;margin-top:8px}</style>
<header><h1>全部卡牌描述文本</h1><p>173张卡 · 419个品质版本 · 文案审阅稿</p><p>简述显示伤害数字，详述显示攻击百分比。示例：攻击100、防御100、队伍等级100；药效／护甲／蓄力等初始为0。特殊顺序见核对备注。游戏UI尚未修改。</p><p><a class="tab" href="#" id="cardsTab">逐卡文案</a> <a class="tab" href="#" id="sharedTab">Pill与共享提示</a> <a class="tab" href="README.md">说明与待复核项</a></p></header>
<div class="tools"><input id="search" aria-label="搜索卡名或效果" placeholder="搜索卡名、CardId、效果或关键词"><select id="group" aria-label="卡组"><option value="">全部卡组</option></select><select id="quality" aria-label="品质"><option value="native">原生品质</option><option>普通</option><option>稀有</option><option>史诗</option><option value="all">全部合法品质</option></select><label><input type="checkbox" id="pending" style="min-width:0">只看语义待复核</label><span class="count" id="count"></span></div><main id="content"></main>
<script>const DATA=__DATA__;let mode='cards';const $=s=>document.getElementById(s);const esc=s=>String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));const copy=s=>esc(s).replace(/〔([^〕]+)〕/g,'<span class="pill">$1</span>');const pair=(c,d,heading)=>`<div class="pair"><div class="panel"><div class="label">简述</div><div class="target-line">${esc(heading)}</div><div class="text">${copy(c)}</div></div><div class="panel detail"><div class="label">详述</div><div class="target-line">${esc(heading)}</div><div class="text">${copy(d)}</div></div></div>`;
[...new Set(DATA.cards.map(c=>c.group))].forEach(g=>$('group').insertAdjacentHTML('beforeend',`<option>${esc(g)}</option>`));
function render(){const s=$('search').value.trim().toLowerCase(),g=$('group').value,q=$('quality').value;
if(mode==='shared'){const defs=DATA.glossary.filter(x=>(x.name+x.text+x.kind).toLowerCase().includes(s));const states=DATA.state_texts.filter(x=>x.join(' ').toLowerCase().includes(s));$('count').textContent=`${defs.length}条关键词，${states.length}条状态提示`;$('content').innerHTML='<h2>Pill与状态说明</h2><div class="gloss">'+defs.map(x=>`<article><h2>${esc(x.name)}</h2><div class="meta">${esc(x.kind)}</div><div>${esc(x.text)}</div><p>${esc(x.example)}</p></article>`).join('')+'</div><h2 style="margin-top:24px">交互与状态模板</h2>'+states.map(x=>`<article><h2>${esc(x[0])}</h2><div class="text">${esc(x[1])}</div><p>${esc(x[2])}</p></article>`).join('');return;}
const list=DATA.cards.filter(c=>(!g||c.group===g)&&(!s||JSON.stringify(c).toLowerCase().includes(s))&&(!$('pending').checked||c.status!=='文案待审')&&(q==='native'||q==='all'||c.variants.some(v=>v.quality===q)));
$('count').textContent=`${list.length}／173张`;$('content').innerHTML=list.map(c=>{const vs=q==='native'?[c.variants[0]]:q==='all'?c.variants:c.variants.filter(v=>v.quality===q);return `<article class="${c.status==='文案待审'?'':'candidate'}"><h2>${esc(c.name)}</h2><div class="meta">${esc(c.group)} · 原生${esc(c.base_quality)} · ${esc(c.status)}</div>`+vs.map(v=>`<section class="variant"><div class="cost">${esc(v.quality)} · ${esc(v.cost)}</div>${pair(v.compact,v.detail,v.target_heading)}${interactivePreview(c,v)}${c.branches.length?'<details><summary>该品质的全部阵赏分支</summary>'+c.branches.filter(b=>b.quality===v.quality).map(b=>`<div class="cost">${esc(b.branch)}阵赏</div>${pair(b.compact,b.detail,b.target_heading)}`).join('')+'</details>':''}</section>`).join('')+(c.common_rules.length?`<details><summary>机制核对资料</summary>${c.common_rules.map(t=>`<div class="rule">${copy(t)}</div>`).join('')}</details>`:'')+`<details><summary>取值与语义核对</summary><code>${esc(c.id)}</code><p>目标入口：${esc(c.target)}</p><p>${esc(c.source_rule)}</p>${c.note?`<div class="review-note">${esc(c.note)}</div>`:''}</details></article>`;}).join('')||'<p>没有匹配卡牌。</p>';}
['search','group','quality','pending'].forEach(id=>$(id).addEventListener('input',render));$('cardsTab').onclick=e=>{e.preventDefault();mode='cards';render()};$('sharedTab').onclick=e=>{e.preventDefault();mode='shared';render()};render();</script></html>'''
page=page.replace('</script>', (HERE/'card_text_review_interaction.js').read_text(encoding='utf-8')+'\n</script>',1)
safe_data=json.dumps(payload,ensure_ascii=False).replace('<',chr(92)+'u003c')
(OUT/'review.html').write_text(page.replace('__DATA__',safe_data),encoding='utf-8')

report=dict(card_ids=173,groups=expected,quality_variants=419,universal_reward_rows=36,glossary_rows=len(GLOSSARY),state_template_rows=len(STATE_TEXTS),all_ids_match_active_baseline=True,all_ids_present_in_current_catalog_source=True,all_pills_defined=True,no_unexpanded_card_tokens=True,no_attack_source_parameter_rows=True,no_dense_multiplication_strings=True,numeric_spot_checks='passed',generator_changed_runtime_code=False,generator_runs_ue=False,pending=meta['pending'])
report.update(target_labels=list(TARGET_LABELS),target_descriptions_complete=True,target_description_variants=419+36,separate_bold_target_lines=True)
report.update(pill_help_variants=419+36,pill_help_current_card_only=True,pill_help_no_duplicates=True,tooltip_modes=['compact','shift_detail','right_button_pill_help'])
report.update(pill_help_click_toggle=True,pill_help_survives_mouseup=True,pill_help_max_characters=max(len(p['description']) for c in CARDS for v in c['variants'] for p in v['pill_descriptions']))
report.update(resource_help_at_resource_bars=['气力','内力'],charge_heavy_arrow_help_merged=True,shared_dot_note_once=True,automatic_hand_in_card_copy=4)
(OUT/'coverage-check.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps(report,ensure_ascii=False,indent=2))
