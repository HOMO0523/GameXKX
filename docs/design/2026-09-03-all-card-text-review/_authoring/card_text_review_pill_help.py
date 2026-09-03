"""Right-button content contains only the pills owned by this card/quality."""
GLOSS_BY_NAME={name:text for name,kind,text,example in GLOSSARY if kind!='对象'}

def names_in(text):
    return list(dict.fromkeys(re.findall(r'〔([^〕]+)〕',text)))

def explain_pills(card,variant,branches=()):
    text='\n'.join([variant['compact'],variant['detail']]+card['common_rules']+
                   [b[k] for b in branches for k in ('compact','detail')])
    names=names_in(text)
    result=[]
    q=QN.index(variant['quality'])
    for name in names:
        assert name in GLOSS_BY_NAME,(card['id'],name)
        description=GLOSS_BY_NAME[name]
        if name=='法术任务':
            rule=next(t for t in card['common_rules'] if t.startswith('〔法术任务〕'))
            count=re.search(r'(\d+)张',rule).group(1)
            description=f'{count}种携带牌各主动使用1次，依序免费重放基础效果，再结算首牌阵赏；每回合限1次。'
        elif name=='重箭':
            timing=next((t for t in card['common_rules'] if t.startswith('〔重箭〕')),'').removeprefix('〔重箭〕')
            description=('基础效果后' if '基础效果后' in timing else '基础效果前')+'锁定并消耗全部蓄力，按牌文结算收益。'
        elif name=='反击':
            description=f'对方单体直接攻击牌后反击攻击者，{100+20*q}%攻击伤害，耗1次；不连锁。'
        elif name=='格挡':
            description=f'对方单体直接攻击牌后反攻：{100+20*q}%攻击＋剩余护甲；耗1次，护甲保留，不连锁。'
        elif name=='冰爆':
            description=f'消耗全部护甲攻击全体敌方；{100+20*q}%攻击，每点护甲再加1个百分点。'
        result.append(dict(name=name,description=description))
    return result

def prepare_pill_help():
    for card in CARDS:
        for v in card['variants']:
            matching=[b for b in card['branches'] if b['quality']==v['quality']]
            v['pill_descriptions']=explain_pills(card,v,matching)
            if matching:
                v['pill_descriptions_by_branch']={b['branch']:explain_pills(card,v,[b]) for b in matching}
        for b in card['branches']:
            b['pill_descriptions']=explain_pills(card,b)

def write_pill_help_table():
    lines=['# 本卡右键pill说明文本','', '[返回总表](README.md)','',
           '右键说明只列当前卡牌、当前品质具有的pill，按出现顺序去重。通用阵赏未定时包含其可能分支；锁定分支后只保留该分支和基础效果的pill。对象名称是独立的加粗行，不作为效果pill。','',
           '以下把同牌不同品质中相同的说明合并；右键实际只展示该品质对应的一份说明。','']
    for card in CARDS:
        lines += ['## '+card['name'],'', '`'+card['id']+'`','', '| Pill | 说明文本 | 适用品质 |','|---|---|---|']
        rows={}
        for v in card['variants']:
            for item in v['pill_descriptions']:
                rows.setdefault((item['name'],item['description']),[]).append(v['quality'])
        if not rows:lines.append('| 无 | 这张牌没有额外的pill说明。 | 全部 |')
        for (name,description),qualities in rows.items():
            lines.append('| '+name+' | '+description.replace('\n','<br>').replace('|','\\|')+' | '+'／'.join(qualities)+' |')
        lines.append('')
    (OUT/'11-card-pill-descriptions.md').write_text('\n'.join(lines),encoding='utf-8')
