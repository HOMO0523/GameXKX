"""Reproduce the authored talent graph and calibrate its shared price base.

The candidate is checked against the real C++ catalog by the economy automation
test before acceptance. This script only writes a diagnostics report.
"""
import argparse
import hashlib
import json
import math
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / 'Source/GameXXK/Private/GameXXKTalentCatalog.cpp'


def catalog():
    source = CATALOG.read_text(encoding='utf-8')
    nodes = {}
    def add(key, effect, amount, ranks, tier, previous=None, capacity=0):
        nodes[key] = dict(id=key, effect=effect, amount=amount, ranks=ranks, tier=tier,
                          previous=[] if previous is None else [previous], capacity=capacity)
    add('Talent.Root', 'UnlockWarehousePage', 1, 1, 0)
    entries = {'Combat': ('CombatFoundation', 5), 'CapacityChest': ('BackpackSlots', 5),
               'IdleOffline': ('UnlockOfflineRewards', 1), 'Tools': ('UnlockTools', 1)}
    for branch, (effect, amount) in entries.items():
        add('Talent.Entry.'+branch, effect, amount, 1, 0, 'Talent.Root')
    pattern = r'AddTrack\(Nodes,\s*TEXT\("([^"]+)"\),\s*TEXT\("[^"]*"\),\s*TEXT\("[^"]*"\),\s*EGameXXKTalentBranch::(\w+),\s*EGameXXKTalentEffect::(\w+),\s*EGameXXKTalentIcon::\w+,\s*(\d+),\s*(\d+),\s*[-\d.f]+\)'
    tracks = re.findall(pattern, source)
    assert len(tracks) == 22, len(tracks)
    for prefix, branch, effect, amount, layers in tracks:
        previous = 'Talent.Entry.'+branch
        for layer in range(1, int(layers)+1):
            key = f'{prefix}.{layer:02d}'
            add(key, effect, int(amount), 5, layer, previous)
            previous = key
    for page, tier, capacity in zip(range(3, 7), (5, 15, 25, 35), (50, 100, 150, 200)):
        add(f'Talent.Capacity.WarehousePage.{page:02d}', 'UnlockWarehousePage', 1, 1, tier,
            f'Talent.Capacity.Backpack.{tier:02d}', capacity)
    def seq(prefix, first, last):
        return [f'Talent.{prefix}.{i:02d}' for i in range(first, last+1)]
    def grid(main, pairs):
        for cycle, key in enumerate(main):
            nodes[key]['tier'] = cycle
            nodes[key]['previous'] = ['Talent.Root' if cycle == 0 else main[cycle-1]]
            for axis, chain in enumerate(pairs[cycle][:2]):
                previous = key
                preserve = axis == 1 and len(pairs[cycle]) == 3 and pairs[cycle][2]
                for offset, child in enumerate(chain):
                    nodes[child]['previous'] = [previous]
                    if not preserve:
                        nodes[child]['tier'] = min(35, cycle+1+offset)
                    previous = child
    grid(['Talent.Entry.Combat', 'Talent.Combat.FlatAttack.08', 'Talent.Combat.FlatHealth.08',
          'Talent.Combat.FlatDefense.08', 'Talent.Combat.CriticalDamage.05'], [
        (seq('Combat.FlatAttack', 1, 7), seq('Combat.FlatHealth', 1, 7)),
        (seq('Combat.FlatDefense', 1, 7), seq('Combat.Movement', 1, 1)),
        (seq('Combat.AttackPercent', 1, 10), seq('Combat.FinalDamage', 1, 10)),
        (seq('Combat.DefensePercent', 1, 10), seq('Combat.HealthPercent', 1, 10)),
        (seq('Combat.CriticalChance', 1, 4), seq('Combat.CriticalDamage', 1, 4))])
    grid(['Talent.Entry.CapacityChest', 'Talent.Capacity.Backpack.34', 'Talent.Capacity.Backpack.35'], [
        (seq('Capacity.Backpack', 1, 33), seq('Capacity.WarehousePage', 3, 6), True),
        (seq('Chest.NormalDrop', 1, 35), seq('Chest.AdvancedDrop', 1, 35)),
        (seq('Chest.OfflineTime', 1, 35), [])])
    grid(['Talent.Entry.IdleOffline', 'Talent.Idle.OnlineGold.35', 'Talent.Idle.OnlineExperience.35'], [
        (seq('Idle.OnlineGold', 1, 34), seq('Idle.OnlineExperience', 1, 34)),
        (seq('Idle.OfflineGold', 1, 35), seq('Idle.OfflineExperience', 1, 35)),
        (seq('Idle.OfflineGoldTime', 1, 35), seq('Idle.OfflineExperienceTime', 1, 35))])
    grid(['Talent.Entry.Tools'], [(seq('Tools.Experience', 1, 10), seq('Tools.Gold', 1, 10))])
    return list(nodes.values())


