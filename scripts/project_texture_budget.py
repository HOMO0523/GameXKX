"""Build-only image budgets. Source pixels, asset identities and sprite grids stay intact."""
from __future__ import annotations
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]


def policy(record):
    path=record['package']
    size=record.get('built_size') or []
    settings=record.get('settings',{})
    if record.get('asset_class')!='Texture2D' or len(size)!=2:
        return dict(action='keep',reason='special_texture_class')
    w,h=size
    compression=settings.get('compression_settings','')
    source_bytes=w*h*4
    if 'NORMALMAP' in compression or any(x in compression for x in ('GRAYSCALE','MASKS','HDR','DISPLACEMENT','ALPHA','DISTANCE_FIELD')):
        return dict(action='keep',reason='specialized_data_format')
    if any(x in settings.get('lod_group','') for x in ('NORMALMAP','SPECULAR','LIGHTMAP','SHADOWMAP','COLOR_LOOKUP')):
        return dict(action='keep',reason='specialized_texture_group')
    if not ('EDITOR_ICON' in compression or record.get('tags',{}).get('Format') in ('B8G8R8A8','R8G8B8A8')):
        return dict(action='keep',reason='already_compressed_or_platform_managed')
    if source_bytes<512*1024 or min(w,h)<4:
        return dict(action='keep',reason='within_small_texture_budget')
    # Pixel-addressed animation sheets keep every source grid coordinate and output dimension.
    grid=any(x in path.lower() for x in ('atlas','/sprites/','walkloop','/cinematics/'))
    if grid:
        if w%4 or h%4:
            return dict(action='keep',reason='exact_sprite_grid_not_block_aligned',estimated_bgra8_bytes=source_bytes)
        return dict(action='optimize',reason='same_size_atlas_bc7',target_size=[w,h],estimated_before_bytes=source_bytes,estimated_after_bytes=w*h)
    if ('/Characters/Follower/Textures/' in path or '/Narrative/Items/' in path) and w%4==0 and h%4==0:
        return dict(action='optimize',reason='same_size_illustration_bc7',target_size=[w,h],estimated_before_bytes=source_bytes,estimated_after_bytes=w*h)
    if '/UI/' not in path and not path.startswith('/Game/1Game/Texture/'):
        return dict(action='keep',reason='non_ui_asset_requires_its_own_format')
    if '/StoryNodes/' in path or '/RouteCamp/' in path:
        target=[1536,512]
    else:
        icon=('T_TrainingNav' in path or 'T_TrainingTopToolbar' in path or
              path.endswith(('T_MasterV2_CloseInk','T_MasterV2_CardLockedIcon')))
        cap=256 if icon else 2048
        scale=min(1.,cap/max(w,h))
        target=[max(4,round(w*scale/4)*4),max(4,round(h*scale/4)*4)]
    return dict(action='optimize',reason='ui_bc7_and_display_budget',target_size=target,
                estimated_before_bytes=source_bytes,estimated_after_bytes=target[0]*target[1])


def main():
    data=json.loads((ROOT/'Saved/ImageOptimization/loaded-textures.json').read_text(encoding='utf-8'))
    assert data['complete'] and len(data['textures'])==data['requested']
    records=[]
    for n in data['textures']:
        assert n.get('loaded') and 'error' not in n,n['package']
        p=policy(n)
        records.append(dict(package=n['package'],object_path=n['object_path'],source_id=n['source_id'],
            original_size=n['built_size'],settings=n['settings'],source_snapshot=n.get('source_snapshot'),
            referencers=n['referencers'],**p))
    destination=ROOT/'SourceAssets/ImageDelivery/texture-budget-plan.json'
    destination.parent.mkdir(parents=True,exist_ok=True)
    destination.write_text(json.dumps(dict(version=1,records=records),ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    changed=[n for n in records if n['action']=='optimize']
    from collections import Counter
    print(json.dumps(dict(total=len(records),changes=len(changed),reasons=dict(Counter(n['reason'] for n in records)),
        estimated_bgra8_before=sum(n['estimated_before_bytes'] for n in changed),estimated_bc7_after=sum(n['estimated_after_bytes'] for n in changed),path=str(destination))))


if __name__=='__main__':main()
