#!/usr/bin/env python3
"""Build legal deck, trigger-cycle, and DPR projections for approved Hell 3-1 builds."""

from __future__ import annotations

import json
import math
import random
import re
import statistics
from pathlib import Path

from export_game_enemy_design_table import build_design_data
from export_game_equipment_design_table import ROLE_STATS


ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/"docs"/"design"/"2026-09-04-project-design-tables"
CARD_PATH=ROOT/"docs"/"design"/"2026-09-03-all-card-text-review"/"card-texts.json"
ROLE_CN={"Blade":"刀客","Guard":"守卫","Healer":"药师","Hunter":"弓手","Sorcerer":"法师","FormationMaster":"阵师"}
NPC_CN={"Npc.TusiChief":"土司首领","Npc.SongJinBao":"宋金宝","Npc.YueBai":"月白","Npc.ZhouGuangZu":"周光祖","Npc.JinGui":"金贵","Npc.QiongMeiEr":"琼么儿"}
NPC_ALL={
"Npc.TusiChief":["Npc.TusiChief.ZhaiZhuHaoLing","Npc.TusiChief.ShiMenShouShi","Npc.TusiChief.TuSiJunLing","Npc.TusiChief.MengZhaiShiYue"],
"Npc.SongJinBao":["Npc.SongJinBao.ShangQianGuWu","Npc.SongJinBao.ErMuMiBao","Npc.SongJinBao.GuiKeLing","Npc.SongJinBao.YiNuoQianJin"],
"Npc.YueBai":["Npc.YueBai.QingYanDianDeng","Npc.YueBai.CanJuanPiZhu","Npc.YueBai.YueBaiZhaoYe","Npc.YueBai.ShanHeCanTu"],
"Npc.ZhouGuangZu":["Npc.ZhouGuangZu.YiCaoBianShi","Npc.ZhouGuangZu.HuangShanFuZhi","Npc.ZhouGuangZu.DiZhiMoTu","Npc.ZhouGuangZu.YanFenFengMai"],
"Npc.JinGui":["Npc.JinGui.ShiJingErMu","Npc.JinGui.QiaoYanZhouXuan","Npc.JinGui.ZaYiChouBei","Npc.JinGui.HouXiangTuoShen"],
"Npc.QiongMeiEr":["Npc.QiongMeiEr.TengQiaoFeiDu","Npc.QiongMeiEr.GuWuMiZong","Npc.QiongMeiEr.YinLingZhenXin","Npc.QiongMeiEr.ShanGeHuanLing"]}
DUAL={"Npc.TusiChief":("Blade","Guard"),"Npc.SongJinBao":("Sorcerer","Blade"),"Npc.YueBai":("FormationMaster","Sorcerer"),"Npc.ZhouGuangZu":("Healer","FormationMaster"),"Npc.JinGui":("Guard","Hunter"),"Npc.QiongMeiEr":("Hunter","Healer")}
NPC_BASE={"Npc.TusiChief":(115,11,14,1.4,10,1.0),"Npc.SongJinBao":(88,8,10,1.0,7,.6),"Npc.YueBai":(84,8,15,1.5,6,.5),"Npc.ZhouGuangZu":(90,8,12,1.2,7,.7),"Npc.JinGui":(92,9,12,1.1,8,.8),"Npc.QiongMeiEr":(96,9,13,1.3,8,.8)}
NAKED={"Hero":(1585,312,206),"Blade":(983,215,75),"Guard":(1308,110,130),"Healer":(882,109,76),"Hunter":(878,214,65),"Sorcerer":(773,163,54),"FormationMaster":(985,130,87)}
FINAL={code:(hp,atk,defense) for code,_,hp,atk,defense in ROLE_STATS}
PARTNER_MANA={"Blade":121,"Guard":117,"Healer":228,"Hunter":123,"Sorcerer":34,"FormationMaster":228}
NPC_MANA={"Npc.TusiChief":123,"Npc.SongJinBao":228,"Npc.YueBai":232,"Npc.ZhouGuangZu":230,"Npc.JinGui":226,"Npc.QiongMeiEr":228}

# Mirrors GameXXKCardRules::IsTerrainActiveCard for active cards used by this
# report. Text matching is intentionally avoided: cards such as 连营布势 mention
# terrain rewards without triggering one, while 山河残图 triggers one without
# changing terrain.
TERRAIN_ACTIVE_CARD_IDS={
"Hero.Formation.GuanShiLuoZi","Hero.Formation.YiZhenHuiXiang","Hero.Formation.LiuHeGuiYi",
"Profession.FormationMaster.GuanShi","Profession.FormationMaster.DingZhen","Profession.FormationMaster.YinShuiHuiYuan",
"Profession.FormationMaster.KunZhen","Profession.FormationMaster.LinYingMiZong","Profession.FormationMaster.JieShanWeiZhang",
"Npc.YueBai.CanJuanPiZhu","Npc.YueBai.ShanHeCanTu"}

