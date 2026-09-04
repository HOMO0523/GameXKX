#!/usr/bin/env python3
"""Build a transparent level-100 design projection for the approved Hell 3-1 specification."""

from __future__ import annotations

import json
import math
import re
from itertools import combinations
from pathlib import Path

from export_game_enemy_design_table import build_design_data
from export_game_equipment_design_table import ROLE_STATS


ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/"docs"/"design"/"2026-09-04-project-design-tables"
CARDS=ROOT/"docs"/"design"/"2026-09-03-all-card-text-review"/"card-texts.json"
SPEC=ROOT/"docs"/"superpowers"/"specs"/"2026-09-03-card-monster-progression-rebalance-design.md"
ROLE_CN={"Blade":"刀客","Guard":"守卫","Healer":"药师","Hunter":"弓手","Sorcerer":"法师","FormationMaster":"阵师"}
NPC_CN={"Npc.TusiChief":"土司首领","Npc.SongJinBao":"宋金宝","Npc.YueBai":"月白","Npc.ZhouGuangZu":"周光祖","Npc.JinGui":"金贵","Npc.QiongMeiEr":"琼么儿"}
DUAL={"Npc.TusiChief":("Blade","Guard"),"Npc.SongJinBao":("Sorcerer","Blade"),"Npc.YueBai":("FormationMaster","Sorcerer"),"Npc.ZhouGuangZu":("Healer","FormationMaster"),"Npc.JinGui":("Guard","Hunter"),"Npc.QiongMeiEr":("Hunter","Healer")}
NPC_BASE={
"Npc.TusiChief":(115,11,14,1.4,10,1.0),"Npc.SongJinBao":(88,8,10,1.0,7,.6),"Npc.YueBai":(84,8,15,1.5,6,.5),
"Npc.ZhouGuangZu":(90,8,12,1.2,7,.7),"Npc.JinGui":(92,9,12,1.1,8,.8),"Npc.QiongMeiEr":(96,9,13,1.3,8,.8)}
NAKED={"Hero":(1585,312,206),"Blade":(983,215,75),"Guard":(1308,110,130),"Healer":(882,109,76),"Hunter":(878,214,65),"Sorcerer":(773,163,54),"FormationMaster":(985,130,87)}
FINAL={code:(hp,atk,defense) for code,_,hp,atk,defense in ROLE_STATS}


def npc_stats():
    out={}
    for npc,(b1,g1,b2,g2,b3,g3) in NPC_BASE.items():
        naked=(math.floor(b1+g1*99),math.floor(b2+g2*99),math.floor(b3+g3*99))
        r1,r2=DUAL[npc]
        delta=tuple(round(((FINAL[r1][i]-NAKED[r1][i])+(FINAL[r2][i]-NAKED[r2][i]))/2) for i in range(3))
        final=tuple(naked[i]+delta[i] for i in range(3))
        out[npc]={"name":NPC_CN[npc],"roles":f"{ROLE_CN[r1]}＋{ROLE_CN[r2]}","naked":{"hp":naked[0],"attack":naked[1],"defense":naked[2]},"projected":{"hp":final[0],"attack":final[1],"defense":final[2]},"note":"按NPC百级裸属性，加双职业平均新装备预算；非冻结NPC最终基准"}
    return out


def ceil_pct(value,pct): return math.ceil(max(0,value)*pct/100)


def estimate_variant(variant,attack,enemies):
    immediate=0;packets=0;fixed=0;setup_required="消耗目标全部〔护甲〕" in variant["detail"]
    detail_lines=[]
    for line in variant["detail"].splitlines():
        if line.startswith("〔编序〕") or line.startswith("〔阵赏〕"): break
        if line.startswith("检索失败："): continue
        detail_lines.append(line)
    compact_lines=[]
    for line in variant["compact"].splitlines():
        if line.startswith("〔编序〕") or line.startswith("〔阵赏〕"): break
        compact_lines.append(line)
    for line in detail_lines:
        aoe="全体敌方" in line
        targets=enemies if aoe else [enemies[1]]
        coeffs=[int(x) for x in re.findall(r'(\d+)%的攻击伤害',line)]
        if not coeffs: coeffs=[int(x) for x in re.findall(r'每次造成(\d+)%',line)]
        hit=1
        m=re.search(r'(?:攻击|命中|造成)(\d+)次',line) or re.search(r'[×x](\d+)',line)
        if m: hit=int(m.group(1))
        for coeff in coeffs:
            for enemy in targets:
                per=max(0,math.ceil(attack*coeff/100)-enemy["defense"])
                immediate+=ceil_pct(per,75)*hit;packets+=hit
    if immediate==0:
        for line in compact_lines:
            aoe="全体敌方" in line;count=3 if aoe else 1
            for raw in re.findall(r'造成(\d+)点(?:固定)?伤害',line): fixed+=ceil_pct(int(raw),75)*count
    dot=0
    for line in compact_lines:
        count=3 if "全体敌方" in line else 1
        for raw,_ in re.findall(r'(\d+)点〔(流血|中毒|灼烧|蚀伤)〕',line): dot+=int(raw)*count
    return {"immediate":immediate+fixed,"dot":dot,"packets":packets,"total":immediate+fixed+dot,"setup_required":setup_required}


