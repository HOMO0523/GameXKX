"""Read-only Slate captures of the current game windows; never starts/stops PIE."""
import argparse
import json
import re
from pathlib import Path

from ue_mcp_client import UnrealMCPClient
from gamexxk_real_play_flow_mcp import _decode_slate_screenshot_png, SLATE_TOOLSET

parser=argparse.ArgumentParser()
parser.add_argument('stem')
args=parser.parse_args()
assert re.fullmatch(r'[a-z0-9-]+',args.stem)
root=Path(__file__).resolve().parents[1]
output=root/'Saved/Codex/UIGuidanceLocalization-20260910/ui-review'
output.mkdir(parents=True,exist_ok=True)
client=UnrealMCPClient(timeout=45)
assert client.connect()
snapshot=str(client.call_tool('Snapshot',{'ref':'','maxDepth':3,'bIncludeSourceLocations':False},toolset_name=SLATE_TOOLSET))
(output/f'{args.stem}-slate.txt').write_text(snapshot,encoding='utf-8')
captures=[]
for line in snapshot.splitlines():
    if not line.startswith('window ') or not any(name in line for name in ('GameXXK Preview','GameXXKDesktopOverlay')):
        continue
    match=re.search(r'\[ref=([^\]]+)\]',line)
    if not match:continue
    ref=match.group(1)
    data=_decode_slate_screenshot_png(client.call_tool('Screenshot',{'ref':ref},toolset_name=SLATE_TOOLSET))
    path=output/f'{args.stem}-{ref}.png'
    path.write_bytes(data)
    captures.append({'ref':ref,'window':line,'path':str(path),'bytes':len(data)})
assert captures,'No current game window was found; the player session was not changed'
print(json.dumps({'captures':captures},ensure_ascii=True))
