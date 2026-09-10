"""Probe or import the three approved resistance icons missing at runtime."""
import argparse
import hashlib
import json
import struct
from pathlib import Path
import unreal
from gamexxk_import_gem_icons import configure

ROOT = Path(__file__).resolve().parents[2]
ART = ROOT / 'SourceArt/UI/Items/Gems/gem-type-runtime-manifest.json'
OUT = ROOT / 'Saved/Diagnostics/ResistanceIconRepair-20260910'
TYPES = {'FireResistance', 'FrostResistance', 'LightningResistance'}

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--apply', action='store_true')
    args = parser.parse_args()
    manifest = json.loads(ART.read_text(encoding='utf-8'))
    records = [r for r in manifest['records'] if r['type'] in TYPES]
    assert len(records) == 3
    output = {'mode': 'apply' if args.apply else 'probe', 'records': []}
    for r in records:
        source = ROOT / r['normalized_png']
        data = source.read_bytes()
        assert hashlib.sha256(data).hexdigest() == r['normalized_sha256']
        assert struct.unpack('>II', data[16:24]) == (512,512) and data[25] == 6
        name = 'T_Item_Gem_' + r['type']
        path = '/Game/GameXXK/UI/Items/Gems/' + name
        existed = unreal.EditorAssetLibrary.does_asset_exist(path)
        if args.apply and not existed:
            task = unreal.AssetImportTask()
            task.filename = str(source)
            task.destination_path = '/Game/GameXXK/UI/Items/Gems'
            task.destination_name = name
            task.automated = True
            task.replace_existing = False
            task.save = False
            unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        # The editor-only existence helper can report false during PIE; loading
        # the exact runtime object path is the decisive check for an empty brush.
        texture = unreal.load_asset(path)
        row = {'type': r['type'], 'objectPath': path + '.' + name, 'existedBefore': existed,
               'loaded': isinstance(texture, unreal.Texture2D), 'sourceSha256': r['normalized_sha256']}
        if args.apply:
            assert isinstance(texture, unreal.Texture2D), path
            configure(texture)
            texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_BC7)
            assert unreal.EditorAssetLibrary.save_loaded_asset(texture)
            assert texture.get_path_name() == row['objectPath']
            files = texture.get_editor_property('asset_import_data').extract_filenames()
            assert files and Path(files[0]).resolve() == source.resolve()
            assert [texture.blueprint_get_size_x(),texture.blueprint_get_size_y()] == [512,512]
            assert not texture.get_editor_property('compression_no_alpha')
            row['audit'] = json.loads(unreal.GameXXKTextureAuditLibrary.inspect_texture(texture, True))
            row['saved'] = True
        output['records'].append(row)
    output['allTypePathsLoadable'] = all(isinstance(unreal.load_asset('/Game/GameXXK/UI/Items/Gems/T_Item_Gem_' + r['type']), unreal.Texture2D) for r in manifest['records'])
    OUT.mkdir(parents=True, exist_ok=True)
    target = OUT / ('repair.json' if args.apply else 'probe.json')
    target.write_text(json.dumps(output,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(output,ensure_ascii=False))

if __name__ == '__main__': main()
