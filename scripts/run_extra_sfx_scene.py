"""Run one guarded supplemental-brief scene command through project UE MCP."""
import sys,json
from ue_mcp_client import UnrealMCPClient
c=UnrealMCPClient()
assert c.connect(),'Editor MCP not ready'
if sys.argv[1]=='start':
    if not c.is_in_pie():c.start_pie()
    print(json.dumps({'pie':c.is_in_pie()}))
else:
    r=c.run_project_python_file('Content/Python/gamexxk_record_sfx_requirements.py',sys.argv[1:]+['--output','Saved/Codex/ExtraSfxRequirements-20260915'])
    assert r.get('success'),r
    print(r['stdout'])
