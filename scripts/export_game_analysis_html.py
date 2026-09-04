#!/usr/bin/env python3
"""Aggregate current GameXXK simulation evidence into a self-contained HTML analysis."""

from __future__ import annotations

import csv
import hashlib
import json
import math
import statistics
from collections import Counter, defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "docs" / "design" / "2026-09-04-project-design-tables"
COMPOSITION_CSV = ROOT / "Saved" / "BalanceObservation" / "party_composition_20260904_current" / "cases.csv"
ORTHOGONAL_CSV = ROOT / "Saved" / "BalanceObservation" / "orthogonal_20260904_current" / "cases.csv"
CARD_JSON = ROOT / "docs" / "design" / "2026-09-03-all-card-text-review" / "card-texts.json"

ROLE_CN = {
    "Blade": "刀客", "Guard": "守卫", "Healer": "药师", "Hunter": "弓手",
    "Sorcerer": "法师", "FormationMaster": "阵师",
}
NPC_CN = {
    "Npc.TusiChief": "土司首领", "Npc.SongJinBao": "宋金宝", "Npc.YueBai": "月白",
    "Npc.ZhouGuangZu": "周光祖", "Npc.JinGui": "金贵", "Npc.QiongMeiEr": "琼么儿",
}
NODE_CN = {"All": "综合", "Battle": "普通战", "Elite": "精英战", "Boss": "首领战"}
EQUIPMENT_CN = {
    "MixedNoBonus": "六件混搭（无套装）", "PoJun": "破军", "XuanJia": "玄甲",
    "QingNang": "青囊", "ZhuiFeng": "追风", "ShiGu": "蚀骨", "ShanHe": "山河",
}
TERRAIN_CN = {"Plain": "平原", "Cliff": "山崖", "Forest": "林地", "WaterShore": "水岸", "Village": "村落", "Cave": "洞穴"}
ARCHETYPE_CN = {
    "Archetype.Blade.BloodEdge": "血刃", "Archetype.Blade.MomentumBreak": "断势",
    "Archetype.Blade.Counterflow": "游刃", "Archetype.Blade.Sheathed": "藏锋",
    "Archetype.Guard.ArmorGrowth": "叠甲", "Archetype.Guard.Protection": "护援",
    "Archetype.Guard.ArmorConversion": "转甲", "Archetype.Guard.ArmorRelease": "释甲",
    "Archetype.Healer.Medicine": "药方", "Archetype.Healer.ToxicExplosion": "毒爆",
    "Archetype.Hunter.BleedVolley": "流血齐射", "Archetype.Hunter.HeavyArrow": "重箭",
    "Archetype.Hunter.ToxicArrow": "毒箭", "Archetype.Hunter.AgilityCycle": "灵动循环",
    "Archetype.Sorcerer.FireSequence": "炎法", "Archetype.Sorcerer.IceSequence": "寒冰",
    "Archetype.Sorcerer.LightningSequence": "雷法", "Archetype.Sorcerer.GeneralTask": "通用任务",
}

NUMERIC = {
    "rounds", "remaining_party_health", "first_round_deaths", "active_cards", "automatic_resolutions",
    "energy_spent", "energy_gained", "mana_spent", "mana_gained", "energy_unspent_at_phase_end",
    "mana_unspent_at_phase_end", "healing_generated", "armor_generated", "overkill_damage", "overhealing",
}
MAP_FIELDS = {"damage_by_source", "damage_by_card_id", "cards_played_by_id", "healing_by_card_id", "armor_by_card_id"}


def parse_map(value: str) -> dict[str, int]:
    result = {}
    for item in filter(None, value.split(";")):
        key, raw = item.rsplit("=", 1)
        result[key] = int(raw)
    return result


def read_rows(path: Path) -> list[dict]:
    with path.open("r", encoding="utf-8", newline="") as source:
        rows = list(csv.DictReader(source))
    for row in rows:
        for field in NUMERIC:
            row[field] = int(row[field])
        for field in MAP_FIELDS:
            row[field] = parse_map(row[field])
        row["win"] = row["outcome"] == "Victory"
        damage = row["damage_by_source"]
        row["hero_damage"] = damage.get("Player", 0)
        row["partner_damage"] = sum(v for k, v in damage.items() if k.startswith("CompanionInstance."))
        row["npc_damage"] = sum(v for k, v in damage.items() if k.startswith("Npc."))
        row["friendly_damage"] = row["hero_damage"] + row["partner_damage"] + row["npc_damage"]
    return rows