HERO_DIG=["Hero.Generic.FengShenBu","Hero.Generic.GuanXi","Hero.Generic.XingQiHuiHuan","Hero.Generic.QingFengYiShi"]
BUILDS=[
{"id":"guard_release","name":"玄甲释甲连爆","role":"Guard","npc":"Npc.TusiChief","hero":HERO_DIG+["Hero.Guard.TieBiTongShou","Hero.Guard.JieJiaHuanFeng","Hero.Guard.LieZhenChengFeng","Hero.Guard.XuanJiaZhenYue"],"partner":["Profession.Guard.TieBi","Profession.Guard.HuZhu","Profession.Guard.PiJiaXingJun","Profession.Guard.ZhenYueLing","Profession.Guard.BiLeiFanGong"],"npc_cards":["Npc.TusiChief.ZhaiZhuHaoLing","Npc.TusiChief.TuSiJunLing","Npc.TusiChief.MengZhaiShiYue"],"sequence":["Profession.Guard.PiJiaXingJun","Profession.Guard.BiLeiFanGong","Hero.Guard.XuanJiaZhenYue","Npc.TusiChief.ZhaiZhuHaoLing"],"sets":{"hero":"追风6","partner":"玄甲6","npc":"青囊6"},"gear":"主角追风6；守卫玄甲6堆防御；土司青囊6补高费抽牌与气力","set_synergy":"玄甲提高守卫产甲10%，保留半甲并在首次格挡后追加80%攻击；六件在敌方首次打穿护甲时补全队护甲与援护。","fallback":"若出生缺壁垒反攻，用镇岳令；若NPC缺寨主号令，用土司军令补标记/破绽。"},
{"id":"hunter_heavy","name":"双重箭蓄力重箭","role":"Hunter","npc":"Npc.JinGui","hero":HERO_DIG+["Hero.Hunter.FengYanDingXian","Hero.Hunter.LieYuLianShi","Hero.Hunter.CuiDuChuanXin","Hero.Hunter.HuiFengGuanRi"],"partner":["Profession.Hunter.YingYan","Profession.Hunter.LianZhuJian","Profession.Hunter.LieWang","Profession.Hunter.ChuanYang","Profession.Hunter.ShouHun"],"npc_cards":["Npc.JinGui.ShiJingErMu","Npc.JinGui.QiaoYanZhouXuan","Npc.JinGui.ZaYiChouBei"],"sequence":["Hero.Hunter.FengYanDingXian","Hero.Hunter.LieYuLianShi","Npc.JinGui.ShiJingErMu","Profession.Hunter.YingYan","Profession.Hunter.ChuanYang"],"sets":{"hero":"蚀骨6","partner":"追风6","npc":"玄甲6"},"gear":"主角蚀骨6堆攻击；弓手追风6；金贵玄甲6堆防御","set_synergy":"主角亲自施加流血/中毒，因此由主角蚀骨完成自动毒爆与首次保留；伙伴追风提供全队抽牌回气，金贵玄甲承担单体压力。","fallback":"缺市井耳目时由杂役筹备供3蓄力；伙伴缺穿杨时改狩魂。"},
{"id":"blade_momentum","name":"气势断岳破军","role":"Blade","npc":"Npc.SongJinBao","hero":HERO_DIG+["Hero.Blade.TongFengYinShi","Hero.Blade.XueLuXiangCheng","Hero.Blade.YingFengHuanBu","Hero.Blade.TongPaoJuShi"],"partner":["Profession.Blade.LieFengZhan","Profession.Blade.HuiFengJiaShi","Profession.Blade.DuanYue","Profession.Blade.PoJun","Profession.Blade.ZhanYiFeiTeng"],"npc_cards":["Npc.SongJinBao.ShangQianGuWu","Npc.SongJinBao.ErMuMiBao","Npc.SongJinBao.YiNuoQianJin"],"sequence":["Npc.SongJinBao.ShangQianGuWu","Profession.Blade.ZhanYiFeiTeng","Profession.Blade.DuanYue","Profession.Blade.PoJun"],"energy_overrides":{"Profession.Blade.DuanYue":0,"Profession.Blade.PoJun":0},"sets":{"hero":"追风6","partner":"破军6","npc":"玄甲6"},"gear":"主角追风6；刀客破军6堆攻击；宋金宝玄甲6堆防御","set_synergy":"破军在冲锋消费时抽牌，保存收招为藏式，并让下一回合首张主动牌重放基础效果；核心牌实付0～1气，玄甲比无法稳定触发的青囊更适合宋金宝。","fallback":"缺赏钱鼓舞时用主角同锋引式供气势；缺一诺千金不影响核心爆发。"},
{"id":"healer_toxic","name":"双DoT毒爆药方","role":"Healer","npc":"Npc.QiongMeiEr","hero":HERO_DIG+["Hero.Healer.YiXueCuiFang","Hero.Healer.HuiChunNiMai","Hero.Healer.DuHuoTongLu","Hero.Healer.BaiCaoJiZhen"],"partner":["Profession.Healer.YaoYin","Profession.Healer.XingQiZhen","Profession.Healer.BaiCaoDu","Profession.Healer.FuGuSan","Profession.Healer.LianQiaoJieDu"],"npc_cards":["Npc.QiongMeiEr.TengQiaoFeiDu","Npc.QiongMeiEr.GuWuMiZong","Npc.QiongMeiEr.YinLingZhenXin"],"sequence":["Npc.QiongMeiEr.GuWuMiZong","Profession.Healer.BaiCaoDu","Profession.Healer.FuGuSan","Profession.Healer.LianQiaoJieDu"],"sets":{"hero":"追风6","partner":"蚀骨6","npc":"青囊6"},"gear":"主角追风6；药师蚀骨6；琼么儿青囊6","set_synergy":"蚀骨让首次双DoT立即毒爆且首次毒爆保留储量；青囊把第一张高费药方变成抽牌、回血与回气节点。","fallback":"缺蛊雾迷踪时用毒火同炉先建立两种DoT；缺连翘引毒时用主角毒火同炉触发毒爆。"},
{"id":"sorcerer_fire","name":"五牌炎序任务","role":"Sorcerer","npc":"Npc.YueBai","hero":HERO_DIG+["Hero.Mage.YanXuLiaoYuan","Hero.Mage.HanXuNingChuan","Hero.Mage.LeiXuYinTing","Hero.Mage.GuiXuTongXuan"],"partner":["Profession.Sorcerer.LingHuoFu","Profession.Sorcerer.JuLing","Profession.Sorcerer.LiHuoYin","Profession.Sorcerer.YanQiang","Profession.Sorcerer.BaoYanShu"],"npc_cards":["Npc.YueBai.QingYanDianDeng","Npc.YueBai.CanJuanPiZhu","Npc.YueBai.YueBaiZhaoYe"],"sequence":["Profession.Sorcerer.LingHuoFu","Profession.Sorcerer.LiHuoYin","Profession.Sorcerer.YanQiang","Profession.Sorcerer.BaoYanShu","Profession.Sorcerer.JuLing","Npc.YueBai.QingYanDianDeng","Npc.YueBai.CanJuanPiZhu","Npc.YueBai.YueBaiZhaoYe"],"sets":{"hero":"追风6","partner":"蚀骨6","npc":"山河6"},"gear":"主角追风6；法师蚀骨6；月白山河6","set_synergy":"炎法用蚀骨保留首次毒爆/灼烧储量；月白山河每回合额外触发当前地势并给首张地势牌减费抽牌。","fallback":"伙伴出生缺爆炎时换燎原寻诀；月白缺任一牌仍能由剩余三牌完成其确定性任务。"},
{"id":"formation_assault","name":"断崖破绽镇煞","role":"FormationMaster","npc":"Npc.YueBai","hero":HERO_DIG+["Hero.Formation.GuanShiLuoZi","Hero.Formation.YiZhenHuiXiang","Hero.Formation.LianYingBuShi","Hero.Formation.LiuHeGuiYi"],"partner":["Profession.FormationMaster.JieShanWeiZhang","Profession.FormationMaster.GuanShi","Profession.FormationMaster.HuiShengZhenSha","Profession.FormationMaster.ZhenShaZhen","Profession.FormationMaster.ShanMenFengSuo"],"npc_cards":["Npc.YueBai.QingYanDianDeng","Npc.YueBai.CanJuanPiZhu","Npc.YueBai.YueBaiZhaoYe"],"sequence":["Profession.FormationMaster.JieShanWeiZhang","Hero.Formation.LianYingBuShi","Profession.FormationMaster.ShanMenFengSuo","Profession.FormationMaster.HuiShengZhenSha","Profession.FormationMaster.ZhenShaZhen","Npc.YueBai.CanJuanPiZhu"],"sets":{"hero":"追风6","partner":"山河6","npc":"蚀骨6"},"gear":"主角追风6；阵师山河6堆防御；月白蚀骨6堆攻击","set_synergy":"山河首张地势牌减1气并抽1张、给队友回内；每回合开始再由阵师触发一次当前地势。","fallback":"出生缺借山为障时用观势落子切平原；缺镇煞阵时以地脉借力替换爆发位。"}
]


