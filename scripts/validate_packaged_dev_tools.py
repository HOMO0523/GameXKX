"""Exercise the complete F10 surface in an isolated Development or ShippingF10 game.

Launch the packaged game with a fresh -UserDir, then pass that user's Saved/DevTools
directory. All gameplay changes use a temporary Dev session, restored in finally.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import time

from gamexxk_dev_client import Client


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dev-dir', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    client = Client('files', args.dev_dir.resolve(), 90)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    evidence = args.report.parent / 'dev-responses'
    evidence.mkdir(exist_ok=True)
    report = {'ok': False, 'checks': [], 'commands': [], 'stages': []}
    calls: set[str] = set()

    def call(command: str, parameters: dict | None = None, expected: bool = True) -> dict:
        response = client.call(command, parameters)
        number = len(report['checks']) + 1
        path = evidence / f'{number:03d}-{command}.json'
        path.write_text(json.dumps(response, ensure_ascii=False, indent=2), encoding='utf-8')
        calls.add(command)
        passed = response.get('ok') is expected
        report['checks'].append({'command': command, 'ok': passed, 'evidence': str(path)})
        assert passed, f'{command}: {response.get("message", response)}'
        print(f'{number}: {command} PASS', flush=True)
        return response

    failure = None
    owns_session = False
    try:
        help_data = call('help')['data']
        advertised = set(help_data['commands'])
        initial = call('inspect', {'compact': True})
        assert not initial['session_active'], 'Use an isolated game with no existing Dev experiment'
        baseline = call('snapshot.export')['data']
        catalogue = call('catalog')['data']['entries']
        call('session.begin')
        owns_session = True
        call('item.give', {'id': 'Currency.Gold', 'quantity': 1234})
        assert call('inspect', {'compact': True})['data']['gold'] >= initial['data']['gold'] + 1234
        call('item.give', {'id': 'Currency.Gold', 'quantity': 0}, expected=False)
        call('character.level', {'character': 'Player', 'level': 100})
        call('equipment.loadout', {'character': 'Player', 'sets': ['PoJun'] * 6,
                                  'level': 100, 'quality': 6, 'enhance': 10,
                                  'affix': 'mid', 'gem': 'balanced'})
        equipment = next(row for row in catalogue if row['category'] == 'equipment')
        call('equipment.create', {'id': equipment['id'], 'level': 100, 'quality': 6})
        call('party.select', {'character': 'Npc.TusiChief'})
        cards = baseline['state']['cardRun']['heroSelectedCardIds']
        call('cards.set', {'character': 'Player', 'cards': cards})
        call('equipment.recommend_all', {'level': 100, 'hero_set': 'PoJun'})
        call('progress.unlock_stages')
        call('progress.unlock_tasks')
        call('snapshot.save', {'name': 'packaged-acceptance'})
        call('snapshot.list')
        call('snapshot.load', {'name': 'packaged-acceptance'})
        scene = call('snapshot.export')['data']
        call('snapshot.import', {'scene': scene})
        call('settings.key', {'key': 'F9'})
        call('settings.key', {'key': 'F10'})
        call('battle.start', {'stage': 'Training.Normal.1-1', 'encounter': 7, 'seed': 20260911})
        assert call('inspect', {'compact': True})['data']['battle_active']
        relic = next(row for row in catalogue if row['category'] == 'relic')
        call('item.give', {'id': relic['id'], 'quantity': 1})
        call('heal')
        call('battle.auto', {'enabled': True})
        call('battle.auto', {'enabled': False})
        call('battle.restart')
        call('battle.return')
        assert not call('inspect', {'compact': True})['data']['battle_active']

        stages = [row['id'] for row in catalogue if row['category'] == 'stage']
        assert len(stages) == 30, f'Expected all 30 existing stages, found {len(stages)}'
        for stage in stages:
            call('battle.start', {'stage': stage, 'encounter': 7, 'seed': 20260911})
            assert call('inspect', {'compact': True})['data']['battle_active'], stage
            call('battle.return')
            report['stages'].append(stage)

        benchmark = call('benchmark.prepare', {'role': 'Blade', 'npc': 'Npc.TusiChief'})['data']
        scenario = {'scene': benchmark, 'stage': 'Training.Normal.1-1', 'encounter': 7,
                    'seed': 20260911, 'max_rounds': 100}
        simulation = call('simulate.run', scenario)['data']
        assert simulation.get('ok'), simulation
        assert not simulation.get('error'), simulation
        call('simulate.start', {**scenario, 'runs': 3})
        deadline = time.monotonic() + 180
        while True:
            batch = call('simulate.status')['data']
            if not batch['running']:
                assert batch['done'] == 3 and not batch['cancelled'], batch
                assert batch['report']['errors'] == 0, batch
                break
            assert time.monotonic() < deadline, 'Batch timed out'
            time.sleep(1)
        call('simulate.start', {**scenario, 'runs': 100})
        call('simulate.cancel')
        assert not call('simulate.status')['data']['running']
        report['commands'] = sorted(calls)
        report['uncovered_commands'] = sorted(advertised - calls - {'session.restore'})
        assert not report['uncovered_commands'], report['uncovered_commands']
        report['binary_md5'] = baseline['binary_md5']
    except Exception as exc:
        failure = repr(exc)
    finally:
        try:
            if owns_session:
                restored = call('session.restore')
                assert not restored['session_active']
                after = call('snapshot.export')['data']['state']
                assert after['playerLevel'] == baseline['state']['playerLevel'], 'Original level did not restore'
                assert after['equipmentCollection'] == baseline['state']['equipmentCollection'], 'Original equipment did not restore'
                call('settings.key', {'key': 'F10'})
                report['restored'] = True
        except Exception as exc:
            failure = f'{failure or ""}; restoration: {exc!r}'
        report['ok'] = failure is None
        report['failure'] = failure
        report['commands'] = sorted(calls)
        args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({'ok': report['ok'], 'calls': len(report['checks']),
                      'commands': len(calls), 'stages': len(report['stages']), 'failure': failure}))
    return 0 if report['ok'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
