"""Reload all optimized assets through UE and verify their format, size and original pixels."""
from pathlib import Path
import json
from ue_mcp_client import UnrealMCPClient
ROOT=Path(__file__).resolve().parents[1]
data=json.loads((ROOT/'Saved/ImageOptimization/applied-textures.json').read_text(encoding='utf-8'))['textures']
paths=sorted(data)
assert all(n['status']=='ready' for n in data.values())
c=UnrealMCPClient(timeout=180);assert c.connect();assert not c.is_in_pie(),'Texture package reload requires editor mode'
for start in range(0,len(paths),8):
    result=c.run_project_python_file('Content/Python/gamexxk_verify_optimized_textures.py',['reload',*paths[start:start+8]])
    assert result.get('success'),result
    if start%40==0 or start+8>=len(paths):print(result['stdout'].strip(),flush=True)
result=c.run_project_python_file('Content/Python/gamexxk_verify_optimized_textures.py',['import-boundary',
    '/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_CloseInk',
    '/Game/GameXXK/UI/RouteCamp/T_CampfireRestBanner_V3',
    '/Game/GameXXK/UI/Relics/Icons/T_Relic_OldMap'])
assert result.get('success'),result
print(result['stdout'].strip(),flush=True)
