"""Read-only context check for deferred patches while another task owns the UE build."""
from pathlib import Path
import json

ROOT=Path(__file__).resolve().parents[1]
FOLDER=ROOT/'Saved/Diagnostics/SeventeenGems/pending'

def main():
    checks=[]
    for patch in sorted(FOLDER.glob('*.patch')):
        lines=patch.read_text(encoding='utf-8').splitlines();index=0;source=[];cursor=0;target='';mode=''
        while index<len(lines):
            line=lines[index]
            if line.startswith('*** Update File: '):
                target=line.split(': ',1)[1];source=Path(target).read_text(encoding='utf-8').splitlines();cursor=0;mode='update'
            elif line.startswith('*** Add File: '): mode='add'
            elif line.startswith('@@') and mode=='update':
                index+=1;old=[]
                while index<len(lines) and not lines[index].startswith(('@@','*** ')):
                    if lines[index] and lines[index][0] in (' ','-'):old.append(lines[index][1:])
                    index+=1
                matches=[i for i in range(cursor,len(source)-len(old)+1) if source[i:i+len(old)]==old]
                first=matches[0] if matches else None
                checks.append({'patch':patch.name,'file':str(Path(target).relative_to(ROOT)),
                    'line':first+1 if first is not None else None,'context':old[:1]})
                if first is not None:cursor=first+len(old)
                continue
            index+=1
    result={'ok':all(c['line'] is not None for c in checks),'hunks':len(checks),'checks':checks}
    (FOLDER.parent/'patch-context-check.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({'ok':result['ok'],'hunks':len(checks),'missing':[c for c in checks if c['line'] is None],
        'critical':[c for c in checks if c['context'] and ('LastCardInteractionError.Reset' in c['context'][0] or 'AddArmor' in c['context'][0])]},ensure_ascii=False))
    return result['ok']

if __name__=='__main__':raise SystemExit(0 if main() else 1)
