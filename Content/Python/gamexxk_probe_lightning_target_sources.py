import unreal,json
rows=[]
for name in ('enemy_14_wildcat','enemy_15_vulture','enemy_16_toad'):
    path=f'/Game/GameXXK/BattleAnimations/Atlases/T_{name}_2k_idle_atlas'
    t=unreal.load_asset(path)
    rows.append({'id':name,'file':t.get_editor_property('asset_import_data').get_first_filename() if t else None})
print(json.dumps(rows))
