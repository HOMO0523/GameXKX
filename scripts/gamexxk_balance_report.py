#!/usr/bin/env python3
"""Collect and present real GameXXK Dev simulations. No combat formulas live here.

python scripts/gamexxk_balance_report.py manifest --suite smoke --output Saved/Balance/smoke.json
python scripts/gamexxk_balance_report.py run --manifest Saved/Balance/smoke.json
python scripts/gamexxk_balance_report.py render --manifest Saved/Balance/smoke.json
"""
from __future__ import annotations
import argparse, base64, csv, datetime as dt, gzip, hashlib, html, itertools, json, math, statistics, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ROLES = dict(Blade='刀客', Guard='守卫', Healer='药师', Hunter='弓手', Sorcerer='法师', FormationMaster='阵师')
NPCS = dict(TusiChief='土司首领', SongJinBao='宋金宝', YueBai='月白', ZhouGuangZu='周光祖', JinGui='金贵', QiongMeiEr='琼梅儿')
DIRECTIONS = dict(Blade='刀系', Guard='守系', Healer='药系', Hunter='弓系', Mage='法系', Formation='阵系')
def read(p): return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def write(p, obj):
    p=Path(p);p.parent.mkdir(parents=True, exist_ok=True);p.write_text(json.dumps(obj,ensure_ascii=False,indent=2),encoding='utf-8')
def get(obj, name, default=0):
    return next((v for k,v in obj.items() if k.casefold()==name.casefold()),default)
def fingerprint():
    paths=list((ROOT/'Source/GameXXK').rglob('*.cpp'))+list((ROOT/'Source/GameXXK').rglob('*.h'))
    return dict(head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
      branch=subprocess.check_output(['git','branch','--show-current'],cwd=ROOT,text=True).strip(),
      sources_sha256={str(p.relative_to(ROOT)).replace('\\','/'):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(paths)})

def make_manifest(args):
    out=Path(args.output).resolve();cases=[];seeds=list(range(args.seed,args.seed+args.runs))
    combos=itertools.product(DIRECTIONS,ROLES,NPCS)
    if args.suite=='smoke':combos=[('Blade','Blade','TusiChief'),('Mage','Sorcerer','SongJinBao')]
    for hero,role,npc in combos:
        cases.append(dict(id=f'hell-{hero}-{role}-{npc}',fixture=dict(role=role,npc='Npc.'+npc,hero_direction=hero,npc_omit=0 if npc=='JinGui' else 3),stage='Training.Hell.3-1',encounter=7,seeds=seeds))
    if args.suite=='matrix':
        for stage,role,npc in itertools.product(['Training.Normal.3-1','Training.Hard.3-1'],ROLES,NPCS):
            cases.append(dict(id=f'{stage}-{role}-{npc}',fixture=dict(role=role,npc='Npc.'+npc,hero_direction='Blade',npc_omit=0 if npc=='JinGui' else 3),stage=stage,encounter=7,seeds=seeds))
    if args.suite=='matrix' and args.extended:
        for role,npc,omit in itertools.product(ROLES,NPCS,range(4)):
            baseline_omit=0 if npc=='JinGui' else 3
            if omit==baseline_omit:continue
            cases.append(dict(id=f'npc-choice-{role}-{npc}-{omit}',kind='npc_choice',fixture=dict(role=role,npc='Npc.'+npc,hero_direction='Blade',npc_omit=omit),stage='Training.Hell.3-1',encounter=7,seeds=seeds))
        for role,variant in itertools.product(ROLES,['enhance0','no_gems']):
            f=dict(role=role,npc='Npc.TusiChief',hero_direction='Blade',npc_omit=3)
            f.update({'enhance':0} if variant=='enhance0' else {'gems':False})
            cases.append(dict(id=f'gear-{role}-{variant}',kind='gear_comparison',fixture=f,stage='Training.Hell.3-1',encounter=7,seeds=seeds))
    write(out,dict(schema=1,created_at=dt.datetime.now(dt.timezone.utc).isoformat(),directory=str(out.parent/out.stem),suite=args.suite,cases=cases,code=fingerprint(),method='Skilled greedy, fixed legal decks, no optimal-play claim'))
    print(f'{out}: {len(cases)} configurations / {sum(len(c["seeds"]) for c in cases)} battles')