def price(tier, base):
    return max(100, int(math.floor(base * 1.35**max(0, min(35, tier)) / 100 + .5))*100)


def simulate(nodes, base, seconds=10, stage_gold=1083):
    ranks = {}
    wallet = waves = spent = 0
    gold_percent = 0
    capacity = 20
    point_count = 0
    prices = {n['id']: price(n['tier'], base) for n in nodes}
    by_id = {n['id']: n for n in nodes}
    def ready(n):
        return ranks.get(n['id'], 0) < n['ranks'] and capacity >= n['capacity'] and all(ranks.get(p, 0)>0 for p in n['previous'])
    def buy(n):
        nonlocal wallet, waves, spent, gold_percent, capacity, point_count
        assert ready(n), n['id']
        cost = prices[n['id']]
        reward = math.floor(stage_gold * (1 + gold_percent/100) + .5)
        extra = max(0, (cost-wallet+reward-1)//reward)
        waves += extra
        wallet += extra*reward-cost
        spent += cost
        ranks[n['id']] = ranks.get(n['id'], 0)+1
        point_count += 1
        if n['effect'] == 'OnlineGoldPercent': gold_percent = min(350, gold_percent+n['amount'])
        if n['effect'] == 'BackpackSlots': capacity = min(200, capacity+n['amount'])
    buy(by_id['Talent.Root'])
    buy(by_id['Talent.Entry.IdleOffline'])
    while gold_percent < 350:
        eligible = [n for n in nodes if n['effect']=='OnlineGoldPercent' and ready(n)]
        assert eligible
        buy(min(eligible, key=lambda n:(prices[n['id']], n['id'])))
    gold_max_waves = waves
    while point_count < sum(n['ranks'] for n in nodes):
        eligible = [n for n in nodes if ready(n)]
        assert eligible, 'unreachable talent'
        buy(min(eligible, key=lambda n:(prices[n['id']], n['id'])))
    return dict(base=base, total_gold=spent, waves=waves, days=waves*seconds/86400,
                gold_talent_max_days=gold_max_waves*seconds/86400, points=point_count,
                nodes=len(nodes), remaining_gold=wallet, final_gold_per_wave=math.floor(stage_gold*4.5+.5))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--seconds', type=float, default=10)
    parser.add_argument('--days', type=float, default=45)
    args = parser.parse_args()
    nodes = catalog()
    stage_gold = math.floor(450*1.05**18+.5)
    old_total = sum(n['ranks']*price(n['tier'], 2500) for n in nodes)
    low, high = 1, 2500
    while low < high:
        mid = (low+high)//2
        if simulate(nodes, mid, args.seconds, stage_gold)['days'] < args.days: low = mid+1
        else: high = mid
    choices = [simulate(nodes, b, args.seconds, stage_gold) for b in range(max(1,low-2), low+3)]
    selected = min(choices, key=lambda c:abs(c['days']-args.days))
    data = {'assumptions': {'daily_online_hours':24, 'seconds_per_wave':args.seconds,
                           'target_days':args.days, 'stage':'Hell.3-3 throughout',
                           'initial_talents':0, 'initial_gold':0, 'priority':'online gold first, then cheapest legal upgrades',
                           'excluded':'stage progression time, dropped chests, salvage, other spending'},
            'catalog_sha256':hashlib.sha256(CATALOG.read_bytes()).hexdigest(),
            'stage_gold':stage_gold, 'old_full_tree_gold':old_total, 'choices':choices,
            'selected':selected, 'tier_prices':[{ 'tier':i,'old':price(i,2500),'new':price(i,selected['base'])} for i in range(36)],
            'nodes':nodes}
    out = ROOT/'Saved/Diagnostics/TrainingEconomy105'
    out.mkdir(parents=True, exist_ok=True)
    (out/'calibration.json').write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({k:v for k,v in data.items() if k!='nodes'},ensure_ascii=False,indent=2))


if __name__ == '__main__': main()
