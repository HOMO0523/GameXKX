"""Recipient descriptions for the review copy, separate from internal attack sources."""
TARGET_LABELS=('单体友方','单体敌方','单体友方/敌方','全体敌方','全体友方')

def target_line(*labels,note='',description=''):
    assert all(s in TARGET_LABELS for s in labels)
    text=description or ('对象：'+'、'.join('〔'+s+'〕' for s in labels) if labels else '无需选择对象')
    if note:text+='；'+note
    return dict(target_labels=list(labels),target_note=note,target_description=text+'。',
                target_heading=' · '.join(labels) if labels else '无需选择对象')

def describe_card_target(card):
    ident=card['id']
    special={
        'Hero.Healer.YiXueCuiFang':target_line('全体友方',note='无需手动选择'),
        'Hero.Healer.BaiCaoJiZhen':target_line('全体友方','全体敌方',description='治疗：〔全体友方〕；中毒、灼烧：〔全体敌方〕'),
        'Hero.Hunter.FengYanDingXian':target_line('单体友方',note='仅自身'),
        'Hero.Mage.GuiXuTongXuan':target_line('单体友方',note='内力回复仅自身，无需手动选择'),
        'Hero.Guard.JieJiaHuanFeng':target_line('单体敌方','单体友方',description='伤害：〔单体敌方〕；护甲、格挡：〔单体友方〕，自动选护甲最高者'),
        'Hero.Guard.XuanJiaZhenYue':target_line('单体友方','全体敌方',description='消耗护甲：〔单体友方〕；伤害：〔全体敌方〕'),
        'Hero.Formation.YiZhenHuiXiang':target_line('全体友方','全体敌方',note='按当前地势确定，无需手动选择'),
        'Hero.Formation.LianYingBuShi':target_line('全体友方','全体敌方',note='作用于下一次地势收益，按该次地势确定'),
        'Hero.Formation.LiuHeGuiYi':target_line('全体友方','全体敌方',note='各地势收益分别作用于对应一方'),
        'Profession.Healer.YaoYin':target_line('单体友方/敌方','全体友方','全体敌方',description='选择：〔单体友方/敌方〕；群体附加效果按下方分支作用于〔全体友方〕或〔全体敌方〕'),
        'Profession.Healer.XingQiZhen':target_line('全体友方',note='无需手动选择，药效归自身'),
        'Profession.Healer.HuiChunLu':target_line('单体友方/敌方','全体友方','全体敌方',description='选择：〔单体友方/敌方〕；治疗或反转作用于所选一方的〔全体友方〕或〔全体敌方〕'),
        'Profession.Healer.YaoWangGuiYuan':target_line('单体友方/敌方','全体友方','全体敌方',description='选择：〔单体友方/敌方〕；治疗或反转作用于所选一方的〔全体友方〕或〔全体敌方〕'),
        'Profession.Sorcerer.XingHuoHuiShou':target_line('单体友方','全体友方',description='内力回复：〔单体友方〕，仅自身；溢出护甲：〔全体友方〕'),
        'Npc.YueBai.CanJuanPiZhu':target_line('全体友方','全体敌方',note='地势收益按当前地势确定，无需手动选择'),
        'Npc.YueBai.ShanHeCanTu':target_line('全体友方',note='护甲、内力作用于全体友方；地势收益按当前地势确定'),
        'Npc.ZhouGuangZu.DiZhiMoTu':target_line('全体友方','全体敌方',note='地势收益按当前地势确定，无需手动选择'),
        'Npc.JinGui.HouXiangTuoShen':target_line('全体友方','单体敌方','单体友方',description='灵动：〔全体友方〕；标记暂列〔单体敌方〕、护甲与格挡暂列〔单体友方〕，具体受益者待复核'),
    }
    if ident in special:return special[ident]
    switch_side={
        'GuanShi':'全体敌方','JieShanWeiZhang':'全体敌方',
        'DingZhen':'全体友方','YinShuiHuiYuan':'全体友方','KunZhen':'全体友方','LinYingMiZong':'全体友方',
    }
    if ident.startswith('Profession.FormationMaster.') and ident.split('.')[-1] in switch_side:
        return target_line(switch_side[ident.split('.')[-1]],note='切换地势后生效，无需手动选择')
    raw=card['target']
    if raw.startswith('任意存活单位'):return target_line('单体友方/敌方')
    if raw.startswith('自身'):return target_line('单体友方',note='仅自身')
    if raw.startswith('生命最低友方'):return target_line('单体友方',note='自动选择生命最低者')
    if raw.startswith('无需选择'):return target_line(note='作用于手牌、牌堆或共享气力')
    for label in TARGET_LABELS:
        if raw.startswith(label):return target_line(label)
    raise ValueError('Unclassified card target: '+ident+' '+raw)

def describe_reward_target(card,branch):
    suffix=card['id'].split('.')[-1]
    b=branch['branch']
    if suffix=='YanMuHuTi':return target_line('全体敌方')
    if suffix=='LieFu':return target_line('全体友方' if b=='普通' else '全体敌方',note='回气、抽牌与自身返甲按各自说明执行')
    if suffix=='XingHuoHuiShou':
        descriptions={
            '普通':'护甲：〔全体友方〕；虚弱：〔全体敌方〕',
            '火':'护甲：〔全体友方〕；灼烧、虚弱：〔全体敌方〕',
            '冰':'冰爆：〔全体敌方〕；返还护甲：〔全体友方〕',
            '雷':'标记、攻击：〔全体敌方〕；护甲：〔全体友方〕',
        }
        return target_line('全体友方','全体敌方',description=descriptions[b])
    if suffix=='ChiYanFengJie':
        if b=='普通':
            info=target_line(description='对象：沿用被重放牌的作用对象')
            info['target_heading']='沿用重放牌对象'
            return info
        if b=='冰':return target_line('单体友方','全体敌方',description='冰牌重放：〔单体友方〕，仅自身；冰爆：〔全体敌方〕')
        return target_line('全体敌方',note='重放保留原牌的各项作用对象')
    raise ValueError('Unclassified reward: '+card['id']+' '+b)

def standardize_recipients(text):
    # Normalize recipient wording; never infer the attacker from these labels.
    text=text.replace('全体敌人','全体敌方')
    text=text.replace('进行〔冰爆〕','对全体敌方进行〔冰爆〕')
    return text

def visible_target_heading(info):
    # Selected-side group effects explain the expansion in their existing body.
    if '单体友方/敌方' in info['target_labels']:return '单体友方/敌方'
    return info['target_heading']

def target_body_note(card):
    if card['id'] in ('Hero.Formation.YiZhenHuiXiang','Hero.Formation.LianYingBuShi',
                       'Npc.YueBai.CanJuanPiZhu','Npc.ZhouGuangZu.DiZhiMoTu'):
        return '地势收益的对象由生效时的地势决定。\n'
    if card['target'].startswith('自身') and card['id'] not in (
        'Profession.Healer.XingQiZhen','Profession.Guard.TieSuoHengJiang'):
        return '基础效果仅作用于自身。\n'
    return ''
