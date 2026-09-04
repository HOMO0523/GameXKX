#!/usr/bin/env python3
"""Export the approved 27-stage, multi-phase monster design from the rebalance specification."""

from __future__ import annotations

import json
import math
import re
from pathlib import Path

from openpyxl import load_workbook

from export_game_design_tables import add_table, new_book, sha


ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/"docs"/"design"/"2026-09-04-project-design-tables"
SPEC=ROOT/"docs"/"superpowers"/"specs"/"2026-09-03-card-monster-progression-rebalance-design.md"
CATALOG=ROOT/"Source"/"GameXXK"/"Private"/"GameXXKEnemyCatalog.cpp"

TIER_CN={"Normal":"普通","Elite":"精英","Boss":"首领"}
DIFF_CN={"Normal":"普通","Hard":"困难","Hell":"地狱"}
DIFF_PHASES={"Normal":1,"Hard":2,"Hell":3}
DIFF_DAMAGE={"Normal":100,"Hard":125,"Hell":150}
NAME_CN={
"Rooster":"公鸡","Goat":"山羊","Weasel":"黄鼬","Civet":"狸猫","Ironfeather":"铁羽斗鸡","Bluehorn":"青角羊王","Money Rat":"金钱鼠",
"Gray Wolf":"灰狼","Boar":"野猪","Macaque":"猕猴","Porcupine":"豪猪","Graymane":"苍鬃狼王","Redtusk":"赤獠猪王","Black Bear":"黑熊",
"Venom Snake":"毒蛇","Wildcat":"山猫","Vulture":"秃鹫","Giant Toad":"巨蟾","White Ape":"白猿","Spiral-Horn Deer":"盘角鹿","Tiger":"老虎"}
NAME_CN.update({"Ironfeather Rooster":"铁羽斗鸡","Bluehorn Goat King":"青角羊王","Graymane Wolf King":"苍鬃狼王","Redtusk Boar King":"赤獠猪王"})
ID_BY_CN={
"公鸡":"Enemy.Ch1.Rooster","山羊":"Enemy.Ch1.Goat","黄鼬":"Enemy.Ch1.Weasel","狸猫":"Enemy.Ch1.Civet","铁羽斗鸡":"Enemy.Ch1.IronfeatherRooster","青角羊王":"Enemy.Ch1.BluehornGoatKing","金钱鼠":"Enemy.Ch1.MoneyRat",
"灰狼":"Enemy.Ch2.GrayWolf","野猪":"Enemy.Ch2.Boar","猕猴":"Enemy.Ch2.Macaque","豪猪":"Enemy.Ch2.Porcupine","苍鬃狼王":"Enemy.Ch2.GraymaneWolfKing","赤獠猪王":"Enemy.Ch2.RedtuskBoarKing","黑熊":"Enemy.Ch2.BlackBear",
"毒蛇":"Enemy.Ch3.VenomSnake","山猫":"Enemy.Ch3.Wildcat","秃鹫":"Enemy.Ch3.Vulture","巨蟾":"Enemy.Ch3.GiantToad","白猿":"Enemy.Ch3.WhiteApe","盘角鹿":"Enemy.Ch3.SpiralHornDeer","老虎":"Enemy.Ch3.Tiger"}
ALIASES={
1:{"R":"公鸡","G":"山羊","W":"黄鼬","C":"狸猫","I":"铁羽斗鸡","B":"青角羊王","M":"金钱鼠"},
2:{"Wf":"灰狼","Bo":"野猪","Ma":"猕猴","P":"豪猪","Gm":"苍鬃狼王","Rt":"赤獠猪王","Bb":"黑熊"},
3:{"S":"毒蛇","C":"山猫","V":"秃鹫","T":"巨蟾","A":"白猿","D":"盘角鹿","Ti":"老虎"}}


def split_top(text):
    out=[];start=0;stack=[];quote=False
    pairs={"(":")","{":"}","[":"]"}
    for idx,ch in enumerate(text):
        if ch=='"': quote=not quote
        elif not quote and ch in pairs: stack.append(pairs[ch])
        elif not quote and stack and ch==stack[-1]: stack.pop()
        elif not quote and not stack and ch==',': out.append(text[start:idx].strip());start=idx+1
    if text[start:].strip(): out.append(text[start:].strip())
    return out


