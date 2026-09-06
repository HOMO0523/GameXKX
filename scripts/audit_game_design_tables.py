#!/usr/bin/env python3
"""Read the three approved workbooks and compare exported UE runtime evidence.

Requires openpyxl. This never regenerates or overwrites the design workbooks.
"""
from pathlib import Path
import argparse,hashlib,json,re
from openpyxl import load_workbook

ROOT=Path(__file__).resolve().parents[1]
def load(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def main():
 p=argparse.ArgumentParser();p.add_argument('--runtime',default='Saved/Automation/DesignTableRuntime/runtime.json');p.add_argument('--output',default='Saved/Codex/DevRecommended-20260906/table-comparison.json');a=p.parse_args()
 runtime=load(a.runtime);books={};evidence=[];issues=[]
 for f in (ROOT/'docs/design/2026-09-04-project-design-tables').glob('*.xlsx'):
  wb=load_workbook(f,read_only=True,data_only=False);sheets={s.title:[dict(zip(next(s.values),row)) for row in list(s.values)[1:]] for s in wb};wb.close()
  books[f.name]={'path':str(f),'sha256':hashlib.sha256(f.read_bytes()).hexdigest(),'sheets':sheets}
 def sheet(name):return next(b['sheets'][name] for b in books.values() if name in b['sheets'])
 def check(table,key,field,expected,actual,tolerance=0):
  ok=abs(expected-actual)<=tolerance if isinstance(expected,(int,float)) and isinstance(actual,(int,float)) else expected==actual
  row=dict(table=table,id=key,field=field,expected=expected,actual=actual,status='pass' if ok else 'difference');evidence.append(row)
  if not ok:issues.append(row)
 cards={(c['id'],c['quality']):c for c in runtime['cards']};pending={'Profession.Sorcerer.RanLingHuanYuan','Npc.JinGui.HouXiangTuoShen'}
 for row in sheet('02_品质版本'):
  key=(row['CardId'],row['当前品质']);actual=cards.get(key)
  check('卡牌',list(key),'catalog_presence',True,actual is not None)
  if not actual:continue
  for field,prop in [('卡名','name')]:check('卡牌',list(key),field,row[field],actual[prop])
  for field,prop in [('气力','energyCost'),('内力','manaCost')]:check('卡牌',list(key),field,row[field],actual['definition'][prop])
  check('卡牌',list(key),'seven_terrain_execution',7,actual['passed_terrains'])
  if row['CardId'] in pending:actual['design_pending']=True
 enemies={x['id']:x for x in runtime['enemies']}
 for row in sheet('01_怪物属性'):
  enemy=enemies.get(row['怪物ID'],{});key=row['怪物ID']
  for field,prop in [('名称','displayName'),('基础生命','baseHP'),('生命/级','hPPerLevel'),('基础攻击','baseAttack'),('攻击/级','attackPerLevel'),('基础防御','baseDefense'),('防御/级','defensePerLevel'),('速度','speed')]:check('怪物',key,field,row[field],enemy.get(prop),1e-5)
  for level in [100,125,135]:
   for field,prop in [('生命','maxHP'),('攻击','attack'),('防御','defense')]:check('怪物',key,f'L{level}{field}',row[f'L{level}{field}'],enemy.get(f'level{level}',{}).get(prop))
 stages={x['stageId']:x for x in runtime['stages']};difficulty={'普通':'Normal','困难':'Hard','地狱':'Hell'}
 for row in sheet('02_27关总表'):
  key=f'Training.{difficulty[row["难度"]]}.{row["关卡"]}';check('怪物',key,'combat_level',row['敌人等级'],stages.get(key,{}).get('combatLevel'))
 for row in sheet('03_189编制'):
  key=f'Training.{difficulty[row["难度"]]}.{row["关卡"]}';encounters=stages.get(key,{}).get('encounters',[]);index=int(row['序号'])-1+{'普通':0,'精英':4,'关底':6,'首领':6,'Boss':6}[row['类型']]
  actual=encounters[index] if 0<=index<len(encounters) else {}
  names=' / '.join(enemies[i]['displayName'] for i in actual.get('enemyDefinitionIds',[]))
  check('怪物',f'{key}:{index+1}','formation',row['左/中/右'],names)
 gems={x['id']:x for x in runtime['gems']}
 for row in sheet('02_宝石总表'):check('装备',row['道具ID'],'value',row['数值'],gems.get(row['道具ID'],{}).get('value'))
 sets={x['id']:x for x in runtime['sets']}
 for row in sheet('05_套装效果'):
  s=sets.get(row['描述符ID'],{});check('装备',row['描述符ID'],'pieces',row['件数'],s.get('requiredPieces'));check('装备',row['描述符ID'],'description',row['效果'],s.get('description'))
 templates={x['id']:x for x in runtime['equipment']};affixes={x['id']:x for x in runtime['affixes']}
 for row in sheet('03_装备模板'):
  if row['类型']=='现代':check('装备',row['模板ID'],'modern_template_presence',True,row['模板ID'] in templates)
 for row in sheet('04_词缀目录'):
  f=affixes.get(row['词缀ID'],{});check('装备',row['词缀ID'],'affix_effect',row['效果族'],f.get('modifierKind'))
 # These are explicit limits in the production rules, not an inferred spreadsheet budget.
 text=(ROOT/'Source/GameXXK/Public/GameXXKEquipmentRules.h').read_text(encoding='utf-8');cap=int(re.search(r'MaxItemLevel\s*=\s*(\d+)',text).group(1));check('装备','equipment_level','max_item_level',135,cap)
 out=Path(a.output);out.parent.mkdir(parents=True,exist_ok=True)
 report=dict(workbooks={n:{k:v for k,v in b.items() if k!='sheets'} for n,b in books.items()},runtime_sha256=hashlib.sha256(Path(a.runtime).read_bytes()).hexdigest(),checks=len(evidence),passed=len(evidence)-len(issues),differences=issues,pending_cards=sorted(pending),rows=evidence,sheet_counts={n:{s:len(rows) for s,rows in b['sheets'].items()} for n,b in books.items()},limits=['Text equality differences are review flags, not automatically gameplay defects.','Numeric basis for the seven final-stat targets omits enhancement and exact affixes; do not pretend arbitrary +10 gear is that target.','All-quality playability is execution/valid-state coverage. Dedicated semantic fixtures supply exact mechanism assertions.','Legacy compatibility templates are covered by existing equipment migration tests, not the modern package export.'])
 out.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps({'checks':len(evidence),'passed':report['passed'],'differences':len(issues),'output':str(out)},ensure_ascii=False))
if __name__=='__main__':main()