def package(cards,attack,count,enemies):
    rows=[]
    for card in cards:
        variant=card["variants"][-1]
        est=estimate_variant(variant,attack,enemies)
        energy=int(re.match(r'(\d+)气',variant["cost"]).group(1))
        rows.append({"id":card["id"],"name":card["name"],"quality":variant["quality"],"energy":energy,"compact":variant["compact"],**est})
    rows.sort(key=lambda r:(-r["total"],r["energy"],r["id"]))
    chosen=rows[:count]
    return {"cards":chosen,"immediate":sum(x["immediate"] for x in chosen),"dot":sum(x["dot"] for x in chosen),"total":sum(x["total"] for x in chosen)}


def best_three_energy_burst(cards):
    best={"cards":[],"immediate":0,"dot":0,"total":0,"energy":0}
    for count in range(1,min(5,len(cards))+1):
        for chosen in combinations(cards,count):
            if any(x["setup_required"] for x in chosen): continue
            energy=sum(x["energy"] for x in chosen)
            if energy>3: continue
            immediate=sum(x["immediate"] for x in chosen);dot=sum(x["dot"] for x in chosen);total=immediate+dot
            if (total,-energy)> (best["total"],-best["energy"]):
                best={"cards":[x["name"] for x in chosen],"immediate":immediate,"dot":dot,"total":total,"energy":energy}
    return best


def synergy(role,npc):
    strong={
        ("Blade","Npc.SongJinBao"):"冲锋、收招、气势与检索闭环",("Blade","Npc.TusiChief"):"气势、护甲、格挡与协战",
        ("Guard","Npc.TusiChief"):"护甲与格挡同源",("Guard","Npc.JinGui"):"护甲保护、标记与过牌",
        ("Healer","Npc.ZhouGuangZu"):"药效、治疗、毒爆与地势",("Healer","Npc.QiongMeiEr"):"药效、三DoT和毒爆",
        ("Hunter","Npc.QiongMeiEr"):"蓄力、流血/中毒与重箭",("Hunter","Npc.JinGui"):"蓄力、标记、过牌与保护",
        ("Sorcerer","Npc.YueBai"):"法术任务、灼烧、标记与检索",("Sorcerer","Npc.SongJinBao"):"法术小任务、气势与零费检索",
        ("FormationMaster","Npc.YueBai"):"地势、法术任务与群体收益",("FormationMaster","Npc.ZhouGuangZu"):"地势双触发、治疗与毒爆"}
    return strong.get((role,npc),"无直接双机制闭环；主要提供泛用资源或补足生存")