def calls(text,marker):
    out=[];pos=0
    while (found:=text.find(marker,pos))>=0:
        start=found+len(marker);depth=1;quote=False;i=start
        while i<len(text):
            ch=text[i]
            if ch=='"': quote=not quote
            elif not quote and ch=='(': depth+=1
            elif not quote and ch==')':
                depth-=1
                if depth==0: break
            i+=1
        out.append(text[start:i]);pos=i+1
    return out


def tv(arg): return re.search(r'TEXT\("([^"]+)"\)',arg).group(1)
def en(arg): return arg.rsplit("::",1)[-1].strip()


def base_enemies():
    rows=[]
    for raw in calls(CATALOG.read_text(encoding="utf-8"),"Definitions.Add(MakeEnemy("):
        a=split_top(raw)
        rows.append({"id":tv(a[0]),"name":tv(a[1]),"chapter":int(a[2]),"tier":en(a[3]),"base_hp":int(a[4]),"hp_per":float(a[5].rstrip('f')),
                     "base_attack":int(a[6]),"attack_per":float(a[7].rstrip('f')),"base_defense":int(a[8]),"defense_per":float(a[9].rstrip('f')),"speed":int(a[10])})
    assert len(rows)==21
    return rows


def stat(enemy,level):
    step=level-1
    rnd=lambda v: math.floor(v+.5)
    return {"hp":enemy["base_hp"]+rnd(enemy["hp_per"]*step),"attack":enemy["base_attack"]+rnd(enemy["attack_per"]*step),"defense":enemy["base_defense"]+rnd(enemy["defense_per"]*step),"speed":enemy["speed"]}


def md_cells(line): return [x.strip() for x in line.strip().strip('|').split('|')]
def body_row(line):
    cells=md_cells(line)
    return line.lstrip().startswith('|') and cells and not all(set(x)<=set('-: ') for x in cells) and not any(x in {"Enemy / intent","Intent","Order","Stage"} or x.endswith("intent") for x in cells[:1])


def section(text,start,end): return text.split(start,1)[1].split(end,1)[0]


def parse_intents(text):
    ordinary=[]
    for line in section(text,"## 10. Ordinary monster intents","## 11. Elite and Boss phase-one intents").splitlines():
        if body_row(line):
            c=md_cells(line)
            if len(c)==2 and " - " in c[0]:
                enemy,intent=c[0].split(" - ",1);ordinary.append([NAME_CN[enemy],intent,c[1]])
    phase1=[];current=None
    phase1_text=section(text,"## 11. Elite and Boss phase-one intents","## 12. Phase-two and phase-three decks")
    keys=["Ironfeather","Bluehorn","Money Rat","Graymane","Redtusk","Black Bear","White Ape","Tiger"]
    for line in phase1_text.splitlines():
        if line.startswith("| Spiral-Horn Deer intent"): current="Spiral-Horn Deer";continue
        for key in keys:
            if line.startswith(f"**{key}"): current=key
        if current and body_row(line):
            c=md_cells(line)
            if len(c)==2 and c[0] not in {"Approved effect"}: phase1.append([NAME_CN[current],c[0],c[1]])
    phase23=[];current=None;phase=0
    for line in section(text,"## 12. Phase-two and phase-three decks","## 13. Authored 27-stage formations").splitlines():
        if line.startswith("#### "): current=line[5:].strip()
        if line.startswith("Phase two") or line.startswith("Phase-two"): phase=2
        if line.startswith("Phase three") or line.startswith("Phase-three"): phase=3
        if current and phase and body_row(line):
            c=md_cells(line)
            if len(c)==3 and c[0].isdigit(): phase23.append([NAME_CN[current],phase,int(c[0]),c[1],c[2]])
    assert len(ordinary)==36 and len(phase1)==42
    assert sum(1 for x in phase23 if x[1]==2)==39 and sum(1 for x in phase23 if x[1]==3)==39
    return ordinary,phase1,phase23


def parse_stages(text):
    rows=[]
    block=section(text,"## 13. Authored 27-stage formations","## 14. Current implementation conflicts")
    for line in block.splitlines():
        if not body_row(line): continue
        c=md_cells(line)
        m=re.fullmatch(r'(Normal|Hard|Hell) ([1-3])-([1-3])',c[0] if c else '')
        if m and len(c)==5:
            diff,chapter,sub=m.group(1),int(m.group(2)),int(m.group(3))
            rows.append({"difficulty":diff,"chapter":chapter,"substage":sub,"level":int(c[1]),"ordinary":c[2].split(' / '),"elite":c[3].split(' / '),"end":c[4]})
    assert len(rows)==27
    return rows