def npc_stats():
    out={}
    for npc,(hb,hg,ab,ag,db,dg) in NPC_BASE.items():
        naked=(math.floor(hb+hg*99),math.floor(ab+ag*99),math.floor(db+dg*99));r1,r2=DUAL[npc]
        delta=tuple(round(((FINAL[r1][i]-NAKED[r1][i])+(FINAL[r2][i]-NAKED[r2][i]))/2) for i in range(3));final=tuple(naked[i]+delta[i] for i in range(3))
        out[npc]={"name":NPC_CN[npc],"roles":f"{ROLE_CN[r1]}＋{ROLE_CN[r2]}","hp":final[0],"attack":final[1],"defense":final[2],"mana":NPC_MANA[npc],"note":"双职业平均至宝装备预算投影；NPC最终词缀尚未单独冻结"}
    return out


def card_meta(card):
    v=card["variants"][-1];cost=re.match(r'(\d+)气／(\d+)内',v["cost"]);compact=v["compact"]
    base=[]
    for line in compact.splitlines():
        if line.startswith("〔阵赏〕"): break
        base.append(line)
    base_text="\n".join(base)
    draw=max([int(x) for x in re.findall(r'抽(\d+)张',base_text)] or [0]);discard=max([int(x) for x in re.findall(r'弃(\d+)张',base_text)] or [0])
    gain=max([int(x) for x in re.findall(r'回复(\d+)(?:点)?气(?:力)?',base_text)] or [0])
    mana_gain=max([int(x) for x in re.findall(r'回复(\d+)(?:点)?内力',base_text)] or [0])
    owner="hero" if card["id"].startswith("Hero.") else "partner" if card["id"].startswith("Profession.") else "npc"
    return {"id":card["id"],"name":card["name"],"energy":int(cost.group(1)),"mana":int(cost.group(2)),"draw":draw,"discard":discard,"gain":gain,"mana_gain":mana_gain,"mana_all":bool(re.search(r'全体友方各回复\d+(?:点)?内力',base_text)),"owner":owner,"formula":"首次开启药方另加1气" in v["cost"],"terrain":card["id"] in TERRAIN_ACTIVE_CARD_IDS,"exhaust":"〔消耗〕" in base_text,"search":"〔检索〕" in base_text,"compact":compact,"detail":v["detail"]}


def draw_one(draw,discard,rng):
    if not draw and discard: rng.shuffle(discard);draw.extend(discard);discard.clear()
    return draw.pop() if draw else None


def simulate_rounds(deck,sequence,meta,build,energy_overrides=None,seeds=4000):
    energy_overrides=energy_overrides or {}
    caps={"hero":30,"partner":PARTNER_MANA[build["role"]],"npc":NPC_MANA[build["npc"]]}
    set_owners={owner:set_name for owner,set_name in build["sets"].items()}
    shanhe_owner=next((owner for owner,set_name in set_owners.items() if set_name=="山河6"),None)
    has_qingnang="青囊6" in set_owners.values()
    has_zhui_feng="追风6" in set_owners.values()
    has_po_jun="破军6" in set_owners.values()
    results=[];completed=0;card_counts=[];mana_floor=[];deadlocks=0;qingnang_counts=[];shanhe_counts=[]
    for seed in range(seeds):
        rng=random.Random(910000+seed);draw=list(deck);rng.shuffle(draw);hand=[];discard=[];exhaust=[]
        for _ in range(5):
            if (c:=draw_one(draw,discard,rng)):hand.append(c)
        step=0;mana=dict(caps);played_total=0;lowest_ratio=1.0;formula_open=set();next_discount=0;free_cards=0;qingnang_total=0;shanhe_total=0
        for round_no in range(1,21):
            if round_no>1:
                for owner in mana:mana[owner]=min(caps[owner],mana[owner]+2)
            energy=3;actions=0;active_this_round=0;shanhe_used=False;pojun_draw_used=False;qingnang_used=False
            while actions<20 and step<len(sequence):
                wanted=sequence[step]
                def payable(cid):
                    m=meta[cid];ec=energy_overrides.get(cid,m["energy"])
                    if m["formula"] and m["owner"] not in formula_open:ec+=1
                    mc=m["mana"]
                    if free_cards>0:return 0,0
                    if next_discount>0:ec=max(0,ec-next_discount)
                    if shanhe_owner==m["owner"] and m["terrain"] and not shanhe_used:ec=max(0,ec-1)
                    return ec,mc
                ecost,mcost=payable(wanted)
                playable=wanted in hand and ecost<=energy and mcost<=mana[meta[wanted]["owner"]]
                if playable: cid=wanted
                else:
                    digs=[]
                    for x in hand:
                        de,dm=payable(x)
                        if x not in sequence[step:] and meta[x]["draw"]>0 and de<=energy and dm<=mana[meta[x]["owner"]]:digs.append(x)
                    if not digs:
                        if wanted in hand:deadlocks+=1
                        break
                    digs.sort(key=lambda x:(meta[x]["energy"],-meta[x]["draw"],x));cid=digs[0]
                hand.remove(cid);m=meta[cid];ecost,mcost=payable(cid);energy-=ecost;mana[m["owner"]]-=mcost;energy+=m["gain"];actions+=1;played_total+=1;active_this_round+=1
                if free_cards>0:free_cards-=1
                elif next_discount>0:next_discount=0
                if cid=="Hero.Generic.QingFengYiShi":next_discount=1
                if cid=="Npc.SongJinBao.YiNuoQianJin":free_cards=2
                if m["formula"]:formula_open.add(m["owner"])
                if m["mana_all"]:
                    for owner in mana:mana[owner]=min(caps[owner],mana[owner]+m["mana_gain"])
                else:
                    mana[m["owner"]]=min(caps[m["owner"]],mana[m["owner"]]+m["mana_gain"])
                lowest_ratio=min(lowest_ratio,min(mana[o]/caps[o] for o in mana))
                if cid==wanted:step+=1
                destination=exhaust if m["exhaust"] else discard;destination.append(cid)
                for _ in range(m["draw"]):
                    if (c:=draw_one(draw,discard,rng)):hand.append(c)
                if shanhe_owner==m["owner"] and m["terrain"] and not shanhe_used:
                    shanhe_used=True;shanhe_total+=1
                    if (c:=draw_one(draw,discard,rng)):hand.append(c)
                    for owner in mana:
                        if owner!=shanhe_owner:mana[owner]=min(caps[owner],mana[owner]+2)
                if has_po_jun and cid in {"Profession.Blade.DuanYue","Profession.Blade.PoJun"} and not pojun_draw_used:
                    pojun_draw_used=True
                    if (c:=draw_one(draw,discard,rng)):hand.append(c)
                if has_zhui_feng and active_this_round%2==0:
                    if (c:=draw_one(draw,discard,rng)):hand.append(c)
                if has_zhui_feng and active_this_round in {2,4}:energy+=1
                if has_zhui_feng and active_this_round==4:
                    if (c:=draw_one(draw,discard,rng)):hand.append(c)
                if has_qingnang and ecost>=2 and not qingnang_used:
                    qingnang_used=True;qingnang_total+=1;energy+=1
                    if (c:=draw_one(draw,discard,rng)):hand.append(c)
                for _ in range(m["discard"]):
                    choices=[x for x in hand if x!=(sequence[step] if step<len(sequence) else None)] or hand
                    if choices:
                        gone=choices[0];hand.remove(gone);discard.append(gone)
                if m["search"] and step<len(sequence):
                    target=sequence[step]
                    if target in draw:draw.remove(target);hand.append(target)
                    elif target in discard:discard.remove(target);hand.append(target)
            if step==len(sequence):results.append(round_no);card_counts.append(played_total);mana_floor.append(lowest_ratio);qingnang_counts.append(qingnang_total);shanhe_counts.append(shanhe_total);completed+=1;break
            discard.extend(hand);hand=[]
            for _ in range(5):
                if (c:=draw_one(draw,discard,rng)):hand.append(c)
    return {"completion":completed/seeds,"avg":statistics.fmean(results) if results else 20,"median":statistics.median(results) if results else 20,"p90":sorted(results)[math.ceil(.9*len(results))-1] if results else 20,"avg_cards":statistics.fmean(card_counts) if card_counts else 0,"mana_floor_pct":100*statistics.fmean(mana_floor) if mana_floor else 0,"resource_blocks":deadlocks,"qingnang_triggers":statistics.fmean(qingnang_counts) if qingnang_counts else 0,"shanhe_triggers":statistics.fmean(shanhe_counts) if shanhe_counts else 0}


