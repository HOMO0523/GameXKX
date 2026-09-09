"""Shared save boundary for audited texture budgets; no pixel or importer-path replacement."""
from pathlib import Path
import json

ROOT=Path(__file__).resolve().parents[2]
_path=ROOT/'SourceAssets/ImageDelivery/texture-budget-plan.json'
_plans=None


def apply_texture_budget(asset, unreal_module=None):
    if unreal_module is None:
        import unreal as unreal_module
    unreal=unreal_module
    global _plans
    if not isinstance(asset,unreal.Texture2D):return False
    if _plans is None:
        data=json.loads(_path.read_text(encoding='utf-8')) if _path.exists() else dict(records=[])
        _plans={n['package']:n for n in data['records'] if n['action']=='optimize'}
    plan=_plans.get(asset.get_path_name().split('.')[0])
    if not plan:return False
    if plan['target_size']!=plan['original_size']:
        asset.set_editor_property('power_of_two_mode',unreal.TexturePowerOfTwoSetting.RESIZE_TO_SPECIFIC_RESOLUTION)
        asset.set_editor_property('resize_during_build_x',plan['target_size'][0])
        asset.set_editor_property('resize_during_build_y',plan['target_size'][1])
    asset.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_BC7)
    return True


def save_loaded_asset(asset,*args,unreal_module=None,**kwargs):
    if unreal_module is None:
        import unreal as unreal_module
    apply_texture_budget(asset,unreal_module)
    return unreal_module.EditorAssetLibrary.save_loaded_asset(asset,*args,**kwargs)


def source_size(asset):
    import unreal
    info=json.loads(unreal.GameXXKTextureAuditLibrary.inspect_texture(asset,False))
    return [info['source_width'],info['source_height']]


def expected_compression(asset,unreal_module=None):
    if unreal_module is None:
        import unreal as unreal_module
    unreal=unreal_module
    data=json.loads(_path.read_text(encoding='utf-8')) if _path.exists() else dict(records=[])
    path=asset.get_path_name().split('.')[0]
    optimized=any(n['package']==path and n['action']=='optimize' for n in data['records'])
    return unreal.TextureCompressionSettings.TC_BC7 if optimized else unreal.TextureCompressionSettings.TC_EDITOR_ICON
