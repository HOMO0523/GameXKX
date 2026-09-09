"""Import the chapter's cropped dialogue busts for user review; no character sprite edits."""
import hashlib
import json
from pathlib import Path
import sys
import unreal

root=Path(__file__).resolve().parents[2]
manifest=json.loads((root/'SourceArt/UI/StoryPortraits/manifest.json').read_text(encoding='utf-8'))
actors=sys.argv[1:] or ['driver','mountain_man','abbot','innkeeper']
results=[]
for actor in actors:
    entry=manifest['characters'][actor]
    assert entry['review'] in ('chapter_review_pending','user_approved'), actor
    source=(root/entry['file']).resolve(strict=True)
    assert source.is_relative_to((root/'SourceArt/UI/StoryPortraits').resolve())
    assert hashlib.sha256(source.read_bytes()).hexdigest()==entry['sha256']
    assert entry['size']==[512,512] and entry['transparent_pixels']>50000 and entry['soft_edge_pixels']>300
    package=entry['texture'].split('.')[0]
    assert package=='/Game/GameXXK/UI/StoryPortraits/T_StoryPortrait_'+actor
    task=unreal.AssetImportTask()
    task.filename=str(source);task.destination_path=package.rsplit('/',1)[0]
    task.destination_name=package.rsplit('/',1)[1]
    task.automated=True;task.replace_existing=True;task.save=False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture=unreal.load_asset(package)
    assert isinstance(texture,unreal.Texture2D)
    texture.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_BC7)
    texture.set_editor_property('compression_no_alpha',False)
    texture.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property('srgb',True);texture.set_editor_property('never_stream',True)
    assert unreal.EditorAssetLibrary.save_loaded_asset(texture)
    audit=json.loads(unreal.GameXXKTextureAuditLibrary.inspect_texture(texture,True))
    assert audit['width']==512 and audit['height']==512 and audit['format']=='BC7',audit
    entry['imported']=True
    results.append({'actor':actor,'asset':texture.get_path_name(),'source_sha256':entry['sha256'],'audit':audit,'user_review':'pending'})
report_name='S00-portraits-import.json' if set(actors).issubset({'driver','mountain_man','abbot','innkeeper'}) else 'remaining-portraits-import.json'
(root/'Saved/StorySystem'/report_name).write_text(json.dumps(results,ensure_ascii=False,indent=2),encoding='utf-8')
(root/'SourceArt/UI/StoryPortraits/manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps(results,ensure_ascii=False))
