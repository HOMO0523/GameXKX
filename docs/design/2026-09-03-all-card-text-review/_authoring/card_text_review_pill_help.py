"""Right-button content contains only the pills owned by this card/quality."""
GLOSS_BY_NAME={name:text for name,kind,text,example in GLOSSARY if kind!='对象'}
DOT_PILLS={'流血','中毒','灼烧','蚀伤','毒爆'}
DOT_NOTE='持续伤害直接损失生命，触发不消耗数值。'

def names_in(text):
    return list(dict.fromkeys(re.findall(r'〔([^〕]+)〕',text)))

def explain_pills(card,variant,branches=()):
    text='\n'.join([variant['compact'],variant['detail']]+card['common_rules']+
                   [b[k] for b in branches for k in ('compact','detail')])
    names=names_in(text)
    result=[]
    q=QN.index(variant['quality'])
    merge_heavy='蓄力' in names and '重箭' in names
    merged=False
    for name in names:
        assert name in GLOSS_BY_NAME,(card['id'],name)
        if merge_heavy and name in ('蓄力','重箭'):
            if not merged:result.append(dict(name='蓄力／重箭',members=['蓄力','重箭'],description=GLOSS_BY_NAME['重箭']))
            merged=True
            continue
        description=GLOSS_BY_NAME[name]
        if name=='法术任务':
            rule=next(t for t in card['common_rules'] if t.startswith('〔法术任务〕'))
            count=re.search(r'(\d+)张',rule).group(1)
            description=f'本组{count}种牌各主动打出1次，依序免费重放；每回合限1次。'
        elif name=='重箭':
            description=GLOSS_BY_NAME['重箭']
        elif name=='反击':
            description=f'对方单体攻击牌结算后反击攻击者，{100+20*q}%攻击伤害；消耗1次。'
        elif name=='格挡':
            description=f'对方单体攻击牌结算后，以{100+20*q}%攻击＋剩余护甲反击；消耗1次。'
        elif name=='冰爆':
            description=f'消耗全部护甲攻击全体敌方；{100+20*q}%攻击，每点护甲再加1个百分点。'
        result.append(dict(name=name,members=[name],description=description))
    return result

def shared_note(items):
    return DOT_NOTE if any(set(i['members'])&DOT_PILLS for i in items) else ''

def prepare_pill_help():
    for card in CARDS:
        for v in card['variants']:
            matching=[b for b in card['branches'] if b['quality']==v['quality']]
            v['pill_descriptions']=explain_pills(card,v,matching)
            v['pill_shared_note']=shared_note(v['pill_descriptions'])
            if matching:
                v['pill_descriptions_by_branch']={b['branch']:explain_pills(card,v,[b]) for b in matching}
                v['pill_shared_note_by_branch']={name:shared_note(items) for name,items in v['pill_descriptions_by_branch'].items()}
        for b in card['branches']:
            b['pill_descriptions']=explain_pills(card,b)
            b['pill_shared_note']=shared_note(b['pill_descriptions'])

def write_pill_help_table():
    lines=['# 本卡右键pill说明文本','', '[返回总表](README.md)','',
           '右键说明只列当前卡牌、当前品质具有的pill，按出现顺序去重。通用阵赏未定时包含其可能分支；锁定分支后只保留该分支和基础效果的pill。对象名称是独立的加粗行，不作为效果pill。','',
           '气力与内力由资源栏介绍；蓄力和重箭同时出现时合并解释。持续伤害共性只在涉及它的卡下补一次。以下把同牌不同品质中相同的说明合并，右键只展示当前品质对应的一份。','']
    for card in CARDS:
        lines += ['## '+card['name'],'', '`'+card['id']+'`','', '| Pill | 说明文本 | 适用品质 |','|---|---|---|']
        rows={}
        for v in card['variants']:
            for item in v['pill_descriptions']:
                rows.setdefault((item['name'],item['description']),[]).append(v['quality'])
        if not rows:lines.append('| 无 | 无额外关键词说明。 | 全部 |')
        for (name,description),qualities in rows.items():
            lines.append('| '+name+' | '+description.replace('\n','<br>').replace('|','\\|')+' | '+'／'.join(qualities)+' |')
        if any(v['pill_shared_note'] for v in card['variants']):lines+=['',DOT_NOTE]
        lines.append('')
    (OUT/'11-card-pill-descriptions.md').write_text('\n'.join(lines),encoding='utf-8')
