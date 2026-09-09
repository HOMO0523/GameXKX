"""Drive bounded texture compilation via project MCP, waiting for real read-back."""
from pathlib import Path
import json
import time
import argparse
from ue_mcp_client import UnrealMCPClient
ROOT=Path(__file__).resolve().parents[1]
plan=json.loads((ROOT/'SourceAssets/ImageDelivery/texture-budget-plan.json').read_text(encoding='utf-8'))
report_file=ROOT/'Saved/ImageOptimization/applied-textures.json'
previous=json.loads(report_file.read_text(encoding='utf-8')) if report_file.exists() else dict(textures={})
pending=[n for n in plan['records'] if n['action']=='optimize' and (previous['textures'].get(n['package'],{}).get('status')!='ready' or not previous['textures'].get(n['package'],{}).get('source_pixels_sha1'))]
# UI first. Big fixed animation sheets compile in groups of two to keep peak memory bounded.
pending.sort(key=lambda n:(n['reason']=='same_size_atlas_bc7',n['estimated_after_bytes'],n['package']))
parser=argparse.ArgumentParser();parser.add_argument('--limit',type=int,default=0);args=parser.parse_args()
if args.limit:pending=pending[:args.limit]
c=UnrealMCPClient(timeout=180);assert c.connect()
count=0
while pending:
    take=2 if pending[0]['estimated_after_bytes']>=8*1024*1024 else 8
    batch=pending[:take];pending=pending[take:]
    paths=[n['package'] for n in batch]
    result=c.run_project_python_file('Content/Python/gamexxk_apply_texture_budget_checked.py',['apply',*paths])
    assert result.get('success'),result
    deadline=time.monotonic()+240
    while True:
        time.sleep(1)
        result=c.run_project_python_file('Content/Python/gamexxk_apply_texture_budget_checked.py',['verify',*paths])
        assert result.get('success'),result
        result=json.loads(result['stdout'].strip().splitlines()[-1])
        if all(n['status']=='ready' for n in result['results']):break
        if any(n['status']=='format_mismatch' for n in result['results']):raise RuntimeError(result)
        if time.monotonic()>deadline:raise TimeoutError(result)
    count+=len(paths)
    print(json.dumps(dict(verified_this_run=count,ready_total=result['ready_count'],remaining=len(pending))),flush=True)
print('ALL_REQUESTED_TEXTURES_VERIFIED',flush=True)