def dmg(atk,coeff,target,ignore=0,amp=1):
    generated=math.ceil(atk*coeff/100)
    after_defense=max(1,generated-max(0,target["defense"]-ignore))
    amplified=math.floor(after_defense*amp)
    return math.ceil(amplified*.75)
def aoe(atk,coeff,targets,amp=1): return sum(dmg(atk,coeff,t,0,amp) for t in targets)


def enemy_hit(atk,coeff,target,group=False):
    generated=math.ceil(atk*coeff/100)
    difficulty=math.ceil(generated*1.5)
    after_defense=max(1,difficulty-target["defense"])
    return math.ceil(after_defense*1.25)


def enemy_pressure(build,stats,npcs,targets):
    party=[stats["Hero"],stats[build["role"]],npcs[build["npc"]]]
    single=min(party,key=lambda x:x["hp"])
    group=lambda atk,coeff:sum(enemy_hit(atk,coeff,x,True) for x in party)
    vulture=targets[0];ape=targets[1];toad=targets[2]
    vulture_avg=(enemy_hit(vulture["attack"],340,single)+group(vulture["attack"],210))/3
    toad_avg=(enemy_hit(toad["attack"],400,single)+group(toad["attack"],190))/3
    ape_cards={
        1:(enemy_hit(ape["attack"],160,single)+enemy_hit(ape["attack"],260,single)+group(ape["attack"],115))/5,
        2:(enemy_hit(ape["attack"],160,single)+group(ape["attack"],100)+enemy_hit(ape["attack"],270,single))/5,
        3:(enemy_hit(ape["attack"],200,single)+group(ape["attack"],125)+enemy_hit(ape["attack"],310,single))/5,
    }
    rows=[]
    threats={1:"标记、灼烧、下张牌+1气；白猿首个减益给自身护甲",2:"虚弱、驱散、下张牌+1气；白猿为各敌首个减益护甲",3:"群体虚弱、群体驱散、连续流血触发；白猿护甲系数160%"}
    for phase in (1,2,3):
        incoming=vulture_avg+toad_avg+ape_cards[phase]
        rows.append({"phase":phase,"incoming_before_armor":round(incoming),"party_hp_ratio":incoming/sum(x["hp"] for x in party),"threat":threats[phase]})
    return rows


def phase_armor_tax(build,targets):
    ape_def=targets[1]["defense"];toad_def=targets[2]["defense"]
    uptime={"guard_release":.7,"hunter_heavy":1.0,"blade_momentum":.8,"healer_toxic":1.0,"sorcerer_fire":1.0,"formation_assault":1.0}[build["id"]]
    aoe_share={"guard_release":.85,"hunter_heavy":.30,"blade_momentum":.25,"healer_toxic":.25,"sorcerer_fire":1.0,"formation_assault":.85}[build["id"]]
    passive=(math.ceil(ape_def*1.2)+3*math.ceil(ape_def*1.0)+3*math.ceil(ape_def*1.6))/3*uptime
    deck=(math.ceil(ape_def*1.2)/4+math.ceil(ape_def*1.8)/4)/3
    toad=math.ceil(toad_def*3.6)/3
    return round((passive+deck)*(.35+.65*aoe_share)+toad*(.25+.75*aoe_share))


def variant_sequence(build,selected):
    bid=build["id"]
    if bid=="guard_release": return build["sequence"][:-1]+(["Npc.TusiChief.ZhaiZhuHaoLing"] if "Npc.TusiChief.ZhaiZhuHaoLing" in selected else ["Npc.TusiChief.TuSiJunLing"])
    if bid=="hunter_heavy": return build["sequence"][:-3]+(["Npc.JinGui.ShiJingErMu"] if "Npc.JinGui.ShiJingErMu" in selected else ["Npc.JinGui.ZaYiChouBei"])+build["sequence"][-2:]
    if bid=="blade_momentum": return (["Npc.SongJinBao.ShangQianGuWu"] if "Npc.SongJinBao.ShangQianGuWu" in selected else ["Hero.Blade.TongFengYinShi"])+build["sequence"][1:]
    if bid=="healer_toxic": return (["Npc.QiongMeiEr.GuWuMiZong"] if "Npc.QiongMeiEr.GuWuMiZong" in selected else ["Hero.Healer.DuHuoTongLu"])+build["sequence"][1:]
    if bid=="sorcerer_fire":
        partner=build["sequence"][:5];priority=["Npc.YueBai.QingYanDianDeng","Npc.YueBai.CanJuanPiZhu","Npc.YueBai.YueBaiZhaoYe","Npc.YueBai.ShanHeCanTu"]
        return partner+[x for x in priority if x in selected]
    if bid=="formation_assault": return build["sequence"][:-1]+(["Npc.YueBai.CanJuanPiZhu"] if "Npc.YueBai.CanJuanPiZhu" in selected else ["Npc.YueBai.QingYanDianDeng"])
    return build["sequence"]


