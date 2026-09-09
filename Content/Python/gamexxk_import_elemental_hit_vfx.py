"""Import the generated six-frame elemental impacts, preserving the exact sprite grid."""
from pathlib import Path
import hashlib, json, struct
import unreal
from gamexxk_texture_budget import save_loaded_asset

ROOT=Path(__file__).resolve().parents[2]
MANIFEST=ROOT/'SourceArt/UI/Battle/VFX/ElementalHits/manifest.json'
DEST='/Game/GameXXK/UI/Battle/VFX/ElementalHits'


def main():
    data=json.loads(MANIFEST.read_text(encoding='utf-8'));assert len(data['effects'])==2
    records=[]
    for effect in data['effects']:
        source=ROOT/effect['atlas'];raw=source.read_bytes()
        assert hashlib.sha256(raw).hexdigest()==effect['atlas_sha256']
        assert struct.unpack('>II',raw[16:24])==(2048,2240) and raw[25]==6
        name=source.stem;path=DEST+'/'+name
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            texture=unreal.load_asset(path)
            imports=texture.get_editor_property('asset_import_data').extract_filenames()
            assert imports and Path(imports[0]).resolve()==source.resolve(),'Different source at '+path
        else:
            task=unreal.AssetImportTask();task.filename=str(source);task.destination_path=DEST;task.destination_name=name
            task.automated=True;task.replace_existing=False;task.save=False
            unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task]);texture=unreal.load_asset(path)
        assert isinstance(texture,unreal.Texture2D)
        settings={'compression_settings':unreal.TextureCompressionSettings.TC_BC7,
          'mip_gen_settings':unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
          'lod_group':unreal.TextureGroup.TEXTUREGROUP_UI,'never_stream':True,'compression_no_alpha':False,
          'srgb':True,'filter':unreal.TextureFilter.TF_BILINEAR,
          'address_x':unreal.TextureAddress.TA_CLAMP,'address_y':unreal.TextureAddress.TA_CLAMP}
        for key,value in settings.items():texture.set_editor_property(key,value)
        assert save_loaded_asset(texture,unreal_module=unreal)
        assert texture.blueprint_get_size_x()==2048 and texture.blueprint_get_size_y()==2240
        assert texture.get_editor_property('compression_settings')==unreal.TextureCompressionSettings.TC_BC7
        records.append({'type':effect['type'],'texture':texture.get_path_name(),'compression':'BC7','grid':[8,8],
          'frames':6,'built_size':[2048,2240],'ground_anchor':[128,250],'source_sha256':effect['atlas_sha256']})
    report={'ok':True,'records':records};output=ROOT/'Saved/Diagnostics/FourteenGems/element-vfx-import.json'
    output.parent.mkdir(parents=True,exist_ok=True);output.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(report,ensure_ascii=False))


if __name__=='__main__':main()
