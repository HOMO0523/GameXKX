"""Import the 17 approved quality-independent gem icons, preserving existing sources."""
from pathlib import Path
import hashlib, json, struct
import unreal
from gamexxk_import_gem_icons import configure

ROOT=Path(__file__).resolve().parents[2]
MANIFEST=ROOT/'SourceArt/UI/Items/Gems/gem-type-runtime-manifest.json'
DEST='/Game/GameXXK/UI/Items/Gems'

def main():
    data=json.loads(MANIFEST.read_text(encoding='utf-8'));records=data['records']
    assert data['approved_for_runtime'] and len(records)==17 and len({r['type'] for r in records})==17
    for record in records:
        source=ROOT/record['normalized_png'];raw=source.read_bytes()
        assert hashlib.sha256(raw).hexdigest()==record['normalized_sha256']
        assert raw[:8]==b'\x89PNG\r\n\x1a\n' and struct.unpack('>II',raw[16:24])==(512,512) and raw[25]==6
    report=[]
    for record in records:
        source=ROOT/record['normalized_png'];name='T_Item_Gem_'+record['type'];path=DEST+'/'+name
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            texture=unreal.load_asset(path)
            imported=texture.get_editor_property('asset_import_data').extract_filenames()
            assert imported and Path(imported[0]).resolve()==source.resolve(),'Different source at '+path
        else:
            task=unreal.AssetImportTask();task.filename=str(source);task.destination_path=DEST;task.destination_name=name
            task.automated=True;task.replace_existing=False;task.save=False
            unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task]);texture=unreal.load_asset(path)
        assert isinstance(texture,unreal.Texture2D);configure(texture)
        texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_BC7)
        assert unreal.EditorAssetLibrary.save_loaded_asset(texture)
        assert texture.blueprint_get_size_x()==512 and texture.blueprint_get_size_y()==512
        assert not texture.get_editor_property('compression_no_alpha')
        assert texture.get_editor_property('compression_settings')==unreal.TextureCompressionSettings.TC_BC7
        report.append({'type':record['type'],'package':path,'object_path':texture.get_path_name(),'source_sha256':record['normalized_sha256'],'size':[512,512],'compression':'BC7'})
    # Extend the existing import-time budget only for these explicitly approved new assets.
    budget_path=ROOT/'SourceAssets/ImageDelivery/texture-budget-plan.json'
    budget=json.loads(budget_path.read_text(encoding='utf-8')) if budget_path.exists() else {'version':1,'records':[]}
    known={r['package'] for r in budget['records']}
    for record in report:
        if record['package'] not in known:
            budget['records'].append({'package':record['package'],'object_path':record['object_path'],'action':'optimize',
                'reason':'same_size_approved_gem_bc7','original_size':[512,512],'target_size':[512,512],
                'estimated_before_bytes':512*512*4,'estimated_after_bytes':512*512,'source_png_sha256':record['source_sha256']})
    budget_path.write_text(json.dumps(budget,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    output=ROOT/'Saved/Diagnostics/SeventeenGems/gem-type-import.json';output.parent.mkdir(parents=True,exist_ok=True)
    output.write_text(json.dumps({'ok':True,'count':17,'records':report},ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({'ok':True,'count':17,'report':str(output)},ensure_ascii=False))

if __name__=='__main__':main()
