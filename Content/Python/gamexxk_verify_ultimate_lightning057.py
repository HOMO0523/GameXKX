import unreal,json
from pathlib import Path
root=Path(unreal.Paths.project_dir());base=root/'SourceArt/UI/Battle/VFX/UltimateLightning057'
report=json.loads((base/'ue-import.json').read_text(encoding='utf-8'));checked=[]
for row in report['textures']:
 t=unreal.load_asset(row['texture']);size=[t.blueprint_get_size_x(),t.blueprint_get_size_y()]
 assert size==[2592,736],size
 assert not t.get_editor_property('compression_no_alpha')
 checked.append({'path':t.get_path_name(),'size':size,'alpha_compression_preserved':True})
lib=unreal.MaterialEditingLibrary;friendly=unreal.load_asset(report['instances'][0]);canonical=unreal.load_asset(report['instances'][1])
assert lib.get_material_instance_scalar_parameter_value(friendly,'FlipX')==1
assert lib.get_material_instance_scalar_parameter_value(canonical,'FlipX')==0
result={'ok':True,'textures':checked,'friendly_flip_x':1,'canonical_flip_x':0,'hooked_to_battle':False}
(base/'ue-verified.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(result,ensure_ascii=False))