def percentile(values: list[int], p: float) -> int:
    ordered = sorted(values)
    return ordered[max(0, math.ceil(len(ordered) * p) - 1)] if ordered else 0


def aggregate(rows: list[dict], key_func) -> list[dict]:
    buckets = defaultdict(list)
    for row in rows:
        buckets[key_func(row)].append(row)
    out = []
    for key, group in buckets.items():
        rounds = [r["rounds"] for r in group]
        total_rounds = sum(rounds)
        friendly = sum(r["friendly_damage"] for r in group)
        hero = sum(r["hero_damage"] for r in group)
        partner = sum(r["partner_damage"] for r in group)
        npc = sum(r["npc_damage"] for r in group)
        out.append({
            "key": key,
            "cases": len(group),
            "wins": sum(r["win"] for r in group),
            "win_rate": sum(r["win"] for r in group) / len(group),
            "avg_rounds": statistics.fmean(rounds),
            "median_rounds": statistics.median(rounds),
            "p90_rounds": percentile(rounds, .9),
            "avg_remaining_hp": statistics.fmean(r["remaining_party_health"] for r in group),
            "first_round_death_rate": sum(r["first_round_deaths"] > 0 for r in group) / len(group),
            "avg_damage": friendly / len(group),
            "damage_per_round": friendly / total_rounds if total_rounds else 0,
            "hero_damage_per_round": hero / total_rounds if total_rounds else 0,
            "partner_damage_per_round": partner / total_rounds if total_rounds else 0,
            "npc_damage_per_round": npc / total_rounds if total_rounds else 0,
            "avg_healing": statistics.fmean(r["healing_generated"] for r in group),
            "avg_armor": statistics.fmean(r["armor_generated"] for r in group),
            "avg_energy_net": statistics.fmean(r["energy_gained"] - r["energy_spent"] for r in group),
            "avg_mana_net": statistics.fmean(r["mana_gained"] - r["mana_spent"] for r in group),
            "avg_overkill": statistics.fmean(r["overkill_damage"] for r in group),
        })
    return out


def score(rows: list[dict]) -> list[dict]:
    if not rows: return rows
    max_dpr = max(r["damage_per_round"] for r in rows) or 1
    min_rounds = min(r["avg_rounds"] for r in rows) or 1
    max_hp = max(r["avg_remaining_hp"] for r in rows) or 1
    for row in rows:
        row["score"] = 50 * row["win_rate"] + 25 * row["damage_per_round"] / max_dpr + 15 * min_rounds / row["avg_rounds"] + 10 * row["avg_remaining_hp"] / max_hp
    return sorted(rows, key=lambda r: (-r["score"], -r["win_rate"], -r["damage_per_round"], r["avg_rounds"]))


def chinese_composition(row: dict) -> None:
    partner, npc = row["key"].split("+", 1)
    row["partner"] = partner
    row["partner_cn"] = ROLE_CN[partner]
    row["npc"] = "Npc." + npc
    row["npc_cn"] = NPC_CN[row["npc"]]
    row["name"] = f"标准主角＋{row['partner_cn']}＋{row['npc_cn']}"


def by_node(rows: list[dict], key_func, translate=None) -> dict[str, list[dict]]:
    result = {}
    for node in ("All", "Battle", "Elite", "Boss"):
        subset = rows if node == "All" else [r for r in rows if r["node"] == node]
        values = score(aggregate(subset, key_func))
        if translate:
            for value in values: translate(value)
        result[node] = values
    return result