def expand_form(token,chapter): return [ALIASES[chapter][x] for x in token.split('-')]


def phases_for(stage,category,names):
    count=DIFF_PHASES[stage["difficulty"]]
    if count==1:return [1,1,1]
    if category=="ordinary":return [1,1,1]
    if category=="elite":return [1,count,1]
    if stage["substage"]==3:return [count,count,count]
    return [1,count,1]


def build_design_data():
    text=SPEC.read_text(encoding="utf-8");enemies=base_enemies();by_name={e["name"]:e for e in enemies}
    ordinary,p1,p23=parse_intents(text);stages=parse_stages(text)
    formations=[]
    for st in stages:
        for category,tokens in (("ordinary",st["ordinary"]),("elite",st["elite"]),("end",[st["end"]])):
            for index,token in enumerate(tokens,1):
                names=expand_form(token,st["chapter"]);pcs=phases_for(st,category,names);units=[]
                for pos,(name,pc) in enumerate(zip(names,pcs),1):
                    s=stat(by_name[name],st["level"]);units.append({"position":pos,"name":name,"id":ID_BY_CN[name],"phases":pc,**s})
                formations.append({**st,"category":category,"index":index,"notation":token,"units":units,"raw_phase_hp":sum(u["hp"]*u["phases"] for u in units)})
    hell31=[f for f in formations if f["difficulty"]=="Hell" and f["chapter"]==3 and f["substage"]==1]
    hell33end=next(f for f in formations if f["difficulty"]=="Hell" and f["chapter"]==3 and f["substage"]==3 and f["category"]=="end")
    assert len(formations)==189 and hell33end["raw_phase_hp"]==33318
    return {"enemies":enemies,"ordinary_intents":ordinary,"phase1_intents":p1,"phase23_intents":p23,"stages":stages,"formations":formations,"hell31":hell31}


