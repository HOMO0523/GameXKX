"""Deterministic final artifact validation, including unchanged earlier briefs."""
import base64,hashlib,json,re,wave,zipfile,urllib.request
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'Deliverables/音效需求';OUT=ROOT/'Saved/Codex/ExtraSfxRequirements-20260915'
def load(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def sha_bytes(b):return hashlib.sha256(b).hexdigest()
def main():
 index=load(BASE/'目录清单.json');assert index['count']==21 and index['unique_reference_wavs']==29 and index['integrated_families']==14 and index['reference_only_families']==7
 embedded_count=0;file_count=0;extras=[]
 for entry in index['packages']:
  package=BASE/entry['folder'];manifest=load(package/'05_文件核验.json')
  for r in manifest['files']:assert sha_bytes((package/r['file']).read_bytes())==r['sha256'],r['file'];file_count+=1
  html=(BASE/entry['html']).read_text(encoding='utf-8');payload=json.loads(re.search(r'<script id="reviewPayload" type="application/json">(.*?)</script>',html,re.S)[1])
  for key,asset in payload['assets'].items():
   assert sha_bytes(base64.b64decode(asset['base64']))==asset['sha256'],(entry['cue'],key);embedded_count+=1
  if entry['number']<16:continue
  assert '参考配音' in html and '新增 · 待接入' in html and '仅为抽牌配音' not in html
  records=load(package/'来源与授权/音频清单.json');assert len(records)==1
  r=records[0]
  with wave.open(str(package/r['file'])) as w:assert w.getframerate()==48000 and w.getnchannels()==1 and w.getsampwidth()==2
  events=manifest['sound_events'];assert all(0<=e['target_frame']<manifest['video']['frame_count'] and abs(e['seconds']-e['target_frame']/60)<1e-9 for e in events)
  extras.append({'cue':entry['cue'],'seconds':entry['seconds'],'frames':entry['frame_count'],'targets':[e['target_frame'] for e in events],'aac_correlation':manifest['video']['aac_mix_correlation']})
 archive=BASE/'音效需求_21类_完整包.zip'
 with zipfile.ZipFile(archive) as z:
  assert z.testzip() is None;zip_files=len(z.namelist())
  for name in z.namelist():assert sha_bytes(z.read(name))==sha_bytes((BASE/name).read_bytes()),name
 with zipfile.ZipFile(BASE/'音效需求_15类_完整包.zip') as z:
  previous=[n for n in z.namelist() if '/' in n]
  for name in previous:assert sha_bytes(z.read(name))==sha_bytes((BASE/name).read_bytes()),'Earlier package changed: '+name
 before=load(OUT/'scene-critical02.json')['state']['cardRun']['activeBattle'];after=load(OUT/'critical02-scene-after.json')['state']['cardRun']['activeBattle']
 assert before['talentCriticalChancePercent']==20 and before['talentCriticalDamagePercent']==50 and before['combatRandomState']==273
 a=(273*196314165+907633515)&0xffffffff;b=(a*196314165+907633515)&0xffffffff
 assert a%100==8 and b%100==19 and (after['combatRandomState']&0xffffffff)==b
 preservation=load(OUT/'preservation-final.json');assert preservation['status']=='all_restored_exactly'
 for r in load(OUT/'preservation-before.json'):assert sha_bytes((ROOT/r['file']).read_bytes())==r['sha256']
 review=load(ROOT/'Saved/Codex/EssentialSfxRequirements-20260914/browser-review/checks-extra.json');assert review['status']=='passed' and len(review['packages'])==6
 with urllib.request.urlopen('http://127.0.0.1:18815/index.html?revision=21',timeout=10) as response:
  http=response.read();assert sha_bytes(http)==sha_bytes((BASE/'index.html').read_bytes())
 result={'status':'passed','categories':21,'unique_reference_wavs':29,'new_categories':extras,'new_seconds':sum(e['seconds'] for e in extras),'total_seconds':index['total_video_seconds'],'embedded_assets_verified':embedded_count,'package_files_verified':file_count,'zip_files':zip_files,'zip_bytes':archive.stat().st_size,'zip_sha256':sha_bytes(archive.read_bytes()),'prior_package_files_unchanged':len(previous),'critical_roll_verified':{'agility':a%100,'critical':b%100,'chance_percent':20},'save_files_preserved':preservation['protected_files'],'http_preview_verified':True,'runtime_audio_integration_changed':False}
 (OUT/'validation.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(result,ensure_ascii=False))
if __name__=='__main__':main()
