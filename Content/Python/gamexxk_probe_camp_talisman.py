"""Reuse the canonical pure-2D Camp fixture and check its real reward."""
import json
import sys
from pathlib import Path
import gamexxk_accept_route_encounter_cards_pie as camp
import unreal

mode = sys.argv[1] if len(sys.argv) > 1 else 'setup'
if mode == 'setup':
    _, pc, subsystem, _ = camp._context()
    if camp._runtime(subsystem)['screen'].endswith('TOWN'):
        if not subsystem.start_training_challenge(unreal.Name('Training.Normal.1-1')):
            raise RuntimeError('The canonical desktop challenge did not open')
        pc.refresh_player_flow_widgets_for_test()
    result = camp.setup_camp()
elif mode == 'resolve':
    result = camp.resolve_camp()
else:
    _, pc, subsystem, _ = camp._context()
    result = {'cleared': camp._fixture_result(subsystem.clear_route_encounter_acceptance_fixture_for_test())}
    pc.refresh_player_flow_widgets_for_test()
destination = Path(__file__).resolve().parents[2] / 'Saved/Codex/CardEffects-20260907'
suffix = sys.argv[2] if len(sys.argv) > 2 else 'current'
(destination / f'camp-{mode}-{suffix}.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps(result, ensure_ascii=False))