def build(path):
    d=build_design_data();wb=new_book("GameXXK 新怪物与阶段数值设计总表")
    add_table(wb,"00_说明",["项目","内容"],[
        ["权威口径","2026-09-03批准重平衡规格；旧EnemyCatalog意图/阶段与训练池冲突时以规格为准"],
        ["训练等级","普通5-45、困难50-90、地狱95-135，每关+5；地狱3-1为125级，3-3为135级"],
        ["难度伤害","普通100%、困难125%、地狱150%；不额外增加生命或防御"],
        ["阶段","普通额外阶段0；困难精英/首领共2阶段；地狱精英/首领共3阶段；转阶段满血、清负面、换牌组"],
        ["总验证规模","12普通×3意图×3难度=108；9阶段怪一阶段126；二阶段78；三阶段39；合计351"],
        ["实现状态","当前代码仍是旧Boss二阶段和旧池；本表是批准设计，不是当前运行时导出"]],{"项目":25,"内容":120})
    enemy_rows=[]
    for e in d["enemies"]:
        s100,s125,s135=stat(e,100),stat(e,125),stat(e,135)
        enemy_rows.append([e["chapter"],TIER_CN[e["tier"]],e["id"],e["name"],e["base_hp"],e["hp_per"],e["base_attack"],e["attack_per"],e["base_defense"],e["defense_per"],e["speed"],
                           s100["hp"],s100["attack"],s100["defense"],s125["hp"],s125["attack"],s125["defense"],s135["hp"],s135["attack"],s135["defense"]])
    add_table(wb,"01_怪物属性",["章节","级别","怪物ID","名称","基础生命","生命/级","基础攻击","攻击/级","基础防御","防御/级","速度","L100生命","L100攻击","L100防御","L125生命","L125攻击","L125防御","L135生命","L135攻击","L135防御"],enemy_rows,{"怪物ID":35})
    stage_rows=[]
    for s in d["stages"]:
        stage_rows.append([DIFF_CN[s["difficulty"]],f"{s['chapter']}-{s['substage']}",s["level"],DIFF_DAMAGE[s["difficulty"]],DIFF_PHASES[s["difficulty"]]," / ".join(s["ordinary"])," / ".join(s["elite"]),s["end"]])
    add_table(wb,"02_27关总表",["难度","关卡","敌人等级","敌方伤害%","精英/首领总阶段","四组普通编制","两组精英编制","关底编制"],stage_rows,{"四组普通编制":62,"两组精英编制":40,"关底编制":22})
    form_rows=[]
    for f in d["formations"]:
        form_rows.append([DIFF_CN[f["difficulty"]],f"{f['chapter']}-{f['substage']}",f["level"],{"ordinary":"普通","elite":"精英","end":"关底"}[f["category"]],f["index"],f["notation"],
                          " / ".join(u["name"] for u in f["units"])," / ".join(str(u["phases"]) for u in f["units"]),f["raw_phase_hp"]])
    add_table(wb,"03_189编制",["难度","关卡","等级","类型","序号","记号","左/中/右","阶段数","总原始阶段生命"],form_rows,{"左/中/右":38})
    h31=[]
    for f in d["hell31"]:
        h31.append([{"ordinary":"普通","elite":"精英","end":"关底"}[f["category"]],f["index"],f["notation"],
                    *[f"{u['name']}｜{u['hp']}HP {u['attack']}攻 {u['defense']}防｜{u['phases']}阶段" for u in f["units"]],f["raw_phase_hp"]])
    add_table(wb,"04_地狱3-1",["类型","序号","记号","左位125级","中位125级","右位125级","总原始阶段生命"],h31,{"左位125级":34,"中位125级":34,"右位125级":34})
    add_table(wb,"05_普通怪意图",["怪物","意图","普通/困难/地狱批准效果"],d["ordinary_intents"],{"普通/困难/地狱批准效果":110})
    add_table(wb,"06_阶段一意图",["怪物","意图","普通/困难/地狱批准效果"],d["phase1_intents"],{"普通/困难/地狱批准效果":110})
    add_table(wb,"07_阶段二意图",["怪物","阶段","顺序","意图","困难/地狱批准效果"],[x for x in d["phase23_intents"] if x[1]==2],{"困难/地狱批准效果":110})
    add_table(wb,"08_阶段三意图",["怪物","阶段","顺序","意图","地狱批准效果"],[x for x in d["phase23_intents"] if x[1]==3],{"地狱批准效果":110})
    add_table(wb,"09_数值换算",["项目","普通","困难","地狱","说明"],[
        ["敌方伤害倍率","100%","125%","150%","只乘敌方伤害，不乘生命/防御"],["Weak 单体/群体","2/1","3/2","4/3","离散层数"],["Mark 单体/群体","2/1","3/2","5/3","上限5"],["流血系数 单体/群体","3/1","5/3","8/5","100级队伍分别×5生成最终储量"],["中毒系数 单体/群体","3/2","6/4","9/6","100级队伍分别×5"],["灼烧系数 单体/群体","2/1","4/2","6/3","100级队伍分别×5"]],{"说明":72})
    add_table(wb,"10_实现差异",["项目","当前旧实现","批准设计"],[
        ["训练等级","使用玩家等级/旧路线夹具","固定5..135关卡等级；地狱3-1=125"],["难度上下文","未完整进入卡牌战斗","普通/困难/地狱伤害100/125/150%"],["阶段模型","Boss专用bPhaseTwo","九名精英/首领数据驱动1/2/3阶段"],["阶段转换","半血标记并可能改属性","伤害包安全截断、满血新阶段、清负面、保正面、换整套牌"],["编制","平行池临时组合","27关×7=189组固定左中右与开场意图"],["意图数值","旧目录78意图","351个难度/阶段解析用例"],["地狱3-1关底","旧第三章路线普通/精英/Boss夹具","125级 秃鹫-白猿-巨蟾，阶段1/3/1，总阶段生命11178"]],{"当前旧实现":62,"批准设计":90})
    add_table(wb,"11_来源校验",["源文件","SHA256"],[[str(SPEC.relative_to(ROOT)),sha(SPEC)],[str(CATALOG.relative_to(ROOT)),sha(CATALOG)]],{"源文件":88,"SHA256":72})
    wb.save(path)
    return {"enemies":21,"ordinary":36,"phase1":42,"phase2":39,"phase3":39,"formations":189}


def main():
    OUT.mkdir(parents=True,exist_ok=True);path=OUT/"GameXXK_怪物与阶段数值设计总表_2026-09-04.xlsx";counts=build(path)
    wb=load_workbook(path,read_only=True);assert wb["02_27关总表"].max_row==28;assert wb["03_189编制"].max_row==190;assert wb["04_地狱3-1"].max_row==8;wb.close()
    print(json.dumps({"path":str(path),"counts":counts},ensure_ascii=False,indent=2))


if __name__=="__main__":main()
