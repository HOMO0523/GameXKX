"""Validate wall-clock dialogue timing in the isolated floating PIE profile."""
import json
import time
from pathlib import Path
from ue_mcp_client import UnrealMCPClient


def main():
    client = UnrealMCPClient(timeout=60)
    client.require_connected()
    client.stop_pie()
    client.wait_for_pie_state(False)
    client.start_pie(play_mode='PlayMode_InEditorFloating')
    client.wait_for_pie_state(True)
    evidence = []

    def run(phase):
        response = client.run_project_python_file(
            'Content/Python/gamexxk_probe_dialogue_autoplay.py', ['--phase', phase])
        state = json.loads(response['stdout'].strip().splitlines()[-1])
        evidence.append({'phase': phase, 'state': state})
        return state

    result = {'ok': False, 'evidence': evidence}
    try:
        run('start')
        time.sleep(1)
        initial = run('enable')
        assert initial['visible'] and initial['line'] == 0
        time.sleep(3)
        early = run('status')
        assert early['line'] == initial['line'], 'Advanced before minimum reading time'
        time.sleep(initial['delay'] - 3 + 1.5)
        advanced = run('status')
        assert advanced['line'] > initial['line'], 'Real widget did not advance on time'
        run('disable')
        run('choice')
        time.sleep(1)
        choice = run('enable')
        assert 'options=0 ' not in choice['diagnostic'], 'Fixture did not expose choices'
        time.sleep(choice['delay'] + 1)
        paused = run('status')
        assert (paused['node'], paused['line'], paused['phase']) == (
            choice['node'], choice['line'], choice['phase']), 'Autoplay selected a choice'
        result['ok'] = True
    except Exception as error:
        result['error'] = str(error)
        raise
    finally:
        try:
            run('disable')
        finally:
            output = Path(__file__).resolve().parents[1] / 'Saved/DialogueAutoPlay/live-result.json'
            output.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(result, ensure_ascii=False))


if __name__ == '__main__':
    main()
