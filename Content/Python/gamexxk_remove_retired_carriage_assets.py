import json
import unreal

assets = [
    '/Game/GameXXK/Cinematics/Prologue/Atlases/T_cinematic_carriage_run_stop_2k_atlas',
    '/Game/GameXXK/Cinematics/Prologue/Atlases/T_cinematic_carriage_run_stop_1k_atlas',
    '/Game/GameXXK/Cinematics/Prologue/Atlases/T_cinematic_carriage_post_stop_idle_2k_atlas',
    '/Game/GameXXK/Cinematics/Prologue/Atlases/T_cinematic_carriage_post_stop_idle_1k_atlas',
    '/Game/GameXXK/Narrative/Dialogues/DA_Dialogue_Tutorial_CarriageNotice',
]
report=[]
for path in assets:
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        report.append({'asset':path,'status':'absent'})
        continue
    refs=unreal.EditorAssetLibrary.find_package_referencers_for_asset(path,True)
    external=[str(r) for r in refs if str(r) not in assets]
    if external:
        report.append({'asset':path,'status':'referenced','references':external})
        continue
    report.append({'asset':path,'status':'deleted' if unreal.EditorAssetLibrary.delete_asset(path) else 'failed'})
print(json.dumps(report,ensure_ascii=False))
