"""Read-only: inspect the real material stores, physical slots and legacy mirrors."""
import json
from pathlib import Path
import unreal
import gamexxk_probe_training_visual_mvp as base

world, pc, wb = base._controller_and_widget()
assert world and wb and 'L_DesktopTrainingHUD' in world.get_path_name()
instance = unreal.GameplayStatics.get_game_instance(world)
dev = next(o for o in unreal.ObjectIterator(unreal.GameXXKDevToolsSubsystem) if o.get_outer() == instance)
state = wb.get_mvp_subsystem().get_runtime_state_copy()
bag = {str(k): int(v) for k, v in state.inventory.items()}
stored = {str(k): int(v) for k, v in state.desktop_inventory.warehouse_items.items()}
rows = []
for item_id in ('Item.EnhancementStone', 'Item.RefinementSand'):
    row = {'id': item_id, 'bag': bag.get(item_id, 0), 'storage': stored.get(item_id, 0), 'slots': []}
    row['total'] = row['bag'] + row['storage']
    for name, slots in [('Bag', state.desktop_inventory.backpack_slots), ('Storage', state.desktop_inventory.warehouse_slots)]:
        row['slots'].extend({'container': name, 'slot': i} for i, entry in enumerate(slots) if str(entry.entry_id) == item_id)
    rows.append(row)
output = {'readOnly': True, 'devSession': dev.is_session_active(), 'materials': rows,
          'chests': len(state.training.owned_chest_tokens),
          'legacyStoneMirror': state.enhancement_material,
          'legacySandMirror': state.equipment_collection.refinement_sand}
path = Path(unreal.Paths.project_dir()) / 'Saved/Codex/UIGuidanceLocalization-20260910/material-stock-live.json'
path.write_text(json.dumps(output, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps(output, ensure_ascii=True))
