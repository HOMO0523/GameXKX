"""Verify legacy story-map recovery in an isolated ShippingF10 process.

Use only an agent-owned test profile. Each imported scene is wrapped in a Dev
session and restored before the next case; no player profile is used.
"""
import argparse
import json
import time
from pathlib import Path
from gamexxk_dev_client import Client


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dev-dir', type=Path, required=True)
    parser.add_argument('--fixtures', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    client = Client('files', args.dev_dir.resolve(), 90)
    report = {'ok': False, 'cases': []}
    initial = client.call('snapshot.export')
    assert initial['ok'] and not initial['session_active'], 'Use an isolated game with no active experiment'
    try:
        for filename in ['legacy-waiting-scene.json', 'two-node-waiting-scene.json', 'two-node-waiting-scene.json']:
            scene = json.loads((args.fixtures / filename).read_text(encoding='utf-8'))
            assert client.call('session.begin')['ok']
            case = {'fixture': filename, 'input_screen': scene['state']['screen']}
            report['cases'].append(case)
            try:
                imported = client.call('snapshot.import', {'scene': scene})
                case['import_ok'] = imported['ok']
                assert imported['ok'], imported.get('message')
                deadline = time.monotonic() + 15
                while True:
                    current = client.call('snapshot.export')
                    assert current['ok']
                    state = current['data']['state']
                    if state['screen'] == 'Battle' or time.monotonic() >= deadline:
                        break
                    time.sleep(.2)
                story = state['narrativeProgress']['mainStory']
                case.update(screen=state['screen'], journey=story['journeyNodeId'], phase=story['phase'],
                            node_count=len(state['routeMapNodes']), active_battle=state['cardRun']['bHasActiveCardBattle'])
                case['ok'] = case['screen'] == 'Battle' and case['active_battle'] and case['journey'] == 'S00-04'
                assert case['ok'], case
            finally:
                case['restored'] = bool(client.call('session.restore')['ok'])
                assert case['restored']
            print(json.dumps(case, ensure_ascii=False), flush=True)
        report['ok'] = True
    finally:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
