"""Merge an independently reviewed text fragment by namespace/key, atomically."""
import argparse
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser();parser.add_argument('fragment');args=parser.parse_args()
fragment=(ROOT/args.fragment).resolve();assert fragment.is_relative_to(ROOT/'Content/Localization/GameXXK')
path=ROOT/'Content/Localization/GameXXK/strings.json'
data=json.loads(path.read_text(encoding='utf-8'));incoming=json.loads(fragment.read_text(encoding='utf-8'))['entries']
index={(e.get('namespace','GameXXK'),e['key']):i for i,e in enumerate(data['entries'])}
added=updated=0
for entry in incoming:
    key=(entry.get('namespace','GameXXK'),entry['key'])
    if key in index:
        old=data['entries'][index[key]]
        assert old.get('nativeSource',old['zh-Hans'])==entry.get('nativeSource',entry['zh-Hans']),('Native identity changed',key)
        if old!=entry:data['entries'][index[key]]=entry;updated+=1
    else:index[key]=len(data['entries']);data['entries'].append(entry);added+=1
pending=path.with_suffix('.json.pending');pending.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8');pending.replace(path)
print(json.dumps({'fragment':str(fragment),'added':added,'updated':updated,'total':len(data['entries'])},ensure_ascii=True))
