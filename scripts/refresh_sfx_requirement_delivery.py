"""Refresh all review pages and archives from already verified media and source tables."""
import base64,hashlib,json,re,shutil,zipfile
from datetime import datetime
from pathlib import Path
from essential_sfx_requirement_catalog import SPECS
from build_essential_sfx_html import complete_package
from build_tool_sfx_review_html import main as build_tool
from build_sfx_requirement_index import main as build_index

ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'Deliverables/音效需求'
PATTERN=r'(<script id="reviewPayload" type="application/json">)(.*?)(</script>)'
def load(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def write(p,value):p.write_text(json.dumps(value,ensure_ascii=False,indent=2),encoding='utf-8')

def main():
 stamp=datetime.now().astimezone();out=ROOT/'Saved/Codex'/('SfxBriefRefresh-'+stamp.strftime('%Y%m%d-%H%M%S'));out.mkdir(parents=True)
 entries=load(BASE/'目录清单.json')['packages'];protected={};reviews={}
 for entry in entries:
  package=BASE/entry['folder'];manifest=load(package/'05_文件核验.json')
  for r in manifest['files']:
   path=package/r['file'];assert sha(path)==r['sha256'],str(path)
   if r['file']!='00_音效需求.html':protected[path.relative_to(BASE).as_posix()]=r['sha256']
  page=package/'00_音效需求.html';payload=json.loads(re.search(PATTERN,page.read_text(encoding='utf-8'),re.S)[2]);reviews[entry['cue']]=payload.get('review')
  for path in [page,package/'05_文件核验.json']:
   backup=out/'previous-generated'/path.relative_to(BASE);backup.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,backup)
 for filename in ['index.html','目录清单.json','00_音效需求总表.xlsx','README_交付说明.md']:shutil.copy2(BASE/filename,out/'previous-generated'/filename)
 write(out/'before.json',{'generated_at':stamp.isoformat(timespec='seconds'),'protected_files':protected,'preserved_review_cues':[c for c,r in reviews.items() if r]})
 build_tool()
 for spec in SPECS:print(json.dumps(complete_package(spec),ensure_ascii=True),flush=True)
 # Reattach any saved candidate/notes before updating file hashes and ZIPs.
 for entry in entries:
  package=BASE/entry['folder'];page=package/'00_音效需求.html';html=page.read_text(encoding='utf-8');match=re.search(PATTERN,html,re.S);payload=json.loads(match[2])
  if reviews[entry['cue']] is not None:
   payload['review']=reviews[entry['cue']];packed=json.dumps(payload,ensure_ascii=False,separators=(',',':')).replace('<','\\u003c')
   page.write_text(html[:match.start(2)]+packed+html[match.end(2):],encoding='utf-8')
  for key,asset in payload['assets'].items():assert hashlib.sha256(base64.b64decode(asset['base64'])).hexdigest()==asset['sha256'],key
  manifest=load(package/'05_文件核验.json');manifest['files']=[{'file':p.relative_to(package).as_posix(),'bytes':p.stat().st_size,'sha256':sha(p)} for p in sorted(package.rglob('*')) if p.is_file() and p.name!='05_文件核验.json'];write(package/'05_文件核验.json',manifest)
  with zipfile.ZipFile(package.with_suffix('.zip'),'w',zipfile.ZIP_DEFLATED) as z:
   for p in sorted(package.rglob('*')):
    if p.is_file():z.write(p,p.relative_to(BASE))
 for relative,digest in protected.items():assert sha(BASE/relative)==digest,relative
 build_index();index=load(BASE/'目录清单.json')
 addon=BASE/'音效需求_新增6类_20260915.zip'
 with zipfile.ZipFile(addon,'w',zipfile.ZIP_DEFLATED) as z:
  for entry in index['packages']:
   if 16<=entry['number']<=21:
    for p in sorted((BASE/entry['folder']).rglob('*')):
     if p.is_file():z.write(p,p.relative_to(BASE))
 archives=[BASE/(e['folder']+'.zip') for e in entries]+[BASE/f"音效需求_{index['count']}类_完整包.zip",addon]
 archive_records=[]
 for archive in archives:
  with zipfile.ZipFile(archive) as z:
   assert z.testzip() is None
   for name in z.namelist():assert hashlib.sha256(z.read(name)).hexdigest()==sha(BASE/name),name
  archive_records.append({'file':archive.name,'bytes':archive.stat().st_size,'sha256':sha(archive)})
 result={'status':'generated_and_file_checks_passed','generated_at':stamp.isoformat(timespec='seconds'),'categories':index['count'],'reference_wavs':index['unique_reference_wavs'],'preserved_source_files':len(protected),'preserved_review_cues':[c for c,r in reviews.items() if r],'archives':archive_records,'output':str(out)}
 write(out/'generation.json',result);print(json.dumps(result,ensure_ascii=True),flush=True)

if __name__=='__main__':main()
