"""Import the 14 approved v5 type icons; quality uses the equipment materials."""
from pathlib import Path
import hashlib
import json
import struct
import unreal
from gamexxk_import_gem_icons import configure, save_asset_with_texture_budget

ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / 'SourceArt/UI/Items/Gems/contour-review-20260909/type-icon-manifest-v5.json'
DEST = '/Game/GameXXK/UI/Items/Gems'


def main():
    manifest = json.loads(MANIFEST.read_text(encoding='utf-8'))
    records = manifest['records']
    assert len(records) == 14 and len({r['type'] for r in records}) == 14
    validated = []
    for record in records:
        source = ROOT / record['normalized_png']
        data = source.read_bytes()
        assert hashlib.sha256(data).hexdigest() == record['normalized_sha256'], source
        assert data[:8] == b'\x89PNG\r\n\x1a\n' and struct.unpack('>II', data[16:24]) == (512, 512)
        assert data[25] == 6, 'Expected a real RGBA PNG'
        name = 'T_Item_Gem_' + record['type']
        assert source.stem == name
        validated.append((source, name, record['normalized_sha256']))
    imported = []
    for source, name, digest in validated:
        path = DEST + '/' + name
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            texture = unreal.EditorAssetLibrary.load_asset(path)
            paths = texture.get_editor_property('asset_import_data').extract_filenames()
            assert paths and Path(paths[0]).resolve() == source.resolve(), 'Refusing to overwrite another source: ' + path
        else:
            task = unreal.AssetImportTask()
            task.filename = str(source)
            task.destination_path = DEST
            task.destination_name = name
            task.automated = True
            task.replace_existing = False
            task.save = False
            unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
            texture = unreal.EditorAssetLibrary.load_asset(path)
        assert isinstance(texture, unreal.Texture2D), path
        configure(texture)
        assert save_asset_with_texture_budget(texture), path
        assert texture.blueprint_get_size_x() == 512 and texture.blueprint_get_size_y() == 512
        imported.append({'type': name.removeprefix('T_Item_Gem_'), 'texture': texture.get_path_name(), 'source_sha256': digest})
    report = {'ok': True, 'count': len(imported), 'records': imported}
    output = ROOT / 'Saved/Diagnostics/FourteenGems/type-icon-import.json'
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(report, ensure_ascii=False))


if __name__ == '__main__':
    main()
