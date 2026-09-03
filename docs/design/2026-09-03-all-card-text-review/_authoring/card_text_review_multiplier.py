"""Wording-only projection: fold real scaling factors into percentage multipliers."""
def pct(x):
    x=Fraction(x)
    return str(x.numerator) if x.denominator==1 else '约'+fmt(x)

def multiplier_copy(t,q,native):
    merged=pct(500*Q[q])
    # Armor retains the authored Defense coefficient; quality is a total multiplier.
    def armor_auth(m):
        val,ref=m.group(1),QN.index(m.group(2))
        amp=100*Q[q]/Q[ref]
        return val+'%防御的〔护甲〕'+('' if amp==100 else '，'+pct(amp)+'%增幅倍率')+'。'
    t=re.sub(r'(\d+(?:\.\d+)?)%防御的〔护甲〕，已含(普通|稀有|史诗)品质；升级按品质比例提高。',armor_auth,t)
    t=re.sub(r'(\d+)%防御的(〔护甲〕|护甲)，品质加成\d+%',lambda m:m.group(1)+'%防御的'+m.group(2)+'，'+pct(100*Q[q])+'%增幅倍率',t)
    t=t.replace('获得相当于','获得')
    # Explicit overflow coefficient scales by level and quality, not Defense.
    t=re.sub(r'按100%转甲(?:，品质加成\d+%)?，等级倍率5倍',f'按{merged}%转为护甲',t)
    # Structured generation prose, including caps and Defense ignore.
    t=re.sub(r'基础值(\d+)的(〔[^〕]+〕|灼烧)(?:，品质加成\d+%)?，等级倍率5倍',lambda m:m.group(1)+'点基础'+m.group(2)+f'，按{merged}%结算',t)
    t=re.sub(r'基础值(\d+)(?:，品质加成\d+%)?，等级倍率5倍',lambda m:m.group(1)+f'点基础值，按{merged}%结算',t)
    t=re.sub(r'灼烧基础值改为(\d+)，品质和等级加成同上',lambda m:'基础灼烧改为'+m.group(1)+f'点，仍按{merged}%结算',t)
    # Each authored healing coefficient carries a reference quality.
    def healing(m):
        coefficient=m.group(1)
        base_multiplier=pct(500*Q[q])
        return f'基础治疗{coefficient}，按{base_multiplier}%结算'
    t=re.sub(r'治疗系数(\d+)(?:，由(普通|稀有|史诗)系数按(?:普通|稀有|史诗)品质折算)?',healing,t)
    t=re.sub(r'；消耗全部〔药效〕，每点增加[\d.]+系数；等级倍率5倍',f'；消耗全部〔药效〕，按{merged}%转为额外治疗',t)
    t=re.sub(r'，每点药效增加[\d.]+系数，等级倍率5倍，消耗全部药效',f'；消耗全部药效，按{merged}%转为额外治疗',t)
    t=re.sub(r'，每点药效增加[\d.]+系数，等级倍率5倍，药效只消耗1次',f'；全部药效只消耗1次，按{merged}%转为额外治疗',t)
    t=t.replace('，等级倍率5倍，不消耗药效','，不消耗药效')
    def supplement(m):
        return f'再结算{m.group(1)}点治疗或反转，按{pct(500*Q[q])}%结算'
    t=re.sub(r'再结算系数(\d+)的治疗或反转(?:，由(普通|稀有|史诗)系数按(?:普通|稀有|史诗)品质折算)?，等级倍率5倍',supplement,t)
    # Shared multiplier at the end of multi-branch Yin/Yang medicine copy.
    t=re.sub(r'品质加成\d+%，等级倍率5倍',f'结算倍率{merged}%',t)
    # Fixed-quality terrain payloads use level scaling only.
    t=t.replace('基础生命10，等级倍率5倍','10点基础生命，按500%结算')
    t=t.replace('基础值2的灼烧，等级倍率5倍','2点基础灼烧，按500%结算')
    t=re.sub(r'基础治疗(\d+)',r'\1点治疗',t)
    t=re.sub(r'(\d+)点基础(〔[^〕]+〕|灼烧|生命)',r'\1点\2',t)
    t=re.sub(r'按(约?[\d.]+)%结算',r'\1%增幅倍率',t)
    t=re.sub(r'按(约?[\d.]+)%转为额外治疗',r'以\1%增幅倍率转为额外治疗',t)
    t=re.sub(r'按(约?[\d.]+)%转为护甲',r'转为护甲，\1%增幅倍率',t)
    t=t.replace('结算倍率','增幅倍率')
    t=re.sub(r'全体友方各恢复生命，(\d+)点治疗',r'全体友方各获得\1点治疗',t)
    t=re.sub(r'目标恢复生命，(\d+)点治疗',r'目标获得\1点治疗',t)
    t=re.sub(r'恢复生命，(\d+)点治疗',r'\1点治疗',t)
    t=re.sub(r'对友方治疗，对敌人造成〔治疗反转〕；(\d+)点治疗',r'\1点治疗或〔治疗反转〕',t)
    t=re.sub(r'对所选一方全体结算治疗或〔治疗反转〕；(\d+)点治疗',r'所选一方全体各结算\1点治疗或〔治疗反转〕',t)
    t=re.sub(r'；消耗全部〔药效〕，以约?[\d.]+%增幅倍率转为额外治疗','；消耗全部〔药效〕，等量增加基础治疗',t)
    t=re.sub(r'；消耗全部药效，以约?[\d.]+%增幅倍率转为额外治疗','；消耗全部药效，等量增加基础治疗',t)
    t=re.sub(r'；全部药效只消耗1次，以约?[\d.]+%增幅倍率转为额外治疗','；全部药效只消耗1次，等量增加基础治疗',t)
    t=t.replace('点基础值','点')
    return t
