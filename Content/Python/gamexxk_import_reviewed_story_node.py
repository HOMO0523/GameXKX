"""Import one approved illustration or explicitly requested chapter-review revision."""
import hashlib
import json
from pathlib import Path
import shutil
import sys
import unreal

ROOT = Path(__file__).resolve().parents[2]
node_id = sys.argv[1]
entry = json.loads((ROOT / 'SourceArt/UI/StoryNodes/manifest.json').read_text(encoding='utf-8'))['nodes'][node_id]
approved = entry['visual_review'] == 'pass' and entry['reviewer'] == 'user'
chapter_preview = '--chapter-preview' in sys.argv[2:] and entry.get('review_batch') == node_id[:3] and entry.get('user_review') == 'pending'
assert approved or (chapter_preview and entry['visual_review'] == 'pass'), 'Import requires an approved or explicit chapter-review revision'
source = (ROOT / entry['file']).resolve(strict=True)
assert source.is_relative_to((ROOT / 'SourceArt/UI/StoryNodes').resolve())
assert hashlib.sha256(source.read_bytes()).hexdigest() == entry['sha256'] == entry['reviewed_sha256']
assert entry['size'] == [1536, 512]
campaign = json.loads((ROOT / 'SourceAssets/Narrative/MainStory/campaign.json').read_text(encoding='utf-8'))
node = next(n for c in campaign['chapters'] for n in c['nodes'] if n['id'] == node_id)
contract = hashlib.sha256(json.dumps({'art': node['art'], 'result': node['result']}, ensure_ascii=False, sort_keys=True).encode('utf-8')).hexdigest()
assert contract == entry['contract_sha256'], 'Illustration must match the current narrative'
package = entry['texture'].split('.')[0]
assert package.startswith('/Game/GameXXK/UI/StoryNodes/T_Story_')
local_asset = ROOT / 'Content' / (package.removeprefix('/Game/') + '.uasset')
output = ROOT / 'Saved/StorySystem/ApprovedImports' / (node_id + '-' + entry['sha256'][:12])
output.mkdir(parents=True, exist_ok=True)
if local_asset.exists() and not (output / local_asset.name).exists():
    shutil.copy2(local_asset, output / local_asset.name)

task = unreal.AssetImportTask()
task.filename = str(source)
task.destination_path = package.rsplit('/', 1)[0]
task.destination_name = package.rsplit('/', 1)[1]
task.automated = True
task.replace_existing = True
task.save = False
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
texture = unreal.load_asset(package)
assert isinstance(texture, unreal.Texture2D)
texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_BC7)
texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
texture.set_editor_property('srgb', True)
texture.set_editor_property('never_stream', True)
from gamexxk_texture_budget import save_loaded_asset
assert save_loaded_asset(texture)
audit = json.loads(unreal.GameXXKTextureAuditLibrary.inspect_texture(texture, True))
assert [audit['source_width'], audit['source_height']] == entry['size'], audit
assert [audit['width'], audit['height']] == entry['size'] and audit['format'] == 'BC7', audit

# Existing story material uses the campfire's same UV edge-fade formula.
# Verify both resources and their authored factors; preserve user-tuned materials.
materials = {}
for label, path in [('story', '/Game/GameXXK/UI/StoryNodes/M_StoryIllustrationSoftEdge'),
                    ('campfire', '/Game/GameXXK/UI/Materials/Followup/M_CampfireBannerSoftEdge')]:
    material = unreal.load_asset(path)
    assert material and material.get_editor_property('material_domain') == unreal.MaterialDomain.MD_UI
    assert material.get_editor_property('blend_mode') == unreal.BlendMode.BLEND_TRANSLUCENT
    values = []
    expressions = unreal.MaterialEditingLibrary.get_material_expressions(material)
    for expression in expressions:
        if isinstance(expression, unreal.MaterialExpressionScalarParameter):
            values.append(float(expression.get_editor_property('default_value')))
        elif isinstance(expression, unreal.MaterialExpressionConstant):
            values.append(float(expression.get_editor_property('r')))
    assert 12.0 in values and 10.0 in values, (label, values)
    materials[label] = {'path': material.get_path_name(), 'edge_factors': [12, 10], 'expression_count': len(expressions)}

report = {'node': node_id, 'source': str(source), 'source_sha256': entry['sha256'],
          'asset': texture.get_path_name(), 'audit': audit, 'materials': materials,
          'complete': True, 'chapter_preview': chapter_preview and not approved, 'backup': str(output / local_asset.name)}
(output / 'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps(report, ensure_ascii=False))
