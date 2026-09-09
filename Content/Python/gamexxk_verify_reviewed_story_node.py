"""Verify a saved approved texture after unloading and reloading its package."""
import gc
import json
from pathlib import Path
import sys
import unreal

root = Path(__file__).resolve().parents[2]
node_id = sys.argv[1]
entry = json.loads((root / 'SourceArt/UI/StoryNodes/manifest.json').read_text(encoding='utf-8'))['nodes'][node_id]
approved = entry['visual_review'] == 'pass' and entry['reviewer'] == 'user'
chapter_preview = '--chapter-preview' in sys.argv[2:] and entry.get('review_batch') == node_id[:3] and entry.get('user_review') == 'pending'
assert approved or (chapter_preview and entry['visual_review'] == 'pass')
package_path = entry['texture'].split('.')[0]
assert package_path.startswith('/Game/GameXXK/UI/StoryNodes/')
folder = root / 'Saved/StorySystem/ApprovedImports' / (node_id + '-' + entry['sha256'][:12])
imported = json.loads((folder / 'report.json').read_text(encoding='utf-8'))
gc.collect()
unreal.SystemLibrary.collect_garbage()
package = unreal.find_package(package_path)
unloaded, error = unreal.EditorLoadingAndSavingUtils.unload_packages([package]) if package else (True, '')
assert unloaded, str(error)
texture = unreal.load_asset(package_path)
audit = json.loads(unreal.GameXXKTextureAuditLibrary.inspect_texture(texture, True))
assert audit['source_pixels_sha1'] == imported['audit']['source_pixels_sha1']
assert [audit['width'], audit['height']] == [1536, 512] and audit['format'] == 'BC7'
result = {'node': node_id, 'unloaded': unloaded, 'reload_verified': True, 'audit': audit}
(folder / 'reload-report.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
print(json.dumps(result))