def run(args):
    manifest=Path(args.manifest).resolve();m=read(manifest);directory=Path(m['directory']);directory.mkdir(parents=True,exist_ok=True)
    if args.via=='files':
        from gamexxk_dev_client import submit_file,request_payload,default_directory
        call=lambda cmd,a:submit_file(Path(args.dev_dir) if args.dev_dir else default_directory(),request_payload(cmd,a),timeout=300)
        done=0
        for case in m['cases']:
            d=directory/case['id'];d.mkdir(exist_ok=True)
            scene=call('benchmark.prepare',case['fixture'])
            if not scene['ok']:raise RuntimeError(scene['message'])
            scene=scene['data'];write(directory/'catalog.json',{'cards':scene.pop('card_catalog')});write(d/'source.json',scene)
            for seed in case['seeds']:
                result=call('simulate.run',dict(scene=scene,stage=case['stage'],encounter=case['encounter'],seed=seed,max_rounds=60))
                write(d/f'seed-{seed}.json',result);done+=1
            write(directory/'progress.json',dict(completed=done,last_case=case['id']))
    else:
        # The existing UE test seam invokes the very same Dev JSON service without UI delays.
        exe=Path(args.ue_root)/'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
        cmd=[str(exe),str(ROOT/'GameXXK.uproject'),'-Unattended','-NoSound','-NullRHI','-NoSplash','-NoPause',f'-GameXXKBalanceManifest={manifest}',f'-ReportOutputPath={directory / "automation"}',f'-ExecCmds=Automation RunTests {args.tests}; Quit']
        write(directory/'invocation.json',{'command':cmd,'manifest_sha256':hashlib.sha256(manifest.read_bytes()).hexdigest(),'code':fingerprint()})
        with (directory/'commandlet.log').open('w',encoding='utf-8') as log:
            r=subprocess.run(cmd,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
        if r.returncode:raise RuntimeError(f'UE exit {r.returncode}; see {directory / "commandlet.log"}')
    print(directory)

def wilson(w,n):
    if not n:return [None,None]
    z=1.96;p=w/n;den=1+z*z/n;mid=(p+z*z/(2*n))/den;half=z*math.sqrt(p*(1-p)/n+z*z/(4*n*n))/den
    return [max(0,mid-half),min(1,mid+half)]
def mean(xs):return statistics.fmean(xs) if xs else None

def collect(m):
    root=Path(m['directory']);rows=[];traces={};catalog=read(root/'catalog.json')['cards'];cards={c['id']:c for c in catalog};binary=set()
    for case in m['cases']:
        src=root/case['id']/'source.json'
        if not src.exists():continue
        source=read(src);state=source['state'];runstate=get(state,'cardRun',{});roster=get(get(runstate,'companionRoster',{}),'permanentCompanions',[])
        active=next((c for c in roster if get(c,'bIsActive',False)),{})
        for seed in case['seeds']:
            path=root/case['id']/f'seed-{seed}.json'
            if not path.exists():continue
            response=read(path);d=response.get('data',{});metrics=d.get('metrics',{});trace=d.get('trace',[]);opening=d.get('opening',{});binary.add(d.get('binary_md5',source.get('binary_md5','unavailable')))
            units=get(opening,'units',[]);party=[u for u in units if get(u,'side','')=='Party'];enemy=[u for u in units if get(u,'side','')=='Enemy']
            rounds=get(metrics,'rounds');failure=get(metrics,'failureReason','');limited=failure in ('Simulation.MaxRounds','Simulation.MaxDecisions');rounds=max((get(t,'round') for t in trace),default=rounds) if limited else rounds;damage=get(metrics,'damageDealt');valid=d.get('ok',False);outcome='limit' if limited else d.get('outcome','error')
            r=dict(id=case['id'],seed=seed,stage=case['stage'],encounter=case['encounter'],hero=case['fixture']['hero_direction'],role=case['fixture']['role'],npc=case['fixture']['npc'].removeprefix('Npc.'),omit=case['fixture'].get('npc_omit',3),enhance=case['fixture'].get('enhance',10),gems=case['fixture'].get('gems',True),outcome=outcome,kind=case.get('kind','baseline'),valid=valid,rounds=rounds,damage=damage,dpr=damage/max(1,rounds),damage_r1=sum(get(t,'effectiveDamage') for t in trace if get(t,'round')==1),dpr_r3=sum(get(t,'effectiveDamage') for t in trace if get(t,'round')<=3)/max(1,min(3,rounds)),late_dpr=sum(get(t,'effectiveDamage') for t in trace if get(t,'round')>3)/max(1,rounds-3) if rounds>3 else None,taken=get(metrics,'damageTaken'),healing=get(metrics,'healingGenerated'),armor=get(metrics,'armorGenerated'),health=get(metrics,'remainingPartyHealth'),max_health=sum(get(u,'maxHP') for u in party),ledger_difference=get(metrics,'damageLedgerDifference'),energy_spent=get(metrics,'energySpent'),mana_spent=get(metrics,'manaSpent'),unspent_energy=get(metrics,'energyUnspentAtPhaseEnd'),cards_played=get(metrics,'activelyPlayedCards'),automatic=get(metrics,'automaticResolutionCount'),overkill=get(metrics,'overkillDamage'),overheal=get(metrics,'overhealing'),error=d.get('error',response.get('message','')),source_damage=get(metrics,'damageBySource',{}),origin_damage=get(metrics,'damageByOrigin',{}),played=get(metrics,'cardsPlayedById',{}),card_damage=get(metrics,'damageByCardId',{}),hero_cards=get(runstate,'heroSelectedCardIds',[]),partner_cards=get(active,'selectedCardIds',[]),npc_cards=get(get(get(runstate,'partySelection',{}),'questNpc',{}),'selectedCardIds',[]),party=[{k:get(u,k,None) for k in ['unitId','role','combatLevel','maxHP','attack','defense','mana','maxMana']} for u in party],enemies=[{k:get(u,k,None) for k in ['unitId','combatLevel','maxHP','attack','defense']} for u in enemy],raw=str(path))
            enemy_hits=[]
            for t in trace:
                party_ids={get(u,'unitId') for u in get(t,'unitsBefore',[]) if get(u,'side','')=='Party'}
                enemy_hits.extend(p for p in get(t,'damagePackets',[]) if get(p,'resolvedTargetUnitId') in party_ids and get(p,'cause','')=='DirectAttack' and not get(p,'bAvoidedByAgility',False))
            r['enemy_direct_hits']=len(enemy_hits)
            r['enemy_chip_after_defense']=sum(get(p,'damageAfterDefense')==1 for p in enemy_hits)
            r['enemy_armor_absorbed']=sum(get(p,'armorAbsorbed') for p in enemy_hits)
            rows.append(r)
            # One complete trace per configuration, with the exact seed retained.
            if case['id'] not in traces:
                slim=[]
                for step in trace:
                    t={k:v for k,v in step.items() if k not in ('healthDelta','manaDelta','armorDelta')}
                    t['damagePackets']=[{k:v for k,v in packet.items() if k in ('sourceUnitId','resolvedTargetUnitId','healthDamage','armorAbsorbed','cause','resolutionOrigin','markStacksBeforeHit','markStacksConsumed','vulnerabilityStacksConsumed','markDamageBonusPercent','momentumDamageBonus','bTriggeredEnemyPhase','enemyPhaseBefore','enemyPhaseAfter')} for packet in step.get('damagePackets',[])]
                    slim.append(t)
                traces[case['id']]=dict(seed=seed,steps=slim)
    audit=ROOT/'Saved/Automation/DesignTableRuntime/runtime.json'
    if audit.exists():
        for enemy in read(audit)['enemies']:
            for phase in enemy['phases']:
                for intent in phase['intents']:
                    cards[f"Monster.Intent.{enemy['id']}.P{phase['phaseNumber']}.{intent['id']}"]={'displayName':intent['displayName']}
    return rows,traces,cards,sorted(binary)

def render(args):
    m=read(args.manifest);rows,traces,cards,binary=collect(m)
    if not rows:raise RuntimeError('No runtime samples found')
    out=Path(args.output or Path(m['directory'])/'report.html').resolve();out.parent.mkdir(parents=True,exist_ok=True)
    summary=dict(created_at=dt.datetime.now(dt.timezone.utc).isoformat(),requested=sum(len(c['seeds']) for c in m['cases']),completed=len(rows),valid=sum(r['valid'] for r in rows),errors=sum(r['outcome']=='error' for r in rows),limits=sum(r['outcome']=='limit' for r in rows),stalemates=sum(r['outcome']=='stalemate' for r in rows),ledger_mismatches=sum(r['ledger_difference']!=0 for r in rows),binary_md5=binary,code=m.get('code'),rows=rows)
    write(out.with_suffix('.json'),summary)
    scalar_keys=[k for k,v in rows[0].items() if not isinstance(v,(dict,list))]
    with out.with_suffix('.csv').open('w',newline='',encoding='utf-8-sig') as f:
        w=csv.DictWriter(f,fieldnames=scalar_keys,extrasaction='ignore');w.writeheader();w.writerows(rows)
    comparison=ROOT/'Saved/Codex/DevRecommended-20260906/table-comparison.json'
    packed_traces={k:base64.b64encode(gzip.compress(json.dumps(v,ensure_ascii=False,separators=(',',':')).encode('utf-8'),mtime=0)).decode('ascii') for k,v in traces.items()}
    payload=dict(summary=summary,traces=packed_traces,cards={k:{'displayName':v.get('displayName',k)} for k,v in cards.items()},roles=ROLES,npcs=NPCS,directions=DIRECTIONS)
    if comparison.exists():payload['table_audit']=read(comparison)
    advice=Path(args.manifest).with_suffix('.advice.html')
    if advice.exists():payload['advice_html']=advice.read_text(encoding='utf-8')
    template=(Path(__file__).parent/'templates/gamexxk_balance_report.html').read_text(encoding='utf-8')
    encoded=json.dumps(payload,ensure_ascii=False,separators=(',',':')).replace('</','<\\/')
    out.write_text(template.replace('__REPORT_DATA__',encoded),encoding='utf-8')
    print(out);print(json.dumps({k:v for k,v in summary.items() if k not in ('rows','code')},ensure_ascii=False))

def main():
    p=argparse.ArgumentParser(description=__doc__);sub=p.add_subparsers(dest='command',required=True)
    q=sub.add_parser('manifest');q.add_argument('--suite',choices=['smoke','matrix'],default='smoke');q.add_argument('--output',required=True);q.add_argument('--runs',type=int,default=5);q.add_argument('--seed',type=int,default=20260906);q.add_argument('--extended',action='store_true')
    q=sub.add_parser('run');q.add_argument('--manifest',required=True);q.add_argument('--via',choices=['commandlet','files'],default='commandlet');q.add_argument('--ue-root',default='D:/UE_5.8');q.add_argument('--dev-dir');q.add_argument('--tests',default='GameXXK.Development.ExportBalanceManifest')
    q=sub.add_parser('render');q.add_argument('--manifest',required=True);q.add_argument('--output')
    args=p.parse_args();{'manifest':make_manifest,'run':run,'render':render}[args.command](args)
if __name__=='__main__':main()