def build_data():
    enemy=build_design_data();h31end=next(x for x in enemy["hell31"] if x["category"]=="end")
    enemies=h31end["units"];card_data=json.loads(CARDS.read_text(encoding="utf-8"));cards=card_data["cards"]
    pending=set(card_data["meta"]["pending"]);cards=[c for c in cards if c["id"] not in pending]
    role_stats={code:{"name":name,"hp":hp,"attack":atk,"defense":defense} for code,name,hp,atk,defense in ROLE_STATS}
    npcs=npc_stats()
    hero_cards=[c for c in cards if c["group"]=="主角"]
    hero=package(hero_cards,role_stats["Hero"]["attack"],8,enemies)
    role_packages={}
    for role,cn in ROLE_CN.items():
        group=f"伙伴·{cn}" if cn!="阵师" else "伙伴·阵师"
        role_packages[role]=package([c for c in cards if c["group"]==group],role_stats[role]["attack"],5,enemies)
    npc_packages={}
    for npc in NPC_CN:
        npc_packages[npc]=package([c for c in cards if c["id"].startswith(npc+".")],npcs[npc]["projected"]["attack"],3,enemies)
    comps=[]
    for role in ROLE_CN:
        for npc in NPC_CN:
            burst=best_three_energy_burst(hero["cards"]+role_packages[role]["cards"]+npc_packages[npc]["cards"])
            immediate=burst["immediate"];dot=burst["dot"];total=burst["total"]
            baseline=0
            for atk in (role_stats["Hero"]["attack"],role_stats[role]["attack"],npcs[npc]["projected"]["attack"]):
                baseline+=ceil_pct(max(0,atk-enemies[1]["defense"]),75)
            comps.append({"role":role,"role_cn":ROLE_CN[role],"npc":npc,"npc_cn":NPC_CN[npc],"name":f"主角＋{ROLE_CN[role]}＋{NPC_CN[npc]}",
                          "immediate":immediate,"dot":dot,"cycle":total,"burst_energy":burst["energy"],"burst_cards":burst["cards"],"baseline100":baseline,"clear_ratio":total/h31end["raw_phase_hp"],
                          "cycles":h31end["raw_phase_hp"]/total if total else 99,"synergy":synergy(role,npc)})
    comps.sort(key=lambda x:(-x["cycle"],-x["baseline100"],x["name"]))
    for idx,row in enumerate(comps,1): row["rank"]=idx
    return {"meta":{"model":"批准设计期望模型（非旧运行时胜率）","party_level":100,"enemy_level":125,"stage":"地狱3-1","enemy_damage_multiplier":150,
                    "stage_end_phase_hp":h31end["raw_phase_hp"],"level_damage_multiplier":75,"cards":173,"pending":card_data["meta"]["pending"]},
            "role_stats":role_stats,"npcs":npcs,"hero_package":hero,"role_packages":role_packages,"npc_packages":npc_packages,
            "compositions":comps,"hell31":enemy["hell31"],"stage_end":h31end}


