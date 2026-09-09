"""Compare the active UMG summary with the rendered Slate text in canonical PIE."""
import argparse
import json
from pathlib import Path
import re
import time

from ue_mcp_client import UnrealMCPClient
from gamexxk_real_play_flow_mcp import _decode_slate_screenshot_png, SLATE_TOOLSET

parser = argparse.ArgumentParser()
parser.add_argument('--name', required=True)
parser.add_argument('--samples', type=int, default=1)
parser.add_argument('--interval', type=float, default=1.0)
parser.add_argument('--advance', type=int, default=0)
parser.add_argument('--gc', action='store_true')
args = parser.parse_args()
destination = Path(__file__).resolve().parents[1] / 'Saved/Codex/IdleSummaryFix-20260908'
client = UnrealMCPClient(timeout=90)
client.require_connected()

def probe(mode='observe', *values):
    result = client.run_project_python_file('Content/Python/gamexxk_probe_idle_summary.py', [mode, *values])
    return json.loads(result['stdout'])

def snapshot():
    root = str(client.call_tool('Snapshot', {'ref': '', 'maxDepth': 3, 'bIncludeSourceLocations': False}, toolset_name=SLATE_TOOLSET))
    match = re.search(r'window "GameXXKDesktopOverlay".*\[ref=(\w+)\]', root)
    assert match, 'The native desktop HUD must be visible'
    ref = match.group(1)
    client.call_tool('Observe', {'ref': ref, 'maxDepth': 80}, toolset_name=SLATE_TOOLSET)
    # Allow the inspector's observer to receive an actual painted frame.
    time.sleep(0.3)
    result = str(client.call_tool('Snapshot', {'ref': ref, 'maxDepth': 80, 'bIncludeSourceLocations': False}, toolset_name=SLATE_TOOLSET))
    return ref, result

rows = []
for index in range(args.samples):
    if args.gc:
        client.execute_console_command('obj gc')
    if args.advance:
        probe('advance', str(args.advance))
    time.sleep(args.interval)
    before = probe()
    ref, slate = snapshot()
    after = probe()
    actual = re.findall(r'text "(\d+/7)"', slate)
    expected = f"{after['runtime_encounter'] + 1}/7"
    stable = before['runtime_encounter'] == after['runtime_encounter']
    row = {'sample': index, 'runtime': after['runtime_encounter'], 'expected': expected,
           'slate': actual, 'stable': stable, 'ok': not stable or actual == [expected],
           'tick': after['tick'], 'widgets': after['widgets']}
    rows.append(row)
    print(json.dumps({k: v for k, v in row.items() if k != 'widgets'}), flush=True)
    if index == 0 or index == args.samples - 1 or not row['ok']:
        prefix = f'{args.name}-{index:02d}'
        (destination / f'{prefix}.txt').write_text(slate, encoding='utf-8')
        image = client.call_tool('Screenshot', {'ref': ref}, toolset_name=SLATE_TOOLSET)
        (destination / f'{prefix}.png').write_bytes(_decode_slate_screenshot_png(image))
(destination / f'{args.name}.json').write_text(json.dumps(rows, ensure_ascii=False, indent=2), encoding='utf-8')
assert all(row['ok'] for row in rows), 'Painted wave progress did not match the authoritative encounter'