def cycle_damage(build,stats,npcs,targets,rounds,selected):
    h=stats["Hero"];p=stats[build["role"]];n=npcs[build["npc"]];center=targets[1]
    bars=[{"phase":0,"hp":x["hp"],"max":x["hp"],"phases":x["phases"]} for x in targets]
    steps=[];raw_total=0;applied_total=0
    def single(amount,index=1):return [{index:max(0,round(amount))}]
    def group(atk,coeff,amp=1):return [{i:dmg(atk,coeff,t,0,amp) for i,t in enumerate(targets)}]
    def resolve(packets):
        raw=applied=waste=0
        for packet in packets:
            for index,requested in packet.items():
                raw+=requested;state=bars[index]
                if state["phase"]>=state["phases"]:waste+=requested;continue
                hp=state["hp"]
                if state["phase"]<state["phases"]-1:
                    dealt=min(requested,max(0,hp-1)) if requested>=hp else min(requested,hp)
                    state["hp"]-=dealt
                    if state["hp"]<=max(1,state["max"]//100):state["phase"]+=1;state["hp"]=state["max"]
                else:
                    dealt=min(requested,hp);state["hp"]-=dealt
                    if state["hp"]<=0:state["phase"]=state["phases"]
                applied+=dealt;waste+=requested-dealt
        return raw,applied,waste
    def add(name,rule,packets):
        nonlocal raw_total,applied_total
        raw,applied,waste=resolve(packets);raw_total+=raw;applied_total+=applied
        steps.append({"name":name,"rule":rule,"damage":applied,"raw_damage":raw,"phase_waste":waste})
    bid=build["id"]
    if bid=="guard_release":
        armor=math.ceil(math.ceil(p["defense"]*.8*1.4)*1.1);post=math.ceil(math.ceil(p["defense"]*.5*1.4)*1.1)
        npc_card="寨主号令" if "Npc.TusiChief.ZhaiZhuHaoLing" in selected else "土司军令";npc_coeff=140 if npc_card=="寨主号令" else 210
        add("披甲行军",f"玄甲2将守卫产甲放大至{armor}",[])
        add("壁垒反攻",f"消耗{armor}甲，攻击倍率{308+armor}%并回补{post}甲",group(p["attack"],308+armor))
        add("玄甲镇岳",f"消耗守卫现有{post}甲，攻击倍率{280+post}%",group(h["attack"],280+post))
        add(npc_card,f"{npc_coeff}%单攻并补防御链",single(dmg(n["attack"],npc_coeff,center)))
        block=math.ceil(max(1,p["attack"]+post-center["defense"])*.75)
        add("敌方阶段格挡／玄甲追击","2次格挡与本回合首次80%攻击追击",single(block)+single(block)+single(dmg(p["attack"],80,center)))
    elif bid=="hunter_heavy":
        packets=single(dmg(h["attack"],196,center))
        for bleed in (56,55,54):packets+=single(dmg(h["attack"],70,center))+single(bleed)
        add("风眼→裂羽","3蓄力：196%＋3段70%；每段重箭后触发当时流血",packets)
        if "Npc.JinGui.ShiJingErMu" in selected:
            packets=[]
            for bleed in (53,52):packets+=single(dmg(h["attack"],70,center))+single(bleed)
            add("市井耳目","给最高攻友方2蓄力并立即2段70%重箭",packets)
        else:
            packets=[]
            for bleed in (53,52,51):packets+=single(dmg(h["attack"],56,center))+single(bleed)
            add("杂役筹备","给最高攻友方3蓄力并立即3段56%重箭",packets)
        add("锐意→穿杨","3蓄力：攻击倍率420%，合计无视84防御",single(dmg(p["attack"],420,center,84)))
    elif bid=="blade_momentum":
        has_song="Npc.SongJinBao.ShangQianGuWu" in selected;momentum=4 if has_song else 6
        add("赏钱鼓舞" if has_song else "同锋引式","给刀客气势并准备冲锋重放",single(dmg(n["attack"],140,center)) if has_song else [])
        add("战意沸腾","再得2气势并返还下一牌费用",[])
        add("断岳",f"{momentum}气势使攻击倍率变为{196+10*momentum}%，随后施加3破绽",single(dmg(p["attack"],196+10*momentum,center)))
        packets=single(dmg(p["attack"],192,center,0,1.3))+sum((single(dmg(p["attack"],70,center)) for _ in range(3)),[])
        add("破军","1层新气势强化首段；消耗3破绽追加3段70%",packets)
    elif bid=="healer_toxic":
        if "Npc.QiongMeiEr.GuWuMiZong" in selected:
            add("蛊雾迷踪","流血28＋中毒42；显式毒爆与蚀骨自动毒爆分包结算",single(28)+single(42)+single(28)+single(42));initial_bleed=28
        else:
            add("毒火同炉","182%攻击；毒42＋灼烧14的两次毒爆",single(dmg(h["attack"],182,center))+single(42)+single(14)+single(42)+single(14));initial_bleed=0
        add("百草毒","中毒继续累积，为后续毒爆与边界伤害准备",[])
        add("腐骨散",f"84%攻击后触发既有{initial_bleed}流血，再补流血/中毒",single(dmg(p["attack"],84,center))+single(initial_bleed))
        add("连翘引毒","补毒并以流血70／中毒100分包毒爆",single(70)+single(100))
        expected_ticks=rounds*2;full_ticks=math.floor(expected_ticks);partial_tick=round((expected_ticks-full_ticks)*100)
        poison_packets=sum((single(100) for _ in range(full_ticks)),[])
        if partial_tick>0:poison_packets+=single(partial_tick)
        add("中毒边界",f"按{rounds:.2f}回合、每回合2个边界，折算{expected_ticks:.2f}次触发",poison_packets)
    elif bid=="sorcerer_fire":
        has_qing="Npc.YueBai.QingYanDianDeng" in selected;has_zhao="Npc.YueBai.YueBaiZhaoYe" in selected
        add("伙伴五牌主动","灵枢98%＋灵火84%＋爆炎112%；三张群攻独立跨阶段",group(p["attack"],98)+group(p["attack"],84)+group(p["attack"],112))
        add("伙伴任务重放","三张群攻重放；启动阵赏触发一次100灼烧",group(p["attack"],98)+group(p["attack"],84)+group(p["attack"],112)+[{0:100,1:100,2:100}])
        packets=[]
        if has_zhao:packets+=single(dmg(n["attack"],140,center))+single(dmg(n["attack"],140,center))
        if has_qing:packets+=single(100)+single(100)
        elif has_zhao:packets+=single(80)
        add("月白三牌任务","按4选3完成基础重放与启动奖励",packets)
    else:
        add("借山为障","山河4令首张地势牌0气；断崖建立破绽/标记",[])
        add("山河6＋连营布势→山门封锁","回合开始额外地势；下一次收益四触发",[])
        add("回声震杀","336%单攻，按标记与3破绽约1.45放大",single(dmg(p["attack"],336,center,0,1.45)))
        add("镇煞阵","448%群攻，按标记/破绽约1.65放大",group(p["attack"],448,1.65))
        add("残卷批注" if "Npc.YueBai.CanJuanPiZhu" in selected else "青焰点灯","补抽牌与地势，或补灼烧并触发",[] if "Npc.YueBai.CanJuanPiZhu" in selected else single(84))
    return steps,applied_total,applied_total/rounds,raw_total,raw_total-applied_total


def build_data():
    raw=json.loads(CARD_PATH.read_text(encoding="utf-8"));cards={c["id"]:c for c in raw["cards"]};pending=set(raw["meta"]["pending"])
    stats={code:{"name":name,"hp":hp,"attack":atk,"defense":defense} for code,name,hp,atk,defense in ROLE_STATS};npcs=npc_stats()
    enemies=build_design_data();end=next(x for x in enemies["hell31"] if x["category"]=="end");targets=end["units"]
    all_ids={x for b in BUILDS for x in b["hero"]+b["partner"]+NPC_ALL[b["npc"]]}
    assert not ({x for b in BUILDS for x in b["hero"]+b["partner"]+b["npc_cards"]} & pending)
    meta={cid:card_meta(cards[cid]) for cid in all_ids}
    reports=[]
    for b in BUILDS:
        assert len(b["hero"])==8 and len(b["partner"])==5 and len(b["npc_cards"])==3 and len(set(b["hero"]+b["partner"]+b["npc_cards"]))==16
        variants=[]
        for missing in NPC_ALL[b["npc"]]:
            selected=[x for x in NPC_ALL[b["npc"]] if x!=missing]
            if set(selected)&pending:continue
            sequence=variant_sequence(b,selected)
            timing=simulate_rounds(b["hero"]+b["partner"]+selected,sequence,meta,b,b.get("energy_overrides"),4000)
            steps,total,dpr,raw_cycle,phase_waste=cycle_damage(b,stats,npcs,targets,timing["avg"],selected)
            tax=phase_armor_tax(b,targets);conservative=max(0,dpr-tax)
            variants.append({"missing":cards[missing]["name"],"selected":[cards[x]["name"] for x in selected],"sequence":[cards[x]["name"] for x in sequence],"timing":timing,"steps":steps,"cycle_damage":total,"raw_cycle_damage":raw_cycle,"phase_waste":phase_waste,"dpr":dpr,"armor_tax":tax,"conservative_dpr":conservative})
        expected_dpr=statistics.fmean(x["dpr"] for x in variants);expected_conservative=statistics.fmean(x["conservative_dpr"] for x in variants);expected_damage=statistics.fmean(x["cycle_damage"] for x in variants);expected_raw_cycle=statistics.fmean(x["raw_cycle_damage"] for x in variants);expected_phase_waste=statistics.fmean(x["phase_waste"] for x in variants);expected_rounds=statistics.fmean(x["timing"]["avg"] for x in variants)
        ideal=max(variants,key=lambda x:x["conservative_dpr"]);pressure=enemy_pressure(b,stats,npcs,targets);party_hp=stats["Hero"]["hp"]+stats[b["role"]]["hp"]+npcs[b["npc"]]["hp"]
        purpose={"guard_release":"最高群体爆发与最稳护甲反击；适合自动战斗容错。","hunter_heavy":"最快完成短循环；适合优先击破白猿阶段。","blade_momentum":"单体阶段压缩强，依赖冲锋/收招顺序。","healer_toxic":"启动较慢但持续伤害稳定，适合长战与高护甲阶段。","sorcerer_fire":"全体同步压阶段，任务重放后爆发集中。","formation_assault":"控制与防御兼顾，断崖状态让后续大招稳定增幅。"}[b["id"]]
        card_rows=lambda ids:[{"id":cid,"name":cards[cid]["name"],"energy":meta[cid]["energy"],"mana":meta[cid]["mana"]} for cid in ids]
        reports.append({**b,"role_cn":ROLE_CN[b["role"]],"npc_cn":NPC_CN[b["npc"]],"hero_names":[cards[x]["name"] for x in b["hero"]],"partner_names":[cards[x]["name"] for x in b["partner"]],"npc_names":ideal["selected"],"hero_cards":card_rows(b["hero"]),"partner_cards":card_rows(b["partner"]),"npc_cards_detail":card_rows([x for x in NPC_ALL[b["npc"]] if cards[x]["name"] in ideal["selected"]]),"sequence_names":ideal["sequence"],"timing":ideal["timing"],"steps":ideal["steps"],"cycle_damage":round(ideal["cycle_damage"]),"raw_cycle_damage":round(ideal["raw_cycle_damage"]),"phase_waste":round(ideal["phase_waste"]),"dpr":ideal["dpr"],"armor_tax":ideal["armor_tax"],"conservative_dpr":ideal["conservative_dpr"],"npc_variant_average_dpr":expected_conservative,"dpr_min":min(x["conservative_dpr"] for x in variants),"dpr_max":max(x["conservative_dpr"] for x in variants),"phase_hp_ratio":ideal["cycle_damage"]/end["raw_phase_hp"],"ten_round_damage":round(ideal["conservative_dpr"]*10),"turns_to_stage_end":end["raw_phase_hp"]/max(1,ideal["conservative_dpr"]),"pressure":pressure,"party_hp":party_hp,"survival_rounds_before_armor":party_hp/max(x["incoming_before_armor"] for x in pressure),"purpose":purpose,"npc_variants":variants})
    reports.sort(key=lambda x:-x["conservative_dpr"])
    for idx,r in enumerate(reports,1):r["rank"]=idx
    return {"meta":{"stage":"地狱3-1","party_level":100,"enemy_level":125,"deck":"8主角+5伙伴+3NPC=16张；起手5；每回合补至5；共享3气；角色回2内","stage_end_hp":end["raw_phase_hp"],"samples_per_npc_loadout":4000,"pending":sorted(pending),"card_count":len(cards),"intent_cases":351,"formations":189,"save_version":36,"damage_label":"防御、25级差和逐伤害包阶段截断后，阶段护甲预算前","conservative_label":"再扣白猿/巨蟾阶段护甲预算","limitations":["每个攻击／DOT包按1%阶段阈值独立截断；同一多段牌的后续段可继续攻击新阶段。","NPC面板使用双职业平均至宝词缀预算；NPC逐条最终词缀尚未单独冻结。","敌方伤害压力使用已实装阶段牌系数、150%地狱倍率和25级差；未模拟灵动、援护与每次实际目标转移。","破军6的次回合首张主动牌重放依赖下一循环实际首牌，本表不把该包加入刀客核心伤害，因此刀客DPR是下界。","每个合法NPC三牌组合用4000次确定性洗牌测算核心循环；不是胜率，也不替代最终PIE自动战斗采样。"]},"stats":stats,"npcs":npcs,"builds":reports,"hell31":enemies["hell31"],"stage_end":end}


HTML=r"""<!doctype html>
<html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>GameXXK 地狱3-1 配队、卡序与伤害期望</title>
<style>
:root{--ink:#251a12;--muted:#705d49;--paper:#f1ddb5;--paper2:#fff8e9;--line:#ba9060;--red:#973e30;--green:#2f6b4b;--blue:#345f79;--shadow:#0005}
*{box-sizing:border-box}body{margin:0;background:#17110d;color:var(--ink);font:14px/1.55 "Microsoft YaHei",sans-serif}.page{max-width:1540px;margin:auto;padding:22px}
.hero,.panel,.build{background:linear-gradient(135deg,#f8edd3,#e6c894);border:1px solid var(--line);border-radius:15px;box-shadow:0 12px 30px var(--shadow)}
.hero{padding:26px;margin-bottom:15px}h1,h2,h3,h4{font-family:KaiTi,"Microsoft YaHei",sans-serif;margin:.35em 0}.sub{color:var(--muted)}
.kpis{display:grid;grid-template-columns:repeat(6,1fr);gap:9px;margin-top:14px}.kpi{background:#fff9eccf;border:1px solid #cda879;border-radius:10px;padding:10px}.kpi b{display:block;color:var(--red);font-size:21px}
.panel{padding:18px;margin:14px 0}.toolbar{display:flex;gap:10px;align-items:center;flex-wrap:wrap}.toolbar select{padding:7px 10px;border-radius:8px;border:1px solid var(--line);background:var(--paper2)}
.summary{overflow:auto}table{width:100%;border-collapse:collapse}th,td{padding:8px;border-bottom:1px solid #c9a16f;text-align:right;vertical-align:top}th{background:#4a3425;color:#fff4dc;position:sticky;top:0}th:first-child,td:first-child{text-align:left}
.build{padding:19px;margin:14px 0}.head{display:flex;justify-content:space-between;gap:12px;align-items:flex-start}.rank{font:bold 24px KaiTi;color:var(--red)}
.tags{display:flex;gap:6px;flex-wrap:wrap;margin:7px 0}.tag{background:#553c2b;color:#fff3da;border-radius:999px;padding:3px 9px}.tag.green{background:var(--green)}.tag.blue{background:var(--blue)}
.metrics{display:grid;grid-template-columns:repeat(7,1fr);gap:7px;margin:10px 0}.metric{background:#fff8e7;border:1px solid #d2ad7b;border-radius:8px;padding:8px}.metric b{display:block;font-size:18px;color:var(--red)}
.deck{display:grid;grid-template-columns:repeat(3,1fr);gap:9px}.deck>div{background:#fff8e6;border:1px solid #cfaa79;border-radius:9px;padding:10px}.card{display:block;padding:3px 0;border-bottom:1px dotted #d9bb91}.cost{float:right;color:var(--blue);font-weight:bold}
.sequence{background:#fff5dc;border-left:4px solid #a96f33;padding:10px;border-radius:6px;margin:10px 0}.steps{border:1px solid #c99f6e;border-radius:8px;overflow:hidden}.step{display:grid;grid-template-columns:34px 145px 1fr 90px;gap:8px;padding:7px;background:#fff9ea}.step:nth-child(even){background:#f6e5c4}.damage{font-weight:bold;color:var(--red);text-align:right}
.two{display:grid;grid-template-columns:1fr 1fr;gap:12px}.note{background:#fff8e8;border-left:4px solid #c58538;padding:10px;border-radius:7px}.warn{border-color:var(--red)}.good{color:var(--green);font-weight:bold}.small{font-size:12px;color:var(--muted)}footer{text-align:center;color:#cbb89c;padding:22px}
@media(max-width:1050px){.kpis,.metrics{grid-template-columns:repeat(3,1fr)}.deck,.two{grid-template-columns:1fr}.step{grid-template-columns:28px 120px 1fr 70px}}@media(max-width:620px){.kpis,.metrics{grid-template-columns:repeat(2,1fr)}.page{padding:9px}}
</style></head><body><main class="page">
<section class="hero"><h1>地狱 3-1：配队、装备、卡序与每回合伤害</h1><p>使用已接入运行时的173张牌、六套装备效果、125级地狱3-1编制和一／三／一阶段。每套构筑固定主角8张、百级伙伴5张、NPC 4选3；每个合法NPC三牌组合用4000次确定性洗牌检查核心顺序。</p><div class="kpis" id="kpis"></div></section>
<section class="panel"><div class="toolbar"><h2>构筑对比</h2><label>排序 <select id="sort"><option value="safe">保守DPR</option><option value="raw">阶段护甲前DPR</option><option value="speed">循环速度</option><option value="survival">护甲前生存轮数</option></select></label></div><div class="summary"><table><thead><tr><th>构筑</th><th>主角／伙伴／NPC</th><th>循环成功</th><th>平均轮数</th><th>阶段护甲前DPR</th><th>护甲预算</th><th>保守DPR</th><th>预计清关轮数</th></tr></thead><tbody id="summary"></tbody></table></div><p class="note">阶段护甲前DPR已计算敌方防御与100→125级差；保守DPR再扣白猿与巨蟾的平均护甲预算。两者都不是胜率。</p></section>
<section id="builds"></section>
<section class="panel two"><div><h2>地狱3-1七组编制</h2><div class="summary"><table><thead><tr><th>类型</th><th>编制</th><th>阶段</th><th>总阶段生命</th></tr></thead><tbody id="forms"></tbody></table></div></div><div><h2>百级批准面板</h2><div class="summary"><table><thead><tr><th>角色</th><th>生命</th><th>攻击</th><th>防御</th></tr></thead><tbody id="stats"></tbody></table></div><p class="small">角色表已含六件至宝与4攻／4防／4生命至宝宝石。NPC为双职业平均至宝词缀预算投影。</p></div></section>
<section class="panel"><h2>测试口径与限制</h2><div id="limits"></div><p class="small">数据门槛：173张卡；351个难度／阶段意图预演；189组固定编制；v36存档。页面生成日期：2026-09-05。</p></section>
<footer>GameXXK 已验证构筑分析 · 2026-09-05</footer></main>
<script>
const D=__DATA__,q=s=>document.querySelector(s),esc=s=>String(s).replace(/[&<>"']/g,c=>({"&":"&amp;","<":"&lt;",">":"&gt;","\"":"&quot;","'":"&#39;"}[c])),fmt=(x,n=1)=>Number(x).toFixed(n),pct=x=>fmt(x*100,1)+"%";
q("#kpis").innerHTML=[["我方等级",D.meta.party_level],["敌人等级",D.meta.enemy_level],["有效卡池",D.meta.card_count],["意图预演",D.meta.intent_cases],["固定编制",D.meta.formations],["关底阶段生命",D.meta.stage_end_hp]].map(x=>"<div class='kpi'>"+x[0]+"<b>"+x[1]+"</b></div>").join("");
function cardList(rows){return rows.map(x=>"<span class='card'>"+esc(x.name)+"<span class='cost'>"+x.energy+"气/"+x.mana+"内</span></span>").join("")}
function equipmentHooks(b){let x=[];if(b.timing.qingnang_triggers>0)x.push("青囊"+fmt(b.timing.qingnang_triggers,2));if(b.timing.shanhe_triggers>0)x.push("山河"+fmt(b.timing.shanhe_triggers,2));return x.length?x.join(" / "):"无"}
function order(){let a=D.builds.slice(),s=q("#sort").value;if(s==="raw")a.sort((x,y)=>y.dpr-x.dpr);else if(s==="speed")a.sort((x,y)=>x.timing.avg-y.timing.avg);else if(s==="survival")a.sort((x,y)=>y.survival_rounds_before_armor-x.survival_rounds_before_armor);else a.sort((x,y)=>y.conservative_dpr-x.conservative_dpr);return a}
function render(){const a=order();q("#summary").innerHTML=a.map(b=>"<tr><td><b>#"+b.rank+" "+esc(b.name)+"</b></td><td>主角／"+esc(b.role_cn)+"／"+esc(b.npc_cn)+"</td><td>"+pct(b.timing.completion)+"</td><td>"+fmt(b.timing.avg,2)+"</td><td>"+fmt(b.dpr)+"</td><td>-"+b.armor_tax+"</td><td><b>"+fmt(b.conservative_dpr)+"</b></td><td>"+fmt(b.turns_to_stage_end,2)+"</td></tr>").join("");
q("#builds").innerHTML=a.map(b=>"<article class='build'><div class='head'><div><span class='rank'>#"+b.rank+"</span><h2>"+esc(b.name)+"</h2><p>"+esc(b.purpose)+"</p></div><div class='tags'><span class='tag'>"+esc(b.role_cn)+"</span><span class='tag'>"+esc(b.npc_cn)+"</span><span class='tag green'>保守DPR "+fmt(b.conservative_dpr)+"</span></div></div><div class='tags'><span class='tag blue'>主角 "+esc(b.sets.hero)+"</span><span class='tag blue'>伙伴 "+esc(b.sets.partner)+"</span><span class='tag blue'>NPC "+esc(b.sets.npc)+"</span></div><p><b>配装：</b>"+esc(b.gear)+"<br><b>套装配合：</b>"+esc(b.set_synergy)+"<br><b>缺牌替代：</b>"+esc(b.fallback)+"</p><div class='metrics'><div class='metric'>循环成功<b>"+pct(b.timing.completion)+"</b></div><div class='metric'>平均完成<b>"+fmt(b.timing.avg,2)+"轮</b></div><div class='metric'>P90<b>"+b.timing.p90+"轮</b></div><div class='metric'>平均出牌<b>"+fmt(b.timing.avg_cards,1)+"张</b></div><div class='metric'>最低内力余量<b>"+fmt(b.timing.mana_floor_pct)+"%</b></div><div class='metric'>装备资源触发<b>"+equipmentHooks(b)+"</b></div><div class='metric'>阶段截断损失<b>"+b.phase_waste+"</b></div><div class='metric'>十轮保守伤害<b>"+b.ten_round_damage+"</b></div><div class='metric'>护甲前生存<b>"+fmt(b.survival_rounds_before_armor,1)+"轮</b></div></div><div class='deck'><div><h4>主角8张</h4>"+cardList(b.hero_cards)+"</div><div><h4>"+esc(b.role_cn)+"5张</h4>"+cardList(b.partner_cards)+"</div><div><h4>"+esc(b.npc_cn)+"3张</h4>"+cardList(b.npc_cards_detail)+"</div></div><div class='sequence'><b>核心顺序：</b>"+b.sequence_names.map(esc).join(" → ")+"</div><div class='steps'>"+b.steps.map((x,i)=>"<div class='step'><b>"+(i+1)+"</b><span>"+esc(x.name)+"</span><span>"+esc(x.rule)+"</span><span class='damage'>"+x.damage+(x.phase_waste?"（截断-"+x.phase_waste+"）":"")+"</span></div>").join("")+"</div><p class='good'>一次核心循环原始生成"+b.raw_cycle_damage+"，阶段截断后约"+b.cycle_damage+"伤害；阶段护甲前 "+fmt(b.dpr)+"/轮；扣除约"+b.armor_tax+"/轮阶段护甲预算后 "+fmt(b.conservative_dpr)+"/轮；预计 "+fmt(b.turns_to_stage_end,2)+" 轮覆盖关底11178阶段生命。</p><div class='two'><div><h4>敌方三阶段压力（我方护甲前）</h4><table><thead><tr><th>白猿阶段</th><th>生命压力/轮</th><th>主要机制</th></tr></thead><tbody>"+b.pressure.map(x=>"<tr><td>"+x.phase+"</td><td>"+x.incoming_before_armor+"</td><td>"+esc(x.threat)+"</td></tr>").join("")+"</tbody></table></div><div><h4>NPC 4选3敏感度</h4><table><thead><tr><th>未携带</th><th>完成轮数</th><th>成功率</th><th>保守DPR</th></tr></thead><tbody>"+b.npc_variants.map(x=>"<tr><td>"+esc(x.missing)+"</td><td>"+fmt(x.timing.avg,2)+"</td><td>"+pct(x.timing.completion)+"</td><td>"+fmt(x.conservative_dpr)+"</td></tr>").join("")+"</tbody></table></div></div></article>").join("")}
q("#sort").addEventListener("change",render);render();
q("#forms").innerHTML=D.hell31.map(x=>"<tr><td>"+({ordinary:"普通",elite:"精英",end:"关底"})[x.category]+x.index+"</td><td>"+x.units.map(u=>esc(u.name)).join(" / ")+"</td><td>"+x.units.map(u=>u.phases).join("/")+"</td><td>"+x.raw_phase_hp+"</td></tr>").join("");
q("#stats").innerHTML=Object.values(D.stats).map(x=>"<tr><td>"+esc(x.name)+"</td><td>"+x.hp+"</td><td>"+x.attack+"</td><td>"+x.defense+"</td></tr>").join("");
q("#limits").innerHTML=D.meta.limitations.map(x=>"<p class='note warn'>"+esc(x)+"</p>").join("");
</script></body></html>"""


def main():
    data=build_data();OUT.mkdir(parents=True,exist_ok=True);jp=OUT/"GameXXK_职业配队与伤害期望分析_2026-09-04.json";hp=OUT/"GameXXK_职业配队与伤害期望分析_2026-09-04.html"
    jp.write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding="utf-8",newline="\n");hp.write_text(HTML.replace("__DATA__",json.dumps(data,ensure_ascii=False,separators=(',',':'))),encoding="utf-8",newline="\n")
    print(json.dumps({"html":str(hp),"builds":[{"name":b["name"],"raw_dpr":round(b["dpr"],1),"conservative_dpr":round(b["conservative_dpr"],1),"rounds":round(b["timing"]["avg"],2)} for b in data["builds"]]},ensure_ascii=False,indent=2))


if __name__=="__main__":main()