HTML=r'''<!doctype html><html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>GameXXK 百级地狱3-1职业配队期望</title><style>
:root{--ink:#291d13;--paper:#f0dfbc;--paper2:#fff5df;--line:#b88e5c;--red:#9d392b;--green:#3d7254;--dark:#1b1410}*{box-sizing:border-box}body{margin:0;background:radial-gradient(circle at top,#5b3e25,#18110d 48%);color:var(--ink);font:15px/1.55 "Microsoft YaHei",sans-serif}.page{max-width:1500px;margin:auto;padding:24px}.hero,.panel{background:linear-gradient(135deg,#f6e9cc,#e5c99a);border:1px solid #c29a67;border-radius:18px;box-shadow:0 14px 38px #0005}.hero{padding:32px;margin-bottom:18px}h1,h2{font-family:KaiTi,serif}h1{margin:0;font-size:34px}h2{border-bottom:2px solid #a6763f;padding-bottom:8px}.note{background:#fff7e7;border-left:4px solid #c98c35;padding:12px;border-radius:8px}.warn{border-left-color:var(--red)}.kpis,.cards{display:grid;grid-template-columns:repeat(5,1fr);gap:12px;margin-top:18px}.kpi,.card{background:#fff8e9c9;border:1px solid #cfad7c;border-radius:11px;padding:14px}.kpi b{display:block;color:var(--red);font-size:25px}.grid{display:grid;grid-template-columns:repeat(12,1fr);gap:16px}.panel{padding:20px;grid-column:span 12}.half{grid-column:span 6}.third{grid-column:span 4}.heat{display:grid;grid-template-columns:100px repeat(6,1fr);gap:4px}.heat div{padding:9px;text-align:center;border-radius:6px}.label{background:#68482f!important;color:#fff;font-weight:bold}.controls{display:flex;gap:12px;margin-bottom:12px}select{padding:7px 10px;background:#fff7e8;border:1px solid #a77c4d;border-radius:7px}table{width:100%;border-collapse:collapse;font-size:13px}th,td{padding:8px;border-bottom:1px solid #c6a171;text-align:right;vertical-align:top}th{background:#4a3323;color:#fff3d7;position:sticky;top:0}th:first-child,td:first-child{text-align:left}.wrap{max-height:570px;overflow:auto;border:1px solid #bd9565;border-radius:8px}.tag{display:inline-block;background:#59412e;color:#fff4db;border-radius:20px;padding:3px 9px;margin:2px}.card h3{margin:5px 0}.bar{height:11px;background:#d8bf98;border-radius:7px;overflow:hidden}.bar i{display:block;height:100%;background:linear-gradient(90deg,#9d392b,#d2a446)}footer{text-align:center;color:#d5c1a0;padding:26px}@media(max-width:900px){.half,.third{grid-column:span 12}.kpis,.cards{grid-template-columns:1fr 1fr}.heat{overflow:auto;grid-template-columns:85px repeat(6,110px)}}
</style></head><body><main class="page"><section class="hero"><h1>GameXXK 百级角色 · 地狱3-1配队与伤害期望</h1><p>采用批准的新装备百级最终属性、173张有效牌、125级地狱3-1固定编制和多阶段规则。当前多阶段运行时尚未迁移，因此这里展示可复算的设计期望，不再引用旧10级无装备模拟胜率。</p><div class="kpis" id="kpis"></div></section><div class="grid">
<section class="panel"><h2>结论与边界</h2><div class="cards" id="top"></div><p class="note warn">排名先从“主角8张＋伙伴5张＋NPC 4选3”的最高品质候选中选牌，再求共享3气、最多5张牌的理论爆发：即时攻击伤害＋DOT生成后触发一次。未计抽牌概率、内力不足、任务前置、护甲、治疗和阶段转场，因此它是上限预算，不是胜率。</p></section>
<section class="panel"><h2>36组队伍热力图</h2><div class="controls"><label>指标 <select id="metric"><option value="cycle">3气爆发预算</option><option value="immediate">即时伤害</option><option value="dot">DOT潜力</option><option value="baseline100">三人各100%攻击</option><option value="cycles">清关理论爆发次数（越低越好）</option></select></label></div><div class="heat" id="heat"></div></section>
<section class="panel half"><h2>组合排名</h2><div class="wrap"><table><thead><tr><th>队伍</th><th>3气爆发</th><th>即时</th><th>DOT</th><th>阶段生命占比</th><th>理论次数</th></tr></thead><tbody id="rank"></tbody></table></div></section>
<section class="panel half"><h2>百级新装备角色属性</h2><div class="wrap"><table><thead><tr><th>角色</th><th>生命</th><th>攻击</th><th>防御</th><th>对白猿100%攻击</th></tr></thead><tbody id="stats"></tbody></table></div><p class="note">七个职业数值已经包含六件至宝装备与12颗至宝宝石4攻/4防/4血。</p></section>
<section class="panel half"><h2>伙伴五牌伤害包</h2><div id="roles"></div></section><section class="panel half"><h2>NPC三牌伤害包</h2><div id="npcs"></div><p class="note warn">NPC装备后最终属性未在规格冻结；此处用NPC百级裸属性＋双职业平均装备预算投影，单独标明，不当作最终角色表。</p></section>
<section class="panel"><h2>地狱3-1七组固定编制</h2><div class="wrap"><table><thead><tr><th>类型</th><th>编制</th><th>左/中/右125级</th><th>阶段数</th><th>总阶段生命</th></tr></thead><tbody id="forms"></tbody></table></div></section>
<section class="panel"><h2>推荐流派</h2><div class="cards" id="recs"></div></section>
<section class="panel"><h2>计算口径</h2><p class="note">玩家100级攻击125级敌人，等级差为−25，直接/固定伤害在防御后按75%向上取整；地狱150%只用于敌方对玩家的伤害。地狱3-1关底为秃鹫－白猿－巨蟾，阶段数1/3/1，总原始阶段生命11178。</p><p class="note">卡牌取各自最高合法品质；攻击倍率从详述读取，DOT从简述读取。全体攻击按三目标计算；不明动态次数按一次保守计算。两项未决卡仍保留：雷走八方倍率、后巷脱身对象。</p></section>
</div><footer>批准设计规格投影 · 2026-09-04</footer></main><script>const D=__DATA__;const f=(x,n=1)=>Number(x).toFixed(n),e=s=>String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
document.querySelector('#kpis').innerHTML=[["我方等级",D.meta.party_level],["敌人等级",D.meta.enemy_level],["关底阶段生命",D.meta.stage_end_phase_hp],["有效卡池",D.meta.cards],["地狱敌伤",D.meta.enemy_damage_multiplier+'%']].map(x=>`<div class="kpi">${x[0]}<b>${x[1]}</b></div>`).join('');
let topRows=D.compositions.slice(0,5);document.querySelector('#top').innerHTML=topRows.map(r=>`<div class="card"><small>理论#${r.rank}</small><h3>${e(r.name)}</h3><span class="tag">${r.cycle}爆发伤害</span><span class="tag">${f(r.clear_ratio*100)}%阶段生命</span><p>${e(r.synergy)}</p><small>${e(r.burst_cards.join('、'))}</small></div>`).join('');
function render(){let metric=document.querySelector('#metric').value,roles=['Blade','Guard','Healer','Hunter','Sorcerer','FormationMaster'],npcs=Object.keys(D.npcs),vals=D.compositions.map(x=>x[metric]),lo=Math.min(...vals),hi=Math.max(...vals),h='<div class="label">伙伴\\NPC</div>'+npcs.map(n=>`<div class="label">${D.npcs[n].name}</div>`).join('');for(let r of roles){h+=`<div class="label">${D.role_stats[r].name}</div>`;for(let n of npcs){let x=D.compositions.find(v=>v.role===r&&v.npc===n),v=x[metric],t=hi===lo?.5:(metric==='cycles'?(hi-v)/(hi-lo):(v-lo)/(hi-lo));h+=`<div title="${e(x.name)}｜${x.cycle}伤害｜${f(x.cycles)}循环" style="background:hsl(${15+105*t} 55% ${84-27*t}%)">${f(v)}</div>`}}document.querySelector('#heat').innerHTML=h}document.querySelector('#metric').addEventListener('change',render);render();
document.querySelector('#rank').innerHTML=D.compositions.map(r=>`<tr><td title="${e(r.synergy)}">#${r.rank} ${e(r.name)}</td><td>${r.cycle}</td><td>${r.immediate}</td><td>${r.dot}</td><td>${f(r.clear_ratio*100)}%</td><td>${f(r.cycles,2)}</td></tr>`).join('');
let ape=D.stage_end.units[1];document.querySelector('#stats').innerHTML=Object.values(D.role_stats).map(r=>{let dmg=Math.ceil(Math.max(0,r.attack-ape.defense)*.75);return `<tr><td>${r.name}</td><td>${r.hp}</td><td>${r.attack}</td><td>${r.defense}</td><td>${dmg}</td></tr>`}).join('');
function packHtml(dict){return Object.entries(dict).map(([k,p])=>`<div class="card"><h3>${D.role_stats[k]?.name||D.npcs[k]?.name}</h3><div class="bar"><i style="width:${Math.min(100,p.total/25)}%"></i></div><p>即时 ${p.immediate}＋DOT ${p.dot}＝<b>${p.total}</b></p><small>${p.cards.map(c=>c.name).join('、')}</small></div>`).join('')}document.querySelector('#roles').innerHTML=packHtml(D.role_packages);document.querySelector('#npcs').innerHTML=packHtml(D.npc_packages);
document.querySelector('#forms').innerHTML=D.hell31.map(x=>`<tr><td>${({ordinary:'普通',elite:'精英',end:'关底'})[x.category]}${x.index}</td><td>${x.notation}</td><td>${x.units.map(u=>u.name+' '+u.hp+'HP').join(' / ')}</td><td>${x.units.map(u=>u.phases).join('/')}</td><td>${x.raw_phase_hp}</td></tr>`).join('');
let thematic=[['直伤循环','Hunter','Npc.SongJinBao'],['流血毒爆','Hunter','Npc.QiongMeiEr'],['冲锋收招','Blade','Npc.SongJinBao'],['护甲格挡','Guard','Npc.TusiChief'],['药效地势','Healer','Npc.ZhouGuangZu'],['法术地势','Sorcerer','Npc.YueBai'],['阵师联动','FormationMaster','Npc.YueBai']];document.querySelector('#recs').innerHTML=thematic.map(([t,r,n])=>{let x=D.compositions.find(v=>v.role===r&&v.npc===n);return `<div class="card"><small>${t} · 理论#${x.rank}</small><h3>${e(x.name)}</h3><p>${e(x.synergy)}</p><span class="tag">${x.cycle}伤害</span><span class="tag">${f(x.cycles,2)}循环</span></div>`}).join('');
</script></body></html>'''


def main():
    d=build_data();OUT.mkdir(parents=True,exist_ok=True);jp=OUT/"GameXXK_职业配队与伤害期望分析_2026-09-04.json";hp=OUT/"GameXXK_职业配队与伤害期望分析_2026-09-04.html"
    jp.write_text(json.dumps(d,ensure_ascii=False,indent=2),encoding="utf-8",newline="\n");hp.write_text(HTML.replace("__DATA__",json.dumps(d,ensure_ascii=False,separators=(',',':'))),encoding="utf-8",newline="\n")
    print(json.dumps({"html":str(hp),"top":d["compositions"][:5],"stage_hp":d["meta"]["stage_end_phase_hp"]},ensure_ascii=False,indent=2))


if __name__=="__main__":main()