def source_hash(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build_data() -> dict:
    comp = read_rows(COMPOSITION_CSV)
    ortho = read_rows(ORTHOGONAL_CSV)
    cards = json.loads(CARD_JSON.read_text(encoding="utf-8"))["cards"]
    names = {c["id"]: c["name"] for c in cards}
    groups = {c["id"]: c["group"] for c in cards}

    compositions = by_node(comp, lambda r: r["variant"], chinese_composition)
    roles = by_node(comp, lambda r: r["companion_role"])
    for values in roles.values():
        for row in values:
            row["name"] = ROLE_CN.get(row["key"], row["key"])
    npcs = by_node(comp, lambda r: r["quest_npc"])
    for values in npcs.values():
        for row in values: row["name"] = NPC_CN.get(row["key"], row["key"])

    archetypes = score(aggregate(comp, lambda r: f"{r['companion_role']}|{r['companion_primary_archetype']}"))
    for row in archetypes:
        role, archetype = row["key"].split("|", 1)
        row["role"] = role
        row["role_cn"] = ROLE_CN.get(role, role)
        row["archetype"] = archetype
        row["name"] = ARCHETYPE_CN.get(archetype, archetype.rsplit(".", 1)[-1])

    equipment_rows = [r for r in ortho if r["dimension"] == "EquipmentSet"]
    equipment = by_node(equipment_rows, lambda r: r["variant"])
    for values in equipment.values():
        for row in values:
            row["name"] = EQUIPMENT_CN.get(row["key"], row["key"])
            row["status"] = "待评审描述符" if row["key"] in {"XuanJia", "ShanHe"} else "当前可用"
    terrain_rows = [r for r in ortho if r["dimension"] == "Terrain"]
    terrains = by_node(terrain_rows, lambda r: r["variant"])
    for values in terrains.values():
        for row in values: row["name"] = TERRAIN_CN.get(row["key"], row["key"])

    damage = Counter(); plays = Counter(); healing = Counter(); armor = Counter()
    for row in comp:
        damage.update(row["damage_by_card_id"])
        plays.update(row["cards_played_by_id"])
        healing.update(row["healing_by_card_id"])
        armor.update(row["armor_by_card_id"])
    card_stats = []
    for card_id in sorted(set(damage) | set(healing) | set(armor) | set(plays)):
        card_stats.append({
            "id": card_id, "name": names.get(card_id, card_id), "group": groups.get(card_id, ""),
            "plays": plays[card_id], "damage": damage[card_id],
            "damage_per_play": damage[card_id] / plays[card_id] if plays[card_id] else 0,
            "healing": healing[card_id], "armor": armor[card_id],
        })
    top_cards = sorted(card_stats, key=lambda r: (-r["damage"], -r["damage_per_play"]))[:24]
    per_play_cards = sorted((r for r in card_stats if r["plays"] >= 30), key=lambda r: (-r["damage_per_play"], -r["damage"]))[:24]

    best_equipment = equipment["All"][0]["name"]
    role_gear = {
        "Blade": "破军（冲锋/收招闭环）", "Guard": "高生命/防御词缀；玄甲待规则落地",
        "Healer": "青囊（高费牌循环）", "Hunter": "蚀骨（DoT）或追风（重箭循环）",
        "Sorcerer": "蚀骨（炎法）或追风（雷/通用）", "FormationMaster": "当前用追风/青囊；山河待消费者落地",
    }
    npc_gear = {
        "Npc.TusiChief": "破军/高护甲词缀", "Npc.SongJinBao": "破军/追风",
        "Npc.YueBai": "蚀骨；山河待规则落地", "Npc.ZhouGuangZu": "青囊；山河待规则落地",
        "Npc.JinGui": "追风/高护甲词缀", "Npc.QiongMeiEr": "蚀骨/青囊",
    }
    composition_rank = {row["key"]: index for index, row in enumerate(compositions["All"], 1)}
    composition_by_key = {row["key"]: row for row in compositions["All"]}
    thematic_pairs = [
        ("综合最优", "Hunter+SongJinBao"),
        ("流血／毒爆", "Hunter+QiongMeiEr"),
        ("冲锋收招", "Blade+SongJinBao"),
        ("护甲格挡", "Guard+TusiChief"),
        ("药效与地势", "Healer+ZhouGuangZu"),
        ("高风险法术爆发", "Sorcerer+YueBai"),
        ("地势联动", "FormationMaster+YueBai"),
    ]
    recommendations = []
    for theme, key in thematic_pairs:
        row = composition_by_key[key]
        recommendations.append({
            "theme": theme,
            "rank": composition_rank[key],
            "name": row["name"],
            "score": row["score"],
            "win_rate": row["win_rate"],
            "dpr": row["damage_per_round"],
            "rounds": row["avg_rounds"],
            "hero": f"主角使用标准低费过牌骨架；主角六件套实测首选：{best_equipment}",
            "partner": f"{row['partner_cn']}：{role_gear[row['partner']]}",
            "npc": f"{row['npc_cn']}：{npc_gear[row['npc']]}",
            "flow": flow_text(row["partner"], row["npc"]),
        })

    role_all = roles["All"]
    equip_all = equipment["All"]
    best_npc_by_role = {}
    for role in ROLE_CN:
        candidates = [r for r in compositions["All"] if r["partner"] == role]
        best_npc_by_role[role] = candidates[0]["npc_cn"]
    equip_gap = (equip_all[0]["damage_per_round"] / equip_all[1]["damage_per_round"] - 1) * 100
    sorcerer = next(r for r in role_all if r["key"] == "Sorcerer")
    formation = next(r for r in role_all if r["key"] == "FormationMaster")
    findings = [
        {"level": "high", "title": "弓手是当前自动策略的显著领先职业",
         "text": f"综合胜率{role_all[0]['win_rate']*100:.1f}%，每回合伤害{role_all[0]['damage_per_round']:.1f}；流血齐射是出生流派第一。"},
        {"level": "high", "title": "宋金宝在六个伙伴职业中都成为评分最高NPC",
         "text": "；".join(f"{ROLE_CN[k]}→{v}" for k, v in best_npc_by_role.items()) + "。这更像通用资源/检索过强信号，而非单一职业协同。"},
        {"level": "high", "title": "追风六件套在当前主角正交矩阵中形成大幅领先",
         "text": f"每回合伤害{equip_all[0]['damage_per_round']:.1f}，比第二名高{equip_gap:.1f}%；需要结合手操与不同队伍复核出牌计数收益。"},
        {"level": "medium", "title": "法师呈现高伤害、低胜率的高风险特征",
         "text": f"每回合伤害{sorcerer['damage_per_round']:.1f}，但胜率仅{sorcerer['win_rate']*100:.1f}%；资源顺序和生存是主要风险。"},
        {"level": "medium", "title": "阵师的价值偏支持，当前战斗耗时最长",
         "text": f"平均{formation['avg_rounds']:.2f}回合；山河套消费者未完成，因此这不是完整地势体系上限。"},
    ]

    return {
        "meta": {
            "composition_cases": len(comp), "orthogonal_cases": len(ortho), "composition_count": 36,
            "level": 10, "chapter": 2, "terrain": "平原", "equipment": "无装备",
            "composition_csv": str(COMPOSITION_CSV.relative_to(ROOT)), "orthogonal_csv": str(ORTHOGONAL_CSV.relative_to(ROOT)),
            "composition_sha256": source_hash(COMPOSITION_CSV), "orthogonal_sha256": source_hash(ORTHOGONAL_CSV),
            "pending_cards": ["Profession.Sorcerer.RanLingHuanYuan", "Npc.JinGui.HouXiangTuoShen"],
            "score_formula": "50×胜率 + 25×相对每回合伤害 + 15×相对速度 + 10×相对剩余生命",
        },
        "compositions": compositions, "roles": roles, "npcs": npcs, "archetypes": archetypes,
        "equipment": equipment, "terrains": terrains, "top_cards": top_cards,
        "per_play_cards": per_play_cards, "recommendations": recommendations, "findings": findings,
    }


def flow_text(role: str, npc: str) -> str:
    role_text = {
        "Blade": "以冲锋→主动牌→收招串起重放与反应伤害",
        "Guard": "先建立护甲/守护，再用格挡与转甲牌换取稳定输出",
        "Healer": "通过真实生命变化积累药效，再用治疗或毒爆结算",
        "Hunter": "先生成流血/中毒/标记与蓄力，再由重箭多段消费",
        "Sorcerer": "按同流派顺序完成五牌任务，以炎/冰/雷分支集中爆发",
        "FormationMaster": "先选地势并保持收益牌密度，用地势双触发放大全队",
    }[role]
    npc_text = {
        "Npc.TusiChief": "土司首领补护甲、格挡、气势和协战",
        "Npc.SongJinBao": "宋金宝提供0费检索、气势、标记和法术小任务",
        "Npc.YueBai": "月白提供灼烧、标记、检索和地势收益",
        "Npc.ZhouGuangZu": "周光祖提供药效、治疗、毒爆和地势收益",
        "Npc.JinGui": "金贵提供标记、蓄力、过牌和护甲保护",
        "Npc.QiongMeiEr": "琼么儿提供蓄力、DoT、毒爆和团队治疗",
    }[npc]
    return f"{role_text}；{npc_text}。"


HTML = r'''<!doctype html>
<html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>GameXXK 职业、配队与伤害期望分析</title>
<style>
:root{--ink:#251b13;--muted:#745d48;--paper:#f1e2c3;--paper2:#e5cfaa;--line:#aa8860;--red:#a13b2b;--green:#37624b;--blue:#355f78;--gold:#c9993c;--shadow:0 16px 42px #1c120d33}*{box-sizing:border-box}body{margin:0;color:var(--ink);background:#19130f;font:15px/1.55 "Microsoft YaHei","Noto Sans SC",sans-serif}body:before{content:"";position:fixed;inset:0;background:radial-gradient(circle at 20% 0,#65482b55,transparent 38%),linear-gradient(120deg,#15100d,#2a1d14);z-index:-1}.page{max-width:1520px;margin:auto;padding:30px}.hero,.panel{background:linear-gradient(135deg,#f6e9cc,#e7d0a7);border:1px solid #c09b6e;border-radius:18px;box-shadow:var(--shadow)}.hero{padding:34px;margin-bottom:20px;position:relative;overflow:hidden}.hero:after{content:"数据依据：真实规则自动模拟";position:absolute;right:28px;top:24px;color:#7d5b35;font-weight:700}.hero h1{font:800 34px/1.2 KaiTi,"STKaiti",serif;margin:0 0 12px}.hero p{max-width:930px;color:var(--muted);margin:0}.kpis{display:grid;grid-template-columns:repeat(5,1fr);gap:12px;margin-top:24px}.kpi{padding:14px;border-radius:12px;background:#fff8e9a8;border:1px solid #d2b48a}.kpi strong{display:block;font-size:26px;color:var(--red)}.grid{display:grid;grid-template-columns:repeat(12,1fr);gap:18px}.panel{padding:22px;margin-bottom:18px}.span12{grid-column:span 12}.span8{grid-column:span 8}.span6{grid-column:span 6}.span4{grid-column:span 4}h2{font:800 24px KaiTi,"STKaiti",serif;margin:0 0 14px;border-bottom:2px solid #a67946;padding-bottom:9px}h3{margin:10px 0;color:#5a3c24}.controls{display:flex;gap:12px;flex-wrap:wrap;margin-bottom:14px}select{background:#fff8e9;border:1px solid #a98258;border-radius:8px;padding:8px 12px;color:var(--ink)}table{width:100%;border-collapse:collapse;font-size:13px}th{background:#4b3424;color:#fff3da;position:sticky;top:0}th,td{padding:8px 9px;border-bottom:1px solid #c9a77d;text-align:right;vertical-align:top}th:first-child,td:first-child{text-align:left}tbody tr:hover{background:#fff8e999}.table-wrap{max-height:620px;overflow:auto;border:1px solid #c6a479;border-radius:10px}.heat{display:grid;grid-template-columns:110px repeat(6,1fr);gap:4px}.heat>div{padding:10px 6px;text-align:center;border-radius:7px;background:#fff8e9}.heat .label{font-weight:800;background:#d7b98e}.bar-row{display:grid;grid-template-columns:150px 1fr 90px;gap:10px;align-items:center;margin:9px 0}.track{height:18px;background:#d7c3a3;border-radius:10px;overflow:hidden}.fill{height:100%;background:linear-gradient(90deg,var(--red),var(--gold));border-radius:10px}.cards{display:grid;grid-template-columns:repeat(3,1fr);gap:14px}.rec{background:#fff8e9;border:1px solid #c9a77d;border-radius:12px;padding:16px}.rec .rank{font-size:12px;color:var(--muted)}.rec h3{font-size:18px}.tag{display:inline-block;background:#5c4735;color:#fff5df;border-radius:999px;padding:3px 9px;margin:2px;font-size:12px}.note{padding:12px 14px;border-left:4px solid var(--gold);background:#fff8e9;color:#654d38;border-radius:8px}.warn{border-left-color:var(--red)}.shares{height:12px;display:flex;border-radius:8px;overflow:hidden;background:#ddd}.shares i:nth-child(1){background:#a13b2b}.shares i:nth-child(2){background:#355f78}.shares i:nth-child(3){background:#568057}footer{color:#cdbb9f;text-align:center;padding:25px}@media(max-width:1000px){.span8,.span6,.span4{grid-column:span 12}.kpis{grid-template-columns:repeat(2,1fr)}.cards{grid-template-columns:1fr}.page{padding:12px}.heat{grid-template-columns:82px repeat(6,110px);overflow:auto}}
</style></head><body><main class="page">
<section class="hero"><h1>GameXXK 职业、队伍配装与伤害期望</h1><p>使用当前战斗适配器和 Skilled 自动决策策略，直接跑真实卡牌、敌人、状态与资源规则。排名用于比较当前版本的相对表现；它不是手操上限，也不把未完成的玄甲/山河描述符当作已生效能力。</p><div class="kpis" id="kpis"></div></section>
<div class="grid">
<section class="panel span12"><h2>当前结论</h2><div class="cards" id="topCards"></div></section>
<section class="panel span12"><h2>平衡观察</h2><div class="cards" id="findings"></div></section>
<section class="panel span12"><h2>36组主角＋伙伴＋NPC热力图</h2><div class="controls"><label>节点 <select id="node"><option value="All">综合</option><option value="Battle">普通战</option><option value="Elite">精英战</option><option value="Boss">首领战</option></select></label><label>指标 <select id="metric"><option value="score">综合评分</option><option value="win_rate">胜率</option><option value="damage_per_round">每回合伤害</option><option value="avg_rounds">平均回合（越低越好）</option><option value="avg_remaining_hp">剩余生命</option></select></label></div><div class="heat" id="heatmap"></div></section>
<section class="panel span8"><h2>组合完整排名</h2><div class="table-wrap"><table><thead><tr><th>队伍</th><th>评分</th><th>胜率</th><th>伤害/回合</th><th>平均回合</th><th>剩余生命</th><th>首回合减员</th><th>伤害来源</th></tr></thead><tbody id="rankRows"></tbody></table></div></section>
<section class="panel span4"><h2>指标条形图</h2><div id="bars"></div><p class="note">伤害/回合按有效友方伤害总和÷战斗回合总和计算，包含主角、伙伴、NPC及其反应伤害。</p></section>
<section class="panel span6"><h2>伙伴职业统计</h2><div class="table-wrap"><table><thead><tr><th>职业</th><th>胜率</th><th>伤害/回合</th><th>回合</th><th>治疗</th><th>护甲</th></tr></thead><tbody id="roleRows"></tbody></table></div></section>
<section class="panel span6"><h2>任务NPC统计</h2><div class="table-wrap"><table><thead><tr><th>NPC</th><th>胜率</th><th>伤害/回合</th><th>回合</th><th>治疗</th><th>护甲</th></tr></thead><tbody id="npcRows"></tbody></table></div></section>
<section class="panel span12"><h2>伙伴出生流派表现</h2><div class="table-wrap"><table><thead><tr><th>职业</th><th>流派</th><th>样本</th><th>胜率</th><th>伤害/回合</th><th>平均回合</th><th>剩余生命</th></tr></thead><tbody id="archRows"></tbody></table></div></section>
<section class="panel span6"><h2>主角六件套正交表现</h2><div class="table-wrap"><table><thead><tr><th>装备</th><th>状态</th><th>胜率</th><th>伤害/回合</th><th>回合</th><th>剩余生命</th></tr></thead><tbody id="equipRows"></tbody></table></div><p class="note warn">装备矩阵固定刀客＋土司首领，只对主角换六件套；玄甲和山河仍缺完整战斗消费者，其排名不能视为最终设计强度。</p></section>
<section class="panel span6"><h2>地势正交表现</h2><div class="table-wrap"><table><thead><tr><th>地势</th><th>胜率</th><th>伤害/回合</th><th>回合</th><th>剩余生命</th></tr></thead><tbody id="terrainRows"></tbody></table></div></section>
<section class="panel span12"><h2>推荐配队与配装</h2><div class="cards" id="recommendations"></div></section>
<section class="panel span6"><h2>累计伤害最高的卡</h2><div class="table-wrap"><table><thead><tr><th>卡牌</th><th>归属</th><th>打出</th><th>总伤害</th><th>伤害/次</th></tr></thead><tbody id="cardRows"></tbody></table></div></section>
<section class="panel span6"><h2>单次伤害期望最高的卡</h2><div class="table-wrap"><table><thead><tr><th>卡牌</th><th>归属</th><th>打出</th><th>总伤害</th><th>伤害/次</th></tr></thead><tbody id="perPlayRows"></tbody></table></div></section>
<section class="panel span12"><h2>统计口径与限制</h2><div id="method"></div></section>
</div><footer>GameXXK 当前代码与设计表自动生成 · 2026-09-04</footer></main>
<script>const D=__DATA__;
const fmt=(v,n=1)=>Number(v).toFixed(n), pct=v=>fmt(v*100,1)+'%', esc=s=>String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
function rowHtml(r,type){let total=r.hero_damage_per_round+r.partner_damage_per_round+r.npc_damage_per_round||1;return `<tr><td>${esc(r.name)}</td><td>${fmt(r.score||0)}</td><td>${pct(r.win_rate)}</td><td>${fmt(r.damage_per_round)}</td><td>${fmt(r.avg_rounds)}</td><td>${fmt(r.avg_remaining_hp)}</td><td>${pct(r.first_round_death_rate)}</td><td><div class="shares" title="主角 ${fmt(r.hero_damage_per_round)} / 伙伴 ${fmt(r.partner_damage_per_round)} / NPC ${fmt(r.npc_damage_per_round)}"><i style="width:${100*r.hero_damage_per_round/total}%"></i><i style="width:${100*r.partner_damage_per_round/total}%"></i><i style="width:${100*r.npc_damage_per_round/total}%"></i></div></td></tr>`}
function render(){const node=document.querySelector('#node').value, metric=document.querySelector('#metric').value, rows=D.compositions[node];let vals=rows.map(r=>r[metric]), lo=Math.min(...vals),hi=Math.max(...vals);const roles=['Blade','Guard','Healer','Hunter','Sorcerer','FormationMaster'],npcs=['Npc.TusiChief','Npc.SongJinBao','Npc.YueBai','Npc.ZhouGuangZu','Npc.JinGui','Npc.QiongMeiEr'];let h='<div class="label">伙伴\\NPC</div>'+npcs.map(n=>`<div class="label">${D.npcNames[n]}</div>`).join('');for(const role of roles){h+=`<div class="label">${D.roleNames[role]}</div>`;for(const npc of npcs){let r=rows.find(x=>x.partner===role&&x.npc===npc),v=r[metric],t=hi===lo?.5:(metric==='avg_rounds'?(hi-v)/(hi-lo):(v-lo)/(hi-lo)),bg=`hsl(${18+105*t} 52% ${82-25*t}%)`;h+=`<div style="background:${bg}" title="${esc(r.name)}｜胜率${pct(r.win_rate)}｜伤害/回合${fmt(r.damage_per_round)}｜${fmt(r.avg_rounds)}回合">${metric==='win_rate'?pct(v):fmt(v)}</div>`}}document.querySelector('#heatmap').innerHTML=h;document.querySelector('#rankRows').innerHTML=rows.map(rowHtml).join('');let top=rows.slice(0,10),mx=Math.max(...top.map(r=>metric==='avg_rounds'?1/r.avg_rounds:r[metric]));document.querySelector('#bars').innerHTML=top.map(r=>{let v=metric==='avg_rounds'?1/r.avg_rounds:r[metric],label=metric==='win_rate'?pct(r[metric]):fmt(r[metric]);return `<div class="bar-row"><span>${esc(r.partner_cn+'＋'+r.npc_cn)}</span><div class="track"><div class="fill" style="width:${100*v/mx}%"></div></div><b>${label}</b></div>`}).join('');renderSimple('#roleRows',D.roles[node]);renderSimple('#npcRows',D.npcs[node]);renderSimple('#equipRows',D.equipment[node],true);renderTerrain(node)}
function renderSimple(sel,rows,equip=false){document.querySelector(sel).innerHTML=rows.map(r=>`<tr><td>${esc(r.name)}</td>${equip?`<td>${esc(r.status)}</td>`:''}<td>${pct(r.win_rate)}</td><td>${fmt(r.damage_per_round)}</td><td>${fmt(r.avg_rounds)}</td>${equip?`<td>${fmt(r.avg_remaining_hp)}</td>`:`<td>${fmt(r.avg_healing)}</td><td>${fmt(r.avg_armor)}</td>`}</tr>`).join('')}
function renderTerrain(node){document.querySelector('#terrainRows').innerHTML=D.terrains[node].map(r=>`<tr><td>${esc(r.name)}</td><td>${pct(r.win_rate)}</td><td>${fmt(r.damage_per_round)}</td><td>${fmt(r.avg_rounds)}</td><td>${fmt(r.avg_remaining_hp)}</td></tr>`).join('')}
function cardRows(rows){return rows.slice(0,20).map(r=>`<tr><td title="${esc(r.id)}">${esc(r.name)}</td><td>${esc(r.group)}</td><td>${r.plays}</td><td>${r.damage}</td><td>${fmt(r.damage_per_play)}</td></tr>`).join('')}
document.querySelector('#kpis').innerHTML=[['组合模拟',D.meta.composition_cases+'场'],['正交模拟',D.meta.orthogonal_cases+'场'],['队伍组合',D.meta.composition_count+'组'],['固定等级',D.meta.level+'级'],['待确认规则',D.meta.pending_cards.length+'项']].map(x=>`<div class="kpi"><span>${x[0]}</span><strong>${x[1]}</strong></div>`).join('');
let overall=D.compositions.All, highD=[...overall].sort((a,b)=>b.damage_per_round-a.damage_per_round)[0],highW=[...overall].sort((a,b)=>b.win_rate-a.win_rate||a.avg_rounds-b.avg_rounds)[0];document.querySelector('#topCards').innerHTML=[[overall[0],'综合最优'],[highD,'伤害最高'],[highW,'胜率优先']].map(([r,t])=>`<article class="rec"><div class="rank">${t}</div><h3>${esc(r.name)}</h3><span class="tag">评分 ${fmt(r.score)}</span><span class="tag">胜率 ${pct(r.win_rate)}</span><span class="tag">伤害/回合 ${fmt(r.damage_per_round)}</span><p>平均 ${fmt(r.avg_rounds)} 回合，剩余生命 ${fmt(r.avg_remaining_hp)}。</p></article>`).join('');
document.querySelector('#archRows').innerHTML=D.archetypes.map(r=>`<tr><td>${esc(r.role_cn)}</td><td>${esc(r.name)}</td><td>${r.cases}</td><td>${pct(r.win_rate)}</td><td>${fmt(r.damage_per_round)}</td><td>${fmt(r.avg_rounds)}</td><td>${fmt(r.avg_remaining_hp)}</td></tr>`).join('');
document.querySelector('#findings').innerHTML=D.findings.map(r=>`<article class="rec"><div class="rank">${r.level==='high'?'高优先级':'观察项'}</div><h3>${esc(r.title)}</h3><p>${esc(r.text)}</p></article>`).join('');
document.querySelector('#recommendations').innerHTML=D.recommendations.map(r=>`<article class="rec"><div class="rank">${esc(r.theme)} · 组合总榜 #${r.rank} · 评分${fmt(r.score)}</div><h3>${esc(r.name)}</h3><p>${esc(r.flow)}</p><p><b>主角：</b>${esc(r.hero)}<br><b>伙伴：</b>${esc(r.partner)}<br><b>NPC：</b>${esc(r.npc)}</p><span class="tag">胜率 ${pct(r.win_rate)}</span><span class="tag">DPR ${fmt(r.dpr)}</span></article>`).join('');
document.querySelector('#cardRows').innerHTML=cardRows(D.top_cards);document.querySelector('#perPlayRows').innerHTML=cardRows(D.per_play_cards);
document.querySelector('#method').innerHTML=`<p class="note"><b>组合矩阵：</b>标准主角、10级、第二章、平原、无装备；六职业伙伴×六任务NPC×30种子×普通/精英/Boss，共${D.meta.composition_cases}场。伙伴每个种子按真实出生六牌并配置五牌，NPC按真实4选3规则。</p><p class="note"><b>装备/地势正交：</b>来自${D.meta.orthogonal_cases}场当前正交矩阵；一次只改变一个维度。综合评分公式：${esc(D.meta.score_formula)}。</p><p class="note warn"><b>边界：</b>所有组合共享标准主角八牌，因此伙伴＋NPC排名是直接模拟；“主角职业牌选择”仍是机制推荐，不冒充36张主角牌组穷举。雷走倍率、后巷脱身对象仍待确认；玄甲与山河未完成消费者。</p><p><small>组合CSV：${esc(D.meta.composition_csv)}<br>SHA256 ${D.meta.composition_sha256}<br>正交CSV：${esc(D.meta.orthogonal_csv)}<br>SHA256 ${D.meta.orthogonal_sha256}</small></p>`;
D.roleNames=__ROLE_NAMES__;D.npcNames=__NPC_NAMES__;document.querySelector('#node').addEventListener('change',render);document.querySelector('#metric').addEventListener('change',render);render();
</script></body></html>'''


def main():
    data = build_data()
    data["roleNames"] = ROLE_CN
    data["npcNames"] = NPC_CN
    OUT.mkdir(parents=True, exist_ok=True)
    json_path = OUT / "GameXXK_职业配队与伤害期望分析_2026-09-04.json"
    html_path = OUT / "GameXXK_职业配队与伤害期望分析_2026-09-04.html"
    json_text = json.dumps(data, ensure_ascii=False, indent=2)
    json_path.write_text(json_text, encoding="utf-8", newline="\n")
    html = HTML.replace("__DATA__", json.dumps(data, ensure_ascii=False, separators=(",", ":")))
    html = html.replace("__ROLE_NAMES__", json.dumps(ROLE_CN, ensure_ascii=False))
    html = html.replace("__NPC_NAMES__", json.dumps(NPC_CN, ensure_ascii=False))
    html_path.write_text(html, encoding="utf-8", newline="\n")
    print(json.dumps({"html": str(html_path), "json": str(json_path), "top": data["compositions"]["All"][:5]}, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
