"""Recheck importer boundaries and report whether the task's pilot can be retired."""
import importlib
import json
from pathlib import Path
import runpy
import traceback
import unreal
import gamexxk_texture_budget

ROOT=Path(__file__).resolve().parents[2]
try:
    importlib.reload(gamexxk_texture_budget)
    for script in ('gamexxk_validate_gem_icons.py','gamexxk_validate_map_ui_assets.py'):
        print('VALIDATOR '+script)
        runpy.run_path(str(ROOT/'Content/Python'/script),run_name='__main__')
    pilot='/Game/GameXXK/UI/ImageOptimizationPilot/T_StoryCompressionPilot'
    exists=unreal.EditorAssetLibrary.does_asset_exist(pilot)
    referencers=[str(x) for x in unreal.EditorAssetLibrary.find_package_referencers_for_asset(pilot,False)] if exists else []
    assert not referencers, referencers
    print(json.dumps({'pilot_existed':exists,'referencers':referencers,'safe_to_retire':not referencers}))
except Exception:
    print(json.dumps({'ok':False,'error':traceback.format_exc()}))
